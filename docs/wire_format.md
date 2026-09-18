# Wire format and decoding contract

The opt-in C++ `protobuf_binary`, `protojson`, and `textproto` protocols have a
separate [Protobuf mapping and decoding contract](protobuf.md). They do not use
the custom binary layouts or ordinary JSON mapping documented below. Java
currently supports only the four original protocols described here.

The `.serializer` source header `serializer version 1;` selects the schema
language. It is independent of the compiler release and is not emitted into
messages. It identifies neither message byte order nor a wire-format version; see
[compiler and schema versions](command_line.md).

The pure Java backend implements the same four protocols for its supported
owning types. Binary fixed-width values are little-endian, IDs and compact
prefixes are unchanged, and Java retains original enum/wire names. Java strings
require valid UTF-8; Java does not provide C++ binary byte-string or view semantics.
See [Java mappings, limits, and restrictions](java.md). JSON whitespace, escaping,
and floating-point spelling may differ while representing the same value. Named
enum fields use names in string-key binary; enum collection elements and union
payloads use compact numeric values. JSON always uses enum names.

This describes Serializer's implemented C++20 codecs. See the dated
[verification record](verification-2026-09-17.md) for tested revisions,
configurations, results, and outstanding checks.

Stream concepts and implicit iostream adapters preserve the protocol bytes defined
here. They add no transport framing, length prefix, protocol marker, or endian
marker. Byte-stream input is one EOF-delimited message and is checked for trailing
data; framed applications must bound each input message themselves. Memory input
may be borrowed and output may be drained in batches without changing its encoding.
See [stream adapters](usage.md#stream-concepts-and-implicit-adapters).

## Binary representation

Binary messages have no implicit version, size envelope, compression, alignment,
or padding. Applications must supply an exact input range and agree on the schema
and protocol. Values are emitted field by field into the output stream.

| Value | Representation |
| --- | --- |
| `char` | One byte |
| `bool` | One byte, `00` or `01`; other input values are rejected |
| Fixed-width integer | Declared width, least significant byte first by default; signed integers preserve their C++20 two's-complement representation |
| `float`, `double` | Supported binary IEC 559 32-bit or 64-bit representation, least significant byte first by default; bit-preserving conversion through an unsigned integer |
| String | Compact byte length followed by those bytes; binary string contents need not be UTF-8 |
| Vector | Compact element count followed by recursively encoded elements |
| Map | Compact entry count followed by each key and value; output follows `std::map` order |
| Numeric enum | Compact nonnegative underlying value; generated enum input/output rejects undeclared values |

Runtime SIMD and bulk array reads/writes preserve these exact representations. Eligible
fixed-width numeric arrays are copied or byte-swapped in blocks after the same
compact count prefix; JSON string scanning preserves validation and escaping.
SIMD introduces no padding, alignment, flags, or protocol marker. See
[runtime SIMD](runtime_simd.md) for coverage and verification scope.

Generated fixed-width field batching also preserves these bytes. C++ native binary
output reserves adjacent scalar fields together, including their existing keyed
headers, then encodes each value separately. Positional input can validate and
consume a whole group while retaining each value/byte work charge; scalar fallback
preserves input partial results and diagnostics. Keyed input still dispatches each
field. See [field batching](usage.md#batch-generated-fixed-width-fields) for scope.

Generated C++ constant field names also preserve wire bytes. JSON copies the same
quoted ASCII names while retaining formatter-controlled punctuation and spacing.
String-key binary copies the same canonical compact length and name together,
including within scalar batches. Names always use schema wire spelling, independent
of generated C++ coding profiles. See [constant field names](usage.md#pre-encode-constant-field-names)
for scope and the [verification record](verification-2026-09-17.md) for results.

Compact integers use the established two-bit length tag and six payload bits in
the first byte, followed by zero to three full payload bytes in big-endian order.

| Value range | Bytes | First-byte tag |
| --- | --- | --- |
| `0..0x3f` | 1 | `00` |
| `0x40..0x3fff` | 2 | `01` |
| `0x4000..0x3fffff` | 3 | `10` |
| `0x400000..0x3fffffff` | 4 | `11` |

Writers select the shortest representation and reject negatives and values above
`0x3fffffff` with `std::out_of_range` before writing that integer. Readers continue
to accept longer representations of the same value for compatibility. A failed
compact read leaves its cursor unchanged. Other failures can leave earlier
prefixes or values consumed or written.

### Byte order

`binary_none`, `binary_integer`, and `binary_string` default to **little-endian**
fixed-width integers and floating-point values on every supported host CPU.
For example, `uint32_t{0x12345678}` is stored as `78 56 34 12`, and `float{1.0}`
as `00 00 80 3f`. CPU-native byte order does not change this contract.

Each binary input/output protocol exposes `static constexpr std::endian wire_endian`.
An application that requires another byte order selects it at compile time:

```cpp
using big_endian_binary_out = rohit::serializer::binary<
    rohit::serializer::serialize_type::out,
    rohit::serializer::serialize_key_type::none,
    std::endian::big>;
static_assert(big_endian_binary_out::wire_endian == std::endian::big);
```

Only `std::endian::little` and `std::endian::big` are supported. This parameter
affects fixed-width scalars throughout nested objects and containers. Compact
lengths, counts, IDs, enum values, and union discriminators always use the compact
encoding described above, independently of fixed-width byte order. Strings remain
byte sequences; JSON has no numeric byte-order setting.

There is no implicit endian marker or auto-detection. Both endpoints must agree
on protocol, byte order, and schema. Applications requiring self-describing files
or messages must put that information in their own envelope. The public defaults
are intentionally changed to little-endian; no compatibility aliases or automatic
fallback decoding are provided.

Independent frozen fixtures in `test/wire_compatibility_test.cpp` preserve the
earlier big-endian and current little-endian contracts for all three binary key
modes. They check decoding against known values and encoding against literal
bytes, including nested fields, numeric arrays, floating-point values, compact
IDs, and a union. Selecting the wrong byte order can succeed with incorrect values;
the schema-language version cannot detect or resolve that mismatch.

### Object modes

- **Positional (`binary_none`):** base objects and fields follow schema order.
  No field key or object terminator is present. A union still has a compact
  zero-based alternative index before its payload.
- **Integer-key (`binary_integer`):** each base object or field starts with its
  compact ID. ID zero terminates the object. A union contains its field ID,
  compact alternative index, and payload.
- **String-key (`binary_string`):** each base object or field starts with its
  length-prefixed wire name. An empty name terminates the object. A union key is
  `field:alternative`. Ordinary generated enum fields retain named values in
  this mode; enum elements inside binary collections use numeric values.

Field/parent IDs must be in `1..0x3fffffff`; wire names must be nonempty. Key
overloads reject incompatible modes at compile time. General binary skipping is
unsupported: an unknown field's key provides neither its type nor its byte extent.

### Generated views

`view`, `readonly`, `mutable`, and `owning` select generated representations.
One enabled representation produces a concrete class; multiple representations
produce a template with the enabled `storage_mode` specializations.

Views map **little-endian `binary_none`** only. `map(span, limits)` validates an
exact message and records inline field offsets. Getters return scalar values,
borrowed strings, or nested views. Mutable setters preserve field and collection
extents; changing a string length, compact width, entry count, or active union
alternative requires rebuilding. No native C++ object layout is overlaid on bytes.
View map entries retain wire order and duplicate keys; keys remain immutable.
Array/map iterators advance through cached entry boundaries without changing the
wire layout or initial mapping validation. Size-preserving setters retain iterator
validity; replacing or relocating the underlying buffer invalidates borrowed access.
See [views.md](views.md) for API, lifetime, visibility, and nested-mode details.

## JSON representation

Outside strings, JSON whitespace consists only of space (`0x20`), tab (`0x09`),
line feed (`0x0a`), and carriage return (`0x0d`). C++ JSON and ProtoJSON readers
use bounded SIMD scans for long runs, with scalar short gaps and tails. This
requires no input padding and preserves input/work budgets and failure positions.
See [whitespace scanning](runtime_simd.md#simd-json-whitespace-scanning) for scope
and verification scope. No additional whitespace characters are accepted.

Strings and names are UTF-8. Output escapes quotes, backslashes, and all control
characters. Input decodes the JSON escapes and surrogate pairs and rejects invalid
UTF-8, unpaired surrogates, and unescaped controls. Ordinary unescaped names are
borrowed from the input for dispatch; escaped names use local scratch storage.
Names are compared as decoded bytes without Unicode normalization.

C++ JSON string helpers reuse escape boundaries from the full validation pass to
copy known plain spans without rescanning them. UTF-8/escape validation, emitted
bytes, string replacement, cursor updates, and resource charges remain unchanged.
The shared ProtoJSON input and Protobuf text quoting helpers use the same paths.
See [JSON scan reuse](usage.md#reduce-repeated-json-scans) for scope and recorded
verification. No new wire representation or trusted-input mode is introduced.

Numbers follow JSON grammar: no leading plus, leading zeroes, missing fractional
or exponent digits, `NaN`, or infinity. Booleans are lowercase. `std::nullptr_t`
represents `null`; a `char` represents a string containing one decoded byte.
Floating-point input uses `std::from_chars`; output uses shortest general-format
`std::to_chars` for round trips. Non-finite output is rejected, and numeric range
errors are reported rather than saturated or wrapped. These rules follow the
[JSON specification](https://www.rfc-editor.org/rfc/rfc8259.html), with the stated
Unicode and destination-type restrictions.

JSON maps retain the established array-of-entries representation:

```json
[{"key": 1, "value": "first"}, {"key": 2, "value": "second"}]
```

`format::compress` means compact JSON without optional whitespace. It is not a
compression codec. Optional [message compression](compression.md) wraps the entire
encoded message in a selected standard compression format; it leaves the inner
wire bytes unchanged and adds no Serializer envelope. Its whole-message overloads
validate both the compressed frame and the complete decoded message.

## Replacement and failure behavior

- Strings, vectors, and maps replace their previous contents. C++ JSON/native
  binary can reuse string/vector capacity, nested owning buffers, and old map
  nodes. Each incoming collection element is decoded into a fresh candidate;
  eligible old elements donate storage only for values present in the input.
  Unused old entries are destroyed. Eligible binary numeric vectors retain their
  clear/resize bulk copy or byte-swap path. Maps retain ordered semantics;
  duplicate keys keep the last complete entry, never a partially decoded candidate.
- Generated objects update fields in input order. Missing fields retain their
  destination values, so fresh construction supplies schema defaults. Duplicate
  fields apply again in order. A selected union alternative is default-constructed
  before decoding it; raw union alternatives must be trivially destructible.
- Objects inside replacement collections start from fresh schema defaults even
  when old storage is reused. Regenerated owning headers propagate typed donors
  through nested fields and parents without changing field selection or budgets.
  See [destination reuse](usage.md#reuse-destination-storage) for eligibility,
  memory tradeoffs, and verification scope. Java and the optional Protobuf
  codecs retain their separately documented replacement implementations.
- Decoding is **partial on failure**. Earlier fields, completed collection
  entries, consumed prefixes, and resource charges remain committed. A malformed
  scalar or JSON string is validated before assigning its destination, but memory
  allocation failures may leave a changed destination. There is no whole-object
  rollback. Decode into a fresh object and separate input view when the application
  needs an atomic commit, and account for that extra storage.
  The optional `deserialize_exact<Value, Protocol>(input[, limits])` helper
  constructs a fresh candidate, calls `finish()`, and returns only a complete
  validated value. It does not roll back input or make a later assignment atomic.
- Source bytes must remain alive and must not overlap destination storage that
  decoding can modify or reallocate. Do not mutate the input cursor outside an
  active decoder or share a decoder between threads. Decoders are noncopyable;
  nested generated readers share the same session by reference.
- Output values and their referenced storage must remain valid throughout encoding;
  they must not refer into an output buffer that encoding can overwrite or relocate.
  Stream-level overlap support does not make an entire multi-write protocol operation atomic.
- Generated fixed-width output groups validate their IDs/names and reserve the
  whole batch before writing. A validation/reservation failure leaves that batch
  unwritten; earlier output remains. The failure prefix can therefore differ from
  separate field writes. Object terminators are emitted separately. Stream policy
  overrides apply to the full batch reservation, including when capacity is available.
- An isolated pre-encoded binary name reserves its compact length and text together.
  Rejection writes neither part, retaining previous output. Its value and object
  terminator remain separate writes. This can change the failure prefix relative
  to dynamically encoded names, whose length and text are written separately.
- `serialize_in` consumes one value. Call `finish()` to require an exact message;
  JSON permits trailing whitespace, while binary requires the cursor at the end.
  Generated buffer convenience calls retain their one-value behavior; byte-stream
  calls validate one EOF-delimited message. The optional exact helper validates
  complete consumption for buffers too. See [exact decoding](usage.md#decode-one-exact-message-into-a-fresh-value).

## Resource limits

Construct the protocol with a copied `decode_limits` policy. Counters accumulate
for the lifetime of that protocol, including nested values and repeated reads.
Construct a new decoder to start a new message budget.

| Limit | Default |
| --- | --- |
| Consumed input bytes | 64 MiB |
| Decoded string or field-name bytes | 16 MiB |
| Elements/entries/fields in one collection or keyed object | 1,048,576 |
| Aggregate nesting depth | 64 |
| Cumulative accounted storage | 64 MiB |
| Work units | 134,217,728 |

Work units cover consumed bytes, decoded values, aggregate entries, and declared
binary collection slots; they are a deterministic work bound, not CPU cycles.
Bulk binary-array decoding retains every scalar-path charge. When the work budget
would run out during an array, the scalar loop preserves its partial result and
failure position; complete payload checks still precede destination changes.
Storage accounting covers decoded strings, vector element storage (including
requested JSON growth slack), and map values plus a node-link allowance. It is
charged even for logical storage that reuses existing capacity. It is not an
exact heap/RSS limit: allocator metadata, allocator over-allocation, stack objects,
custom user types, and old storage retained temporarily as donors require separate
application accounting. JSON string scans
are bounded by the remaining input and work limits before any allocation.

```cpp
auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
rohit::serializer::decode_limits limits{};
limits.max_input_bytes = 1024 * 1024;
limits.max_collection_elements = 4096;
limits.max_nesting_depth = 16;
rohit::serializer::json<rohit::serializer::serialize_type::in> decoder{input, limits};
decoder.serialize_in(destination);
decoder.finish();
```

Handwritten positional serializers must hold `decoder.enter_object()` while
reading nested fields to participate in nesting limits. Handwritten operations
that bypass protocol reads also bypass its budgets.

## Schema evolution

Use `stable_ids` for every class that participates in an evolving integer-key
schema. Every member and parent then requires an explicit ID:

```text
class person stable_ids {
  public string name ("fullname", 3);
  public uint64 identifier (4);
}
```

Parsing rejects duplicate IDs, including explicit/implicit collisions, duplicate
wire names, empty names, out-of-range IDs, and duplicate enum/union alternatives.
Implicit IDs retain their existing declaration-order assignment. The generator
groups colliding hashes and verifies the complete name before selecting a field
or enum. Hashes are internal lookup aids and are not wire identifiers.

- Never reuse a removed field ID or wire name for a different meaning. Keep an
  application schema history. The separate [compatibility checker](schema_evolution.md)
  compares revisions and enforces a persistent reservation policy; ordinary
  parsing and `stable_ids` alone do not check that history.
- A renamed C++ member can retain compatibility by preserving its explicit ID
  and wire-name override. Changing a field type or width requires a coordinated
  schema/version change.
- Enum values and union indices are declaration-order values. Append new
  alternatives without renumbering old ones; retain reserved old alternatives
  instead of reusing their numbers. Unknown alternatives are rejected.
- Native keyed readers reject unknown fields. Old readers therefore do not automatically
  accept new fields. Negotiate versions out of band or use distinct message
  contracts. Positional readers require the same field order and types.
- Missing fields retain destination values as described above. Adding a field
  can be backward-readable by a newer keyed reader initialized with a default;
  it does not make an older reader accept a newer message.

Native keyed binary lacks the type/extent information needed to skip an unknown
field. Adding it requires a separately versioned format and explicit agreement;
no such format is introduced by compatibility checking. Existing C++ Protobuf
binary can skip unknown fields under its [documented limitations](protobuf.md).
Neither the schema-language version nor `stable_ids` identifies a message wire version.

## Diagnostics

`rohit::exception::base_parser::code()` exposes `invalid_input`, `invalid_type`,
`unknown_field`, `numeric_range`, or `resource_limit`. Messages omit payload excerpts
by default. `decode_limits::diagnostics` can opt into an escaped excerpt, bounded
by the requested byte count and an absolute 256-source-byte cap. Handwritten
exceptions can pass the same `diagnostic_options` to their constructor. Explicit
application-supplied error text is not automatically redacted.

This is an implementation contract, not a completed security or portability
qualification. See [the prepared validation targets](../qualification/README.md).

## Portable owning implementations

The C++ compiler generates native owning codecs for Java, JavaScript, Go, and C#
using these existing four protocols. The new JS/Go/C# decoders require exact
consumption and retain the documented missing/duplicate-field semantics. Their
limits match the portable Java policy rather than C++ storage/work accounting.
All portable strings require valid UTF-8 and JSON characters must be ASCII.
JavaScript represents 64-bit fields with `bigint` and preserves full decimal JSON
integer tokens. Binary map output is explicitly sorted; JSON text escaping can
vary without changing values. See [portable language mappings and limits](portable_languages.md)
and the [all-pairs verification](verification-multilanguage-2026-09-17.md).
