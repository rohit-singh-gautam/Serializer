# Serializer

A C++20 schema compiler with C++ and pure Java output supporting JSON and three
binary protocols. The C++ runtime API and default generated C++ use `snake_case`.
Opt-in C++ codecs also support **Protobuf binary, ProtoJSON, and TextProto** through
compile-time protocol templates, for both encoding and decoding. See
[Protobuf codecs](docs/protobuf.md) for generation, schema mappings, and limitations.
Language-specific output profiles select layouts and naming conventions.
Schemas use `.serializer` and begin with `serializer version 1;`.
Share declarations with `include common.serializer;` before any declarations.
Paths are unquoted and relative to the including file. Includes are loaded once
per entry schema and emitted together in its generated output. Namespace scopes
are reused during parsing; duplicate types and namespace/type conflicts are rejected.
See [schema includes](docs/usage.md#share-declarations-with-includes) and the
[paired C++/Java examples](example/includes/README.md).
Quoted defaults preserve literal spaces, for example
`public string label { "schema default" };`; escaping the space is unnecessary.
Run `serializer --version` for compiler version **1.0.0** and supported schema versions.
See [command-line options](docs/command_line.md) for multi-language generation and overrides.
See [Java output](docs/java.md) for dependency-free Java 17+ codecs and
[all examples](example/README.md) for self-contained example folders.

**Existing callers:** follow the [migration guide](migration.md) and regenerate
headers before compiling against the updated API.

**Getting started:** the [usage guide](docs/usage.md) covers schema authoring,
automatic header generation with CMake, and decoding an exact message with limits.

**VS Code:** [Rohit Serializer](docs/editor_extension.md) highlights
`.serializer` files, supplies snippets, and provides CMake header-generation,
schema declaration/generated-header navigation, and missing-include assistance. Run
`./install_extension.ps1` from PowerShell to build and install the local extension
(Node.js 22+, npm, and the VS Code CLI are required). Marketplace publication is pending.
The VS Code extension version is **1.1.1**, with ID `rohitjairajsingh.serializer-language`
(Rohit Jairaj Singh). Its release version is independent of the compiler version.
It also supplies a dedicated 32×32 icon for `.serializer` files in Explorer and
editor tabs when supported by the selected file icon theme.

**Visual Studio:** a separate [Rohit Serializer VSIX](editors/visual_studio/README.md)
packages the same grammar and basic editing configuration for Visual Studio 2022/2026
on Windows x64. Build it with `./editors/visual_studio/build.ps1`, then install
`out/extensions/serializer-visual-studio-1.0.2.vsix` with Visual Studio's VSIX Installer.
It supplies lexical editing; use existing CMake targets for generation. Native IDE
installation and interactive editing verification remain pending.

## Integrate with a coding agent

**Agent entry point: [Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).**
When this repository is supplied for integration, read that file for schema,
CMake, generated-code, and codec instructions. [AGENTS.md](AGENTS.md) also directs
agents to it. Users can give their agent this request, adapting the checkout path:

```text
Read vendor/Serializer/.agents/skills/serializer-integration/SKILL.md
and use it to integrate Serializer into this application's CMake build.
```

This works by providing the file directly, including from another project's root.
For skill discovery and invocation, see [repository agent skill](#repository-agent-skill).

## Build and test

With CMake, a C++20-or-newer compiler and standard library, clang-format 19+, and
GoogleTest available:

```sh
# Linux (GNU Make)
make all
make test
```

```powershell
# Windows (PowerShell; GNU Make is not required)
./make.ps1 all
./make.ps1 test
```

Both wrappers default to Release and configure/build all enabled CMake targets,
including tests and C++ style examples on a fresh cache. `test` builds first and
runs CTest; `configure` only configures. Set `VCPKG_ROOT` to use its toolchain for
GoogleTest, or provide an installed GoogleTest package through CMake. The wrappers
do not install a compiler or clang-format. Java, benchmarks, and fuzzers remain
opt-in. See [build wrapper options](docs/cmake_integration.md#build-this-repository)
for build directories, configurations, and additional CMake settings.

If configuration reports a missing clang-format, install version 19 or newer and
rerun `make all`. On Ubuntu/Debian with that package available, use
`sudo apt install clang-format-19`. Installing Clang alone does not necessarily
install clang-format. See [formatter setup](docs/cmake_integration.md#formatter-setup)
for Windows and custom installation paths.
The [build requirements](docs/cmake_integration.md#build-this-repository) distinguish
default and optional tools; [vcpkg package builds](docs/cmake_integration.md#vcpkg-package-builds)
disable development targets and do not require their dependencies.

The equivalent direct CMake commands are:

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The existing platform presets use vcpkg through `VCPKG_ROOT`. Windows presets
use Ninja with an externally selected x64 compiler environment. Visual Studio
sets up that environment when opening the folder; for command-line builds, run
these commands in an x64 Native Tools Command Prompt with Ninja on `PATH`:

```sh
cmake --preset DebugWindows
cmake --build --preset DebugWindows
ctest --test-dir out/build/DebugWindows -C Debug --output-on-failure
```

After updating an older Windows preset, delete the CMake cache and reconfigure
in Visual Studio to clear any cached platform setting. See
[Visual Studio folder builds](docs/cmake_integration.md#visual-studio-folder-builds)
for setup and the Ninja/platform error explanation.

When embedding the library, add this repository with `add_subdirectory` and link
to `Serializer::serializer_lib`. It supplies the public include path and C++20
requirement. The `serializer_generate` helper below handles that linkage for
schema consumers. Installed packages expose the same targets and helper through
`find_package(Serializer CONFIG REQUIRED)`.
Use `-DSERIALIZER_BUILD_TESTS=OFF` for a standalone build without GoogleTest.

C++20 is the minimum language mode and the build default. A newer mode selected
with `CMAKE_CXX_STANDARD`, such as `-DCMAKE_CXX_STANDARD=23`, is preserved. Public
headers also check the language mode when used outside CMake.

Regenerate LLVM-profile headers after updating to the storage-donor naming fix.
This prevents a parameter/template-type collision and changes local names only,
not APIs or wire data.
The runtime also accepts mapped views through a little-endian `binary_none`
encoder's `serialize_out(view)` call. Empty identifier input reports a schema
diagnostic before attempting to read the stream.

Portability verification: the default 13 CTest targets pass on Linux x64 with
GCC 15.2 and Clang 21.1, and Windows x64 with MSVC 19.51. The Windows x86 vcpkg
package and installed-consumer round trips have also been checked. Native macOS,
Android, and ARM Linux builds still require their CI runners; these local checks
do not establish support for every vcpkg triplet.

The `rohit::byteswap` helper forwards supported integers directly to
`std::byteswap` when `__cpp_lib_byteswap >= 202110L`; otherwise it uses a constexpr
C++20 fallback. Supported floating-point values are converted through their
integer bit representations. `rohit::change_endian` accepts supported scalar
types and little/big byte orders, with compile-time rejection of unsupported
types and mixed native byte order. Booleans are unchanged.

Generate a header by running the built executable:

```sh
serializer --input example/config/config.serializer --output config.hpp
```

Format C++ files with the repository's `.clang-format`; see
[CodingStandard.md](CodingStandard.md) for naming and coding rules.

### SIMD in the schema compiler

`SERIALIZER_ENABLE_SIMD=ON` is the build default. The `.serializer` parser scans whitespace,
comments, and identifier spans in blocks, then constructs each identifier string
once. On x64, the baseline scanner uses 16-byte SSE2 blocks; supported MSVC, GCC,
and Clang builds also include a separately compiled 32-byte AVX2 scanner selected
after CPU/OS checks. Short spans and other architectures use scalar scanning.
Vector loads stay within the input bounds and require no trailing padding.

This is an internal schema-compiler optimization. No `simd` schema keyword or
output-profile setting is needed. The same build option also controls the shared
runtime optimizations below; generated methods call these helpers normally.
Use `-DSERIALIZER_ENABLE_SIMD=OFF` when configuring a source build to disable the
explicit SIMD scanners. See [CMake integration](docs/cmake_integration.md) for
consumer configuration and [qualification](qualification/README.md#schema-scanner-validation)
for the prepared boundary cases. Configuration, compilation, tests, and timing
comparisons remain deferred; no measured speedup is claimed.

### SIMD in runtime serialization

All C++ protocols use the shared runtime paths automatically:

- Compact and formatted JSON scan ordinary string spans with SSE2/AVX2 on supported
  x64 CPUs. UTF-8 validation and escaping rules remain unchanged. Escaped strings
  write directly into reserved stream storage, with a source snapshot only when
  expanding output overlaps its input. JSON input uses the same bounded scanners.
- Native JSON and ProtoJSON input scan long whitespace runs in SSE2/AVX2 blocks,
  advancing and charging the consumed run once. Short gaps and tails stay scalar.
  Only space, tab, line feed, and carriage return are accepted; input/work budgets
  bound every scan. See [JSON whitespace scanning](docs/runtime_simd.md#simd-json-whitespace-scanning).
- Positional, integer-key, and string-key binary output write eligible contiguous
  integer and floating-point arrays in one payload reservation. Matching byte
  order uses a bulk copy; differing byte order uses SIMD swaps with scalar tails.
  The count prefix, keys, scalar bits, and wire bytes are unchanged.
- Binary input bulk-decodes the same numeric arrays after checking the complete
  payload and resource budgets. Matching byte order uses a copy; differing byte
  order uses SIMD swaps. Work charges and partial results on failure are preserved.
- Nested objects, maps, and unions use these paths for their contained strings
  and arrays. Individual scalars, compact integers, enums, and packed boolean
  vectors retain their existing scalar handling. JSON numeric formatting still uses
  scalar conversion. Binary views already copy their encoded bytes in bulk;
  individual view setters retain scalar updates.

Link consumers to `Serializer::serializer_lib`, including users of pre-generated
headers. CPU dispatch and vector instructions live in compiled helpers, keeping
ISA flags out of consumer code. `SERIALIZER_ENABLE_SIMD=OFF` disables the explicit
schema and runtime SIMD backends; bulk array reads/writes and direct JSON output remain.
Short inputs and unsupported architectures use scalar fallbacks. No input/output
padding is required. See [runtime SIMD details](docs/runtime_simd.md), including
the prepared validation matrix. Rebuild the runtime library and consumers to use
the whitespace scanner; schema headers do not need regeneration. Builds, tests,
and benchmarks remain deferred.

### Reusing destination storage

C++ JSON and all three native binary codecs reuse eligible nested string/vector
buffers and map nodes when decoding into an existing collection. Regenerated
owning classes pass storage donors through typed field access, while each incoming
element still starts with fresh schema defaults. Collection replacement, duplicate
map keys, resource limits, and partial-failure behavior remain unchanged.

Reuse the destination across messages and create a fresh decoder for each message's
budget. No schema keyword or caller opt-in is needed. Java and the optional Protobuf
codecs retain their existing replacement paths. See [destination reuse](docs/usage.md#reuse-destination-storage)
for limitations and an example. Focused tests are added; generation, builds, test
execution, and performance measurements remain deferred.

### Batching generated fixed-width fields

Regenerated C++ owning serializers group consecutive fixed-width scalar fields in
batches of up to 16. All native binary output modes reserve once per batch, then
encode each field separately, including existing IDs or names. Positional binary
input checks a complete batch's range, work budget, and Boolean values together;
if it cannot safely complete the batch, it uses the original scalar reads to retain
partial results and diagnostics. Keyed input keeps per-field dispatch.

No schema option is needed. Wire bytes, byte order, and object padding rules remain
unchanged. JSON, Protobuf, and custom protocols without batch hooks retain their
existing calls. A failed output reservation writes none of the current batch;
earlier output remains. See [field batching](docs/usage.md#batch-generated-fixed-width-fields)
for boundaries and verification status. Generation, builds, test execution, and
performance measurements remain deferred.

### Pre-encoding constant field names

Regenerated C++ owning serializers prepare constant JSON and string-key binary
field names at compile time. JSON copies a prequoted name without rescanning it
for escaping; string-key binary copies the compact length and name together.
Fixed-field binary batches use these same constants and a compile-time total size.
Parent names, renamed wire keys, and union alternative keys are included; JSON's
fixed map wrapper names are also pre-encoded by the runtime.

No schema option is needed. Wire bytes and formatting remain unchanged, and
custom protocols without the optional name hook still receive `std::string_view`.
This trades some compiler work and constant storage for less repeated encoding
work; it does not shrink messages or add storage to each object. See
[constant field names](docs/usage.md#pre-encode-constant-field-names) for scope,
failure behavior, and examples. Tests are prepared; generation, builds, test
execution, and performance measurements remain deferred.

### Reducing repeated JSON string scans

C++ JSON string helpers retain escape boundaries from their validation pass.
Unescaped input copies directly into reusable destination storage; escaped input
and output copy known plain prefixes/suffixes without scanning them again. Only
the region between the first and last escapes needs further escape processing.
All UTF-8, escape, resource-limit, and output-reservation checks remain active.

This is automatic with the updated runtime headers and needs no regenerated schema
code or option. It works with SIMD enabled or disabled and adds no per-string
allocation for scan metadata. See [JSON scan reuse](docs/usage.md#reduce-repeated-json-scans)
for scope and remaining passes. Focused tests are prepared; builds, test execution,
sanitizers, and benchmarks remain deferred.

### Output language and coding standard

Keep target-language settings in a generator config, separate from the `.serializer`
schema. Both C++ and Java output are implemented. For C++:

```ini
[output]
language = cpp

[cpp]
coding_standard = google
naming = profile
format = true
```

```sh
serializer --input account.serializer --output account.hpp --config serializer_output.ini
```

Supported profiles: `serializer` (default), `core`, `google`, `llvm`, `gnu`,
`cert`, `misra`, `autosar`, and `qt`. They select presentation rules and rename
schema-derived C++ identifiers while retaining wire names and IDs. Runtime-required
method names remain fixed. `--cpp.naming preserve` retains earlier C++ names.
These presets do not establish full compliance with a coding standard.

See [configuration and naming rules](docs/output_configuration.md) and
[examples for every profile](example/coding_styles/README.md). Formatting runs
at generation time; runtime consumers do not need clang-format. Config-file
paths are relative to the config; CLI overrides take precedence. A custom
`format_file` can supply organization-specific clang-format rules.

CMake detects clang-format 19+ for test/example builds from its normal program
search paths and, on Windows, standard LLVM and Visual Studio installations.
Fresh build directories repeat discovery without a manually configured path.
Set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` only to select a specific installation;
explicit paths are also checked for a runnable version 19+. If none is installed,
install LLVM or Visual Studio's C++ Clang tools and configure again.
The standard test build includes all profile examples; a build with tests disabled
can opt into them with `SERIALIZER_BUILD_STYLE_EXAMPLES=ON`. All nine C++ style
examples were generated, built, and run on Windows during Java backend validation;
see [verification scope and outstanding suite failures](docs/java.md#verification-performed-for-this-implementation).

### Pure Java output

Generate owning Java classes and direct codecs with no native runtime dependency:

```sh
serializer --input example/java/round_trip/account.serializer --output AccountSchema.java --config example/java/round_trip/java.ini
```

Java profiles are `serializer`, `google`, and `oracle`, selected with
`--java.coding_standard`. They use conventional Java naming; `--java.naming preserve`
retains valid schema identifiers. These are presentation presets, not full guide
compliance. Java supports owning objects, enums, arrays, maps, unions, and parent
composition across all four protocols. Views and packed layout are rejected.
See [Java usage, limits, and compatibility](docs/java.md).

Enable `SERIALIZER_BUILD_JAVA_EXAMPLES=ON` to generate and compile the
[Java examples](example/java/README.md) with JDK 17+. With tests enabled, CTest
also runs malformed-input and two-way C++/Java compatibility checks. Every C++
and Java style example has its own schema, config, and consumer folder.

### CMake consumer integration

Serializer ships its generation helper in [cmake/serializer_generate.cmake](cmake/serializer_generate.cmake).
After adding Serializer as a dependency, a consumer needs:

```cmake
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.serializer)
```

A normal build of `my_app` builds the generator when using the source dependency,
generates `account.hpp`, and supplies the generated include directory, runtime
library, and C++20 requirement. Schema and generator changes trigger regeneration.
Use `CONFIG output.ini` to select output profiles. See the
[CMake integration guide](docs/cmake_integration.md) for complete source-dependency
and installed-package examples, options, and shared schemas.

This integration works independently of editor settings. `.vscode/*` remains
ignored. For VS Code, select CMake Tools as the C/C++ configuration provider so it
receives the consumer's include paths. The real header must also exist: configure
and build once, or generate just the headers in an already configured build:

```sh
cmake --build build --config Debug --target serializer_generated_headers
```

No custom VS Code task or Serializer IntelliSense extension is required. See the
[IntelliSense guide](docs/intellisense.md) for the repository presets and
profile-specific includes. The optional [VS Code extension](docs/editor_extension.md)
exposes the same build targets through commands and diagnoses missing includes.
The extension also provides **Go to Declaration** to source schemas and **Go to
Definition** to existing generated C++ headers for includes and schema types.
C++ type references use the C++ language service to find their originating schema.
Right-click a `.serializer` file in Explorer or its editor tab and choose
**Serializer: Go to Implementation** to open its available generated header.
Navigation and **Open Generated Header** only use available files: they never
build, configure, save inputs, or prompt for generation. See
[navigation details and limitations](docs/editor_extension.md#navigate-available-schemas-and-headers).
Source builds and installed-package generation have
been checked on Windows; see [compiler verification](docs/command_line.md#verification).
The extension's Windows editor-host generation smoke test passed; see
[extension verification](docs/editor_extension.md#verification-performed) for scope.
Live C/C++ IntelliSense reparsing remains unverified.

## Language Construct

Every `.serializer` file starts with `serializer version 1;` before declarations.
The declaration snippets below omit this header. Complete files in `example/`
include it. See [schema versioning](docs/command_line.md#schema-files).

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
Array and map views provide sequential `begin()`/`end()` iterators. Prefer range-based
loops for variable-length collections to avoid rescanning earlier entries on each index.
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

### Why the directory starts with a dot

A leading `.` is a hidden-directory convention, not a Git ignore rule. This skill
is tracked in Git and included with the source repository. The repository's
`.gitignore` excludes `.vscode/*` but does not exclude `.agents/`. Keep `.agents`
when copying or packaging the checkout; some file browsers and shell wildcards
omit hidden entries.

It follows the open [Agent Skills format](https://agentskills.io/specification):
a folder with a `SKILL.md` containing YAML `name` and `description`, followed by
Markdown instructions. Repository discovery locations are host-specific. This
repository uses Codex's `.agents/skills` convention:

```text
.agents/skills/serializer-integration/
  SKILL.md
  agents/openai.yaml
```

### Automatic discovery and explicit use

The YAML file under `agents/` supplies Codex UI metadata and permits automatic
invocation when an integration task matches the skill description. Codex discovers
repository skills from the working directory through the repository root. In the
CLI or IDE extension, select this skill with `$serializer-integration` or `/skills`.
See [Codex skill discovery](https://learn.chatgpt.com/docs/build-skills#where-codex-loads-local-skills).

Example request:

```text
Use $serializer-integration to add Serializer to my C++ application,
define a person schema with stable_ids, and decode integer-key binary
messages with explicit resource limits.
```

### Use from a consuming project

Codex's ancestor-directory scan does not automatically load a nested dependency's
skill from the consuming project's root. Use the direct-path request above, or
add this instruction to the application's own `AGENTS.md`, adapting the path:

```markdown
When integrating or changing Serializer usage, read and follow
vendor/Serializer/.agents/skills/serializer-integration/SKILL.md.
```

For skill-selector discovery in that project, copy the complete
`serializer-integration` folder into its `.agents/skills/`. Keep that copy aligned
with the Serializer version in use and retain the matching checkout for linked
documentation. Other agents have their own discovery rules; providing the direct
file path lets them read the same instructions without relying on automatic scanning.
These instructions accompany the source checkout; the CMake binary installation
does not currently install the skill or its documentation.

[AGENTS.md](AGENTS.md#usage-documentation-and-repository-skill) requires changes to
keep this README and the skill current together.
