# CMake integration for consumers

The [C++ managed snapshot runtime](managed/cpp_runtime.md) is built by default
(`SERIALIZER_BUILD_MANAGED=ON`); link `Serializer::managed` when using its APIs.
The target generates both runtime record headers using the compiler, with internal
formatting disabled, and exports them in installed packages. No formatter is
needed for these records. Explicit OFF builds omit headers that require them. Schemas with `managed` declarations use the same
`serializer_generate` helper; pass `CONFIG project.ini` for a shared `[managed]
id_type = uint64` setting. See the [standalone ledger example](../example/managed/ledger/README.md).

For JS, TypeScript declarations, Go, or C# source generation, use
`serializer_generate_source(TARGET name SCHEMA file OUTPUT generated/schema.go
LANGUAGE go OPTIONS --go.package application)`. It uses the same C++ host
compiler and tracks transitive includes through depfiles. The helper ships with
both `add_subdirectory` and installed packages; attach the target-language build
as a dependent consumer step. See [portable output integration](portable_languages.md#build-integration-and-verification)
and the [five-language example](../example/interoperability/README.md).


For Java source generation, use `serializer_generate_java(TARGET name SCHEMA file
OUTPUT Schema.java CONFIG java.ini)`. This creates a generation target without
linking a C++ runtime into the application. See [Java CMake usage](java.md#cmake-and-verification).
The `serializer_generate` helper documented below remains C++-specific.

Both helpers accept entry schemas containing `include common;`.
The compiler emits a depfile, and CMake tracks direct and transitive included files
automatically; they do not need to be listed manually in `DEPENDS`. Only list entry
schemas in `SCHEMAS`/`SCHEMA`, since included declarations join the entry's output.
See [include usage](usage.md#share-declarations-with-includes) and
[C++/Java examples](../example/includes/README.md).
Each helper explicitly selects its own backend, so both may share a configuration
with `[output] language = cpp,java`. Schema paths use `.serializer`, and every
schema starts with a supported language header; use `serializer version 1.3.0;`
for inferred arrays and typed magic. The original `1` header remains an exact
alias for `1.0.0`. The installed package supports
`find_package(Serializer 1.5.0 EXACT CONFIG REQUIRED)`; omit `EXACT` to accept a
newer compatible release within the same major version. See
[compiler and schema versioning](command_line.md).

Serializer ships a `serializer_generate` function with both its source tree and
its installable CMake package. Use CMake 3.28+, a C++20 compiler and standard
library, and clang-format 19+ for the default output formatting. The integration
has no editor dependency and requires no `.vscode` files.

## Build this repository

Optional message compression is controlled by `SERIALIZER_WITH_ZSTD`,
`SERIALIZER_WITH_LZ4`, and `SERIALIZER_WITH_ZLIB` (all ON by default). The repository
vcpkg manifest enables the matching `compression-zstd`, `compression-lz4`, and
`compression-zlib` features by default, or provide upstream CMake installations
through `CMAKE_PREFIX_PATH`. For a reduced dependency set, disable unwanted CMake
backends, set `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON`, and select retained manifest
features through `VCPKG_MANIFEST_FEATURES`. Cached values are preserved; reset
`SERIALIZER_BUILD_MANAGED` and `SERIALIZER_WITH_*` with `-U` or override them explicitly. Installed packages rediscover enabled dependencies
for their exported runtime target. See [compression](compression.md) for targets,
limits, and supported format profiles. Missing enabled dependencies are errors.

The root wrappers forward to CMake and preserve its incremental build and schema
generation rules. On a fresh build, tests and all C++ style examples are enabled;
GoogleTest 1.18.0+ and clang-format 19+ must be available. Both platforms provide
`configure`, `all`, `test`, `clean`, and `rebuild` with synchronized defaults and
corresponding options. `all` configures and builds, `test` also runs CTest, and
`configure` stops after configuration. `clean` uses the selected configured tree's
CMake clean target; `rebuild` configures normally and uses CMake's `--clean-first`
to complete native cleanup before building.

| Requirement | When it is needed |
| --- | --- |
| CMake 3.28+ | Every source build; CTest is included with CMake. |
| Zstandard, LZ4, and zlib CMake packages | Default runtime build; the repository vcpkg manifest acquires them automatically. Disable individual backends with `SERIALIZER_WITH_*=OFF`. |
| C++20 compiler and standard library | Library, schema compiler, and C++ consumers; the selected compiler must support the target architecture. |
| Native build tool | GNU Make for `make all`; Ninja for the Linux and Windows presets; MSBuild/Visual Studio C++ tools or another configured generator for the Windows wrapper. |
| PowerShell | Windows `make.ps1` wrapper. |
| Node.js 22+, npm and Visual Studio 2022/2026 MSBuild | Both editor extension packages, included in Windows `make.ps1 all` and `rebuild`; `configure`, `test`, and `clean` remain CMake-only. |
| Git, HTTPS certificates, curl, zip, unzip, tar | Obtaining sources and bootstrapping/downloading vcpkg dependencies; the Linux setup installs these tools. |
| GoogleTest 1.18.0+ CMake config package | `SERIALIZER_BUILD_TESTS=ON`; supplied by this checkout's vcpkg `gtest` dependency or an existing installation, discovered with `find_package(GTest 1.18.0 CONFIG REQUIRED)`. |
| clang-format 19+ | Generated C++ tests, style/iostream/compression examples, benchmarks, fuzzers, and consumer generation with formatting enabled. |
| JDK 17+ (`java` and `javac`) | `SERIALIZER_BUILD_JAVA_EXAMPLES=ON`; Java source generation itself requires no JDK. |
| Official Protobuf library and `protoc` | `SERIALIZER_BUILD_TESTS=ON` together with `SERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS=ON`; Serializer's own Protobuf codecs do not require them. |
| Clang with libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer | `SERIALIZER_BUILD_FUZZERS=ON`; the fuzz target rejects MSVC mode. |

### Windows test runtime DLLs

GoogleTest is used only by repository tests. The vcpkg `gtest` package or an
installed GTest config package at version 1.18.0 or newer supplies its imported
CMake targets; it is separate
from the generated application's runtime dependencies.

Every executable built in `test/` has a Windows post-build step that copies its
transitive imported shared runtime DLLs beside the executable, including shared
GoogleTest libraries. CMake's `TARGET_RUNTIME_DLLS` and `TARGET_FILE_DIR` provide
the paths for the selected configuration, so Debug and Release use their matching
libraries without hardcoded vcpkg locations. Copies occur only when files differ;
an empty dependency list needs no action. CTest and direct launches then find
these DLLs in the executable directory.

The repository manifest sets a `gtest` minimum of 1.18.0 while retaining the
default registry baseline for unrelated dependencies. An older cached
`GTest_DIR` can still select an outdated external installation. Upgrade that
installation or use a vcpkg install satisfying the manifest, clear the override,
and reconfigure with the existing toolchain and build options:

```sh
cmake -S . -B build -U GTest_DIR
```

Rebuild test executables after selecting the new package so their imported
library metadata and staged DLLs match. Changing the cache alone does not update
an already built executable or its local dependency copies.

For an older cached build or a missing `gtest.dll` loader error, reconfigure and
rebuild the affected test target to add or refresh these local copies. For example,
after the normal configure step:

```sh
cmake --build build --config Release --target core_serializer_test
ctest --test-dir build -C Release -R "^core_serializer_test$" --output-on-failure
```

The staging step covers test-directory executables rather than the installation
or deployment of application binaries. See the
[verification record](verification-proprietary-licensing-2026-10-09.md) for the
configurations actually tested.

### Build wrapper commands

`setup.sh` installs the default Linux build tools and bootstraps vcpkg; it does
not install optional Java, Protobuf interoperability, or fuzzing dependencies.
Distribution package names alone do not guarantee the required versions: check
`cmake --version` and use a C++20-capable compiler/standard library. Set
`VCPKG_ROOT` in the shell running Make after setup, as recorded in `~/.bashrc`.

```sh
make all
make test CONFIG=Debug JOBS=8
make clean CONFIG=Debug
make rebuild CONFIG=Debug JOBS=8
make all BUILD_DIR=out/build/minimal CMAKE_ARGS='-DSERIALIZER_BUILD_TESTS=OFF'
```

On Windows, use PowerShell without installing GNU Make:

```powershell
./make.ps1 all
./make.ps1 test -Configuration Debug -Jobs 8
./make.ps1 clean -Configuration Debug
./make.ps1 rebuild -Configuration Debug -Jobs 8
./make.ps1 all -BuildDirectory out/build/minimal -CMakeArgs '-DSERIALIZER_BUILD_TESTS=OFF'
```

After the CMake build succeeds, `./make.ps1 all` and `./make.ps1 rebuild` restore
locked npm dependencies and package both the Visual Studio Code and Visual Studio
extensions. The two manifests must have the same release version; a mismatch or
any packaging failure
fails the command. Packages are written to `out/extensions` and are not installed.
Visual Studio's full-framework MSBuild is required for the VSIX even if CMake
uses Ninja or another generator. First builds need npm/NuGet access for dependencies.
Run `./editors/build.ps1` to package only the extensions, optionally with
`-MSBuildPath '<path to MSBuild.exe>'`. Use direct CMake commands for a CMake-only
build; Windows `configure`, `test`, and `clean` do not package extensions. Linux
wrapper commands build native CMake targets only.

Both default to Release, four build jobs, and `out/build/make-Release`; changing
configuration changes the default directory. Relative PowerShell build paths are
resolved against the repository root, and the script can be invoked from another
directory. Run GNU Make from the repository root (or use `make -C`).

`clean` runs `cmake --build <directory> --config <configuration> --target clean`
only when that build tree has `CMakeCache.txt`. Without a configured tree it is a
successful no-op: it does not configure, acquire dependencies, or validate a vcpkg
toolchain. It preserves the cache, installed dependencies, and extension packages
and never recursively deletes directories. Select the intended build directory
and configuration explicitly when using nondefault values. `rebuild` configures
normally, then runs `cmake --build` with `--clean-first`. CMake completes native
cleanup before building, so `make -j rebuild` cannot race the two steps. Windows
then packages the editor extensions as with `all`. To refresh a stale
configuration instead, follow
[cache recovery](#recover-a-stale-visual-studio-instance).

The common options map between wrappers as follows:

| Linux Make variable | Windows PowerShell parameter | Default |
| --- | --- | --- |
| `CONFIG` | `-Configuration` | `Release` |
| `BUILD_DIR` | `-BuildDirectory` | `out/build/make-<configuration>` |
| `JOBS` | `-Jobs` | `4` |
| `VCPKG_ROOT` | `-VcpkgRoot` | `VCPKG_ROOT` environment value, if set |
| `CMAKE_ARGS` | `-CMakeArgs` | No additional CMake arguments |
| `CMAKE` | `-CMakeCommand` | `cmake` |
| `CTEST` | `-CTestCommand` | `ctest` |

Set `VCPKG_ROOT` to enable its CMake toolchain. Without it, CMake searches for
installed dependencies normally. PowerShell also accepts `-VcpkgRoot` (pass an
empty string to disable automatic toolchain selection on a fresh cache).
Use `CMAKE_ARGS` on Linux or the `-CMakeArgs` string array on Windows for package
paths, compiler/generator selection, or optional Java/benchmark/fuzzer settings.
Use separate build directories when changing compilers, architectures, or
toolchains. Existing cache options are retained unless explicitly overridden.
Override `CMAKE`/`CTEST` or `-CMakeCommand`/`-CTestCommand` when executables are
outside `PATH`. Every wrapper stops on a failed configure, build, clean, test, or
packaging command.

The [iostream examples](../example/iostream/README.md) are included with tests.
To build them without GoogleTest, configure with `SERIALIZER_BUILD_TESTS=OFF`
and `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON`, build
`serializer_iostream_examples`, and run CTest with `-L serializer_iostream`.

The [compression examples](../example/compression/README.md) are also included
with tests for the enabled `SERIALIZER_WITH_*` dependencies. To build them without
GoogleTest, set `SERIALIZER_BUILD_TESTS=OFF` and
`SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON`, build `serializer_compression_examples`,
and run CTest with `-L serializer_compression_examples`. With every optional
compression dependency disabled, only the uncompressed example is built.

### Recover a stale Visual Studio instance

If CMake reports `could not find specified instance of Visual Studio` after
replacing or moving a Visual Studio installation, the build directory may still
cache the old installation in `CMAKE_GENERATOR_INSTANCE`. A successful vcpkg
compiler detection does not replace that cached generator instance.

Refresh the affected CMake configuration using the wrapper's existing argument
forwarding, then build normally:

```powershell
./make.ps1 configure -CMakeArgs '--fresh', '-DSERIALIZER_BUILD_MANAGED=ON'
./make.ps1 all
```

`--fresh` resets `CMakeCache.txt` and the associated `CMakeFiles` configuration;
it does not delete source files or the entire build directory. CMake rediscovers
the installed compiler. Reapply any nondefault options, dependency paths, or
formatter overrides previously supplied only through the cache. The managed
option above is an example: include it when managed support is wanted. The
wrapper continues to supply the configuration and the `VCPKG_ROOT` toolchain
when set. Pass the same `-BuildDirectory` and `-Configuration` to both commands
if the affected build uses nondefault values.

Do not simply replace `CMAKE_GENERATOR_INSTANCE` in an initialized cache: other
cached compiler and tool paths may still refer to the removed installation.
Use a separate build directory when changing generators, architectures, or
toolchains. A refresh cannot install a missing compiler; if discovery still
fails, check that the required Visual Studio C++ build tools are installed.

### Visual Studio folder builds

Install Visual Studio's **Desktop development with C++** workload, including the
MSVC compiler, Windows SDK, and C++ CMake tools for Windows. The repository needs
CMake 3.28+ and clang-format 19+ for its default tests/examples; see
[formatter setup](#formatter-setup). Set `VCPKG_ROOT` to a vcpkg installation
before launching Visual Studio so the presets can load its toolchain and obtain
GoogleTest from the repository manifest.

Open the repository with **File > Open > Folder** and select `DebugWindows` or
`ReleaseWindows`. Both inherit the Ninja generator and an x64 architecture with
`strategy: external`. Visual Studio uses that architecture to initialize the MSVC
environment without passing a generator platform to CMake. Command-line preset
builds need an x64 Native Tools Command Prompt with Ninja on `PATH`:

```sh
cmake --preset DebugWindows
cmake --build --preset DebugWindows
ctest --test-dir out/build/DebugWindows -C Debug --output-on-failure
```

If an older configuration reports `Ninja does not support platform specification`
for `x64`, delete the CMake cache and reconfigure in Visual Studio. The previous
Windows preset used `strategy: set` without selecting a generator, so Visual
Studio's default Ninja generator received an unsupported platform argument.
`CMAKE_CXX_COMPILER not set, after EnableLanguage` can follow that failure even
when MSVC is installed. Do not pass `-A x64` to Ninja; the compiler environment
selects its target architecture. See Microsoft's
[CMake preset documentation](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs).

### Formatter setup

The default build generates C++ test and example headers, so it requires a
runnable clang-format 19 or newer. A successful vcpkg GoogleTest installation
does not supply this host tool, and installing Clang alone may omit it.
On Ubuntu/Debian releases providing the package:

```sh
sudo apt install clang-format-19
make all
```

On Windows, install LLVM or Visual Studio's C++ Clang tools with clang-format
19 or newer, then rerun `./make.ps1 all`. CMake searches versioned executables on
`PATH` and the standard Windows LLVM/Visual Studio locations. A previous
not-found result does not require deleting the build directory; rerunning
configuration searches again.

For an installation outside those locations, pass the executable explicitly:

```sh
make all CMAKE_ARGS='-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=/path/to/clang-format'
```

```powershell
./make.ps1 all -CMakeArgs '-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=C:/Tools/LLVM/bin/clang-format.exe'
```

CMake caches the selected path for subsequent builds. If a cached explicit path
is no longer valid, replace it with the new path. For a compiler/library-only
build without generated tests/examples, use the separate minimal build shown
above; keep tests enabled when intending to run `make test` or `./make.ps1 test`.

### vcpkg package builds

This section describes packaging the current source. The published upstream
`rohit-singh-gautam-serializer` port can use an older revision and different
options; see [source and package availability](distribution.md). The root
development manifest does not control that port's installed capabilities.

A port builds the library and schema compiler directly through CMake; it does
not run `setup.sh` or the repository's Make/PowerShell wrappers. Follow vcpkg's
[maintainer guidance](https://learn.microsoft.com/en-us/vcpkg/contributing/maintainer-guide#do-not-build-testsdocsexamples-by-default)
and explicitly disable development targets:

```cmake
vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DSERIALIZER_BUILD_TESTS=OFF
    -DSERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS=OFF
    -DSERIALIZER_BUILD_MANAGED_EXAMPLES=OFF
    -DSERIALIZER_BUILD_STYLE_EXAMPLES=OFF
    -DSERIALIZER_BUILD_IOSTREAM_EXAMPLES=OFF
    -DSERIALIZER_BUILD_JAVA_EXAMPLES=OFF
    -DSERIALIZER_BUILD_BENCHMARKS=OFF
    -DSERIALIZER_BUILD_FUZZERS=OFF
    -DSERIALIZER_INSTALL=ON
)
```

This configuration builds all runtime features by default and needs Zstandard,
LZ4, and zlib. It needs no GoogleTest, clang-format, JDK, Protobuf runtime, or
sanitizer libraries. Map the compression dependencies to default-enabled port
features, and keep `SERIALIZER_BUILD_MANAGED=ON` for the generated records.
The root repository manifest is for development; a vcpkg port must declare its
own dependencies. Declare `vcpkg-cmake` and `vcpkg-cmake-config` as host
dependencies when using their helpers; vcpkg manages its CMake/Ninja tooling,
while the CI host supplies the compiler and platform SDK. Keep download hashes,
patches, CMake package/tool relocation, copyright installation, and version
database entries consistent in the port repository.

Installing the package is separate from running its schema compiler. C++
generation still requires a host clang-format when formatting is enabled; pass
`CLANG_FORMAT` to `serializer_generate`, configure the CLI formatter path, or
explicitly select `format = false` for a separate formatting pipeline. Cross
compilation also requires a host-runnable Serializer through `GENERATOR`;
the target package's executable may be unable to run on the build host.

Package verification must cover the advertised platforms and linkage modes.
Windows shared-library builds require exported symbols; the current source has
no DLL export annotations, so a Windows port must select static library linkage.
Linux package checks do not establish macOS, Android, or ARM compatibility.

## Use a source dependency

With a Serializer checkout at `vendor/Serializer`, a complete consumer build is:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

set(SERIALIZER_BUILD_TESTS OFF CACHE BOOL "Build Serializer's own tests")
add_subdirectory(vendor/Serializer)

add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.serializer)
```

The same helper is available after bringing in the source with CMake's
`FetchContent_MakeAvailable`. Use the application's existing dependency mechanism.
For a complete schema and C++ consumer, see the [usage guide](usage.md).

Configure and build the application:

```sh
cmake -S . -B build
cmake --build build --config Debug --target my_app
```

The build compiles the Serializer generator when needed, produces
`build/generated/my_app/account.hpp`, then compiles `my_app`. The consumer can
write `#include <account.hpp>`; the helper supplies its include directory, links
`Serializer::runtime`, and propagates the C++20 minimum. Existing newer
language modes are preserved. Editing the schema, generator, or tracked output
configuration makes the next build regenerate the header.

### Schema-scanner configuration

`SERIALIZER_ENABLE_SIMD` defaults to `ON` and controls both schema scanning and
runtime JSON scanning/binary array byte swapping. Supported x64 builds provide
SSE2 and isolated AVX2 backends selected after CPU/OS checks. Short spans and
other architectures use scalar code; universal macOS builds use the baseline
scanner for each architecture. The AVX2 compiler flags apply only to its source
files and are not propagated to applications.

Set `SERIALIZER_ENABLE_SIMD=OFF` in the CMake cache before adding Serializer to
disable its explicit SIMD backends. Installed generators and runtime libraries
retain the choice made when they were built; `serializer_generate` does not change it.
Applications using pre-generated headers also link `Serializer::runtime`
for the shared runtime helpers. Bulk array reads/writes remain enabled when SIMD is disabled.
This is separate from output coding profiles and requires no schema syntax
changes. See the
[performance guide](performance.md#simd-in-the-schema-compiler) for scope and validation status.

## Use an installed package

To prepare an installation from a Serializer checkout:

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_INSTALL=ON
cmake --build build/package --config Release
cmake --install build/package --config Release --prefix /path/to/serializer-install
```

Use a writable prefix appropriate for the platform, such as `C:/deps/Serializer`
on Windows. This installs the library, generator executable, public headers,
licensing files, exported CMake targets, and generation helper. `SERIALIZER_INSTALL`
defaults to enabled for a top-level Serializer build and disabled when embedded.
It does not install a compiler, editor extension, or clang-format.

The consumer then uses:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

find_package(Serializer CONFIG REQUIRED)
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.serializer)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/serializer-install
cmake --build build --config Release --target my_app
```

The package exposes `Serializer::runtime` and `Serializer::serializer`,
with the same names as the source-dependency aliases. Generation uses the installed
executable. Choose an installation compatible with the consuming compiler and
target platform; the generation executable must also run on the build host.
This is CMake install/export support, not an automatically downloaded binary release.

`Serializer::runtime` contains the application runtime and uses 0BSD, permitting
proprietary applications. `Serializer::managed`, when enabled, uses the same
runtime license and links this target. The existing `Serializer::serializer_lib`
target remains available for compiler/parser/generator API compatibility and
continues under GPL-3.0-or-later; generated-model applications should use the
runtime target. Running the GPL generator at build time does not license its output
under GPL. See [licensing](licensing.md) for schema notices and dependency terms.

## Helper options

Call `serializer_generate` once per target and list all its schemas together:

```cmake
serializer_generate(TARGET my_app
  SCHEMAS schemas/account.serializer schemas/address.serializer
  CONFIG serializer_output.ini
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/my_app")
```

| Argument | Behavior |
| --- | --- |
| `TARGET` | Required existing local executable or library; aliases and imported targets are rejected |
| `SCHEMAS` | Required list of existing schema files; each produces `<stem>.hpp` |
| `OUTPUT_DIRECTORY` | Defaults to `${CMAKE_CURRENT_BINARY_DIR}/generated/<target>`; relative paths use the current build directory |
| `CONFIG` | Generator INI file; tracked for regeneration |
| `CODING_STANDARD` | Overrides the C++ profile from the config |
| `FORMAT_FILE` | Overrides and tracks a custom clang-format YAML file |
| `CLANG_FORMAT` | Overrides the formatter executable; otherwise uses `SERIALIZER_CLANG_FORMAT_EXECUTABLE` when set, then the generator's config/default |
| `GENERATOR` | Executable target or existing executable path; defaults to `Serializer::serializer` |
| `VISIBILITY` | `PRIVATE`, `PUBLIC`, or `INTERFACE`; defaults to `PRIVATE` except for interface libraries, which require `INTERFACE` |
| `DEPENDS` | Additional files or targets affecting generation, using CMake's custom-command dependency rules |
| `VERSION_POLICY_AS_OF` | Optional pinned `YYYY-MM-DD` for compile-time release policies; supported by all generation helpers |
| `VERSION_POLICY_WARNINGS_AS_ERRORS` | Flag promoting current-release expiry warnings to generation failure; supported by all helpers |

Relative schema, config, format-file, and generator paths use the current source
directory. Prefer an absolute path for `CLANG_FORMAT`; a bare executable name is
resolved on `PATH` by the generator. By default it invokes `clang-format`.
CLI overrides from the helper take precedence over values in the config.
When building this repository's tests or C++ examples, CMake automatically finds
a runnable clang-format 19+ through its normal program search paths. On Windows,
it also checks standard LLVM locations and Visual Studio installations, including
instances other than the selected compiler's installation. Deleting the build
cache triggers discovery again. An explicit `SERIALIZER_CLANG_FORMAT_EXECUTABLE`
still takes precedence and is version-checked. If no suitable tool is installed,
configuration explains which dependency to install; it does not download tools.
This discovery applies to repository builds requiring formatting; standalone CLI
and consuming-project overrides continue to follow the rules above.
For full output settings, including naming and disabling formatting for a separate
formatting pipeline, use [the INI configuration](output_configuration.md).

A YAML file selected only inside an INI config is not automatically discovered as
a build dependency; list it in `DEPENDS`, or select it with `FORMAT_FILE` instead.
Use explicit dependencies for any other indirect inputs. Inputs must exist at
configuration time. Keep outputs in the build tree and do not edit them manually.
Use distinct output directories for different profiles; the helper rejects
multiple rules writing the same destination, including duplicate schema stems.

Cross-compilation needs a host-built Serializer executable. Pass its absolute
path as `GENERATOR`; the helper rejects the default source-built target during
cross-compilation. The runtime library is still built for the application's target.

## Generate several configurations from one parse

Use `serializer_generate_variants` when several C++ targets need different INI
configurations of the same schemas. It is available from both source dependencies
and installed packages starting with compiler/runtime **1.5.0**:

```cmake
add_library(models32 INTERFACE)
add_library(models64 INTERFACE)
add_library(models_separate64 INTERFACE)

serializer_generate_variants(
  TARGETS models32 models64 models_separate64
  SCHEMAS schemas/dimensions_managed.serializer
  CONFIGS profiles/managed32.ini profiles/managed64.ini profiles/separate64.ini
  OUTPUT_DIRECTORIES
    "${CMAKE_CURRENT_BINARY_DIR}/generated/models32"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/models64"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/models_separate64")
```

Each `CONFIGS` entry belongs to the corresponding `TARGETS` entry. Supply one INI
per target; omitting `CONFIGS` is allowed only for a single target using defaults.
The three profiles above can set `id_type` under `[managed]` to `uint32`,
`uint64`, and `uint64` with `separate_values = true`, respectively.
Link each consumer to its selected model target. Keep each compiled
consumer's generated model definitions consistent; different profiles do not
automatically gain distinct C++ namespaces.

`OUTPUT_DIRECTORIES` is optional and defaults to
`${CMAKE_CURRENT_BINARY_DIR}/generated/<target>` for each target. When supplied,
its length must match `TARGETS`; relative entries use the current build directory.
Each schema produces `<stem>.hpp` in every target's directory. Targets must be
existing local executables or libraries, and output paths must be distinct.

The helper accepts the same shared `CODING_STANDARD`, `FORMAT_FILE`,
`CLANG_FORMAT`, `GENERATOR`, `VISIBILITY`, `VERSION_POLICY_AS_OF`,
`VERSION_POLICY_WARNINGS_AS_ERRORS`, and `DEPENDS` arguments as `serializer_generate`.
Shared CLI overrides apply to every configuration. Omitted `VISIBILITY` selects
`PRIVATE` for ordinary targets and `INTERFACE` for interface libraries independently;
an explicit visibility must be valid for every target. Schema and config paths
follow the existing source-directory rules. The helper explicitly selects C++.

For each entry schema, one custom command invokes the compiler with all configs
and outputs. The compiler parses that schema and its includes once, prepares every
variant, then writes the headers and one shared depfile. A single generation rule
owns the batch, so building several consumers concurrently cannot run competing
commands for the same outputs. Changing a tracked schema, config, formatter input,
or generator regenerates that schema's variants together; an unchanged incremental
build skips generation. All configurations participate when any registered target
requests that batch.

Each target retains its own include directory, runtime linkage, generated-header
properties, and `<target>_serializer_headers` target. The existing
`serializer_generated_headers` aggregate also includes all variants. Editor build
and navigation discovery therefore use the same target and depfile contracts.
For direct compiler calls, see
[batch-generation output ordering and failure behavior](command_line.md#generate-several-configurations-from-one-parse).

## Share a schema between targets

Generate once on an interface library and link consumers to it:

```cmake
add_library(app_models INTERFACE)
serializer_generate(TARGET app_models SCHEMAS schemas/account.serializer)

add_executable(client client.cpp)
add_executable(server server.cpp)
target_link_libraries(client PRIVATE app_models)
target_link_libraries(server PRIVATE app_models)
```

Both consumers inherit the include directory, runtime dependency, and generation
ordering. [The qualification build](../qualification/CMakeLists.txt) uses this
pattern. `PUBLIC` on an ordinary library also propagates the generated API to its
consumers. `INTERFACE` propagates it only to consumers, not the target's own sources.
If publishing such a library, install its generated public headers and provide
the matching install include directory yourself; this helper attaches generated
headers and their include directory to the build interface only.

## Generate headers before editing

A normal application build already performs generation. To prepare headers
without compiling the application, use either target after configuration:

```sh
cmake --build build --config Debug --target my_app_serializer_headers
cmake --build build --config Debug --target serializer_generated_headers
```

The first refreshes one consumer's schemas. The second refreshes all schemas
registered with this helper in the current build, including enabled Serializer
fixtures/examples. Both build the source-dependency generator when needed;
neither runs tests. The consumer also exposes `SERIALIZER_GENERATED_HEADERS` and
`SERIALIZER_HEADERS_TARGET` CMake target properties for tooling.

CMake configuration supplies include paths but does not execute generation.
IntelliSense needs the real header to exist after a first successful build or
header-generation step. Select an editor's CMake integration to obtain per-target
settings; see [IntelliSense troubleshooting](intellisense.md).

**Validation status:** the [verification record](verification-2026-09-17.md)
records CMake configuration, generation, compilation, and execution of the
repository's fixtures and examples, plus a separate installed-package consumer
check in the compatibility/exact-decoding follow-up. Editor verification was not
performed in those runs; see the record's scope before extending its results to
other configurations.

## All language examples and native codec tests

`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON` registers `serializer_language_examples`
and `serializer_native_codecs`. Build the C++ compiler first; these CTest commands
then compile the target SDK consumers and run all four examples per language,
any-to-any exchanges, and malformed-input suites. See [SDK requirements](../example/README.md).
Set `SERIALIZER_EXAMPLE_WSL_LANGUAGES=c,rust,swift` for explicitly selected WSL
SDKs on Windows, and `SERIALIZER_EXAMPLE_SANITIZERS=ON` for GCC/Clang C sanitizers.
These options do not change the compiler implementation or require SDKs during
an ordinary build. The previous five-runtime CMake subset remains available.

For reproducible [release policies](versioning.md#reference-dates-and-reproducible-generation), pass `VERSION_POLICY_AS_OF 2026-10-06` to the helper. Incremental generation remains dependency-driven: a date change alone does not rebuild existing output. Change the pinned value or explicitly rerun generation to advance age-based bounds.
