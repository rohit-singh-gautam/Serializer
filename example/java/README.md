# Pure Java examples

Every example has its own `account.def`, `java.ini`, and `Main.java`.
Generated `AccountSchema.java` and `.class` files belong in separate build folders.

| Example | Schema | Config | Consumer |
| --- | --- | --- | --- |
| Round trip | [account.def](round_trip/account.def) | [java.ini](round_trip/java.ini) | [Main.java](round_trip/Main.java) |
| Serializer style | [account.def](coding_styles/serializer/account.def) | [java.ini](coding_styles/serializer/java.ini) | [Main.java](coding_styles/serializer/Main.java) |
| Google style | [account.def](coding_styles/google/account.def) | [java.ini](coding_styles/google/java.ini) | [Main.java](coding_styles/google/Main.java) |
| Oracle style | [account.def](coding_styles/oracle/account.def) | [java.ini](coding_styles/oracle/java.ini) | [Main.java](coding_styles/oracle/Main.java) |

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
