# Fixed XSD bundles and contract tools

The [2.25 C++ ordered element-only mapping](ordered_xml.md) is an explicit
non-XSD XML contract. It does not remove fixed-XSD grouped/repeated-particle
generation gates. Export/compatibility tooling rejects that mapping before
publication until the complete ordered grammar contract is implemented.

Compiler/runtime **2.20.0** adds native-enum export/string-space binding and
expanded tooling qualification. The
[tooling inventory](../../qualification/xsd_tooling/README.md) separates rich
compiler/AST/XSD proofs from the ordinary eleven-output export/rebind profile.
Both Windows/Linux scopes pass their
[final qualification](../verification/verification-xsd-tooling-2026-10-21.md).
Schema language remains **1.21.0** and both editors remain **2.0.10**.

Compiler/runtime **2.11.0** established the language-**1.20.0** tooling baseline.
Serializer compiles
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

## Windows UTF-8 CLI paths

The MSVC-built `serializer.exe` embeds an `activeCodePage` UTF-8 process manifest.
Unicode CLI paths use that process setting on Windows 10 version 1903 and later
without requiring a system-locale change. Older Windows behavior is not newly
qualified. Microsoft documents the
[per-process UTF-8 manifest support](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page).
This compiler setting does not configure a consuming application's own process.

`schemaLocation` values are interpreted as UTF-8 filesystem paths. Relative
locations emitted into exported schemas use explicit UTF-8 path bytes rather
than a locale-dependent narrow conversion. Canonical containment, traversal,
network/absolute-location rejection and dependency checks retain the same rules.
These filesystem changes do not alter native payloads or XML document encoding.

## Bundle identity and supported contracts

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
and interleaving are retained. The shared compiler retains a
bounded particle model for nested sequence/choice/all and heterogeneous repeated
choices, checks ambiguity and exact occurrences, and supplies reference trace
validation. Singleton `all` and supported global-owner key/unique/keyref contracts
have generated owning profiles across the outputs; storage combinations retain
their backend capability gates. A compiled particle model is not a promise of
generated ordered storage for grouped/repeated heterogeneous content. Supported
mixed child/text mappings require their explicit ordered native storage;
association does not infer arbitrary mixed layouts. `complexContent` restriction,
dynamic/abstract type selection and restricted list item types remain unsupported.

Identity binding accepts a strict namespace-aware child-path XPath subset and
same-owner key/unique references. Supported global owners used inside nested
records retain independent identity scopes; identity declarations attached to a
local XSD element still reject. The static
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

Python supports namespace-independent outer union enumeration/fixed constraints;
other backend and namespace-dependent combinations retain their capability gates.
C++, Python, JavaScript and TypeScript support QName-list enumeration, defaults and fixed values, including
an enumerated list inside a union member. Schema values retain ordered expanded
item identities, so instance prefix aliases and shadowed schema prefixes do not
change acceptance. Empty lists and no-namespace items are distinct. Defaults
use a private field scope; mixed no-namespace/default-namespace lists coexist with
other QName attributes and string union fallbacks. Python/JavaScript/TypeScript
keep the native lexical payload unchanged during output. C++ resolves root
attribute/text presentation conflicts while constructing the fresh input candidate;
it keeps existing lexical strings when their original owner scope still represents
the same expanded identities. Native wire layouts and field IDs remain unchanged.
C++ graphs using schema-owned QName-list defaults/fixed values use the direct token
codec. Its bounded normalized-event prefix inventory prevents an early default
from binding an otherwise unbound string-union prefix occurring later in the document.
Automatic import supports builtin QName lists as native string arrays; backend
capability checks still apply to their attached restrictions. Release 2.24 adds
Go/Java/C# scalar/list enumeration, default/fixed values and selected QName/list
union members with [separate executed evidence](../verification/verification-qname-constraints-2026-10-21.md).
Kotlin/Swift also support expanded scalar/list enumeration and retain their
namespace-dependent default/fixed and union gates. C/Rust keep their QName-list
restriction gates. All five preserve QName-pattern aliases and lexical prefix-list
enumeration. Namespace-dependent outer union restrictions and
union defaults still reject, as does exporting these schema-scope QName-list
constraints. See the [QName-list qualification](../verification/verification-qname-lists-2026-10-21.md).

C++ and Python scalar children retain occurrence-specific namespace scopes
for QName, QName-list, prefix-list and nested union values, including child-local
declarations, shadowing and default-namespace resets. Native strings and wire
codecs remain independent of this XML-only state. Applications editing or
reordering indistinguishable lexical strings must keep the scope metadata with
its corresponding occurrence. See the
[qualified scope checks](../verification/verification-qname-scopes-2026-10-20.md);
broader backend/restriction combinations remain **OPC-R01/OPC-R03**.

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
**XML-B02/B10** and the XSD work units in [the central backlog](../pending.md).
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
The [2.20.0 tooling inventory](../../qualification/xsd_tooling/README.md) qualifies
selected rich export/compare/bundle families. Rejected XSD constructs and full
OPC/3MF conformance retain their separate gates.

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

### Native enum contract export

The qualified 2.20.0 profile exports a native enum's original declared spellings
as an XSD string-space enumeration:

```text
serializer version 1.20.0;
namespace orders {
  xml(namespace("urn:orders"));
  enum state { open, in_progress, closed }
  class entry stable_ids {
    public state value (37) {state::open};
  }
}
```

Presentation naming does not change XML labels such as `in_progress`. Native
enum ordinals, string-key/JSON wire names, field IDs, constructor defaults and
readonly behavior retain their existing values. This bounded profile admits
`xs:string` with empty/preserve whitespace; supported facets can narrow the
admitted labels, and at least one declared native enumerator must remain
admitted. An XSD default/fixed label must identify an admitted native enumerator;
generation preflight rejects unknown or facet-excluded values before publication.
XML decode converts an admitted label to that existing
value rather than renumbering the enum. A schema default does not rewrite the
native constructor default. Text-only enum defaults/fixed values are exported on
the global element where the XSD semantics require them.

Ordinary, optional and repeated enum element members are distinct from one XML
list-lexical collection. Enum text requires one nonoptional scalar field;
optional/repeated text still rejects. Attribute occurrence rules are unchanged.
Token/collapsing/normalized spaces, enum list-lexical export and numeric, QName or
list binding spaces reject explicitly. Native enum aliases and explicit enum
ordinals are not schema grammar features. Compiler binding/export support does not imply
that every backend executes every rich restriction; use the current case-specific
verification record.

Rebinding the authored native model preserves its enum type, defaults and methods.
A fresh declaration import of an exported enumeration instead produces a
constrained native string with independently imported names/IDs. The importer
does not reconstruct native enums, authored methods or native constructor defaults
from XSD; that route proves the XML declaration is usable, not native API equality.

### Directional reader comparison

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

Occurrence intervals may widen. A matching sequence can widen to `all`
independently of the `all` declaration order; the reverse direction remains a
different check. Reordered sequences still diagnose an incompatibility. Scalar,
default and fixed-value differences are conservative contract mismatches, not a
general XSD language-inclusion calculation. A renamed/unmapped required member
can instead fail association before comparison. Keep that preflight diagnostic
and backend capability rejection distinct from `XML incompatibility:` output.
For example, a bound particle can still hit a C++ direct-token or portable
unordered-storage gate. Neither case is an automatic rename migration.

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
