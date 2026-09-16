# Using Serializer in a C++ application

For pure Java generation and runtime usage, see [Java output](java.md) and the
[Java examples](../example/java/README.md). The workflow below is for C++.

For Protobuf binary, ProtoJSON, or TextProto, enable `[cpp] protobuf = true` in
the generator config and use `protobuf_binary`, `protojson`, or `textproto` as the
`serialize_out`/`serialize_in` protocol template. These codecs require no external
Protobuf runtime. See [Protobuf usage and mapping rules](protobuf.md), including
field-number restrictions and Protobuf defaults on input.

Serializer compiles a schema into a C++ header, then reads or writes generated
objects through JSON or binary protocols. Use a C++20-or-newer compiler and
standard library; the CMake project requires CMake 3.28 or newer. Header generation
uses clang-format 19+ by default. Applications using existing headers do not need it.

The schema compiler enables bounded SIMD scanning on supported x64 builds, with
scalar fallbacks. This is automatic and needs no additional schema keyword; see
[schema-scanner configuration](cmake_integration.md#schema-scanner-configuration)
for the `SERIALIZER_ENABLE_SIMD` source-build option.
The same option also controls [runtime SIMD](runtime_simd.md): JSON string
scanning and endian conversion of C++ binary numeric arrays on input and output
across all key modes. Bulk decoding preserves resource limits and partial-failure behavior.
Link `Serializer::serializer_lib` even when using pre-generated headers.

The examples below describe the current source API. They have been reviewed
against the source but have not been generated, compiled, or executed as part of
this documentation change. See [qualification](../qualification/README.md) for
the deferred validation work.

## 1. Define a schema

The optional [VS Code extension](editor_extension.md) supplies `.serializer`
highlighting and a `schema` snippet with the required version header.

Save this as `schemas/person.serializer` in your application:

```text
serializer version 1;

namespace demo {
class person stable_ids {
  public string name (1) { "Unknown" };
  public uint32 age (2) { 0 };
  public array uint32 scores (3);
}
}
```

- Use `class`, `enum`, and `namespace`; schema declarations are not C++ source.
- Start every `.serializer` file with `serializer version 1;` before declarations.
  See [compiler options and versioning](command_line.md) for the file contract,
  compiler version, and generating C++ and Java together.
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

add_executable(serializer_example main.cpp)
serializer_generate(TARGET serializer_example SCHEMAS schemas/person.serializer)
```

The helper ships with Serializer and is also available through an installed
`find_package(Serializer CONFIG REQUIRED)` package. It generates
`build/generated/serializer_example/person.hpp` before compiling the application,
adds its include directory, and links `Serializer::serializer_lib`, which supplies
the C++20 minimum. A newer language mode is preserved. Schema, config, and
generator dependencies drive regeneration; do not edit generated headers manually.
The default formatter must be available as `clang-format` on `PATH`; pass
`CLANG_FORMAT "/path/to/clang-format"` when using another location.

For multiple consumers of one schema, generate it on a shared interface library
and link that target from each consumer. See the [CMake integration guide](cmake_integration.md)
for this pattern, package installation, and the complete helper API.

Building `serializer_generated_headers` generates registered headers and builds
the generator if necessary, without building the consuming application. In VS Code,
use CMake Tools as the C/C++ configuration provider. No custom task is required;
the [IntelliSense guide](intellisense.md) explains the initial generation step.

For another C++ presentation style, add `CONFIG output.ini` to `serializer_generate`.
The helper tracks that file as a dependency. If it selects a custom `format_file`,
pass that YAML file through `DEPENDS` too, or use the helper's `FORMAT_FILE` option
to select and track it directly. See the
[language-specific output configuration](output_configuration.md) and
[examples for all nine profiles](../example/coding_styles/README.md).
The default profile converts names such as `personID` to `person_id` while
retaining their wire spellings. Use `--cpp.naming preserve` when retaining an
existing schema-derived C++ API is required.

Build commands, when validation is being performed:

```sh
cmake -S . -B build
cmake --build build --config Debug
```

You can also invoke an already-built generator directly:

```sh
serializer --input schemas/person.serializer --output person.hpp
```

Use the executable's actual path if it is not on `PATH`. In a cross-compilation
build, generation requires a compiler executable built for the host machine;
pass its path through the helper's `GENERATOR` option.

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
Use range-based loops over array/map views for sequential access. Arrays yield
values or nested views; maps yield key/value pairs in wire order. Mutable iterators
provide `set(value)` for array elements and `set_value(value)` for map values,
subject to the same encoded-size restrictions. Iterator copies advance independently.
See [views.md](views.md) for a complete example and collection/nested-type rules.

## 4. Handle limits and failed input

- Each decoder owns cumulative byte, storage, work, and nesting accounting.
  Share it by reference for nested reads and create a fresh decoder per message
  budget. All configured defaults still apply when you override only some limits.
- `finish()` rejects trailing binary data; JSON permits trailing whitespace.
  The generated `object.serialize_in<Protocol>(input)` convenience call uses
  default limits and does not perform this final exact-message check.
- Strings, vectors, and maps replace old contents. C++ native codecs can reuse
  nested buffers and map nodes as described below; resource accounting still applies.
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

### Reuse destination storage

Keep an owning destination between messages to reuse its allocations. Construct a
fresh decoder over each exact message so budgets restart:

```cpp
std::vector<std::string> names; // Keep this outside the application's receive loop.

// For each message, while its independently owned input bytes remain alive:
const auto input = rohit::make_constant_full_stream(message.data(), message.size());
rohit::serializer::binary_none<rohit::serializer::serialize_type::in> decoder{input};
decoder.serialize_in(names);
decoder.finish();
```

This works automatically for C++ JSON and positional, integer-key, and string-key
binary, including either configured byte order. Existing vector slots can donate
string and nested container storage. Maps can recycle old nodes, replacing their
keys and values only after each complete incoming entry. A donor node need not have
the same old key; map key parsing still uses a fresh key temporary. Completed
incoming map entries never become donors for later entries, so an incomplete
duplicate leaves the last complete value intact.

Regenerate owning headers to reuse fields inside collection elements, including
parents and nested generated classes. Each element is freshly constructed; only
fields actually present in the message take storage from the old element. Missing
keyed fields therefore retain schema defaults inside collections. An ordinary
top-level object update still retains its existing values for missing fields.
Donor selection uses typed generated code with `if constexpr`, without runtime
reflection or field metadata lookup. Keep using the existing one-argument input
methods; the additional generated donor parameter is internal runtime support.

Reuse is limited to supported owning types with nonthrowing move assignment; map
node reuse also requires nonthrowing key assignment. Other/custom types retain
fresh-element decoding. Scalar vectors keep their existing scalar or bulk paths.
Shorter collections destroy unused old entries; decoding an empty collection does
not cache its removed elements. Retaining capacity does not guarantee zero
allocations: growth, default initializers, map bookkeeping, and fresh keys can
allocate. Old donor storage stays alive until consumed or discarded, which can
increase temporary memory usage. Storage and work limits still apply to logical
decoded data, including reused storage; allocation accounting is not a heap limit.

Wire bytes and replacement/failure contracts are unchanged. Input must remain
independent of destination storage. Java and Protobuf/ProtoJSON/TextProto keep their
existing decode paths. Focused reuse/defaults/duplicate/failure/limit tests are
included in the normal C++ test target; generation, compilation, test execution,
and allocation/performance measurements remain deferred for this implementation.

## Agent-assisted integration

Use the [Serializer integration skill](../.agents/skills/serializer-integration/SKILL.md)
to apply this workflow to an existing application. When supplying a Serializer
checkout to an agent, give it that file's path directly; skill installation is
not required for direct use. The [README](../README.md#integrate-with-a-coding-agent)
provides a copyable request, and its [discovery guide](../README.md#repository-agent-skill)
explains automatic selection and use from a consuming project's root.
