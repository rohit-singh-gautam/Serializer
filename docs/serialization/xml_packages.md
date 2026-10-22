# XML, compression streams and packages

Compiler/runtime **2.28.0** supports [explicit ordered element-only content](ordered_xml.md)
for owning record-variant arrays in all eleven outputs. Existing models and mixed
content retain their behavior. Fixed-XSD nested/repeated sequence/choice bindings
and XSD export preserve the complete ordered grammar; see the
[actual SDK checks](../verification/verification-ordered-xml-2026-10-22.md).

The package Save contract uses complete archive rewrites. See
[OPC ZIP records and explicit C++ physical metadata replay](zip_metadata.md) for
Annex B consumption, metadata ownership, regenerated fields and signature policy.
The [fifth-edition metadata qualification](../verification/verification-opc-metadata-2026-10-21.md)
records shared URI/content-type/Relationships, UTF-16 and markup-compatibility
decisions across the actual language SDKs.

Compiler/runtime **2.19.0** extends
[fixed singleton path canonicalization](#fixed-singleton-path-canonicalization)
across eleven outputs, with additive opt-in C/Rust writers and Go loaders.
The schema language remains **1.21.0**. See the
[actual SDK qualification](../verification/verification-package-relocation-2026-10-21.md).

Compiler/runtime **2.17.0** adds language **1.21.0**
[package policies and application references](package_application_contracts.md).
Use automatic scoped keys/references and immutable expected XML/model identities
with fixed/default/runtime ZIP storage selection. Generated profiles now cover all
eleven output languages, including [C](../languages/c_packages.md),
[Rust](../languages/rust_packages.md) and [Swift](../languages/swift_packages.md).
The [current batch record](../verification/verification-pending-105-2026-10-21.md)
separates tested owning profiles from broader graph/provider/corpus requirements.
The 2.16 behavior below remains available to older declarations.

Compiler/runtime **2.16.0**, schema language **1.20.0**. Baseline XML generation
covers all output languages; the stream and package API examples below use C++20.
Supported XML feature combinations are qualified per backend. Generated OPC
profiles are available in C++, Python, Java, JavaScript/TypeScript, Go, C# and Kotlin;
unsupported backend combinations reject before output publication. Existing
native wire formats and managed journals are unchanged.
See [remaining work](../pending.md) and the [full design](../design/xml_support.md).

The [C# and Kotlin/JVM package APIs](../languages/csharp_kotlin_packages.md) add
standard-provider ZIP32/ZIP64, descriptors, bounded sources/private spooling,
typed XML aggregates, opaque policies and staged Save in 2.16. Existing schema
package declarations select these facades. Their automatic XML operation contexts
share the package budget; the [50-item record](../verification/verification-pending-50-2026-10-20.md)
tracks the original acceptance evidence. Version 2.17 adds fixed/default/runtime
storage dispatch and declarative application references as described above.

Compiler/runtime **2.14.0** also supplies selected asynchronous
[JavaScript/TypeScript ZIP/OPC APIs](../languages/javascript_packages.md):
bounded readers/writers, typed XML aggregates, extension policies, canonical
metadata and explicit Node filesystem Save. Stored ZIP remains browser-import
safe. The [actual qualification](../verification/verification-javascript-packages-2026-10-20.md)
records independent inputs/outputs, C++ interchange and Windows publication
faults. Version 2.17 adds automatic native XML budget propagation, supported
storage overrides and generated application key/reference checks. Version 2.28.0
adds declared XML/JSON/BINARY_NONE pipeline selection; arbitrary undeclared codec
replacement remains unsupported.

The selected [Java package API](../languages/java_packages.md) uses Java 17
standard providers for the same typed XML/opaque package workflow, with bounded
random sources, explicit private spooling and staged filesystem Save.
The [Java qualification](../verification/verification-java-packages-2026-10-20.md)
records strict actual JDK 17 compilation/execution, separate Linux JRE execution,
independent ZIP/XML decisions and C++ exchange. Java-array output limits and the
remaining provider and graph boundaries stay explicit. Version 2.17 adds nested
native XML budgets, storage profiles and generated application references.

The selected [Go ZIP/OPC API](../languages/go_packages.md) uses Go 1.22 standard
APIs for ZIP32/ZIP64, descriptors, bounded sources/spooling and staged Save.
Schema packages add typed owning XML facades, metadata/relationship checks,
transactional loading, preservation and canonical output. The
[physical ZIP proof](../verification/verification-go-packages-2026-10-20.md) and
[OPC proof](../verification/verification-go-opc-packages-2026-10-20.md) record actual
Windows/Linux SDKs, independent archives, C++ exchanges and Save/quota faults.
Version 2.17 adds automatic nested native-codec accounting, storage profile
dispatch and generated application key/reference checks. Existing positional
`XmlOptions` literals remain compatible; additive `XmlOperationOptions` APIs
carry explicit operation policies outside package calls.

The [token codec guide](xml_tokens.md) describes the new generated direct paths
and backend-specific advanced mappings. The [typed OPC example](../../example/opc/README.md)
uses a commented shared schema and C++/Python consumers to fill XML and binary
attachment roles, with independent ZIP/XML and cross-language checks.

The [advanced shared contracts](xml_advanced_contracts.md) add compiled scalar,
particle and identity validation, broader local XSD tooling, concrete model and
projection metadata, document profiles and a separate restricted HTML fragment
adapter. Existing C++ package facades expose owning `document_profiles()` metadata
for declared role shape, envelope defaults, expected model/root and vocabulary.
The current generated owning codecs implement the selected scalar, identity,
ordered-content and wildcard contracts across eleven outputs. Unsupported
schema/storage combinations still reject before output publication.

The C++ [XML event API](xml_events.md) includes a provider-independent bounded
structural validator and a LibXml2-backed incremental UTF-8 reader. The reader
retains no XML tree, resolves namespace scope and rejects DTD/user/external
entities. Root closure is insufficient: call `finish()` to check byte EOF before
publication. The whole-object/generated APIs below keep their documented paths.

The [shared content contracts](xml_shared_contracts.md) add checked compiler
IR/configuration and reference validation for presence/nil, ordered mixed
content, maps, variants and positioned extension slots. Selected C++/Python
codecs are implemented in 2.12; remaining backend/slot contracts reject with an
explicit capability diagnostic. Public raw extension helpers can preserve semantic foreign content
with full namespace context; preservation never implies required vocabulary
understanding. These shared contracts do not expand fixed-XSD/backend support.

```text
owning model
    |
    +-- XML codec --> XML document
    +-- any codec --> compression --> byte stream
    +-- any codec --> named ZIP part
                         |
                         +-- optional OPC metadata
```

## Enable XML

XML support is enabled by default (`SERIALIZER_WITH_XML=ON`) and requires
`LibXml2::LibXml2`. The repository vcpkg manifest includes the `xml` feature by
default. Its minimal libxml2 dependency omits optional encoding backends.
Public Serializer headers do not expose libxml2 headers. Installed CMake packages
discover the dependency and export the `SERIALIZER_WITH_XML` target capability.
Libxml2 uses the [MIT license](https://gnome.pages.gitlab.gnome.org/libxml2/).

Use `-DSERIALIZER_WITH_XML=OFF` for a build without XML support. Existing build
caches retain their selection; set `-DSERIALIZER_WITH_XML=ON` explicitly when
upgrading a cache which previously disabled it. Both root build wrappers use this
same CMake default.

Inline XML declarations enable generation automatically. Select `[xml] enabled =
false` only to deliberately omit those XML entry points; a package declaration
rejects that conflicting selection. XML without schema-authored declarations
remains opt-in through configuration. See
[automatic and manual XSD examples](../../example/xsd/README.md) for all outputs.

```powershell
./make.ps1 configure -CMakeArgs @(
  '-DSERIALIZER_WITH_XML=ON',
  '-DVCPKG_MANIFEST_FEATURES=xml')
```

Select ordinary owning models in generator configuration:

```ini
[xml]
enabled = true

[xml.class.orders::order]
root = order
namespace = urn:orders
attributes = id
name.items = item
```

Unlisted members, including a simple integer, become children. `attributes`
explicitly selects scalar attributes; `name.<member>` changes its XML name.
The class key uses the schema-qualified name, before target naming changes.
CLI overrides are `--xml.enabled`, `--xml.xsd`, `--xml.runtime_xsd` and
`--xml.validation_scope`. INI XSD paths resolve relative to the INI file.

| Output | XML provider | Generated API |
| --- | --- | --- |
| C++ | Optional libxml2 | `xml::serialize` / `deserialize`, `xml::codec` |
| Java | JAXP | `encodeXml` / `decodeXml` |
| JavaScript / TypeScript | `@xmldom/xmldom` 0.9.12; TS uses JS | `encodeXml` / `decodeXml` |
| Go | `encoding/xml` | `EncodeXML` / `Decode<Type>XML` |
| C# | `System.Xml` | `EncodeXml` / `DecodeXml` |
| Rust | `roxmltree` 0.21.1 | `encode_xml` / `decode_xml` |
| Python | Standard-library Expat | `encode_xml` / `decode_xml` |
| Swift | FoundationXML | `encodeXml` / `decodeXml` |
| Kotlin | JAXP | `encodeXml` / `decodeXml` |
| C | libxml2 | `<type>_encode_xml` / `<type>_decode_xml` |

Every backend emits typed mapping code rather than converting through JSON.
See the [buildable examples](../../example/xml/README.md) for dependencies,
ownership and exact signatures. Existing binary/JSON protocol selectors keep
their values; XML uses separate entry points. The C++ protocol adapter composes
with the existing generic stream helpers.

```xml
<order xmlns="urn:orders" id="7">
  <count>3</count>
  <item>Widget</item>
</order>
```

The supported mapping covers checked numeric/Boolean/string scalars, enums,
nested owning records, optional scalars/records and repeated one-dimensional
arrays. Fixed arrays require their exact length. Children follow declared order;
unknown attributes/children, duplicates, wrong roots/namespaces and invalid scalar
lexemes fail. Optional absence differs from an empty string. Unsupported shapes
fail generation. Configured simple text with attributes and explicit collection
wrappers are supported as described below. The 2.12 [token subsets](xml_tokens.md)
established nil/map/variant, mixed-content, concrete-generic and inherited mappings;
the current [token guide](xml_tokens.md) defines their eleven-output owning
contracts. Supported explicit presence and map contracts also allow optional
wrapped collections. Revision/magic/digest and borrowed readonly output have their
documented adapters; managed model XML, export-only reference input and unsupported
nested collection shapes retain explicit diagnostics. QName/list/decimal lexical
handling follows the documented scalar/XSD contract; specialized character/date/time/base64/hex-binary
adapters remain unavailable. Byte integers remain ordinary numeric scalars or
repeated elements.

### Text content and collection wrappers

```ini
[xml.class.orders::price]
attributes = currency
text = amount

[xml.class.orders::catalog]
name.prices = price
wrapper.prices = prices
```

The first mapping emits `<price currency="USD">12.5</price>`. `amount` must be
one nonoptional scalar (including a supported enum); every other field must be an
attribute. Empty string text is valid; empty numeric text fails. All field names,
lexical conversions, unknown-attribute and resource checks remain active. Child
elements in a text slot reject. This does not introduce ordered mixed content.

The collection mapping emits `<catalog><prices><price .../></prices></catalog>`.
Wrappers use the record namespace, retain item names and are required exactly once
in declared field order, including for empty collections. Unknown attributes,
text or children inside a wrapper reject. Fixed arrays retain exact lengths and
every wrapper counts toward node/depth budgets. Configuration validates conflicting
names, unknown fields and incompatible shapes before publishing any output.
These configuration mappings can be used without a fixed XSD; explicit text and
wrapper overrides of XSD bindings reject.

## Whole-object APIs

```cpp
#include <rohit/serializer.hpp>
#include <rohit/xml.hpp>

namespace codec = rohit::serializer;
const auto bytes = codec::xml::serialize(order);
auto copy = codec::xml::deserialize<orders::order>(bytes);

codec::serialize_to<codec::xml::codec>(output, order);
auto loaded = codec::deserialize_exact<
    orders::order, codec::xml::codec>(input);
```

`xml::limits` bounds document bytes, text, names, nodes, attributes, namespace
bindings, depth and logical retained tree bytes. Generated builders check limits
while constructing the tree. Input bytes are bounded before parsing. Provider
DOM adapters, including JavaScript, can build provider trees before
all structural checks; C++ checks its retained-tree budgets while reading.
These are logical resource limits, not a total heap/CPU quota. Decoding returns
a fresh candidate after complete validation.
Assignment to an existing object may itself throw.

Compiler/runtime 2.20.1 charges generated element/attribute namespace declarations
in Java/C#/Kotlin/Swift direct output. Kotlin/Swift count namespace declarations
against their combined node/attribute limit, including empty-default resets;
their type-name declarations are charged exactly once in both ordinary and
preserved scopes. Java/C# count declarations against the attribute limit, and
C# input normalizes declaration names to prefix/URI text. These checks run before
growing declaration output and preserve existing emitted bytes.

Output counters are not yet identical across every backend and adapter. Other
provider-generated declaration paths, retained namespace scopes, tree construction
and text policies retain their separate resource qualification gates. An unusually
tight policy can still differ between adapters; broader alignment remains part
of `XML-B13` in the [pending requirements](../pending.md).

All outputs now have direct generated output paths. Supported C++/Python token
graphs and Java/C#/Go/Kotlin input use direct/pull traversal; richer C++/Python mappings
and other input adapters retain their documented trees. Complete output bytes
are staged. See [token paths and boundaries](xml_tokens.md). Configured names and
QName prefixes accept UTF-8 Unicode XML NCNames from compiler/runtime 2.16.0;
values are XML 1.0 UTF-8. DTDs,
user-declared/external entities and non-1.0 XML are rejected. Predefined entities
and character references are supported.
Bundled parsers load no XML external resources; caller-supplied validators must
enforce the same rule. Namespace identity uses URI plus local
name. Rewriting does not preserve comments, prefixes, processing instructions or
lexical formatting. QName-valued fields need the explicit namespace-aware
adapters described below; ordinary strings do not retain QName semantics.
Schema-location hints do not select a validator. The strict generated mapping
rejects undeclared attributes, including unbound `xsi:schemaLocation` hints.

C++ clients constructing XML names can include `<rohit/xml_name.hpp>` and call
`xml::is_ncname(text)`. This allocation-free `constexpr` check returns false for
empty names, namespace separators, invalid XML name characters or malformed UTF-8;
it does not require the XML provider. Namespace URIs have their separate contract.

## Fixed XSD and optional runtime validation

```ini
[xml]
enabled = true
xsd = orders.xsd
runtime_xsd = false
```

The fixed build input is authoritative for placement, namespace, sequence and
occurrence constraints. Do not repeat its attribute decisions in configuration;
member renames may associate different model names. Changing the XSD requires
regeneration and recompilation; CLI depfiles include it.

The common baseline accepts bounded local XSD 1.0 import/include bundles,
inline/named root complex types, nested record bindings, singleton nested
sequences, primitive typed elements/attributes, global references and facet-free
primitive simple aliases. C++ and Python additionally support the richer facets,
lists, unions, extensions and restricted choices in the
[XSD tooling guide](xsd_tooling.md). Unsupported declarations, identity constraints
and conflicting bindings fail generation. No runtime XSD files or general
whole-document validator calls are required by the generated default path;
patterns use the selected backend's XSD regular-expression provider.

Primitive bindings include `string`, `boolean`, signed/unsigned 8–64-bit integers,
`float` and `double`. Arbitrary-precision numeric imports retain lexical strings.
Annotations do not become application fields. One fixed XSD bundle applies to
the complete generation unit; nested records can bind compatible named or
unambiguous anonymous complex types without their own global element.

To allow an explicit prepared validator, generate with `runtime_xsd = true` and
`validation_scope = input`, `output` or `both` (default: `input`).
Callers must supply a prepared validator to invoke it. With none supplied, only
the generated mapping checks run, even when runtime support was enabled.

```cpp
const codec::xml::prepared_schema schema{trusted_xsd_bytes};
auto value = codec::xml::deserialize<orders::order>(
    xml_bytes, {}, &schema);
```

Callers select and prepare the trusted schema once. C++/C use libxml2, Java/Kotlin
use JAXP, and C# uses `XmlSchemaSet`; Python's optional prepared provider requires
`lxml` (the ordinary codec uses only the standard library). These providers can
validate a broader standalone XSD 1.0 subset than the optimized binder, with
external schema references rejected. C++ validation uses a separate context per
call; follow each adapter's documented lifetime and concurrency contract.

Go, JavaScript/TypeScript, Rust and Swift expose a caller-supplied validation
interface. They do not bundle an XSD engine: the application's prepared provider
must enforce trusted schemas, disable external resolution and obey its resource
and concurrency policy. Every adapter checks the selected validation direction.
Supplying a validator to a model generated without runtime support fails. Generic
stream helpers use generated checks; pass validators through XML-specific helpers.
Bundled providers support XSD 1.0, not XSD 1.1.

## Composable compression streams

```cpp
#include <rohit/compression_stream.hpp>

namespace compression = codec::compression;
compression::output_stream compressed{
    output, compression::gzip_options{}};
codec::serialize_to<codec::json>(compressed, order);
compressed.finish();
```

The borrowed underlying stream may be a file, memory, custom transport or another
compatible stream wrapper. Finish the wrapper receiving codec bytes before
finishing its underlying wrappers. Finishing does not close, flush or synchronize
the borrowed sink. Destruction never emits a trailer. Failed writers are fenced.
External sinks can retain a partial prefix on failure.

Gzip/zlib/raw DEFLATE, Zstandard and LZ4 compress ordinary codec byte batches into
one standard message. `none` passes bytes through; custom whole-span backends
remain bounded staged adapters. Contiguous codec reservations can still require
scratch storage, checked against the input bound before growth.

`compression::input_stream{source, decode_options}` validates and decompresses
one complete bounded message before exposing its bytes. Use `deserialize_exact`
for complete object consumption. This is whole-object serialization; no record
callbacks or incremental object-edit protocol is introduced. Existing compression
parameter overloads remain available. See [compression](compression.md).

## Plain ZIP and OPC

`<rohit/package.hpp>` provides `package::zip_writer`/`zip_reader` for arbitrary
codec bytes. Stored entries need no compression provider; DEFLATE requires zlib.
ZIP32/ZIP64 input supports data descriptors, central-directory checks and selective
payload reads with CRC/size validation. ZIP64 sizes, offsets and entry counts remain
subject to the caller's limits. Writers select ZIP64 automatically when narrow
fields cannot represent their values; `entry_options::force_zip64` also permits
small entries to use ZIP64 size fields. Encryption, split archives and unsupported
methods fail explicitly. Logical ZIP names are canonical UTF-8 relative paths.

Seekable sources are borrowed. Nonseekable sources require an explicit
`input_options{spool_directory, max_spool_bytes}` as the reader's third argument.
The reader copies bounded batches into one exclusively created private file in
that directory, under the same I/O/archive/cancellation budget. It never silently
buffers a whole archive in memory or interprets part names as filesystem paths.
The file is removed on reader destruction and on failed construction. The caller
must supply a usable directory; no filesystem extraction occurs.
Spooling charges requested read batches, including its bounded EOF/overflow probe;
size indexing and later part reads consume additional I/O from that same budget.
Exception-enabled standard streams retain their exception mask.

```cpp
namespace package = codec::package;
package::zip_writer archive{output};
archive.write("model.json", [&](std::ostream& part) {
  codec::serialize_to<codec::json>(part, order);
}, {.method = package::zip_method::deflate});
archive.finish();
```

`write` stages one bounded complete part; archive reads stage the selected
compressed/expanded part. Construction validates metadata, not every payload's
CRC. Call `validate_all()` when the operation requires checking all entries.
One shared `package::budget` accumulates metadata, I/O, expanded bytes, entries
and relationships; failed/repeated work is not refunded. It also exposes cumulative
decoder-allocation, work, reference and preserved-byte categories for profile
callbacks. OPC relationship admission and supported XML key/unique/keyref observers
automatically use the same reference category. Each complete candidate tuple
counts, including repeated equal keyref requests; incomplete optional tuples do
not count. Limits bound scratch and part/archive sizes. DEFLATE checks cancellation
at provider output boundaries, including zero-output batches. The compression
options' `check_cancelled` callback can supply the same policy outside packages;
custom providers must cooperate with their sink's output boundaries.

OPC requires XML. `opc_writer` adds content-type defaults/overrides, parts and
source-scoped relationships, then generates `[Content_Types].xml` and `.rels`.
`opc_reader` discovers relationships by source/type and resolves internal relative
targets. External links remain inert data. XML metadata uses the shared parser.

Applications supply payload models and package vocabulary; they do not need
`.serializer` schemas for the standard content-type or relationship metadata.
The [OPC/3MF authoring requirements](../design/opc_3mf_support.md) distinguish
remaining generic capabilities from application work and show a restricted schema
and current build workflow. C++ and Python declarative profiles are described below.

```cpp
package::opc_writer document{output};
document.write_part("/data/order.xml", "application/xml",
    [&](std::ostream& part) {
      codec::serialize_to<codec::xml::codec>(part, order);
    });
document.add_relationship({"/", "r1", "urn:orders:main",
                           "data/order.xml"});
document.finish();
```

This is a documented OPC subset, not complete Office/OpenXML support. Part URIs
and decoded escapes use the ASCII subset; Unicode part naming is unsupported.
Equivalent case/escape identities and ancestor/descendant part collisions are
rejected. Internal query/fragment selectors are retained while existence checks
resolve their underlying part; applications interpret the selectors.
Applications supply vocabulary rules, expected types/root names, relationship
cardinalities, payload key/reference checks and business validation. The library
checks archive/metadata integrity, relationship uniqueness and target existence.
The selected edition is ECMA-376 Part 2, fifth edition (December 2021), with
Part 3 fifth-edition Relationships preprocessing. OPC maps physical ZIP files to
logical parts: directory/non-file records and ASCII names outside the PartName
grammar are excluded, their payloads still undergo bounded integrity checks, and
relationships cannot resolve to them. They are not retained as unknown parts.
The general ZIP reader keeps its separate filename policy.

## Caller-authored aggregate profiles

`load_opc<Model>(reader, callback, extension_policy)` builds a fresh model and
returns `aggregate_result<Model>` only after all selected parts, caller checks and
remaining payload integrity succeed. `load_opc_into` swaps the complete aggregate,
including retained extensions, into an existing destination with a nonthrowing
swap. Any earlier failure preserves that destination.

The callback receives the candidate model and a `profile_loader`. Its
`read_related(source, role, expected_content_type, decode, minimum, maximum)`
discovers renamed parts through relationships, requires internal targets, checks
exact content-type strings and cardinalities, and invokes the selected codec.
Each callback receives the part metadata and original shared budget. It must
charge its decoder allocations/work and reference indexes there; automatic
generated codec accounting is not supplied by this caller-authored API.
Perform payload key/reference and business checks before returning from the
aggregate callback. Throw `package::error` with `field_path`, `reference` and
`relationship_id` for application diagnostics. Codec failures add the selected
part and link when absent; payload contents are never copied into diagnostics.

`<rohit/package_profile.hpp>` adds `reference_index<Key>` for integral or string
payload keys. Register `add_target<Model>(scope, key, {part, field_path, relationship_id})`, retain
forward references with `require_target<Model>`, then call `validate()` before
returning from the aggregate callback. The shared budget bounds key/index storage,
reference counts and validation work. Duplicate keys within a scope, missing
targets and wrong model types report the source part, field and key. Reference
admission and validation-work exhaustion retain these locations and an optional
relationship ID. The original two-string location initializer remains valid. Equal keys
in distinct scopes are independent. The index retains no model pointers; the
application resolves validated IDs into its own model relationships.
This optional reference-index helper requires C++ RTTI for local expected-type
checks; no RTTI identity is written to a document or side channel.

Unknown parts and relationships default to rejection. Explicit `skip` validates
payload integrity and discards optional unknown data; `preserve` retains opaque
part bytes, content types and authored unknown relationship context under the
preserved-byte and metadata budgets. Profile predicates identify required
capabilities, which fail under every unknown policy. `write_preserved` appends
retained data to an OPC writer after known-field edits; finalization rechecks
source/target existence, so edits cannot silently publish dangling preserved links.
### Fixed singleton path canonicalization

Generated C++/Python facades retain loaded fixed singleton typed-role names in
`preserved_extensions::relocations` / `PreservedExtensions.relocations`. Passing
the loaded extensions to generated Save automatically rebinds preserved source
and internal target names to the role's declared canonical output path. Rebinding
is simultaneous, compares OPC ASCII identities, and resolves relative targets in
their original source scope before changing either endpoint. Query/fragment
suffixes remain verbatim; external targets are inert and unchanged even when their
source moves. Unknown part bytes and caller-owned extension records are unchanged.
Conflicting origins, removed targets and source-local ID collisions still reject
before file replacement. Empty relocation lists retain the caller-authored API's
existing behavior. Only a discovered path whose exact spelling differs from the
declared output path creates automatic provenance; an unchanged role does not
cause unrelated URI spelling to be rewritten.

From 2.19, Java, JavaScript/TypeScript, C#, Kotlin and Swift generated facades retain
equivalent private provenance on their loaded extensions. Pass that same extension
object with the edited input; rebuilding extensions from public part/link arrays
does not transfer private provenance. C provides additive canonical encode/write/
Save functions, and Rust provides qualified `CanonicalPackage` methods without
changing their existing value layouts or ordinary discovered-path writers. Go's
additive canonical loaders select the fixed output paths before publishing a value
while retaining its public option/value layouts and ordinary Load behavior. See
the [C](../languages/c_packages.md), [Rust](../languages/rust_packages.md),
[Go](../languages/go_packages.md), [Java](../languages/java_packages.md),
[JS/TS](../languages/javascript_packages.md),
[C#/Kotlin](../languages/csharp_kotlin_packages.md) and
[Swift](../languages/swift_packages.md) guides.

Moves are simultaneous. A retained internal target is resolved from its original
source, then the original source and resolved target are independently mapped.
The raw suffix beginning at the first literal `?` or `#` remains verbatim;
external URI references are inert and keep their complete target text. Unmoved
links retain their spelling. Part bytes and caller-owned metadata remain
unchanged across repeated encode/Save calls. Source occurrence indexes for dynamic
children retain their meaning when their fixed parent moves.

Moved origins retain role ownership even after an optional role is removed. A
preserved source or target referring to that removed origin rejects, including a
swap where another role now occupies the same path. Occupied output paths,
ambiguous origins, dangling links and source-local relationship ID collisions
also reject before aggregate/final-file publication. A failed Save preserves the
previous destination; borrowed stream writes retain their documented partial
write behavior. Automatically generated IDs keep their backend's existing
algorithm. Dynamic role renames and arbitrary caller path edits still need
explicit application rebinding. Scratch/work/metadata use the operation budget;
these logical counters do not establish hard process-allocation bounds.
Opaque XML bytes carry their original namespace declarations. Rewritten known XML
does not retain unknown attributes/elements through this part-level API; semantic
XML-extension preservation, exact lexical preservation and signatures are separate
features.

`write_canonical_zip` and `write_canonical_opc` accept caller-owned complete parts,
sort names and source-local relationship IDs, use fixed DOS-epoch timestamps and
zero platform attributes, then finalize the archive. Auxiliary pointer indexes
are bounded and reserved from the shared scratch capacity before output, so nested
compression cannot spend that same capacity again. ZIP names remain relative; OPC names have leading
slashes. Byte equality requires identical part bytes, authored names/IDs,
compression options and compressor implementation/version. This policy does not
normalize XML or establish a cross-compressor byte-equality promise.

## Complete file Save

`save_zip_file(path, callback)` and `save_opc_file(path, callback)` create a complete
staged file in the destination directory, finalize it, synchronize it, publish
through native replacement and synchronize the parent directory. Preparation
failures preserve the previous file. `publication_uncertain` reports a failure
after replacement when durability cannot be confirmed. Arbitrary streams have no
replacement guarantee.

The [native Save fault matrix](../verification/verification-package-save-faults-2026-10-20.md)
qualifies creation, partial writes, file synchronization and replacement failures
on Windows and Linux static runtimes, plus Linux postpublication directory sync.
Power-loss and document/sidecar generation recovery remain separate contracts.

These helpers add no Serializer history, journal or session parts. Application
profiles must keep that state in side channels. The generic builder accepts
caller-provided payloads; it does not identify or police their application meaning.
These helpers do not automatically attach to native managed stores. Runtime 2.13
adds a separate [caller-supplied document-codec adapter](../guides/usage.md#external-documents-with-a-differential-sidecar)
with exact-byte generation binding and recoverable document/sidecar publication.
Generated profiles compose through the caller-supplied managed document encoder
and decoder. The application selects its document projection, provider and parser
budgets. Format/provider/platform qualification retains its recorded scope.

## Inline XML authoring

With language 1.20, keep defaults and exceptions beside the fields they describe:

```text
serializer version 1.20.0;
namespace geometry {
  xml(namespace("urn:geometry"), scalars(attribute));
  class vertex {
    public double x (1);
    public double y (2);
    public double z (3);
  }
  class mesh {
    public array vertex vertices (1) xml(element, name("vertex"), wrapper("vertices"));
  }
}
```

Namespace defaults inherit into nested namespaces and classes. Class `xml(...)`
can change `root`, `namespace`, `scalars(element|attribute)` and
`unknown(reject|skip|preserve)`. A field exception can select `attribute`, `element`
or `text`, and override `name`, `namespace` or a repeated-element `wrapper`.
Collections and records remain elements under a scalar-attribute default.
An optional `[xml] namespace = uri` and `scalars = attribute|element` supplies
configuration defaults; explicit local mappings resolve exceptions. Existing INI
class entries remain supported and sparse entries retain inherited defaults.

When an XSD supplies the XML contract, prefer namespace `using schema "local.xsd";`
and omit repeated placement annotations. Paths are relative to the declaring
Serializer source. See [XSD binding/import](xsd_tooling.md) for associations,
limits and backend restrictions. Importing model declarations is a separate CLI
operation; the binding directive alone does not create classes.

All eleven owning outputs support the documented scalar/list/union facets,
namespace scopes, portable patterns and declared XML wildcard/extension contracts.
Repeated heterogeneous order requires its supported explicit native mapping;
arbitrary storage layouts still reject. C++ preservation adds an XML-only one-pointer
`xml_extensions` slot only when requested;
its opaque storage allocates on the first retained unknown value. Use `.get()`
to test whether storage exists and `.value()` to inspect retained data.
QName/prefix-list mappings similarly retain only their needed namespace bindings.
Compiler/runtime 2.16.0 additionally retains complete C++/Python scalar-child
scopes by field and occurrence, including local/shadowed declarations and default
namespace resets. Keep these XML-only scopes associated with the corresponding
values when editing. Go/Java/C#/Kotlin/Swift retain per-occurrence bindings,
including QName/list enumeration/default/fixed and first-selected outer union
identities in 2.28.0; see the
[scalar checks](../verification/verification-scalar-identities-2026-10-22.md).
The separate `prefix_list` default/fixed/export adapter requires its external
capability contract. C/Rust retain scalar scopes and foreign extension state
through XML document companions, including wrapped/nil records, typed mixed/ordered
children, lifecycle metadata and supported map/variant graphs.
See the [new graph checks](../verification/verification-native-namespace-graphs-2026-10-22.md) and
[current namespace checks](../verification/verification-qname-scopes-2026-10-20.md).
Unknown core names remain errors. Extension policies preserve semantic content
and placement, not original prefixes, formatting or signatures.

## Declarative C++ package profiles

This language 1.20 declaration generates the profile facade as well as XML codecs:

```text
serializer version 1.20.0;
namespace files {
  xml(namespace("urn:files"), scalars(attribute));
  class model { public string label (1); }
  package archive {
    part main : model {
      name("/3D/model.xml");
      content_type("application/vnd.example.model+xml");
      relationship("urn:example:main-model");
      compression(deflate);
    }
    part attachments : bytes {
      name(dynamic); content_type(dynamic);
      relationship("urn:example:attachment"); source(main);
      occurs(0, unbounded); compression(deflate);
    }
  }
}
```

Generate with the ordinary C++ CLI or `serializer_generate` and link
`Serializer::runtime` from an XML-enabled build. The generated API is:

```cpp
files::model model{};
model.label = "example";
files::archive::save("example.package", {&model, {}});
std::ifstream input{"example.package", std::ios::binary};
auto loaded = files::archive::load(input);
// loaded.value.main is the decoded model; loaded.extensions holds requested opaque data.
```

`input` borrows models and attachment spans for synchronous output. Dynamic typed
roles use `model_part_view<T>`; opaque roles use `binary_part_view`. Each view
selects its actual name/media type and source occurrence index. A decoded dynamic
typed role owns `model_part<T>` values with actual discovered metadata. Names and
media types declared as fixed are compiler defaults; loading discovers the role
by its relationship type and validates its expected media type. Occurrence bounds
apply to the complete role across its source instances. Repeated roles require
dynamic names and generated relationship IDs. Source-role cycles reject.

`write(stream, input, shared_budget)` writes a complete package to a borrowed
stream; a failed stream write can retain a prefix. `save(path, input, limits)`
uses staged full-file replacement. `load(stream, limits, extension_policy,
validate)` constructs an owning aggregate before returning. An optional validation
callback `(const value&, package::budget&)` checks application references/domain
rules before publication. It throws on failure and shares the operation budget.
Generated XML codecs automatically borrow that budget for allocations, work,
preservation and complete identity tuples. Resource, cancellation and deadline
failures retain the part URI, selected generated member path or identity-constraint
name, and owning relationship ID. Failures before a member is selected report
the known outer location. These diagnostics contain no scalar payloads.
Pass a retained `preserved_extensions` pointer as `input.extensions` to write
opaque parts and links after editing; finalization revalidates their targets.

The runtime writes `[Content_Types].xml` and scoped `.rels` automatically.
Applications do not define Serializer schemas for these infrastructure files.
Package declarations do not add geometry or format-specific validation. Python
uses the same declarations with the API below. Other-language package generation
rejects explicitly; baseline XML generation across all outputs remains available
independently.

## Declarative Python package profiles

Generate the same declaration using `--language python --python.output files.py`.
The standard-library implementation includes ZIP32/ZIP64, DEFLATE, OPC metadata,
typed aggregate loading, opaque-part preservation and staged full-file Save:

```python
from files import FilesArchive, FilesModel, BinaryPart

model = FilesModel()
model.label = "example"
value = FilesArchive.Input(
    main=model,
    attachments=[BinaryPart(b"payload", "/Attachments/data.bin", "application/octet-stream")],
)
FilesArchive.save("example.package", value)
loaded = FilesArchive.load("example.package")
assert loaded.value.main.label == "example"
```

The facade also supplies `encode(input)`, `write(stream, input)` and
`load(bytes_or_path_or_stream)`. `PackageLimits` and `PackageBudget` share
cumulative XML/ZIP accounting, cancellation and an optional monotonic deadline.
Generated typed-role codecs also share reference admission with OPC relationships;
Save's staged typed validation continues spending the original budget. Exhaustion
retains the selected part, member or identity-constraint path and relationship ID.
An optional `validate(candidate, budget)` callback rejects domain-invalid data
before publication. Pass retained `loaded.extensions` as `Input.extensions` to
preserve permitted unknown parts and links after editing. External targets remain
inert. Save rereads its staged candidate before replacement; a failure after
replacement reports `publication_uncertain`.
Save polls caller cancellation/deadline policy immediately after returning
`before_publish` and `after_publish` hooks. The first boundary retains the old
destination; the second reports uncertainty while retaining the new complete
archive. A throwing hook keeps its original error precedence. Use a fresh budget
for recovery and inspect the committed generation before retrying.

Byte-like input is addressed in bytes, including typed and multidimensional
memoryviews. Keep borrowed mutable input unchanged through a synchronous call.
Noncontiguous input needs an admitted contiguous copy. Borrowed source streams
may return short byte reads; short sink writes fail and can retain a prefix.
Neither stream is closed or flushed. Cancellation/deadlines remain active after
the final sink callback. Owned private spools and failed Save staging are closed
and removed, including failure to transfer a raw temporary descriptor to a stream.
See the [executed boundary and fault matrix](../verification/verification-package-lifecycle-2026-10-20.md).

Seekable Python streams are borrowed directly from their current position to
their measured end. Successful reads leave the caller's stream at that end;
archive-relative offsets never address an earlier prefix. Every byte extent,
I/O charge and buffer bound is checked before a read. Genuine nonseekable input
continues to use a bounded private spool. Both C++ and Python accept classic ZIP
archives containing exactly 65,535 entries and check optional ZIP64 end records,
including agreement with ordinary fields that have not overflowed. The
[ZIP64 input qualification](../verification/verification-package-zip64-input-2026-10-20.md)
covers logical offsets around 4 GiB, real entry-count transitions, malformed
records, exact resource limits and limits reduced by one.

Compiler **2.14.0** validates Python relationship IRIs using literal ASCII
delimiters, preserving Unicode authority spelling without normalization. Unicode
lookalikes of `/`, `?`, `#`, `@` and `:` remain authority data under the selected
[RFC3987 syntax](https://www.rfc-editor.org/rfc/rfc3987#section-2.2).
Malformed escapes, IP literals, zone identifiers and non-ASCII ports reject;
internal targets still reject schemes and authorities. The
[package verification](../verification/verification-package-lifecycle-2026-10-20.md)
records the selected profile, independent ZIP/XML fixtures and atomic Save tests.

The richer Python XML subset includes scalar facets/defaults/fixed values,
typed lists, QName/prefix bindings, unions, supported repeated choices and
positioned lax/skip extensions. XSD patterns use the optional `lxml` provider; a requested pattern
without that provider rejects explicitly. Unsupported content models still
reject during generation. Python/container/provider allocations are not a hard
process-memory bound. Check the [central backlog](../pending.md) for remaining
backend and provider qualification.

## Shared XML/package operation context

C++ `xml::serialize`, `deserialize`, `parse` and `write` accept an optional
`xml::operation_context*`. It carries cooperative cancellation, a caller-selected
steady-clock deadline and allocation/work accounting. Generated package facades
bridge these checks to their single `package::budget`; nested XML operations do
not start a fresh cumulative budget. `package::limits::deadline` is optional.
Direct builder callbacks can use `budget.xml_context(part, relationship_id)` for
the same composition; the existing no-argument and part-only C++ calls remain
valid. Python `PackageBudget.xml_context(part, relationship_id)` supplies the
equivalent borrowed context for custom generated XML codecs. Field diagnostics
borrow generated member names during traversal and materialize paths only on
failure; Python admits its optional diagnostic wrappers through the shared
allocation category. Ordinary XML callbacks retain their original resource events
and exceptions. Declarative application-key discovery remains separate from the
existing XSD identity observers and caller-authored reference checks.
Deferred start-tag/attribute presentation, retained-tree rendering and provider
work outside an instrumented native validation/admission scope can retain only
the part and relationship. Native field paths are not persisted into XML trees.
Provider-local admission may also produce a codec error before a shared resource
callback runs. These phases do not guarantee native member-path attribution or
shared-resource classification; the part/relationship context and original
provider error remain the available diagnostics.
The borrowed context and callback captures must outlive the synchronous operation.

Release 2.22.0 adds generated completion checks around prepared validators,
including readonly projections, and moves C ZIP final cancellation polls before
destination replacement. A provider may finish validation and then signal
cancellation; no output or input candidate may escape that final check. See the
[publication regressions](../verification/verification-operation-publication-2026-10-21.md)
and [physical namespace admission matrix](../verification/verification-runtime-boundaries-2026-10-21.md).
These cooperative checks do not forcibly preempt opaque provider code or cap its
internal process memory.

C#/Kotlin/Swift also expose standalone public cancellation policies. Each helper
returns a copy retaining the original limits and any existing operation ledger:

```csharp
var policy = new Limits().WithCancellation(checkOperation);
```

```kotlin
val policy = XmlLimits().withCancellation(::checkOperation)
```

```swift
let policy = XmlLimits().withCancellation(checkOperation)
```

`checkOperation` is a synchronous caller callback that throws the caller's own
cancellation or host-clock deadline error. The policy retains its callback state;
resources captured by the callback must remain valid through the operation.
Nested captures share the first original failure and forward parent admissions
once. Construct a fresh wrapper per top-level operation and after failure, and
do not share one wrapped policy concurrently. Wrapping a failed parent ledger
does not reset that ledger. These APIs are checked from separately compiled
assemblies/JARs/Swift modules, not only same-module consumers.

Swift FoundationXML delegate failures retain the first error and stop building
the XML tree or namespace state. The synchronous provider finishes processing
the already byte-admitted input; the codec then throws that original error
before returning a candidate. This avoids a Windows FoundationXML crash caused
by reentrant parser abort. It does not promise immediate interruption of opaque
provider CPU or a hard provider-memory ceiling.

Release 2.18.0 adds native C/Rust operation composition and supported XSD identity
package graphs. C descriptors accept an independent `srl_xml_operation` and optional
policy selection without extending old native aggregates; Rust provides qualified
operation traits without changing `XmlLimits` literals or old native errors/results.
The native operation inventory counts primary elements, physical attributes
(including namespace declarations) and expanded-name/decoded scalar vocabulary
once. Identity replays add actual work/allocation and one reference per complete
tuple before keyref deduplication. Infrastructure metadata retains its own existing
admission inventory; these logical counters are not process-peak measurements.
Known default construction is admitted before generated allocation. Opaque
authored factory/provider internals do not gain allocation or preemption hooks.

Selected-codec C/Rust/Go entry points, JS/TS `options.codecs` and Kotlin's public
`xmlLimits(CodecContext)` keep caller codecs on that same ledger. Use supplied
bridged policies rather than reconstructing them inside callbacks. Preserve their
synchronous lifetime and declared model contract. See the backend package guides
for typed callbacks and collision-selected helper names. Version 2.28 composes
C/Rust XML document companions with these package adapters.

Ordinary XML input remains UTF-8. C++, Python, C and Rust OPC infrastructure metadata additionally
accepts BOM-marked UTF-16 in either byte order; its declaration must be absent or
identify `UTF-16`. Unsupported/conflicting encodings reject, and generated output
remains UTF-8. C++ direct raw parsing can select
`operation_context.input_encoding = input_encoding_policy::utf8_or_utf16`.
The C/Rust owning profiles charge original encoded metadata bytes and qualify
strict conversion/provider boundaries separately from typed model decoding;
see the [encoding replay](../verification/verification-opc-utf16-2026-10-21.md).
Raw parsing can also opt into `allow_mixed_content`; this retains ordered text
segments for infrastructure processing and does not relax typed field readers.

Generated C++ package profiles stage complete encoded parts. Their ordinary rich
XML entry point can retain bounded trees; token-required contracts use the new
direct path, and bare XML callers can select `serialize_tokens` explicitly.
Borrowed model inputs avoid duplicate owning models, but do not promise zero-copy
XML, bounded constant memory or tree-free streaming. Non-XML generated models gain
no package fields or runtime checks. All eleven generated package pipelines share
cumulative contexts; process-memory/performance qualification retains its separate
measured scope.

## Generated package pipelines

Version 2.28 supports generated profiles in all eleven outputs with XML, native
JSON and BINARY_NONE codecs and stored/DEFLATE compression. Schema `codec(...)`,
`compression(...)` and `policy(fixed|defaults)` bind defaults. The existing INI
`[package.<qualified package>]` role selectors `.codec`, `.compression`, `.policy`,
`.vocabularies`, `.requires_type` and `.target_mode` configure the same compiled
contract. Runtime envelope overrides select codec/compression only for default
roles; they cannot replace the native model, XML root/type, MIME or container.
Positional-only generation rejects JSON selection before invoking any caller codec.
The old compression-only and selected XML-codec APIs retain their meaning.
An XML callback supplied for a JSON/BINARY_NONE envelope rejects before invocation.

XML profiles require a matching expanded `xsi:type` when configured and resolve
root `mc:MustUnderstand` prefixes against the original root scope. Every URI must
belong to the statically known plus explicitly declared vocabulary inventory.
Only that authorized root marker is consumed; unknown core fields remain closed.
Byte roles may select `internal`, `external` or `both`. External occurrences use
the exact inert URI as their name, empty content type and empty bytes; no ZIP
member is emitted and loading never fetches the URI. Dynamic external/both roles
require dynamic name/media selectors and cannot parent another role.

Loading retains original internal physical names as provenance. Reordering whole
repeated parts preserves associations. Supply the generated explicit rename plan
for intentional dynamic/repeated moves; original names compare by OPC part-name
identity. Source scopes and internal relationships rebind simultaneously, including
combined runtime codec selection and Save. External URI strings do not enter
internal rename matching. Missing, ambiguous, removed and colliding targets reject
before aggregate/file publication.

C package owners retain singleton `_srl_xml_<role>` pointers and repeated
`xml_state`; `as_input` borrows them and aggregate free owns them. Rust aggregate
`xml_companions` owns role/original-physical-name associations independently from
unchanged `ModelPart` and native model layouts. Retain this store with reordered
values. Generated document adapters capture presentation independently from a
selected decoder and borrow the exact model/sidecar during encoding. Rust adds
`encode_renamed_profiles` and `save_renamed_profiles`; C selected helpers add
`_encode_renamed_with_profiles` and `_save_renamed_with_profiles`. Existing methods
remain available. XML companions never become native JSON/BINARY_NONE fields.

Use the supplied callback operation/context. Native codec traversal, temporary
XML replay, retained XML state and final publication share work/allocation and
preserved-byte admissions with first-failure cancellation. These are logical
resource contracts, not hard limits on opaque provider/factory internals or
process peaks. The [executed pipeline record](../verification/verification-package-pipelines-2026-10-22.md)
separates each actual SDK result from the pending final combined regression.
