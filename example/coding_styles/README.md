# C++ coding-style examples

Every profile uses [account.def](account.def). Each program writes an owning
object, changes its account ID through a mutable view, decodes it, and checks the
original JSON field and enum names. The configurations select different C++ names
and layouts; the wire contract is identical.

| Profile | Configuration | C++ consumer |
| --- | --- | --- |
| Serializer | [serializer.ini](serializer.ini) | [serializer.cpp](serializer.cpp) |
| C++ Core Guidelines | [core.ini](core.ini) | [core.cpp](core.cpp) |
| Google | [google.ini](google.ini) | [google.cpp](google.cpp) |
| LLVM | [llvm.ini](llvm.ini) | [llvm.cpp](llvm.cpp) |
| GNU | [gnu.ini](gnu.ini) | [gnu.cpp](gnu.cpp) |
| SEI CERT | [cert.ini](cert.ini) | [cert.cpp](cert.cpp) |
| MISRA | [misra.ini](misra.ini) | [misra.cpp](misra.cpp) |
| AUTOSAR | [autosar.ini](autosar.ini) | [autosar.cpp](autosar.cpp) |
| Qt | [qt.ini](qt.ini) | [qt.cpp](qt.cpp) |

See [output configuration](../../docs/output_configuration.md) for the precise
scope of these presentation profiles and their compliance limitations.

## Generate one example

From the repository root, after building `serializer` and installing clang-format 19+:

```sh
mkdir build/example-google
serializer input example/coding_styles/account.def output build/example-google/account.hpp config example/coding_styles/google.ini
```

Compile [google.cpp](google.cpp) with this generated header directory and the
repository's `include` directory on its include path. Use C++20 or newer.
For another profile, select its `.ini` and corresponding `.cpp` from the table.
Keep headers for different profiles in separate directories and executables;
they define alternative C++ APIs for the same schema.

## Build all examples

The following commands are provided for the later validation step; they have
**not been run** for this change:

```sh
cmake -S . -B build/styles -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_STYLE_EXAMPLES=ON
cmake --build build/styles --config Debug
```

Set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` to an absolute executable path if the
formatter is not on `PATH`. CMake generates each header with that formatter and
its matching config. Each executable is named `serializer_style_<profile>` and
returns zero when the checks succeed. It does not need GoogleTest.

With `SERIALIZER_BUILD_TESTS=ON`, all nine examples are built and registered with
CTest alongside the library tests. Header generation, compilation, and execution
remain deferred for this implementation step.
