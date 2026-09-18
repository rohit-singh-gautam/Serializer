# Any-to-any interoperability

The C++ compiler generates C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python,
Swift, Kotlin, and C output from [one schema](message.serializer), which includes
[common types](common.serializer). The handwritten consumers live under
`example/<language>/interoperability`; shared schemas remain here.

```sh
python example/run.py --compiler build/serializer --example interoperability --language all
```

See [runner options and SDK setup](../README.md). The full suite runs all producers
before any consumer. It checks ten native runtimes plus a TypeScript consumer:
**11 × 11 × 4 protocols × 3 union variants = 1,452 exchanges**. TypeScript uses
the JS runtime with generated declarations and is also compiled in strict mode.
The fixture directory contains a `producers.txt` manifest, so a selected subset
checks precisely the participating languages.

Every consumer independently constructs the same values, writes all four native
protocols, and reads output from every producer. The fixture covers signed and
unsigned boundaries through 64 bits, floats and negative zero, booleans,
characters, Unicode, defaults, parent composition, nested objects, arrays, maps
with several key types, enum collection contexts, explicit wire-name overrides,
multi-byte IDs, and every union alternative.

Every positional producer is also compared against the shared frozen
[expected bytes](../../test/positional_binary_fixture.json). The prefix covers
the fields preceding `payload`, and the three suffixes cover the union tag and
active payload. These bytes were specified with explicit little-endian scalar
representations and compact lengths, independently of generated codecs. Update
them only for an intentional fixture/schema change, never automatically from a
producer's output.

## Different machine byte orders

The configured wire byte order must match even when the machines have different
native byte orders. All generated language runtimes use little-endian wire data;
C++ additionally supports explicit big-endian wire selection. A big-endian host
does not imply a big-endian message.

Add two actual big-endian target builds to the existing matrix:

```sh
python example/run.py --compiler build/serializer --example interoperability --language all --big-endian
```

This requires `s390x-linux-gnu-g++`, `s390x-linux-gnu-gcc`, and `qemu-s390x` on
Linux (Ubuntu packages `g++-s390x-linux-gnu` and `qemu-user`). On Windows these
tools run through WSL; the normal languages retain their configured native/WSL
SDK selection. The s390x binaries are statically linked and compiled with
assertions requiring a big-endian host target. C++ uses its scalar backend on
s390x. Both cross-built consumers use the same generated schemas and handwritten
fixtures as their native counterparts.

The extended matrix has **13 participants x 13 participants x 4 protocols x 3
variants = 2,028 exchanges**, including 507 positional checks. Little-endian
native producers and big-endian target producers decode each other's output,
and every producer must match the frozen positional bytes. An unavailable
compiler/emulator is a failure when this option is requested, never a silent
skip. With CTest, enable both `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON` and
`SERIALIZER_EXAMPLE_BIG_ENDIAN=ON`.

This qualifies the exercised s390x C/C++ consumers under emulation; it does not
claim execution of every managed-language runtime on big-endian hardware.
See the [verification record](../../docs/verification-cross-endian-2026-09-18.md)
for the exercised configurations, results, and limits.

## Existing five-runtime CMake subset

Prerequisites: the normal C++ build tools, Java 17+, Node.js 22+, Go 1.22+, and the
.NET 8+ SDK with the net8.0 targeting pack. All generated files stay in the build
tree. The target SDKs are optional for ordinary Serializer builds.

```sh
cmake -S . -B build -DSERIALIZER_BUILD_INTEROP_EXAMPLES=ON
cmake --build build --target serializer_interop_examples --config Release
ctest --test-dir build -C Release -R serializer_all_language_interoperability --output-on-failure
```

The CTest first runs every producer in `emit` mode, then every consumer in
`verify` mode. In this subset, each consumer verifies all 60 messages. This exercises **300
producer/consumer exchanges**, including 240 between different languages.
Binary output is byte-identical; JSON can differ in escaping or numeric spelling.
Complete decoded data is checked through a canonical positional encoding of the
independently constructed expected value.

For a Ninja build, generated consumers live under
`build/example/interoperability/generated`. The C++ executable is
`build/example/interoperability/serializer_interop_cpp` (plus `.exe` on Windows).
Each consumer accepts `<fixtures-directory> emit|verify`:

```sh
node build/example/interoperability/generated/javascript/main.mjs build/example/interoperability/fixtures verify
java -cp build/example/interoperability/generated/java/classes Main build/example/interoperability/fixtures verify
build/example/interoperability/generated/go/interop_go build/example/interoperability/fixtures verify
dotnet build/example/interoperability/generated/csharp/bin/Release/net8.0/Interop.dll build/example/interoperability/fixtures verify
```

Run the full CTest once before invoking an individual consumer so fixtures from
all producers exist. Multi-configuration generators may put the C++ executable
under the selected configuration directory.

## Browser and TypeScript

Serve `build/example/interoperability` using a local static HTTP server and open
`generated/javascript/browser.html`. It exercises all four protocols and decodes
the C++ integer-key fixture. The generated JS uses browser APIs and has no
Node-specific dependency. A headless Chromium test is registered when Python
and a supported browser are discovered; it runs the fixture producer test first.

Normal tests with this option enabled also compile
`test/portable/consumer.mts` when `tsc` is discoverable. Set CMake's
`serializer_tsc` cache path to the executable if necessary. The declarations
enforce `bigint` for 64-bit fields and preserve enum/protocol literal types.

## Performance checks

The JS, Go, and C# consumers accept `benchmark` in place of `verify`. They warm up
their runtime, report encoded size, and measure 10,000 encode and decode calls
for each protocol. Run one process at a time on an otherwise idle machine.
These are illustrative local measurements, not a statistically controlled
cross-language ranking.

For Go allocation measurements, enable normal tests, build them, then run in
`build/test/portable/go`:

```sh
go test -run '^$' -bench BenchmarkNativeCodec -benchmem
```

See [portable language support](../../docs/portable_languages.md) for APIs,
limitations, resource policy, build integration, and the verification record.
