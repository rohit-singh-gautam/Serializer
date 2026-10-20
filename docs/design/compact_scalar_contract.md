# Signed and floating compact payload contract

This design completes **SC01**, the numeric contract prerequisite for five
proposed scalar annotations. **None of these five names is an implemented schema
keyword.** Compiler/runtime **2.14.0** and schema language **1.20.0** continue to reject them.
The implemented unsigned annotations remain documented in
[compact scalars](../schema/compact_integers.md). This design reserves no schema
syntax, annotation identifier or wire discriminator, and changes no existing
serialized bytes or public generated API.

A future implementation must add a backward-compatible schema-language minor
release, compiler/runtime feature release, all affected generators and editor
support, version checks, compatibility diagnostics and actual SDK qualification.
It must never reinterpret an existing fixed-width field or existing annotation.
Changing a persisted field to one of these encodings requires explicit payload
migration. Older supported schemas retain their meaning.

## Scope and shared rules

| Proposed annotation | Native field type | Binary field payload |
| --- | --- | --- |
| `compact_zigzag` | Signed int8/int16/int32/int64 | ZigZag transform, then shortest unsigned LEB128 |
| `compact_float16` | float or double | IEEE 754 binary16 bits, two little-endian bytes |
| `compact_bfloat16` | float or double | bfloat16 bits, two little-endian bytes |
| `compact_float32` | double | IEEE 754 binary32 bits, four little-endian bytes |
| `compact_scaled` | float or double | Checked exact quantization to selected signed width, then ZigZag/LEB128 |

These payload definitions are independent of the selected ordinary scalar byte
order. The enclosing field key, protocol framing and identity keep their normal
contract. These annotations affect only the three native binary protocols;
JSON retains the existing native field representation and validation. Protobuf
encodings retain their protocol-specific mapping. No cross-protocol equality of
values after a deliberately lossy floating conversion is implied.

The initial scope is owning scalar fields, including optional scalar values
where presence is already represented separately. Collection elements, maps,
variants, enums, discriminators, magic, packed/view storage and unresolved generic
parameters remain unsupported until qualified independently. Floating storage
stays its declared native type; no new half-precision public field type is added.
Combinations with another compact annotation or a lenient truncation policy
reject. Quantization is an explicit lossy choice, not integer overflow permission.

Writers validate the scalar and complete payload before emitting that payload.
Earlier outer bytes, including its field identity, may already exist. Readers
check byte extent before access and width before narrowing, and decode into the
native fresh candidate. Existing object transaction/publication rules remain in
force. No format auto-detection or fallback to fixed width is allowed.

## Signed ZigZag

For a signed value `n` of declared width `w`:

- If `n >= 0`, the unsigned value is `2*n`.
- Otherwise it is `2*(-(n+1))+1`.

The mathematical result is within the corresponding unsigned width, including
the signed minimum. Implementations must convert the safe magnitude `-(n+1)` to the corresponding
unsigned type before doubling and adding one. Signed negation of the minimum,
signed multiplication beyond its range and left shifts of negative signed values
are not portable implementations.

Decode even `u` as `u/2` and odd `u` as `-(u/2)-1`. Emit shortest unsigned LEB128,
with at most 2/3/5/10 bytes for 8/16/32/64-bit signed fields. Readers reject
truncation, continuation beyond that maximum, nonzero unused high bits, redundant
terminal zero groups and trailing payload bytes. There is no lenient narrowing.
The established [ZigZag/base-128 definition](https://protobuf.dev/programming-guides/encoding/)
supplies the integer transform; Serializer keeps its own enclosing native format.

| Input | Unsigned transform | Payload hex |
| ---: | ---: | --- |
| 0 | 0 | `00` |
| -1 | 1 | `01` |
| 1 | 2 | `02` |
| -64 | 127 | `7f` |
| 64 | 128 | `80 01` |
| int64 minimum | 18446744073709551615 | `ff ff ff ff ff ff ff ff ff 01` |
| int64 maximum | 18446744073709551614 | `fe ff ff ff ff ff ff ff ff 01` |

## Floating conversion

| Format | Sign bits | Exponent bits | Fraction bits | Exponent bias | Canonical quiet NaN bits |
| --- | ---: | ---: | ---: | ---: | --- |
| binary16 | 1 | 5 | 10 | 15 | `0x7e00` |
| bfloat16 | 1 | 8 | 7 | 127 | `0x7fc0` |
| binary32 | 1 | 8 | 23 | 127 | `0x7fc00000` |

The primary provider references describe [binary16](https://docs.nvidia.com/cuda/cuda-math-api/cuda_math_api/struct____half.html)
and [bfloat16](https://docs.nvidia.com/cuda/cuda-math-api/cuda_math_api/struct____nv__bfloat16.html).
The following overflow, canonicalization and byte-order choices are Serializer's
proposed contract, rather than claims that every provider uses these policies.

Convert the exact value represented by the source native floating bits directly
to the target using round-to-nearest, ties-to-even. Preserve gradual underflow and
subnormals; do not flush them to zero. Underflow to zero retains the source sign.
Preserve signed zeros and infinities. A finite value that rounds beyond the
largest finite target rejects before its payload is written; it must not silently
become infinity. Values above the largest finite value can still round down to
it. Thus 65519 becomes binary16's 65504, while the tie 65520 rejects.

All input NaNs, including signaling or negative NaNs, encode the single positive
quiet NaN pattern above. Preserve neither NaN sign nor payload. Readers accept
all target NaN encodings and construct a quiet native NaN without executing a
signaling conversion; the next encode is canonical. This explicit loss means
applications needing exact NaN payloads should use ordinary floating storage.

Float/double-to-binary16 and double-to-bfloat16 must not round through binary32.
For example the binary64 value immediately above `1 + 2^-11` rounds to binary16
`0x3c01`; an intermediate binary32 value would incorrectly tie to `0x3c00`.
Likewise the binary64 value immediately above `1 + 2^-8` rounds to bfloat16
`0x3f81`, not `0x3f80`. Hardware conversion instructions may be used only when
these direct-rounding, subnormal, finite-overflow and NaN rules are retained
independently of process floating-point environment flags.

Reading finite binary16/bfloat16 into either declared native type, or binary32
into double, is exact. Decode the prescribed little-endian bytes independently of
host layout. Rounding applies when writing the compact payload, not when editing
the ordinary native field.

| Format and input | Target bits | Wire hex |
| --- | --- | --- |
| binary16 `+0` / `-0` | `0x0000` / `0x8000` | `00 00` / `00 80` |
| binary16 `1` | `0x3c00` | `00 3c` |
| binary16 `2^-24` | `0x0001` | `01 00` |
| binary16 `2^-25` / `-2^-25` | `0x0000` / `0x8000` | `00 00` / `00 80` |
| binary16 `1 + 2^-11` | `0x3c00` | `00 3c` |
| binary16 `1 + 3*2^-11` | `0x3c02` | `02 3c` |
| bfloat16 `1` | `0x3f80` | `80 3f` |
| bfloat16 `2^-133` | `0x0001` | `01 00` |
| binary32 `1` | `0x3f800000` | `00 00 80 3f` |
| binary32 `2^-149` | `0x00000001` | `01 00 00 00` |

## Scaled quantization

The required schema parameters are a positive finite `scale`, finite `offset`
and signed quantized width 8/16/32/64. An omitted offset means exact zero; width
must be explicit. Parameter grammar is a later language-design step. Decimal
parameter spellings denote exact base-10 rational values at compile time, not
host-rounded floating constants. Equivalent spellings have identical meaning.
Reject zero/negative/nonfinite scales, nonfinite offsets, unsupported widths and
parameter expressions that do not belong to the eventual supported grammar.

Interpret the source native floating bits as their exact mathematical value `x`.
For finite input compute:

```text
q = nearest_integer_ties_to_even((x - offset) / scale)
r = offset + q * scale
```

The arithmetic that decides `q` is exact; host intermediate rounding must not
change half-way decisions. Reject NaN and infinity, then reject `q` outside the
selected signed range. Reject a reconstructed `r` that rounds outside the finite
range of the declared native field. Write `q` with the signed encoding above.
Readers first enforce the same signed width, reconstruct the same exact `r`, and
round once to the declared native type using nearest/ties-to-even with gradual
underflow. A nonzero underflow keeps its sign; an exact zero is canonical positive
zero. Scaled encoding does not preserve negative zero and has no special NaN or
infinity code. There is no clamping or modulo overflow.

The mathematical quantization error is at most `scale/2`; conversion of `r` to the
native type contributes its separately specified floating rounding error. Avoid
claiming a universal relative error bound near zero. For a half-unit scale,
1.25 quantizes to 2, 1.75 to 4, -1.25 to -2 and -1.75 to -4. Decimal-looking runtime
values are still their actual native binary values: binary64 `0.15` is slightly
below exact `3/20`, so exact scale `0.1`, offset 0 gives `q=1`, not 2.

Changing scale, offset or quantized width changes the binary field contract and
requires migration even when some particular values happen to share bytes.
Defaults must be validated against the same rules during schema compilation.
Comparisons and arithmetic on the field use its ordinary native value until an
explicit compact encode/decode round trip occurs.

## Reference evidence and implementation gates

The standard-library-only [reference runner](../../qualification/compact_scalar_contract.py)
uses exact rational arithmetic for conversion decisions. It checks every 16-bit
pattern in both formats, independent Python IEEE binary16/binary32 conversion
for fixed-seed binary64 samples, signed boundaries, malformed compact integers,
scaled ties/ranges and preserved unsupported-keyword diagnostics/output.
The [dated evidence](../verification/verification-compact-scalar-contract-2026-10-20.md)
records the actual execution. These are payload-design vectors, not generated
codec, parser or editor implementation claims.

Before enabling any annotation, every supported backend must pass the same
frozen payload vectors and malformed-input boundaries with real SDKs; verify
both ordinary scalar byte orders, all native protocols, JSON/Protobuf boundaries,
optionals, native limits, default values, collision diagnostics, migration checks,
direct double rounding and unchanged old schemas/bytes. Add exact wire-kind and
schema-version metadata only with that implementation. Keep individual numeric
formats unavailable until their complete contract is implemented.
