# Wire format and decoding contract

The schema language header is compiler input metadata and is never serialized.
`serializer version 1;` and `serializer version 1.0.0;` select the same contract
and produce identical codecs. Schema-language and compiler release versions are
independent of the application payload revisions described below.
Language `1.1.0` adds inferred fixed-array extents and typed magic; selecting a
newer header alone does not change existing declarations' bytes or public APIs.

Language `1.2.0` adds [compact unsigned scalar payloads](compact_integers.md).
`compact_prefix` uses the existing two-MSB byte-count tag and 30-bit maximum;
`compact_varint` uses canonical unsigned LEB128 with seven payload bits per
byte and a continuation MSB. Only annotated owning uint8/16/32/64 fields change
their native binary payload encoding. Keys, counts, enums and union indices
retain their existing prefix encoding. JSON and optional Protobuf scalar mappings
remain unchanged. Strict prefix output rejects overflow; lenient output retains
the low 30 bits. Every reader checks bounds and declared scalar width; varint
readers also reject overlong, overflowing and unterminated encodings.

Language `1.3.0` adds [digest byte fields](digest.md). The four native protocols
encode them as ordinary `uint8` sequences: compact element count followed by raw
bytes in binary, and numeric byte arrays in JSON. Fixed `digest[N]` and named
`digest(algorithm)` retain the count and require exact cardinality. The algorithm
name adds no wire tag and is not verified against a message's content. Bare
`digest` has no implied algorithm or size. Existing schema types named `digest`
retain their prior meaning. Views and optional Protobuf digest mappings reject.

Language `1.5.0` adds `ignore(warning magic, version)` for parser simplicity
diagnostics. These suppressions are compiler input metadata, are never serialized,
and leave generated APIs, field identities, storage and bytes unchanged. Omitting
the explicit default name from `public version uint32 version (1) { 1 };` likewise
preserves the member and wire name `version`. Replacing an ordinary field with
`magic` or `version` can change the layout and must be reviewed separately; warnings
do not perform that migration. See [simplicity warnings](schema_reference.md#simplicity-warnings).

See [payload versioning](versioning.md) for revision types, historical positional
layouts, field lifetimes, replacements, reservations, common read policies, and
compiler-only release-date/count policies. These resolve to ordinary version bounds
and add no dates or policy evaluation to generated codecs.

[Fixed magic and format exclusions](magic_and_omission.md) are opt-in owning
schema annotations. Native binary formats prepend the exact magic bytes before
the version and other object fields, with no length, key or NUL terminator.
Native JSON includes one required matching `magic` value unless explicitly
excluded. Legacy byte magic is a UTF-8 string. Language `1.1.0` typed magic uses
the selected native scalar/enum encoding, with no additional magic key, field ID,
or sequence count in binary. Enum magic uses compact ordinals in positional and
integer-key binary; string-key binary uses the usual length-prefixed enum name.
JSON uses its ordinary scalar or enum-name representation. Protobuf uses the
declared magic field ID and corresponding scalar/enum mapping; legacy magic
remains a fixed string. All readers validate and discard the constant.

`omit(format, ...)` removes the field's bytes, key and positional slot only from
the selected representation. Decoding preserves its schema default and rejects
an explicitly supplied excluded known keyed field. C++ selects these layouts
at compile time. The compatibility checker compares magic and omissions per
format; migrating an existing artifact requires agreed schemas for both peers.
Unmarked classes retain their existing layout. `binary_positional` is an exact
alias of `binary_none`.

[Schema generics](generics.md) are expanded before codec generation. Each concrete
application encodes exactly like an equivalent ordinary class; no parameter names,
type tags, or generic envelope bytes are added. Both peers must agree on the
concrete type. Substitution preserves declared IDs, wire names, order, and defaults;
changing an argument is subject to the existing compatibility rules.

The optional [C++ managed runtime](managed/cpp_runtime.md) uses generated
[envelope schemas](../schemas/managed_records.serializer), encoded with the existing
`binary_integer` protocol. They add document/schema identity, object-ID allocation
marks, and snapshot history. Loading requires matching schema, ID width, and
compile-time history mode and label policy; the wire mode cannot switch the store specialization.

Labels are compile-time disabled by default. Such stores use format version 3:
`records::unlabeled_envelope` for tree, `records::unlabeled_state_envelope` for
linear/disabled. Unlabeled tree revisions omit label key 3, keeping number key 1,
parent key 2, and snapshot key 4. Unlabeled linear entries omit label key 1,
keeping snapshot key 2. Omitted keys are not reused. There is no label field,
not even an empty-string placeholder.

With `history_labels::enabled`, tree stores retain the version-one
`records::envelope` with numbered revisions,
parent links, current revision, and revision high-water mark. Tree records may
arrive in any order but must form one valid rooted tree with a matching current
snapshot. Revision IDs are not reused when reloading an older live-document save.

Label-enabled linear stores use version-two `records::state_envelope`. Common fields
keep keys 1-6, 9, and 10. Keys 7, 8, and 11 from the tree envelope are not reused.
Field 12 is an ordered array of entries (`history_entry` with label key 1 and
snapshot key 2 when enabled, `unlabeled_history_entry` with snapshot key 2 otherwise);
field 13 is a zero-based uint64 cursor. Linear envelopes require a nonempty array,
a cursor within bounds, and an entry at that index equal to `current_snapshot`.
All historical models and count/byte limits are validated before publication.
Order is meaningful and is never sorted by the loader. No revision IDs, parents,
or revision high-water marks are present. Disabled envelopes require empty entries
and cursor zero. All other validation below applies to both label policies.
Version-one linear/disabled saves and cross-label-policy loads are rejected; no
automatic migration is provided. Persistent object IDs and their high-water marks are
preserved independently of history. Over-budget imports fail without pruning.

The [version-two journal container](managed/journal.md#version-two-file-contract)
uses these unchanged envelopes for its base. Subsequent frames contain one existing
serialized root snapshot or a small control record. Length/checksum framing,
implicit sequence chaining, commit CRCs, and base/sidecar generation binding protect
recovery. It is a separate file format,
not trailing bytes accepted by ordinary exact-message decoding. Collaboration
wire protocols remain proposals.

The file-stream/journal-stream refactor preserves the managed journal version-two
framing byte for byte. Memory buffers, standard byte streams, and `file_stream`
share the same record writer/reader; synchronization is an explicit backend
capability, not an additional serialized field.

Schemas without managed declarations retain their codecs.
Default managed classes serialize `persistent_id` under reserved integer key
`1073741823` (`0x3fffffff`) or string key `persistent_id`, after their application
fields in positional codecs. Application field IDs are not shifted. A conflicting
metadata key/name is rejected. With `[managed] separate_values = true`, generated
companions instead encode identity at field 1 and payload at field 2; ordinary
payloads keep their ID-free representation. The direct and separated modes use
different schema bindings, so envelopes are not interchangeable. The document
namespace is generated automatically for new stores, while object IDs start at 1
and increment within that document. Saved namespaces and allocation high-water
marks are restored during load; deletion and undo never renumber surviving objects.
Direct managed Protobuf output is currently unsupported.

The opt-in C++ `protobuf_binary`, `protojson`, and `textproto` protocols have a
separate [Protobuf mapping and decoding contract](protobuf.md). They do not use
the custom binary layouts or ordinary JSON mapping documented below. Java
currently supports only the four original protocols described here.

Rust, Python, Swift, Kotlin, and C implement these same four native protocols for
their supported owning types, alongside JS/TypeScript and Go/C#. They preserve
integer widths, little-endian scalars, compact prefixes, IDs, and enum contexts.
Portable strings require valid UTF-8. Swift uses `WireString` map keys to keep
differently encoded Unicode sequences distinct. See [native runtime contracts](native_languages.md)
for ownership, exact-decode failure handling, limits, and target-specific APIs.
The [full interoperability test](../example/interoperability/README.md) checks all
producer/consumer directions; this adds no new wire values or schema version.

The `.serializer` source header `serializer version 1;` selects the schema
language. It is independent of the compiler release and is not emitted into
messages. It identifies neither message byte order nor a wire-format version; see
[compiler and schema versions](command_line.md).

The pure Java backend implements the same four protocols for its supported
owning types. Binary fixed-width values are little-endian, IDs and compact
prefixes are unchanged, and Java retains original enum/wire names. Java strings
require valid UTF-8, as do the C++ codecs. Java does not provide borrowed views.
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
| String | Compact UTF-8 byte length followed by valid UTF-8 bytes; embedded NULs are preserved |
| Vector | Compact element count followed by recursively encoded elements |
| Map | Compact entry count followed by each key and value; C++ output follows `std::map` order; see [map ordering](#map-ordering) |
| Numeric enum | Compact nonnegative underlying value; generated enum input/output rejects undeclared values |

### Map ordering

Native maps decode entries in either key order; owning destinations retain the
last complete value for duplicate keys. Byte order for numeric scalars does not
determine the order of map entries. In particular, schema `char` is stored as
plain `char` in C++ and `byte` in Java: C++ map order depends on compiler char
signedness, and Java sorts signed byte values. Other generated backends order
these keys as unsigned bytes. With keys `1` and `255`, signed-char C++ and Java
write `255` first, while unsigned-char C++ and the unsigned-byte backends write
`1` first. Both messages represent the same map.

This historical ordering is preserved in all three native binary protocols.
Do not treat native serialization as a universal canonical encoding for hashes
or signatures. Prefer `map(uint8)` for new portable byte-keyed contracts requiring
consistent unsigned ordering. Changing an existing `char` key to `uint8` is a
schema migration; JSON character keys and integer keys have different representations.
JSON `char` remains restricted to ASCII, where the signedness difference does not arise.

### Codec implementation

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
encoding described above, independently of fixed-width byte order. UTF-8 text is
unchanged; JSON has no numeric byte-order setting. Explicit big-endian wire
selection is currently available in C++; other generated backends implement the
little-endian wire profile regardless of their machine's native byte order.

For example, `uint32{0x12345678}` remains `78 56 34 12` with little-endian wire
configuration on both x86-64 and big-endian s390x. Changing the machine does not
change the schema or require changing the wire setting. The interoperability
runner's `--big-endian` option executes s390x C/C++ producers and consumers under
QEMU alongside the other languages. Every positional producer is also checked
against `test/positional_binary_fixture.json`, specified independently of the
generated codecs. See [the runner](../example/interoperability/README.md).

By default, binary string writers and readers reject invalid UTF-8, including overlong
sequences, surrogate code points, and truncated multibyte sequences. C++ owning
string decoding leaves that destination unchanged on this validation failure;
view mapping validates strings without allocation, and view setters validate
before modifying the backing bytes. No Unicode normalization is performed.
Use `array uint8` for arbitrary payload bytes. This tightens the former C++-only
raw-string behavior without changing valid UTF-8 wire bytes; see
[migration](../migration.md#binary-string-validation).

C++ owning binary codecs accept the compile-time policy
`binary_text_validation::unchecked` after the existing stream template argument.
It omits runtime text checks, including dynamic field names, while retaining
bounds, resource budgets, and all other decoding rules. Constant names still
validate during compilation. It changes neither valid wire bytes nor endian
selection: valid text interoperates with strict readers. Invalid text remains
outside this contract even when a C++ caller opts out. JSON, generated mapped
views, and other runtimes remain strict. See [policy usage](usage.md#choose-c-binary-text-validation).

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
  Language `1.4.0` `variant(...)` fields use identical indices, field IDs, and
  colon-qualified alternative names in all four native protocols. Owning C++
  variants destroy the old value when constructing the selected alternative;
  invalid numeric indices are rejected before replacing it. See
  [unions and variants](unions_and_variants.md) for language mappings and APIs.
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
application accounting. Protobuf text token/numeric scratch is also outside this
storage budget; bound it with input/work limits and TextProto's token-length limit.
TextProto decoded strings are charged before each append, including Unicode
expansion and concatenated quoted fragments. JSON string scans
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

## Collaboration envelopes

The optional C++ collaboration protocol uses the ordinary `binary_integer` codec
over [generated coordination records](../schemas/collaboration_records.serializer).
Its envelope binds protocol version, model schema ID, persistent-ID width,
document namespace, and authority epoch. Default session IDs and all operation,
sequence, target, and grant counters are uint64 even for uint32 model IDs; snapshots retain the model's
configured ID width. Accepted records contain the current model and allocation
watermark, plus one transaction's field/dependency history metadata. Lock/presence actions
have explicit values in schema comments and matching runtime enums. Authority-local
lease deadlines are not serialized. Exact decoding does not establish trusted
session or authority identity. See the [runtime contract](managed/collaboration_runtime.md)
for ordering, retry semantics, persistence boundaries, and limits. Existing codec
and saved-document formats are unchanged. History-bearing coordination messages use
new protocol bindings because native keyed readers reject added fields.

Protocol version 5 retains opaque application command payloads for custom authority
handlers. The default authority uses protocol version 6: `change_proposal.command`
encodes `model_change` with format version 1 (field ID 1), the base allocation
watermark (2), candidate allocation watermark (3), and complete candidate snapshot
(4). The outer accepted base sequence and inner base allocation watermark must
both match authority state. The allocation range is bounded by `max_created_ids`
and the configured model ID width; the encoded payload and snapshot have separate
byte limits. Identity validation, final-diff policy checks, and lock validation
precede publication. Protocol contexts prevent these payloads from being mistaken
for custom commands. Existing envelope field IDs are unchanged.

Application-defined session types use protocol **7** for custom commands or **8**
for model proposals. The C++ typed records use the generated `session_envelope`:

| Field ID | Meaning |
| --- | --- |
| 1 | `session_format`: stable, nonempty session codec name |
| 2 | `payload`: exact binary encoding of the corresponding existing record |
| 3 | `sessions`: ordered `session_value` records, each with encoded ID bytes at field 1 |

Every numeric session field inside the payload is zero. A proposal, acceptance,
lock request, grant, presence record, or lock update has exactly one session value;
a lock snapshot has one per grant, in the same order, including repeated owners.
Nonzero numeric placeholders and mismatched counts are rejected. The typed decoder
checks the expected session format, per-ID byte limits, canonical re-encoding, and
exact consumption of both envelope and payload. Caller decode limits also apply
to nested decoding. Use the public collaboration codec helpers for typed records.

The supplied codecs use `serializer.session.uint8.v1`, `uint16.v1`, `uint32.v1`,
`uint64.v1` (each with the same `serializer.session.` prefix), or
`serializer.session.string.v1`. Numeric values use the ordinary binary scalar
encoding at their declared width. Strings use the ordinary UTF-8 binary string encoding
and allow at most 4096 encoded bytes by default. Custom policies supply their own
stable name and bounded codec. Protocols 5/6 use generated records directly for
the default uint64 policy. Protocols 1..4 remain reserved for the previous records;
this runtime rejects their context bindings. Join snapshots have operation 0 and a default-constructed session
value, which carries no authored-edit origin.

### Collaborative history records

`model_change` additionally reserves fields 5 (`history_action`) and 6
(`target_operation`). Ordinary format-1 edits require both to be zero.
Format-2 history requests require empty snapshot bytes and zero allocation fields,
action 2 (undo) or 3 (redo), and an optional target operation in the requesting
session. Zero selects the current stack tip, which must be visible at the request base sequence. The authority uses its own
retained values and dependencies for the inverse. A request may have a stale base
sequence only when the target was already visible and its dependencies still match.

Field 7 of `accepted_change` is `transaction_record`: action (1), target operation
(2), `field_change` array (3), and `entity_dependency` array (4). Action values are
1 edit, 2 undo and 3 redo. The outer session/operation/sequence supplies author,
transaction grouping and accepted order; there is no second author identity.
Join snapshots contain the default empty record.

`field_change` fields are entity ID (1), schema field ID (2), before/after presence
(3/4), before/after encoded values (5/6), and before/after contribution versions
(7/8). Values use the existing generic `binary_integer` codec. Managed ownership
encodes a child ID, map of application key to uint64 child ID, or array of uint64
child IDs; descendant payloads are addressed separately. Ordinary fields are
atomic values, even when they contain nested objects or collections. Absent values
have false presence and empty bytes. `entity_dependency` holds entity ID (1) and
before/after attachment versions (2/3). Versions are contribution tokens, restored
by inverse operations; the outer accepted sequence remains strictly monotonic.

An authority may retain unchanged fields as guards for relocated subtrees. These
records describe acceptance and do not authorize clients to patch a model directly.
Both peers must upgrade from protocols 1..4. Ordinary document envelope versions
remain unchanged; journal restoration uses the new [operation tag 5](managed/journal.md#version-two-file-contract).

### Durable collaboration client records

Store clients use separate generated records; authority messages and protocol
versions 5/6/7/8 are unchanged. The native journal frame payload is a bounded
`binary_integer` `client_checkpoint` with kind (field 1), model bytes (2),
`client_state` (3), and binding string `serializer.collaboration.client.v1` (4).
Kind 1 contains a complete native managed envelope; kind 2 contains a native journal
operation, possibly empty. Kind is a wrapper discriminator, not a native journal
operation tag. Existing file framing/CRC/flush rules enclose the entire pair.
Ordinary model readers reject this wrapper. `save()` emits a full kind-1 checkpoint.

`client_state` field IDs are format version **1** (1), session policy name (2),
canonical session bytes (3), authority domain (4), acknowledged model in local IDs
(5), accepted receive cursor (6), authority allocation watermark (7), last reserved
operation number (8), identity mappings (9), transaction records (10), pending FIFO
(11), local undo stack (12), redo stack (13), exact in-flight proposal bytes (14),
provisional ID mappings (15), and synchronization interval in milliseconds (16).
Stacks and queues reference one-based transaction numbers. Identity mappings hold
local uint64 (1) and authoritative uint64 (2), validated against the model ID width.
The operation watermark includes IDs reserved for host lock requests through
`session.reserve_operation_id()`, as well as document submissions. Reservations
may leave gaps and need no transaction entry. This uses the existing field 8;
the checkpoint format is unchanged. Host lock request payloads are not stored in
the document outbox merely by reserving an ID.

`client_transaction` fields are local number (1), history action (2), local target
number (3), before/after model bytes (4/5), authority operation/epoch (6/7), status
(8), grant references (9), invalidated flag (10), label (11), and accepted sequence
(12). Actions retain 1 edit, 2 undo, 3 redo. Status values are 1 queued, 2 accepted,
3 conflict, 4 denied, 5 obsolete grant, 6 failed, 7 uncertain, 8 discarded. Accepted
sequence may lead the receive cursor when sending and receiving separately.

Local model IDs are stable independently of authoritative IDs. Translation applies
only to generated persistent identity slots; ordinary scalar fields and map keys
retain application values. Persisted exact requests are retried only in their own
authority epoch. Client history durability does not restore authority deduplication
or grant/version tables. See [local collaboration](managed/local_collaboration.md)
for recovery prerequisites, validation, budgets and explicit conflict resolution.

### History-free collaboration checkpoints

`pending_client_checkpoint` retains wrapper IDs 1..4 but uses binding
`serializer.collaboration.pending.v1` and `pending_client_state`. Its state retains
client-state field IDs 1..11 and 14..16, omits undo/redo fields 12/13, and adds the
monotonic local transaction watermark at field 17. State format version remains 1
within this distinct binding. `pending_client_transaction` retains IDs 1, 4..10 and
12 from `client_transaction`; history action, target and label fields 2/3/11 are absent.
Transactions are sorted by their nonzero local number, which can have gaps after
compaction; pending FIFO entries reference these numbers, not vector positions.
Acknowledged transactions remain until their accepted sequence is received, then are
removed; outstanding and uncertain work remains. Recovery checks the monotonic watermark
instead of requiring the transaction array length to grow forever.

History-enabled checkpoint bytes, ordinary model envelopes, native journal framing and
authority protocol versions are unchanged. History-free authorities use edit action 1
and compact field/ownership conflict addresses; they omit inverse values/version history
and reject history requests. Clients must not infer undo support from the protocol
version alone. Checkpoint policy changes require explicit migration; readers do not
silently drop archived transactions or inverse history.

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

## Fixed owning arrays and generic templates

Generic type/value arguments add no wire metadata. Fixed arrays keep the ordinary
sequence representation and count in all four native protocols. A present fixed
array must contain exactly its schema extent. Binary readers check the count
before elements; JSON bounds iteration and rejects short/extra elements. Missing
keyed fields retain destination/default values; an explicit empty sequence is not
a missing field. Runtime budgets and endian selection are unchanged. Direct
Protobuf generation of fixed arrays is explicitly rejected. From compiler 1.6.0,
JavaScript/TypeScript and Python fixed arrays without initializers use the same
counts and exact-cardinality checks as native C++; their owning arrays/lists are
initialized to the extent. Other portable backends remain unsupported. See
[generic contracts](generics.md) and [qualification](verification-dimensions-2026-10-04.md).

## Compatible native JSON input

The opt-in `read_policy::flexible` reader ignores additional keyed
fields, rejects duplicate object keys, and validates skipped values under the
same resource limits as typed fields. Default native JSON still rejects unknown
fields. C++ optional host values encode as JSON `null` or their contained value;
native binary formats are unchanged. See [usage](usage.md#json-compatibility).
