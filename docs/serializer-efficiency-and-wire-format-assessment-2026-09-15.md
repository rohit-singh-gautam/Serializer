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
[person.def](../test/resources/person.def);
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

Implementation baseline: `fcf4f84d7a17f6628e489f881511854b72a5d8ce`.

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
