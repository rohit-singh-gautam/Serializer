# Schema and protocol reference

[Back to the project overview](../README.md)

Look up schema declarations, field IDs, buffer views, and native protocol choices.
Start with [small schema examples](schema_examples.md) if you are new to the syntax.

## Schema structure

Every `.serializer` file starts with `serializer version 1;` before declarations.
The declaration snippets below omit this header. Complete files in `example/`
include it. See [schema versioning](command_line.md#schema-files).

### Namespace

Namespaces group related declarations. Nested namespaces and qualified namespace names
express the same hierarchy:

```text
namespace a {
  namespace b {
  }
}
```

This is equivalent to:

```text
namespace a::b {
}
```

### Class

Use `class` in schema files. Every member explicitly states `public`, `protected`,
or `private`. Members end with a semicolon; class and enum declarations do not.

```text
class person {
  public string name;
  public uint32 age;
}
```

Class attributes go after the class name and before an optional parent list.
Implemented attributes are `stable_ids`, `packed`, `owning`, `view`, `readonly`,
and `mutable`. Their order does not matter. `packed` controls generated native
C++ layout, depends on compiler support, and cannot be combined with `view`.
Binary serialization always encodes fields individually.

### Generic owning classes

Declare type parameters with `class result<T>`. C++ emits a native template even
without a concrete schema use. Nested applications such as `response<T>` containing
`result<T>` work directly with application-side C++ arguments. Optional
`instantiate count_result = result<uint32>;` and `result<uint32>` fields define
concrete contracts for cross-language generation and compatibility checks.

Declare positive dimensions as `uint64 N`; use trailing defaults such as
`class matrix<uint64 Rows, uint64 Cols = Rows, T = double>`. Defaults may reference
earlier parameters. Empty `<>` is valid when every parameter has a default.
`array[Rows * Cols] T` has an exact owning extent; `array T` is unchanged.
Expressions permit decimal literals, dimension names, parentheses, `+`, and `*`;
checked uint64 arithmetic rejects negative/zero dimensions and overflow.
Fixed arrays require 1..65,536 elements. C++ uses `std::array`; other backends and
Protobuf generation explicitly reject fixed arrays. See [generics](generics.md)
for scope resolution, resource limits, native C++ use, and support boundaries.

### Owning objects and buffer views

| Class header | Generated modes | Class template |
| --- | --- | --- |
| `class person` or `class person owning` | Owning | No |
| `class person view` | Read-only and mutable views | Yes |
| `class person view readonly` | Read-only view | No |
| `class person view mutable` | Mutable view | No |
| `class person view mutable readonly` | Both views | Yes |
| `class person view owning` | Owning and both views | Yes |
| `class person view owning readonly` | Owning and read-only view | Yes |
| `class person owning mutable view` | Owning and mutable view | Yes |

`readonly` and `mutable` require `view`; either restricts the enabled view modes.
With multiple modes, select `person<rohit::serializer::storage_mode::owning>`,
`person<rohit::serializer::storage_mode::read_only_view>`, or
`person<rohit::serializer::storage_mode::mutable_view>`. Only requested modes exist.
With one mode, use `person` directly.

Generated views map **little-endian positional binary (`binary_none`)** through
`person::map(span, limits)`. Getters access mapped fields; mutable setters update
existing bytes without changing field sizes. Strings require the same byte length.
Arrays, maps, nested objects, and active union alternatives have borrowed accessors.
Array and map views provide sequential `begin()`/`end()` iterators. Prefer range-based
loops for variable-length collections to avoid rescanning earlier entries on each index.

See [the view usage guide](views.md) for examples, nested-mode requirements, lifetimes,
mutation limits, and compilation costs.

All binary codecs default to **little-endian** fixed-width integers and floating
point. Inspect `Protocol::wire_endian` or `View::wire_endian` in C++; views also
expose `key_type`. Explicit codec byte-order selection is documented in
[the wire-format contract](wire_format.md#byte-order).
Messages contain no automatic byte-order marker. Both endpoints must agree on it.

Wire byte order is independent of machine byte order: the same scalar values use
identical wire bytes on little- and big-endian machines. Collection ordering is a
separate contract: high-byte `map(char)` keys retain historical backend/compiler
ordering, so equivalent maps need not produce identical bytes. For new portable
byte-keyed maps, prefer `map(uint8)`; see [map ordering](wire_format.md#map-ordering).

Other language backends currently use the little-endian wire profile; explicit
big-endian wire selection is a C++ feature. The [interoperability
runner](../example/interoperability/README.md) pins positional output to shared frozen
bytes and can include emulated big-endian C/C++ hosts. See [cross-endian
verification](verification-cross-endian-2026-09-18.md) for the exercised matrix and its
scope.

Strings require valid UTF-8 in every language, including C++ binary codecs and mapped
views. Embedded NULs remain valid; use `array uint8` for arbitrary bytes. Older C++
binary strings containing invalid UTF-8 are now rejected; see
[migration](../migration.md#binary-string-validation). Validation is strict by default.
C++ owning binary codecs also offer an explicit compile-time
`binary_text_validation::unchecked` policy for already validated or otherwise
guaranteed-valid text.

It removes runtime text scans, not bounds or resource checks. Invalid UTF-8 remains
outside the cross-language contract. See [policy
selection](usage.md#choose-c-binary-text-validation); JSON, mapped views, and other
language runtimes remain strict. Rebuild the runtime and C++ consumers together; schema
regeneration is unnecessary for this policy.

Independent legacy big-endian and current little-endian fixtures exercise explicit
protocol selection. The schema-language version does not identify message byte
order or wire version. See the [verification record](verification-2026-09-17.md).

### Explicit field IDs with `stable_ids`

`stable_ids` tells the generator: **every field and parent must have an explicit
numeric ID**. It is useful for schemas that evolve while using integer-key binary.

Without explicit IDs, numbers follow declaration order: `name` above gets ID 1
and `age` gets ID 2. Inserting a field before them changes those implicit IDs.
Assigning IDs explicitly preserves their identity when declarations move:

```text
class person stable_ids {
  public string name (1);
  public uint32 age (2);
}
```

Adding `public string country;` to that class fails generation with
`stable_ids requires explicit field and parent IDs`. Assign a new unused ID:

```text
public string country (3);
```

Explicit IDs already work without the keyword; `stable_ids` prevents accidentally
omitting one. Adding the keyword to an existing schema should preserve its current
IDs, including any previously implicit IDs. It does not compare schema history or
prevent a person from manually changing or reusing an ID.

Parents and members share the containing class's ID space. A parent's own fields
have their own ID space:

```text
class employee stable_ids : public person ("person", 1) {
  public uint64 employee_id (2);
}
```

IDs must be unique within that class and in `1..0x3fffffff`; zero ends an
integer-key object. JSON and string-key binary identify fields by their wire
names. Positional binary still depends on declaration order even with explicit IDs.
Keyed readers reject unknown fields, so stable IDs alone do not make old readers
accept added fields. See [schema evolution](wire_format.md#schema-evolution).

Use the separate [schema compatibility checker](schema_evolution.md) to
compare revisions and enforce a persistent policy of reserved field IDs/names:

```sh
serializer --input current.serializer --check-against previous.serializer \
  --compatibility-protocol binary_integer --compatibility-direction backward \
  --compatibility-policy compatibility.json
```

It checks identity/type changes, positional order, and enum/union ordinals, and
distinguishes native unknown-field rejection from Protobuf binary skipping.
This does not introduce a new native wire format or a `reserved` schema keyword.

`stable_ids` is independent of `view` and is not required for mapping buffers.
Views use positional binary, so field order and types must still match.
`inplace` and `simd` are not implemented schema keywords.

### Datatypes

|Type|C++ Type|Size byte|Common name|
|---|---|---|---|
|char|char|1|Character|
|int8|int8_t|1|integer|
|int16|int16_t|2|integer|
|int32|int32_t|4|integer|
|int64|int64_t|8|integer|
|uint8|uint8_t|1|unsigned integer|
|uint16|uint16_t|2|unsigned integer|
|uint32|uint32_t|4|unsigned integer|
|uint64|uint64_t|8|unsigned integer|
|float|float|4|Floating point|
|double|double|8|Floating point|
|bool|bool|1|Bool|
|string|std::string|variable|String|

## Collection types

Use `array Type` for a sequence and `map(KeyType) ValueType` for a keyed collection.
See the [schema examples](schema_examples.md) for complete declarations.

## Comments

Use `//` for a line comment and `/* ... */` for a block comment.

## Serialization protocols

Choose JSON, positional binary, integer-key binary, or string-key binary through
the protocol template. The examples below assume an object named `pr` and a stream.

Read and write JSON:

```cpp
pr.serialize_out<rohit::serializer::json>(stream);
pr.serialize_in<rohit::serializer::json>(stream);
```

Write formatted JSON:

```cpp
rohit::serializer::json_out<true> json_out { stream, rohit::serializer::format::beautify };
pr.serialize_out(json_out);
```

Three predefined formats are available:

- `rohit::serializer::format::compress`
- `rohit::serializer::format::beautify`
- `rohit::serializer::format::beautify_vertical`

Use `rohit::serializer::write_format` to define a custom format.

`format::compress` selects compact JSON formatting; it does not compress data. For exact
fresh-value decoding, use `rohit::serializer::deserialize_exact<Value, Protocol>(input[,
limits])`. It decodes a candidate and calls `finish()` before returning; input may still
be consumed on failure. See [the helper's
contract](usage.md#decode-one-exact-message-into-a-fresh-value). Existing member and
static decoding APIs retain their behavior. For wire details, schema evolution, decoder
limits, and failure behavior, see [the wire-format contract](wire_format.md).

Optional timing, allocation, and fuzz targets are described in
[qualification](../qualification/README.md).

`SERIALIZER_BUILD_FUZZERS=ON` builds a separate runtime with libFuzzer coverage,
AddressSanitizer, and UndefinedBehaviorSanitizer, including compiled SIMD helpers.
Four targets cover native codecs, mapped views, optional Protobuf codecs, and
scalar/baseline/dispatched SIMD comparisons. A deterministic corpus generator and
CTest seed replay work without GoogleTest. See [fuzzing](../qualification/README.md#fuzzing)
for Clang prerequisites, corpus controls, bounded campaigns, and verification scope.

Positional binary writes fields in schema order without field IDs, names, or an
object terminator. Both ends must agree on field order and types. Unions still
write an alternative index before their payload.

```cpp
pr.serialize_out<rohit::serializer::binary_none>(stream);
pr.serialize_in<rohit::serializer::binary_none>(stream);
```

Integer-key binary writes a compact numeric ID before each field and ID zero
after each object. IDs occupy one to four bytes and must be in `1..0x3fffffff`.
Schema declarations can specify an ID, for example `public uint8 count (3);`.

```cpp
pr.serialize_out<rohit::serializer::binary_integer>(stream);
pr.serialize_in<rohit::serializer::binary_integer>(stream);
```

String-key binary writes a length-prefixed wire name before each field and an
empty-name terminator after each object. The wire name defaults to the member
name; a declaration such as `public uint8 count ("total", 3);` overrides both its
wire name and numeric ID. Empty wire names are rejected. Union names include the
selected alternative as `field:alternative`.

```cpp
pr.serialize_out<rohit::serializer::binary_string>(stream);
pr.serialize_in<rohit::serializer::binary_string>(stream);
```

For one `uint8` field with value `7`, numeric ID `3`, and wire name `total`:

| Mode | Encoded bytes (hex) |
| --- | --- |
| Positional | `07` |
| Integer-key | `03 07 00` |
| String-key | `05 74 6f 74 61 6c 07 00` |

Only IDs, lengths, numeric enum values, and union indices use the compact integer
encoding. Ordinary integer fields retain their declared byte width. Negative
compact integers and values above `0x3fffffff` throw `std::out_of_range` before
writing that integer.

## Native JSON host types

C++ generic fields may use `std::optional<T>` when encoding and decoding native
JSON; an empty value is JSON `null`. This is a host type capability, not a new
schema type or binary encoding. Evolving JSON readers can explicitly select
`json_read_policy::compatible`; regenerate classes for the unknown-member hook.
See [JSON compatibility](usage.md#json-compatibility).
