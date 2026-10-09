# Build and generation guide

[Back to the project overview](../README.md)

Build Serializer, choose an output language, and connect generation to your application.
For the shortest introduction, start with the [README](../README.md#get-started).

## Build and test

With CMake, a C++20-or-newer compiler and standard library, clang-format 19+, and
GoogleTest 1.18.0 or newer available:

```sh
# Linux (GNU Make)
make all
make test
make clean
make rebuild
```

```powershell
# Windows (PowerShell; GNU Make is not required)
# all/rebuild also package both extensions; require Node.js 22+, npm and Visual Studio MSBuild
./make.ps1 all
./make.ps1 test
./make.ps1 clean
./make.ps1 rebuild
```

The Linux `Makefile` and Windows `make.ps1` keep their common commands, defaults,
and corresponding options synchronized. Both default to Release and configure/build
all enabled CMake targets, including tests and C++ style examples on a fresh cache.

| Command | Common CMake behavior |
| --- | --- |
| `configure` | Configure the selected build tree without building. |
| `all` (default) | Configure and build all enabled targets. |
| `test` | Configure and build all enabled targets, then run CTest. |
| `clean` | Run CMake's clean target for an existing configured tree; otherwise succeed without configuring. |
| `rebuild` | Configure, then use CMake's `--clean-first` to finish native cleanup before building, including with parallel GNU Make. |

`clean` preserves `CMakeCache.txt`, installed dependencies, and extension packages;
it never recursively deletes the build tree. It needs no vcpkg toolchain validation
or compiler configuration for an unconfigured tree. Use the same build-directory
and configuration options for `all`, `clean`, and `rebuild` to select the intended
outputs. This clean is separate from CMake's `--fresh` cache recovery.

Set `VCPKG_ROOT` to use its toolchain for GoogleTest, or provide an installed
GoogleTest package through CMake. The wrappers do
not install a compiler or clang-format. Java, benchmarks, and fuzzers remain opt-in.
The repository manifest requires the vcpkg `gtest` package at version 1.18.0 or
newer; CMake loads its installed configuration with
`find_package(GTest 1.18.0 CONFIG REQUIRED)` when tests are enabled.
GoogleTest is a test dependency rather than part of `Serializer::runtime` or
`Serializer::managed`.
Windows builds stage imported shared runtime DLLs, including shared GoogleTest
libraries, beside each repository test executable. Reconfigure and rebuild an
older test target to enable staging before running it directly or through CTest.
See [test DLL setup](cmake_integration.md#windows-test-runtime-dlls) for scope.
If a cached `GTest_DIR` still selects an older package, update that installation,
reconfigure with `-U GTest_DIR`, and rebuild the tests with the existing toolchain.

See [build wrapper options](cmake_integration.md#build-this-repository) for build
directories, configurations, and additional CMake settings.

If CMake still selects a removed Visual Studio installation, follow
[stale Visual Studio cache recovery](cmake_integration.md#recover-a-stale-visual-studio-instance)
to refresh the build configuration and reapply your nondefault options.

On Windows, `./make.ps1 all` and `./make.ps1 rebuild` also restore locked npm
dependencies and build both editor extension packages in `out/extensions`, after
the CMake build succeeds. This requires Node.js 22+, npm, and Visual Studio
2022/2026 or Build Tools with MSBuild. The wrapper checks that extension versions
match and stops on packaging failures; neither package is
installed. Run `./editors/build.ps1` to build only the two extension packages.

Linux wrapper commands build native CMake targets. Windows `configure`, `test`,
and `clean` remain CMake-only.

If configuration reports a missing clang-format, install version 19 or newer and
rerun `make all`. On Ubuntu/Debian with that package available, use
`sudo apt install clang-format-19`. Installing Clang alone does not necessarily
install clang-format. See [formatter setup](cmake_integration.md#formatter-setup)
for Windows and custom installation paths.
The [build requirements](cmake_integration.md#build-this-repository) distinguish
default and optional tools; [vcpkg package builds](cmake_integration.md#vcpkg-package-builds)
disable development targets and do not require their dependencies. Runtime features
(managed records, SIMD, and all compression backends) are enabled by default.
Package builds require Zstandard, LZ4, and zlib; internal managed-record generation
needs no formatter.

To build only the enabled CMake targets, use:

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
[Visual Studio folder builds](cmake_integration.md#visual-studio-folder-builds)
for setup and the Ninja/platform error explanation.

When embedding the library, add this repository with `add_subdirectory` and link
to `Serializer::runtime`. It supplies the public include path and C++20
requirement. The `serializer_generate` helper below handles that linkage for
schema consumers. Installed packages expose the same targets and helper through
`find_package(Serializer CONFIG REQUIRED)`.
The runtime target uses 0BSD and permits proprietary application use. The
separate `Serializer::serializer_lib` compiler API target remains GPL; see
[licensing](licensing.md) for generated output, schema notices, and package scope.
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

The [latest four-toolchain verification](verification-toolchains-2026-10-07.md)
records full default builds, CTest suites, and installed consumers on Windows x64
(MSVC and clang-cl) and Ubuntu WSL2 x64 (GCC and Clang), using Release with a C++20
baseline and managed records, SIMD, and all compression backends enabled.

The [earlier verification record](verification-2026-09-17.md) retains its source
revision, configurations, and outstanding checks. Earlier portability checks
reported 13 CTest targets on Linux x64 with GCC 15.2 and Clang 21.1, and Windows x64
with MSVC 19.51. The Windows x86 vcpkg package and installed-consumer round trips
have also been checked separately.

Native macOS, Android, and ARM Linux builds still require their CI runners; these local
checks do not establish support for every vcpkg triplet.

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
[CodingStandard.md](../CodingStandard.md) for naming and coding rules.

## Output language and coding standard

Keep target-language settings in a generator config, separate from the `.serializer`
schema. C++, Java, JavaScript, Go, C#, Rust, Python, Swift, Kotlin, and C output are implemented, with optional
TypeScript declarations for JavaScript. For C++:

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

See [configuration and naming rules](output_configuration.md) and
[examples for every profile](../example/coding_styles/README.md). Formatting runs
at generation time; runtime consumers do not need clang-format. Config-file
paths are relative to the config; CLI overrides take precedence. A custom
`format_file` can supply organization-specific clang-format rules.

CMake detects clang-format 19+ for test/example builds from its normal program search
paths and, on Windows, standard LLVM and Visual Studio installations. Fresh build
directories repeat discovery without a manually configured path. Set
`SERIALIZER_CLANG_FORMAT_EXECUTABLE` only to select a specific installation; explicit
paths are also checked for a runnable version 19+. If none is installed, install LLVM or
Visual Studio's C++ Clang tools and configure again.

The standard test build includes all profile examples; a build with tests disabled can
opt into them with `SERIALIZER_BUILD_STYLE_EXAMPLES=ON`. All nine C++ style examples
were generated, built, and run on Windows during Java backend validation; see [current
verification and historical
results](java.md#verification-performed-for-this-implementation).

### C++ constant-evaluation generation

For compiler/runtime 1.4.0 constant binary APIs, add `constant_evaluation = true`
to the config's `[cpp]` section. Add `protocols = binary_none` only when the
application wants positional-only generated codecs; the defaults are false/all.
The equivalent CLI options are `--cpp.constant_evaluation true` and
`--cpp.protocols binary_none`, with normal CLI-over-INI precedence.

Keep generation in the existing application build:

```cmake
serializer_generate(TARGET constant_example
  SCHEMAS schemas/payload.serializer
  CONFIG serializer_output.ini)
```

The helper tracks the configuration and regenerates the owning header. The flag
adds a separate constexpr traversal and static metadata; it retains existing
runtime serialization methods. The independent positional-only profile removes
JSON/keyed formats and rejects Protobuf, managed or view combinations. Generate
each qualified model consistently for every consuming translation unit.

Include `<rohit/constant_binary.hpp>` at call sites. See the
[usage example](usage.md#count-and-emit-constant-positional-bytes) and
[full API/support contract](constant_evaluation.md) for deterministic two-pass
factories, transient storage, runtime decoding and qualified C++20 toolchains.
For borrowed emission-only DTOs, also set `[cpp] emission_only = true`; this
requires constant_evaluation true and protocols binary_none, with Protobuf false.
Generate a second owning header on a separate target/output directory when a runtime
reader is required. Emission models use a terminal emission namespace and expose
only concrete positional output. See the [profile limits and lifetime rules](constant_evaluation.md#94-emission-only-borrowed-models).

The constant-binary and emission-only tests use the actual compiler, generated
headers and existing owning runtime consumers; they do not depend on a handwritten
replacement model codec.

## Pure Java output

Generate owning Java classes and direct codecs with no native runtime dependency:

```sh
serializer --input example/java/round_trip/account.serializer --output AccountSchema.java --config example/java/round_trip/java.ini
```

Java profiles are `serializer`, `google`, and `oracle`, selected with
`--java.coding_standard`. They use conventional Java naming; `--java.naming preserve`
retains valid schema identifiers. These are presentation presets, not full guide
compliance. Java supports owning objects, enums, arrays, maps, unions, and parent
composition across all four protocols. Views and packed layout are rejected.
See [Java usage, limits, and compatibility](java.md).

Enable `SERIALIZER_BUILD_JAVA_EXAMPLES=ON` to generate and compile the
[Java examples](../example/java/README.md) with JDK 17+. With tests enabled, CTest
also runs malformed-input and two-way C++/Java compatibility checks. Every C++
and Java style example has its own schema, config, and consumer folder.

## JavaScript, Go, and C# output

Generate standalone owning codecs for modern JavaScript, Go 1.22+, or .NET 8+:

```sh
serializer --input example/interoperability/message.serializer --language js,typescript,go,csharp --js.output schema.mjs --typescript.output schema.d.mts --go.output schema.go --csharp.output Schema.cs
```

Use `bigint` for JS 64-bit fields, `New<Type>()` for Go schema defaults, and the
output filename's outer class for C#. These codecs implement the four native
protocols, exact-message decoding, bounded input, and sorted map output. They
use direct field access, pre-encoded keys, native switch dispatch, and explicit
endian primitives. See [portable language APIs and limitations](portable_languages.md).

The [language examples](../example/README.md) provide four runnable programs per
language, including a complex model spanning 13 included schema files. The shared
interoperability matrix checks every direction across ten runtimes plus a typed
TypeScript consumer: 1,452 exchanges covering four protocols and three union
alternatives. Enable `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON` and run CTest
with all SDKs installed. The smaller five-runtime matrix remains available via
`SERIALIZER_BUILD_INTEROP_EXAMPLES=ON`.

## Rust, Python, Swift, Kotlin, and C output

```sh
serializer --input example/interoperability/message.serializer --language rust,python,swift,kotlin,c --rust.output schema.rs --python.output schema.py --swift.output Schema.swift --kotlin.output Schema.kt --c.output schema.h
```

These standalone codecs use native owning types and the same four wire protocols. C
exposes explicit initialization/free functions and transactional decode; Swift uses
exact UTF-8 `WireString` map keys; Kotlin preserves unsigned widths and uses primitive
numeric arrays. See [APIs, SDKs, ownership, and limits](native_languages.md) and the
[verification record](verification-native-2026-09-18.md). The [finalization
fixes](../migration.md#finalization-correctness-fixes) correct large floating defaults
in Rust/C output and leading U+FEFF preservation in Windows Swift.

Regenerate the affected sources when updating the compiler.

## CMake consumer integration

Serializer ships its generation helper in [cmake/serializer_generate.cmake](../cmake/serializer_generate.cmake).
After adding Serializer as a dependency, a consumer needs:

```cmake
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.serializer)
```

A normal build of `my_app` builds the generator when using the source dependency,
generates `account.hpp`, and supplies the generated include directory, runtime
library, and C++20 requirement. Schema and generator changes trigger regeneration.
Use `CONFIG output.ini` to select output profiles. See the
[CMake integration guide](cmake_integration.md) for complete source-dependency
and installed-package examples, options, and shared schemas.

When several C++ targets need different configurations of the same schemas,
register them together with `serializer_generate_variants(TARGETS ... SCHEMAS ...
CONFIGS ...)`. Each entry schema is parsed once and generates all configured
variants in one incremental build command. Direct CLI callers can repeat
`--config` and the matching outputs. See
[batch CLI generation](command_line.md#generate-several-configurations-from-one-parse)
and [CMake variants](cmake_integration.md#generate-several-configurations-from-one-parse)
for ordering, output-directory rules, and examples. Existing INI files retain
their syntax.

This integration works independently of editor settings. `.vscode/*` remains
ignored. For VS Code, select CMake Tools as the C/C++ configuration provider so it
receives the consumer's include paths. The real header must also exist: configure
and build once, or generate just the headers in an already configured build:

```sh
cmake --build build --config Debug --target serializer_generated_headers
```

No custom VS Code task or Serializer IntelliSense extension is required. See the
[IntelliSense guide](intellisense.md) for the repository presets and profile-specific
includes. The optional [VS Code extension](editor_extension.md) exposes the same build
targets through commands and diagnoses missing includes. The extension also provides
**Go to Declaration** to source schemas. **Go to Definition** opens included
schemas or existing generated output for type references.

C++ type references use the C++ language service to find their originating schema.
Right-click a `.serializer` file in Explorer or its editor tab and choose **Serializer:
Go to Implementation** to open its available generated header. Navigation and **Open
Generated Header** only use available files: they never build, configure, save inputs,
or prompt for generation. See [navigation details and
limitations](editor_extension.md#navigate-available-schemas-and-headers).

Source builds and installed-package generation have been checked on Windows; see
[compiler verification](command_line.md#verification). The extension's Windows
editor-host generation smoke test passed; see [extension
verification](editor_extension.md#verification-performed) for scope. Live C/C++
IntelliSense reparsing remains unverified.

Release catalogs and nested policies are evaluated by the schema compiler. Pin `--version-policy-as-of YYYY-MM-DD`, or `VERSION_POLICY_AS_OF` in CMake, for deterministic compatibility bounds. See [release policies](versioning.md#release-dates-and-compile-time-policies).
