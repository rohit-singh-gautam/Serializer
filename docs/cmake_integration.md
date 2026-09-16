# CMake integration for consumers

For Java source generation, use `serializer_generate_java(TARGET name SCHEMA file
OUTPUT Schema.java CONFIG java.ini)`. This creates a generation target without
linking a C++ runtime into the application. See [Java CMake usage](java.md#cmake-and-verification).
The `serializer_generate` helper documented below remains C++-specific.
Each helper explicitly selects its own backend, so both may share a configuration
with `[output] language = cpp,java`. Schema paths use `.serializer`, and every
schema starts with `serializer version 1;`. The installed package supports
`find_package(Serializer 0.1.0 EXACT CONFIG REQUIRED)`; see
[compiler and schema versioning](command_line.md).

Serializer ships a `serializer_generate` function with both its source tree and
its installable CMake package. Use CMake 3.28+, a C++20 compiler and standard
library, and clang-format 19+ for the default output formatting. The integration
has no editor dependency and requires no `.vscode` files.

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
for the shared runtime helpers. Bulk writes remain enabled when SIMD is disabled.
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
