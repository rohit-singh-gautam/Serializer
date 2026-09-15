# Migrating to the snake_case Serializer API

This is a breaking C++ source migration. Update callers and regenerate schema
headers with the updated `serializer` executable before compiling. Legacy
headers and symbol aliases are not provided.

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

JSON output, binary layouts, field order, discriminator values, and identifier
hashing are preserved by this migration. Renaming a schema member itself may
change its default wire name; retain an explicit name modifier when preserving
that external contract.

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
