# C++ performance features

[Back to the project overview](../README.md)

These notes describe implemented optimizations, regeneration requirements, and verification limits.
They do not imply measured speedups where benchmarks remain outstanding.

## SIMD in the schema compiler

`SERIALIZER_ENABLE_SIMD=ON` is the build default. The `.serializer` parser scans
whitespace, comments, and identifier spans in blocks, then constructs each identifier
string once. On x64, the baseline scanner uses 16-byte SSE2 blocks; supported MSVC, GCC,
and Clang builds also include a separately compiled 32-byte AVX2 scanner selected after
CPU/OS checks. Short spans and other architectures use scalar scanning.

Vector loads stay within the input bounds and require no trailing padding.

This is an internal schema-compiler optimization. No `simd` schema keyword or
output-profile setting is needed. The same build option also controls the shared runtime
optimizations below; generated methods call these helpers normally. Use
`-DSERIALIZER_ENABLE_SIMD=OFF` when configuring a source build to disable the explicit
SIMD scanners. See [CMake integration](cmake_integration.md) for consumer configuration
and [qualification](../qualification/README.md#schema-scanner-validation) for the
boundary cases and [verification results](verification-2026-09-17.md).

Timing comparisons remain outstanding; no measured speedup is claimed.

## SIMD in runtime serialization

All C++ protocols use the shared runtime paths automatically:

- Compact and formatted JSON scan ordinary string spans with SSE2/AVX2 on supported
  x64 CPUs. UTF-8 validation and escaping rules remain unchanged. Escaped strings
  write directly into reserved stream storage, with a source snapshot only when
  expanding output overlaps its input. JSON input uses the same bounded scanners.
- Native JSON and ProtoJSON input scan long whitespace runs in SSE2/AVX2 blocks,
  advancing and charging the consumed run once. Short gaps and tails stay scalar.
  Only space, tab, line feed, and carriage return are accepted; input/work budgets
  bound every scan. See [JSON whitespace scanning](runtime_simd.md#simd-json-whitespace-scanning).
- Positional, integer-key, and string-key binary output write eligible contiguous
  integer and floating-point arrays in one payload reservation. Matching byte
  order uses a bulk copy; differing byte order uses SIMD swaps with scalar tails.
  The count prefix, keys, scalar bits, and wire bytes are unchanged.
- Binary input bulk-decodes the same numeric arrays after checking the complete
  payload and resource budgets. Matching byte order uses a copy; differing byte
  order uses SIMD swaps. Work charges and partial results on failure are preserved.
- Binary strings use a dedicated bounded UTF-8 validator: AVX2 checks complete
  Unicode sequences, while the baseline accelerates ASCII with SSE2/word scans
  and validates Unicode scalarly. Quotes, controls, and embedded NULs are valid
  ASCII here. See [binary text validation](runtime_simd.md#binary-utf-8-validation).
- Nested objects, maps, and unions use these paths for their contained strings
  and arrays. Individual scalars, compact integers, enums, and packed boolean
  vectors retain their existing scalar handling. JSON numeric formatting still uses
  scalar conversion. Binary views already copy their encoded bytes in bulk;
  individual view setters retain scalar updates.

Link consumers to `Serializer::runtime`, including users of pre-generated
headers. CPU dispatch and vector instructions live in compiled helpers, keeping ISA
flags out of consumer code. `SERIALIZER_ENABLE_SIMD=OFF` disables the explicit schema
and runtime SIMD backends; bulk array reads/writes and direct JSON output remain. Short
inputs and unsupported architectures use scalar fallbacks. No input/output padding is
required. See [runtime SIMD details](runtime_simd.md), including the prepared validation
matrix.

Rebuild the runtime library and consumers to use the whitespace scanner; schema headers
do not need regeneration. See the [verification record](verification-2026-09-17.md) for
unit and sanitizer/fuzz coverage. Other runtime benchmarks remain outstanding; binary
UTF-8 measurements are recorded in [UTF-8
verification](verification-utf8-2026-09-18.md).

## Reusing destination storage

C++ JSON and all three native binary codecs reuse eligible nested string/vector
buffers and map nodes when decoding into an existing collection. Regenerated
owning classes pass storage donors through typed field access, while each incoming
element still starts with fresh schema defaults. Collection replacement, duplicate
map keys, resource limits, and partial-failure behavior remain unchanged.

Reuse the destination across messages and create a fresh decoder for each message's
budget. No schema keyword or caller opt-in is needed. Java and the optional Protobuf
codecs retain their existing replacement paths. See [destination reuse](usage.md#reuse-destination-storage)
for limitations and an example. Focused tests pass in the configurations in the
[verification record](verification-2026-09-17.md); performance measurements
remain outstanding.

## Batching generated fixed-width fields

Regenerated C++ owning serializers group consecutive fixed-width scalar fields in
batches of up to 16. All native binary output modes reserve once per batch, then encode
each field separately, including existing IDs or names.

Positional binary input checks a complete batch's range, work budget, and Boolean values
together; if it cannot safely complete the batch, it uses the original scalar reads to
retain partial results and diagnostics. Keyed input keeps per-field dispatch.

No schema option is needed. Wire bytes, byte order, and object padding rules remain
unchanged. JSON, Protobuf, and custom protocols without batch hooks retain their
existing calls. A failed output reservation writes none of the current batch;
earlier output remains. See [field batching](usage.md#batch-generated-fixed-width-fields)
for boundaries and the [verification record](verification-2026-09-17.md)
for results. Performance measurements remain outstanding.

## Pre-encoding constant field names

Regenerated C++ owning serializers prepare constant JSON and string-key binary field
names at compile time. JSON copies a prequoted name without rescanning it for escaping;
string-key binary copies the compact length and name together. Fixed-field binary
batches use these same constants and a compile-time total size.

Parent names, renamed wire keys, and union alternative keys are included; JSON's fixed
map wrapper names are also pre-encoded by the runtime.

No schema option is needed. Wire bytes and formatting remain unchanged, and custom
protocols without the optional name hook still receive `std::string_view`. This trades
some compiler work and constant storage for less repeated encoding work; it does not
shrink messages or add storage to each object. See [constant field
names](usage.md#pre-encode-constant-field-names) for scope, failure behavior, and
examples. See the [verification record](verification-2026-09-17.md) for generation and
test results.

Performance measurements remain outstanding.

## Reducing repeated JSON string scans

C++ JSON string helpers retain escape boundaries from their validation pass.
Unescaped input copies directly into reusable destination storage; escaped input
and output copy known plain prefixes/suffixes without scanning them again. Only
the region between the first and last escapes needs further escape processing.
All UTF-8, escape, resource-limit, and output-reservation checks remain active.

This is automatic with the updated runtime headers and needs no regenerated schema
code or option. It works with SIMD enabled or disabled and adds no per-string
allocation for scan metadata. See [JSON scan reuse](usage.md#reduce-repeated-json-scans)
for scope and remaining passes. The [verification record](verification-2026-09-17.md)
separates passing unit tests and bounded sanitizer/fuzz runs from outstanding
platform checks and benchmarks.
