# Codec validation tools

These tools separate regression tests, performance measurements, and fuzzing.
The dated [verification record](../docs/verification-2026-09-17.md) ties executed
checks to a source revision and configurations, and lists outstanding work.

## Regression tests

The ordinary `core_serializer_test` target includes `decoder_qualification_test.cpp`
and a generated `assessment.serializer` fixture. Added cases cover exact/truncated compact
integers, odd-offset scalar reads, float/double wire bytes, JSON grammar and Unicode,
replacement/failure behavior, cumulative limits, old/new schemas, enum fields and
collections, escaped field names, empty objects, and diagnostics. Existing tests
were adjusted for strict JSON and exact input views.
Independent frozen legacy big-endian and current little-endian messages in
`wire_compatibility_test.cpp` pin all three native binary modes against explicit
protocol selection. Schema-language version 1 does not identify message byte order.

`stream_boundary_test.cpp` directly checks rejected compact output values and
exact-capacity single-byte writes. On Linux with GCC/Clang, the standalone
`stream_allocation_failure_test` uses ELF linker wrapping to fail malloc/realloc
deterministically: failed growth and replacement must preserve bytes, cursor,
capacity, and ownership, and recovery/move/destruction must release storage once.
No allocator hooks are added to the production runtime. This injection target is
not built on Windows or macOS; portable boundary tests remain in the core suite.

`schema_compatibility_test.cpp` and `serializer_compatibility_cli` exercise the
separate evolution checker and reservation policy. `deserialize_exact_test.cpp`
and the Protobuf stream tests cover exact fresh-value decoding, suffix rejection,
defaults, resource limits, and input/transport failures. The dated record includes
focused ASan/UBSan checks for these additions.

Run the normal supported-platform build/test matrix for the platforms being qualified.
Also run optimized and address/undefined-sanitized builds, and C++20/C++23 builds
to cover the byte-swap fallback and standard-library paths.

### Schema scanner validation

`schema_scan_test.cpp` prepares checks for all byte values, unaligned starts,
exact-size input allocations, empty input, scalar/vector transitions, identifier
delimiters, qualified names, comment boundaries, and unterminated-comment cursors.
It exercises the baseline scanner, the CPU-selected scanner, and short-token dispatch.

Extend validation with `SERIALIZER_ENABLE_SIMD=ON` and `OFF`, on x64
with and without AVX2 available, and on a platform using the scalar fallback.
Include address-sanitized runs to catch reads beyond unpadded input. See the
[verification record](../docs/verification-2026-09-17.md) for executed unit tests
and the limits of current sanitizer coverage. Measure parsing separately from C++ emission,
formatter startup, and file I/O before reporting an end-to-end generator speedup;
the existing codec benchmarks do not measure schema compilation.

### Runtime SIMD validation

`runtime_simd_test.cpp` prepares exhaustive byte classification for JSON scanners,
exact-size and unaligned buffers, byte-swap tails and in-place swaps, and scalar
reference comparisons for numeric arrays in all three binary key modes and both
byte orders. JSON cases cover compact and formatted output, controls/UTF-8, direct
escaping, source aliasing across growth, capacity failures, and reservation counts.
Float cases compare representation bits, including negative zero and NaN payloads.
Array input now reads scalar-encoded bytes from exact-size storage at multiple
offsets and checks destination capacity reuse. `binary_array_decode_test.cpp`
prepares every work-budget boundary around a numeric array, partial-failure
positions, truncation, input/allocation/collection/depth limits, cumulative
budgets across repeated reads, and boolean/enum validation fallbacks.

`json_whitespace_scan_test.cpp` prepares exhaustive whitespace classification,
stop positions across vector boundaries, exact-size and unaligned inputs,
scalar tails, generated/formatted JSON, invalid whitespace, and native
JSON/ProtoJSON byte/work limits with exact failure cursors. The Protobuf test
target adds nested ProtoJSON object/array/map gaps and transactional rejection
of invalid whitespace. Include empty, compact, and heavily indented JSON when
benchmarking. Unit tests and sanitizer/fuzz results are recorded separately in
[verification](../docs/verification-2026-09-17.md).

For broader qualification, run these and the existing generated-object,
wire-format, view, and decoder-limit cases with SIMD enabled and disabled, with
and without AVX2 available, and under address/undefined sanitizers. Verify both
source-dependency and installed-library consumers, including pre-generated headers.
Benchmark long/short strings, escape density, all JSON layouts, each binary key
mode, matching/opposite endian arrays, and small records. Keep schema-compiler
timings separate. Benchmarks and unrecorded platform configurations remain outstanding; see
[runtime SIMD](../docs/runtime_simd.md) for implemented scope.

## Timing and allocations

`utf8_validation_test.cpp` compares baseline and CPU-selected UTF-8 validation
against an independent code-point decoder. It covers all byte pairs at vector
boundaries, every Unicode scalar, unaligned exact-size buffers, truncation,
invalid ranges, mutations, and deterministic random spans. `binary_text_test.cpp`
covers strict and unchecked owning policies, nested/custom/standard streams,
unchanged limits, and strict view mapping/mutation. See
[UTF-8 verification](../docs/verification-utf8-2026-09-18.md) for executed tests,
sanitizer scope, and before/after timing; other unrecorded benchmarks remain open.
The runtime SIMD fuzzer compares both UTF-8 backends with the existing scalar
sequence checker; deterministic seeds include dense Unicode and incomplete tails.

Configure `SERIALIZER_BUILD_BENCHMARKS=ON` to add:

- `codec_benchmark`: codec timing with no allocation interception.
- `codec_allocation_profile`: a separate executable that intercepts C++ allocation
  and counts malloc-backed stream construction/growth. Its timing includes profiler
  overhead and must not be substituted for `codec_benchmark` timings.

Both accept an optional positive `iterations_per_sample` argument (default 100),
perform 16 warmup messages, and report nine samples. Each workload is round-trip
checked before timing. Generated records cover scalars, short/long strings,
escaped text, integer/floating arrays, objects, maps, nested objects, and a union.
All four protocols are measured for encoding and decoding separately. Encoding
covers fresh fitted buffers, fresh forced growth, and fitted reuse; decoding covers
fresh and reused destinations. Input views use actual encoded size.

CSV output contains encoded bytes, minimum/median/maximum nanoseconds per message,
messages/second, allocation calls/bytes per message, and incremental peak storage.
The allocation columns are zero in the timing-only executable. Throughput in
bytes/second, if calculated, must use encoded bytes for the selected protocol.

Allocation metrics count requested payload storage, not allocator headers, size
classes, or process RSS. Peaks exclude preexisting source and reusable buffers.
`peak_temporary_bytes` is peak new storage minus storage still retained at sample
completion. Moving `realloc` is conservatively accounted as overlapping old/new
buffers even if an allocator grows in place. The profiler is for these single-thread
workloads; it does not intercept arbitrary `malloc` calls in user code or dependencies.

Record CPU/architecture, OS, compiler, standard library, language mode, configuration,
optimization flags, stream type, and payload shape alongside results. The executable
prints compiler/configuration and pointer width; full build flags and host details
remain the operator's responsibility. Use optimized builds, retain all samples,
and keep I/O and code generation outside codec timing. LTO is disabled for these
targets to keep the separate observation function opaque to the optimizer.

## Fuzzing

Use Clang's GNU-style driver with libFuzzer, AddressSanitizer, and
UndefinedBehaviorSanitizer runtimes, CMake 3.28+, and clang-format 19+. Configuration
checks that all three runtimes link. Clang-cl/MSVC are not supported by these targets.
GoogleTest, Java, and the external Protobuf runtime are not required.

`serializer_fuzz_runtime` recompiles the production library's complete source list
with `-fsanitize=fuzzer-no-link,address,undefined`. It retains the production
definitions and per-source AVX2 flags. Fuzz executables link only this instrumented
variant and add libFuzzer's main; the normal library, generator, benchmarks, and
installed package keep their existing flags. UBSan findings stop execution through
`-fno-sanitize-recover=all`. See [LLVM's libFuzzer guide](https://llvm.org/docs/LibFuzzer.html)
and [UBSan recovery controls](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html#usage).

| Target | Coverage |
| --- | --- |
| `codec_fuzz` | Generated JSON and all three native binary modes, both binary byte orders, compact integers, JSON strings/numbers/nested vectors |
| `view_fuzz` | Exact positional mapping, read-only access, nested/collection iterators, size-preserving mutation, and an owning-decoder cross-check |
| `protobuf_fuzz` | Generated Protobuf binary, ProtoJSON, and TextProto readers, including unknown fields and malformed lengths/tags |
| `runtime_simd_fuzz` | Scalar reference versus compiled baseline and CPU-dispatched string/whitespace scans, plus 2/4/8-byte disjoint and in-place swaps |

Every file has three harness control bytes followed by at most 4096 payload bytes:

1. Protocol selector: `codec_fuzz` uses 0=JSON record, 1/2/3=positional/integer-key/
   string-key little-endian record, 4=JSON string, 5=JSON double, 6=JSON nested vectors,
   7=compact integer, and 8/9/10=the three big-endian record modes.
   `protobuf_fuzz` uses 0=binary, 1=ProtoJSON, 2=TextProto. `runtime_simd_fuzz` uses
   0/1/2 for 2/4/8-byte swaps and always checks all three scanners. Views ignore it.
2. Budget selector modulo seven: baseline or a restrictive input-byte, string-byte,
   collection-count, depth, allocation, or work budget. The SIMD target ignores it.
3. Starting offset modulo 32: the payload is copied into an allocation ending exactly
   at the payload boundary, allowing ASan to check unpadded tails at odd alignments.

The baseline budgets are 4096 input bytes, 1024 bytes per string, 64 collection
elements, depth 8, 16384 accounted allocation bytes, and 8192 work units. Restrictive
variants reduce those respectively to 16, 16, 2, 1, 32, or 32. Expected parse errors
are caught; unexpected failures escape. Successful view mapping must support its
documented getters/setters without suppressing subsequent exceptions.

From the repository root on Linux (including WSL), configure and replay seeds:

```sh
cmake -S . -B out/build/fuzz-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_FUZZERS=ON
cmake --build out/build/fuzz-clang --parallel 4
ctest --test-dir out/build/fuzz-clang -L serializer_fuzz --output-on-failure
```

`fuzz_corpus_generator` writes named deterministic seeds under
`<build>/qualification/corpus/seeds/<target>`. It uses actual generated encoders,
checks full record seeds decode, and includes every strict prefix of those records,
compact-width boundaries, six restrictive budgets, malformed fields/UTF-8/escapes,
and scalar/SSE2/AVX2 transition sizes. The `serializer_fuzz_corpus` build target also
runs generation. Use a fresh output directory for a pristine corpus after changing
the generator; it overwrites its named seeds but does not delete other files.

CTest's fixture first generates seeds, then each target replays its corpus with
`-runs=0`. For a bounded mutation campaign, keep discoveries and crash artifacts
separate from the deterministic seeds:

```sh
build=out/build/fuzz-clang
for target in codec_fuzz view_fuzz protobuf_fuzz runtime_simd_fuzz; do
  mkdir -p "$build/qualification/corpus/discoveries/$target" "$build/qualification/artifacts/$target"
  "$build/qualification/$target" -seed=12345 -runs=20000 -max_len=4099 \
    -timeout=10 -rss_limit_mb=2048 \
    -artifact_prefix="$build/qualification/artifacts/$target/" \
    "$build/qualification/corpus/discoveries/$target" \
    "$build/qualification/corpus/seeds/$target" || exit 1
done
```

Repeat with `-DSERIALIZER_ENABLE_SIMD=OFF` in a separate build directory. A fixed
random seed and pristine initial corpus make a run reproducible for the same
toolchain/build; compiler, platform, or corpus changes can alter mutations. Save
commands, tool versions, flags, exit status, and final libFuzzer statistics. These
bounded runs do not establish exhaustive coverage or security qualification.

The [2026-09-17 record](../docs/verification-2026-09-17.md) includes corpus counts
and hashes, the bounded campaigns performed, and compile-command checks proving
that runtime/SIMD objects carry the instrumentation.

## Measurement-dependent decisions

Optional compression backends add `compression_fuzz` and generated seeds when any
`SERIALIZER_WITH_ZSTD`, `SERIALIZER_WITH_LZ4`, or `SERIALIZER_WITH_ZLIB` option is ON.
Its selector chooses Zstandard/LZ4/gzip/zlib/raw DEFLATE modulo five. Policies 1/2/3/4
restrict compressed bytes, expanded bytes, window bytes, and zero-byte output;
policies 5/6 retain the inner allocation/work restrictions. Successful frames are
also parsed with the generated positional codec. Seeds include strict prefixes,
trailing data, concatenated frames, corrupted bytes, empty frames, and expansion
limits. Add this target to the campaign loop above. See
[compression verification](../docs/verification-compression-2026-09-17.md).

The fuzz runtime recompiles Serializer's compression adapters with instrumentation
and links the enabled dependencies. To instrument library internals too, build
zstd/LZ4/zlib themselves with Clang's
`-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all
-fno-omit-frame-pointer`, and use those installations in a separate sanitizer
build. Link ordinary consumers in that diagnostic build with ASan/UBSan too;
prebuilt dependencies do not acquire instrumentation from Serializer's options.
The named dependencies/flags and actual campaign results must be recorded.

Keep these as experiments until the measured workloads justify a change:

- Direct-to-output integer formatting with exact-fit and custom-policy fallback.
- Replacing the public virtual stream hierarchy or templating every protocol on it.
- Map insertion hints based on validated ordering, or alternate container APIs.
- New binary type/length framing, ordinary-integer variable encoding, or language
  backends require separate version/API contracts. Compression backends are now
  optional; choosing algorithms/levels/thresholds still requires workload benchmarks.

The current implementation keeps exact-length stack formatting, preserves ordered
maps and stream reservation overrides, and introduces no new binary wire framing.
