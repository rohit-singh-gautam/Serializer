# Multi-language verification — 2026-09-17

This record covers the working-tree JS/TypeScript, Go, and C# implementation on
base commit `685bc5f8feaea7ab4d99f1ae0e9aefb8419a0ae5`. The parser and all generators
remain C++20. See [portable language support](portable_languages.md) for the
supported owning subset and public APIs.

## Environments and correctness

Windows x64, Intel Core Ultra 9 185H:

- MSVC 19.51, Ninja, Release; compiler and C++ consumers use warnings as errors.
- Java 17.0.18, Node.js 22.23.1, Go 1.26.4, .NET SDK 8.0.425/10.0.401 with
  generated C# compiled for `net8.0` and warnings as errors.
- TypeScript 7.0.2; Chrome 152.0.7977.84 for the headless browser test.

Configured `out/build/multilang` with normal tests,
`SERIALIZER_BUILD_JAVA_EXAMPLES=ON`, and
`SERIALIZER_BUILD_INTEROP_EXAMPLES=ON`. Optional compression libraries were off.
GoogleTest was supplied from an existing local vcpkg installation; its runtime
DLL directory must be on `PATH` when running the tests.

All **43 CTest entries passed** across the full run and the final targeted rerun.
The first usable full run passed 42 entries; a new generator test used an
unsupported hyphenated schema wire name. Correcting that fixture and rerunning
the core executable passed all **205 unit tests**, including the five new
portable-writer tests. Earlier startup attempts without GoogleTest's DLL path
were stopped and are not counted as passing runs.

The checks include:

- One compiler invocation emits C++, Java, JS, TypeScript declarations, Go, and
  C# from the same entry schema and transitive include.
- **300 exchanges:** five producers × five consumers × four protocols × three
  union alternatives. Of these, 240 cross a language boundary. Each consumer
  constructs its expected value independently and checks complete decoded data;
  all binary encodings match byte for byte.
- Signed/unsigned extrema, exact 64-bit integers, negative zero, Unicode,
  escaped text, nested objects, parent composition, arrays, maps, enum contexts,
  explicit names/IDs, and each union alternative.
- New runtime rejection tests for truncation, trailing input, numeric range,
  invalid Unicode and booleans, resource limits, unknown keys/tags, and malformed
  JSON. Duplicate-field and collection replacement behavior is checked.
- TypeScript declaration checking and private-field visibility; browser
  round trips for all four protocols and decoding a C++-produced fixture.
- CLI language/config precedence, output-path validation, native naming
  collisions, and preserving existing output when batch generation fails.
- Existing C++/Java, CLI, schema compatibility, Protobuf, includes, coding-style,
  stream, and uncompressed-runtime tests.

The installed package was separately exercised with `find_package(Serializer
CONFIG REQUIRED)` in a `LANGUAGES NONE` consumer. Its
`serializer_generate_source` helper generated a JS module using the installed
C++ executable; Node verified full-width integer round trips in all four
protocols. No C++ application compiler or target SDK is required for generation.

Ubuntu under WSL, Linux amd64: the compiler built with GCC 15.2 and the repository
warnings-as-errors settings. Node 24.15.0 and Go 1.26.0 passed the same portable
runtime boundary/rejection tests against output from that Linux compiler.
Java and .NET were not installed in that Linux environment, so the complete
five-language matrix was qualified on Windows only.

## Local performance samples

The example benchmark warms up each runtime and times 10,000 encodes and
decodes per protocol. Processes were run sequentially. These are illustrative
single-run measurements, not statistically controlled cross-language rankings.
JIT tiering, garbage collection, process order, and host load can affect results.

| Runtime | Protocol | Bytes | Encode messages/s | Decode messages/s |
| --- | --- | ---: | ---: | ---: |
| JS | JSON | 962 | 39,601 | 13,347 |
| JS | BINARY_NONE | 319 | 78,521 | 212,789 |
| JS | BINARY_INTEGER | 365 | 80,300 | 302,883 |
| JS | BINARY_STRING | 564 | 72,884 | 93,316 |
| Go | JSON | 962 | 216,971 | 37,679 |
| Go | BINARY_NONE | 319 | 698,485 | 644,737 |
| Go | BINARY_INTEGER | 365 | 740,121 | 384,244 |
| Go | BINARY_STRING | 564 | 570,877 | 350,462 |
| C# | JSON | 1,002 | 82,162 | 14,268 |
| C# | BINARY_NONE | 319 | 185,411 | 286,666 |
| C# | BINARY_INTEGER | 365 | 183,955 | 255,969 |
| C# | BINARY_STRING | 564 | 244,688 | 179,642 |

JSON size differs because C# escapes additional characters; values remain equal.
The binary sizes are identical in all five languages.

For `BenchmarkNativeCodec`, which decodes a message containing 1,024 integers,
checked array-capacity reservation and primitive-path changes reduced Go
allocation from **13,760 bytes / 25 allocations** to **4,936 bytes / 15
allocations per decode**. The before/after timing environments differed, so a
speedup ratio is not claimed. Reproduce the final allocation measurement with:

```sh
cd out/build/multilang/test/portable/go
go test -run '^$' -bench BenchmarkNativeCodec -benchmem
```

The implementation also pre-encodes schema keys, dispatches fields directly,
uses native endian operations, and caches JS DataViews. It does not establish
performance parity with C++, or replace workload-specific throughput,
allocation, latency, and encoded-size benchmarks.

## Remaining qualification

macOS, ARM, older supported Go/.NET/browser baselines, independent fuzzing of
the new runtimes, and production-scale workloads were not qualified here.
Rust, Python, WebAssembly codecs, and React/Next.js-specific adapters are not
implemented. The JS module runs in both Node and browsers; framework integration
can consume its existing byte-oriented API.
