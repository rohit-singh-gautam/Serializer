# Serializer

A C++20 schema compiler and serialization library supporting JSON and three
binary protocols. The public API and generated methods use `snake_case` names.

**Existing callers:** follow the [migration guide](migration.md) and regenerate
headers before compiling against the updated API.

**Getting started:** the [usage guide](docs/usage.md) covers schema authoring,
automatic header generation with CMake, and decoding an exact message with limits.
For AI-assisted integration, use the [repository skill](#repository-agent-skill).

## Build and test

With CMake, a C++20-or-newer compiler and standard library, and GoogleTest available:

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The existing platform presets use vcpkg through `VCPKG_ROOT`. On Windows:

```sh
cmake --preset DebugWindows
cmake --build --preset DebugWindows
ctest --test-dir out/build/DebugWindows -C Debug --output-on-failure
```

When embedding the library, add this repository with `add_subdirectory` and link
to `serializer_lib`. It supplies the public include path and C++20 requirement.
Use `-DSERIALIZER_BUILD_TESTS=OFF` for a standalone build without GoogleTest.

C++20 is the minimum language mode and the build default. A newer mode selected
with `CMAKE_CXX_STANDARD`, such as `-DCMAKE_CXX_STANDARD=23`, is preserved. Public
headers also check the language mode when used outside CMake.

The `rohit::byteswap` helper forwards supported integers directly to
`std::byteswap` when `__cpp_lib_byteswap >= 202110L`; otherwise it uses a constexpr
C++20 fallback. Supported floating-point values are converted through their
integer bit representations. `rohit::change_endian` accepts supported scalar
types and little/big byte orders, with compile-time rejection of unsupported
types and mixed native byte order. Booleans are unchanged.

Generate a header by running the built executable:

```sh
serializer input example/config.struct output config.hpp
```

Format C++ files with the repository's `.clang-format`; see
[CodingStandard.md](CodingStandard.md) for naming and coding rules.

## Language Construct

### Namespace
This directly maps to C++ name space this can be hierarchical. This can be similar to C++ syntax like "A::B::C".
```
namespace A { namespace B { } }
```
is equivalent to
```
namespace A::B { }
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
See [the view usage guide](docs/views.md) for examples, nested-mode requirements,
lifetimes, mutation limits, and compilation costs.

All binary codecs default to **little-endian** fixed-width integers and floating
point. Inspect `Protocol::wire_endian` or `View::wire_endian` in C++; views also
expose `key_type`. Explicit codec byte-order selection is documented in
[the wire-format contract](docs/wire_format.md#byte-order).
Messages contain no automatic byte-order marker. Both endpoints must agree on it.

These changes are implemented in source; generation, builds, and tests remain
deferred for this step.

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
accept added fields. See [schema evolution](docs/wire_format.md#schema-evolution).

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
Following collecting types are supported
1. Array
1. Map

## Comments
"//" till new line and anything under "/*" and "*/" will be ignore.

## Serializer Type
Output is template based, hence one of following serializer can be used:
1. JSON
1. Binary
  1. Positional Binary
  1. ID based indexing
  1. String based indexing

if ```cpp test::person pr``` is name of your class different serializer can be applied as follows:

JSON Serializer support.
```cpp
pr.serialize_out<rohit::serializer::json>(stream);
pr.serialize_in<rohit::serializer::json>(stream);
```

JSON Output Serializer support with beautification
```cpp
rohit::serializer::json_out<true> json_out { fullstream1, rohit::serializer::format::beautify };
pr.serialize_out(json_out);
```

There are three predefined format
1. ```cpp rohit::serializer::format::compress ```
1. ```cpp rohit::serializer::format::beautify ```
1. ```cpp rohit::serializer::format::beautify_vertical ```

More can be generated using structure ```cpp rohit::serializer::write_format ```

`format::compress` selects compact JSON formatting; it does not compress data.
For wire details, schema evolution, decoder limits, and failure behavior, see
[the wire-format contract](docs/wire_format.md). Optional timing, allocation,
and fuzz targets are described in [qualification](qualification/README.md).

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

## Example
### Simple class
Below input:
```cpp
namespace test {
class person {
    public string name;
    public uint64 ID;
}
}
```
This will result in:
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.hpp>

namespace test {
class person {
public:
  std::string name { };
  std::uint64_t ID { };

  template <typename SerializeOutProtocol>
  void serialize_out(SerializeOutProtocol& serializer_protocol) const;
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_out(rohit::stream& stream) const;
  template <typename SerializeInProtocol>
  void serialize_in(SerializeInProtocol& serializer_protocol);
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_in(const rohit::stream& stream);
}; // class person
}
```

### Array
```cpp
namespace arraytest {
class person {
    public string name;
    public uint64 ID;
}

class personlist {
    public uint64 listid;
    public array person list;
}
}
```

Above input will generate:
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.hpp>

namespace arraytest {
class person {
public:
  std::string name { };
  std::uint64_t ID { };

  template <typename SerializeOutProtocol>
  void serialize_out(SerializeOutProtocol& serializer_protocol) const;
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_out(rohit::stream& stream) const;
  template <typename SerializeInProtocol>
  void serialize_in(SerializeInProtocol& serializer_protocol);
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_in(const rohit::stream& stream);
}; // class person

class personlist {
public:
  std::uint64_t listid { };
  std::vector<person> list { };

  template <typename SerializeOutProtocol>
  void serialize_out(SerializeOutProtocol& serializer_protocol) const;
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_out(rohit::stream& stream) const;
  template <typename SerializeInProtocol>
  void serialize_in(SerializeInProtocol& serializer_protocol);
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_in(const rohit::stream& stream);
}; // class personlist

} // namespace arraytest
```

### Map
```cpp
namespace maptest {
class person {
    public string name;
    public uint64 ID;
}

class personlist {
    public uint64 listid;
    public map(uint64) person list;
}
}
```
Above code will result in below C++ code
```cpp
/////////////////////////////////////////////////////////
// This is auto genarated file using serializer. Must  //
// not be manually edited. For more information refer  //
// to https://github.com/rohit-singh-gautam/Serializer //
/////////////////////////////////////////////////////////
#pragma once
#include <rohit/serializer.hpp>

namespace maptest {
class person {
public:
  std::string name { };
  std::uint64_t ID { };

  template <typename SerializeOutProtocol>
  void serialize_out(SerializeOutProtocol& serializer_protocol) const;
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_out(rohit::stream& stream) const;
  template <typename SerializeInProtocol>
  void serialize_in(SerializeInProtocol& serializer_protocol);
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_in(const rohit::stream& stream);
}; // class person

class personlist {
public:
  std::uint64_t listid { };
  std::map<std::uint64_t, person> list { };

  template <typename SerializeOutProtocol>
  void serialize_out(SerializeOutProtocol& serializer_protocol) const;
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_out(rohit::stream& stream) const;
  template <typename SerializeInProtocol>
  void serialize_in(SerializeInProtocol& serializer_protocol);
  template <template<rohit::serializer::serialize_type> class SerializerProtocol>
  void serialize_in(const rohit::stream& stream);
}; // class personlist

} // namespace maptest
```

### Enum
This is a specialize case where in string and JSON mode value name will be serialized in other binary mode its positional ID will be serialized.

Example of CPP:
```cpp
namespace enumtest {
enum testenum {
    test1,
    test2,
    test3,
    test4,
    test5,
    test6
}

class test {
    public testenum te;
}
}
```

### Member Modifier
Member can have custom numeric ID for indexing with integer or custom string for indexing with string. This can be done by adding number or a string in double quote inside a round bracket after definition of member variable.

Example:
```cpp
namespace test {
class person {
    public string name ("Name", 3);
    public uint64 ID ("id", 4);
}
}
```

Similar parameter can also be added for parent class definition example:
```cpp
namespace test {
class personex : public person ("Person", 5) {
    public uint64 ID ("id", 6);
}
}
```

### Default value
Default value can be added for member variable by adding a value in braces after definition of member variable example:
```cpp
namespace test {
class person {
    public string name {"None"}("Name", 3);
    public uint64 ID ("id", 4) { 4 };
}
}
```

## Roadmap
1. Check for validity for default value.
1. Store position of member variable in input stream.
1. Bit field.

## Repository agent skill

The [serializer-integration skill](.agents/skills/serializer-integration/SKILL.md)
guides an agent through using this library in a C++ application: schema design,
`stable_ids` adoption, generated headers, protocol selection, and bounded input.

It follows the open [Agent Skills format](https://agentskills.io/specification):
a folder with a `SKILL.md` containing YAML `name` and `description`, followed by
Markdown instructions. Repository discovery locations are host-specific. This
repository uses Codex's `.agents/skills` convention:

```text
.agents/skills/serializer-integration/
  SKILL.md
  agents/openai.yaml
```

The YAML file under `agents/` supplies optional Codex UI metadata. Codex discovers
repository skills from the working directory through the repository root. In the
CLI or IDE extension, select this skill with `$serializer-integration` or `/skills`.
See [Codex skill discovery](https://learn.chatgpt.com/docs/build-skills#where-codex-loads-local-skills).

Example request:

```text
Use $serializer-integration to add Serializer to my C++ application,
define a person schema with stable_ids, and decode integer-key binary
messages with explicit resource limits.
```

When Serializer is a nested dependency, its skill is not automatically discovered
from the consuming project's root. Copy the complete skill folder into that
project's `.agents/skills`, or explicitly ask the agent to read the dependency's
`SKILL.md`. Keep the matching Serializer checkout available for the linked docs;
the skill explains how to locate it when copied. Other compatible agents use the
same skill format with their own discovery and invocation rules.

[AGENTS.md](AGENTS.md#usage-documentation-and-repository-skill) requires changes to
keep this README and the skill current together.
