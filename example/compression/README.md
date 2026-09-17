# Compression examples

Runnable C++20 examples cover every built-in compression format, explicit
uncompressed output, and the custom-backend interface. Each folder has a
`main.cpp` and an executable. All use the generated owning classes from
[shared/document.serializer](shared/document.serializer), with `binary_integer`
(little-endian) as the explicitly selected inner protocol.

| Folder | Executable / CTest name | Required CMake option | Demonstrates |
| --- | --- | --- | --- |
| [none](none/README.md) | `serializer_compression_none` | None | Member calls; identity output matches ordinary wire bytes |
| [zstd](zstd/README.md) | `serializer_compression_zstd` | `SERIALIZER_WITH_ZSTD=ON` | Zstandard frame; member output and fresh exact decoding |
| [lz4](lz4/README.md) | `serializer_compression_lz4` | `SERIALIZER_WITH_LZ4=ON` | LZ4 frame; generated static APIs |
| [gzip](gzip/README.md) | `serializer_compression_gzip` | `SERIALIZER_WITH_ZLIB=ON` | gzip member; member APIs with `std::stringstream` |
| [zlib](zlib/README.md) | `serializer_compression_zlib` | `SERIALIZER_WITH_ZLIB=ON` | zlib stream; static APIs with separate standard streams |
| [deflate](deflate/README.md) | `serializer_compression_deflate` | `SERIALIZER_WITH_ZLIB=ON` | Raw DEFLATE; standalone byte compression and exact message decoding |
| [custom](custom/README.md) | `serializer_compression_custom` | `SERIALIZER_WITH_ZLIB=ON` | Application-owned backend; free serialization helpers |

## Build and run

Run from the repository root with CMake 3.28+, a C++20 compiler/standard library,
and clang-format 19+. Make the selected dependencies available through
`CMAKE_PREFIX_PATH` or the matching vcpkg features described in the
[compression dependency guide](../../docs/compression.md#build-and-dependency-options).
For Ninja on Windows, use an initialized MSVC developer shell.

Enable all three dependencies to build all seven examples:

```sh
cmake -S . -B out/build/compression-examples -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON -DSERIALIZER_WITH_ZSTD=ON -DSERIALIZER_WITH_LZ4=ON -DSERIALIZER_WITH_ZLIB=ON
cmake --build out/build/compression-examples --config Debug --target serializer_compression_examples --parallel 4
ctest --test-dir out/build/compression-examples -C Debug -L serializer_compression_examples --output-on-failure
```

The standalone examples option needs no GoogleTest. With
`SERIALIZER_BUILD_TESTS=ON`, the regular test build also includes the examples
for enabled formats. Compression dependencies still default to OFF; CMake lists
the enabled examples and omits executables for disabled backends. Missing
dependencies explicitly enabled with `SERIALIZER_WITH_*` are configuration errors.
Set `CMAKE_BUILD_TYPE=Debug` at configuration time for a single-configuration
generator when a Debug build is desired.

For a dependency-free compression configuration, use a separate build directory:

```sh
cmake -S . -B out/build/compression-none -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON -DSERIALIZER_WITH_ZSTD=OFF -DSERIALIZER_WITH_LZ4=OFF -DSERIALIZER_WITH_ZLIB=OFF
cmake --build out/build/compression-none --config Debug --target serializer_compression_examples --parallel 4
ctest --test-dir out/build/compression-none -C Debug -L serializer_compression_examples --output-on-failure
```

This configuration builds and runs only `serializer_compression_none`. To run a
single enabled example, select its target and exact CTest name, for example:

```sh
cmake --build out/build/compression-examples --config Debug --target serializer_compression_gzip
ctest --test-dir out/build/compression-examples -C Debug -R "^serializer_compression_gzip$" -V
```

`-V` displays the verified serialized and transmitted byte counts. These counts
describe this fixture, not compression benchmarks or guaranteed size savings.
CMake generates `document.hpp` in the build tree through `serializer_generate`;
do not generate or check in a copy beside the source files.

## Shared message and limits

The deterministic document contains a title, revision, labels, and eight sections
with sixteen repetitive paragraphs each. Its serialized representation exceeds
64 KiB, so the examples exercise more than one compression staging chunk.
[Shared support](shared/example_support.cpp) compares every decoded field and
supplies explicit budgets: 1 MiB serialized/expanded data, 2 MiB compressed data,
and 1 MiB backend history, plus separate object decoding limits. Each executable
returns a nonzero status on failure and creates no external files.

Send exactly one frame/member per message and agree on the compression format,
schema, inner protocol, and byte order separately. These examples use bounded
whole-message staging, including for standard streams. Member decoding can
partially update fields on an inner parse failure; `deserialize_exact` returns a
fresh value only after complete validation. See the
[compression contract](../../docs/compression.md) for memory accounting, input
consumption, checksums, and supported standard-format profiles.

The custom example delegates to the built-in gzip backend to demonstrate the
extension interface. It is not an additional compression algorithm. Brotli, XZ,
bzip2, and other algorithms require application-provided adapters; they are not
bundled or demonstrated as implemented backends here.

## Verification: 2026-09-17

These examples were verified as working-tree additions to Serializer commit
`891b8b8f96242ded95d383b1bdff9c697eb481f0`. Both builds used Ninja, Debug,
`SERIALIZER_BUILD_TESTS=OFF`, and `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON`:

| Configuration | Result |
| --- | --- |
| Windows x64, MSVC 19.51; all three compression dependencies ON | 7/7 example CTests passed |
| Linux x86-64 in WSL Ubuntu, Clang 21; all three dependencies ON | 7/7 example CTests passed |
| Linux x86-64 in WSL Ubuntu, Clang 21; all three dependencies OFF | 1/1 example CTest passed; no GoogleTest or compression package needed |

The enabled builds used Zstandard 1.5.7, LZ4 1.10.0, and zlib 1.3.1. These runs
cover the examples' field comparisons and identity-byte check. No new sanitizer,
fuzzing, macOS, or ARM runs were performed for these examples. Earlier runtime
qualification is recorded separately in the
[compression verification record](../../docs/verification-compression-2026-09-17.md).
