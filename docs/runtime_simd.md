# SIMD in runtime serialization

`SERIALIZER_ENABLE_SIMD=ON` enables explicit SIMD in the schema compiler and
shared runtime library. Generated owning classes call the normal protocol API;
no schema keyword, class attribute, output profile, or generated intrinsics are
needed. Consumers link `Serializer::serializer_lib`, including when headers were
generated elsewhere.

## Protocol coverage

| Path | Implementation |
| --- | --- |
| Compact JSON and both formatted JSON modes | SIMD scans ordinary string spans; complete UTF-8 validation precedes writing. Quoted output is written directly after one reservation. |
| JSON input | The same bounded scans accelerate string validation and copying between escapes. Unicode and escape validation remain scalar at special bytes. |
| Positional, integer-key, and string-key binary output | Eligible numeric arrays use one reservation for their payload after the count prefix. Matching-endian payloads use a bulk copy; opposite-endian payloads use SIMD byte swapping. |
| Positional, integer-key, and string-key binary input | Eligible numeric arrays use a bulk copy or byte swap after validating the complete payload and resource budgets. The successful bulk path charges and advances the payload once. |
| Nested objects, maps, and unions | Contained strings and arrays use the same protocol helpers. Traversal, field identifiers, ordering, and discriminators remain unchanged. |
| Binary views | Existing positional bytes are copied as a complete message. Scalar setters still update individual fields. There is no additional field-encoding pass to vectorize. |

Numeric array eligibility requires contiguous `std::vector` storage with
padding-free integers of 1, 2, 4, or 8 bytes, or the supported 32-/64-bit binary
IEC 559 floating-point types. Byte copies and swaps preserve negative zero,
infinities, and NaN payload bits. No whole C++ class layout is copied.
`std::vector<bool>`, enums, strings, objects, and variable-width elements retain
element-wise encoding and decoding; nested numeric vectors still reach the bulk helper.

JSON number formatting, individual scalar fields, and compact binary prefixes
retain their scalar implementations. SIMD availability does not imply every
operation benefits from vector instructions.

## Bulk binary-array decoding

C++ binary input checks the declared count, destination capacity limit, allocation
budget, and complete payload range before changing the vector. Size multiplication
is bounded by the available input range. It reuses vector capacity, creates live
scalar elements with `resize`, and copies or byte-swaps their representations.
This removes per-element decoder calls, bounds checks, and cursor updates on the
successful bulk path. Standard `std::vector::resize` still initializes the elements
before their representations are overwritten; this is not an uninitialized-storage API.

Resource accounting remains identical to scalar decoding: declared collection
slots, decoded values, payload bytes, and the aggregate itself all retain their
charges. If the remaining work budget cannot cover the entire array, the decoder
uses the scalar loop to preserve the exact partial result, failure position, and
resource charges. Empty arrays clear the destination without reading a payload.
Input must remain alive and must not overlap destination storage that decoding
modifies or reallocates, as required by the existing decode contract.

## Dispatch and bounds

Supported x64 builds use SSE2 for 16-byte blocks and an isolated AVX2 backend for
32-byte blocks after CPU/OS feature checks. CPU selection is cached with
thread-safe initialization. AVX2 compiler flags apply only to the backend source
files, which are excluded from unity compilation. Short spans, vector tails, and
unsupported architectures have scalar paths; no ARM NEON backend is provided.

Vector loads and stores stay inside the provided ranges. No readable or writable
padding, special allocator, or vector alignment is required. Binary payload-size
multiplication and JSON escaped-size additions are checked before reserving the
payload. The binary count or earlier fields can already have been written when
a later reservation fails, as permitted by the existing output contract.

`stream::append_transformed(source, source_size, output_size, writer)` supports
direct output after one reservation. Its non-throwing callback must write exactly
`output_size` bytes and read only the supplied `source_size` bytes. Sources that
alias growable stream storage are rebased after reservation. When the output
overlaps its input, the helper snapshots the source before writing, so quotes or
expanding escapes cannot overwrite unread bytes. This rare alias case can allocate;
ordinary JSON output no longer creates a temporary escaped string.

## Configuration and validation

Set `-DSERIALIZER_ENABLE_SIMD=OFF` when building Serializer to disable its explicit
SIMD backends. Bulk numeric-array reads/writes and direct JSON output remain enabled;
ordinary library copies may still use optimized machine instructions. Installed
libraries and generators retain their build-time setting. See
[CMake integration](cmake_integration.md#schema-scanner-configuration).

`test/runtime_simd_test.cpp` prepares comparisons against scalar binary encoding
and an independent JSON escaping reference, plus byte-classification, exact-size
buffers, unaligned starts, scalar/vector tails, Unicode failures, overlapping
sources, and reservation-count checks. Existing generated-object and view tests
remain part of the validation matrix.
Binary arrays are decoded from scalar-encoded, exact-size input at multiple byte
offsets into reused storage. `test/binary_array_decode_test.cpp` covers work-budget
boundaries and partial results, truncated payloads, input/allocation/collection/depth
limits, cumulative session budgets, and scalar boolean/enum validation.

**Validation status:** source implementation and tests are prepared. Configuration,
header generation, compilation, tests, sanitizer runs, and benchmarks remain
deferred. No measured speedup is claimed. See
[qualification](../qualification/README.md#runtime-simd-validation) before reporting
performance or platform coverage.
