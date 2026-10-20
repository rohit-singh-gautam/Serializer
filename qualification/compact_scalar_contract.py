# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify proposed compact-scalar payload contracts without enabling schema syntax."""
import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FORMATS = {'float16': (5, 10), 'bfloat16': (8, 7), 'float32': (8, 23)}
SAMPLE_SEED = 0x5343414C4152
SAMPLE_COUNT = 10000


def power_of_two(exponent):
    """Return an exact binary quantum with no host floating-point rounding."""
    return Fraction(1 << exponent) if exponent >= 0 else Fraction(1, 1 << -exponent)


def nearest_even(value):
    """Round an exact rational to its nearest integer, resolving a tie to even."""
    lower, remainder = divmod(value.numerator, value.denominator)
    doubled = 2 * remainder
    return lower + (doubled > value.denominator or doubled == value.denominator and lower % 2 != 0)


def finite_payload(exact, negative, exponent_bits, fraction_bits):
    """Round an exact nonnegative rational directly to target bits without an intermediate float."""
    bias = (1 << (exponent_bits - 1)) - 1
    sign = int(negative) << (exponent_bits + fraction_bits)
    if exact == 0:
        return sign
    exponent = exact.numerator.bit_length() - exact.denominator.bit_length()
    if exact < power_of_two(exponent):
        exponent -= 1
    minimum_exponent = 1 - bias
    if exponent < minimum_exponent:
        significand = nearest_even(exact / power_of_two(minimum_exponent - fraction_bits))
        return sign | significand
    significand = nearest_even(exact / power_of_two(exponent - fraction_bits))
    if significand == 1 << (fraction_bits + 1):
        exponent += 1
        significand >>= 1
    if exponent > bias:
        raise OverflowError('Finite value rounds beyond target finite range')
    return sign | ((exponent + bias) << fraction_bits) | (significand - (1 << fraction_bits))



def binary_payload(value, exponent_bits, fraction_bits):
    """Round binary64 directly to target bits, rejecting finite overflow and canonicalizing NaN."""
    maximum_exponent = (1 << exponent_bits) - 1
    sign = int(math.copysign(1.0, value) < 0) << (exponent_bits + fraction_bits)
    if math.isnan(value):
        return (maximum_exponent << fraction_bits) | (1 << (fraction_bits - 1))
    if math.isinf(value):
        return sign | (maximum_exponent << fraction_bits)
    return finite_payload(Fraction.from_float(abs(value)), bool(sign), exponent_bits, fraction_bits)


def binary_value(bits, exponent_bits, fraction_bits):
    """Decode every target pattern, producing exact finite binary64 values and quiet native NaN."""
    sign = -1 if bits >> (exponent_bits + fraction_bits) else 1
    fraction = bits & ((1 << fraction_bits) - 1)
    exponent = (bits >> fraction_bits) & ((1 << exponent_bits) - 1)
    maximum_exponent = (1 << exponent_bits) - 1
    if exponent == maximum_exponent:
        return math.nan if fraction else math.copysign(math.inf, sign)
    if exponent == 0 and fraction == 0:
        return math.copysign(0.0, sign)
    bias = (1 << (exponent_bits - 1)) - 1
    significand = fraction if exponent == 0 else (1 << fraction_bits) + fraction
    return float(sign * significand * power_of_two((1 if exponent == 0 else exponent) - bias - fraction_bits))


def zigzag(value, width):
    """Encode the complete declared signed range without relying on a signed left shift."""
    if width not in (8, 16, 32, 64) or not -(1 << (width - 1)) <= value < 1 << (width - 1):
        raise OverflowError('Signed value outside declared range')
    return 2 * value if value >= 0 else 2 * (-(value + 1)) + 1


def unzigzag(value, width):
    """Decode unsigned ZigZag bits only after enforcing the declared signed type width."""
    if not 0 <= value < 1 << width:
        raise OverflowError('ZigZag payload outside declared width')
    return value // 2 if value % 2 == 0 else -(value // 2) - 1


def varint(value):
    """Produce the shortest unsigned base-128 representation."""
    assert value >= 0
    output = bytearray()
    while value >= 128:
        output.append((value & 127) | 128)
        value >>= 7
    output.append(value)
    return output.hex()


def parse_varint(data, width):
    """Reject truncation, surplus bytes, nonminimal terminal groups and width overflow."""
    if not data or len(data) > (width + 6) // 7:
        raise ValueError('Invalid compact extent')
    value = 0
    for index, byte in enumerate(data):
        group = byte & 127
        if index > 0 and byte < 128 and group == 0:
            raise ValueError('Nonminimal compact payload')
        value |= group << (7 * index)
        if value >= 1 << width:
            raise OverflowError('Compact payload exceeds declared width')
        if byte < 128:
            if index != len(data) - 1:
                raise ValueError('Trailing compact payload')
            return value
    raise ValueError('Truncated compact payload')


def scaled(value, scale, offset, width, native_width=64):
    """Quantize exact input and reconstruct directly in the declared binary32/binary64 format."""
    if not math.isfinite(value) or scale <= 0 or width not in (8, 16, 32, 64) or native_width not in (32, 64):
        raise ValueError('Invalid scaled value or policy')
    if native_width == 32 and binary_value(binary_payload(value, 8, 23), 8, 23) != value:
        raise ValueError('Input is not a declared binary32 value')
    quantized = nearest_even((Fraction.from_float(value) - offset) / scale)
    zigzag(quantized, width)
    reconstructed = offset + quantized * scale
    exponent_bits, fraction_bits = (8, 23) if native_width == 32 else (11, 52)
    bits = finite_payload(abs(reconstructed), reconstructed < 0, exponent_bits, fraction_bits)
    native = binary_value(bits, exponent_bits, fraction_bits)
    return quantized, native


def check_contract():
    """Cross-check exact vectors, every 16-bit pattern and independent Python IEEE conversions."""
    vectors = []
    scalar_cases = [
        ('float16', 0.0, 0x0000), ('float16', -0.0, 0x8000),
        ('float16', 1.0, 0x3c00), ('float16', -2.0, 0xc000),
        ('float16', 2.0**-24, 0x0001), ('float16', 2.0**-25, 0x0000),
        ('float16', -(2.0**-25), 0x8000), ('float16', 3.0*2.0**-25, 0x0002),
        ('float16', math.nextafter(1.0+2.0**-11, math.inf), 0x3c01),
        ('float16', 1.0+2.0**-11, 0x3c00), ('float16', 1.0+3.0*2.0**-11, 0x3c02),
        ('float16', 65504.0, 0x7bff), ('float16', 65519.0, 0x7bff),
        ('float16', math.inf, 0x7c00), ('float16', -math.inf, 0xfc00),
        ('float16', math.nan, 0x7e00),
        ('bfloat16', 1.0, 0x3f80), ('bfloat16', -2.0, 0xc000),
        ('bfloat16', 2.0**-133, 0x0001), ('bfloat16', 2.0**-134, 0x0000),
        ('bfloat16', math.nextafter(1.0+2.0**-8, math.inf), 0x3f81),
        ('bfloat16', 1.0+2.0**-8, 0x3f80), ('bfloat16', 1.0+3.0*2.0**-8, 0x3f82),
        ('bfloat16', math.inf, 0x7f80), ('bfloat16', math.nan, 0x7fc0),
        ('float32', 1.0, 0x3f800000), ('float32', -0.0, 0x80000000),
        ('float32', 2.0**-149, 0x00000001), ('float32', 2.0**-150, 0x00000000),
        ('float32', 1.0+2.0**-24, 0x3f800000), ('float32', 1.0+3.0*2.0**-24, 0x3f800002),
        ('float32', math.inf, 0x7f800000), ('float32', math.nan, 0x7fc00000),
    ]
    for name, value, expected in scalar_cases:
        bits = binary_payload(value, *FORMATS[name])
        assert bits == expected, (name, value, hex(bits), hex(expected))
        size = (1 + sum(FORMATS[name])) // 8
        vectors.append({'format': name, 'input': repr(value), 'payload_hex': bits.to_bytes(size, 'little').hex()})
    exhaustive = {}
    for name in ('float16', 'bfloat16'):
        exponent_bits, fraction_bits = FORMATS[name]
        canonical_nan = (((1 << exponent_bits) - 1) << fraction_bits) | (1 << (fraction_bits - 1))
        for bits in range(1 << 16):
            value = binary_value(bits, exponent_bits, fraction_bits)
            expected = canonical_nan if math.isnan(value) else bits
            assert binary_payload(value, exponent_bits, fraction_bits) == expected, (name, bits)
            if name == 'float16' and not math.isnan(value):
                assert struct.unpack('<e', bits.to_bytes(2, 'little'))[0] == value
                assert struct.pack('<e', value) == bits.to_bytes(2, 'little')
        exhaustive[name] = 1 << 16
    samples = random.Random(SAMPLE_SEED)
    compared = rejected = 0
    for _ in range(SAMPLE_COUNT):
        value = struct.unpack('<d', samples.getrandbits(64).to_bytes(8, 'little'))[0]
        for name, code, width in (('float16', '<e', 2), ('float32', '<f', 4)):
            try:
                oracle = struct.pack(code, value)
            except (OverflowError, struct.error):
                try:
                    binary_payload(value, *FORMATS[name])
                except OverflowError:
                    rejected += 1
                else:
                    raise AssertionError(('Oracle overflow accepted', name, value))
            else:
                actual = binary_payload(value, *FORMATS[name]).to_bytes(width, 'little')
                if not math.isnan(value):
                    assert actual == oracle, (name, value, actual.hex(), oracle.hex())
                compared += 1
    for value in (65520.0, -65520.0):
        try:
            binary_payload(value, *FORMATS['float16'])
        except OverflowError:
            pass
        else:
            raise AssertionError('Finite binary16 tie overflow accepted')
    signed_cases = 0
    for width in (8, 16, 32, 64):
        for value in (-(1 << (width-1)), -64, -2, -1, 0, 1, 2, 63, (1 << (width-1))-1):
            encoded = zigzag(value, width)
            assert unzigzag(encoded, width) == value
            assert unzigzag(parse_varint(bytes.fromhex(varint(encoded)), width), width) == value
            vectors.append({'format': 'zigzag'+str(width), 'input': str(value), 'payload_hex': varint(encoded)})
            signed_cases += 1
    malformed = 0
    for width in (8, 16, 32, 64):
        for data in (b'', b'\x80', b'\x80\x00', b'\x00\x00', b'\xff' * ((width + 6) // 7), b'\xff' * ((width + 6) // 7) + b'\x01'):
            try:
                parse_varint(data, width)
            except (ValueError, OverflowError):
                malformed += 1
            else:
                raise AssertionError(('Invalid compact payload accepted', width, data))
    assert varint(zigzag(-(1 << 63), 64)) == 'ffffffffffffffffff01'
    assert varint(zigzag((1 << 63)-1, 64)) == 'feffffffffffffffff01'
    scaled_vectors = [(1.25, Fraction(1, 2), Fraction(0), 2),
                      (1.75, Fraction(1, 2), Fraction(0), 4),
                      (-1.25, Fraction(1, 2), Fraction(0), -2),
                      (-1.75, Fraction(1, 2), Fraction(0), -4),
                      (12.5, Fraction(1), Fraction(10), 2)]
    for value, scale, offset, expected in scaled_vectors:
        quantized, reconstructed = scaled(value, scale, offset, 16)
        assert quantized == expected
        assert abs(Fraction.from_float(value) - (offset + quantized * scale)) <= scale / 2
        vectors.append({'format': 'scaled16', 'input': repr(value), 'scale': str(scale), 'offset': str(offset),
                        'quantized': quantized, 'decoded': repr(reconstructed), 'payload_hex': varint(zigzag(quantized, 16))})
    assert scaled(0.15, Fraction(1, 10), Fraction(0), 8)[0] == 1
    assert math.copysign(1.0, scaled(-0.0, Fraction(1), Fraction(0), 8)[1]) == 1
    # Exact reconstruction must neither overflow binary32 nor round through binary64.
    maximum_float32 = struct.unpack('<f', bytes.fromhex('ffff7f7f'))[0]
    try:
        scaled(maximum_float32, power_of_two(128), Fraction(0), 8, 32)
    except OverflowError:
        pass
    else:
        raise AssertionError('Scaled binary32 reconstruction overflow accepted')
    assert scaled(maximum_float32, power_of_two(128), Fraction(0), 8, 64) == (1, 2.0**128)
    midpoint_above = Fraction(1) + power_of_two(-24) + power_of_two(-80)
    quantized, direct = scaled(1.0, midpoint_above, Fraction(0), 8, 32)
    assert quantized == 1 and struct.pack('<f', direct).hex() == '0100803f'
    assert struct.pack('<f', float(midpoint_above)).hex() == '0000803f'
    negative_underflow = scaled(0.0, Fraction(1), -power_of_two(-150), 8, 32)[1]
    assert struct.pack('<f', negative_underflow).hex() == '00000080'
    assert struct.pack('<f', scaled(-0.0, Fraction(1), Fraction(0), 8, 32)[1]).hex() == '00000000'
    vectors.append({'format': 'scaled8/binary32', 'input': '1.0', 'scale': str(midpoint_above),
                    'offset': '0', 'quantized': quantized, 'decoded': repr(direct), 'decoded_hex': '0100803f',
                    'payload_hex': varint(zigzag(quantized, 8))})
    vectors.append({'format': 'scaled8/binary32', 'input': '0.0', 'scale': '1',
                    'offset': str(-power_of_two(-150)), 'quantized': 0, 'decoded': '-0.0',
                    'decoded_hex': '00000080', 'payload_hex': '00'})

    try:
        scaled(128.0, Fraction(1), Fraction(0), 8)
    except OverflowError:
        pass
    else:
        raise AssertionError('Scaled range overflow accepted')
    for value, scale in ((math.inf, Fraction(1)), (math.nan, Fraction(1)), (1.0, Fraction(0)), (1.0, Fraction(-1))):
        try:
            scaled(value, scale, Fraction(0), 8)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid scaled policy accepted')
    return {'golden_vectors': vectors, 'exhaustive_patterns': exhaustive, 'independent_samples': SAMPLE_COUNT,
            'independent_conversion_checks': compared, 'independent_overflow_checks': rejected,
            'signed_boundary_vectors': signed_cases, 'malformed_integer_vectors': malformed, 'scaled_native_widths': [32, 64], 'reference_only': True}


def main():
    """Retain reproducible design vectors and prove all proposed modifiers remain unavailable."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or build.is_relative_to(ROOT) and build.relative_to(ROOT).parts[0] != 'out':
        parser.error('In-repository output must stay beneath ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    compiler = args.compiler.resolve()
    compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
    report = check_contract()
    rejected = []
    for name in ('compact_zigzag', 'compact_float16', 'compact_bfloat16', 'compact_float32', 'compact_scaled'):
        source, output = build/(name+'.serializer'), build/(name+'.hpp')
        source.write_text('serializer version 1.20.0; class contract_model { public '+name+' '+('int64' if name=='compact_zigzag' else 'double')+' value (1); }\n', encoding='utf-8')
        sentinel = b'preserved existing output\n'
        output.write_bytes(sentinel)
        command = [str(compiler), '--input', str(source), '--language', 'cpp', '--cpp.output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=60)
        (build/(name+'.log')).write_text(result.stdout+result.stderr, encoding='utf-8')
        assert result.returncode != 0 and output.read_bytes() == sentinel, name
        rejected.append(name)
    assert hashlib.sha256(compiler.read_bytes()).hexdigest() == compiler_hash
    report.update(compiler_sha256=compiler_hash, proposed_keywords_rejected=rejected,
                  python=sys.version, runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (build/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    summary = {key: value for key, value in report.items() if key != 'golden_vectors'}
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
