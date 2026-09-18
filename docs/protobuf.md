# Protobuf binary, ProtoJSON, and TextProto

C++ owning objects can encode and decode these three standard representations
directly. The protocol is a template argument, just like `binary_integer`:

```cpp
template <rohit::serializer::serialize_type Direction>
using protobuf_binary = rohit::serializer::protobuf_codec<
    Direction, rohit::serializer::protobuf_format::binary>;
```

The public aliases are `rohit::serializer::protobuf_binary`,
`rohit::serializer::protojson`, and `rohit::serializer::textproto`, declared in
`<rohit/protobuf.hpp>`. `if constexpr` selects the implementation; generated
methods dispatch typed fields with compile-time numeric IDs. There are no
runtime descriptors, reflection, external Protobuf libraries, or `protoc` steps
in the application build. Generated Java currently retains its original four
protocols.

## Generate compatible C++ types

Enable the additional generated methods explicitly:

```ini
[cpp]
protobuf = true
```

```sh
serializer --input account.serializer --output account.hpp --cpp.protobuf true
```

Use the INI file with `serializer_generate(TARGET app SCHEMAS account.serializer
CONFIG output.ini)` for CMake consumers. The generated header includes the new
protocol header. All existing protocols remain available. With `protobuf = false`
(the default), generation and the existing wire formats are unchanged.

The compiler validates field numbers, wire names, maps, and requested storage
before publishing output. This setting enables codecs; it does not emit a `.proto`
file or introduce new `.serializer` syntax. Regenerate existing headers before
using these protocols.

## Encode and decode

```cpp
#include <account.hpp>

namespace codec = rohit::serializer;

// Encode the chosen representation using the established protocol-template API.
void send(const demo::account& value, rohit::stream& output) {
  value.serialize_out<codec::protobuf_binary>(output);
}

// Supply exactly one message; all remaining bytes belong to that message.
demo::account receive(const rohit::stream& input) {
  codec::decode_limits limits{};
  limits.max_input_bytes = 1024 * 1024;
  codec::protobuf_binary<codec::serialize_type::in> decoder{input, limits};
  demo::account value{};
  value.serialize_in(decoder);
  decoder.finish();
  return value;
}
```

Replace `protobuf_binary` with `protojson` or `textproto` in both calls to select
the corresponding text format. The stream convenience overload also works:
`value.serialize_in<codec::protojson>(input)`.

Decoding constructs a replacement object and commits it only after an exact
message succeeds. Failure can consume input, but leaves the destination object
unchanged. Input bytes, string sizes, cumulative collection entries, nesting,
work, and destination storage are checked through `decode_limits`. These are
resource budgets, not exact heap accounting. Keep the input buffer alive and
independent of the destination. Decoder instances have one owner and share their
budgets with every nested field.

TextProto checks decoded string length and storage budgets before appending bytes,
including escaped Unicode and adjacent quoted fragments. `max_allocation_bytes`
accounts for logical decoded storage, not total parser memory: temporary identifier
and numeric-token buffers, allocator growth slack, and diagnostic construction are
not covered. Input/work limits bound token scanning; TextProto tokens additionally
obey `max_string_bytes`. Set those limits alongside the storage budget.

The optional `deserialize_exact<Value, Protocol>(input[, limits])` runtime helper
also returns a fresh owning value for these codecs. It preserves each codec's
framing rules; for binary, valid concatenated messages may merge. For schema
revision checks, the separate [compatibility checker](schema_evolution.md)
supports `protobuf_binary` and validates this mapping before comparing revisions.
Its first version does not check ProtoJSON/TextProto compatibility.

ProtoJSON uses the shared [SIMD whitespace scanner](runtime_simd.md#simd-json-whitespace-scanning)
for long runs within message and input/work limits. Short gaps and tails remain
scalar. TextProto retains its separate whitespace/comment parser. The focused
whitespace tests pass in the configurations recorded in
[verification](verification-2026-09-17.md), which also covers bounded fuzz runs
for all three optional codecs. Performance measurements remain outstanding.

Binary messages have no automatic outer size prefix or terminator. Applications
must provide framing. Concatenating two binary encodings can form a valid merged
message; `finish()` cannot infer where an application intended to split them.

## Mapping contract

The receiver needs an independently agreed equivalent Protobuf schema. Names of
message types need not match, but field numbers, scalar types, nesting, enum
values, and text field names must match this table:

| Serializer construct | Protobuf representation |
| --- | --- |
| `int8`, `int16`, `int32` | `int32`; narrow destinations are range checked |
| `uint8`, `uint16`, `uint32` | `uint32`; narrow destinations are range checked |
| `char` | `uint32` holding the unsigned byte value, `0..255` |
| `int64`, `uint64` | `int64`, `uint64`, using ordinary varints, not ZigZag |
| `float`, `double`, `bool` | Matching Protobuf scalar |
| `string` | UTF-8 Protobuf `string`, not arbitrary `bytes` |
| enum | Declared names and zero-based numeric ordinals |
| class field or parent | Embedded message under its wire name and field ID |
| `array T` | `repeated T`; numeric/bool/enum output is packed |
| `map(K) V` | Standard map entries: key field 1, value field 2 |
| union | Embedded message at the union's ID, containing a `oneof`; alternative IDs are index + 1 |

`array uint8` means repeated `uint32`, **not** Protobuf `bytes`. The current schema
has no `sint32`, `sint64`, `fixed32`, `fixed64`, explicit field-presence, or
well-known-type declarations. It does not implement special `Any`, `Timestamp`,
`Duration`, or wrapper-message JSON mappings. These codecs target the supported
mapping above, not arbitrary external Protobuf schemas.

Use `stable_ids` for evolving messages. Protobuf field IDs must be in
`1..536870911`, excluding `19000..19999`. IDs are never silently renumbered.
Wire names and enum names must be ASCII identifiers; colliding lowerCamel JSON
names are rejected. Map keys are restricted to integers, `char`, `bool`, and
`string`. Enum, floating-point, and object map keys are rejected. Owning storage
is required; positional binary buffer views are not Protobuf views.

Union alternative IDs follow declaration order. Reordering alternatives changes
the Protobuf contract. Serializer has no unset union state: an absent union maps
to its first alternative with the Protobuf default value. Presence is therefore
not preserved across a decode/encode cycle. Likewise, absent scalar fields use
zero/false/empty/first-enum defaults, **not `.serializer` member initializers**.
Nested messages and collections are reset recursively before root decoding.

## Format behavior

- **Binary:** field tags contain number and wire type; nested messages and strings
  are length delimited. Scalar floats use little-endian IEEE bits. Readers accept
  packed and unpacked repeated scalars, merge duplicate nested messages, and use
  the last scalar/map value. Unknown fields, including unknown groups, are skipped
  and are not retained on re-encoding. Unknown enum values and values outside a
  narrow destination range are rejected.
- **ProtoJSON:** field names use Protobuf lowerCamel conversion; input also accepts
  original wire names. 64-bit integers are emitted as decimal strings, maps as
  JSON objects, and enums by their schema names. Numeric enum input is accepted
  only for declared values. `NaN`, `Infinity`, and `-Infinity` are quoted. Null
  fields act as unset values; null collection elements/map values are rejected.
  Input accepts integral decimal/exponent forms without passing integers through
  floating-point conversion. Duplicate ordinary fields use the last value,
  including original/JSON-name aliases. A later nested object replaces the earlier
  object instead of merging with it; later `null` resets the field to its Protobuf
  default. Repeated union wrapper fields likewise replace the prior wrapper;
  conflicting alternatives within one union object are rejected.
- **TextProto:** output uses original wire names, enum names, and repeated field
  occurrences. Input supports `#` comments, `{}`/`<>` nested messages, repeated
  array lists, adjacent single/double-quoted strings, C-style escapes, octal and
  hexadecimal integers, and standard nonfinite floating-point spellings.
  Duplicate singular fields and conflicting union alternatives are rejected.

Both text readers reject unknown field names. Output includes default scalar
values; JSON includes empty collections. Exact bytes or text need not match the
official runtime's ordering, whitespace, or default omission. Output is not
claimed to be canonical. Protobuf encoding does not change existing JSON or
Serializer binary encodings.

Binary nested messages and packed arrays currently use temporary payload buffers
to compute length prefixes. No zero-copy or performance superiority is claimed.
Benchmark this implementation against the application's actual workload.

## Verification

The default test build generates and compiles `test/resources/protobuf.serializer`
and runs `protobuf_codec_test`. It covers direct template selection, round trips,
independent golden bytes, malformed input, resource limits, and schema rejection.

For independent interoperability tests, install the official Protobuf C++ runtime
and compiler and configure:

```sh
cmake -S . -B build -DSERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS=ON
cmake --build build --config Debug --target protobuf_codec_test
ctest --test-dir build -C Debug -R protobuf_codec_test --output-on-failure
```

Only this test option requires `find_package(protobuf CONFIG REQUIRED)`. It uses
the reference `.proto` fixture under `test/resources`, not a schema export feature.
Two-way interoperability with Protobuf 6.33.4 has been exercised on Windows for
all three formats. Broader Protobuf conformance-suite and performance qualification
remain unperformed.

The [finalization verification record](verification-finalization-2026-09-18.md)
records the current regression checks, including duplicate-field replacement and
pre-allocation TextProto limits. The old 87/91 core-test result belongs to the
[historical implementation run](java.md#verification-performed-for-this-implementation);
its four failures were resolved and are not current pending items.

The mapping follows the official [binary wire format](https://protobuf.dev/programming-guides/encoding/),
[ProtoJSON specification](https://protobuf.dev/programming-guides/json/), and
[Text Format specification](https://protobuf.dev/reference/protobuf/textformat-spec/).
