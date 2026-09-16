# C++ coding-style examples

Every profile has its own folder containing `account.serializer`, its configuration, and
its C++ consumer. Each program writes an owning
object, changes its account ID through a mutable view, decodes it, and checks the
original JSON field and enum names. The configurations select different C++ names
and layouts; the wire contract is identical.

| Profile | Schema | Configuration | C++ consumer |
| --- | --- | --- | --- |
| Serializer | [account.serializer](serializer/account.serializer) | [serializer.ini](serializer/serializer.ini) | [serializer.cpp](serializer/serializer.cpp) |
| C++ Core Guidelines | [account.serializer](core/account.serializer) | [core.ini](core/core.ini) | [core.cpp](core/core.cpp) |
| Google | [account.serializer](google/account.serializer) | [google.ini](google/google.ini) | [google.cpp](google/google.cpp) |
| LLVM | [account.serializer](llvm/account.serializer) | [llvm.ini](llvm/llvm.ini) | [llvm.cpp](llvm/llvm.cpp) |
| GNU | [account.serializer](gnu/account.serializer) | [gnu.ini](gnu/gnu.ini) | [gnu.cpp](gnu/gnu.cpp) |
| SEI CERT | [account.serializer](cert/account.serializer) | [cert.ini](cert/cert.ini) | [cert.cpp](cert/cert.cpp) |
| MISRA | [account.serializer](misra/account.serializer) | [misra.ini](misra/misra.ini) | [misra.cpp](misra/misra.cpp) |
| AUTOSAR | [account.serializer](autosar/account.serializer) | [autosar.ini](autosar/autosar.ini) | [autosar.cpp](autosar/autosar.cpp) |
| Qt | [account.serializer](qt/account.serializer) | [qt.ini](qt/qt.ini) | [qt.cpp](qt/qt.cpp) |

See [output configuration](../../docs/output_configuration.md) for the precise
scope of these presentation profiles and their compliance limitations.

## IntelliSense without building the example programs

Each `account.hpp` is generated into its profile's own build directory. If VS Code
reports it as missing, follow the [IntelliSense guide](../../docs/intellisense.md).
These examples use the shipped [CMake helper](../../docs/cmake_integration.md).
After configuring the selected preset, build `serializer_generated_headers` to
refresh enabled headers without compiling the examples or running tests. For one
profile, build `serializer_style_<profile>_serializer_headers`. The generator is
built when needed. A normal example build also generates its header automatically;
no VS Code task or tracked `.vscode` configuration is required.

## Generate one example

From the repository root, after building `serializer` and installing clang-format 19+:

```sh
mkdir build/example-google
serializer --input example/coding_styles/google/account.serializer --output build/example-google/account.hpp --config example/coding_styles/google/google.ini
```

Compile [google.cpp](google/google.cpp) with this generated header directory and the
repository's `include` directory on its include path. Use C++20 or newer.
For another profile, select its `.ini` and corresponding `.cpp` from the table.
Keep headers for different profiles in separate directories and executables;
they define alternative C++ APIs for the same schema.

## Build all examples

Use the following commands to build every profile; each executable runs its checks.


```sh
cmake -S . -B build/styles -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_STYLE_EXAMPLES=ON
cmake --build build/styles --config Debug
```

Set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` to an absolute executable path if the
formatter is not on `PATH`. CMake generates each header with that formatter and
its matching config. Each executable is named `serializer_style_<profile>` and
returns zero when the checks succeed. It does not need GoogleTest.

With `SERIALIZER_BUILD_TESTS=ON`, all nine examples are built and registered with
CTest alongside the library tests. The Java backend change also validates all nine C++ examples.

For Java style profiles, see [Java examples](../java/README.md).
