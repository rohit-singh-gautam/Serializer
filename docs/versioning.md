# Schema revisions and compatibility

Every schema still begins with `serializer version 1;`. That declaration selects the schema language. A class member declared with `version` is a separate **payload revision**, serialized with the object.

```text
class example_model stable_ids {
  public version { 10 } compatibility { 8 };
  public uint64 id (2);
  created(9) public bool enabled (3) { true };
  obsolete(10) public string old_name (4);
  created(10) replaced(old_name) public string name (5) { "new" };
  reserve id {7, 9} variable {retired} display {"retired_name"};
}
```

One class may declare one version member, with `public`, `protected`, or `private` access. Its initializer is required and defines the current revision. `compatibility {8}` defines the inclusive minimum; omitted compatibility defaults to the current revision. A writer can emit any revision in this interval by setting the member. The member defaults to type `uint8`, name/wire name `version`, and ID 1. When that ID is occupied or reserved, the compiler selects the first available ID. Specify an ID explicitly when a durable contract must freeze that choice; changing the selected default ID changes the wire identity.

```text
public version { 1 };
public version version { 1 };
public version uint32 version { 70000 };
public version uint16 ver { 300 };
public version version (5) { 1 };
public version version3 ver ("schema_version", 6) { "1.10.0" } compatibility { "1.2.0" };
```

| Schema type | Binary discriminator | JSON discriminator |
| --- | --- | --- |
| `uint8`, `uint16`, `uint32`, `uint64` | Declared unsigned width | Number |
| `float`, `double` | IEEE binary32/binary64 | Finite nonnegative number |
| `version2`, `version3`, `version4` | Exactly 2/3/4 uint16 components, without a count | Canonical quoted dotted string |

`uint34` is not a supported width; use `uint32` or `uint64`. Dotted components are decimal integers in 0..65,535 with no leading zeros. Their count is fixed by the type: `"1.2"`, `"1.2.3"`, or `"1.2.3.4"`. Comparisons are numeric and lexicographic by component: `1.10.0` follows `1.2.0`. C++ provides `rohit::serializer::version2`, `version3`, and `version4`; the other generated APIs use canonical strings. Floating revisions use the declared precision and exact comparisons; integral or dotted revisions are easier to maintain for discrete releases.

## Field lifetimes, replacements, and reservations

Place lifecycle annotations before the access modifier. `created(V)` includes the field from V onwards; `obsolete(V)` excludes it from V onwards. The active interval is `[created, obsolete)`. Boundaries use the class revision type, cannot exceed the current revision, and must form a nonempty interval. Fields with neither boundary remain active throughout the supported history. Bare `obsolete` preserves the field and records deprecation metadata without changing the wire layout; give a boundary to remove it from later payloads.

Keep obsolete definitions, IDs, names, types, defaults, and relative order intact while their versions remain supported. They remain generated fields so the application can read and convert historical data. A decoded newer model supplies schema defaults for inactive fields. Encoding an older revision includes its original fields and omits later additions.

`created(V) replaced(old)` associates a successor with a retained predecessor whose `obsolete(V)` boundary matches. Each predecessor has one successor. This metadata validates the contract; conversion remains explicit application code, especially for changes such as uint64 to float. The successor uses a new identity. For example, after reading revision 8, copy `old_name` into `name`, then set `version = 10` before writing.

`reserve` permanently protects unused IDs, variable names, and wire display names in a class. Any subset of `id`, `variable`, and `display` is allowed, with nonempty comma-separated lists. Generation rejects collisions and reuse. Do not reserve an identity while its original definition is still present; preserve that definition until the historical revision is no longer supported. The compatibility checker combines embedded ID/display reservations with its external reservation configuration.

## Read policies

| Policy | Payload revision | Unknown JSON fields | Unknown binary fields |
| --- | --- | --- | --- |
| `strict` | Current revision only | Reject | Reject |
| `compatible` | Inclusive minimum through current | Reject | Reject |
| `flexible` | Inclusive minimum through current | Skip validated bounded JSON values | Reject |

All modes reject missing/duplicate version discriminators, nonfinite/invalid revisions, unsupported revisions, and known fields outside their lifecycle. Omitted ordinary keyed fields retain their schema defaults. Compatible mode describes declared historical layouts; it does not guess arbitrary formats. Flexible JSON validates skipped values, nesting, UTF-8 and resource limits. Future revisions remain unsupported even with flexible JSON.

C++ replaces `json_read_policy` with `read_policy`; the former unknown-JSON-field skipping behavior is `read_policy::flexible`. Supply it as the JSON template's third parameter or a binary alias's fourth parameter:

```cpp
template <rohit::serializer::serialize_type Direction>
using compatible_json = rohit::serializer::json<Direction, rohit::stream,
    rohit::serializer::read_policy::compatible>;
template <rohit::serializer::serialize_type Direction>
using compatible_binary = rohit::serializer::binary_none<Direction, rohit::stream,
    rohit::serializer::binary_text_validation::strict,
    rohit::serializer::read_policy::compatible>;
```

Other languages select `ReadPolicy` through the generated `Limits` object; C uses `srl_read_policy`, and Rust uses `ReadPolicy::Strict/Compatible/Flexible`. Defaults are strict. See the [language examples](../example/README.md#versioning) for the precise call in each language.

## Positional binary and performance

Versioned positional objects always write/read the discriminator first, before parents and other fields, even when the schema declaration occurs later. Its width is fixed by the schema; a reader must know that type and ID. Remaining fields keep declaration order and use the layout active at the decoded revision. Nested versioned objects carry their own discriminator. Keyed formats recognize the version key in any input order.

An unversioned class retains the original positional bytes and direct codec path. Version metadata, presence tracking, and lifecycle branches are generated only for versioned classes; C++ retains fixed-field batching on unversioned classes. Reservations and bare obsolete metadata add no wire bytes. No performance benchmark should be inferred from wire-layout regression tests.

A payload written before any version member existed has no discriminator. Decode it with its original unversioned schema and explicitly migrate it; adding a member does not make legacy bytes self-describing. Evolving the discriminator's type/identity or changing retained historical field layouts is incompatible.

## Support and runnable examples

Unmanaged owning models support this feature in C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C with JSON, positional binary, integer-key binary, and string-key binary. Versioned buffer views, managed models, and C++ Protobuf/ProtoJSON/TextProto mapping currently produce an unsupported diagnostic. Generic concrete instances retain revision metadata.

Run the [shared versioning schema](../example/schemas/versioning/model.serializer) and handwritten consumers:

```sh
python example/run.py --compiler build/serializer --example versioning --language all
```

The examples read revision 8, preserve its positional layout, explicitly migrate the replacement, and round-trip revision 10 using all four protocols. Every consumer also verifies eight explicit revision types against frozen positional bytes, including binary32 `0.1` and the maximum uint64 value. The schema also demonstrates each supported revision type and access/name/ID choices. Generated output stays in the build tree. See [usage](usage.md) and [wire format](wire_format.md) for general codec and decoding contracts.

See the [verification record](verification-versioning-2026-10-06.md) for completed checks and remaining coverage. Release dates and time-based expiry policies are proposals and are not implemented syntax.
