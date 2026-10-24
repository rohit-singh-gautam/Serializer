# Java output

Compiler/runtime **2.32.0** adds opt-in [caller-owned output windows](#caller-owned-output-windows)
for ordinary native codecs and readonly projections. Schema language remains
**1.23.0** and the minimum SDK remains Java 17.

Language `1.3.0` [digest fields](../schema/digest.md) provide variable and exact-length
byte collections, including named-algorithm lengths, in all four native codecs.
Compute hashes through the application's provider; generated Java does not
automatically compute digest fields or import a cryptographic dependency.

Serializer generates **Java 17+** source whose ordinary codecs use JDK APIs.
Optional Zstandard/LZ4 compression providers are loaded reflectively when selected;
generated source compiles without either provider jar. The C++ schema compiler
is used at build time only. Each output file contains one public outer class,
namespace containers, generated owning types, and private codec helpers.

Compiler/runtime **2.14.0** adds optional whole-message identity, gzip, zlib and
raw-DEFLATE helpers using `java.util.zip`, plus optional Zstandard and LZ4 frames.
Ordinary generated codec methods and
their wire bytes remain unchanged. See [compression](#whole-message-compression).

Schema directives such as `include common;` load reusable declarations
into the entry schema's Java output. Reopened namespace blocks share one static
container; duplicate qualified types and namespace/type conflicts fail during
parsing. See [include rules](../guides/usage.md#share-declarations-with-includes) and the
[three paired C++/Java examples](../../example/includes/README.md).

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
`serializer version 1;`. See [the CLI guide](../guides/command_line.md) for generating C++
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
Schema `char` retains Java's signed byte map ordering; C++ depends on compiler
char signedness, while other backends order these byte keys unsigned. High-byte
keys remain semantically interoperable but can produce differently ordered bytes.
Prefer `map(uint8)` for new portable byte-keyed contracts; see
[map ordering](../serialization/wire_format.md#map-ordering).
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
independent of the profile. Each [Java style example](../../example/java/README.md)
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
Binary strings require valid UTF-8 in both Java and C++; use `array uint8` for
arbitrary binary payloads. JSON `char` is restricted to one ASCII byte. Null field values
have no wire representation. Big-endian output, borrowed views, explicit SIMD,
and native acceleration are not implemented. Caller-owned output storage is an
explicit opt-in described below.

## Caller-owned output windows

Enable the generation option in an INI file:

```ini
[java]
output_buffer_reuse = true
```

The equivalent CLI override is `--java.output_buffer_reuse true`. The default is
`false`, preserving existing generated method sets and application-defined
overloads. Regeneration with the option adds:

```java
public int encode(Protocol protocol, byte[] destination, int offset, int length)
```

Owning codecs and readonly translator projections support all four enabled native
protocols. A `binary_none` generation profile retains its existing protocol gate.
The method writes directly into `[offset, offset + length)` and returns the byte
count actually written, without retaining the destination. For example, with an
application-owned `OutputStream output`:

```java
byte[] encodedStorage = new byte[64 * 1024];
int written = account.encode(AccountSchema.Protocol.BINARY_INTEGER,
    encodedStorage, 0, encodedStorage.length);
output.write(encodedStorage, 0, written);
```

The caller owns and keeps the array alive. Give the codec exclusive access to the
window for the entire call, avoid concurrent model mutation, and ensure the window
does not alias mutable model payloads. Each call has independent writer state;
the array can be reused immediately after the call returns. Read/send only the
returned byte count after success; an earlier longer message can remain beyond it.

Invalid ranges, including arithmetic overflow, reject with
`IndexOutOfBoundsException`. Null/range/protocol admission occurs before writes.
Insufficient capacity throws `IllegalArgumentException`; model/depth errors retain
their existing categories. These failures may leave partial bytes **inside** the
window. Bytes **outside** it are preserved, including during auto-size backpatches.
No successful byte count is returned on failure. An empty positional record may
successfully return zero for a zero-length window.

The existing `byte[] encode(Protocol)` returns independent exact-size storage.
The window overload reuses primary output storage and avoids the final owning
copy. Writer objects, UTF-8 temporaries, collection iteration and other codec work
can still allocate. This API does not provide a whole-process peak bound or an
allocation-free guarantee. XML/package/stream/channel APIs retain their separate
contracts. See the [Windows/Linux checks and scoped measurements](../verification/verification-java-output-buffer-2026-10-24.md).
The [runnable output-buffer example](../../example/java/output_buffer/README.md)
includes its schema, opt-in INI, strict Java consumer and standalone CMake build.

## Caller-owned NIO buffers

Compiler/runtime **2.33+** independently supports writable heap and direct
`java.nio.ByteBuffer` output. Enable the generation option:

```ini
[java]
nio_output_buffer_reuse = true
```

The CLI override is `--java.nio_output_buffer_reuse true`. Default `false`
preserves existing method sets and application-native overloads. It is independent
of `output_buffer_reuse`: enable either or both. Regeneration adds this overload
to owning codecs and readonly translator projections:

```java
public int encode(Protocol protocol, java.nio.ByteBuffer destination)
```

All four enabled native protocols retain their bytes. A restricted `binary_none`
profile keeps its existing protocol gate. The method writes into the buffer's
initial `[position(), limit())` window and returns the number of bytes written.
On success, it advances the caller's position by that count. Limit, capacity,
byte order and mark stay unchanged; wire byte order is independent of the
buffer's order. Heap/direct slices use their own logical window coordinates.

```java
java.nio.ByteBuffer storage = java.nio.ByteBuffer.allocateDirect(64 * 1024);
int written = account.encode(AccountSchema.Protocol.BINARY_INTEGER, storage);
storage.flip(); // The application can now drain the encoded bytes through its own channel policy.
```

Reuse storage by setting its position and limit before the next call. Grant
exclusive access to the buffer's cursor and window, keep it alive, avoid concurrent
model mutation, and ensure its underlying storage does not alias mutable model
payloads. The codec uses an independent private view and retains no buffer
reference after return.

Read-only buffers throw `ReadOnlyBufferException` before writes, even for an empty
record. Null arguments throw `NullPointerException`; disabled protocols retain
their existing error. Insufficient remaining capacity throws
`IllegalArgumentException`. Admission and model failures leave the caller's
position, limit, order and mark unchanged. They may leave partial bytes inside
the initial window; outside bytes are preserved, including during size
backpatches. No successful count is returned on failure. Empty positional records
may return zero with no remaining capacity on a writable buffer.

The existing owning and array-window APIs retain their behavior. This API reuses
primary storage and avoids an owning message array; writer/view/UTF-8 and other
temporaries can still allocate. It does not establish a whole-process peak bound,
allocation-free encoding, native/SIMD acceleration, direct codec streaming or a
channel API. See the [qualification and scoped measurements](../verification/verification-java-nio-output-2026-10-24.md)
and [installed-package example](../../example/java/output_buffer/README.md).

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

## ZIP and OPC packages

Compiler/runtime 2.14.0 adds Java 17 package facades for existing schema 1.20.0
typed XML and opaque part roles. The standard JDK implementation provides checked
ZIP32/ZIP64 reading, stored/raw-DEFLATE output, OPC metadata and Relationships
compatibility processing, extension policies, bounded source adapters and staged
atomic Save. See the [Java package guide](java_packages.md) for generated APIs,
ownership, resource accounting and the remaining shared-context/profile/reference
limits.

## Whole-message compression

Each generated file supplies a nested `SerializerCompression` helper. If that name
conflicts with the outer class or a schema declaration, the generator selects the
first unused `SerializerCompression_2`, `_3`, and so on. Its generated Javadoc
records the selected name; existing schema types retain their names.
When an output class itself is named `Format`, `Input`, `Output` or another nested
compression helper type, that helper type gains `_2` inside the compression
container, with an emitted naming comment. This preserves those output filenames.

```java
var encoding = new AccountSchema.SerializerCompression.EncodeOptions(
    AccountSchema.SerializerCompression.Format.GZIP);
byte[] compressed = AccountSchema.SerializerCompression.encode(
    () -> account.encode(AccountSchema.Protocol.BINARY_INTEGER), encoding);
var decoding = new AccountSchema.SerializerCompression.DecodeOptions(
    AccountSchema.SerializerCompression.Format.GZIP);
var restored = AccountSchema.SerializerCompression.decode(compressed, decoding,
    plain -> AccountSchema.Demo.Account.decode(plain,
        AccountSchema.Protocol.BINARY_INTEGER));
```

`compress(byte[], EncodeOptions)` and `decompress(byte[], DecodeOptions)` operate
on arbitrary complete codec bytes. The supplier/function adapters above validate
provider availability before encoding or input decoding, then return only a
completed result. The inner decoder retains its ordinary exact-message checks and
`Limits`; pass its explicit limits overload when needed. No compression envelope,
protocol autodetection or implicit compression is added. Readers select `NONE`,
`GZIP`, `ZLIB`, `DEFLATE`, `ZSTD` or `LZ4` explicitly. An unavailable optional
provider rejects before serializers, cancellation callbacks or borrowed I/O.

| Format | Application dependency | Default encoding profile |
| --- | --- | --- |
| `NONE`, `GZIP`, `ZLIB`, `DEFLATE` | JDK | Level six for DEFLATE-based formats |
| `ZSTD` | `com.github.luben:zstd-jni:1.5.7-16` qualified | Level three, content checksum enabled |
| `LZ4` | `at.yawk.lz4:lz4-java:1.12.0` qualified | Fast mode, 64 KiB independent blocks, content checksum enabled |

Pin the selected provider in the application and place its jar on the runtime
classpath. Zstandard also requires its matching native library; zstd-jni bundles
supported-platform libraries and loads them during provider admission. LZ4 uses
the provider's safe pure-Java block and XXHash APIs. The maintained LZ4 artifact
uses the `net.jpountz.*` package names; select one LZ4 artifact on the classpath.
See the [provider's installation guidance](https://github.com/yawkat/lz4-java#installation).
`available(format)` probes the selected optional deployment without invoking
application callbacks or streams; native provider loading can access its own
deployment files. It does not download or install dependencies.

The `Output` and `Input` wrappers borrow ordinary Java streams:

```java
try (var compressedOutput = new AccountSchema.SerializerCompression.Output(
    destination, encoding)) {
  compressedOutput.write(account.encode(AccountSchema.Protocol.JSON));
  compressedOutput.finish();
}
try (var compressedInput = new AccountSchema.SerializerCompression.Input(
    source, decoding)) {
  var copy = AccountSchema.Demo.Account.decode(compressedInput.readAllBytes(),
      AccountSchema.Protocol.JSON);
  compressedInput.finish();
}
```

Output stages one bounded complete message and requires explicit `finish()` to
compress and publish it. `flush()` does not finalize or flush the borrowed sink;
`close()` discards unfinished staging and never emits a trailer. Input reads its
bounded source through EOF and validates one complete frame before exposing
decompressed bytes. Its `finish()` rejects unread inner bytes. Neither wrapper
closes its underlying stream. Exceptions fence the affected wrapper, and later
reads/writes/finish calls fail. External sink failures can retain a partial prefix.
An EOF probe can consume one byte beyond a source's input limit before rejecting.

`EncodeOptions` defaults to 64 MiB each of serialized input and compressed output,
with the format-specific levels above. Its full constructor takes
`(format, level, maxInputBytes, maxOutputBytes, checkCancelled)`. An additive
overload appends `boolean checksum`; it selects Zstandard/LZ4 content checksums,
while other formats retain their mandatory framing checks. `DecodeOptions` defaults to 64 MiB each of
compressed input and expanded output and an 8 MiB history budget; its full
constructor takes `(format, maxInputBytes, maxOutputBytes, maxWindowBytes,
checkCancelled)`. DEFLATE-based formats require at least 32 KiB of history budget.
DEFLATE levels are 0 through 9. Zstandard validates its provider's full advertised
minimum/maximum, including negative fast levels, before callbacks or application
I/O. The selected LZ4 profile accepts level zero for fast mode and one through nine
for high compression. Both policies are immutable; no dictionary is supplied.

Gzip/zlib checksums, gzip optional headers/FHCRC and exact single-stream boundaries
are validated. Truncation, dictionaries, trailing bytes and concatenated streams
reject before inner decoding. Raw DEFLATE has no checksum and cannot detect every
corruption. Zstandard readers accept checksummed and unchecked standard frames,
reject nonzero dictionary IDs, and round the history allowance down to a power of
two. LZ4 readers support linked and independent standard blocks, validate descriptor,
block and content checksums when present, and admit the advertised maximum block
size against the history policy before decoding. Dictionary descriptors, legacy
and skippable frames are rejected. Logical bounds include expansion and framing overhead, but exclude
JVM/provider internals, spare buffer capacity and the ordinary encoder callback's
allocation. Both directions stage complete messages and may hold several bounded
byte arrays; this is not an incremental object protocol or a hard heap bound.

The optional `Runnable` cancellation hook runs around bounded provider/I/O calls,
including zero-output calls, and after the decoder callback before returning its
candidate. Application stream I/O, Zstandard provider calls and checksum updates
use batches of at most 32 KiB. LZ4 encoding uses at most 64 KiB per block-provider
call; decoding uses one advertised block of at most 4 MiB plus at most 64 KiB of
retained linked history. Parser loops also poll cancellation. A provider call
cannot be preempted. Cancellation retains its original exception.
Compression `Failure.code` is `UNAVAILABLE`, `INVALID_OPTIONS`, `INVALID_DATA` or
`RESOURCE_LIMIT`. Borrowed-stream I/O, allocation and inner-codec exceptions retain
their types; unexpected checked provider I/O uses `UncheckedIOException` with its
original cause. Unrelated provider runtime failures propagate separately.
Wrappers and application callbacks require caller-managed synchronization.

The [selected optional-provider proof](../verification/verification-java-compression-providers-2026-10-20.md)
records actual emitted Java 17 compilation, JDK 26 execution, present/absent SDKs,
native deployment failures, independent standard frames and direct C++ exchanges.
Other SDK/platform/provider versions and complete shared-operation budget profiles
remain separate qualification work.

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
[example CMake file](../../example/java/CMakeLists.txt) for compilation rules.

### Verification performed for this implementation

The current [2026-09-17 verification record](../verification/verification-2026-09-17.md) identifies
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

## Optional values

Language 1.7.0 and compiler/runtime 1.12.0 support `optional<T>`, including
nested presence, record payloads, optional arrays/maps, and collections of
optionals. See the [optional contract](../schema/optional.md) for the standard target
representations, exact JSON/binary framing, defaults, and limits. Runnable
[optional examples](../../example/README.md#optional-values) exercise every output.
