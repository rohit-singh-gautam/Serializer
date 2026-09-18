# Serializer examples

Each language has at least four runnable examples in its own folder:

- [C++](cpp/README.md)
- [Java](java/README.md)
- [JavaScript](javascript/README.md)
- [TypeScript](typescript/README.md)
- [Go](go/README.md)
- [C#](csharp/README.md)
- [Rust](rust/README.md)
- [Python](python/README.md)
- [Swift](swift/README.md)
- [Kotlin](kotlin/README.md)
- [C](c/README.md)

`basic`, `collections`, `complex`, and `interoperability` exercise all four native
protocols. The [complex contract](schemas/complex/model.serializer) spans 13
schema files with nested and diamond includes, reopened namespaces, inheritance,
arrays, maps, enums, and audit unions. The [shared fixtures](schemas/) are the
same for every language; the C++ compiler produces all selected outputs from
one entry schema. Generated artifacts stay in the build tree.

## Run

Build `serializer` in Release first, then select any installed target SDKs:

```sh
python example/run.py --compiler build/serializer --language go
python example/run.py --compiler build/serializer --language rust,python,c --example complex
python example/run.py --compiler build/serializer --language all
```

On Windows use `build/serializer.exe` and an MSVC developer environment for C/C++.
Use `--cpp-library` when the matching Serializer library is not beside the
compiler. The runner uses Release builds; pass the matching Release library.

SDKs: C++20 and C11 compilers, JDK 17+, Node.js with ES2020 BigInt, TypeScript,
Go 1.22+, .NET 8+, Rust 2021, Python 3.10+, Swift 6, Kotlin/JVM 2.0+.
Use `SERIALIZER_KOTLINC`, `SERIALIZER_TSC`, `SERIALIZER_JAVA`, etc. to override
SDK executable paths. The runner never downloads SDKs or silently skips one.
Only the selected SDKs are required; the C++ schema compiler needs none of them.
The TypeScript samples include narrow declarations for the Node APIs they use;
applications may use `@types/node` instead.

On Windows, `--wsl-languages c,rust,swift` explicitly builds and runs those targets
with installed WSL SDKs. `--sanitize` enables address/undefined sanitizers with
the GCC/Clang C toolchain. SDK and language baseline details are in the
[native language guide](../docs/native_languages.md).

Add `--big-endian` to `--example interoperability --language all` to include
QEMU s390x C/C++ producers and consumers using the same little-endian wire
profile. On Windows the cross compilers and QEMU run in WSL. See
[cross-endian requirements](interoperability/README.md#different-machine-byte-orders).
The positional fixtures are pinned to shared expected bytes in both matrices.

## CTest

```sh
cmake -S . -B build -DSERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release -L serializer_native --output-on-failure
```

These opt-in tests compile SDK consumers during CTest and run all 44 examples,
the 1,452-exchange matrix, and the five new runtime boundary suites. All SDKs
must be available. Set `SERIALIZER_EXAMPLE_WSL_LANGUAGES` and
`SERIALIZER_EXAMPLE_SANITIZERS` for the corresponding runner options.
Enable `SERIALIZER_EXAMPLE_BIG_ENDIAN=ON` to include the s390x participants.
The existing `SERIALIZER_BUILD_INTEROP_EXAMPLES` option retains the smaller
C++/Java/JS/Go/C# matrix and optional browser test.

## Additional C++ and Java examples


Each example lives in its own folder with its inputs and consumer sources.
Generated output stays in the build tree.

- [Configuration schema](config/README.md): a small C++ schema with inheritance.
- [C++ coding styles](coding_styles/README.md): nine independently selectable profiles.
- [Iostream examples](iostream/README.md): seven memory, file, buffered, and custom
  stream examples sharing a large 52-class telemetry schema.
- [Compression examples](compression/README.md): Zstandard, LZ4, gzip,
  zlib, raw DEFLATE, uncompressed output, and a custom backend with explicit limits.
- [Java examples](java/README.md): a runnable pure Java round trip and three style profiles.
- [Schema includes](includes/README.md): three paired C++/Java examples for shared
  types, reopened namespaces, and repeated/diamond includes.
- [Five-runtime interoperability subset](interoperability/README.md): one shared schema,
  independent C++/Java/JS/Go/C# producers and consumers, all 25 language pairs,
  four protocols, a browser example, and optional throughput measurements.
