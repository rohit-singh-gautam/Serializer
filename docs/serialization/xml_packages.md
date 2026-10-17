# XML, compression streams and packages

Compiler/runtime **2.4.0**. XML generation covers all output languages; the stream
and package API examples below use C++20. These are independent, opt-in APIs. Existing
schema language **1.17.0**, native wire formats and managed journals are unchanged.
See [remaining work](../xml_pending.md) and the [full design](../design/xml_support.md).

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

Build with `SERIALIZER_WITH_XML=ON`; the default is OFF. This requires
`LibXml2::LibXml2`. With the repository vcpkg manifest, select the optional `xml`
feature as well. Its minimal libxml2 dependency omits optional encoding backends.
Public Serializer headers do not expose libxml2 headers. Installed CMake packages
discover the dependency and export the `SERIALIZER_WITH_XML` target capability.
Libxml2 uses the [MIT license](https://gnome.pages.gitlab.gnome.org/libxml2/).

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
wrappers are supported as described below. XML omission selectors,
generic/managed/view models, revision lifecycles, magic, digests, maps, variants,
mixed content and `xsi:nil` remain unsupported.
Optional collections, collection items that are optional, and nested collections
are also unsupported. There are no specialized character, date/time, decimal,
QName, base64 or hex-binary adapters; supported byte integers remain ordinary
numeric scalars or repeated elements.

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
These mappings are currently configured without a fixed XSD; explicit text and
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
DOM adapters, including JavaScript and Kotlin, can build provider trees before
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

Most adapters use bounded whole-document trees and complete XML byte strings;
JavaScript, Rust and C encode directly into bounded output. Further tree-free paths
remain pending. Names use an ASCII NCName subset; values are XML 1.0 UTF-8. DTDs,
user-declared/external entities and non-1.0 XML are rejected. Predefined entities
and character references are supported.
Bundled parsers load no XML external resources; caller-supplied validators must
enforce the same rule. Namespace identity uses URI plus local
name. Rewriting does not preserve comments, prefixes, processing instructions,
QName-valued text or lexical formatting; this is a typed data mapping.
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

The optimized subset accepts bounded local XSD 1.0 import/include bundles,
inline/named root complex types, nested named complex record bindings with global
roots, singleton nested sequences, primitive typed elements/attributes, global
references and facet-free primitive simple aliases. The
[XSD tooling guide](xsd_tooling.md) defines the exact resolver, bundle identity,
export and compatibility contract. Unsupported declarations, facets, identity
constraints and conflicting bindings fail generation. No runtime XSD files or
general validator calls are required by the generated default path.

Primitive bindings are `string`, `boolean`, signed/unsigned 8–64-bit integers,
`float` and `double`. XSD-bound enums, constrained custom simple types,
defaults/fixed values and facets are rejected; annotations are ignored as metadata.
One fixed XSD bundle applies to the
complete generation unit: each emitted class needs a compatible global root
declaration; there is no per-class XSD-file selection.

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
rejected. Internal query/fragment targets are unsupported.
Applications supply vocabulary rules, expected types/root names, relationship
cardinalities, payload key/reference checks and business validation. The library
checks archive/metadata integrity, relationship uniqueness and target existence.

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

These helpers add no Serializer history, journal or session parts. Application
profiles must keep that state in side channels. The generic builder accepts
caller-provided payloads; it does not identify or police their application meaning.
These helpers are not attached to native managed stores and do not establish an atomic
document/sidecar transaction. A managed dual-codec document bridge and recovery
generation binding remain [pending](../xml_pending.md).
