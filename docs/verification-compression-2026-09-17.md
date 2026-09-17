# Compression verification — 2026-09-17

This record covers the optional C++ message-compression implementation described
in [compression](compression.md). Earlier codec/schema results remain in the
[previous verification record](verification-2026-09-17.md).

## Source identity

The base is commit `4475652f666635c0fde504f5f66830abb897ef94`
(`Add schema compatibility checks and exact decoding`), plus these 18 source,
build, and test files, still uncommitted when tested:

```text
CMakeLists.txt
cmake/serializer_compression.cmake
cmake/serializer_config.cmake.in
cmake/serializer_fuzz_runtime.cmake
include/rohit/compression.hpp
include/rohit/serializer.hpp
qualification/CMakeLists.txt
qualification/compression_fuzz.cpp
qualification/fuzz_corpus_generator.cpp
src/compression.cpp
src/cpp_writer.cpp
test/CMakeLists.txt
test/compression_allocation_failure_test.cpp
test/compression_interop.cpp
test/compression_interop_test.py
test/compression_test.cpp
test/protobuf_stream_test.cpp
vcpkg.json
```

Combined SHA-256:
`acbe7c77c2066405ce83b2810556f52b76515d406dd4a370b05646a02329b2f9`.
Sort the relative paths above, then hash each UTF-8 path, one NUL, and the file
content with CRLF normalized to LF. Accompanying documentation changes do not
alter the tested implementation. Generated headers came from the actual compiler.

## Configurations and results

Both hosts were x64 with SIMD ON. Optional Protobuf codecs were generated; the
official Protobuf interoperability option and Java builds were OFF for this work.

| Configuration | Result |
| --- | --- |
| Ubuntu 26.04.1 under WSL2, Clang 21.1.8, libstdc++ 15, C++20, Ninja, RelWithDebInfo; all three compression dependencies ON; tests/fuzzers ON | 38/38 CTest entries passed |
| Same host, Debug with `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`; compression dependency C sources also rebuilt with sanitizer/coverage instrumentation | 38/38 CTest entries passed; no sanitizer findings |
| Same host, Release; all compression dependencies OFF, fuzzers OFF | 27/27 CTest entries passed |
| Windows x64, MSVC 19.51.36257, MSVC STL, C++20, Ninja, Debug; all compression dependencies ON; fuzzers OFF | 29/29 CTest entries passed |
| Installed Linux package plus separate Release consumer | Dependency discovery, schema generation, compilation, and Zstandard/LZ4/gzip message round-trips passed |

Linux used CMake 4.2.3, GoogleTest 1.17.0, and clang-format 19. Windows used CMake
4.4.2 and GoogleTest 1.15.2. Backends were upstream **Zstandard 1.5.7**, **LZ4 1.10.0**,
and **zlib 1.3.1**, built from release-tag source archives. Their archive SHA-256s:

| Backend | SHA-256 |
| --- | --- |
| Zstandard | `37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3` |
| LZ4 | `537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b` |
| zlib | `17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c` |

Linux checked both directions against standalone zstd/LZ4 tools and Python's
gzip/zlib implementation for all five formats: 40 interoperability checks across
empty, small, compressible, and 200,000-byte high-entropy inputs. Windows ran the
three Python-backed format checks; frozen independent Zstandard/LZ4 fixtures and
generated message tests ran on both hosts.

The isolated allocation test injects failures into C++ scratch/output allocations,
checks release of every tracked allocation, and retries successfully. Linux
passed 34 injected failures, including the ASan/UBSan configuration; Windows
passed 44. Backend C allocator failures are not deterministically injected by this test. Windows uses
a separate compilation of `compression.cpp` and the test with
`_ITERATOR_DEBUG_LEVEL=0`: MSVC's debug iterator proxies allocate inside `noexcept`
constructors, making those particular bookkeeping failures nonrecoverable. Normal
runtime/core tests retain their original Debug iterator settings and ABI.

## Coverage and bounded fuzzing

Regressions cover all seven C++ message codecs, regenerated member/static APIs,
the fresh exact helper, buffer/standard-stream inputs, decompressed view mapping
and edits, custom backends, and unchanged uncompressed bytes. Compression tests
exercise zero-length frames, 64 KiB boundaries, multiple chunks, every strict
prefix of representative frames, checksum corruption, unaligned input, extra
bytes, concatenated streams, resource limits, disabled formats, invalid options,
and preserved output/destination state on relevant failures.

`compression_fuzz` uses **898 deterministic seeds**, SHA-256
`9612c8e09b7b00a45ac86a5ca2f32ed68d33b79c2781226bee692b7439bdf60b`.
The digest hashes sorted file names followed by a NUL and each file's bytes.
Generation also preserves the established native/view/Protobuf/SIMD corpora.
The new seeds include valid/empty frames, strict prefixes, independent resource
limits, odd alignments, corruption, concatenation, and expansion limits.

Compile-command inspection confirmed all **14** fuzz-runtime translation units,
including `compression.cpp` and SIMD helpers, use
`-fsanitize=fuzzer-no-link,address,undefined`. The diagnostic dependency builds
used that flag on their C sources too, with non-recovering errors and frame
pointers. The diagnostic build deliberately instruments the ordinary runtime,
generator, and test objects with ASan/UBSan as well. This does not change the
default production build's flags. External GoogleTest binaries and upstream
assembly routines were not independently sanitizer-instrumented.

The bounded compression campaign used a separate empty discoveries directory and:

```text
-seed=12345 -runs=50000 -max_len=4099 -timeout=10 -rss_limit_mb=2048 -print_final_stats=1
```

It completed **50,000 reported executions**, exit status 0, with no ASan/UBSan
findings. Executions include initialization; this is not a claim of 50,000 novel
mutations. Seed bytes are reproducible with the identified dependency versions;
mutations can differ with toolchain, instrumentation, platform, and corpus.

## Reproduction and remaining work

Configure the selected dependency install prefix, GoogleTest, and optional tools:

```sh
cmake -S . -B build/compression -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++-21 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="<compression-prefix>;<gtest-prefix>" \
  -DSERIALIZER_WITH_ZSTD=ON -DSERIALIZER_WITH_LZ4=ON -DSERIALIZER_WITH_ZLIB=ON \
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_FUZZERS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/compression --parallel 4
ctest --test-dir build/compression --output-on-failure
```

Python 3.9+ enables standalone interoperability checks. Put zstd/LZ4 executables
on PATH or supply `serializer_zstd_program`/`serializer_lz4_program`; otherwise
their external-tool tests are omitted, while frozen-fixture unit tests remain.
Use separate directories and rebuilt dependencies for sanitizer qualification.
The [fuzz workflow](../qualification/README.md#fuzzing) explains corpus isolation;
include `compression_fuzz` when compression backends are enabled.

No performance, compression-ratio, or allocation benchmarks were performed.
Longer campaigns, larger fuzz payloads, C++23, GCC, native big-endian, ARM, macOS,
WebAssembly, Java compression interoperability, Windows sanitizers, and dictionary
or multi-member handling are not established by this record. No complete release
or security qualification is claimed. The implementation deliberately stages
bounded complete messages; incremental decoding and additional bundled algorithms
remain separate future work.
