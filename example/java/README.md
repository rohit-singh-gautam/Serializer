# Java examples

Four runnable examples live directly under `example/java`:

| Folder | What it demonstrates |
| --- | --- |
| [basic](basic/) | Scalars, full-width IDs, a wire-name override, defaults, and a typed field edit |
| [collections](collections/) | Typed arrays, maps, enums, Unicode, embedded NUL, and integer boundaries |
| [complex](complex/) | An order archive spanning 13 shared schema files, nested/diamond includes, parent composition, orders, customers, inventory, payments, shipments, audit unions, and aggregate maps |
| [interoperability](interoperability/) | Independent typed fixture construction and any-to-any exchange across all four protocols and three union alternatives |

From the repository root, after building the C++ compiler:

```sh
python example/run.py --compiler build/serializer --language java
```

Use `build/serializer.exe` on Windows and run from a developer command prompt
for MSVC. `--example complex` selects one example. Generated code and executables
stay under `out/examples`; maintained source is never overwritten. Each folder
contains a `model.serializer` entry that includes the shared contract. The runner
compiles that shared contract once per example for all requested language outputs.
See [SDK setup and runner options](../README.md).

The first three programs accept `<input.json> <output.json>`. They decode the
independent fixture, increment `revision` through the typed model, exercise all
four codecs, and write the result. The runner compares all resulting JSON data
against the independent fixture, including exact 64-bit integers. The fourth
program accepts `<fixtures-directory> emit|verify`; the runner supplies the
producer manifest and runs every producer before any consumer.

## Additional Java profile examples

Each profile example has its own `account.serializer`, `java.ini`, and `Main.java`.
Generated `AccountSchema.java` and `.class` files belong in separate build folders.

| Example | Schema | Config | Consumer |
| --- | --- | --- | --- |
| Round trip | [account.serializer](round_trip/account.serializer) | [java.ini](round_trip/java.ini) | [Main.java](round_trip/Main.java) |
| Serializer style | [account.serializer](coding_styles/serializer/account.serializer) | [java.ini](coding_styles/serializer/java.ini) | [Main.java](coding_styles/serializer/Main.java) |
| Google style | [account.serializer](coding_styles/google/account.serializer) | [java.ini](coding_styles/google/java.ini) | [Main.java](coding_styles/google/Main.java) |
| Oracle style | [account.serializer](coding_styles/oracle/account.serializer) | [java.ini](coding_styles/oracle/java.ini) | [Main.java](coding_styles/oracle/Main.java) |

Each consumer exercises all four protocols, unsigned values, UTF-8 text, arrays,
maps, enums, and union alternatives. The schemas deliberately share a wire
contract but are separate files so each style can be inspected independently.

```sh
cmake -S . -B build/java -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_JAVA_EXAMPLES=ON
cmake --build build/java --config Debug
java -cp build/java/example/java/serializer_java_round_trip/classes serializer.example.Main
```

CMake requires a JDK 17+ installation and compiles each example as Java 17 with
warnings treated as errors. With tests enabled, CTest also runs the examples.
The targets are `serializer_java_round_trip`, `serializer_java_style_serializer`,
`serializer_java_style_google`, and `serializer_java_style_oracle`.

See [Java output](../../docs/java.md) for direct CLI/javac commands, API mappings,
profile scope, protocol compatibility, resource limits, and current limitations.
