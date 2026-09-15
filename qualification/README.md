# Codec validation tools

These targets are prepared for a later validation pass. They have not been
configured, built, or run as part of the assessment implementation.

## Regression tests

The ordinary `core_serializer_test` target includes `decoder_qualification_test.cpp`
and a generated `assessment.def` fixture. Added cases cover exact/truncated compact
integers, odd-offset scalar reads, float/double wire bytes, JSON grammar and Unicode,
replacement/failure behavior, cumulative limits, old/new schemas, enum fields and
collections, escaped field names, empty objects, and diagnostics. Existing tests
were adjusted for strict JSON and exact input views.
Existing wire snapshots remain the binary compatibility baseline.

Run the normal supported-platform build/test matrix when validation is authorized.
Also run optimized and address/undefined-sanitized builds, and C++20/C++23 builds
to cover the byte-swap fallback and standard-library paths.

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
