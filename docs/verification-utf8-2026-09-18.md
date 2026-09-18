# Binary UTF-8 verification - 2026-09-18

This follow-up measures the cost of strict C++ binary string validation, adds a
dedicated SIMD validator, and provides an explicit compile-time opt-out for
guaranteed-valid text. It builds on the
[cross-endian qualification](verification-cross-endian-2026-09-18.md).

## Implemented behavior

- `binary_text_validation::strict` remains the default in all native C++ binary
  key modes and both wire byte orders. Generated views remain strict.
- `binary_text_validation::unchecked` removes runtime UTF-8 scans in owning
  codecs, including dynamic names and nested collections. Stream rebinding
  preserves the policy. No runtime policy branch or message bytes are added.
- Constant generated names still validate at compile time. Bounds, resource
  budgets, non-text validity, and exact-message checks remain unchanged.
- AVX2 validates complete UTF-8 sequences in bounded 32-byte blocks, with an
  all-ASCII fast path. SSE2/word ASCII scanning and scalar Unicode validation
  provide the baseline. The existing SIMD build option controls acceleration,
  not validation policy. Unsupported CPUs do not execute AVX2 instructions.
- JSON and other language runtimes are unchanged. Valid UTF-8 remains the
  cross-language contract even when C++ checks are disabled.

## Correctness checks

- Windows x64 MSVC 19.51.36257.0 Release: all **220 core tests passed**.
- Dedicated tests compare baseline and dispatched validation against an
  independent code-point decoder: every byte pair at six vector positions,
  every legal Unicode scalar, exact-size unaligned input, every prefix of
  boundary sequences, mutations of valid text, and 10,000 deterministic random
  spans. Tests include overlong forms, surrogates, out-of-range values, embedded
  NULs, quotes, and control bytes.
- Clang ASan/UBSan under WSL: **11 focused tests passed with SIMD enabled and 11
  with explicit SIMD disabled**. The SIMD translation unit was instrumented
  separately with AVX2 enabled; baseline/dispatcher objects used baseline flags.
- The maintained CMake `runtime_simd_fuzz` target passed **100,000 seeded
  libFuzzer runs** under Clang 21 ASan/UBSan with SIMD enabled, seed `20260918`,
  and maximum input length 4,099 bytes (three control bytes plus 4,096 payload
  bytes). The corpus generator adds dense Unicode and truncated-vector seeds.
  All runtime SIMD translation units were instrumented by the normal fuzz build;
  mutation discoveries were kept separate from deterministic seeds. This bounded
  campaign is not an exhaustive or security qualification claim.
- Opt-out tests cover all three key modes and both endian choices, strict/unchecked
  equality for valid bytes, malformed text pass-through, retained bounds and
  string limits, invalid Boolean rejection, trailing-byte rejection, nested
  generated values, custom buffers, and standard stream adapters. JSON and view
  rejection remain active when owning output was produced unchecked.
- The current C/C++ runner, including both s390x big-endian participants, passed
  **192 exchanges** (4 producers x 4 consumers x 4 protocols x 3 variants),
  including shared frozen positional bytes. Native C used ASan/UBSan. The wider
  2,028-exchange matrix predates this follow-up; other runtimes were not modified.
- The rebuilt five-runtime C++/Java/JavaScript/Go/C# CTest interoperability matrix
  also passed, including the shared frozen positional bytes.

This is bounded verification, not proof of all possible input sequences or
qualification of every architecture. No ARM NEON implementation was added.

## Timing comparison

Host: Intel Core Ultra 9 185H, Windows 11 build 26200, MSVC 19.51.36257.0,
x64 C++20 Release, `/O2 /Ob2 /DNDEBUG /MD /EHsc /GL-`, SIMD enabled. Baseline is
revision `8a26aa35accb83c6b765456e23b992bb3dbd6fce`, before C++ binary UTF-8 checks.
Generated model headers and benchmark measurement helpers were identical.

The local benchmark reused `qualification/codec_benchmark.cpp`'s measurement
functions and separate observation translation unit, selecting positional binary
for the four existing workload shapes plus dense Unicode and control-heavy text.
All records were round-trip checked before timing. Each operation used 16 warmup
messages and nine samples of 2,000 messages. Seven runs of each executable
alternated baseline/SIMD/unchecked order on CPU affinity mask 1. Reported values
are medians of per-process medians, in nanoseconds per record.

| Payload | Operation with reused storage | No-validation baseline | Strict SIMD | Unchecked policy |
| --- | --- | ---: | ---: | ---: |
| Numeric-only, 25-byte record | Encode | 4.60 | 4.60 | 4.40 |
| Numeric-only, 25-byte record | Decode | 10.30 | 10.25 | 10.25 |
| 10-byte ASCII string | Encode | 27.30 | 30.05 | 26.65 |
| 10-byte ASCII string | Decode | 86.85 | 94.80 | 87.45 |
| 16,384-byte ASCII string | Encode | 115.35 | 306.20 | 112.35 |
| 16,384-byte ASCII string | Decode | 170.30 | 401.25 | 167.50 |
| 16,380-byte dense Unicode | Encode | 97.25 | 4,854.20 | 96.10 |
| 16,380-byte dense Unicode | Decode | 153.55 | 4,928.85 | 149.45 |
| 16,384-byte quote/control-heavy string | Encode | 95.40 | 288.65 | 96.35 |
| 16,384-byte quote/control-heavy string | Decode | 151.60 | 384.35 | 150.75 |

Unicode repeats two three-byte UTF-8 code points; control-heavy text repeats
newline, double quote, backslash, and NUL. These are stress distributions, not
representative claims about application data. Hot reused buffers make copying
particularly cheap. Small timing differences and collection-workload variation
do not establish speedups; other CPUs, cold buffers, and concurrency were not
measured. Mapped views and keyed-protocol timing were not included in this table.

For comparison, the preceding validator reused the JSON scanner and then checked
one Unicode sequence or special ASCII byte at a time. In the earlier seven-run
measurement, dense Unicode took 18,806.70/21,813.40 ns for encode/decode, and the
control-heavy case took 39,279.50/40,201.20 ns. The dedicated validator removes
that JSON-specific per-control-byte cost and vectorizes Unicode checks. Strict
validation is much faster than that implementation but is **not free**; the
unchecked option restores approximately the original string-copy cost.

## Allocation comparison

Separate `codec_allocation_profile` runs used 100 messages per sample. All 80
existing rows (four workloads, four protocols, five operation/storage cases)
matched the baseline exactly in encoded size, allocation count, requested
allocation bytes, peak new bytes, and peak temporary bytes. Instrumented timings
were not used above. This counts requested payload storage, not allocator
metadata, process RSS, or stack usage; the extra stress distributions were not
added to that allocation-profile run.

The local timing source, independent builds, raw CSV runs, summaries, and initial
comparison report are retained under `out/performance/utf8-validation/` (ignored
build output, not part of the distributed source). Build the maintained
qualification tools with `SERIALIZER_BUILD_BENCHMARKS=ON`; their unextended default
workloads remain documented in [qualification](../qualification/README.md#timing-and-allocations).
Policy examples and rebuild requirements are in
[usage](usage.md#choose-c-binary-text-validation).
