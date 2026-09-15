# Using Serializer in a C++ application

Serializer compiles a schema into a C++ header, then reads or writes generated
objects through JSON or binary protocols. Use a C++20-or-newer compiler and
standard library; the CMake project requires CMake 3.28 or newer. Header generation
uses clang-format 19+ by default. Applications using existing headers do not need it.

The examples below describe the current source API. They have been reviewed
against the source but have not been generated, compiled, or executed as part of
this documentation change. See [qualification](../qualification/README.md) for
the deferred validation work.

## 1. Define a schema

Save this as `schemas/person.def` in your application:

```text
namespace demo {
class person stable_ids {
  public string name (1) { "Unknown" };
  public uint32 age (2) { 0 };
  public array uint32 scores (3);
}
}
```

- Use `class`, `enum`, and `namespace`; schema declarations are not C++ source.
- Write an access modifier on every member, followed by its schema type and name.
- `array T` generates `std::vector<T>`; `map(K) T` generates `std::map<K, T>`.
- Members end with `;`. Classes and enums end with `}` without a trailing `;`.
- Parenthesized metadata assigns a wire name and/or numeric ID. For example,
  `public string name ("fullname", 1);` uses `fullname` in JSON and string-key
  binary. The default C++ profile emits `name`; other profiles can rename the
  C++ member without changing the wire key.
- Braced initializers supply defaults for newly constructed objects. Missing
  keyed fields retain whatever values the destination currently has.

### When to use `stable_ids`

Use it to make forgetting an explicit ID a generator error. The keyword does not
assign or remember IDs: the numbers in parentheses do that. When adopting it on
an existing schema, first determine the current IDs and write those exact numbers.
Implicit IDs count parents first, then members in declaration order; an explicit
override does not renumber the subsequent declaration-order slots.

For the schema above, `public string country;` would be rejected. Add
`public string country (4);` instead. Keep IDs 1, 2, and 3 unchanged. Each containing
class requires unique parent/member IDs in `1..0x3fffffff`; the generator also
rejects duplicate wire names. Apply the keyword independently to nested/base
classes whose IDs should be explicit.

Reordering declarations with explicit IDs preserves integer-key identity, but
changes positional binary order and can change emitted field order. Older keyed
readers still reject newly added fields. Removing/reusing IDs, changing field
types, and renumbering enum or union alternatives need an application versioning
decision. See the [complete evolution rules](wire_format.md#schema-evolution) and
the [README examples](../README.md#explicit-field-ids-with-stable_ids).

`view`, `owning`, `readonly`, and `mutable` select generated representations;
see [the view guide](views.md). `inplace` and `simd` are not accepted keywords.
The following build and codec example uses an owning C++ object.

## 2. Generate headers as part of the build

Assume a Serializer checkout is already available at `vendor/Serializer`. Add the
following to your application's `CMakeLists.txt`, adapting the target and paths:

```cmake
cmake_minimum_required(VERSION 3.28)
project(serializer_example LANGUAGES CXX)

set(SERIALIZER_BUILD_TESTS OFF CACHE BOOL "Build Serializer's own tests")
add_subdirectory(vendor/Serializer)
find_program(APP_CLANG_FORMAT NAMES clang-format clang-format-22 clang-format-21
  clang-format-20 clang-format-19 REQUIRED)

set(person_schema "${CMAKE_CURRENT_SOURCE_DIR}/schemas/person.def")
set(generated_directory "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(person_header "${generated_directory}/person.hpp")

add_custom_command(
  OUTPUT "${person_header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_directory}"
  COMMAND serializer input "${person_schema}" output "${person_header}"
    cpp.clang_format "${APP_CLANG_FORMAT}"
  DEPENDS serializer "${person_schema}"
  VERBATIM
)

add_executable(serializer_example main.cpp "${person_header}")
target_link_libraries(serializer_example PRIVATE serializer_lib)
target_include_directories(serializer_example PRIVATE "${generated_directory}")
```

`serializer_lib` supplies the include directory and C++20 minimum. Keep generated
headers in the build directory, and regenerate them when schemas or the generator
change. Do not edit generated headers manually. If multiple independent targets
need the same generated header, use one custom generation target and make the
consumers depend on it; see [the qualification build](../qualification/CMakeLists.txt).

For another C++ presentation style, add `config "${output_config}"` to the
generation command and include that file in `DEPENDS`. If it selects a custom
`format_file`, add that YAML file to `DEPENDS` too. See the
[language-specific output configuration](output_configuration.md) and
[examples for all nine profiles](../example/coding_styles/README.md).
The default profile converts names such as `personID` to `person_id` while
retaining their wire spellings. Use `cpp.naming preserve` when retaining an
existing schema-derived C++ API is required.

Build commands, when validation is being performed:

```sh
cmake -S . -B build
cmake --build build --config Debug
```

You can also invoke an already-built generator directly:

```sh
serializer input schemas/person.def output person.hpp
```

Use the executable's actual path if it is not on `PATH`. In a cross-compilation
build, generation requires a compiler executable built for the host machine;
adapt the custom command to use that executable rather than a target-only binary.

## 3. Encode and decode an exact message

Save this as `main.cpp` for the CMake example:

```cpp
#include <rohit/serializer.hpp>
#include <person.hpp>

#include <cstddef>
#include <iostream>

// Encode one record, then decode only its written bytes under explicit limits.
int main() {
  demo::person original{};
  original.name = "Ada";
  original.age = 37;
  original.scores = {8, 9, 10};

  constexpr std::size_t initial_capacity_bytes = 256;
  rohit::full_stream_auto_alloc output{initial_capacity_bytes};
  original.serialize_out<rohit::serializer::binary_integer>(output);

  const auto input = rohit::make_constant_full_stream(
      output.begin(), output.current_offset());
  rohit::serializer::decode_limits limits{};
  limits.max_input_bytes = 1024 * 1024;
  limits.max_string_bytes = 4096;
  limits.max_collection_elements = 1024;
  limits.max_nesting_depth = 16;

  demo::person decoded{};
  rohit::serializer::binary_integer<rohit::serializer::serialize_type::in>
      decoder{input, limits};
  decoder.serialize_in(decoded);
  decoder.finish();

  std::cout << decoded.name << ", age " << decoded.age << '\n';
}
```

Use `current_offset()` for the message length, not buffer capacity. Retain the
output storage while the input view is alive, and do not reset, overwrite, or grow
it during decoding. Destination storage must be independent of the input bytes.
For a reused output buffer, call `reset()` before encoding the next message after
all readers of the previous message have finished.

### Choose the protocol

| Protocol | Field identity | Application requirement |
| --- | --- | --- |
| `json` | Wire names | UTF-8 JSON and agreed field names/types |
| `binary_none` | Declaration order | Identical field order and types |
| `binary_integer` | Numeric IDs | Agreed IDs and types; `stable_ids` helps enforce explicit assignments |
| `binary_string` | Wire names | Agreed names and types |

Use the same protocol at both ends. To use JSON in the example, replace both
`binary_integer` occurrences with `json`. For formatted output, construct
`json_out<true>` with a `write_format` and pass it to the object's `serialize_out`.
`format::compress` only removes optional JSON whitespace.

Binary fixed-width values default to **little-endian**, independent of the host.
Inspect `Protocol::wire_endian`; select the third `binary` template argument only
when your application requires another byte order. Compact prefixes retain their
defined encoding. Messages carry no automatic protocol or endian marker, so both
endpoints must agree on those choices. See [byte order](wire_format.md#byte-order).

### Use generated views

Add `view` to generate read-only and mutable views, then optionally restrict them
with `readonly` or `mutable`. Add `owning` to retain an owning representation too.
Keyword order does not matter. A single representation is a plain class; multiple
representations use a `storage_mode` template parameter. Existing owning fields
retain their original API.

Views map little-endian `binary_none` buffers only. A read-only mapping borrows a
`std::span<const std::uint8_t>`; a mutable mapping needs `std::span<std::uint8_t>`.
Use generated `get_field()` and `set_field()` accessors. Setters preserve encoded
sizes, and every borrowed value depends on the buffer's lifetime and stable layout.
See [views.md](views.md) for a complete example and collection/nested-type rules.

## 4. Handle limits and failed input

- Each decoder owns cumulative byte, storage, work, and nesting accounting.
  Share it by reference for nested reads and create a fresh decoder per message
  budget. All configured defaults still apply when you override only some limits.
- `finish()` rejects trailing binary data; JSON permits trailing whitespace.
  The generated `object.serialize_in<Protocol>(input)` convenience call uses
  default limits and does not perform this final exact-message check.
- Strings, vectors, and maps replace old contents. String/vector capacity can be
  reused; nested element allocations and map nodes may still be rebuilt.
- Decoding can leave partial changes on failure. Catch
  `rohit::exception::base_parser` for codec parse errors and inspect `code()`.
  Allocation failures can also propagate as standard C++ exceptions.
- If the existing destination must remain intact on failure, decode and finish
  into a separate temporary object before committing it to application state.
  Include the temporary's memory cost in application limits.
- Error messages omit automatic payload excerpts by default. Opt-in excerpts
  belong in controlled diagnostics, not ordinary public error responses.

See [the wire-format contract](wire_format.md) for exact binary representations,
Unicode/numeric rules, limit defaults, duplicates, and union restrictions. Use
[migration.md](../migration.md) when adapting older generated headers or callers.

## Agent-assisted integration

Use the [Serializer integration skill](../.agents/skills/serializer-integration/SKILL.md)
to apply this workflow to an existing application. The
[README](../README.md#repository-agent-skill) explains discovery and invocation.
