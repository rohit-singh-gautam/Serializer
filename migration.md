# Migrating to the snake_case Serializer API

## Nested destination storage reuse

Regenerate C++ owning headers with the updated compiler to reuse buffers in
generated objects inside arrays and maps. Existing input calls and schema syntax
remain valid; native JSON/binary wire bytes are unchanged. The runtime uses typed
generated donor support internally. Older headers continue decoding through the
fresh-element fallback, while direct string/container reuse is runtime-provided.
The generator reserves `serializer_reuses_storage` and `StorageSource` for its
support declarations; conflicting generated C++ names are diagnosed. Collection
elements still use schema defaults for missing fields. Review
[reuse and failure behavior](docs/usage.md#reuse-destination-storage) before relying
on retained capacity or partial updates. Generation/build/test execution for this
optimization remains deferred.

## Optional Protobuf protocols

Regenerate C++ headers with `--cpp.protobuf true` (or `[cpp] protobuf = true`) to
use `protobuf_binary`, `protojson`, or `textproto` through the existing protocol
template API. Existing protocol aliases and wire formats are unchanged. Protobuf
input uses standard defaults for absent fields, not schema member initializers;
field IDs, maps, names, and union mappings have additional restrictions. Review
[the Protobuf contract](docs/protobuf.md) before changing an application's protocol.

## Versioned schema files and compiler options

Rename `.def` and `.struct` schemas to `.serializer` and add `serializer version 1;`
as the first statement. Update schema paths in scripts and CMake. All repository
examples and fixtures use this format. Legacy file extensions and headerless files
are rejected by the executable; the library parser still accepts headerless fragments
unless called as `parser::parse(input, true)`.

Replace bare CLI keys with long options: `input` becomes `--input`, `output` becomes
`--output`, `config` becomes `--config`, and `cpp.naming` becomes `--cpp.naming`.
The CMake helpers already use the updated interface. For both languages, pass
`--language cpp,java --cpp.output account.hpp --java.output AccountSchema.java`.
Each backend's settings can be overridden independently on the command line.
`serializer --version` reports compiler release 0.1.0 and schema language version 1.
These are separate version domains; neither changes wire bytes. See
[the complete CLI contract](docs/command_line.md).

## Java output and example folders

Java generation is an opt-in backend (`--language java`) and does not change the
default C++ API or wire layout. See [Java output](docs/java.md) for supported
features, Java naming, parent composition, and differences from C++ ownership.

The configuration schema moved to `example/config/config.serializer`. C++ coding-style
examples now live in `example/coding_styles/<profile>/`, each with its own
`account.serializer`, `<profile>.ini`, and `<profile>.cpp`. Java examples live in
`example/java/`, with a separate folder for the round trip and each Java profile.
Update scripts that referenced the earlier shared schema or flat example paths.

## C++ source migration

This is a breaking C++ source migration. Update callers and regenerate schema
headers with the updated `serializer` executable before compiling. Legacy
headers and symbol aliases are not provided.

## Output profiles and generated names

Generation now defaults to the repository's C++ naming and clang-format rules.
For example, a schema field `ID` becomes the C++ member `id`, `reverseListMap`
becomes `reverse_list_map`, and type `IP` becomes `ip`. JSON/string-key names and
enum/union wire spellings retain their schema values. IDs, binary layout, and
endianness are unaffected by selecting an output profile.

Use `--cpp.naming preserve` (or `[cpp] naming = preserve` in the output config) to
retain previous schema-derived C++ identifiers. Otherwise update callers along
with regenerated headers. Profiles also rename union support types, discriminator
members, conversion helpers, and view accessors; runtime-required hooks keep their
existing spelling. Name collisions and reserved C++ identifiers are diagnosed.
Literal and declared-enum defaults are supported during renaming; opaque C++
expressions require `naming = preserve` or a rewritten default.

Install clang-format 19+ for generation. Set `--cpp.clang_format` on the CLI or
`SERIALIZER_CLANG_FORMAT_EXECUTABLE` for the repository's CMake generation rules
when it is not on `PATH`. Formatting failures do not replace the destination
header. CLI errors now return nonzero and unknown arguments are rejected.

See [output configuration](docs/output_configuration.md) and the
[profile examples](example/coding_styles/README.md). Generation, compilation,
and regression testing for this step remain deferred.

## Generated representations and byte order

Binary fixed-width integers and floating-point values now default to little-endian.
Compact prefixes retain their existing encoding. This is an intentional wire
change with no compatibility aliases or automatic fallback. Applications that
require a specific byte order can set the third `binary` template parameter and
inspect `wire_endian`; see [the wire contract](docs/wire_format.md#byte-order).

Schemas can request `view`, restrict it with `readonly`/`mutable`, and add `owning`.
One mode retains a concrete class name; multiple modes require `person<storage_mode>`.
Owning nested fields and parents select the owning specialization. Nested types
must enable the modes requested by their owners. Views map little-endian positional
binary and expose size-preserving getters/setters. See [views.md](docs/views.md).
These source changes have not yet been generated, built, or tested for this step.

## Headers and build targets

| Previous | Current |
| --- | --- |
| `<rohit/stream.h>` | `<rohit/stream.hpp>` |
| `<rohit/serializer.h>` | `<rohit/serializer.hpp>` |
| `<rohit/serializercreator.h>` | `<rohit/serializer_creator.hpp>` |
| CMake target `serializerlib` | `serializer_lib` |
| Test target `CoreSerializerTest` | `core_serializer_test` |

The `serializer` executable name and its `input` / `output` arguments are
unchanged. Generated test headers now use `.hpp`. Link to `serializer_lib` to
inherit the include directory and C++20 requirement. Tests are enabled by default
for a standalone build and disabled by default when included as a subdirectory;
set `SERIALIZER_BUILD_TESTS` explicitly to override this.

Runtime JSON scanning and binary array conversion now call compiled helpers.
Applications that previously included runtime headers without linking Serializer
must link `Serializer::serializer_lib`, even when using pre-generated headers or
building with `SERIALIZER_ENABLE_SIMD=OFF`. The shipped `serializer_generate`
helper already supplies this dependency. SIMD preserves wire bytes and requires
no schema or output-profile changes; see [runtime SIMD](docs/runtime_simd.md).

The minimum language mode is C++20. CMake defaults to at least that version,
requires the selected standard, and disables compiler extensions for this
project's targets. Higher `CMAKE_CXX_STANDARD` settings remain effective. Direct
header users must also enable C++20 or later; the header check uses `_MSVC_LANG`
on MSVC to accommodate its default `__cplusplus` reporting.

`byteswap` and `change_endian` now share scalar constraints: booleans, integers
without padding bits, and supported 32-bit/64-bit binary IEC 559 floating-point
representations. Other types are rejected even when source and destination byte
orders match. `change_endian` accepts only little/big byte orders; `native` is
accepted when it aliases one of those orders. Both helpers are `constexpr` and
`noexcept`. Integers forward directly to `std::byteswap` when the feature is
available, with a C++20 fallback otherwise.

## Public C++ names

Namespaces remain rooted at `rohit`. Library-owned names use `snake_case`.

| Previous | Current |
| --- | --- |
| `Stream`, `FullStream` | `stream`, `full_stream` |
| `FullStreamAutoAlloc` | `full_stream_auto_alloc` |
| `FullStreamAutoAllocLimits` | `full_stream_auto_alloc_limits` |
| `FullStreamLimitChecked` | `full_stream_limit_checked` |
| `StreamAutoFree`, `FixedBuffer` | `stream_auto_free`, `fixed_buffer` |
| `streamlimit_t` | `stream_limits` |
| `MinReadBuffer`, `MaxReadBuffer` | `min_read_buffer_bytes`, `max_read_buffer_bytes` |
| `MakeStream`, `MakeConstantStream` | `make_stream`, `make_constant_stream` |
| `MakeConstantFullStream`, `MakeStreamFromFile` | `make_constant_full_stream`, `make_stream_from_file` |
| `CurrentOffset`, `RemainingBuffer`, `Reset` | `current_offset`, `remaining_buffer`, `reset` |
| `Write`, `WriteRaw`, `Append`, `Reserve` | `write`, `write_raw`, `append`, `reserve` |
| `Hash`, `ChangeEndian` | `hash`, `change_endian` |
| `typecheck` | `type_check` |
| `SerializeType::In`, `SerializeType::Out` | `serialize_type::in`, `serialize_type::out` |
| `SerializeKeyType::{None,Integer,String}` | `serialize_key_type::{none,integer,string}` |
| Protocol member `serialize_key_type` | `key_type` |
| `JsonOut` | `json_out` |
| `binaryInBase`, `binaryOutBase` | `binary_in_base`, `binary_out_base` |
| `SerializeIn`, `SerializeOut` | `serialize_in`, `serialize_out` |
| `StructSerializeIn`, `StructSerializeOut` | `struct_serialize_in`, `struct_serialize_out` |
| `SerializeInMemberByName` | `serialize_in_member_by_name` |
| `SerializeInMemberByIdentifier` | `serialize_in_member_by_identifier` |
| `GetStream` | `get_stream` |
| `write_format::intendtext` | `write_format::indent_text` |

Apply the same word conversion to the other stream/protocol helpers and exception
types, for example `BadInputData` becomes `bad_input_data`. The protocol templates
`json`, `binary_none`, `binary_integer`, and `binary_string` retain their names.
Custom serializer protocols and handwritten serializable types must implement
the renamed methods and expose `key_type` with the new enum type.
For custom protocols, `key_type` must be a static constant expression set to
`none`, `integer`, or `string`. Generated methods select the field operations with
`if constexpr` and reject unsupported modes at compile time.

## Binary field keys

The three binary modes retain their field-key encodings, with fixed-width values
using the byte order described above:

- `binary_none` writes field values in schema order without field identifiers or
  an object terminator. Unions retain a compact alternative index, including zero.
- `binary_integer` writes compact field IDs and ends each object with ID zero.
- `binary_string` writes length-prefixed wire names and ends each object with an
  empty name. Union keys retain the `field:alternative` spelling.

Schema parsing now rejects field and parent IDs outside `1..0x3fffffff` and empty
wire names. Decimal IDs are parsed without narrowing; excessively large tokens
raise `bad_member_spec` instead of overflowing or truncating. Existing default
ID assignment and explicit valid ID/name overrides remain unchanged.

For handwritten serializers, integer-key fields reject ID zero and string-key
fields reject empty names with `std::invalid_argument`. Numeric and named field
overloads now reject incompatible key modes at compile time. Positional union
discriminators still use the numeric pair overload, where zero is valid.

`serialize_out_variable` now throws `std::out_of_range` for negative values and
values above `0x3fffffff`, before writing any bytes for that integer. This also
applies to lengths, enum values, and union indices that use the same encoding.
Failures do not roll back previously written fields or prefixes. Ordinary
fixed-width signed integer fields continue to support negative values.

## Decoder, JSON, and generator qualification changes

Generated keyed fields and union payloads now hold references in their temporary
pairs/tuples. Serialization does not copy or move the source field. Generated
enum output borrows its spelling; the existing owning `to_string` API remains.
Regenerate headers through the generator when the build pass is authorized.
Custom output protocols need `struct_serialize_out_empty()` for empty generated
objects. Named enum input can optionally provide `serialize_in_named_enum(value)`;
custom protocols without it retain the owning-string fallback.

Binary scalar input now copies bytes into aligned storage and converts byte order
through unsigned representations. String-key input borrows field names during
dispatch. Binary vectors validate their count and storage budget before one
reservation, and strings assign directly from validated input spans.

Strings, vectors, and maps now **replace** existing contents on input. Vectors
move decoded elements and reuse capacity; maps retain ordered semantics and use
the last complete duplicate-key entry. Objects remain partial updates: missing
fields retain their destination values, and repeated fields apply in order.
Failures retain earlier writes, consumed prefixes, completed fields/entries, and
budget charges. This is not whole-object atomic decoding.

Both input protocols accept `decode_limits` as a second constructor argument and
share its accounting through nested values. Defaults bound consumed bytes, string
length, collection/object counts, nesting depth, accounted storage, and work.
Protocols are noncopyable; use a reference for nesting and a new protocol per new
message budget. `finish()` opts into exact-message validation. Generated stream
convenience overloads continue to read one value without requiring end-of-input.
Input bytes must remain independent of mutable destination storage.

JSON now validates strings, escapes, Unicode, punctuation, number grammar, and
range. Mixed-case booleans, leading plus signs/zeroes, trailing commas, invalid
UTF-8, unpaired surrogates, numeric overflow, and malformed suffixes are rejected.
Output escapes names/strings. Floating output uses shortest general-format
`std::to_chars`, so text such as `3.140000` becomes `3.14`; non-finite values are
rejected. Empty objects and `std::nullptr_t` values are supported. JSON map wire
shape remains an array of `key`/`value` entries.

Schema parsing rejects duplicate IDs/names, explicit/implicit ID collisions,
duplicate enum/union alternatives, and unknown class attributes. `stable_ids`
requires explicit IDs for every member and parent in that class. Generated name
dispatch groups hash collisions and checks full equality. Numeric enum input
checks generated enum membership. Union readers validate alternatives and begin
the selected member's lifetime before decoding; raw union payloads must be
trivially destructible. Generated serializer methods are public even when a
class is empty or its last data member is private. The generator exits with a
failure status when schema parsing or writing fails.

Parser exceptions expose a structured `code()` and omit input excerpts by default.
Use `diagnostic_options` or `decode_limits::diagnostics` for bounded, escaped,
opt-in excerpts. `format::compress` means whitespace suppression, not compression.
Recompile consumers after these decoder and exception-interface changes.

See [the wire-format contract](docs/wire_format.md) for exact limits, exception
categories, lifetime rules, and schema-evolution requirements, and
[qualification tools](qualification/README.md) for the prepared validation pass.

## Stream safety and ownership

Owning `stream_auto_free` and `full_stream_auto_alloc` objects are now move-only;
`full_stream_auto_alloc_limits` remains noncopyable and supports ownership moves.
Use `std::move` to transfer ownership or a borrowed stream view to share access.
Pointer-taking owning constructors still require exclusively owned,
malloc-compatible storage. `fixed_buffer` also supports move assignment.

Forward cursor movement and dereferencing now reject exhausted buffers before
changing state; reaching the end exactly is allowed. All `full_stream` variants
check backward movement. `at_unchecked()`, direct pointer mutation, and backward
movement on a base `stream` retain caller-validated preconditions. Borrowed stream
assignment copies the complete view; assignment through a const base view only
commits a cursor sharing the same buffer end.

For a batch, validate or reserve the complete byte count once, then use the
nonvirtual `push_unchecked`, `append_unchecked`, `advance_unchecked`,
`get_curr_and_increase_unchecked`, or existing `at_unchecked` helpers. They do not
check bounds or allocate, including on an allocating stream. The caller must keep
all accesses within the validated range; growth invalidates borrowed pointers.

```cpp
// data is a byte array whose storage is independent of output's allocation.
output.reserve(data.size());
for (const auto byte : data) {
  output.push_unchecked(byte);
}
```

Byte-only `write_raw(...)` calls now reserve once for the whole argument pack.
Their bytes are captured before reservation, so byte arguments may refer to the
output allocation even if it grows. Insufficient capacity rejects the batch
before writing any of its bytes. Mixed argument packs retain per-argument appends.
Binary input uses unchecked consumption after its existing range validation;
three- and four-byte variable integers now require exactly two and three bytes
after their first byte, correcting the previous oversized remaining-byte checks.

`write(...)` now batches multiple characters, booleans, character arrays, strings,
and string views into one reservation. For example, `write('"', value, '"')`
reserves the complete quoted text. Boolean text remains `1` or `0`. Numeric
mixtures and single arguments retain their existing formatting paths. Lengths
and scalar bytes are captured first; text ranges are copied in argument order
with overlap support. The built-in allocating streams rebase internal sources
after growth. A length, source-range, or reservation failure occurs before the
batch writes any bytes.

The protected `reserve_fragments` hook handles batched source rebasing. Custom
streams that relocate storage must handle this hook as well as `reserve_append`;
the built-in allocating streams provide both. Each batch still invokes virtual
`reserve` once so custom limits remain effective. Recompile consumers after this
addition to the virtual interface.

`append_external(source, size)` skips source-alias bookkeeping for independently
owned data. The source must not overlap the stream's backing storage or become
invalid during reservation. General `append` retains rebasing and overlap support.
Integer text output and binary scalar output use the independent-source path;
integer text still reserves only its actual formatted size.

Prefix comparisons now compare bytes consistently on signed-char platforms,
including bytes above `0x7F`. Empty prefixes and literal terminators retain their
existing behavior. Pointer helpers rely on the C++20 rules for null-plus-zero and
null-pointer subtraction while retaining invalid-range checks.

Bounded allocating streams copy their supplied limits; callers no longer need to
keep the limits object alive, and later edits to that object do not alter an
existing stream. Invalid policies are rejected. Zero-capacity streams allocate
lazily, and zero-byte operations on empty storage are valid.

Bounded reservations reuse the validated cursor offset and capacity and perform
growth calculations only when necessary. Adopted storage can be larger than the
logical maximum; reservations still enforce that maximum.

File helpers now throw on failed or incomplete I/O instead of returning partial
input or silently accepting failed output. Literal comparison and explicit
literal hashing exclude the final string terminator. These corrections require
the deferred build and regression verification before release.

## Parser and writer API

| Previous | Current |
| --- | --- |
| `Parser::Parse` | `parser::parse` |
| `Writer::CPP::Write` | `writer::cpp::write` |
| `Base`, `Namespace`, `Class`, `Enum` | `syntax_node`, `namespace_node`, `class_node`, `enum_node` |
| `Member`, `Parent`, `TypeName` | `member`, `parent`, `type_name` |
| `AccessType`, `ObjectType`, `ClassAtributes` | `access_type`, `object_type`, `class_attributes` |
| `AccessType::{Private,Protected,Public}` | `access_type::{private_access,protected_access,public_access}` |
| `ObjectType::{Namespace,Class,Enum}` | `object_type::{namespace_type,class_type,enum_type}` |
| `Member::{none,array,map,Union}` | `member::modifier_type::{none,array,map,variant}` |
| `Name`, `MemberList`, `statementlist`, `parentlist` | `name`, `member_list`, `statements`, `parents` |
| `modifer`, `typeNameList`, `displayName` | `modifier`, `type_name_list`, `display_name` |
| `declaredNameSpace`, `definedNameSpace` | `declared_namespace`, `defined_namespace` |

## Generated code and serialized data

Generated protocol methods now use the renamed API and two-space indentation.
Schema-defined identifiers keep their spelling: a schema member named `ID`
still generates a member named `ID`. The generator does not silently rewrite
user-defined classes, enum values, JSON keys, custom member names, or numeric IDs.
The existing fixtures deliberately retain mixed-case names to test that contract.

The API renaming alone preserves JSON output, binary layouts, field order, and
discriminator values. The later qualification changes above intentionally update
JSON formatting/parsing and internal name dispatch while preserving valid binary
wire representations. Renaming a schema member itself may change its default
wire name; retain an explicit name modifier when preserving that external contract.

```cpp
#include <rohit/serializer.hpp>
#include "person.hpp"

test::test1::person person{"Ada", 322};
rohit::full_stream_auto_alloc output{256};
person.serialize_out<rohit::serializer::json>(output);

const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
test::test1::person decoded{};
decoded.serialize_in<rohit::serializer::json>(input);
```

Use the checked-in `.clang-format` for future C++ edits. The repository's rules are
documented in [CodingStandard.md](CodingStandard.md).
