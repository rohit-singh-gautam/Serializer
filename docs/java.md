# Java output

Serializer generates self-contained **Java 17+** source with no JNI, native library,
reflection, third-party runtime, or formatter dependency. The C++ schema compiler
is used at build time only. Each output file contains one public outer class,
namespace containers, generated owning types, and private codec helpers.

Schema directives such as `include common.serializer;` load reusable declarations
into the entry schema's Java output. Reopened namespace blocks share one static
container; duplicate qualified types and namespace/type conflicts fail during
parsing. See [include rules](usage.md#share-declarations-with-includes) and the
[three paired C++/Java examples](../example/includes/README.md).

## Generate and run

From the repository root, with the built `serializer` executable on `PATH`:

```sh
mkdir -p build/java/classes
serializer --input example/java/round_trip/account.serializer --output build/java/AccountSchema.java --config example/java/round_trip/java.ini
javac --release 17 -encoding UTF-8 -d build/java/classes build/java/AccountSchema.java example/java/round_trip/Main.java
java -cp build/java/classes serializer.example.Main
```

In PowerShell, use `New-Item -ItemType Directory -Force build/java/classes` for the
first command. Generated files belong in the build tree. `AccountSchema.java`
determines the public outer class name, which must be a valid Java identifier.

```ini
[output]
language = java

[java]
coding_standard = google
naming = profile
package = serializer.example
```

`java.package` is optional; omitting it uses the unnamed package. The CLI accepts
`--language java`, `--java.coding_standard`, `--java.naming`, and `--java.package` overrides.
Precedence is defaults, config, then CLI. Output must end in `.java`.
Use `--java.package=` to clear a configured package. Schema files begin with
`serializer version 1;`. See [the CLI guide](command_line.md) for generating C++
and Java together with independent settings and output paths.

## API and storage

```java
AccountSchema.Demo.Account account = new AccountSchema.Demo.Account();
account.accountId = 42;
account.displayName = "Ada";
byte[] bytes = account.encode(AccountSchema.Protocol.BINARY_INTEGER);
AccountSchema.Demo.Account copy = AccountSchema.Demo.Account.decode(
    bytes, AccountSchema.Protocol.BINARY_INTEGER);
```

Protocols are `JSON`, `BINARY_NONE`, `BINARY_INTEGER`, and `BINARY_STRING`.
JSON is compact UTF-8. All binary scalar widths are explicitly little-endian;
compact prefixes retain Serializer's existing encoding. There is no protocol or
version header. Both endpoints must agree on the schema and protocol.

| Schema | Generated Java representation |
| --- | --- |
| `int8`, `uint8`, `char` | `byte` |
| `int16`, `uint16` | `short` |
| `int32`, `uint32` | `int` |
| `int64`, `uint64` | `long` |
| `float`, `double`, `bool` | `float`, `double`, `boolean` |
| `string` | `String`, encoded as strict UTF-8 |
| `enum` | Java enum with original wire names and numeric ordinals |
| `array T` | `T[]`, including primitive arrays |
| `map(K) T` | `NavigableMap<K, T>` initialized with compatible ordering |
| Parent classes | Composed `base0`, `base1`, ... fields in declaration order |
| Union `payload` | `payloadIndex` plus typed fields such as `payloadCount` |

Unsigned fields store their full bit pattern in the corresponding signed Java
primitive. For example, `uint64` maximum is `-1L`; use `Long.toUnsignedString` for
display. JSON writes the unsigned decimal value and checks ranges on input.
Generated arrays use primitive storage without boxing for binary decoding;
JSON arrays use a temporary list because their size is not prefixed. Maps box
primitive keys/values. Generated code directly accesses fields and uses switches
for keyed dispatch. No speedup or allocation parity with C++ is claimed.

Access modifiers apply to fields. Parent composition preserves serialization
order and keys, but is not Java subtype inheritance. Union indices are zero-based;
only the selected payload is serialized. Inactive payload fields retain their
values in memory. Java owning classes have mutable value fields and are not
thread-safe when mutated concurrently with encoding.

## Java coding styles

Java has multiple conventions, not one universal coding standard. Common public
guides include [Google Java Style](https://google.github.io/styleguide/javaguide.html)
and the historical [Oracle/Sun Code Conventions](https://www.oracle.com/java/technologies/javase/codeconventions-content.html).
Serializer supports these **presentation presets**, not complete guide compliance:

| `java.coding_standard` | Block indentation | Profile naming |
| --- | --- | --- |
| `serializer` (default) | 2 spaces | UpperCamelCase types/namespace containers, lowerCamelCase fields, UPPER_SNAKE_CASE enum constants |
| `google` | 2 spaces | Same Java naming |
| `oracle` | 4 spaces | Same Java naming |

Braces attach to their declarations in every preset. Generated documentation and
method names remain stable. These presets do not run Google Java Format,
Checkstyle, or a full line-wrapping/compliance pass. `naming = preserve` keeps
schema identifiers when they are valid Java names. Keywords and naming collisions
are rejected before output is written. Wire names, IDs, and enum spellings are
independent of the profile. Each [Java style example](../example/java/README.md)
has its own schema, config, and consumer.

## Defaults and unsupported schema features

Supported defaults are range-checked decimal numeric literals, booleans, quoted
strings with ordinary Java/C++ escapes, and declared enum constants. They are
emitted as literals, with no per-object parsing. Arbitrary C++ expressions,
collection/union defaults, and direct self-containing owning fields are rejected.

This backend supports owning classes only: `view`, `readonly`, `mutable`, and
`packed` output are not implemented for Java and are rejected. Map keys support
integral primitives, booleans, strings, and enums; object and floating-point map
keys are rejected. Preserve the generated map comparator when replacing a map.
String keys sort by UTF-8 bytes, and unsigned integer keys use unsigned ordering.
Java strings cannot represent arbitrary invalid UTF-8 bytes supported by the C++
binary string codec. JSON `char` is restricted to one ASCII byte. Null field values
have no wire representation. Big-endian output, borrowed views, explicit SIMD,
native acceleration, and configurable output-buffer reuse are not implemented.

## Decoding limits and errors

`decode` returns a fresh object only after consuming the entire message; trailing
JSON whitespace is permitted. Malformed input throws `IllegalArgumentException`.
Unknown keys, unknown enum/union values, noncanonical booleans, numeric overflow,
and malformed UTF-8/UTF-16 are rejected. Missing keyed fields retain defaults;
duplicate fields apply in input order, and duplicate map keys keep the last value.
Nested object fields merge repeated keyed occurrences within the fresh message.

Use the overload taking `Limits` to customize policy:

```java
var limits = new AccountSchema.Limits(1024 * 1024, 64 * 1024, 10000, 32);
var copy = AccountSchema.Demo.Account.decode(bytes,
    AccountSchema.Protocol.BINARY_INTEGER, limits);
```

Defaults are 64 MiB message bytes, 16 MiB per string, 1,000,000 cumulative
collection entries, and 64 object nesting levels. JSON strings are bounded by
both their escaped input length and decoded UTF-8 length. Limits bound input and
logical work, not exact JVM heap use. Encoding limits object nesting to 64 levels.
Input arrays and encoded objects must not be changed concurrently with a codec call.

## CMake and verification

Enable examples with `-DSERIALIZER_BUILD_JAVA_EXAMPLES=ON`; CMake locates JDK 17+
and compiles with `--release 17 -encoding UTF-8 -Xlint:all -Werror`. This option is
off by default so C++ consumers do not require a JDK. When tests are also enabled,
CTest runs the Java examples and two-way C++/Java interoperability tests, including
exact binary byte comparisons, primitive boundaries, malformed input, and limits.

For consumers using CMake:

```cmake
serializer_generate_java(TARGET account_java
  SCHEMA schemas/account.serializer
  OUTPUT generated/AccountSchema.java
  CONFIG java.ini)
```

Build `account_java` or aggregate `serializer_generated_java` to generate sources.
`SCHEMA`/`CONFIG` are source-relative; `OUTPUT` is build-relative. `GENERATOR` can
supply a host executable and `DEPENDS` adds dependencies. This helper generates
sources only; attach `javac`, Maven, or Gradle compilation in the consuming project.
Source and installed Serializer packages expose the helper. See the runnable
[example CMake file](../example/java/CMakeLists.txt) for compilation rules.

### Verification performed for this implementation

The current [2026-09-17 verification record](verification-2026-09-17.md) identifies
the tested source revision and configurations. The full Windows C++/Java suite
passes there, including two-way interoperability and exact binary byte comparisons.
The four failures listed below belong to the earlier implementation run and were
not reproduced in the current recorded run.

Historical implementation run:

On Windows with MSVC 19.51 and JDK 17.0.18, the compiler, all nine C++ style
examples, all four Java examples, and the Java qualification program built.
The eight output-options/Java-generator unit tests and 15 example/CLI/interoperability
CTest entries passed. The interoperability check verifies both directions for all
four protocols and exact binary bytes, including enum union payloads.

That earlier full C++ unit suite reported four failures:
`serialize_parser.identifier`, `serialize_parser.hierarchical_identifier`,
`serialize_parser.access_type`, and `binary_view.edits_preserve_the_encoded_layout`.
The parser failures concern empty-input exception types; the view test reports
an unsupported-type exception. That run was not a clean full-suite qualification.
No performance benchmark, GraalVM build, or non-Windows Java runtime test was run
in either verification; those checks remain outstanding.
