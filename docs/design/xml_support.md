# XML and package support design

Status: implementation specification and remaining requirements, **2026-10-17**.
Compiler/runtime **2.4.0**, schema language **1.17.0**. See the
[implemented API and exact subset](../serialization/xml_packages.md),
[generated-language examples](../../example/xml/README.md) and the
[central pending backlog](../xml_pending.md). XML generation now covers all output
backends; compression streams and ZIP/OPC have a C++ implementation. Cross-language
parity and qualification remain completion gates. Examples marked **proposed**
remain design sketches, not accepted syntax or options.

The 2.4.0 implementation adds configured simple text with attributes and wrappers,
pinned local XSD bundles and subset export/compatibility tools, native positional
format selection across backends, and C++ ZIP64/spooling/aggregate profiles.
The pending report remains authoritative for unimplemented requirements; these
increments do not complete all phases of this specification.

The [OPC/3MF authoring requirements](opc_3mf_support.md) refine the generic
package requirements with an application/framework responsibility table, a
restricted manual schema/build route and the supported declarative C++ bindings.

[XML and XSD](#recommended-scope) ·
[Multipart packages and OPC](#multipart-packages-and-opc) ·
[Compression matrix](#compression-support-matrix-and-composition) ·
[Document-file profiles](#reusable-document-file-profiles) ·
[Dual codecs](#separate-external-and-internal-codecs) ·
[OPC APIs](#opc-apis-and-generated-application-profiles) ·
[Implementation phases](#enhancement-plan-and-release-gates) ·
[Feature catalog](../project/feature_status.md)

## Recommended scope

Provide an **optional XML data codec**, with explicit member mapping and optional
external XSD validation. General features, including XML, compression and packages,
must support every generated language; missing backends are pending work, not
language-specific exemptions. Qualify each backend. Only features inherently tied
to a language may differ. Keep existing output and dependencies unchanged unless
selected. Full XML document editing and HTML templating need separate models.

**Optimization is the default for every selected format and supported backend.**
For XML, treat the XSD and its complete import/include bundle as fixed build
inputs. Generate specialized encoders, decoders and required checks; the default
application needs no XSD files, schema interpreter or general XSD validator.
Changing that build-time bundle requires regeneration and recompilation.
**Runtime XSD loading and validation must also be selectable during generation**
through CLI/INI configuration. This adds a runtime validator to optimized codecs;
it is explicit opt-in, never an automatic fallback.

```text
Serializer model
      |
      +-- existing binary / JSON
      |
      +-- optional XML mapping
                |
                +-- XML bytes
                +-- generated contract checks
```

**Answers at a glance**

| Question | Decision |
| --- | --- |
| XML today? | Generated owning codecs and a fixed XSD subset; see the implemented guide and pending report. |
| Attribute or child? | Derive from supplied XSD; otherwise default to children. |
| Must XML have a schema? | No; XSD is an optional extra contract. |
| How to apply XSD? | Compile a fixed bundle into optimized codecs by default. |
| Is HTML XML? | Ordinary HTML is a separate syntax. |
| New language constructs? | Small XML metadata; defer HTML grammar. |
| ZIP for other codecs? | Optional codec-independent ZIP input/output, including binary and JSON. |
| Does XML provide OPC? | XML is one component; add ZIP storage and package rules. |
| Saved history in these document files? | No; full-file Saves, with history/recovery/session state in side channels. |
| XML file with binary internal state? | Separate codecs; proposed qualified internal `binary_none` profile. |

## Attribute and child mapping

An XSD declares which values are attributes and which are elements. For
Serializer-authored documents, the Serializer mapping must make that choice.
Names, primitive types and numeric field IDs cannot determine it.

**Proposed mapping sketch**, independent of final annotation spelling:

```text
order -> root {urn:orders}order
  id: uint32
    -> attribute {}id
  customer: string
    -> child {urn:orders}customer
  items: array string
    -> repeated child {urn:orders}item
```

Resulting document:

```xml
<order xmlns="urn:orders" id="7">
  <customer>R&amp;D</customer>
  <item>A</item>
  <item>B</item>
</order>
```

```text
order
  @id = 7           attribute
  customer = R&D    child
  item = A          repeated child
  item = B
```

Use scalar attributes for explicit metadata, child elements for nested records
and normal collections. Attributes have no child markup or significant order;
they cannot repeat with the same expanded name. Whitespace-separated list
attributes require an explicit list mapping. A separate **text** mapping permits
simple content with attributes, such as
`<price currency="USD">12.50</price>`.
[XSD Primer](https://www.w3.org/TR/xmlschema-0/#basicExample)

Proposed metadata must cover root name, namespace URI, local name, attribute,
element, text, collection item name and optional wrapper. Allow an XML-specific
name without changing existing JSON/binary names. Emit attributes before child
content; validate ambiguous names and incompatible mappings at generation time.

When an XSD is supplied, use it as the XML contract: derive element/attribute
placement, names, order and constraints without repeating those declarations in
`.serializer`. Bindings may still associate model members with XSD declarations,
select a root or resolve alternatives the XSD permits; do not guess ambiguous
associations. Reject unsupported or conflicting bindings. An XSD can allow more
than one shape, so generation must select a documented representation.

Without an XSD, prefer portable Serializer mappings and deterministic defaults.
A sidecar mapping can avoid an initial grammar change, but introduces another
contract to synchronize; the current sidecar is a compatibility route, not the
target minimum-coding interface. Put authored XML namespace/placement defaults
and field exceptions with `.serializer` declarations so users do not repeat
field lists in INI. With an XSD, derive these details from it. Keep generation
and provider policy in configuration. Language `1.20.0` implements inline metadata
and schema association for the supported subset. These inputs form a fixed build
contract with optimized generation by default. See the
[single-authoring workflow](opc_3mf_support.md#schema-first-authoring-and-declaration-import).

### Configuration ownership

Most controls can be configuration. Proposed division:

| Location | Controls |
| --- | --- |
| Supplied XSD plus model bindings | Authoritative XML shape; no duplicated placement metadata. |
| `.serializer` without XSD | Target location for XML shape/defaults and local exceptions, authored once. Current sidecar mappings remain a supported fallback. |
| Generator CLI/INI | XML inclusion, provider/build policy, configured XSD bundle and runtime XSD support; the target interface must not require duplicate authored field lists. |
| Runtime API/configuration | Limits, compiled codecs and trusted XSD preparation when enabled. |

**Implemented INI settings** for the supported fixed-XSD subset:

```ini
[xml]
enabled = true
xsd = schemas/order_v1.xsd
# Optimized generation is the default.
# No external validator is enabled.
```

To permit runtime validation, add these generator settings:

```ini
[xml]
runtime_xsd = true
validation_scope = input
```

Equivalent CLI overrides are `--xml.runtime_xsd true` and
`--xml.validation_scope input`. The default is `runtime_xsd = false`;
`validation_scope` selects `input`, `output` or `both` when enabled. Combine these
keys with the existing `[xml]` section rather than repeating the section.

Enabling this option permits the generated runtime validator hook. Bundled
prepared providers and caller-supplied provider interfaces differ by backend;
see the [implemented provider contract](../serialization/xml_packages.md#fixed-xsd-and-optional-runtime-validation).
The application can prepare a trusted schema at startup or explicitly select
another prepared schema later. Without this option, generated codec calls never
invoke a general validator and reject a supplied one. Both configurations keep
static model mappings and mandatory generated checks. Runtime libraries may
still contain shared optional XML provider code; per-model opt-out does not
promise physical removal of all provider symbols from an executable.

A simple integer remains a **child by default when no XSD is supplied**:
`<order><count>7</count></order>`. Explicit attribute mapping gives
`<order count="7"/>`. A configurable scalar-attribute default is possible; nested
records and normal collections still require elements.

Without XSD, resolve placement as explicit member mapping, then model default,
then project default. Freeze those defaults with the schema: changing them changes
XML representation and requires regeneration. Runtime policy may add validation,
but must not silently remap generated children into attributes or disable required
generated checks. No inline HTML grammar is needed for these choices.

## Referencing and applying an XSD

The default decoder combines these checks with parsing:

```text
XML bytes
    |
    v
Well-formed XML and namespaces
    |
    v
Generated mapping and value checks
    |
    v
Application invariants
    |
    v
Commit replacement model
```

The diagram shows logical validation stages. Current adapters generally parse a
bounded tree and then traverse typed fields; a fused generated token traversal
remains an optimization target. An explicitly enabled general validator may
supplement them. Some constraints
require document-wide state. Success must cover the entire document before
committing the destination.

An XML document can advertise its XSD:

```xml
<order
  xmlns="urn:orders"
  xmlns:xsi=
    "http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="urn:orders order.xsd"
  id="7">
  <customer>R&amp;D</customer>
  <item>A</item>
  <item>B</item>
</order>
```

`xsi:schemaLocation` contains namespace/location pairs and is only a hint.
`xsi:noNamespaceSchemaLocation` serves documents without a target namespace.
Neither invokes validation. Configure the validator with an explicit local
schema bundle only when that optional integration is selected. The generator's
resolver must allow only pinned imports/includes; neither it nor a runtime
validator should fetch document-supplied locations. A namespace URI identifies
a vocabulary; it need not point to an XSD.
[XSD Structures](https://www.w3.org/TR/xmlschema11-1/)

Require the configured root/schema to apply: an undeclared-root warning or lax
validation fallback must not count as successful contract validation.

Complete small **XSD 1.0** for the example:

```xml
<xs:schema
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  targetNamespace="urn:orders"
  elementFormDefault="qualified"
  attributeFormDefault="unqualified">
  <xs:element name="order">
    <xs:complexType>
      <xs:sequence>
        <xs:element name="customer"
          type="xs:string"/>
        <xs:element name="item" type="xs:string"
          minOccurs="0" maxOccurs="unbounded"/>
      </xs:sequence>
      <xs:attribute name="id" type="xs:unsignedInt"
        use="required"/>
    </xs:complexType>
  </xs:element>
</xs:schema>
```

Choose one source of truth per integration:

| Workflow | Responsibility |
| --- | --- |
| Serializer first | Generate codecs from mappings; optionally export an XSD. |
| Existing XSD first | Bind the supported subset to supplied models today; derive XML mappings. Generating `.serializer` declarations from XSD remains proposed. |
| General runtime validation | Explicit opt-in for supplementary or independent checks. |

No XSD is required for XML. Offer generated XSD export for documentation,
external consumers and independent validation; do not require a second
hand-maintained schema. Export the actual supported contract and diagnose
anything it cannot express. Do not infer a complete schema from sample XML.

```text
XSD supplied
  XSD + model bindings
           |
           v
     generated codecs

No XSD supplied
  model + XML defaults
           |
           +-- generated codecs
           +-- optional generated XSD
```

### Build preparation and runtime checks

**XSD means XML Schema Definition:** it describes XML structure and value
constraints. The standard permits different implementation strategies; Serializer
selects build-time processing and optimized generated code as its default.
[XSD overview](https://www.w3.org/TR/xmlschema11-1/)

| Proposed approach | What executes |
| --- | --- |
| Default build-time processing | Read the fixed XSD bundle; emit specialized codecs and required checks. |
| Default generated validation | Run direct checks on actual values; no runtime XSD file needed. |
| Runtime XSD support enabled during generation | Include the validator API; load/prepare a trusted XSD at runtime. |

Generated checks are **runtime checks with rules fixed during generation**.
A compiler cannot validate future input values: a dynamic count restricted to
1..100 still needs checking. Static structure/type guarantees can remove proven
checks; XML parsing and unproven value/occurrence constraints remain. Only XML
documents already available during the build can be fully validated then.

The default must generate every applicable check for its declared XSD subset.
Reject unsupported constructs during generation; never silently skip constraints
or switch to a general validator. Explicit `runtime_xsd = true` may cover
separately qualified constraints through the included validator. Validation scope
selects input, output or both; generated contract and resource checks remain
mandatory. Diagnose unavailable validator dependencies when the option is enabled.

An encoder can emit known names, attributes and order directly, avoiding runtime
schema interpretation. Escaping, legal characters and dynamic value constraints
still need checking unless the model types prove them. A decoder can combine
generated checks with parsing in one traversal; no separate general-validation
pass belongs to the default. Measure throughput, allocations and generated code
size before claiming a performance improvement.

For the optional general-validator integration, prepare schemas once, not per
message. For example, .NET distinguishes schema compilation from document
validation and reuses already compiled schema sets.
[Schema compilation](https://learn.microsoft.com/en-us/dotnet/api/system.xml.schema.xmlschemaset.compile),
[document validation](https://learn.microsoft.com/en-us/dotnet/standard/data/xml/xml-schema-xsd-validation-with-xmlschemaset)
Generated checks can share decoding work; a general validator may add work or
another pass. Neither is automatically “super slow.” Measure schema preparation
separately from per-document latency, memory and throughput; keys and assertions
can require more state/work. No XML performance measurements exist for Serializer.

### Decoding the wrong contract

Decode against an explicitly selected root, namespace and accepted version.
Do not try arbitrary XSDs or switch contracts because input advertises a location.
Dispatch among multiple supported versions only through an explicit application
policy. Each decoder enforces its own contract before committing the model.

If the contract requires child `count`, an attribute named `count` does not
satisfy it. Reject missing required members, unexpected/duplicate members,
invalid order/counts, bad lexical values and range/facet violations. Preserve
well-formedness, namespace and resource checks even without a general validator.
Allow compatible unknown fields only where the chosen contract/policy permits.

An XML document does not inherently identify the exact XSD used by its sender.
Two schema versions may accept the same document. Matching a root/namespace alone
does not prove a version match; use explicit protocol versioning where needed,
and check the complete selected contract.

### XSD version lifetime

An XSD does not change itself at runtime, and publication does not make its file
immutable. Publishers can revise schemas. XSD supports vocabulary evolution
without mandating hot reload or one universal versioning policy.
[W3C versioning guidance](https://www.w3.org/TR/xmlschema-guide2versioning/)

Adopt an explicit project rule: every changed released contract gets a new
version; XSD does not assign that version automatically. A compatible revision
can keep its namespace; a breaking revision needs an explicit migration and
version-selection policy. Treat each released contract as immutable: pin its
complete XSD/import/include bundle by version and content hash. Changes to the
build-time bundle require regeneration and recompilation. Track every transitive
schema as a build dependency so changes cannot leave stale generated code.

With runtime XSD support enabled, the application may load a schema through the
trusted preparation API without rebuilding. This can add constraints within the
compiled contract; it cannot remove generated checks or expand the codec's model
representation. Changes to generated names, namespaces, placement, types or
structure still require regeneration and recompilation. Enabling runtime support
does not imply automatic hot reload or loading `xsi:schemaLocation`. Bind each
document to one prepared schema; select compiled versions through explicit policy.

Import/export must report unsupported constructs; reading an XSD alone does not
check actual document values. Do not promise arbitrary XSD round-tripping. Start
optional general validation with an explicitly qualified XSD 1.0 provider;
XSD 1.1 assertions/type alternatives require separately qualified support.
[XSD 1.1 additions](https://www.w3.org/TR/xmlschema11-1/)

If “quote the schema” means literal quoting: XML attribute values require
quotes. Escape `&`, `<` and the chosen quote delimiter, including in schema
location strings. Never paste raw user text into markup. The XML declaration
`<?xml version="1.0"?>` selects XML syntax, not an XSD or Serializer version.
[XML syntax](https://www.w3.org/TR/xml/#sec-starttags)

## XML complexities that affect the model

**Namespaces.** Match `{URI, local name}`, not prefix spelling. In the example,
`order` is in `urn:orders`; unprefixed `id` has no namespace. Prefix rebinding,
default-namespace resets, qualified attributes and reserved `xml`/`xmlns` names
need tests. Duplicate attributes are checked by expanded name.
[Namespaces in XML](https://www.w3.org/TR/xml-names/#defaulting)

**Presence and defaults.** Preserve these distinctions where the contract needs
them:

| Input | Meaning |
| --- | --- |
| No `note` element | Absent. |
| `<note/>` | Present, empty. |
| `<note></note>` | Same empty value. |
| `<note xsi:nil="true"/>` | Explicit nil; needs an in-scope `xsi` binding. |

XSD nil requires a nillable declaration; it forbids text/child elements but can
retain attributes. An omitted attribute can receive an XSD
default; an element default applies to an existing empty element, not an absent
element. Do not silently merge XSD defaults with Serializer defaults. Decide
whether validation supplies defaults to decoding. One optional value cannot
represent absent, nil and present separately; add an explicit presence model
only where required.
[XSD defaults and nil](https://www.w3.org/TR/xmlschema-0/#Nils)

**Order and content.** Sequences, choices, unordered `xs:all` groups and repeated
groups need explicit models. Separate arrays cannot retain heterogeneous
interleaving. Mixed text and elements need an ordered sequence of alternatives:

```xml
<p>Hello <b>world</b>!</p>
```

```text
content[0] = text("Hello ")
content[1] = element(b, "world")
content[2] = text("!")
```

First release: element-only records plus simple text-with-attributes. Reject
mixed-content bindings; do not flatten them. Defer repeated choices, wildcard
capture, substitution groups and dynamic `xsi:type` dispatch until their type
and ordering contracts are implemented.
[XSD content models](https://www.w3.org/TR/xmlschema-1/#Complex_Type_Definitions)

**Typed values.** Define locale-independent lexical forms, checked numeric
ranges, enum names and base64/hex bytes. Serializer's byte-valued `char` needs a
numeric mapping, not arbitrary XML characters. XSD decimal is not binary floating
point; arbitrary integers, dates/time zones, durations, whitespace facets and
QName-valued text require explicit adapters. Specify Boolean alternatives and
`INF`/`NaN` policy. Enforce XSD fixed values and pattern, enumeration, length and
range facets during validation. Unsupported types must diagnose, never silently
narrow.
[XSD datatypes](https://www.w3.org/TR/xmlschema11-2/)

**Identity and evolution.** XSD keys/key references and ID/IDREF are document
constraints, not Serializer managed identities. Define map-entry shapes,
inheritance layout, recursion and reference handling; reject object cycles
without an explicit reference representation. XML compatibility depends on names,
namespaces, order and occurrence constraints. Stable numeric IDs alone do not
preserve it. Treat unknown attributes/elements as explicit reject, skip or
capture policies, with strict rejection as the proposed default.

**Data versus original spelling.** Record round-trips need not preserve prefixes,
attribute order, quote style, entity spelling, CDATA boundaries, comments or
processing instructions. A document editor needs a separate preservation model.
[XML Information Set](https://www.w3.org/TR/xml-infoset/)
Stable output is also distinct from Canonical XML; signatures require a selected
canonicalization/signature implementation. Defer them to a specialized adapter.
[Canonical XML 1.1](https://www.w3.org/TR/xml-c14n11/)

## Parser and resource contract

Proposed baseline: namespace-aware XML 1.0, UTF-8 output, UTF-8/UTF-16 input.
A UTF-8-only initial reader must advertise that narrower profile. Reject DTDs,
external entities and malformed/illegal characters; permit predefined entities
and valid numeric references. Perform no automatic XInclude or external-resource
loading. Handle XML line-ending and attribute-value normalization, CDATA as
text, and `xml:space` deliberately. Preserve text whitespace; pretty printing
must not change text/mixed content.
[XML processing rules](https://www.w3.org/TR/xml/)

Use a maintained event/pull parser behind a small adapter rather than extending
the JSON scanner into an XML parser. Expat is an event-parser candidate; select
the XSD validator separately. Qualify dependency licenses, platform packages,
security updates and allocator/limit controls before choosing a provider.
[Expat API](https://libexpat.github.io/doc/api/latest/)

Extend existing decode budgets with attributes per element, namespace bindings,
name/text bytes and parser/validator work. Skipped subtrees still consume budgets.
Account for validator identity tables and temporary buffers separately: streaming
does not imply constant memory or a hard process-memory limit. Return location,
expanded name and member path in bounded diagnostics.

Decode into a temporary owning model; commit after parsing, mapping and optional
validation succeed. Input may be consumed on failure. Streaming output may leave
a prefix after failure; offer staging when callers require atomic output or
validation before release. Limit decompressed bytes when compression is used.

## HTML and XHTML boundary

HTML served as `text/html` uses HTML syntax and parsing rules. XML-form HTML
(commonly called XHTML) uses XML rules when processed as XML, for example with
`application/xhtml+xml`; its HTML elements use `http://www.w3.org/1999/xhtml`.
Looking like XML does not select the browser parser.
[HTML versus XML](https://html.spec.whatwg.org/multipage/introduction.html#html-vs-xhtml)

```text
Requested output
      |
      +-- XML data -> XML codec
      |
      +-- XHTML subset -> XML binding
      |             + HTML conformance checks
      |
      +-- text/html -> separate HTML format
                       via dedicated adapter
```

| XML syntax | HTML syntax |
| --- | --- |
| `<br/>` | `<br>` is a void element. |
| `<p/>` is empty. | `<p/>` does not self-close. |
| Attributes need values/quotes. | `<input disabled>` is allowed. |
| Malformed input fails. | Parser repairs many malformed structures. |

HTML also has omitted tags, special case rules, raw-text script/style content,
foreign content and template fragments; parsing can insert or move nodes.
[HTML syntax](https://html.spec.whatwg.org/multipage/syntax.html),
[HTML parsing](https://html.spec.whatwg.org/multipage/parsing.html)

**Recommendation:** do not add HTML constructs to Serializer's language for the
XML feature. XML mapping can cover a declared XHTML subset; a complete document
often needs the deferred mixed-content model. Add real HTML support only for a
demonstrated use case through an HTML-aware library or template adapter.
If exposed as a Serializer protocol, HTML should have its own format identity
and implementation; XHTML remains an XML vocabulary, not a new XML wire format.
XSD validity and XML escaping do not provide HTML sanitization or safe script,
CSS or URL handling. The HTML standard itself discourages its XML syntax.
[XML syntax status](https://html.spec.whatwg.org/multipage/xhtml.html),
[script content rules](https://html.spec.whatwg.org/multipage/scripting.html#restrictions-for-contents-of-script-elements)

## Multipart packages and OPC

This section consolidates **all PKG01–PKG09 requirements and acceptance criteria**
from the former temporary package proposal, which it replaces.
PKG07 and acceptance gate 13 supersede the proposal's incremental-record clause
with the user's later requirement for whole-object compression streams. The
remaining requirements are preserved. C++ implements the documented ZIP32/OPC
subset; generated typed profiles and general backend parity remain pending.

Add an optional, **codec-independent ZIP/package layer** for single payloads,
related typed parts and opaque attachments. Support ZIP output and input for every
selected supported codec, including JSON, native binary and XML, plus mixed-codec
archives. Discover parts, check references and complete output through one bounded
operation. Qualify each codec/backend combination independently.

Reuse generated models/codecs, stream adapters, decode budgets and suitable
compression dependencies. Whole-message compression does not provide ZIP entries
or relationships; a byte-stream API does not establish incremental parsing.
XML parts reuse the same XML parser, mappings and object model.

### ZIP across all codecs

ZIP is an archive container around encoded bytes. Keep it independent of the
model codec and of the optional OPC profile:

```text
Model
  |
  +-- binary_none --+
  +-- JSON ---------+-> ZIP entry -> ZIP output
  +-- XML ----------+
  +-- other codec --+

ZIP input -> open/decompress entry
          -> selected decoder -> model
```

Support one model as one entry, many models as multiple entries, and mixed codecs
with binary attachments. ZIP's stored/DEFLATE choice is independent of each
entry's codec. ZIP creation must produce a real archive with entry framing,
integrity values and a finalized directory, not merely DEFLATE-compressed bytes.
[ZIP format](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT)

**Plain ZIP requires no XML/XSD, content-type manifest or relationships.** OPC
adds its standard metadata only when that profile is selected. Use explicit
expected codecs/types, or an application-declared profile for dispatch; filenames
and media-type labels alone do not prove a payload's schema or validity.

### Build selection and runtime execution

Choose the enabled codecs and ZIP capability during generation/build. When ZIP
is requested, fixed direct composition is the optimized default; share one ZIP
implementation instead of generating archive algorithms per model. Include only
selected dependencies. Existing unwrapped serialization remains the default when
ZIP is not requested.

Encoding, compression, CRC calculation and archive finalization run on actual
data at runtime. Compile-time selection removes unnecessary dispatch; it cannot
precompute bytes for unknown future values. A fixed pipeline can still accept
runtime entry names, counts, values and streams.

Offer a separate opt-in for runtime selection among explicitly enabled compiled
pipelines. Unsupported codecs/methods fail clearly; no automatic format guessing,
schema interpreter or runtime XSD dependency is required for ZIP.

**Proposed generator configuration sketch**, not implemented options:

```ini
[package]
container = zip
profile = none
selection = fixed
compression = deflate
```

Here `profile = opc` would enable OPC conventions; `selection = runtime` would
include runtime selection support. Fixed and selectable paths must use the same
wire contracts, checks and budgets. Applications can also explicitly compose a
codec with the reusable ZIP source/sink API without changing the schema language.

### Compression support matrix and composition

Keep **payload codec**, **container/profile** and **compression** independent.
Avoid combined codec identities such as `json_gzip` or `xml_zip`.

| Format/layer | Current support or proposed work |
| --- | --- |
| No compression | Existing codec bytes; default remains unwrapped. |
| gzip | Implemented C++ single-member whole-message compression. |
| zlib / raw DEFLATE | Implemented C++; distinct framing choices. |
| Zstandard / LZ4 frame | Implemented C++ whole-message compression. |
| ZIP stored / DEFLATE | C++ named-entry archive input/output and generated Python package support; broader parity pending. |
| OPC | C++ and generated Python profiles over ZIP; selected XML metadata and arbitrary part bytes. Broader parity pending. |
| Whole-object compression streams | C++ composable output wrappers and bounded whole-message input wrappers; other backends pending. |
| Brotli / XZ / bzip2 | Not bundled; caller-written custom backends are possible. |

The implemented compression helpers cover eight owning C++ codecs when XML is
selected: XML, JSON, three native binary modes, Protobuf binary, ProtoJSON and TextProto. Other generated
backends do not yet have equivalent bundled helpers. Preserve current exact
single-frame/member, checksum, trailing-data and resource policies. XML and
package paths require their own qualification alongside ordinary codec tests.
[Compression contract](../serialization/compression.md)

Standard JSON followed by standard gzip already works in C++:

```cpp
namespace codec = rohit::serializer;
codec::serialize_to<codec::json>(
    output, value,
    codec::compression::gzip_options{.level = 6});
```

Use the enabled zlib backend (`SERIALIZER_WITH_ZLIB=ON`). The result is gzip
containing JSON, with no Serializer wrapper. Existing options-variant/span
helpers stage bounded whole messages in both directions. The new C++ output
stream wrapper maintains compression state across byte batches; its input wrapper
stages one bounded message. Both serialize one complete object.
[Working APIs](../guides/usage.md#compress-complete-messages)

Provide whole-object input/output filters that satisfy Serializer
stream concepts, backed by reusable compression state. Keep a convenience
serialization parameter/profile that composes the same pipeline. Fixed generated
composition is the optimized default; optional runtime selection uses only enabled
backends. Actual compression still executes on runtime bytes.
Serialization always handles the entire object. Internal byte batches do not
introduce incremental records or object edits. Input wrappers may retain a bounded
complete message; their memory behavior must be explicit per backend.

```text
JSON encoder -> gzip output stream -> file/socket
file/socket -> gzip input stream -> JSON decoder

XML encoder -> ZIP entry stream -> ZIP writer
```

Output requires explicit fallible `finish()` for trailers and archive finalization;
destructors only clean up. Input must validate trailers, truncation and exact
consumption before publishing a fresh candidate. Bound compressed/expanded bytes,
backend windows, work and allocations across layers. ZIP needs named entries and
an archive lifecycle, not a single unnamed compression filter. Avoid automatic
outer gzip around ZIP; ZIP's stored/DEFLATE policy applies per entry.
[Gzip framing](https://www.rfc-editor.org/rfc/rfc1952.html),
[zlib framing](https://www.rfc-editor.org/rfc/rfc1950.html)

**Proposed profile settings**, not implemented generator options:

```ini
[pipeline]
codec = json
container = none
compression = gzip
```

For ZIP select `container = zip` and a per-entry compression policy; for OPC also
select its package profile. Keep document export and side-channel pipelines
independently configurable. Use standard interoperable framing and record the
selected backend licenses/notices: zlib, Zstandard and LZ4 have permissively
licensed implementations; this does not grant blanket rights for every possible
ZIP extension.
[zlib license](https://zlib.net/zlib_license.html),
[Zstandard license](https://github.com/facebook/zstd/blob/dev/LICENSE),
[LZ4 library license](https://github.com/lz4/lz4/blob/dev/lib/LICENSE)

### Package terms and ownership

**Content-type metadata** labels the representation of each part: for example,
`metadata.json` as `application/json` and `photo.png` as `image/png`. OPC stores
extension defaults and per-part overrides in `[Content_Types].xml`. Serializer
reads/writes and checks that mapping; the application/profile declares its media
types and expected generated types. This does not choose XML attributes/children.
[Parts overview](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/parts-overview)

**Relationship discovery** finds a target by its declared role rather than a
hardcoded filename. **Resolution** interprets its URI against the source and
locates/checks the target:

```text
order part
   |
   +-- attachment relationship
                |
                v
           photo part
```

The target architecture assigns scoped IDs, URI resolution and
existence/type/cardinality checks to Serializer. The implemented C++ OPC subset
checks scoped relationship IDs, internal target resolution and target existence;
generated typed reference/cardinality integration remains pending. The
application/profile declares what an attachment means, which roles
are required and how many are allowed. External links remain data unless a caller
explicitly supplies a resolver.
[Relationships overview](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/relationships-overview)

**Package integrity** checks archive headers/bounds, decompression, CRC and
finalization. CRC detects corruption; authentication/signatures are separate.
**Reference checks** verify declared part links and payload keys/references.
**Shared resource budgets** limit the complete operation, including decompression,
decoded allocations, reference indexes, retained parts, staging and work. A
100 MiB expanded-byte budget means 100 MiB for the package, not for each entry.

The following target ownership split includes mechanisms still pending in the
[implementation report](../xml_pending.md). The library owns reusable mechanics;
applications own vocabulary and policy:

| Serializer provides | Application supplies |
| --- | --- |
| ZIP source/sink, compression, integrity and finalization | Destination/streams and supported output choices. |
| Optional OPC metadata/profile implementation | Vocabulary-specific media types, roles and package rules. |
| Optimized codecs and declared-contract checks | Model/schema selection and contract-version policy. |
| Scoped reference validation and URI resolution | Key/reference declarations, scopes and cardinalities. |
| Shared budget enforcement, cancellation and cleanup | Numeric limits and cancellation decisions. |
| Candidate/staging and full-save replacement helpers | Business invariants, Save timing and external effects. |
| Explicit external-resolution hooks | Trust, authorization, credentials and access policy. |
| Preservation/reproducibility mechanisms | Required extensions and desired preservation policy. |

Applications should not have to reimplement ZIP headers, OPC relationship parsing,
decompression limits or generic reference checks. Serializer should not infer
business semantics or automatically load external targets. Single-part plain ZIP
does not require relationship or cross-part reference machinery.

### Reusable document file profiles

**Include optional document-file profiles in Serializer's scope.** Serialized
bytes can already be the application's file format when both ends agree on the
codec and schema. C++ file streams and managed save/journal formats are already
implemented. C++ also provides reusable ZIP/OPC builders and full-file Save
helpers; the managed document-state profile below remains proposed.
[File storage](../guides/usage.md#file-streams-and-journal-records),
[managed persistence](../managed/cpp_runtime.md#history-persistence-and-limitations)

```text
Application document contract
            |
Optional document profile
            |
Codec + optional ZIP/OPC
            |
Streams and file storage
```

Offer a native Serializer document profile that declares format identity/revision,
root schema and codec, payload revision, required capabilities/parts, attachments
and external state-channel binding. Keep profile, payload, schema-language and compiler
versions separate. Generate fixed profile composition by default; allow runtime
selection only among configured supported modes. Raw file output remains available
without added framing, manifests or package dependencies.

```text
Document package
  profile metadata
  complete current-state payload
  current attachments/

Separate side channel
  internal state and undo/redo history
  recovery baseline + append journal
  session/outbox state

Every file Save -> complete new file
               -> finalize and publish
```

History preserves user undo/redo; journals recover durable work after interruption.
**For these document/package profiles, history and recovery are side-channel-only,
and every saved file is a full Save.** Do not put history, active journals,
managed envelopes or session/outbox state into the final file. Live history may
be in memory and persist in a separate sidecar; collaboration uses its separate
transport. Sidecar-only recovery checkpoints must not modify the final file or
clear its unsaved-content state.

Reuse the existing managed/history engine through a new adapter. Keep current
native managed-envelope and appended/sidecar journal formats unchanged; the
side-channel-only restriction applies to the proposed external-file profiles.
Current journal APIs do not open arbitrary ZIP/OPC files, and other backends do
not automatically gain native managed engines.
[Current journal boundary](../managed/journal.md#stream-and-file-adapter-boundary)

Capture one committed snapshot and coordinate full-file publication with a
prepared side-channel baseline under the writer protocol. Finalize/flush the new
file before replacement; retire only covered recovery records after successful
publication, preserving retained undo/redo and any newer edits. Bind side channels
to document identity, schema/profile, saved-file digest, generation and sequence.
Use permitted profile metadata or the external binding; never insert private
wrappers into standard XML. Reject stale bindings. A missing sidecar can open the
complete final file as a fresh session without its former history. Qualify crash
boundaries explicitly; two files do not become atomic merely by being adjacent.

Attachments referenced by retained history must remain available. A future adapter
must order durable attachment writes before commits that reference them and retain
historical blobs in side storage until no history/recovery reference needs them.
Only current document attachments belong in the final file. Ordinary managed
transactions do not already make external files transactional. Dirty-state checks
must compare the final document's persistent projection with its last successful
Save; history/session changes alone need not change that file.

Serializer owns reusable framing/profile metadata, version checks, bounded reads,
attachment mechanisms, transactional load and qualified save/recovery adapters.
Applications own document vocabulary, file extensions/media types/associations,
business rules, migration and retention policies, and authorization. This profile
is opt-in for Serializer-owned files; never insert it into external standard parts
or redefine another application's file format. Preserve `.serializer` for schemas.
Deliver snapshot/profile export first; qualify managed import/history and live
package-journal integration separately.

### Separate external and internal codecs

**Use independent document and internal-state codec selections.** A final XML
file does not require XML history, journals or collaboration messages:

```text
One logical model
  |
  +-- XML/JSON/... -> full saved file
  |
  +-- binary_none -> side-channel history
  +-- binary_none -> side-channel journal
  +-- binary_none -> collaboration transport
```

For the new optimized profile, default to a generated `binary_none` internal
layout where qualified. Unsupported requested combinations fail generation;
never silently select another codec or generic fallback. Keep stable identity/ownership,
allocation high-water marks, retained history and session/outbox semantics. The
internal projection must retain recovery-required state and exclude transients;
external codec omissions must not silently remove internal state.

**Proposed configuration**, not current compiler options:

```ini
[document]
payload_codec = xml
internal_codec = binary_none
save = full
history = sidecar
journal = sidecar
```

The internal selection covers history, journal values and collaboration records;
their framing/protocols remain separate. Positional `binary_none` needs exact
layout agreement: bind codec, byte order, schema/layout fingerprint, ID width,
record versions and projection policy in sidecar headers and collaboration
handshakes. Reject mismatches before decoding; migrate state/history explicitly.
Keep bounded framing, integrity, sequences, commit checks and uncertain-write
fencing. Removing field keys does not remove these requirements.

Current native snapshots/envelopes and collaboration records use
`binary_integer`; `cpp.protocols = binary_none` currently rejects managed models.
This dual-codec adapter is proposed, not an existing option or a change to those
wires. Measure complete save/edit/recovery/network workloads before claiming that
the new internal encoding is faster.
[Native snapshot codec](../../include/rohit/managed.hpp),
[collaboration contract](../managed/collaboration_runtime.md),
[positional limitations](../guides/usage.md#count-and-emit-constant-positional-bytes)

### OPC scope and feature wording

**OPC is an optional package profile above codecs.** The C++ subset is implemented;
other backends and generated vocabulary profiles remain pending. Target
**ECMA-376 Part 2, fifth edition, December 2021**, with an explicitly documented
supported subset. Keep plain ZIP access distinct from OPC validation. OPC is a
reusable package contract; XML support alone does not establish its conformance.
[ECMA-376 parts and editions](https://ecma-international.org/publications-and-standards/standards/ecma-376/)

```text
Application vocabulary/profile
             |
OPC names, content types, relationships
             |
Named streams from any selected codec
             |
ZIP source and sink
```

Keep the components independently usable:

| Component | Responsibility |
| --- | --- |
| Generated codec | Encode/decode one typed part. |
| Package source/sink | Enumerate, open and write bounded named-part streams. |
| ZIP adapter | Archive framing, directory, compression and integrity. |
| Package profile | Content types, metadata, relationships and structural rules. |
| Validation context | Reference resolution and shared operation budgets. |

### OPC APIs and generated application profiles

Provide **both a reusable package builder API and optional generated profiles**.
The C++ builder is implemented; generated profiles remain proposed. See the
[current builder APIs](../serialization/xml_packages.md#plain-zip-and-opc).
The builder accepts part URI, media type, content stream/typed encoder and
relationships. It generates `[Content_Types].xml` and `.rels`, checks declarations
and references, and finalizes standard OPC/ZIP output. Applications should not
have to handwrite the infrastructure XML.

**Proposed pseudocode**, not a current API:

```text
package = opc_writer(output, limits)
part = package.add_part(
  "/document.xml", "application/xml")
encode_xml(snapshot, part)
part.finish()
package.link(
  root, "urn:example:main", "/document.xml")
package.finish()
```

Reserve package metadata names for writer-managed output. Offer validated
metadata import/preservation for existing packages, not arbitrary duplicate
`[Content_Types].xml` payload entries. Keep the logical package model separate
from its ZIP storage adapter.
[OPC part creation](https://learn.microsoft.com/en-us/windows/win32/api/msopc/nf-msopc-iopcpartset-createpart),
[package serialization](https://learn.microsoft.com/en-us/windows/win32/api/msopc/nf-msopc-iopcfactory-writepackagetostream)

A generated application profile declares fixed part roles/names, model types,
codecs, media types and cardinalities, then calls the same builder through an
optimized Save/load interface. Initial bindings may use INI/profile data or typed
code; no full document-markup language is required. The profile describes a
standard package, not a private Serializer wire envelope. Runtime values and
attachments fill its declared parts. Generic APIs allow dynamic parts when needed.

`finish()` validates and finalizes the package. A filesystem full-Save helper
stages the complete output before replacement; an arbitrary stream can retain
partial bytes on failure. Document-profile saves use the side-channel-only policy
above; a package builder does not implicitly add history or recovery parts.
Finish each entry before finalizing the package; reject unfinished/failed entries
and surface deferred size, CRC and stream errors before reporting success. The
logical package holds bounded metadata and stream providers, not mandatory
in-memory copies of every payload.

Illustrative package layout:

```text
package.zip
  [Content_Types].xml
  _rels/.rels
  data/order.xml
  data/_rels/order.xml.rels
  media/photo.bin
```

XML codecs handle metadata and selected XML parts. ZIP/OPC code must additionally
validate archive integrity, part identities, media-type coverage, relationship
targets and package budgets; XSD cannot supply those checks. OPC support is not
full DOCX/XLSX/PPTX support: application vocabularies, document semantics and
rendering are separate. ECMA-376 Part 3 markup compatibility also needs explicit
scope; preserving unknown XML does not implement `AlternateContent` or
`MustUnderstand` processing.
[OPC fundamentals](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/open-packaging-conventions-overview),
[ECMA-376 scope](https://ecma-international.org/publications-and-standards/standards/ecma-376/)

Use target wording such as **“Multipart packages with ZIP storage and
an OPC profile, typed parts from any selected codec, binary attachments and
relationship discovery.”**
Publish exact edition/subset and verification results before claiming support.

### PKG01 Named parts and standard metadata

- Read/write typed parts from any selected codec, including multiple XML parts,
  and opaque binary parts; do not encode attachments as XML text. Expose logical
  name, media type, declared size or explicit unknown size, observed byte counts
  and content stream. Plain ZIP media types may be unknown or caller-supplied.
- Support OPC media-type extension defaults and per-part overrides. A valid
  override may differ from its default. Reject conflicting duplicates and missing
  declarations according to the profile. Dispatch using the caller-selected
  profile and expected type; report unsupported media or retain it opaquely under
  an explicit preservation policy.
- For relationship-based profiles, discover entry parts by type and cardinality,
  with configurable payload filenames. For XML parts, bind expected root expanded
  names; reuse attribute, element, ordered/repeated child, presence/default and
  checked-scalar mappings.
  Standard XML parts need no extra Serializer envelope. Missing mapping features
  are XML dependencies, not reasons to add another parser/object model.

In OPC, logical parts are URI-addressable resources whose discovery is independent
of archive layout.
[Parts overview](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/parts-overview)

### PKG02 ZIP storage contract

- Use standard ZIP: stored and DEFLATE entries, arbitrary archive order, integrity
  verification and data descriptors. Check directory offsets, entry extents,
  arithmetic and local/central header consistency, allowing valid descriptor
  placeholders, before trusting a member. Reject duplicate logical identities.
- Declare ZIP64 support for sizes/counts/offsets within caller limits, or return
  a specific unsupported-feature error. Explicitly reject unsupported compression,
  encryption and multi-volume archives; never interpret them as stored entries.
- Reuse suitable compression libraries while separating ZIP framing and per-entry
  processing from whole-message compression APIs.

[ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT)

### PKG03 Relationships and payload references

- Represent package- and part-level relationships by source, source-local ID,
  relationship type, target URI and internal/external mode. IDs must be unique
  within one source set; the same ID in another source set is valid.
- Resolve internal targets against their source; check existence, expected media
  type and profile cardinality. External targets remain data with no implicit
  network/filesystem access. An optional caller resolver has its own policy/budget.
- Validate payload keys/references with declared key types, scope boundaries,
  forward references and duplicate/missing/wrong-type targets. Keep payload IDs,
  relationship IDs and runtime identities distinct; preserve authored IDs on
  export. Relationship cycles are legal unless the profile forbids them.
- Use URI resolution plus the profile's naming/equivalence rules, never OS-path
  normalization, blanket case folding or repeated percent decoding. Parent
  segments can be valid; reject a resolved target that violates package rules.

[Relationships overview](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/opc/relationships-overview),
[URI resolution](https://www.rfc-editor.org/rfc/rfc3986.html)

### PKG04 Shared limits and selective access

- Enumerate metadata/open one part without decompressing every entry; reading an
  attachment must not decode unrelated typed parts. Start with seekable inputs
  and diagnose unsupported capabilities. Later nonseekable support must declare
  spool behavior, location and limit; no silent whole-package memory buffering.
- Share one budget across archive work, decompression, codecs, references and
  preservation; nested calls cannot reset it. Bound input, entries, metadata/index
  storage, per-part/total expanded bytes, decoder allocations, XML depth/token
  sizes, relationships, unresolved references, retained extensions, staging and
  work. Enforce actual expanded bytes despite false declared sizes; compression
  ratios are supplementary limits.
- Check cancellation in bounded batches; release streams and staging resources
  on termination.

### PKG05 Validation and transactional results

- Separate archive integrity, package structure, codec decoding, typed conversion,
  references and caller semantic checks. Vocabulary-specific meaning stays with
  the caller. Decode into a fresh candidate; publish/commit only after all required
  parts, references and integrity checks pass. Failure leaves the existing typed
  destination unchanged. Selective access validates accessed content and relevant
  metadata, not the complete package.
- For XML parts, carry the safe parsing policy: no external entities or automatic
  DTD, schema or namespace-URI retrieval. Never extract entries into ambient paths.
  Caller-selected staging is bounded and isolated from logical part names.
- Structured errors identify category, source part, payload location, generated
  field path and relevant relationship/reference when available. XML locations
  refer to decompressed parts; archive offsets are separate. Limit failures include
  required/permitted values; diagnostic generation is bounded too.

### PKG06 Complete package output

- Write generated typed parts and caller-supplied binary streams through one sink.
  Emit consistent media-type/relationship metadata when required by the selected
  profile; finalize the archive directory before reporting success in every ZIP
  mode.
- Offer filesystem staging and destination replacement only after finalization.
  State platform replacement/durability guarantees; pre-replacement failures leave
  the existing file intact. Report uncertain replacement outcomes explicitly.
  Arbitrary output streams may contain partial bytes and need a separate contract.

### PKG07 Whole-object stream composition

Serialize one entire object through composable byte-stream wrappers. Compression
may process codec byte batches internally, but no incremental record consumer,
incremental object serializer or per-edit compression protocol is required.
Support explicit fallible finalization, nested wrapper lifetimes, cancellation and
clear error propagation. Input must validate the complete frame and object before
publishing a fresh candidate.

Document bounded message/part/tree staging where needed. Budget metadata,
reference indexes, scratch and retained typed results separately, and measure
auxiliary memory. Tree-free optimized XML remains a performance enhancement;
owning models do not promise constant total memory.

### PKG08 Optional extension preservation

Follow-up: provide explicit reject/skip/preserve policies for optional unknown
parts, relationships, attributes and elements. Unsupported required capabilities
fail even under preserve mode.

Preserve opaque payload bytes plus needed metadata/relationships across known-field
edits. Bound retention and specify borrowed/staged ownership. If an edit invalidates
preserved references, require a profile decision. XML extensions need placement,
child order and namespace context, including prefixes inside QName-valued content.
Reuse expanded-name handling; exact namespace-name identity is separate from
package URI equivalence.
[Namespaces in XML](https://www.w3.org/TR/xml-names/)

Distinguish opaque-byte preservation, XML semantic preservation and exact XML
lexical preservation. Typed rewriting does not promise original whitespace,
prefixes, archive bytes or signatures. Signing and verification are separate
features.

### PKG09 Reproducible output

Follow-up: offer deterministic part/relationship ordering, explicit timestamp and
metadata policies, stable authored IDs and predictable codec output, including
XML. Distinguish logical
equivalence from identical archive bytes. A byte-equality guarantee must identify
the writer, compressor, options and metadata policy.

### Package acceptance gates

Apply each gate when its corresponding requirement ships; follow-ups qualify
separately from the first delivery.

1. Two typed XML parts plus one binary attachment, reordered entries: both types
   decode, attachment bytes match and forward references resolve.
2. Renamed entry part: relationship-based discovery succeeds without a fixed name.
3. Same relationship ID across source sets succeeds; duplicates within one fail.
4. Duplicate/missing/wrong-type payload references identify their source and leave
   the destination unchanged.
5. Relative/escaped/case-variant/traversal targets follow profile identity rules;
   malformed or escaping targets fail.
6. Prefix changes preserve expanded-name binding; distinct namespace names remain
   distinct.
7. Many small parts and dishonest sizes fail within shared resource budgets.
8. Bad integrity, truncation, invalid offsets or conflicting names fail before
   publishing the typed result.
9. External relationships/XML resource declarations cause no implicit access.
10. Late export failure/cancellation preserves the destination before replacement
    and releases temporary resources.
11. Preserving unknown content across a known edit retains opaque bytes and the
    declared XML guarantees.
12. Unsupported required capabilities fail even when unknown content is preserved.
13. Large complete objects meet the declared byte/tree/scratch budgets through
    stream wrappers; model/index memory is reported separately. No incremental
    record API is implied.
14. Repeated deterministic exports meet the declared logical/byte equality policy.
15. Single-entry ZIP output/input works with each qualified codec/backend; JSON
    and native-binary plain ZIP paths have no XML/OPC dependency or metadata.
16. Mixed-codec entries round-trip with explicit profile/type selection. Stored
    and DEFLATE paths, directory finalization and failure handling are qualified.
17. Fixed and optional runtime-selected pipelines interoperate; disabled choices
    fail clearly and ZIP-disabled consumers incur no package overhead.
18. Optional document-profile loads verify identity, versions and required parts
    before publication; raw codec/file bytes remain unchanged when disabled.
19. Final files contain full current state/attachments and no Serializer history,
    journal or session/outbox state. Side-channel reopen preserves identities,
    allocation marks and retained history/cursor/branches when correctly bound.
20. Package/journal integration rejects stale baselines and survives injected Save,
    publication and cleanup failures without discarding required recovery data.
21. External XML round-trips its declared export projection; internal positional
    binary preserves complete recovery state/identities. The bridge retains
    side-channel-only state; sidecar/peer layout mismatches fail before decoding.
22. Interrupted Full Save recovers a coherent old or new file/baseline generation,
    never a mixed pair; uncertain publication fences writes until recovery.
    Dirty state follows the export projection; recovery cleanup retains undo.
23. OPC builder and generated profile emit independently readable equivalent
    packages with correct manifests/relationships and no private envelope.
24. JSON+gzip and other qualified compositions read with independent tools;
    filters validate finish/trailers and preserve shared limits/publication rules.

Use independent producer fixtures and an independent archive reader, alongside
round trips, malformed inputs and boundaries. Publish the archive/profile subset
and qualify each backend: generated codecs alone do not establish package support.

### Package delivery boundaries

Deliver **PKG01–PKG06 first**; qualify PKG07 as whole-object stream composition
across codecs and languages, then add PKG08/PKG09 with separate fixtures and measured guarantees.
Keep packaging opt-in and preserve existing wires/APIs. Ordinary codecs and
managed operations incur no package-related work, storage or logging overhead. Reuse existing managed
transactions for candidate import; add no second history/journal engine.

Encryption, signing, automatic remote retrieval, general filesystem extraction
and a complete XSD-validation engine are outside the first package delivery.
An explicitly enabled existing XSD provider remains a separate XML integration.

## Enhancement plan and release gates

1. **Mapping contract.** Specify roots, expanded names, placement, text,
   repetition/wrappers, presence, ordering, type lexemes and unknown-field policy.
   Extend protocol omission rules and the compatibility checker deliberately.
   Define magic/version/digest/compact/derived-size behavior explicitly; raw
   binary preambles cannot appear before an XML root.
2. **Optional codecs across outputs.** Add generator hooks and parser
   adapters for owning records in every generated language. Make optimized generation the default whenever
   XML is selected, with mandatory generated checks. Keep XML opt-in and outside
   the existing `all`
   default; retain `binary_none` restrictions and avoid XML dependencies in
   binary-only builds. General per-backend protocol selection is a separate
   enhancement, not something already provided by `cpp.protocols`.
3. **XSD compilation.** Resolve and pin the complete build-time schema bundle,
   track its dependencies, and emit direct checks for a declared subset. Fail
   generation for unsupported constructs in the default mode. Provide CLI/INI
   `runtime_xsd` support that includes a validator adapter and trusted preparation
   API on request; declare its separate coverage and dependencies. Qualify both
   enabled/disabled builds, validation scope and runtime schema selection.
4. **Schema tooling.** Add portable annotations, diagnostics and a documented
   XSD export subset; later add subset import with explicit rejection reports.
   Keep schema paths out of generated model identity.
5. **Qualification.** Add independent XML/XSD fixtures, malformed/duplicate/
   namespace/presence/order/type cases, UTF-16 and encoding/BOM mismatches,
   adversarial limits, fuzzing and destination rollback checks. Cross-check
   another parser/validator and Windows/Linux;
   measure time, allocations, encoded size and generated code size. Regression
   test existing formats and XML-disabled dependencies/output.
6. **Later expansion.** Qualify every selected language separately; diagnose
   requested unsupported backends. Consider ordered mixed content, wildcards,
   advanced XSD, managed/view bindings and signatures only with concrete needs.
   Keep HTML and source-preserving editing as separate features.

Implementation touches the [format vocabulary](../../src/schema_formats.hpp),
[wire identity](../../include/rohit/versioning.hpp),
[model metadata](../../include/rohit/serializer_creator.hpp),
[C++ generator](../../src/cpp_writer.cpp),
[output configuration](../../include/rohit/output_options.hpp) and
[decode limits](../../include/rohit/decode.hpp). XML needs its own generation
hooks; current name/value traversal lacks attribute/text/root information.
Preserve existing enum values, wire bytes, generated names and older schemas.

A shipped codec is a compiler/runtime minor feature release. New grammar also
requires a schema-language minor release with feature gating, including includes.
XSD/XML versions and payload revisions remain independent. Update README,
integration skill, usage/wire/migration docs, both editors and synchronized editor
patch versions when their implementation changes. The implementation increments
the compiler/runtime minor release to **2.3.0**; schema grammar stays **1.17.0**.
The [pending report](../xml_pending.md) records remaining gates; qualification
results must describe executed builds and checks separately from this specification.
