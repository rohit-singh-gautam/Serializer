# CMake integration for consumers

For Java source generation, use `serializer_generate_java(TARGET name SCHEMA file
OUTPUT Schema.java CONFIG java.ini)`. This creates a generation target without
linking a C++ runtime into the application. See [Java CMake usage](java.md#cmake-and-verification).
The `serializer_generate` helper documented below remains C++-specific.

Both helpers accept entry schemas containing `include common.serializer;`.
The compiler emits a depfile, and CMake tracks direct and transitive included files
automatically; they do not need to be listed manually in `DEPENDS`. Only list entry
schemas in `SCHEMAS`/`SCHEMA`, since included declarations join the entry's output.
See [include usage](usage.md#share-declarations-with-includes) and
[C++/Java examples](../example/includes/README.md).
Each helper explicitly selects its own backend, so both may share a configuration
with `[output] language = cpp,java`. Schema paths use `.serializer`, and every
schema starts with `serializer version 1;`. The installed package supports
`find_package(Serializer 1.0.0 EXACT CONFIG REQUIRED)`; see
[compiler and schema versioning](command_line.md).

Serializer ships a `serializer_generate` function with both its source tree and
its installable CMake package. Use CMake 3.28+, a C++20 compiler and standard
library, and clang-format 19+ for the default output formatting. The integration
has no editor dependency and requires no `.vscode` files.

## Build this repository

The root wrappers forward to CMake and preserve its incremental build and schema
generation rules. On a fresh build, tests and all C++ style examples are enabled;
GoogleTest and clang-format 19+ must be available. `all` configures and builds,
`test` also runs CTest, and `configure` stops after configuration.

| Requirement | When it is needed |
| --- | --- |
| CMake 3.28+ | Every source build; CTest is included with CMake. |
| C++20 compiler and standard library | Library, schema compiler, and C++ consumers; the selected compiler must support the target architecture. |
| Native build tool | GNU Make for `make all`; Ninja for the Linux and Windows presets; MSBuild/Visual Studio C++ tools or another configured generator for the Windows wrapper. |
| PowerShell | Windows `make.ps1` wrapper. |
| Git, HTTPS certificates, curl, zip, unzip, tar | Obtaining sources and bootstrapping/downloading vcpkg dependencies; the Linux setup installs these tools. |
| GoogleTest CMake package | `SERIALIZER_BUILD_TESTS=ON`; supplied by this checkout's vcpkg manifest or an existing installation. |
| clang-format 19+ | Generated C++ tests, style examples, benchmarks, fuzzers, and consumer generation with formatting enabled. |
| JDK 17+ (`java` and `javac`) | `SERIALIZER_BUILD_JAVA_EXAMPLES=ON`; Java source generation itself requires no JDK. |
| Official Protobuf library and `protoc` | `SERIALIZER_BUILD_TESTS=ON` together with `SERIALIZER_BUILD_PROTOBUF_INTEROP_TESTS=ON`; Serializer's own Protobuf codecs do not require them. |
| Clang with libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer | `SERIALIZER_BUILD_FUZZERS=ON`; the fuzz target rejects MSVC mode. |

`setup.sh` installs the default Linux build tools and bootstraps vcpkg; it does
not install optional Java, Protobuf interoperability, or fuzzing dependencies.
Distribution package names alone do not guarantee the required versions: check
`cmake --version` and use a C++20-capable compiler/standard library. Set
`VCPKG_ROOT` in the shell running Make after setup, as recorded in `~/.bashrc`.

```sh
make all
make test CONFIG=Debug JOBS=8
make all BUILD_DIR=out/build/minimal CMAKE_ARGS='-DSERIALIZER_BUILD_TESTS=OFF'
```

On Windows, use PowerShell without installing GNU Make:

```powershell
./make.ps1 all
./make.ps1 test -Configuration Debug -Jobs 8
./make.ps1 all -BuildDirectory out/build/minimal -CMakeArgs '-DSERIALIZER_BUILD_TESTS=OFF'
```

Both default to Release, four build jobs, and `out/build/make-Release`; changing
configuration changes the default directory. Relative PowerShell build paths are
resolved against the repository root, and the script can be invoked from another
directory. Run GNU Make from the repository root (or use `make -C`).

Set `VCPKG_ROOT` to enable its CMake toolchain. Without it, CMake searches for
installed dependencies normally. PowerShell also accepts `-VcpkgRoot` (pass an
empty string to disable automatic toolchain selection on a fresh cache).
Use `CMAKE_ARGS` on Linux or the `-CMakeArgs` string array on Windows for package
paths, compiler/generator selection, or optional Java/benchmark/fuzzer settings.
Use separate build directories when changing compilers, architectures, or
toolchains. Existing cache options are retained unless explicitly overridden.
Every wrapper stops on a failed configure, build, or test command.

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
    -DSERIALIZER_BUILD_STYLE_EXAMPLES=OFF
    -DSERIALIZER_BUILD_JAVA_EXAMPLES=OFF
    -DSERIALIZER_BUILD_BENCHMARKS=OFF
    -DSERIALIZER_BUILD_FUZZERS=OFF
    -DSERIALIZER_INSTALL=ON
)
```

This configuration needs no GoogleTest, clang-format, JDK, Protobuf runtime, or
sanitizer libraries. Declare `vcpkg-cmake` and `vcpkg-cmake-config` as host
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
`Serializer::serializer_lib`, and propagates the C++20 minimum. Existing newer
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
Applications using pre-generated headers also link `Serializer::serializer_lib`
for the shared runtime helpers. Bulk array reads/writes remain enabled when SIMD is disabled.
This is separate from output coding profiles and requires no schema syntax
changes. See the
[README](../README.md#simd-in-the-schema-compiler) for scope and validation status.

## Use an installed package

To prepare an installation from a Serializer checkout:

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_INSTALL=ON
cmake --build build/package --config Release
cmake --install build/package --config Release --prefix /path/to/serializer-install
```

Use a writable prefix appropriate for the platform, such as `C:/deps/Serializer`
on Windows. This installs the library, generator executable, public headers,
license, exported CMake targets, and generation helper. `SERIALIZER_INSTALL`
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

The package exposes `Serializer::serializer_lib` and `Serializer::serializer`,
with the same names as the source-dependency aliases. Generation uses the installed
executable. Choose an installation compatible with the consuming compiler and
target platform; the generation executable must also run on the build host.
This is CMake install/export support, not an automatically downloaded binary release.

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

**Validation status:** these rules are implemented in source and used by the
repository's fixtures and examples. CMake configuration, generation, compilation,
install/package consumption, and editor verification have not been run for this
implementation step. The commands above are provided for later validation.
