# Serializer

A C++20 schema compiler and serialization library supporting JSON and three
binary protocols. The public API and generated methods use `snake_case` names.

**Existing callers:** follow the [migration guide](migration.md) and regenerate
headers before compiling against the updated API.

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
namespace A { namepace B { } }
```
is equivalent to
```
namespace A::B { }
```

### Struct
By default all the members are public.

Syntax:
```
struct <name> packed : <public|private|protected> <parent> {
<public|private|protected> [array|map] <type> <variable>;
};
```

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
