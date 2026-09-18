"""Check high-byte char-map semantics while pinning each producer's existing binary order."""

import argparse
import importlib.util
from pathlib import Path
import struct
import subprocess

PRODUCERS = ('cpp_signed', 'cpp_unsigned', 'java', 'python')
PROTOCOL_NAMES = ('json', 'positional', 'integer', 'string')
KEYS = (255, 127, 0, 128)
VALUE_BASE = 1000


def fixture(schema, ascii_only):
    """Construct a complete independent value, not a canonical re-encoding oracle."""
    value = schema.CharMap()
    value.entries = {key: VALUE_BASE + key for key in KEYS if not ascii_only or key <= 127}
    return value


def frozen_binary(producer, protocol):
    """Pin the preexisting signed/unsigned ordering and explicit native framing bytes."""
    keys = (128, 255, 0, 127) if producer in ('cpp_signed', 'java') else (0, 127, 128, 255)
    payload = bytes([len(keys)]) + b''.join(bytes([key]) + struct.pack('<I', VALUE_BASE + key)
                                           for key in keys)
    if protocol == 'integer':
        return b'\x01' + payload + b'\x00'
    if protocol == 'string':
        return b'\x07entries' + payload + b'\x00'
    return payload


def main():
    """Run every producer/consumer direction, comparing decoded keys and values."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp-signed', type=Path, required=True)
    parser.add_argument('--cpp-unsigned', type=Path, required=True)
    parser.add_argument('--java', type=Path, required=True)
    parser.add_argument('--classes', type=Path, required=True)
    parser.add_argument('--schema', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    args.fixtures.mkdir(parents=True, exist_ok=True)
    (args.fixtures / 'producers.txt').write_text('\n'.join(PRODUCERS) + '\n', encoding='utf-8')
    spec = importlib.util.spec_from_file_location('char_map_schema', args.schema)
    schema = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(schema)
    commands = ([str(args.cpp_signed)], [str(args.cpp_unsigned)],
                [str(args.java), '-cp', str(args.classes), 'Main'])
    for command in commands:
        subprocess.run([*command, str(args.fixtures), 'emit'], check=True)
    for protocol, name in zip(schema.Protocol, PROTOCOL_NAMES):
        value = fixture(schema, protocol == schema.Protocol.JSON)
        (args.fixtures / f'python_{name}.bin').write_bytes(value.encode(protocol))
    try:
        fixture(schema, False).encode(schema.Protocol.JSON)
    except ValueError:
        pass
    else:
        raise AssertionError('Python JSON accepted a high-bit character key')
    for producer in PRODUCERS:
        for protocol, name in zip(schema.Protocol, PROTOCOL_NAMES):
            data = (args.fixtures / f'{producer}_{name}.bin').read_bytes()
            actual = schema.CharMap.decode(data, protocol)
            expected = fixture(schema, protocol == schema.Protocol.JSON)
            if actual.entries != expected.entries:
                raise AssertionError(f'Python decoded changed keys or values from {producer}/{name}')
            if name != 'json' and data != frozen_binary(producer, name):
                raise AssertionError(f'Existing binary order or framing changed for {producer}/{name}')
    for command in commands:
        subprocess.run([*command, str(args.fixtures), 'verify'], check=True)
    print('PASS: 4 producers x 4 consumers x 4 protocols = 64 semantic exchanges; '
          'binary keys 0/127/128/255, ASCII JSON, and existing binary order verified')


if __name__ == '__main__':
    main()
