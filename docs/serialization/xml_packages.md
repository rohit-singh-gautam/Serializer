# XML, compression streams and packages

Compiler/runtime **2.15.0**, schema language **1.20.0**. Baseline XML generation
covers all output languages; the stream and package API examples below use C++20.
Supported XML feature combinations are qualified per backend. Generated OPC
profiles are available in C++, Python, Java, JavaScript/TypeScript and Go;
unsupported backend combinations reject before output publication. Existing
native wire formats and managed journals are unchanged.
See [remaining work](../pending.md) and the [full design](../design/xml_support.md).

Compiler/runtime **2.14.0** also supplies selected asynchronous
[JavaScript/TypeScript ZIP/OPC APIs](../languages/javascript_packages.md):
bounded readers/writers, typed XML aggregates, extension policies, canonical
metadata and explicit Node filesystem Save. Stored ZIP remains browser-import
safe. The [actual qualification](../verification/verification-javascript-packages-2026-10-20.md)
records independent inputs/outputs, C++ interchange and Windows publication
faults. Automatic nested cumulative contexts, arbitrary runtime codec dispatch
and generated application key/reference checks remain pending.

The selected [Java package API](../languages/java_packages.md) uses Java 17
standard providers for the same typed XML/opaque package workflow, with bounded
random sources, explicit private spooling and staged filesystem Save.
The [Java qualification](../verification/verification-java-packages-2026-10-20.md)
records strict actual JDK 17 compilation/execution, separate Linux JRE execution,
independent ZIP/XML decisions and C++ exchange. Java-array output limits and the
remaining automatic nested budget/runtime profile/reference work stay explicit.

The selected [Go ZIP/OPC API](../languages/go_packages.md) uses Go 1.22 standard
APIs for ZIP32/ZIP64, descriptors, bounded sources/spooling and staged Save.
Schema packages add typed owning XML facades, metadata/relationship checks,
transactional loading, preservation and canonical output. The
[physical ZIP proof](../verification/verification-go-packages-2026-10-20.md) and
[OPC proof](../verification/verification-go-opc-packages-2026-10-20.md) record actual
Windows/Linux SDKs, independent archives, C++ exchanges and Save/quota faults.
Automatic nested cumulative native-codec accounting, runtime profile dispatch
and generated application key/reference checks remain pending.

The [token codec guide](xml_tokens.md) describes the new generated direct paths
and backend-specific advanced mappings. The [typed OPC example](../../example/opc/README.md)
uses a commented shared schema and C++/Python consumers to fill XML and binary
attachment roles, with independent ZIP/XML and cross-language checks.

The [advanced shared contracts](xml_advanced_contracts.md) add compiled scalar,
particle and identity validation, broader local XSD tooling, concrete model and
projection metadata, document profiles and a separate restricted HTML fragment
adapter. Existing C++ package facades expose owning `document_profiles()` metadata
for declared role shape, envelope defaults, expected model/root and vocabulary.
These compiler/reference contracts do not close dependent generated-language
codecs; unsupported combinations still reject before output publication.

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
add selected C++/Python nil/map/variant mappings, Python mixed content, non-C++
concrete generics, and Go/Python inherited models. Supported explicit presence
and map contracts also allow optional wrapped collections. XML omission,
managed/view/revision/magic/digest mappings, optional collection items and nested
collections remain gated. QName/list/decimal lexical handling requires its
documented rich fixed-XSD path; specialized character/date/time/base64/hex-binary
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

Output counters are not yet identical across backends. In particular,
provider-generated namespace declarations are not charged as authored output
attributes by every adapter, while input parsers can count those declarations.
An unusually tight attribute policy can therefore reject an otherwise valid
re-encoded document on input. Qualifying and aligning those counters remains
part of the pending cross-backend resource contract.

All outputs now have direct generated output paths. Supported C++/Python token
graphs and Java/C#/Go/Kotlin input use direct/pull traversal; richer C++/Python mappings
and other input adapters retain their documented trees. Complete output bytes
are staged. See [token paths and boundaries](xml_tokens.md). Configured names use
an ASCII NCName subset; values are XML 1.0 UTF-8. DTDs,
user-declared/external entities and non-1.0 XML are rejected. Predefined entities
and character references are supported.
Bundled parsers load no XML external resources; caller-supplied validators must
enforce the same rule. Namespace identity uses URI plus local
name. Rewriting does not preserve comments, prefixes, processing instructions or
lexical formatting. QName-valued fields need the explicit namespace-aware
adapters described below; ordinary strings do not retain QName semantics.
Schema-location hints do not select a validator. The strict generated mapping
rejects undeclared attributes, including unbound `xsi:schemaLocation` hints.

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
callbacks. Limits bound scratch and part/archive sizes. DEFLATE checks cancellation
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
payload keys. Register `add_target<Model>(scope, key, {part, field_path})`, retain
forward references with `require_target<Model>`, then call `validate()` before
returning from the aggregate callback. The shared budget bounds key/index storage,
reference counts and validation work. Duplicate keys within a scope, missing
targets and wrong model types report the source part, field and key. Equal keys
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
Generated C++/Python facades also retain loaded fixed singleton typed-role names in
`preserved_extensions::relocations` / `PreservedExtensions.relocations`. Passing
the loaded extensions to generated Save automatically rebinds preserved source
and internal target names to the role's declared canonical output path. Rebinding
is simultaneous, compares OPC ASCII identities, and resolves relative targets in
their original source scope before changing either endpoint. Query/fragment
suffixes remain verbatim; external targets are inert and unchanged even when their
source moves. Unknown part bytes and caller-owned extension records are unchanged.
Conflicting origins, removed targets and source-local ID collisions still reject
before file replacement. Empty relocation lists retain the caller-authored API's
existing behavior. Dynamic role renames and other generated package backends need
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
Applications supply their supported package/XML encoder and decoder, parser
budgets and document projection. Built-in profile integration and complete
format/provider/platform qualification remain [pending](../pending.md).

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

C++ and Python additionally support typed lexical lists, QName/prefix-list
context, supported scalar facets/defaults, unions, patterns, restricted repeated
choices and declared foreign XML extensions. These richer contracts reject on
backends that cannot enforce them. C++ preservation adds an XML-only one-pointer
`xml_extensions` slot only when requested;
its opaque storage allocates on the first retained unknown value. Use `.get()`
to test whether storage exists and `.value()` to inspect retained data.
QName/prefix-list mappings similarly retain only their needed namespace bindings.
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
An optional `validate(candidate, budget)` callback rejects domain-invalid data
before publication. Pass retained `loaded.extensions` as `Input.extensions` to
preserve permitted unknown parts and links after editing. External targets remain
inert. Save rereads its staged candidate before replacement; a failure after
replacement reports `publication_uncertain`.

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
Direct builder callbacks can use `budget.xml_context()` for the same composition.
The borrowed context and callback captures must outlive the synchronous operation.

Ordinary XML input remains UTF-8. C++ and Python OPC infrastructure metadata additionally
accepts BOM-marked UTF-16 in either byte order; its declaration must be absent or
identify `UTF-16`. Unsupported/conflicting encodings reject, and generated output
remains UTF-8. C++ direct raw parsing can select
`operation_context.input_encoding = input_encoding_policy::utf8_or_utf16`.
Raw parsing can also opt into `allow_mixed_content`; this retains ordered text
segments for infrastructure processing and does not relax typed field readers.

Generated C++ package profiles stage complete encoded parts. Their ordinary rich
XML entry point can retain bounded trees; token-required contracts use the new
direct path, and bare XML callers can select `serialize_tokens` explicitly.
Borrowed model inputs avoid duplicate owning models, but do not promise zero-copy
XML, bounded constant memory or tree-free streaming. Non-XML generated models gain
no package fields or runtime checks. Performance qualification and non-C++ context
parity remain tracked in [pending work](../pending.md).
