# Compact scalar encodings

Schema language **1.2.0**, compiler/runtime **1.3.0**, and editor extensions
**1.1.25** add two optional encodings for owning unsigned scalar fields. The
public field type remains `uint8`, `uint16`, `uint32`, or `uint64`.

```text
serializer version 1.2.0;
class counters {
  public compact_prefix strict uint32 value (3) {32};
  public compact_prefix lenient uint32 unchecked (4) {32};
  public compact_varint uint64 total (5) {0};
}
```

An omitted policy means `strict`. The policy spelling is `lenient`. Repeated
encodings/policies and a policy without a compact encoding are errors.
Each declaring file, including dependencies, must select language `1.2.0` or
newer. Older schema contracts and unannotated field bytes remain unchanged.

## Names and wire choices

| Modifier | Encoding | Value range | Maximum payload size |
| --- | --- | --- | --- |
| `compact_prefix` | Top two bits of the first byte encode the following-byte count; remaining bytes carry most-significant groups first | Unsigned, at most `0x3fffffff`, further bounded by the declared type | 4 bytes |
| `compact_varint` | Unsigned LEB128: seven low-to-high payload bits per byte, MSB set when another byte follows | Full declared unsigned range | 2/3/5/10 bytes for uint8/16/32/64 |

Prefix widths are 1, 2, 3, and 4 bytes for 6, 14, 22, and 30 value bits.
This is the existing Serializer ID/length encoding applied explicitly to field
payloads. `compact_prefix` describes the mechanism without confusing a host
type's width with its payload capacity. If a capacity-based name is desired,
`compact30` would describe this format; it is not an accepted schema alias.

| Value | Prefix bytes (hex) | Varint bytes (hex) |
| --- | --- | --- |
| 0 | `00` | `00` |
| 32 | `20` | `20` |
| 63 | `3f` | `3f` |
| 64 | `40 40` | `40` |
| 128 | `40 80` | `80 01` |
| 16384 | `80 40 00` | `80 80 01` |
| 1073741823 | `ff ff ff ff` | `ff ff ff ff 03` |
| 4294967295 | Strict overflow | `ff ff ff ff 0f` |

Small values benefit most. Varints can exceed the fixed-width size near the
type's upper bound. Prefix and varint payload bytes are independent of C++'s
selected scalar wire byte order. Field keys, collection counts, enum ordinals,
union indices and terminators retain their existing encoding.

## Strict and lenient policies

Strict prefix serialization rejects values above `0x3fffffff` before writing
that field's payload. Earlier object bytes, including a keyed field's identity,
may already have been written. A known strict prefix default exceeding the
wire range is a compiler error. Compact defaults must be unsigned integer
literals representable by the declared host type.

Lenient prefix serialization takes the low 30 bits without an overflow check:
`0x40000000` becomes zero and `0xffffffff` becomes `0x3fffffff`. This is deliberate
data loss with deterministic interoperable bytes, rather than undefined
behavior. No compiler pragma is needed. Typed backends use unsigned operations;
dynamic backends require an integer value and use the declared-width mask in
lenient mode.

Varints represent every value of the declared unsigned type, so typed fields
need no additional value-range check. In dynamic backends, strict encoding
validates the declared host range; lenient encoding keeps the low declared-width
bits. `strict` and `lenient` produce identical bytes for in-range varints.

Readers enforce input and resource bounds in both policies. Prefix input checks
the declared type's range. Varint input rejects truncation, excess continuation
bytes, unused high payload bits and redundant terminal zero groups. Writers
emit the shortest encoding. Strictness introduces no extra per-object state;
decoded values do not need a redundant prefix range check beyond the wire and
host-width checks.

## Supported scope and compatibility

All eleven outputs support these scalar annotations: C++, Java, JavaScript,
TypeScript declarations using the JavaScript runtime, Go, C#, Rust, Python,
Swift, Kotlin and C. JSON retains ordinary integer values and range rules.
Optional C++ Protobuf binary, ProtoJSON and TextProto retain their existing
protocol-specific scalar mapping. The annotation affects only the three native
binary protocols.

Collections, maps, unions, signed integers, enums, floating-point values,
payload version discriminators, magic, packed classes and mapped views reject
compact annotations. Generic scalar parameters are checked again when bound.
Ordinary owning and managed field storage retain their underlying scalar type.

Changing a field from fixed width to compact, or between prefix and varint,
changes its native binary contract and requires an explicit payload migration.
The schema compatibility checker reports this change for native binary formats.
Changing only strict/lenient preserves valid wire bytes but changes writer
acceptance; consider the application's value contract separately.

## Proposed signed and floating encodings

These names describe possible follow-up formats and are **not implemented
schema keywords**:

| Proposed name | Encoding | Tradeoff |
| --- | --- | --- |
| `compact_zigzag` | Map signed `0, -1, 1, -2...` to unsigned `0, 1, 2, 3...`, then unsigned LEB128 | Lossless; efficient near zero |
| `compact_float16` | Convert float/double to IEEE binary16 | Two bytes; reduced precision and range |
| `compact_bfloat16` | Convert float/double to bfloat16 | Two bytes; float32 exponent width with fewer fraction bits |
| `compact_float32` | Convert double to IEEE binary32 | Four bytes; reduced precision and range |
| `compact_scaled` | Round `(value - offset) / scale` to an integer, then compact it | Schema-defined quantization error and bounds |

[ZigZag and base-128 varints](https://protobuf.dev/programming-guides/encoding/)
have established portable definitions. [IEEE binary16](https://docs.nvidia.com/cuda/archive/12.5.1/cuda-math-api/struct____half.html)
uses one sign, five exponent and ten fraction bits;
[bfloat16](https://www.tensorflow.org/jvm/api_docs/java/org/tensorflow/ndarray/impl/buffer/layout/Bfloat16Layout)
keeps the eight-bit exponent and seven fraction bits.

Floating formats need explicit rounding, overflow, NaN, infinity and signed-zero
contracts before implementation. Applying varints to raw IEEE bits is lossless
but can expand typical floats and does not guarantee compression. Whole-message
[compression](compression.md) remains available when exact floating bits matter.

See [usage](usage.md), [schema reference](schema_reference.md),
[wire format](wire_format.md), [migration](../migration.md), and the
[compact qualification record](verification-compact-2026-10-07.md).
