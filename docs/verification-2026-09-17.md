# Verification record — 2026-09-17

## Compatibility and exact-decoding follow-up

The subsequent compatibility-checker and exact-decoding changes were validated
against the same base commit, `36c887d871bef3bbce81d9dfdb3afc4c730933e2`, plus the
14 source/test files below. Their combined SHA-256, using the sorted-path/NUL/LF
content method described in the initial record, is
`2e4b14e4767e85cf24dc5b2d8e2143ef1ded6863b31a65fd668df8bdcb89a671`.
These changes were still uncommitted when tested.

```text
CMakeLists.txt
include/rohit/schema_compatibility.hpp
include/rohit/serializer.hpp
src/schema_compatibility.cpp
src/serializer.cpp
test/CMakeLists.txt
test/deserialize_exact_test.cpp
test/protobuf_stream_test.cpp
test/resources/wire_compatibility.serializer
test/schema_compatibility_cli.cmake
test/schema_compatibility_test.cpp
test/stream_allocation_failure_test.cpp
test/stream_boundary_test.cpp
test/wire_compatibility_test.cpp
```

Using the same Windows/MSVC Debug and Linux/Clang RelWithDebInfo toolchains and
SIMD ON configurations described below:

- **34/34 Windows CTest entries passed**, including Java interoperability.
- **31/31 Linux CTest entries passed**, including seed replay and allocation failures.
- **17 focused tests passed under ASan/UBSan**: schema comparison, exact native
  decoding, and optional Protobuf stream/helper tests. The test objects used
  Clang C++20, `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all
  -fno-omit-frame-pointer` and linked the fully instrumented runtime variant.
- The compatibility CLI suite passed with an ASan/UBSan-built compiler executable
  linked to that variant, including malformed/oversized policy files, reader
  directions, includes, prohibited generation options, and preserved output files.
- After moving compatibility-option validation ahead of generator configuration
  loading, the affected native/Java CLI checks were rerun and passed on both hosts
  (Java CLI on Windows only); the sanitized compatibility CLI also passed again.
- A separate Linux Release consumer found the installed CMake package, compiled
  and exercised both new public APIs, and ran the installed compatibility CLI.

No sanitizer findings occurred. This follow-up replays the deterministic fuzz
corpus but does not claim a new mutation campaign for the added features. The
160,000-execution campaign below belongs to the initial identified snapshot.
The full GoogleTest suite was not sanitizer-instrumented, and no new scalar-only,
C++23, non-x64, Windows-sanitizer, or benchmark qualification was performed.
The compatibility checker does not yet support ProtoJSON/TextProto comparison;
the exact helper was exercised with all seven C++ codecs. See
[schema evolution](schema_evolution.md) and [exact decoding](usage.md#decode-one-exact-message-into-a-fresh-value)
for feature scope and remaining semantic/framing limitations.

## Initial fuzzing, failure-path, and wire-fixture verification

The remaining sections record the earlier snapshot, before the compatibility
checker and exact helper were added. Their source hashes, test counts, and
outstanding checks apply to that initial snapshot.

### Source identity

These results use commit `36c887d871bef3bbce81d9dfdb3afc4c730933e2`
(`Add static serialize and deserialize APIs to generated owning classes`), plus
the following regression-test changes present at that initial verification:

- `test/CMakeLists.txt`
- `test/resources/wire_compatibility.serializer`
- `test/stream_allocation_failure_test.cpp`
- `test/stream_boundary_test.cpp`
- `test/wire_compatibility_test.cpp`

The tests were run from isolated source snapshots. The test changes were not yet
committed at verification time; this record does not assign them an invented
commit. Their combined SHA-256 is
`f6bc1e9b958afbdaf4bb4459cdc17001e0f1aa42d65cac677a664f3935f1d922`:
sort the five relative paths above, then hash each UTF-8 path, one NUL byte, and
its UTF-8 file content with newlines normalized to LF, in that order.
Documentation-only edits accompanying this record do not alter the tested code.

The instrumented runtime, four fuzz targets, and deterministic corpus generator
are already present in the identified base commit. Generated C++/Java output was
produced through the repository's CMake generation targets.

### Configurations and results

Both hosts used x64 on an Intel Core Ultra 9 185H with AVX2 available.

| Configuration | Result |
| --- | --- |
| Windows x64, MSVC 19.51.36257, MSVC STL, C++20, Ninja, Debug (`/Od /RTC1`, debug CRT), SIMD ON, tests ON, Java examples ON, fuzzers OFF | **33/33 CTest entries passed**, including 176 core cases, 13 Protobuf cases, nine C++ style examples, seven iostream examples, Java examples/CLI, includes, and two-way C++/Java interoperability |
| Ubuntu 26.04.1 under WSL2, Clang 21.1.8, libstdc++ 15, C++20, Ninja, RelWithDebInfo (`-O2 -g -DNDEBUG`), SIMD ON, tests ON, fuzzers ON, Java examples OFF | **30/30 CTest entries passed**, including the same 176 core/13 Protobuf cases, allocation-failure executable, and five fuzz corpus generation/replay entries |
| Same Linux compiler/configuration, SIMD OFF, tests OFF, fuzzers ON | **5/5 corpus generation/replay entries passed** |
| Four fuzz executables, separately for SIMD ON and OFF, fixed seed 12345, 20,000 reported executions per target/configuration | **160,000 reported executions completed**, all exit status 0, no ASan/UBSan findings |
| Standalone allocation-failure regression compiled with Clang `-O2 -g`, ASan/UBSan, and non-recovering sanitizer errors | **16 deterministic failure scenarios passed**, no sanitizer findings |

Windows used CMake 4.4.2, GoogleTest 1.15.2 from vcpkg, and Microsoft JDK
17.0.18; Java compilation used `--release 17 -encoding UTF-8 -Xlint:all -Werror`.
Linux used CMake 4.2.3, Ninja 1.13.2, clang-format 19, and GoogleTest 1.17.0
(upstream tag commit `52eb8108c5bdec04579160ae17225d66034bd723`).
External official-Protobuf interoperability and benchmark targets were OFF.

The four earlier failures listed in [the Java implementation history](java.md#verification-performed-for-this-implementation)
did not recur: `serialize_parser.identifier`, `serialize_parser.hierarchical_identifier`,
`serialize_parser.access_type`, and `binary_view.edits_preserve_the_encoded_layout`
all passed. Those historical failures are not current suite failures for this snapshot.

### What the added regressions establish

- Compact output rejects `-1`, `0x40000000`, and `UINT64_MAX` with
  `std::out_of_range`, preserving the existing output prefix, spare bytes, cursor,
  allocation address, and capacity. Exact-capacity byte and character appends
  succeed; the following byte is rejected without modifying adjacent guards.
- Both allocating stream policies retain ownership, bytes, capacity, and cursor
  after injected growth failures through reservation, byte append, increment,
  writable-range acquisition, aliased append, and compact output. Initial lazy
  allocation and replacement allocation also fail deterministically. Recovery,
  move, and destruction release the owned allocation exactly once. ELF linker
  wrapping isolates injection in a test executable, with no production hooks.
- Six literal wire fixtures independently specify the earlier big-endian and
  current little-endian contracts for positional, integer-key, and string-key
  binary. Decoders must produce known values and encoders must match frozen bytes.
  The fixture includes signed/unsigned scalars, float/double bits, an array,
  nested object, union, and multi-byte compact field ID. A separate case proves
  that the wrong endian selection can parse successfully with incorrect values.
  The schema-language version does not identify byte order or wire version.

### Fuzz corpus and instrumentation

Compile-command inspection confirmed `-fsanitize=fuzzer-no-link,address,undefined`
and `-fno-sanitize-recover=all` on **every** instrumented runtime translation unit:
12 with SIMD ON, including the AVX2 helpers, and 10 with SIMD OFF. Normal runtime
objects had no sanitizer flags. Fuzz targets link the instrumented runtime variant.
The generator executable used to compile schemas remains an ordinary host tool.

Both configurations generated the same **7,251** deterministic seed files.
Each digest below hashes sorted file names followed by a NUL and the file bytes.

| Target | Seeds | SHA-256 |
| --- | ---: | --- |
| `codec_fuzz` | 2,550 | `a9cf5480a12e78ebb3b0d954a21f33f2ba90654c93dadf88772b45f9b92f8422` |
| `view_fuzz` | 272 | `ab76f1c1034b7edd743acec0c50f9a40b907f20a64ce0012c6e869ec9d293a06` |
| `protobuf_fuzz` | 1,504 | `589993e216f5f9065e24fb6f7b7fce1f39ccbcc9eab7fcb207a074a621f516c8` |
| `runtime_simd_fuzz` | 2,925 | `58718e17249d6610e7540dad9680253f09d42115fbf12c401bacd53c17525363` |

Seeds cover complete messages and every strict prefix, compact boundaries,
malformed fields/UTF-8/escapes, all six resource-limit policies, unaligned starts,
and scalar/SSE2/AVX2 boundaries. View cases exercise validated access and
size-preserving edits; optional codecs cover Protobuf binary, ProtoJSON, and
TextProto. SIMD cases compare compiled helpers with scalar references using
allocations that end exactly at the payload boundary.

CTest replay used `-runs=0 -seed=12345 -max_len=4099`. Each subsequent bounded
campaign started with an empty discoveries directory and used:

```text
-seed=12345 -runs=20000 -max_len=4099 -timeout=10 -rss_limit_mb=2048 -print_final_stats=1
```

Seed files, discoveries, and crash artifacts occupied separate directories.
The reported 20,000 executions include corpus initialization; they are not a
claim of 20,000 novel mutations. See the [fuzz workflow](../qualification/README.md#fuzzing)
for complete commands. Reproducibility requires the same toolchain/build and
initial corpus; coverage-guided mutations can differ across configurations.

### Reproduce the regression builds

On Linux, with an installed GoogleTest package available to CMake:

```sh
cmake -S . -B build/verification -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++-21 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="<gtest-install-prefix>" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_FUZZERS=ON \
  -DSERIALIZER_BUILD_JAVA_EXAMPLES=OFF -DSERIALIZER_ENABLE_SIMD=ON
cmake --build build/verification --parallel 4
ctest --test-dir build/verification --output-on-failure
```

For the scalar fuzz build, use a separate directory with tests OFF, fuzzers ON,
and SIMD OFF. On Windows, use an x64 Visual Studio developer shell, Ninja, Debug,
tests ON, Java examples ON, SIMD ON, and fuzzers OFF. Set `JAVA_HOME` to JDK 17
and `GTest_DIR` to the installed package's `share/gtest`; ensure its matching
debug DLL directory is on `PATH` before CTest. The normal Windows build presets
are another supported dependency setup, but were not the isolated configuration
used for this record.

The additional sanitized allocation check needs no GoogleTest or generated header:

```sh
clang++-21 -std=c++20 -O2 -g -Iinclude \
  -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
  -fno-builtin-malloc -fno-builtin-realloc -fno-builtin-free \
  test/stream_allocation_failure_test.cpp \
  -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=free \
  -o build/verification/stream_allocation_failure_sanitized
build/verification/stream_allocation_failure_sanitized
```

### Still outstanding

- Longer or continuous fuzz campaigns, additional schemas, larger payloads beyond
  the harness's 4096-byte limit, and exhaustive coverage are not established.
- The full GoogleTest suite and schema compiler were not run under sanitizers;
  ASan/UBSan coverage here is the four fuzz targets, their instrumented runtime,
  corpus generator, and the standalone allocation-failure check. MSan/TSan and
  Windows sanitizer/fuzzer runs were not performed.
- The full scalar-only unit suite, C++23 configuration, native big-endian hosts,
  x64 without AVX2, macOS, ARM, Android, and WebAssembly were not checked here.
  Explicit big-endian fixtures on x64 do not qualify a big-endian host.
- Allocation-failure injection is Linux-only; Windows/macOS allocator interception
  was not implemented or verified. Windows did run the portable boundary tests.
- Official Protobuf runtime interoperability was not enabled. Java ran on Windows;
  the Linux Java build was not performed because only a JRE was available, and
  GraalVM was not checked.
- Performance/allocation benchmarks and installed-package validation were not
  performed for this snapshot. No measured speedup or complete release/security
  qualification is claimed.
