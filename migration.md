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
