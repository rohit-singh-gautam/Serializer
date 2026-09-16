# Codec validation tools

These targets are prepared for a later validation pass. They have not been
configured, built, or run as part of the assessment implementation.

## Regression tests

The ordinary `core_serializer_test` target includes `decoder_qualification_test.cpp`
and a generated `assessment.serializer` fixture. Added cases cover exact/truncated compact
integers, odd-offset scalar reads, float/double wire bytes, JSON grammar and Unicode,
replacement/failure behavior, cumulative limits, old/new schemas, enum fields and
collections, escaped field names, empty objects, and diagnostics. Existing tests
were adjusted for strict JSON and exact input views.
Existing wire snapshots remain the binary compatibility baseline.

Run the normal supported-platform build/test matrix when validation is authorized.
Also run optimized and address/undefined-sanitized builds, and C++20/C++23 builds
to cover the byte-swap fallback and standard-library paths.

### Schema scanner validation

`schema_scan_test.cpp` prepares checks for all byte values, unaligned starts,
exact-size input allocations, empty input, scalar/vector transitions, identifier
delimiters, qualified names, comment boundaries, and unterminated-comment cursors.
It exercises the baseline scanner, the CPU-selected scanner, and short-token dispatch.

When testing is authorized, run with `SERIALIZER_ENABLE_SIMD=ON` and `OFF`, on x64
with and without AVX2 available, and on a platform using the scalar fallback.
Include address-sanitized runs to catch reads beyond unpadded input. These cases
have not been compiled or executed. Measure parsing separately from C++ emission,
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
benchmarking; these cases have not been compiled or executed.

When validation is authorized, run these and the existing generated-object,
wire-format, view, and decoder-limit cases with SIMD enabled and disabled, with
and without AVX2 available, and under address/undefined sanitizers. Verify both
source-dependency and installed-library consumers, including pre-generated headers.
Benchmark long/short strings, escape density, all JSON layouts, each binary key
mode, matching/opposite endian arrays, and small records. Keep schema-compiler
timings separate. Compilation and execution of these cases remain deferred; see
[runtime SIMD](../docs/runtime_simd.md) for implemented scope.

## Timing and allocations

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

Configure `SERIALIZER_BUILD_FUZZERS=ON` with Clang and a libFuzzer-capable toolchain
to build `codec_fuzz` with address and undefined-behavior sanitizers. It feeds exact
input ranges through generated records in all four modes, plus JSON strings,
floating values, and nested vectors. Each attempt uses small explicit byte,
string, collection, nesting, allocation, and work limits. Expected parse failures
are caught; sanitizer failures and unexpected exceptions remain visible.

Use an input corpus containing valid outputs, each truncated prefix, compact
integer boundaries, escaped Unicode, invalid UTF-8, unknown fields/discriminators,
and excessive length/count prefixes. See the official
[libFuzzer usage guide](https://llvm.org/docs/LibFuzzer.html) for corpus and run
options. No fuzz campaign or sanitizer result is claimed here.

## Measurement-dependent decisions

Keep these as experiments until the measured workloads justify a change:

- Direct-to-output integer formatting with exact-fit and custom-policy fallback.
- Replacing the public virtual stream hierarchy or templating every protocol on it.
- Map insertion hints based on validated ordering, or alternate container APIs.
- New binary type/length framing, ordinary-integer variable encoding, compression,
  or language backends. These require separate version/API contracts.

The current implementation keeps exact-length stack formatting, preserves ordered
maps and stream reservation overrides, and introduces no new binary wire framing.
