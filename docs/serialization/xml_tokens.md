# Generated XML token codecs

Python QName-list enumeration, defaults and fixed values retain the schema's
ordered expanded identities. Equivalent instance aliases, child-local prefixes,
empty lists, no-namespace items and list-valued union members are supported.
These graphs retain their existing whole-document Python path and XML-only scope
state; the change does not introduce tree-free decoding or a hard allocation
ceiling. Other backend restrictions and namespace-dependent outer union
restrictions/defaults remain gated. See the
[executed QName-list inventory](../verification/verification-qname-lists-2026-10-21.md).
Schema defaults/fixed lists with no-namespace items also reject an active
nonempty instance default namespace; scopes of other QName fields remain intact.

Compiler/runtime **2.16.0** retains C++/Python namespace-dependent scalar-child
scopes by field and occurrence. Local/shadowed QName prefixes, QName-list items,
default namespace resets and nested union branch identities survive output;
native string values and JSON/binary encoding keep their contract. C++ direct
and retained-tree paths share the scope store. Python selects whole-document
decoding for these graphs. Scope metadata follows occurrence positions; editing
or reordering indistinguishable lexical values requires keeping their namespace
metadata associated. Valid Unicode XML mapping names and QName prefixes are
accepted independently of the native parsing provider.

Go/Java/C# add direct plain QName, QName-list and prefix-list codecs. Use Go
`SetXMLNamespaces`, Java `setXmlNamespaces` or C# `SetXmlNamespaces` to validate
and copy explicit authored bindings. These replace captured scopes on that record;
nested records bind independently.
Kotlin/Swift owning records support the same plain scalar scope contracts.
C/Rust preserve them through XML document companions; use document decode and
encode-with-extensions entry points. Their wrapped/nil, mixed/all, revision/magic
and other graph combinations still have explicit capability gates.
Namespace-dependent union/default/fixed/enumeration combinations remain gated.
See [current executed checks](../verification/verification-qname-scopes-2026-10-20.md)
and the [remaining requirements](../pending.md#opc-and-3mf-requirements).

Compiler/runtime **2.15.0** introduced selected C++ child unions with
QName or QName-list members. The first successful member and its expanded names
must agree in the child and owning record namespace scopes. Optional, repeated
and nested-union fields use that same check; child-local/shadowed bindings
that would lose identity rejected in that release. Python supports outer enumeration and
fixed restrictions on namespace-independent unions, comparing through the first
successful member's primitive value space. Numeric, Boolean and string values
remain distinct; ordered list values compare item by item. QName-dependent outer
restrictions remain unavailable. The later Python QName-list extension above
supersedes the original QName-list enumeration/fixed restriction. See the
[selected qualification](../verification/verification-pending-xml-2026-10-20.md).

Compiler/runtime **2.12.0** adds direct XML codec paths without changing schema
language **1.20.0** or native JSON/binary wire contracts. The
[verification record](../verification/verification-xml-tokens-2026-10-19.md)
identifies executed SDK checks and the remaining individual X-task instances.

## C++

Regenerate owning headers, link `Serializer::runtime`, and include
`<rohit/xml_tokens.hpp>`. Require `find_package(Serializer 2.12 CONFIG REQUIRED)`.

```cpp
// Encode/decode the complete message without retaining an XML element tree.
const auto bytes = rohit::serializer::xml::serialize_tokens(value);
const auto decoded = rohit::serializer::xml::deserialize_tokens<model>(bytes);
```

The token path traverses generated scalar, attribute, nested, optional, repeated,
fixed-array, simple-content, collection-wrapper, map and tagged/`xsi:type` variant mappings.
Singleton choices and supported top-level `all` groups are traversed directly.
Ordered mixed arrays visit private native variant fragments directly. Input
coalesces provider text between child elements, while output visits authored
fragments in order. `max_mixed_fragments` (100000) and `max_mixed_text_bytes`
(8 MiB) bound one owner; fixed-array storage retains its exact native extent.
It uses static
field names, validates exact EOF, and returns a fresh owning candidate only after
success. Prefixes are presentation choices; expanded names determine identity.
Output may use more element-local namespace declarations than the older tree
writer. No throughput or encoded-size improvement is claimed.

`token_writer` and `token_reader` expose the lower-level direct path. The reader
borrows the input message until destruction and pulls bounded chunks from the
incremental LibXml2 event producer. Its optional final `chunk_bytes` argument
defaults to 4096; a positive smaller value exercises split tokens and UTF-8.
Scalar lexical values and bounded queued events may be retained, along with the
owning destination. There is no retained document tree. `max_tree_bytes` does not
bound these paths; other XML limits and the optional cumulative operation context
remain active. Provider internals are not a hard peak-memory bound.

Generated `xml_tokens_supported` describes each record's own contract. Nested
records must also support the selected path; unsupported rich combinations fail
explicitly. Existing whole-object entry points retain their documented rich
mapping path unless the selected contract requires tokens. Explicit presence
contracts select the token path through `xml::serialize`/`deserialize` as well.
They preserve absent, nil and empty values using the
[shared presence contract](xml_shared_contracts.md#presence-absence-and-nil).
Maps reject duplicate native key identities, including lexical Boolean/integer
aliases. `limits.max_map_entries` (1024) and `max_map_key_bytes` (1 MiB) apply per
wrapper. Element keys and values may occur in either order. Variant type hints
resolve against the payload's namespace scope and must agree with its selected
alternative. Generated key/unique/keyref observers validate directly over the
same token traversal; forward keyrefs resolve before publication. A borrowed
`identity_limits` policy in `operation_context.identity_bounds` tightens tuple,
index and work admissions without building or reparsing a document tree.

## Python

Regenerated Python owning models select direct output and incremental Expat input
for the token-compatible object graph. `decode_xml_chunks(chunks, limits=None,
context=None)` accepts arbitrary UTF-8 byte slices and publishes only after
provider EOF. `encode_xml` and `decode_xml` select the same paths automatically.
Existing namespace-dependent rich/preserved mappings retain their supported
whole-document paths; an unsupported chunked combination rejects explicitly.

The token graph supports scalar facets, bounded approved portable patterns,
authoritative simple-content/wrappers, singleton choices, and XSD `all`
permutations, maps, tagged/`xsi:type` variants, flattened owning inheritance and
ordered mixed fragments. Other unimplemented shared contracts retain explicit
generation errors.

Approved portable patterns execute prepared Thompson programs without lxml.
`Limits(xml_pattern=XmlPatternLimits(max_states=16384,
max_input_bytes=8 * 1024 * 1024, max_work_units=64 * 1024 * 1024))` tightens the
operation's pattern admissions. Unsupported legacy XSD patterns retain their
explicit lxml path; the portable path never substitutes a host-regex dialect.

`Limits(xml_map=XmlMapLimits(...), xml_mixed=XmlMixedLimits(...))` supplies
per-map key/entry and mixed fragment/text bounds. Maps preserve optional absent
versus present-empty wrappers and reject duplicate canonical keys. Variants
require exactly one known correctly typed payload. Mixed input joins provider
text chunks between child elements so byte chunking cannot change model order.
Authored output visits adjacent/empty text fragments individually; invisible
boundaries between adjacent or empty XML text cannot survive a byte round trip.
An unconfigured fragment class used as mixed storage keeps native codecs and
reports an explicit error for standalone XML; no redundant fragment mapping is
required. Explicit independent fragment mappings retain their own validation.

Generated XSD key/unique/keyref validation retains scalar tuples and open path
selections. Keyrefs may precede their keys and resolve before publication. Keys
require complete non-nil fields; incomplete unique/keyref tuples are omitted.
Numeric aliases compare by exact value, strings follow their declared whitespace
policy, and QName keys use expanded names. Limits bound the local indexes:

```python
limits = Limits(xml_identity=XmlIdentityLimits(
    max_tuples=10000, max_tuple_bytes=1024 * 1024))
value = Registry.decode_xml_chunks(chunks, limits=limits)
```

`XmlIdentityLimits` also bounds constraints, fields, selected nodes, depth and
work. Zero is a real bound. These XML-only additions leave ordinary generated
Python modules unchanged. Shared context work/allocation charges are cumulative;
Python container overhead is outside logical admission accounting. Identity
constraints on an unsupported token graph reject rather than bypass validation.

Revision/magic metadata checks current and historical field intervals before
constructing an owning model. Owning raw/fixed/MD5 digest fields use bounded exact
hexadecimal values; native protocol field identities stay unchanged. Positioned
extension slots support reject/skip/preserve, namespace-aware capability checks
and exact slot/count rules. Configure them with
`Limits(xml_extensions=XmlExtensionOptions(...))`. Only preserved foreign
subtrees retain semantic content; skipped subtrees do not. Mixed/`all` groups
combined with new slots and legacy rich QName/wildcard combinations retain
their explicit capability limits.

Python readonly shared/borrowed/weak value references and scoped translators also
export through the target's XML mapping. They retain live source values without
constructing an owning target. Collections and nested values remain borrowed;
export-only input graphs reject before candidate construction.

Python also emits `Html`, `HtmlLimits`, `HtmlEvent` and `HtmlAttribute` with XML
support. `Html.parse_fragment`/`write_fragment` implement the approved
[separate HTML subset](xml_advanced_contracts.md#html-is-a-separate-fragment-adapter).
They do not parse ordinary HTML through XML or provide a general sanitizer.

## Other outputs and OPC example

All eleven outputs now have qualified direct record output and owning token
input. Java, C#, Go, Kotlin, Rust and C use native pull readers; Swift uses
Foundation callbacks with private typed frames. Swift checks complete provider
EOF and fatal errors separately, preserving Unicode namespace IRIs despite a
nonfatal provider URI warning. An active lexical cursor preserves distinct raw
Unicode attribute and namespace-prefix spellings that Foundation's Swift
dictionaries can merge; completed elements are not retained. Swift native optional layers distinguish absent,
nil and empty values; top-level supported `all` groups accept every permutation.
Go adds flattened owning bases
and generated bounded Thompson-state programs for the approved portable pattern
subset, with independent Unicode/value-space checks. Java and C# also execute
prepared scalar/pattern programs. Java/C# also support exact arbitrary-precision
integer/decimal lexical values, typed and lexical lists, namespace-independent
unions, defaults/fixed values and authoritative simpleContent. Namespace-dependent
QName contracts retain explicit gates until their backend qualification passes.
Go/Java/C# map codecs enforce typed canonical
key uniqueness and per-wrapper `XmlMapLimits`, including nested values and
optional absent versus present-empty wrappers. Explicit Go/Java/C# presence
also retains every nil/absent/empty native layer, with authoritative XSD
nillability, required fields and inferred optional collection wrappers.
Existing entry points retain
their defaults; explicit policies use Go's `EncodeXMLWithMapLimits` /
`DecodeTypeXMLWithMapLimits` or Java/C# overloads. Kotlin presence mappings retain
all native optional layers for absent, nil and empty scalars/records/collections.
Provider-only pattern constructs reject precisely for these portable programs.
Go/Java/C# variants accept exactly one native typed payload in element-tag and
`xsi:type` modes. QName hints must match the selected inventory; active namespace
scopes preserve prefix shadowing and empty type namespaces.

JavaScript/TypeScript add incremental `decodeXmlChunks` input backed by pinned
`saxes` 6.0.0 and fatal UTF-8 decoding. Typed private setters build an unpublished
owning candidate directly; complete provider EOF and resource checks precede
return. The ordinary `decodeXml` path selects the same frames. Nil/choice/`all`,
native maps, tagged/typed variants and inherited private fields share those
direct callbacks. Prepared portable pattern programs enforce state/input/work
bounds; exact integer/decimal comparisons avoid binary64 rounding. Scalar
whitespace, Unicode length, enumeration and default/fixed checks apply in both
directions. Ordered private fragment arrays use logical text-run and fragment
bounds; scoped key/unique/keyref observers validate exact numeric/QName tuples
and forward references during direct input/output. Existing rich DOM helpers remain separate.

Rust adds pinned `xmlparser` 0.13.6 alongside the existing legacy `roxmltree`
dependency. The [provider API](https://docs.rs/xmlparser/0.13.6/xmlparser/)
supplies borrowed lexical tokens; generated adapters enforce nesting, duplicate
expanded attributes, namespace scopes and exact EOF. `decode_xml_bytes` adds
explicit UTF-8 admission alongside the existing `decode_xml(&str)` API. C uses
LibXml2's pull reader and replaces the initialized destination only after the
complete candidate succeeds; bounds, malformed input and allocation errors
release candidate/provider storage first. Rust and C preserve absent, nil and
empty native optional layers for scalars, records and wrapped collections,
including fixed-array sizes; both support unordered singleton `all` traversal.
Kotlin/Swift maps retain native typed dictionaries and use the optional trailing
`mapLimits` companion parameter for per-wrapper entry/key bounds. The same policy
propagates to nested values without changing existing `XmlLimits` layouts.
Their `mixedLimits` companion independently bounds ordered fragment count and
logical text bytes; native private fragment variants retain their authored order.
All eleven outputs qualify the selected mixed/map/variant mappings, including
both element and namespace-resolved `xsi:type` inventories. Java/C#/Go identity
observers, also implemented in Kotlin/Swift, validate keys, unique tuples and forward references directly, using
independent owner-local index policies.
C/Rust check the same selected identity contract through a separate bounded
streaming pass over original input or completed private output. It retains active
paths and bounded owner-local tuple indexes, without a completed XML tree. This
adds a validation scan. Their `with_identity_limits` companion entrypoints select
the independent policy while preserving ordinary entrypoints and limit layouts.
Release 2.18.0 adds separate C operation descriptors and a collision-selected Rust
operation module/qualified trait. Primary native XML events are charged once;
secondary identity passes add real work/allocation and one reference for every
complete tuple before equal keyrefs are deduplicated. Canonical schema owner/member
paths identify fields independently of XML names or output naming profiles.
Known generated default storage is admitted before construction. Opaque authored
factories and provider internals retain their separate transient-allocation and
preemption limits. See [C package contexts](../languages/c_packages.md) and
[Rust package contexts](../languages/rust_packages.md).
C/Rust/Kotlin/Swift expose the approved balanced ordinary HTML event adapter separately
from XML parsing, with exact input/output/event/depth/attribute/text limits.
Their scalar adapters check exact numeric boundaries, lexical lists/unions,
defaults/fixed values and bounded portable Unicode patterns. Decimal binary32
input converts directly at its native width, including subnormals and signed zero.
C/Rust/Kotlin/Swift owning models also check revision/magic metadata against exact native
historical layouts. Owning raw/fixed/algorithm digest bytes use strict hexadecimal
XML without recomputing their contents. Kotlin/Swift's trailing `digestLimits` policy
propagates through nested records independently of map/mixed/document limits.
Kotlin/Swift positioned extension slots use the trailing `extensionOptions`
policy. Supply `XmlExtensionOptions(understoodNamespaces=...)` in Kotlin or
`XmlExtensionOptions(understoodNamespaces: ...)` in Swift; preservation permission
does not establish semantic understanding. Slots retain only foreign attributes
and subtrees, including their complete original namespace context and mixed text.
Known members remain direct. `XmlExtensionLimits` bounds slots, foreign roots and
logical preserved bytes per owner, including the shared contract's metadata
inventory admissions; ordinary document bounds remain cumulative.
Serializer's `urn:serializer:xml:model` vocabulary remains closed even when a
foreign slot explicitly lists that URI; future lifecycle metadata cannot become
opaque extension content. C++ exposes its identity as `xml::model_metadata_namespace`.
Skipping retains no foreign payload and works with zero preserved-byte admission.
Replay preserves static field/repeated-item boundaries and rejects edits that
remove a required retained position. Attribute-only slots also work on scalar
text records. Child slots with scalar text, and slots combined with mixed/`all`
storage, reject precisely. Namespace declarations introduced by an ordinary
ancestor can increase a nested owner's captured context on the first replay;
the selected byte limit must admit that actual context rather than discount it.
Java/C#/Go qualify the same positioned foreign-only reference contract. Java/C#
take `XmlExtensionOptions` in trailing XML overloads. Go adds
`EncodeXMLWithExtensionOptions`/`DecodeTypeXMLWithExtensionOptions` and
`WithAllBounds` companions, preserving existing option struct literals and
ordinary entrypoints. These options propagate through nested owning records and
live readonly record references. A root translator has no implicit opaque
sidecar and rejects when required foreign occurrences cannot be supplied.
C/Rust expose explicit owning document companions rather than adding opaque
storage to native record layouts. In C, initialize `Type_xml_document` with
`Type_xml_document_init`, decode with `Type_decode_xml_document`, encode with
`Type_encode_xml_document`, and release it with `Type_xml_document_free`. Failed
decodes preserve the initialized destination and failed encodes preserve the
output buffer. In Rust, `Type::decode_xml_document` returns `XmlDocument<T>` with
public `value` and `extensions`; call
`document.value.encode_xml_with_extensions(&document.extensions, ...)` to replay
the borrowed sidecar. These companion APIs take `XmlExtensionOptions` (C:
`srl_xml_extension_options`) explicitly. Ordinary native codecs serialize only
the value. Keep the opaque sidecar paired with its decoded owner graph.

These native companions filter and replay direct token streams around the known
owning codec, retaining foreign attributes/subtrees and a private known-XML byte
buffer. They add buffers and validation scans, without retaining a known XML tree
or cloning the native model. Private namespace framing is bounded to sixteen
times the caller's document byte limit, with overflow checks; the original input,
modeled values and final output still obey the original limits. Extension item
and logical preserved-byte admissions are cumulative over the whole document;
occurrence positions/counts remain owner-local. Original input and final replay
are checked by enabled identity/provider observers before publication. Mixed/`all`
graphs, revision/magic combinations, extension-bearing map/variant/wrapped/nil
owners and export/reference adapters retain explicit native companion gates.
Fixed-XSD extension overlays remain outside this selected contract. These broader
combinations belong to OPC-R02/R12 rather than the completed X16 reference tasks.
Non-C++ concrete owning generic instances are accepted where their
resolved fields satisfy the selected mapping; abstract/unresolved instances
still reject. All outputs support private/multiple owning inheritance through
their existing composed native bases and XML-only access helpers where required.
C++ also supports checked concrete owning templates through generated field bindings, preserving native
template applications and the distinct types of named instances. Each backend's qualification and unsupported advanced contracts
remain separate in [the backlog](../pending.md).

Prepared Java/C#/Kotlin XSD validators use immutable schemas and separate
per-call validation state. They check bounds, require a declared expanded root,
reject DTDs and schema include/import/redefine/override acquisition, and ignore
instance schema-location hints. Their selected concurrent/no-fetch qualification
is recorded separately from application-supplied validators in other backends.
C/Rust/Go/Swift also qualify real prepared LibXml2 callbacks through a maintained
Windows test-only native bridge. Each shares an immutable schema while allocating
a separate validation context per call; the bridge is not installed or required
by generated applications. JavaScript/TypeScript's optional prepared provider
uses the pinned libxml2-wasm adapter and rejects unavailable or disposed providers.

The [typed OPC example](../../example/opc/README.md) demonstrates a shared catalog
schema and C++/Python package facades. It fills a required XML catalog and optional
opaque attachment roles, then saves and reloads the package. An independent
ZIP/XML reader checks metadata and values alongside all four C++/Python
producer-consumer pairs. Package metadata is generated; the example is an
application vocabulary, without an official 3MF conformance claim.

## Upgrade and remaining work

Regenerate selected XML outputs and rebuild consumers with matching runtime
headers/libraries. Existing schema IDs, enum values, native formats and managed
state need no migration. XML prefix/declaration presentation can change; compare
expanded names and logical values rather than byte-identical XML text.

The selected X01-X22 reference tasks pass in all eleven outputs; their exact
fixtures, SDKs and remaining broader contracts are recorded in the
[verification record](../verification/verification-xml-tokens-2026-10-19.md).
Existing capability errors remain for combinations outside those checked
contracts. No new editor grammar or generated file layout
is introduced; both editor packages remain **2.0.5**.
