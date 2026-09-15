# IntelliSense for generated C++ headers

An error such as `cannot open source file "account.hpp"` means IntelliSense
cannot find the generated header. Two things must be available:

1. The real header, produced from the schema by the Serializer compiler.
2. The consumer's CMake configuration, including its generated include directory,
   compiler, and C++ language mode.

## Consumer projects

Use the shipped [CMake integration](cmake_integration.md):

```cmake
# After add_subdirectory(vendor/Serializer) or find_package(Serializer CONFIG REQUIRED):
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.def)
```

An ordinary build generates `account.hpp` before compiling the consumer and sets
the target's include paths. This works independently of VS Code. No Serializer
IntelliSense extension or custom editor task is required. The repository keeps
`.vscode/*` ignored; editor preferences remain local to the user.

Configuration alone does not create the header. After configuring a fresh build,
build the application once, or generate only its headers:

```sh
cmake --build build --config Debug --target my_app_serializer_headers
```

Generation still builds the Serializer generator when using a source dependency.
It does not compile the application or run tests. Honor any active instruction
to defer configuration, generation, or builds before executing these commands.

## VS Code configuration

Install Microsoft C/C++ and CMake Tools. Open the application's CMake project
root and configure its normal build with CMake Tools. Select **CMake Tools** as
the C/C++ configuration provider. This can be a user-level setting, so it does
not require a committed workspace file:

```json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

CMake Tools supplies source-specific include directories, compiler settings,
and language mode through the existing
[C/C++ configuration-provider interface](https://code.visualstudio.com/docs/cpp/configure-intellisense).
Use **CMake: Build** normally, or select the `serializer_generated_headers` build
target to refresh all registered headers without building consumers. With presets,
select the matching configure/build presets first.

After generation, let IntelliSense reparse the source. If the error remains, use
**C/C++: Log Diagnostics** to check the selected provider and the source's generated
include directory. Reconfigure after changing CMake targets or options. Rebuild
after changing schemas or output configs so generated declarations are current.

## This repository's coding-style examples

Every coding profile generates a separate `account.hpp`. With `DebugWindows`,
the Google example uses:

```text
out/build/DebugWindows/example/coding_styles/google/account.hpp
```

The LLVM header is in `llvm/account.hpp` under the same build directory. Each
consumer needs its own profile's directory. Keep these paths on the CMake targets;
combining all profile directories into a recursive editor include path could
select an API with the wrong C++ naming convention.

When generation is allowed, select `DebugWindows` (Windows) or `DebugLinux`
(Linux), then run **CMake: Configure**. The repository presets need `VCPKG_ROOT`;
the standard test build also requires GoogleTest. Header generation needs a
C++20 compiler and clang-format 19+. If the formatter is not found, configure with
`SERIALIZER_CLANG_FORMAT_EXECUTABLE` set to its absolute executable path.

For a configured Windows preset, generate every enabled header with:

```sh
cmake --build --preset DebugWindows --target serializer_generated_headers
```

For only the Google example's header:

```sh
cmake --build --preset DebugWindows --target serializer_style_google_serializer_headers
```

Reconfigure existing build directories once to make the new targets available.
The aggregate target includes schemas from every `serializer_generate` call;
the repository's built-in schemas depend on these options:

| Enabled feature | Headers included |
| --- | --- |
| `SERIALIZER_BUILD_TESTS=ON` | Test fixtures and all nine coding-style examples |
| `SERIALIZER_BUILD_STYLE_EXAMPLES=ON` | All nine coding-style examples |
| Benchmarks or fuzzers enabled | Qualification records |
| All of these disabled | No built-in schemas; consumer registrations still apply |

To browse examples with tests disabled, configure with
`-DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_STYLE_EXAMPLES=ON`.

## Other editors

Use the editor's CMake integration to read the consumer's target settings.
For editors using `compile_commands.json`, enable `CMAKE_EXPORT_COMPILE_COMMANDS`
with a supported generator and point the editor at that build's database. CMake
supports this export with Makefile and Ninja generators; the Visual Studio
generator used by the Windows preset does not provide it. In either case, the
real generated header must exist. See
[CMake's compile-command export documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html).

**Validation status:** CMake configuration, header generation, compilation, and
editor verification remain deferred for this change. The missing-header diagnostic
will remain until generation succeeds and IntelliSense receives the matching
CMake configuration.
