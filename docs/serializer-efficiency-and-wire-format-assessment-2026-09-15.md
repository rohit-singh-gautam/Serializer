# Serializer efficiency and wire-format assessment

Reviewed: 2026-09-15. Source revision: `18f068f932363b58c3792e43cb09cee5f9c016d6`
(`Code cleanup`).

## Scope and conclusion

This document assesses Serializer's own generator, codec, stream implementation,
and tests. Findings and recommendations are grounded in this repository's source.
Source line numbers and historical symbol names below refer to the reviewed
revision. File links use the current names after the API migration; see
[migration.md](../migration.md) for the corresponding symbol names.

The architecture is a promising foundation for schema-driven binary serialization:
generated field access, compile-time protocol selection, and contiguous buffer
writes avoid runtime reflection and an intermediate document tree. Positional
binary has the lowest structural overhead of the implemented modes. However,
generated deep copies, collection allocation patterns, and runtime dispatch limit
efficiency. Buffer, numeric, and decoding correctness issues should be addressed
before interpreting benchmark results or qualifying untrusted network input.

This is a source assessment with existing-test verification, not a performance
benchmark, exhaustive defect audit, or completed consumer integration. No throughput
ranking against other libraries or measured speedup is claimed. These are proposed
changes unless marked in the implementation record below.

## 1. Efficient design choices to preserve

**Current implementation status:** Steps 1-9 below record the changes after this
historical assessment. The concrete correctness, allocation, JSON, diagnostics,
and schema-validation recommendations are implemented in source. New regression,
fuzz, timing, and allocation-profile targets are prepared but have not been run.
Measurement-dependent API/format experiments remain conditional; see step 9 and
[the qualification guide](../qualification/README.md).

- Generated serializers select protocol behavior with `if constexpr`; ordinary
  field access does not require runtime reflection or metadata lookup. This is an
  existing design choice. Keyed input still dispatches on IDs or names read from
  the message; compile-time protocol selection does not eliminate that work.
- Output goes directly into a contiguous stream buffer. Binary values are encoded
  field by field, rather than copying whole C++ object layouts and their padding.
- Positional binary omits field identifiers. Integer-key binary uses compact IDs;
  string-key binary preserves field names.
- JSON member names are read as `std::string_view`, avoiding a temporary owning
  string for normal member dispatch. Generated dispatch hashes the name and uses
  a switch; hashing still scans the name.
- JSON integer output uses `std::to_chars` with a stack buffer.
- Output buffer growth generally doubles capacity for small incremental writes,
  giving amortized growth rather than reallocating for every byte.

Evidence: [generator](../src/cpp_writer.cpp), lines 212-229 and 373-393;
[codec](../include/rohit/serializer.hpp), lines 232-243 and 981-1019;
[stream](../include/rohit/stream.hpp), `AppendString` and `FullStreamAutoAlloc`.

## 2. Performance findings and recommendations

### 2.1 Remove generated deep copies first

The generator emits owning pairs for ordinary fields in JSON and both keyed binary
modes, for example:

```cpp
serializerProtocol.StructSerializeOut(
    std::make_pair(std::string_view{"list"}, list));
```

`std::make_pair` copies the lvalue member into the pair. Strings, vectors, maps,
and nested objects can therefore allocate and copy their contents before the
serializer traverses them. Nested generated serialization can copy descendants
again at successive levels. This adds temporary memory as well as CPU work.
Ordinary positional binary fields are passed directly, but union output also
uses owning pairs or tuples, including in positional mode.

**Recommendation:** Generate separate key/value arguments with values accepted by
`const&`, or explicitly reference-bearing pairs/tuples with suitable lifetimes.
Do not move from the object being serialized. Preserve the encoded bytes.

Evidence: [cppwriter.cpp](../src/cpp_writer.cpp), lines 146-191.

### 2.2 Reuse destination storage and avoid element copies

| Path | Current cost | Recommendation |
|---|---|---|
| Binary vector input | Reads the element count, then appends without reserving capacity. | Validate the count and allocation budget, then reserve the required capacity once. |
| JSON vector input | `emplace_back(valuetype)` copies a parsed temporary. | Move the temporary, or deserialize into a destination element if the chosen failure contract permits it. |
| JSON string input | Appends one character at a time. | Scan bounded spans and append/assign runs; integrate escape decoding and reuse capacity. |
| Binary string input | Constructs and assigns a temporary owning string. | Assign directly from the validated byte range when destination reuse is appropriate. |
| String-key binary input | Constructs an owning string for every field name. | Decode a bounded `string_view` into the input for immediate dispatch, with explicit lifetime rules. |
| Map input | Inserts individual nodes into `std::map`. | Retain ordered-map semantics where required; evaluate insertion hints for validated sorted input or alternative container support as separate API work. |

Define replacement versus append semantics before optimizing reuse. JSON strings
and both vector readers currently append to existing values; maps insert into the
existing map. Repeated decoding into the same object can accumulate data. A later
failure can also leave partially updated fields or collections. Capacity reuse
must not silently change the documented result or failure behavior.

Evidence: [serializer.h](../include/rohit/serializer.hpp), lines 353-436,
830-861, and 875-880.

### 2.3 Reduce runtime dispatch in hot loops

Serializers hold `Stream&` or `const Stream&`, while cursor increments, cursor
advances, and reservation are virtual. JSON scanning can therefore incur an
indirect call for each character unless the compiler devirtualizes the calls.
The `inline` keyword does not itself remove virtual dispatch.

JSON collection output also accepts its per-element callback as `std::function`.
The small capturing lambdas do not necessarily allocate, but type erasure can
impede inlining and introduce an indirect call per element.

**Recommendation:** Replace the collection callback with a templated callable.
Measure optimized stream dispatch before considering a larger change such as a
stream-type template or a bounded local cursor committed after processing a span.
Any such change must preserve bounds checks, growth, and error behavior.

Evidence: [stream.h](../include/rohit/stream.hpp), lines 167-195 and 217;
[serializer.h](../include/rohit/serializer.hpp), lines 691-750.

### 2.4 Avoid temporary floating-point text

JSON floating-point input scans the token, constructs a `std::string`, then calls
`std::stof` or `std::stod`. Output constructs a string using `std::to_string`.
This adds temporary construction, possible allocation, and an extra input scan.

**Recommendation:** Use bounded `std::from_chars` parsing and buffer-based
`std::to_chars` output. Specify precision, round-trip guarantees, range errors,
and treatment of non-finite values. Changing formatting can change JSON bytes;
qualify that behavior explicitly rather than treating it as invisible cleanup.

Evidence: [serializer.h](../include/rohit/serializer.hpp), lines 364-373 and 719-722.

### 2.5 Correct reservation and growth calculations

Single-byte `Append` calls `Reserve(value)` instead of `Reserve(1)`. For example,
a comma requests 44 bytes of remaining capacity. This can cause premature growth
or false overflow errors; a zero byte requests no capacity before writing.

Large appends calculate capacity through a loop that repeatedly adds the old
capacity. Compute the required capacity and geometric growth directly, with
overflow checks and a nonzero starting capacity. A zero-capacity starting state
cannot make progress in the current additive loop.

Preserve the old allocation until `realloc` succeeds. The current direct
assignment can lose the original pointer on failure, and the no-length growth
overload does not check for allocation failure. Audit owning-stream copy/move
behavior and all growth paths as part of this work.

Evidence: [stream.h](../include/rohit/stream.hpp), lines 227-228 and 349-376.

## 3. Wire size and scaling

| Mode | Wire overhead and tradeoff |
|---|---|
| Positional binary | No ordinary field names/IDs or object terminator; requires agreement on field order and types. |
| Integer-key binary | Encoded ID per field and a zero-ID object terminator; generated copies currently add CPU/memory overhead. |
| String-key binary | Length-prefixed name per field and an empty-name terminator; input currently copies names before dispatch. |
| JSON | Field names, punctuation, numeric text conversion, and optional whitespace; maps are arrays of `key`/`value` objects. |

Ordinary binary integers use their declared width: a `uint64_t` containing `1`
still occupies eight bytes. Lengths, IDs, and enums use the custom one-to-four-byte
variable encoding with six payload bits in the first byte. Applying variable
encoding to ordinary integers would be a wire-format change and should depend on
measured value distributions and decoding costs.

Traversal is generally linear in the represented data. Map decoding adds
approximately O(n log n) insertion work and individual node allocations. Generated
copies add traversal and allocation proportional to the copied subobjects; their
cost can grow with nesting depth. A reusable, adequately sized output buffer
reduces allocation overhead but does not remove those copies.

Evidence: [serializer.h](../include/rohit/serializer.hpp), lines 681-687,
790-808, 956-977, 981-1019, and 1039-1044.

## 4. Applicable wire-format and robustness findings

### 4.1 Fix exact-message length boundaries and reject unsupported values

After reading the first variable-encoding byte, the three-byte branch requires
three remaining bytes instead of two; the four-byte branch requires seven instead
of three. Valid encodings at the end of an exact-size input are rejected.
The writer has no error branch above `0x3fffffff` and emits no bytes for such values.
Reject out-of-range lengths, IDs, and enum values explicitly before modifying
output; define negative-value behavior for the integral entry point as well.

The branch conditions imply these boundary cases for future exact-size tests:

| Values | Bytes emitted | Expected current behavior from source inspection |
|---|---:|---|
| 63 | 1 | Accepted |
| 64; 16,383 | 2 | Accepted |
| 16,384; 4,194,303 | 3 | Valid input rejected |
| 4,194,304; 1,073,741,823 | 4 | Valid input rejected |
| 1,073,741,824 | 0 | Writer returned without encoding or error |

These are source-derived expectations, not newly executed test results.

The existing variable-integer test resets a 256-byte output buffer and decodes
against its full capacity. Spare capacity masks these exact-end failures. Construct
reader views ending at the number of bytes actually written.

Evidence: [serializer.h](../include/rohit/serializer.hpp), lines 790-808 and
956-977; [coreserializertest.cpp](../test/core_serializer_test.cpp), lines 469-488.

### 4.2 Make scalar encoding portable

Binary scalar paths dereference `reinterpret_cast<T*>` pointers into byte buffers.
Offsets after short fields or variable-length prefixes need not satisfy `T`'s
alignment. Use `memcpy` between byte storage and a suitably aligned scalar, with
explicit byte-order conversion; this also avoids relying on typed access to
arbitrary byte storage.

Floating-point endian conversion is not implemented through an integer bit
representation. The C++23 branch passes its argument directly to `std::byteswap`,
including floating-point arguments from binary serialization; the fallback also
applies integer bit operations to its argument. These paths need correction and
explicit float/double instantiation checks; no new compiler probe was run here.
Use a bit-preserving conversion to a matching unsigned integer representation,
swap those bits, and convert back. State and verify the supported floating-point
representation and widths; add float/double golden-byte tests.

Evidence: [serializer.h](../include/rohit/serializer.hpp), lines 825-840 and
990-1005; [stream.h](../include/rohit/stream.hpp), lines 32-71.

### 4.3 Complete bounded decoding and define failure behavior

Base `Stream` cursor advancement is unchecked, and some checked variants validate
after advancement or do not validate the entire requested length. JSON whitespace,
key, and string loops can dereference before verifying that input remains.
Audit read/write boundaries, exact-capacity success, truncation, overflow-safe
arithmetic, and behavior for each stream implementation.

For untrusted input, support explicit limits on total bytes, string length,
collection count, nesting depth, and cumulative allocation/work. Validate lengths
against these limits before reserving memory. A count is not automatically a byte
length, especially for nested or variable-size elements.

Define whether failed decoding leaves a partial result or preserves the original
object and cursor. If atomic replacement is required, decode into temporary state
and commit on success, accounting for the extra memory in benchmarks.

JSON also needs complete escaping/unescaping, numeric grammar/range handling, and
malformed-input checks. Raw string writes and incomplete parsing perform less work
than a fully qualified JSON codec; performance comparisons must use equivalent
behavior.

Evidence: [stream.h](../include/rohit/stream.hpp), `Stream`,
`FullStreamLimitChecked`, and `FullStreamAutoAlloc`;
[serializer.h](../include/rohit/serializer.hpp), lines 219-243, 334-373, and 708-728.

### 4.4 Preserve explicit field IDs and specify schema evolution

Explicit member and parent IDs are already implemented. `ParseNameSpec` reads
numeric overrides; `ParseMember` and
`ParseParent` use them. The existing schema includes:

```text
public string name ("fullname", 3) { "None" };
public uint64 ID (4) { 0 };
```

The README's earlier statement that explicit IDs are future work conflicts with
its later modifier examples and the implementation. Update that documentation.

The compatibility concern still applies to implicit declaration-order IDs.
Require explicit, stable IDs for evolving schemas; validate uniqueness, supported
range, the zero terminator reservation, and collisions between implicit and
explicit IDs. Define rules for removed fields, reused IDs, enum values, union
discriminators, defaults, duplicate fields, and type changes.

Generated keyed readers currently throw on unknown fields. An ID or name alone
does not describe the unknown value's type or extent, so general binary skipping
requires additional framing/type information or a schema-aware version contract.
Do not claim forward compatibility solely because a mode uses keys. Introducing
type/length framing would change the wire format and should be versioned.

Generated name dispatch compares hashes without confirming name equality. Add
collision-aware generation and equality validation before trusting a hash match
as a field identity.

Evidence: [parser.cpp](../src/parser.cpp), lines 270-302, 325-351, and 381-400;
[person.serializer](../test/resources/person.serializer);
[cppwriter.cpp](../src/cpp_writer.cpp), lines 350-393.

### 4.5 Document diagnostics and compression accurately

- **Diagnostics:** Parser exceptions include surrounding input bytes. For network
  consumers, provide structured error codes and bounded, opt-in input excerpts;
  avoid returning or logging payload fragments by default. Existing excerpts are
  bounded, but are not a data-redaction mechanism.
- **Compression:** `format::compress` disables JSON whitespace. No binary compression
  codec appears in the reviewed implementation or declared dependencies. Clarify
  this terminology. If compression is added, keep it as a separately specified
  layer, with compressed and decompressed size limits and independently measured
  CPU, memory, and byte savings. No compression implementation is proposed in this
  step.

Evidence: [stream.h](../include/rohit/stream.hpp), `BaseParser::CreateWhatString`;
[serializer.h](../include/rohit/serializer.hpp), `write_format` and `format::compress`;
[vcpkg.json](../vcpkg.json) and [README](../README.md).

## 5. Suggested implementation order

| Priority | Work | Acceptance evidence |
|---|---|---|
| P0: correctness | Single-byte reservation, exact-size variable decoding, range rejection, safe scalar access and floating conversion, stream/growth failure handling. | Exact-size and truncated-input tests, float/double compile and golden-byte tests, allocation-failure checks, supported-toolchain builds. |
| P1: largest allocation wins | Remove generated owning pairs/tuples; reserve validated binary collections; remove unnecessary input copies. | Same encoded bytes and intended decode semantics; reduced allocations/copies on large strings and nested collections. |
| P1: decoder qualification | Resource budgets, JSON correctness, defined partial/atomic failure semantics, safe diagnostics. | Bounded malformed-input and fuzz tests; resource-limit and reuse/failure tests. |
| P2: local CPU improvements | Templated callbacks, character conversion, bounded span processing. | Representative optimized-build benchmarks plus formatting/round-trip checks. |
| P2: compatibility | Stable-ID validation, unknown-field/version contract, name-collision handling, wire specification. | Old/new schema fixtures, duplicate/unknown-field cases, golden bytes across supported consumers. |
| P3: measured expansion | Stream abstraction changes, alternative containers, new compression layer or language backends. | Workload evidence justifying API/format complexity and explicit compatibility qualification. |

Keep implementation-only optimizations byte-compatible. Review formatting changes,
failure-contract changes, and any new wire framing as explicit behavior changes.

## 6. Benchmark and verification plan

Benchmark serialization and deserialization separately for all four modes using:

- Small scalar records; short and long strings; integer and floating-point arrays;
  vectors of generated objects; maps; and nested collections/unions.
- Fresh and reused objects/buffers, adequate initial capacity and forced growth,
  and exact-size input views. Ensure reuse does not accidentally accumulate data.
- Small and large payloads, realistic value distributions, explicit compiler,
  optimization, architecture, and stream-type settings.
- Time per message, messages/second, encoded bytes, allocation count/bytes, and
  peak temporary memory. Define the byte basis for any throughput metric.
- Warm-up and repeated samples, with observable/validated outputs so the compiler
  cannot eliminate work. Keep disk/network I/O and optional compression separately
  measured. Exclude code generation from codec timing and measure it independently
  if build-time efficiency matters.

Before comparing libraries, align supported types, JSON correctness, bounds/resource
checks, schema guarantees, output format, and buffer reuse. Do not attribute the
benefit of omitted validation or different precision to implementation efficiency.

### Verification record

- During the local efficiency review, `cmake --build --preset ReleaseWindows`
  succeeded, including generation of test headers and the test executable.
- `ctest --test-dir out/build/ReleaseWindows -C RelWithDebInfo --output-on-failure`
  then passed **1/1 CTest target**. This is a target count, not an individual
  GoogleTest case count or performance measurement.
- The build and test results above predate the API migration and the implementation
  step below. They do not validate subsequent changes.
- No throughput benchmark, allocation profile, sanitizer/fuzz campaign,
  cross-language test, or end-to-end consumer qualification was performed in this
  review. Passing the existing suite does not resolve the identified coverage gaps.

## 7. Implementation record

### Step 1: Make compile-time protocol selection explicit

Implementation baseline: `da91b4d95181fded1caeb219454dc2126d4fc934`.

The generator already emitted `if constexpr` branches and direct field access.
This step tightens that existing contract in generated `serialize_out` and
`serialize_in` methods:

- Read `key_type` from `SerializeOutProtocol` or `SerializeInProtocol` explicitly.
- Require a compile-time mode of `none`, `integer`, or `string` with a dependent
  `static_assert`. The previous `static_assert(true)` accepted unsupported modes
  without reading or writing fields.
- Preserve the existing field emission and keyed input dispatch. No runtime
  reflection or metadata table is introduced.

Custom protocols must expose a static constant-expression `key_type` using the
supported enum values. Supported modes retain the same emitted field operations;
no speedup or new benchmark result is claimed for this clarification.

Only generator source is changed for this implementation step. Header regeneration,
compilation, and all tests are deferred until requested. Future verification should
cover each supported mode and compile-time rejection of unsupported modes.

### Step 2: Harden direct, field-by-field binary output

The generator already serialized fields individually, and the stream already
stored output in contiguous memory. This step improves scalar writes within that
existing design:

- Characters and booleans append one byte through the stream's reservation policy.
  Booleans encode as zero or one independently of the native `bool` object size.
- Integral values convert to their unsigned representation and selected byte
  order, then append their bytes without a typed store into the output buffer.
  A field can start at an unaligned offset without requiring aligned scalar access.
- Floating-point output bit-casts a supported 32-bit or 64-bit IEC 559 scalar into
  a matching unsigned integer and uses the same selected-endian output path. Other
  floating-point representations are rejected at compile time.
- Both single-byte stream append overloads reserve `sizeof(value)` bytes, fixing
  the previous reservation based on the numeric byte value. This also affects
  variable-length headers and JSON punctuation using those overloads.

Object field order, keys, enum encoding, string lengths, and collection traversal
remain as before. No raw whole-object copy or intermediate record buffer is added;
only the individual converted scalar is held locally before its bytes are appended.
Fixed-width scalars now default to little-endian, an intentional wire change in
the subsequent view implementation. Compact prefixes retain their encoding.
The codecs expose `wire_endian` and an explicit byte-order template argument.
See [the wire contract](wire_format.md#byte-order) and [view guide](views.md).
Generation, builds, and tests remain deferred for this implementation step.

The single-byte reservation issue in section 2.5 and the output-side scalar issues
in section 4.2 are addressed by this source change. Binary input alignment and
floating-point conversion, variable-length range/boundary handling, buffer-growth
failure handling, and generated owning-pair copies remain for later steps.

No build, header regeneration, or tests were run for this step. Future verification
should cover exact-capacity and too-small buffers, unaligned field offsets, signed
integer boundaries, floating-point bit patterns, and generated objects with native
padding across all binary modes.

### Step 3: Correct stream bounds, growth, ownership, and I/O

Further source inspection of [stream.hpp](../include/rohit/stream.hpp) identified
and addressed these issues:

- Forward movement and dereferencing validate available bytes before access or
  cursor mutation. Exact-end movement is permitted. Backward movement in every
  `full_stream` variant checks the stored start, including postfix operations.
- Capacity checks use remaining byte counts rather than forming an out-of-range
  pointer. Empty buffers and zero-byte operations avoid null-pointer arithmetic.
- `push` reserves before writing. Overlapping append ranges use `memmove`, and
  owned streams rebase sources inside their allocation after successful growth.
- Growth computes a bounded geometric capacity directly, handles zero capacity,
  and checks arithmetic before allocation. `realloc` failure preserves the old
  storage and cursor. Replacement-buffer allocation also commits only on success.
- Bounded streams validate and own a snapshot of their limits, including zero
  minimum capacity and growth up to the exact maximum.
- Owning streams prevent accidental copies and support ownership moves. Borrowed
  assignment updates the complete view; const cursor commits require a shared end.
- Byte swapping handles one-byte values, uses unsigned integer operations, and
  converts supported floating-point representations through their integer bits.
  Unsupported-type assertions are dependent on the instantiated type for C++20.
- Literal comparison checks the correct number of bytes, and explicit literal
  hashing excludes the trailing terminator. Boolean text output avoids the
  deleted `std::to_chars(bool)` overload.
- File reads validate size, allocation, seeking, and complete reads, with owned
  storage released on failure. File writes report open, write, and close errors.

See [migration.md](../migration.md#stream-safety-and-ownership) for caller-visible
changes. Explicit unchecked access and caller-supplied raw pointer ranges still
require valid storage and lifetimes. These changes do not complete the separate
binary decoder, JSON grammar, or request-wide resource-budget work.

The changes have only been reviewed as source. Builds, regression tests, allocation
failure tests, sanitizer runs, and performance measurements remain deferred at the
user's request. Future verification should exercise every cursor overload,
empty/exact/short buffers, overlapping append with growth, ownership transfer,
allocation limits/failures, integer and floating-point byte swaps, literal helpers,
and failed or short file I/O.

### Step 4: Require C++20 and constrain endian conversion

- CMake establishes C++20 as its minimum/default language mode while retaining a
  caller-selected newer standard. The public `cxx_std_20` usage requirement
  continues to propagate to consumers. A header guard rejects older language
  modes, including MSVC configurations that report their mode via `_MSVC_LANG`.
- For supported integral types, `rohit::byteswap` forwards directly to
  `std::byteswap` when `__cpp_lib_byteswap >= 202110L`. Its C++20 fallback reverses
  an object-byte array using `std::bit_cast` and constexpr `std::reverse`, avoiding
  assumptions about native byte order. Booleans remain no-ops; supported floating
  values pass through an integer representation and use the same integer path.
- Shared concepts constrain both endian helpers to booleans, integers without
  padding bits, and supported binary IEC 559 floating-point widths. Unsupported
  types cannot bypass validation through the unchanged-value branch.
- `change_endian` requires each byte-order argument to resolve to little or big
  endian, rejecting mixed native order and invalid enum values at compile time.
  Same-order, single-byte, and boolean conversions remain no-ops. Both helpers
  are `constexpr` and `noexcept`.

The constraints follow the public [standard byteswap specification](https://eel.is/c++draft/bit.byteswap)
and [byte-order definition](https://eel.is/c++draft/bit.endian). No build,
configuration run, generated-header regeneration, or tests were performed for this
step. Deferred verification should cover C++20 fallback and newer-library
forwarding, supported scalar types, and compile-time rejection of unsupported
types and byte-order arguments.

### Step 5: Separate checked operations from prevalidated batches

Ordinary forward cursor operators and dereferencing retain their checks. Explicit
nonvirtual, `noexcept` helpers provide byte access, single-byte output, range
copying, range consumption, and cursor advancement without capacity checks or
allocation. The caller validates the entire input range or reserves the entire
output range before using these helpers. Their contract applies equally to
borrowed and allocating streams.

Byte-only `write_raw(...)` calls capture their arguments, reserve the complete
batch once, and copy it directly into the stream. Binary variable-length output
already calls this API, so its two-, three-, and four-byte cases now use one
reservation per encoded value. Capturing arguments before reservation preserves
references into an allocation that may move. A capacity failure writes none of
the batch; mixed argument packs continue to append each argument separately.

Binary input uses unchecked range consumption and advancement only after its
existing length check. The three- and four-byte variable-integer cases validate
the actual remaining payload lengths of two and three bytes, respectively.
Other decoder issues, including typed unaligned scalar reads, remain separate.

For ten byte writes, one reservation followed by unchecked stores eliminates nine
redundant capacity checks compared with reserving each byte. This is a source-level
comparison, not a measured machine-branch count or throughput claim. The checked
operators remain available for operations without a prior range guarantee.

No build, configuration run, header regeneration, tests, or benchmarks were run.
Deferred verification should cover reservation counts, byte ordering, exact and
insufficient capacity, source aliases across growth, unchanged checked-operator
behavior, and exact/truncated variable-length input payloads.

### Step 6: Simplify remaining-capacity validation

`remaining_buffer()` removes the separate equal-pointer return. A combined check
rejects mismatched null pointers and reversed ranges, while permitting two null
pointers for an empty stream. C++20 defines their difference as zero. Runtime
validation remains because the public API still permits direct cursor and end
pointer mutation. Nonnull pointers must belong to the same live buffer; ordering
checks cannot establish allocation identity.

This removes a source-level conditional without claiming a measured speedup.
Builds and tests remain deferred; verification should cover null-empty streams,
exhausted buffers, nonempty ranges, mismatched null pointers, and reversed ranges.

### Step 7: Apply the stream optimization review

- Prefix equality uses `std::memcmp` for nonempty validated ranges, fixing
  comparisons of identical bytes above `0x7F` on signed-char targets. Empty
  prefixes and the array overload's final-terminator handling are preserved.
- Cursor advancement and consumed-size helpers no longer branch for zero
  increments or equal pointers. Capacity and offset accessors consolidate their
  null-state checks; offset validation now also runs when the cursor equals the
  buffer start. Invalid ranges still throw in checked accessors.
- Bounded reservations compute the validated offset and capacity once, enforce
  the logical maximum, and enter the growth calculation only when required.
  Growth remains transactional and retains geometric sizing and allocation
  limits, including for adopted buffers larger than the logical maximum.
- `write(...)` batches multiple characters, booleans, character arrays, strings,
  and string views. It validates the total length, captures scalar bytes and text
  lengths, reserves once, then copies parts in argument order. A fixed-size stack
  array holds descriptors, not copies of the text. Internal text sources are
  rebased after growth, and range copies retain overlap support. Character-only
  batches reuse the existing byte-pack path. Numeric mixtures and single arguments
  retain their existing paths.
- `append_external` reserves once and copies an independently owned source without
  alias bookkeeping. Integer formatting and binary scalar output use this path.
  General appends retain alias rebasing and `memmove`. The numeric conversion
  buffer is no longer zero-initialized; conversion success is checked before its
  produced prefix is appended. Only the actual formatted length is reserved.

The new protected `reserve_fragments` hook supports source rebasing for batches.
The built-in allocating streams implement it and still call virtual `reserve`
once per batch. Custom relocating streams need the equivalent hook; consumers
must be recompiled after changes to the virtual interface. Batch capacity failure
writes none of that batch, consistent with byte-only `write_raw`.

Direct-to-output numeric formatting and a wider virtual-dispatch redesign remain
evaluation candidates: they need a policy-aware way to use existing capacity
without rejecting exact-fit output or bypassing custom reservation limits.
No measured performance improvement is claimed. In particular, descriptor setup
for short mixed writes must be included in future measurements.

No builds, configuration runs, generated-header regeneration, or tests were run.
Deferred verification should cover signed/unsigned-char prefix comparisons,
null-empty and invalid pointer states, exact/insufficient capacity, bounded
reservations against oversized adopted storage, allocation failures, one
reservation per text batch, overlapping text sources before and after growth,
custom reservation overrides, and integer boundary formatting.

### Step 8: Enforce the existing binary field-key contract

Source review confirmed that generated output and input already implement the
three modes described in section 1:

- Positional binary writes and reads fields in schema order with no field IDs,
  field names, or object terminator. Union alternative indices remain necessary
  to select the payload type; they are not field identifiers.
- Integer-key binary writes one-to-four-byte field IDs and a zero-ID terminator.
- String-key binary preserves each configured wire name with a compact length
  prefix and an empty-name terminator. Union keys retain `field:alternative`.

This step closes validation gaps without changing representable wire values:

- The compact integer writer rejects negative values and values above
  `0x3fffffff` before writing that integer. Previously, negative values could emit
  an invalid first byte and oversized values could silently emit nothing. The
  same validation covers lengths, enum values, and union indices.
- Integer-key field output rejects ID zero. String-key field output rejects
  empty names. Numeric/named field overloads reject incompatible key modes at
  compile time; positional union index zero remains valid.
- Schema fields and parents require IDs in `1..0x3fffffff` and nonempty wire
  names. Decimal ID parsing uses `std::from_chars` to reject conversion overflow
  instead of narrowing `std::stoul` results. Valid explicit metadata and default
  ID assignment remain unchanged.
- Both keyed terminators write the existing zero byte directly. Ordinary scalar
  integers retain their declared width and signed-value support.

The README now documents the existing ID/name override syntax and shows exact
bytes for the same field in each mode. The migration guide records the newly
rejected inputs and exceptions. These updates resolve the output-range gap
identified in section 4.1; unrelated decoder and schema-evolution work remains
outside this step.

No builds, configuration runs, generated-header regeneration, tests, or
benchmarks were run. Deferred verification should cover the existing wire
snapshots for all modes, compact integer width boundaries, negative/oversized
values, zero field IDs versus zero union indices, explicit parent/field metadata,
empty names, and malformed or oversized decimal ID tokens. Rejection of a value
does not roll back previously emitted fields or prefixes.

### Step 9: Complete the concrete assessment recommendations

| Assessment area | Source implementation |
| --- | --- |
| 2.1 Generated copies | Keyed fields and all union payloads use `std::cref` in reference-bearing pairs/tuples. Enum field names use borrowed views; source objects are neither copied nor moved. |
| 2.2 Destination reuse | Strings/vectors/maps have explicit replacement semantics. Binary vectors reserve once after count/storage validation, JSON vectors move temporaries and validate growth, binary strings assign ranges directly, and binary field names borrow bounded views. |
| 2.3 Runtime callbacks/scanning | JSON collection callbacks are templates. Whitespace, number, and string scans use bounded local spans and commit cursor changes in runs. |
| 2.4 Floating text | Bounded `from_chars` parsing and shortest general-format `to_chars` output replace owning conversion strings. Range and non-finite behavior are explicit. |
| 2.5 / 8.1-8.6 Stream fixes | Prior steps retain checked operators, transactional growth, batching, byte comparisons, independent-source copying, and reduced reservation work. |
| 4.1 Compact integers | The reader validates the full tagged extent before a single cursor update. Tests use actual encoded lengths, including every truncated prefix at width boundaries. Writer range checks remain in place. |
| 4.2 Scalar portability | Binary input uses `memcpy` into aligned unsigned storage and bit-preserving integer/floating conversion. No typed scalar loads from byte buffers remain. |
| 4.3 Bounded decoding | A shared, noncopyable decoder session tracks byte, string, collection, depth, storage, and work limits. Strict JSON strings/Unicode, numbers, punctuation, literals, and empty objects are implemented. Replacement and partial-failure behavior are documented. |
| 4.4 Schema identity | ID/name uniqueness and explicit/implicit collisions are validated. `stable_ids` requires explicit member/parent IDs. Hash groups check full name equality. Enum/union values are validated and selected raw union members are constructed before input. |
| 4.5 Diagnostics/compression | Structured exception categories and bounded opt-in excerpts replace automatic payload excerpts. Documentation distinguishes compact JSON from compression. |
| 6 Verification/measurement | Regression cases and optional generated-record fuzz, timing, and allocation-profile targets are added for later execution. |

The binary representation for valid existing values remains unchanged. JSON
escaping and shortest floating-point text are intentional output changes, and
strict parsing rejects previously accepted malformed values. Collections replace
existing contents; generated objects retain partial updates and missing-field
defaults from their destination. Raw union payloads must be trivially destructible.
See [migration](../migration.md) and [the wire specification](wire_format.md).

Additional source integration fixes make these paths usable: input capability
checks now check `serialize_in`, formatter indentation has balanced push/pop
behavior, empty/private-ended classes expose public serialization methods,
type-resolution copies retain resolved metadata, schema EOF/comment handling is
bounded, and schema parsing/generation exceptions return a nonzero exit status.

The following assessment items explicitly depend on measurement or a separate
API/format decision and are **not unconditional implementation recommendations**:

- Direct integer formatting into output storage and a public stream-hierarchy
  redesign: retain exact-length stack formatting and custom reservation policies
  until profiling demonstrates a benefit and a compatible policy contract.
- Sorted-map insertion hints/alternative containers: preserve arbitrary incoming
  order and `std::map` behavior; evaluate separately with workload evidence.
- Ordinary-integer variable encoding, unknown-field framing, compression, and
  new language backends: introduce none without a versioned contract and measured
  justification. The document explicitly proposes no compression codec here.

Prepared tests cover exact/truncated binary inputs, odd-offset float/double bytes,
strict JSON grammar/Unicode and round trips, destination reuse and failure, resource
limits, generated modes and enum collections, schema identity, and diagnostics.
Timing and allocation profiling are separate executables to keep instrumentation
out of codec timing. Allocation peaks report requested incremental storage, not
allocator/RSS measurements; their scope is documented in the qualification guide.

No builds, configuration runs, generated-header regeneration, tests, fuzzing, or
benchmarks were run in this implementation step, as requested. Source/whitespace
review is not runtime, portability, or performance qualification.

## 8. Follow-up optimization review of stream.hpp

These findings describe the source after step 6. Step 7 records the implementation
status; direct-to-output numeric formatting and the broader virtual-dispatch
redesign remain candidates. The review retains checked public operations and
the explicit unchecked helpers for previously validated batches. No measured
speedup is claimed.

### 8.1 Correct byte comparisons before measuring them

Both `stream::operator==` overloads compare `char` elements against `std::uint8_t`
elements using `std::equal`. On a signed-char target, the same stored byte can
promote to different numeric values: a character containing `0xFF` promotes to
`-1`, while the stream byte promotes to `255`. This can reject identical byte
sequences containing non-ASCII data.

Use a byte-wise comparison, such as `std::memcmp`, after validating the prefix
length and handling the empty case. Preserve the existing prefix semantics and
the array overload's treatment of a final terminator. This fixes correctness
and permits a bulk byte comparison; any performance benefit needs measurement.

### 8.2 Remove redundant pointer-arithmetic special cases

`advance_cursor` branches on a zero increment. `get_size_from` and
`fixed_buffer::capacity` branch on equal pointers before subtracting. C++20
already defines adding zero to a null pointer and subtracting two null pointers,
so these special cases are unnecessary when their range preconditions hold.
The analogous branches in `full_stream::capacity` and `current_offset` can be
consolidated with their invalid-state checks, as done for `remaining_buffer`.
Keep the checks for mismatched null pointers and reversed ranges.

This reasoning applies to pointer arithmetic; it does not justify calling memory
copy functions with null pointers. See the public [C++20 pointer-arithmetic rules](https://timsong-cpp.github.io/cppwp/n4861/expr.add).

### 8.3 Reduce work when a bounded stream already has sufficient capacity

`full_stream_auto_alloc_limits::check_resize` always enters `grow_storage`, even
when no allocation is needed. That path computes a checked cursor offset,
validates allocation-policy bounds, computes required capacity, and obtains a
checked capacity before returning. The unbounded allocator already has a shorter
path for a request that fits.

Separate the bounded stream's common reservation path from its growth calculation,
and reuse validated offsets and capacities rather than recomputing them. Retain
the logical maximum check: inherited pointer constructors can adopt storage larger
than the default maximum, so physical spare capacity alone does not authorize a
write. Preserve allocation-failure behavior and downstream reservation overrides.

### 8.4 Batch mixed text writes

`write(...)` still calls `append_string` separately for every argument. For
example, JSON output's `write('"', value, '"')` reserves three times. The generator
also makes many mixed text writes. The byte-only optimization in `write_raw`
does not cover these calls.

Start with characters, literals, strings, and string views: calculate the total
length with checked arithmetic, reserve once, and write the parts through the
unchecked helpers. Handle aliased sources before growth and preserve overlapping
source behavior. Specify that capacity failure rejects the complete batch before
writing, consistent with byte-only `write_raw`.

### 8.5 Reduce integer-formatting setup and copying

`append_string` zero-initializes a temporary character array, formats the integer
with `std::to_chars`, then appends the produced prefix. Successful conversion
writes every byte of that prefix; clearing the unused array bytes is unnecessary.
Retain a sufficient buffer-size bound and ensure conversion success before
consuming the result. See the [C++20 to_chars contract](https://timsong-cpp.github.io/cppwp/n4861/charconv.to.chars).

A larger change could format directly into already available output storage when
the stream policy permits the maximum representation length. Keep the stack-buffer
fallback when only the actual shorter representation fits: reserving the maximum
unconditionally can reject otherwise valid writes or cause unnecessary growth.
Measure whether the compiler already eliminates the temporary initialization.

### 8.6 Reduce alias bookkeeping and virtual dispatch where proven unnecessary

The pointer append path calls virtual `reserve_append`, which can perform source
alias checks and then call virtual `reserve`. This is required for general sources
that may reside in an allocation moved by growth. Some callers have independently
owned sources, including formatted stack buffers and converted scalar temporaries.

Such callers can use one policy-aware reservation followed by an unchecked copy,
avoiding alias-rebasing work. A broader refactor could reserve virtual dispatch for
growth, but it must preserve maximum-capacity policies and custom overrides.
Do not replace general overlapping copies with `memcpy` or bypass reservation
policies simply to shorten the call chain. Inspect optimized code before changing
the public stream hierarchy.

Suggested order: fix byte comparisons, simplify pointer helpers, improve bounded
reservations, then batch mixed writes and evaluate direct integer formatting.
The virtual-dispatch redesign is a later candidate. No builds, tests, header
regeneration, or benchmarks were run for this review.
