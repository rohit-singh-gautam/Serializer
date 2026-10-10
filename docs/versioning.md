# Schema revisions and compatibility

Every schema begins with a supported schema-language header. Use
`serializer version 1.6.0;` for new schemas; older `1.0.0`, `1.1.0`, `1.2.0`, `1.3.0`, `1.4.0`, and `1.5.0`
headers remain supported. The original `serializer version 1;` is an exact alias
for `1.0.0`, not the latest language version. See the
[schema-language policy](command_line.md#schema-language-version-policy).
A class member declared with `version` is a separate **payload revision**,
serialized with the object; neither version selects the compiler/runtime release.

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

One class may declare one version member, with `public`, `protected`, or `private` access. Its initializer is required and defines the current revision. `compatibility {8}` defines the inclusive minimum; omitted compatibility defaults to the current revision unless a release policy supplies a floor. A writer can emit any revision in this interval by setting the member. The member defaults to type `uint8`, name/wire name `version`, and ID 1. When that ID is occupied or reserved, the compiler selects the first available ID. Specify an ID explicitly when a durable contract must freeze that choice; changing the selected default ID changes the wire identity.

```text
public version { 1 };
public version uint32 { 70000 };
public version uint16 ver { 300 };
public version (5) { 1 };
public version version3 ver ("schema_version", 6) { "1.10.0" } compatibility { "1.2.0" };
```

The default member name remains `version` when omitted; access, generated APIs,
wire name and ID remain the same. Custom names such as `ver` and `revision` are
intentional. Explicitly repeating `version` is still accepted, but produces a
`[simplicity-version]` warning. To demonstrate or retain that spelling deliberately,
use language `1.5.0` and `public version version { 1 } ignore(warning version);`.
Ordinary supported scalar fields named `version` or `revision` also receive the
advisory suggestion to use `version`; see [simplicity warnings](schema_reference.md#simplicity-warnings).

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

Unmanaged owning models support this feature in C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C with JSON, positional binary, integer-key binary, and string-key binary. C++ managed owning models also support payload revisions from compiler/runtime 1.11.0. Versioned buffer views and C++ Protobuf/ProtoJSON/TextProto mapping remain unsupported. Generic concrete instances retain revision metadata.

Run the [shared versioning schema](../example/schemas/versioning/model.serializer) and handwritten consumers:

```sh
python example/run.py --compiler build/serializer --example versioning --language all
```

The examples read revision 8, preserve its positional layout, explicitly migrate the replacement, and round-trip revision 10 using all four protocols. Every consumer also verifies eight explicit revision types against frozen positional bytes, including binary32 `0.1` and the maximum uint64 value. The schema also demonstrates each supported revision type and access/name/ID choices. Generated output stays in the build tree. See [usage](usage.md) and [wire format](wire_format.md) for general codec and decoding contracts.

See the [payload-versioning record](verification-versioning-2026-10-06.md) and
[release-policy record](verification-release-policies-2026-10-06.md) for completed
checks and remaining coverage.

## Managed payload revisions

```text
serializer version 1.6.0;
class document stable_ids managed {
  public version {2} compatibility {1};
  obsolete(2) public string old_title (2);
  created(2) replaced(old_title) public string title (3) {"untitled"};
}
```

The generated owning codecs retain the ordinary revision contract. Editors reject
inactive fields and expose no version setter: changing the discriminator is a
schema conversion, not an ordinary durable field edit. Inactive managed children
and alternatives do not participate in live identity, collaboration or history
field traversal. Active children retain their identities through undo and Save/load.

A payload revision describes one object. A managed Save also contains a schema
binding and every retained snapshot. Adding a field or changing lifecycle metadata
can change that binding; accepting an ordinary old payload does not automatically
upgrade a whole document. `load_migrated<SourceRoot>(bytes, converter)` converts the
current payload and all retained history, preserving document namespace, identities,
logical type keys, labels and history structure. Conversion must preserve identity
width and validate every resulting snapshot before publication.
`recover_migrated_journal` writes a separate destination journal after recovery;
old and new schema records are not appended to one journal. Attached collaboration
requires coordinated upgrade/resynchronization, not live per-message conversion.
See [state enhancements](state_enhancements.md) and the
[managed document example](../example/managed/document_features/README.md).

Compiler 1.11 includes omission and lifecycle metadata in managed fingerprints.
Some unchanged older schemas therefore receive a new binding: omitted durable
fields and ordinary versioned children are affected. Strict `load` rejects the
old binding. Supply the exact historical traits/schema ID to `load_migrated` and
an explicit converter for every retained value; even an identity converter is an
application decision. IDs and history are preserved, while the new Save binds
the current fingerprint. The [legacy binding regressions](../test/managed_document_features_test.cpp)
cover JSON-only omission and an ordinary versioned child. Automatic compatibility
with every old managed Save is not implied.

## Release dates and compile-time policies

Release catalogs and policies belong to the version declaration. Serializer evaluates
these during schema compilation and emits the ordinary minimum/current version bounds.
Generated readers and writers contain no release catalog, dates, clock calls, or policy
tree. Existing unversioned codec paths and payload bytes are unchanged.

```text
public version uint16 { 10 }
  releases {
    8 { "2024-10-01" };
    9 { "2025-04-01" };
    10 { "2026-10-01" };
  }
  policy {
    any {
      max_age { 2 years };
      all {
        keep_last { 3 };
        compatibility { 8 };
      };
    };
  };
```

Versions use the declared discriminator type, including quoted dotted versions.
Catalog versions must be unique and strictly increasing; dates must be nondecreasing
in version order, and the last release must equal the current revision. Dates are
quoted ISO `YYYY-MM-DD` values in years 0001 through 9999. A catalog alone supplies
metadata without extending the default compatibility floor.

A policy block contains exactly one expression, ending with a semicolon. `any` and
`all` may nest and must contain at least one child. Each leaf is an **allow condition**:
`any` accepts the union and chooses the lowest child floor; `all` accepts the
intersection and chooses the highest. The runtime accepts the resulting version
interval, including intermediate values when release numbers have gaps; the catalog
is not a runtime whitelist. Parser limits are 32 expression levels, 4,096 expressions,
4,096 catalog entries, and 128 bytes per metadata literal.

| Leaf | Generation-time meaning |
| --- | --- |
| `max_age { 2 years };` | First release on or after the inclusive age cutoff |
| `keep_last { 3 };` | Third-latest catalog entry, including current; larger counts retain the entire catalog |
| `released_since { "2025-01-01" };` | First release on or after this inclusive date |
| `expires_on { "2027-01-01" };` | Retain the catalog before the date; current version only on or after it |
| `compatibility { 8 };` | Allow revision 8 through current within this subtree |

Counts are positive uint32 decimal integers. Age units are `day(s)`, `week(s)`,
`month(s)`, and `year(s)`. Weeks are seven days. Months and years use calendar
arithmetic with month-end/leap-day clamping, so one year can differ from 365 days.
Cutoffs preceding the supported calendar retain its earliest date. Date/count leaves
require a releases catalog; a compatibility-only tree does not.

An outside-tree `compatibility {8}` overrides the final tree result. Inside a tree,
compatibility is a normal leaf: an enclosing `all` may still restrict it. Every branch
is validated even when an override exists. Catalog-based explicit compatibility
cannot precede its earliest retained release or exceed current. Conversion and retained
historical layouts remain subject to the existing evolution contract.

The current revision always remains readable, even when time conditions expire it.
A time condition exceeded by current produces a compiler warning, including conditions
inside an `any` or an overridden tree. `--version-policy-warnings-as-errors` promotes
that warning to failure before any generated output or depfile is replaced.

### Reference dates and reproducible generation

`--version-policy-as-of YYYY-MM-DD` is optional. Otherwise the compiler captures the
current UTC date once for the invocation and shares it across includes, all requested
languages, and both schemas in `--check-against`. `--verbose` reports the selected date
and each resolved minimum. No generation timestamp is injected into the output.

```sh
serializer --input model.serializer --output model.hpp --cpp.format false \
  --version-policy-as-of 2026-10-06 --version-policy-warnings-as-errors --verbose
python example/run.py --compiler build/serializer --example versioning --language all \
  --version-policy-as-of 2026-10-06
```

Age-based bounds advance only when generation runs again. Already-built programs keep
those bounds until regenerated and rebuilt. Incremental CMake builds do not regenerate
solely because the date changes. Pin the reference date in CI for reproducibility.
All CMake generation helpers accept `VERSION_POLICY_AS_OF` and the flag
`VERSION_POLICY_WARNINGS_AS_ERRORS`.

Library callers use `parser::parse_options` with `version_policy_as_of`,
`version_policy_warnings_as_errors`, and synchronous `warning` / `information`
callbacks. Pass it to `parse_file(path, options)` or `parse(input, require_header,
options)`. `parser::version_policy_reference_date()` supplies a shared UTC date for
multiple calls. Empty dates select UTC today; library diagnostics are delivered only
to supplied callbacks. Policy resolution occurs before generic lowering so concrete
instances retain the same resolved bounds.
