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
When building this repository's tests/examples with `make all` or `./make.ps1 all`,
install that host tool first; see [formatter setup](cmake_integration.md#formatter-setup).
To open this repository in Visual Studio, select a Windows CMake preset and follow
[Visual Studio folder builds](cmake_integration.md#visual-studio-folder-builds)
for compiler environment setup and clearing an older Ninja/platform cache.

The schema compiler enables bounded SIMD scanning on supported x64 builds, with
scalar fallbacks. This is automatic and needs no additional schema keyword; see
[schema-scanner configuration](cmake_integration.md#schema-scanner-configuration)
for the `SERIALIZER_ENABLE_SIMD` source-build option.
The same option also controls [runtime SIMD](runtime_simd.md): JSON string and
whitespace scanning, and endian conversion of C++ binary numeric arrays on input
and output across all key modes. Bulk decoding preserves resource limits and
partial-failure behavior.
Link `Serializer::serializer_lib` even when using pre-generated headers.

The examples below describe the current source API. They have been reviewed
against the source but have not been generated, compiled, or executed as part of
this documentation change. See [qualification](../qualification/README.md) for
the deferred validation work.

## 1. Define a schema

The optional [VS Code extension](editor_extension.md) supplies `.serializer`
highlighting and a `schema` snippet with the required version header.
It also navigates includes and type references: **Go to Declaration** opens the
original schema, and **Go to Definition** opens available generated C++ output.
For an entire schema, right-click its file in Explorer or its editor tab and
choose **Serializer: Go to Implementation** to open an existing generated header.
Navigation never builds or offers generation; see
[available-file navigation](editor_extension.md#navigate-available-schemas-and-headers).
The separate [Visual Studio extension](../editors/visual_studio/README.md) packages
the shared highlighting grammar and basic editing configuration for Visual Studio
2022/2026 on Windows x64. Neither extension is required for compilation or codecs.

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
  Quoted defaults may contain literal spaces: `public string label { "schema default" };`.
  Spaces, escaped quotes, and braces within quotes are preserved exactly; a quoted
  `}` does not end the initializer. Unclosed quotes or braces are schema errors.

### Share declarations with includes

Save reusable types in `schemas/common.serializer`:

```text
serializer version 1;
namespace demo {
  class account stable_ids {
    public uint32 id (7);
  }
}
```

Then compile an entry schema such as `schemas/request.serializer`:

```text
serializer version 1;
include common.serializer;

namespace demo {
  class request stable_ids {
    public account owner (1);
  }
}
```

- Write `include path/to/common.serializer;` without quotes or angle brackets.
  Use ASCII letters, digits, `_`, `-`, `.`, and forward slashes in a relative path;
  spaces, backslashes, absolute paths, and other extensions are rejected.
  `./` and `../` are supported. Paths resolve from the including file, independently
  of the compiler's working directory. Comments may separate directive tokens.
- Every file requires its own `serializer version 1;` header. Includes follow that
  header and precede all declarations, at file scope only.
- Nested dependencies load before their includers. Repeated paths, normalized path
  aliases, and diamond dependencies contribute declarations only once per entry
  compilation. Include cycles and chains deeper than 32 files (including the
  entry file) are errors. Diagnostics identify the failing file and include chain.
- Namespace creation reuses the existing logical scope, including equivalent
  `namespace a::b` and nested namespace blocks. Source blocks retain their order.
  A second class or enum with the same qualified name, or a namespace/type name
  conflict, fails during parsing. Equal leaf names in different namespaces are valid.
- Referenced types must still precede their use. Including files cannot provide
  missing types to their dependencies. Including a file does not inject its
  declarations into the caller's namespace or change field IDs or wire names.
- One entry schema produces one combined C++ header or Java compilation unit.
  Java emits one static container per logical namespace. Compile only the entry
  schema for a combined model; generated C++ headers from overlapping entry graphs
  can contain duplicate definitions if included together. Separate generated-file
  imports are not implemented.

Use `serializer_generate(TARGET app SCHEMAS schemas/request.serializer)` or
`serializer_generate_java` as usual. Both helpers track transitive schema dependencies
through compiler depfiles and regenerate when an included file changes. Direct CLI
callers can request `--depfile output.d`. Library callers use
`parser::parse_file(path)`, which returns owning `statements` and canonical
`dependencies`; pass `statements` to either existing writer. The stream-only
`parser::parse` APIs reject includes because they have no source directory.

The [include examples](../example/includes/README.md) contain three runnable pairs
of C++ and Java consumers: shared types, reopened namespaces, and diamond includes.

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

### Stream concepts and implicit adapters

Regenerate C++ headers to use any implementation satisfying the public concepts in
`<rohit/stream_concepts.hpp>`. Inheritance from `rohit::stream` is unnecessary.
Generated static `Type::serialize<Protocol>(stream, value)` and
`Type::deserialize<Protocol>(stream[, limits])` calls select their implementation
at compile time, preserving the chosen protocol. The member
`serialize_out<Protocol>(stream)` and `serialize_in<Protocol>(stream[, limits])`
APIs remain available for existing callers and destination reuse.

```cpp
#include <person.hpp>
#include <sstream>
#include <utility>

// Encode into a standard memory stream; its unread contents become one input message.
void round_trip(demo::person& value) {
  std::stringstream message;
  demo::person::serialize<rohit::serializer::json>(message, value);
  auto decoded = demo::person::deserialize<rohit::serializer::json>(message);
  value = std::move(decoded);
}
```

Static `serialize` takes the stream first and a const reference to the value
second; it returns `void`. Static `deserialize` value-initializes an owning
object, decodes it, and returns it by value. Its return is marked `[[nodiscard]]`.
Schema defaults and protocol-specific default behavior are preserved. An exception
prevents a partially decoded object from being returned, but input may already
be consumed. The helper adds no heap-allocated wrapper or extra encoding step.

Both APIs use the existing stream adapters. Omitting `limits` calls the existing
default-input path; passing `decode_limits` selects the explicit-limit path.
They are generated for owning classes, including the owning specialization of
classes that also expose views. Views retain their `map` API and borrowed lifetime.
Regenerate C++ headers to obtain the static helpers.

The same calls accept `std::ifstream`, `std::ofstream`, `std::fstream`, base-class
`std::istream`/`std::ostream` references, and structural custom byte sources/sinks.
For complete programs, see the [iostream example suite](../example/iostream/README.md):
each stream category has its own folder, and all seven examples share a large
52-class schema with multi-batch payloads. It includes separate and bidirectional
memory/file streams, an explicitly buffered file, base references, and a custom
non-seekable stream buffer. Enable `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON` to
build and run the suite without GoogleTest, or use the standard test build.

Standard-stream adaptation supports narrow `char` streams; wide streams do not
provide the required byte interface.
Open binary files with `std::ios::binary`. Stream formatting flags and locale do
not control Serializer's wire encoding. No automatic protocol detection occurs.

| Recognized capability/type | Selected behavior |
| --- | --- |
| `input_buffer` / `output_buffer` | Bind the concrete buffer directly; preserve contiguous scans, scalar batching, alias handling, and allocation policy. |
| `std::istringstream` / `std::stringstream` input | Borrow `view()` from the current read position; avoid an encoded-message copy. |
| `std::ifstream` / `std::fstream` input | Read into the final owned message buffer in up to 64 KiB batches. No seeking or regular-file assumption is required. |
| `std::ofstream` / `std::fstream` output | Implicit scratch-buffer adapter with an initial 64 KiB capacity; drain completed batches as needed. |
| Other byte sources/sinks, including erased standard-stream references | Generic adapter with 8 KiB read batches or initial output capacity. |

Type recognition uses the static type. Passing a string or file stream as a base
reference selects the generic path and remains supported. Standard memory output
streams use generic batched output: C++20 does not expose their writable storage
through a portable reservation API. These policies do not imply a measured speedup.

Input byte streams contain **one message extending through EOF**, starting at their
current read position. Input is bounded by `decode_limits::max_input_bytes` and
receives `decoder.finish()` validation. Memory streams retain their own bytes;
other streams buffer the whole encoded message before decoding. This is not
incremental parsing. For consecutive framed messages, supply an exact-size input
buffer or a byte source bounded to one frame; no length prefix is inserted or read.

Use the generated overload `destination.serialize_in<Protocol>(input, limits)`
for explicit limits on an existing object, or
`Type::deserialize<Protocol>(input, limits)` to create a new one.
Omitting `limits` preserves the existing default-limit
overload and custom-protocol constructor behavior. Both overloads select the same
implicit adapter from the stream's static type:

```cpp
rohit::serializer::decode_limits limits{};
limits.max_input_bytes = 16 * rohit::serializer::decode_limits::mebibyte;
auto decoded = demo::person::deserialize<rohit::serializer::binary_integer>(stream, limits);
```

`serialize_from<Protocol>(input, destination, limits)` remains available as a
free-function alternative and is the shared implementation behind the generated
member overloads and static factory. Output likewise has
`serialize_to<Protocol>(output, value)`, shared by both output APIs.
Regenerate existing C++ headers to obtain the new two-argument member overload.
All buffer-input convenience calls, including the static factory, retain their
existing behavior: no implicit `finish()` check.
For exact buffer validation, construct the concrete decoder and call `finish()`:

```cpp
// Decode a custom contiguous cursor with explicit resource limits.
template <rohit::type_check::input_buffer Input>
void decode_person(const Input& input, demo::person& value,
                   rohit::serializer::decode_limits limits) {
  rohit::serializer::json<rohit::serializer::serialize_type::in, Input>
      decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}
```

`json`, `binary_integer`, `binary_string`, `binary_none`, `protobuf_binary`,
`protojson`, and `textproto` accept the concrete buffer type as their optional
second template argument. `binary` takes it after `WireEndian`; `json_out` takes
it after the `beautify` argument. Existing protocol names and default buffer types
remain valid. Low-level codecs require buffer concepts; generated calls and the
free functions perform implicit standard-stream adaptation. For formatted JSON,
construct `buffered_output_stream<Output>` explicitly, use it with
`json_out<true, decltype(adapter)>`, and call `adapter.finish()` after encoding.

The concept contracts are:

- `buffer_view`: `curr()` and `remaining_buffer()` expose one valid byte range.
- `input_buffer`: additionally supplies `full()` and a `noexcept`
  `get_curr_and_increase_unchecked(size)` through const access. Checked decoder
  reads advance its cursor without mutating the bytes. Pointers must remain stable
  throughout decoding. Source bytes must be independent of decoded object storage.
- `output_buffer`: supports reservation and `noexcept` unchecked writable-range
  acquisition, raw and external appends, transformed appends, integer formatting,
  and text/byte batch writes. See the concept declaration for the exact expressions.
  A failed reservation leaves its batch unwritten. A successful reservation provides
  a contiguous range valid until the next reservation. Appends preserve the native
  alias/rebasing contract; transform callbacks must be invoked synchronously and
  write exactly their requested output size.
- `byte_input_stream`: standard-style `read`, `gcount`, `peek`, `eof`, `fail`, and
  `bad` operations. `read` fills the requested range or reports EOF/failure;
  `gcount` reports actual bytes read. `peek` must report EOF without consuming data.
- `byte_output_stream`: standard-style `write` and `fail`; write the complete range
  or set failure state/throw. Short writes cannot be silently reported as success.
- `input_stream` and `output_stream` accept either their buffer or byte-stream
  capability. Compatibility `type_check::stream` now means an input/output buffer;
  `write_stream` identifies readable buffer views, including `fixed_buffer`.

Concepts check expressions and types; implementations must also uphold these
lifetime, bounds, aliasing, and failure contracts. Input cursor advancement and
output range acquisition must not throw after the caller has checked/reserved them.

Adapters borrow their streams, leave exception masks unchanged, and do not close
or explicitly flush the underlying stream. Output drains only completed reservations;
a single large string, scalar array, or other batch may grow scratch storage beyond
its initial capacity. Sources aliasing scratch storage postpone draining until the
reservation completes. Output failure may leave an external prefix written; a
failed output adapter cannot be retried. Destructors do not perform fallible I/O.

An initially failed or EOF-marked input is rejected. Successful byte-stream reads
leave the source at EOF; normal EOF is accepted even when EOF/fail exceptions are
enabled. I/O errors throw `std::ios_base::failure`; an oversized encoded input
throws `std::length_error`. Decoder errors keep their existing categories and may
partially modify the destination. A memory-stream size rejection occurs before
consumption; other failures may consume input. Encoded-message staging storage is
separate from decoded-object allocation accounting. Decode a temporary object if
replacement must be transactional.

`parser::parse` also accepts these input capabilities (byte sources use the default
message byte limit), and both `writer::cpp::write` and `writer::java::write` accept
output capabilities. Their `generate` functions return the validated generated
source as a string. Schema includes still require `parser::parse_file`.
Binary views retain stable-span mapping and can copy their positional bytes to
custom buffers or standard output streams; streams do not extend a view's lifetime.

Verification: the regenerated MSVC Debug build passed all 24 CTest
targets, including the core and Protobuf tests and seven iostream examples. Clang 21 on Linux built
the compiler and generated style/include examples and passed a separate smoke test
covering all seven C++ protocols with independent buffers and standard streams.
All seven large-schema iostream examples also passed in a standalone Clang 21
build with GoogleTest disabled.
The full Linux GoogleTest suite was not run because that dependency was unavailable.
Performance benchmarks have not been run.

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
  Generated `object.serialize_in<Protocol>(input[, limits])` calls perform this
  final exact-message check for byte streams; contiguous buffer calls preserve
  their existing behavior without an implicit `finish()`. Omitting `limits`
  uses the defaults.
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
Regenerate LLVM-profile output after the donor naming fix to avoid a local
parameter colliding with its template type. Public hooks and wire bytes are unchanged.

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

### Batch generated fixed-width fields

Regenerate C++ owning headers to batch consecutive scalar fields automatically.
For example, `uint8 kind`, `uint32 sequence`, and `double reading` occupy 13 bytes
in positional binary. They share one output reservation, and each field is encoded
at its own wire offset. No C++ object layout or padding is copied. The same fields
share an output reservation in integer-key/string-key binary, including their
existing interleaved IDs or names.

Eligible schema fields are `bool`, `char`, fixed-width signed/unsigned integers,
`float`, and `double`. A batch contains 2 through 16 consecutive eligible fields.
Longer runs split into bounded groups; isolated fields use scalar calls. Strings,
arrays, maps, enums, unions, and nested objects end a run. Parent and nested-object
serializers batch their own runs. This bounds generated unrolling and the scalar
snapshots taken before an output reservation can relocate storage.

Positional binary input checks the group's input range and remaining byte/work
budgets, validates Boolean bytes, then loads fields without unaligned typed access
and commits one cursor/accounting update. If the checks fail, the original scalar
reads determine which earlier fields are completed, where the cursor stops, and
which exception is raised. Each value and consumed byte retains its usual work
charge; allocation and nesting behavior are unchanged. Keyed input retains
individual dispatch to accept the existing ordering, missing-field, and duplicate
semantics.

The generated calls detect protocol hooks at compile time using `if constexpr` and
`requires`. JSON and custom protocols without those hooks keep their existing
field calls and framing. Protobuf and Java retain their existing codec paths.
No schema keyword, output option, or SIMD setting is needed. Binary byte order,
wire bytes, and public object input/output calls are unchanged.

Output validates IDs/names and performs one virtual stream reservation before
writing a batch. Custom reservation policies remain authoritative. Rejection
writes none of that batch, while earlier output remains; an output failure may
therefore leave a shorter prefix than individual field writes. A later object
terminator can still fail separately. Source objects and borrowed wire names must
remain valid and independent of the output buffer; input bytes must likewise stay
independent of decoded storage.

`test/fixed_field_batch_test.cpp` covers scalar byte equivalence, golden bytes,
reservation counts, both endians, unaligned input, run boundaries, custom-protocol
fallbacks, resource budgets, Boolean failures, and output rejection. The normal
C++ target generates its fixture; output-profile tests also exercise grouped
fields. These are prepared tests: generation, compilation, test execution, and
performance measurements remain deferred. No measured speedup is claimed.

### Pre-encode constant field names

Regenerate C++ owning headers to prepare the encoded form of schema wire names
at compile time. For example:

```text
class account {
  public uint32 user_id ("userID");
}
```

The spelling `userID` never changes between objects. Previously each JSON write
sent it through the ordinary string validator and escape-size scan. String-key
binary wrote its compact length and then its text through separate operations.
The generated output now selects an immutable name token at compile time:

| Protocol | Prepared name bytes | Work performed for each object |
| --- | --- | --- |
| JSON | `"userID"`, including both quote characters | Copy the quoted name; let the formatter write its colon and spacing; encode the current value |
| String-key binary | `06 75 73 65 72 49 44` in hexadecimal | Copy the length and name together; encode the current value |
| Positional/integer-key binary | No name token | Keep the existing positional or numeric-ID output |

The JSON fast path accepts plain ASCII names that need no escaping, covering the
schema's supported identifiers, explicit wire-name aliases, qualified parent
names, and `field:Alternative` union keys. Other constant spellings passed through
the internal helper fall back to ordinary validated JSON string output. Dynamic
strings still receive full UTF-8 validation and escaping. JSON's fixed `key` and
`value` map wrapper names also use constant tokens; map keys themselves remain
ordinary values. Colons, commas, indentation, and object framing keep their
existing formatting rules.

String-key binary tokens contain the canonical one-to-four-byte compact length
and the original name. They work with either scalar endianness: compact lengths
retain their own wire encoding. Within a fixed-width field batch, the token sizes
and scalar widths determine the reservation size at compile time, removing the
per-call name validation, length-prefix encoding, and size accumulation. The
batch still reserves once and writes every field in its original position.

Constant arrays have static lifetime and are shared by matching name token
specializations; serialized objects gain no data members or allocations. The
tradeoff is additional template/constant-evaluation work and read-only program
data, especially when using both named codecs with many unique names. Messages
have exactly the same size and bytes. Expected gains are from avoiding repeated
name processing, particularly for many small records; they have not been measured.

Generated code uses `detail::constant_field_name<Protocol, "name">()`. It selects
the protocol's optional static `encoded_field_name<Name>()` hook with `requires`
and `if constexpr`. Custom protocols without the hook receive the original
`std::string_view`, including in their existing batch calls. A protocol subclass
that needs ordinary names can hide the inherited hook with a deleted template.
Existing generated headers continue using their original name paths; regeneration
enables the generated optimization. No schema keyword, output option, or SIMD
setting is required. Input, views, Java, and Protobuf codecs are unchanged.

Stream reservation policies remain authoritative. An isolated binary name is now
one write: if its reservation fails, neither its length nor its text is written;
previous output survives. Its value and the object terminator can still fail
separately. This can leave a shorter output prefix than the previous split name
writes. Fixed-field batches retain their whole-batch reservation behavior.

Prepared coverage in `test/constant_field_name_test.cpp` includes exact name bytes,
formatted JSON, dynamic escaping, both binary endians, generated parents/unions,
wire aliases, compact-length boundaries, batch and isolated reservation counts,
custom-protocol fallback, and fixed/custom stream failures. Generation,
compilation, test execution, and benchmarks remain deferred for this step.

### Reduce repeated JSON scans

C++ JSON string input/output reuses information from the full validation pass.
The scanner records the first escape and the end of the last escape in addition
to the decoded or encoded byte count. For a string containing a long ordinary
prefix, one newline escape, and a long ordinary suffix:

```text
Validation:  [check prefix][check escape][check suffix]
Copy:        [bulk copy]  [convert]     [bulk copy]
```

Previously, input scanned ordinary strings once to validate them and again while
copying. It now copies validated unescaped input directly into the destination,
reusing its capacity. Plain object keys continue borrowing the validated input;
escaped keys use the same optimized copying into caller-owned scratch storage.

For escaped input and output, known plain prefixes and suffixes are copied without
another character-classification scan. The remaining transformation covers only
the interval between the first and last escapes. Multiple separated escapes
still require scanning ordinary spans inside that interval, and input escapes
are parsed again during conversion. This does not make every string operation
single-pass: validation and copying remain separate, and bytes still have to be
copied to their destination. Unescaped direct JSON output already used a bulk
copy after validation; that path is retained.

The full string is always validated, including malformed UTF-8 or escapes in its
suffix. Input string validation and limit checks finish before the destination
changes. Existing byte, value-work, allocation, and string-length charges remain
unchanged. Output still validates before its single reservation and writes only
after the reservation succeeds. `append_transformed` keeps its source rebasing
and overlap snapshot behavior; saved byte offsets remain valid after rebasing.
Input bytes must remain independent of decoded destination storage as before.

Metadata has fixed size and adds no per-string heap allocation. There is no
message cache, trusted-string mode, schema keyword, or new output configuration.
Recompile consumers with the updated runtime headers when building; generated
headers do not need regeneration for this optimization. Both scalar and SIMD
builds use the same reuse path, with bounded SIMD scanning still available for
the regions that require inspection.

Shared helpers also serve ProtoJSON string input and ProtoJSON/TextProto quoted
output, so those calls inherit the optimization without changing Protobuf
mapping or formatting. TextProto's separate input parser, JSON numbers, native
binary codecs, views, and Java retain their existing algorithms. Pre-encoded
constant field names continue using their separate compile-time path.

Prepared coverage in `test/json_scan_reuse_test.cpp` includes empty/plain strings,
Unicode, surrogate pairs, escape positions around scan-block boundaries, exact
and unaligned buffers, truncated/malformed suffixes, resource limits and cumulative
charges, borrowed keys, destination reuse, overlapping/growing output, and custom
reservation rejection. Existing runtime SIMD tests cover the shared helpers
across JSON formatting modes. Builds, tests, sanitizer runs, and performance
measurements remain deferred. No measured speedup is claimed.

### SIMD JSON whitespace scanning

Native JSON and ProtoJSON readers automatically scan long whitespace runs with
the existing SSE2/AVX2 runtime backends. Short gaps and remaining tails stay scalar.
For example, decoding a deeply indented object can skip its long indentation in
blocks before parsing the next field. Spaces inside `"keep  these spaces"` remain
string data and are handled by the string reader.

Only JSON's space, tab, line feed, and carriage return count as whitespace.
Scanning stays within the input and remaining decoder budgets, then charges and
advances once per run. ProtoJSON also bounds scans by the current message. Rebuild
the runtime library and consumers with matching headers; generated schema headers
do not need regeneration. Use the existing `SERIALIZER_ENABLE_SIMD` build option
to choose explicit SIMD or scalar scanning. No schema or application API change
is needed. See [implementation and prepared coverage](runtime_simd.md#simd-json-whitespace-scanning).
Builds, test execution, sanitizer runs, and benchmarks remain deferred.

## Agent-assisted integration

Use the [Serializer integration skill](../.agents/skills/serializer-integration/SKILL.md)
to apply this workflow to an existing application. When supplying a Serializer
checkout to an agent, give it that file's path directly; skill installation is
not required for direct use. The [README](../README.md#integrate-with-a-coding-agent)
provides a copyable request, and its [discovery guide](../README.md#repository-agent-skill)
explains automatic selection and use from a consuming project's root.
