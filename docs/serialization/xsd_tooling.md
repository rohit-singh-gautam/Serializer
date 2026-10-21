# Fixed XSD bundles and contract tools

Compiler/runtime **2.11.0** retains schema language **1.20.0**. Serializer compiles
a supported fixed XSD subset into generated XML mappings. The primitive/ordinary
record baseline generates in all eleven outputs; rich scalar and content
contracts have C++ and Python implementations. Other outputs reject unsupported
rich combinations before publication. Bundle processing happens during generation
with the compiler's XML provider enabled; generated codecs do not load these files.

```ini
[xml]
enabled = true
xsd = schemas/order.xsd
# Optional: require the exact identity printed by --verbose.
# bundle_sha256 = <64 lowercase hexadecimal digits>
```

The root schema may include another schema in its own namespace or import a
different namespace. Every `schemaLocation` must name an explicit relative local
file. Canonical paths, including symlink targets, must remain inside the root
schema's directory. Network locations, absolute paths, URI escapes, missing
locations and conflicting namespaces reject. Chameleon includes create separate
component instances per adopted namespace; finite legal include/import cycles
deduplicate by canonical path and effective namespace. Limits cover 64 logical
instances, 32 reference levels and 16 MiB of aggregate source bytes, including
repeated chameleon instances. No document-supplied schema hint changes this allowlist.

The bundle identity is SHA-256 over the byte string `Serializer XSD bundle 1\n`
followed by every logical document instance in lexicographic relative-path and
effective-namespace order. Each instance adds its UTF-8 generic relative path
and exact file contents, each preceded by its decimal byte length and `:`.
If a physical path has multiple logical instances, each of those instances then
adds its effective target namespace, also framed as decimal UTF-8 byte length,
`:` and namespace bytes. Single-instance paths add no namespace component.
Moving an unchanged bundle to a different checkout
does not change its identity. Changing a path or source byte does. This is a build
input identity, not XML canonicalization or a signature. Chameleon instances
retain the original source bytes; repeated instances contribute those bytes
again. Existing previously supported bundles retain their identities.
`bundle_sha256` pins it;
a mismatch fails generation. `--verbose` prints the actual identity. Generated
depfiles include every transitive XSD source.

The compiler/importer supports global element/attribute references, named and
inline complex types, primitive aliases, builtin scalar lists, simple unions
with independently retained branch restrictions, singleton sequences, unit
occurrence model groups and attribute groups, static `complexContent` extension,
and `simpleContent` extension. Text and mandatory singleton collection wrappers
can be derived from unambiguous XSD structure without duplicate INI inventories.
Mixed complex types support scalar text plus
attributes without child particles. A nested class can bind a named or
unambiguous anonymous complex type without its own global element. Defining
document qualification defaults and wildcard target namespaces remain distinct
from an element's global root namespace.

C++ and Python enforce supported numeric bounds, Unicode/list lengths,
whitespace, enumeration/default/fixed values, XSD patterns and union branches,
singleton choices, and a repeated-choice subset with one typed repeated branch
plus positioned foreign wildcard content. Exact branch/group occurrence limits
and interleaving are retained. Multiple known branches in a repeated choice,
grouped choice branches, unordered content and identity checks still need their
dependent generated-backend implementations. The shared compiler now retains a
bounded particle model for nested sequence/choice/all and heterogeneous repeated
choices, checks ambiguity and exact occurrences, and supplies reference trace
validation. Top-level `all` and global-owner key/unique/keyref contracts also bind
shared IR and have reference validators. Their generated codecs reject explicitly.
General mixed child/text content, `complexContent` restriction, dynamic/abstract
type selection and restricted list item types remain unsupported.

Identity binding accepts a strict namespace-aware child-path XPath subset and
same-owner key/unique references; nested local identity owners reject. The static
bundle cap is 128 identity constraints, in addition to document/byte limits.
Projected fields carrying `default`/`fixed` declarations reject because the
reference validator does not augment missing/empty instance values from a schema;
this includes empty defaults. Other ordinary XSD default/fixed bindings are unchanged.
See [advanced shared contracts](xml_advanced_contracts.md) for supported selectors,
value spaces, compilation limits and generated capability boundaries.

Readers validate patterned numeric/list fields against the original XML lexical
value after XSD whitespace normalization and before native conversion. Writers
validate their emitted canonical spelling. A valid integer input `01` can decode
to native `1` yet fail output when a two-digit pattern rejects canonical `1`;
original numeric spelling is not retained. Numeric enumeration/fixed/bound checks
compare in the declared value space, including float/double width and XSD 1.0 NaN
rules. C++ patterns use libxml2; Python patterns require the optional `lxml`
provider. Provider calls are cooperative boundaries, not internally preemptible
operations or a hard process-memory guarantee.
C++ native floating conversion still rejects out-of-range lexical values,
including underflow-to-zero inputs that XSD can round to zero. Complete floating
conversion edge parity remains **OPC-R03**, separately from the checked facet
comparison rules.

Outer union enumeration/fixed constraints and namespace-dependent union defaults
reject until selected-member/schema-scope identities are represented. QName-list
enumeration rejects at every union depth; QName-list fixed values also reject.
C++ union-QName scalar children reject. Existing plain QName child generation
remains accepted, but child-local/shadowed bindings that cannot be represented
by the owning record's namespace map reject during loading. Ordinary QName
attributes/text retain their supported context/default behavior; complete C++
per-field prefix preservation remains **OPC-R01**.

The pinned [official Core 1.4 fixture](../../test/resources/3mf_core_1_4/README.md)
includes the local W3C XML vocabulary, language-or-empty union, numeric/matrix/color
patterns, text metadata and repeated resources choice. Its import/binding and
generated-consumer checks qualify this compiler/XML subset. They do not establish
all 3MF package, geometry, resource, extension or application rules, or arbitrary
XSD 1.0 support.

The compiler validates a temporary closed grammar projection with libxml2.
Provider-unrepresentable finite `maxOccurs` values become `unbounded` only in that
projection, after strict numeric parsing and original interval checks. Native
metadata and generated validators retain the exact original limits. Checked
`mixed`/`abstract` booleans are canonicalized for the provider; annotation bodies
and structural whitespace are omitted from the rebuilt projection. Original
parsed sources, bytes and SHA-256 identity remain unchanged. This grammar-checking
superset does not replace native occurrence checks. Other facet/provider limits
are not silently widened. Scalar resolution bounds depth to 64, expanded union
members to 1,024 and copied facet values to 16 MiB per resolution.

Runtime XSD providers are a separate optional API; the fixed compiler bundle
resolver does not enable runtime imports or remote loading. Qualification of
the earlier 2.7 subset is recorded in the
[2.7 checkpoint record](../verification/verification-opc-xsd-2026-10-19.md).
The [2.8 backend record](../verification/verification-opc-backends-2026-10-19.md)
records the new rich/Core checks and the status of full native qualification.
Remaining constructs and backend/provider checks are tracked under **OPC-R03–R05**,
**OPC-R12** and the XSD work units in [the central backlog](../pending.md).
Historical baseline checks do not qualify later additions.

## Bind an existing model in its schema

An application can retain its chosen API names and native field IDs while
selecting one XSD bundle relative to the declaring `.serializer` file:

```text
serializer version 1.20.0;

namespace orders {
  using schema "order.xsd";

  class order stable_ids {
    public uint32 id (1);
    public string customer (2);
  }
}
```

The supplied XSD must declare the compatible `order` contract. `using schema`
binds declarations already present; it does not create missing classes. XML
selection is automatic for schema-authored XML metadata unless explicitly
disabled. A package declaration cannot be combined with explicit XML disable.
All declaring files using this syntax need a `1.20.0` header. Paths resolve from
the declaring file, and generation dependencies include transitive XSD inputs.

For a contract authored without XSD, use namespace/class defaults such as
`xml(namespace("urn:orders"), scalars(attribute));` and field exceptions such as
`xml(element)`, `xml(text)`, `xml(name("item"))` or
`xml(attribute, namespace("http://www.w3.org/XML/1998/namespace"), name("lang"))`.
Explicit `list`, `qname` and `prefix_list` adapters have C++ and Python support.
Class INI mappings remain compatible and merge nonempty overrides over inherited
defaults; an explicit empty reset through a legacy class INI entry is not
represented. A field's inline namespace override can explicitly be empty.

## Import declarations from XSD

```powershell
serializer --import-xsd schemas/order.xsd --namespace orders --root order --output build/imported/order.serializer --depfile build/imported/order.serializer.d
serializer --input build/imported/order.serializer --language cpp --output build/generated/order.hpp
```

Create the output directories first. `--namespace` defaults to `imported` and
accepts a lowercase snake_case namespace; `--root` selects a global complex root
by local name or `{namespace_uri}local`. Without it, all eligible global complex
roots are selected. Multiple global roots sharing one complex type require an
explicit selection. Recursive owning records and unsupported scalar/root shapes
reject rather than yielding a weaker model.

Import generates reviewable ordinary models, inline XML bindings and one
`using schema` dependency, with deterministic `xsd_` host names. Explicit native
member IDs derive from expanded XML identity and attribute/element placement,
not declaration position; collisions reject. Adding an unrelated field does
not renumber an existing identity. Namespace identities disambiguate generated
type names. Arbitrary-precision numeric XML types remain lexical strings rather
than being silently narrowed to machine integers. A fixed builtin scalar list
can become a typed fixed array while retaining one XML attribute/text value.

The CLI requires a `.serializer` destination with an existing parent. Identical
existing output is accepted; differing existing source is not overwritten.
Import changes into a fresh file and review native compatibility, declaration
order, renamed XML identities and authored behavior before migration. Stable IDs
alone do not preserve a positional layout after field reordering. `--depfile`
records the complete pinned XSD graph. Import is separate from ordinary
`--input` generation and cannot be mixed with unrelated generator options.

The [automatic and optional hand-authored examples](../../example/xsd/README.md)
generate and compile the same primitive contract in all eleven languages. Rich
C++/Python feature checks and other-backend rejection are separate from that baseline;
an imported model is not an OPC package or a 3MF conformance claim.

## Review XSD evolution

Compiler/runtime **2.19.2** qualifies two existing one-source workflows across
all eleven outputs. In the import workflow, generate a changed XSD into a fresh
`.serializer` destination, then review the explicit native names, IDs, types and
declaration order before regenerating application code. An unrelated new XML
field does not renumber surviving expanded identities. Renaming an XML field
changes its imported identity and can change both the native name and ID; the
importer does not infer that two different XML names represent the same
application field. Adding/reordering declarations can change positional bytes
even when surviving IDs remain stable. Differing existing imported source is
protected, including application edits; identical reruns remain idempotent.

In the authored-model workflow, retain native field names, IDs, declaration order,
defaults and behavior. Associate the new XSD through `using schema` and use a
supported field exception such as `xml(name("client"))` to bind the existing
native `customer` field to its new XML name. XML child order comes from the XSD
and can differ from native declaration order. No second INI field inventory is
required. The [working v1/v2 models](../../qualification/xsd_evolution/README.md)
retain literal positional, integer-key and string-key bytes while their XML
children change from `customer,note` to `note,client`.

This preserves the authored native contract, not compatibility between renamed
XML vocabularies. Use the corresponding XML schema or an application migration.
Ambiguous automatic root associations require an explicit `--root`; ambiguous
authored namespace associations require a supported inline namespace exception.
The qualification verifies exact diagnostics and unchanged source, dependency
files and existing generated output on these preflight failures. It does not
promise cross-file atomic replacement after arbitrary filesystem I/O failures.
Broader XSD constructs, rich export/compare/bundle coverage and OPC/3MF conformance
retain their separate gates.

## Export and compare bound contracts

Export the supported actual model contract with:

```powershell
serializer --input model.serializer --config xml.ini --xml.export-xsd order.xsd
```

Native code output is optional for this command. Export supports scalar fields,
attributes, optional/repeated occurrence intervals and nested ordinary records in
one namespace, including supported scalar facets, builtin lists and default/fixed
values. Text, collection wrappers, supported enumerations/scalar facets and
cross-namespace dependencies can be exported. The exported XSD is compiled before
publication. Unsupported advanced choice/wildcard storage, union, inherited
pattern-group and context-dependent QName/prefix-list exports reject.
Import/binding support does not imply export parity. Export does
not infer types from sample XML; declaration import is the separate command
above. See the [authoring contract](../design/opc_3mf_support.md#schema-first-authoring-and-declaration-import).

Compare the previous and current fixed contracts against the same model with:

```powershell
serializer --input model.serializer --config current.ini --xml.compare-xsd previous.xsd
```

`current.ini` must enable XML and select the current `xsd`. The check requires the
current generated reader to accept the previous bound contract. It diagnoses
expanded root/field name changes, attribute/element placement, element order,
representation changes, narrowed occurrence intervals and changed scalar/default
semantics. Both schemas must bind
the supplied model completely; unsupported or conflicting bindings fail instead
of yielding a compatibility claim. This checks the supported bound XML subset,
not arbitrary XSD language inclusion or application-level invariants. Use separate
checks in the opposite direction when both reader directions are required.

Both tools require exactly one generator configuration and validate all artifacts
before replacing outputs. They may accompany ordinary language output flags.

For a pinned multi-namespace bundle, use the standalone command:

```powershell
serializer --input model.serializer --config xml.ini --xml.export-xsd-bundle out/model-xsd
```

The existing parent must be a directory. Export publishes flat `namespace_N.xsd`
files with local imports plus `manifest.json` containing an entry filename,
SHA-256 pin and exact schema inventory. A complete candidate is validated and
read back in a reserved sibling directory before publication. Identical reruns
succeed; different/edited existing directories remain untouched and require a new
export destination. Serialize concurrent mutations to the same output directory.
Bundle export cannot accompany ordinary generated outputs, comparison or depfiles.
Use the manifest entry and SHA-256 through `xml.xsd` and `xml.bundle_sha256` when
binding the exported bundle. See [executed checks](../verification/verification-xml-advanced-contracts-2026-10-19.md).
