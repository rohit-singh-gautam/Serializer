# Schema include examples for C++ and Java

Each folder contains a `request.serializer` entry schema, its dependencies, and
both `main.cpp` and `Main.java`. Every consumer round-trips its model through JSON
and all three standard binary protocols. Generated code stays in the build tree.

| Folder | Demonstrates |
| --- | --- |
| [shared_types](shared_types/request.serializer) | `include common.serializer;`, a qualified class reference, and an included enum/default |
| [reopened_namespaces](reopened_namespaces/request.serializer) | Reopening `demo` across files, plus separate `demo::account` and `audit::account` types |
| [diamond](diamond/request.serializer) | Two branches including the same file via `../`, an explicit repeated include, and inheritance from an included type |

Includes use bare paths and a semicolon:

```text
serializer version 1;
include common.serializer;
include ../shared/types.serializer;
```

Paths resolve from the including file. Each included file has its own version
header. Include files before declarations; do not wrap includes inside namespaces.
Generate the entry schema only: included declarations become part of that entry's
header or Java outer class. Overlapping generated C++ headers can redefine shared
types if used together. See the [complete include contract](../../docs/usage.md#share-declarations-with-includes).

## Build and run all six consumers

From the repository root, using a configured C++20 toolchain, clang-format 19+,
GoogleTest, and JDK 17+:

```sh
cmake -S . -B out/include-examples -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_JAVA_EXAMPLES=ON
cmake --build out/include-examples --config Debug
ctest --test-dir out/include-examples -C Debug -R serializer_include_ --output-on-failure
```

Use the [platform build setup](../../docs/cmake_integration.md#build-this-repository)
for compiler and dependency discovery. The normal test build includes all three C++
consumers. `SERIALIZER_BUILD_JAVA_EXAMPLES=ON` adds all three Java consumers.

Verified on Windows x64 with MSVC and JDK 17: all six consumers compiled and passed
their round trips. The parser/CLI regression suite and a Ninja rebuild test also
passed, including edits to newly added transitive dependencies.

To generate a single pair directly, create an output directory and use the built
compiler (substitute any of the other example folders):

```sh
serializer --input example/includes/shared_types/request.serializer --language cpp,java --cpp.output out/request.hpp --java.output out/IncludeSchema.java --depfile out/includes.d
javac --release 17 -d out/include-classes out/IncludeSchema.java example/includes/shared_types/Main.java
java -cp out/include-classes Main
```

Compile `main.cpp` using the generated header directory and
`Serializer::serializer_lib`, as shown by this folder's [CMake configuration](CMakeLists.txt).
The CMake helpers automatically track changes in included schemas for both languages.

## Conflicts are caught during parsing

Reopening a namespace reuses its scope; it does not allow a second type with an
occupied name. This is rejected even when the declarations come from different files:

```text
namespace demo { class account {} }
namespace demo { class account {} } // Duplicate type: demo::account
```

Classes and enums share that type-name space. A namespace also cannot reuse a
class or enum name at the same level. Equal leaf names in different namespaces
are valid, as demonstrated by `reopened_namespaces`.
