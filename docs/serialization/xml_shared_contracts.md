# Shared XML content contracts

Compiler/runtime **2.10.0** implements the shared compiler contracts for S03–S07:
explicit presence/nil states, ordered mixed content, map entries, variant/type
selection and positioned foreign-extension slots. S02 supplies a separate
[incremental C++ event reader](xml_events.md#incremental-byte-reader).

The new INI selections bind checked compiler IR and have reference validators.
Compiler/runtime **2.12.0** adds generated C++/Python presence and map mappings,
C++ element-tag variants, Python element/`xsi:type` variants and Python ordered
mixed content; see [token codecs and their supported subsets](xml_tokens.md).
Positioned-slot codecs (X16), C++ `xsi:type` variants, and the corresponding
contracts in other languages remain pending. An unsupported valid contract reaches a clear backend
capability error before generated output is published; an invalid contract fails
its configuration or binding checks first. Binding a contract does not enable a
codec or authorize publishing a decoded model. Existing generated XML mappings
and native JSON/binary behavior keep their previous contracts.

These are generator configuration and additive C++ runtime APIs. Schema language
stays **1.20.0**, and both editor packages stay **2.0.5**. There are no new
`.serializer` declarations or annotations. See [executed checks and remaining
qualification](../verification/verification-xml-shared-contracts-2026-10-19.md)
and [the backlog](../pending.md).

## Configuration and binding

Use `[xml] enabled = true` and `[xml.class.source::qualified_name]`, selecting
schema member/alternative names before generated naming profiles apply. Keys and
policy tokens are case-sensitive. Unknown properties, missing member selectors,
orphan map/variant/mixed subproperties and conflicting mappings reject. All
examples below are complete **binding examples**, intended to reach either the
shared contract or its explicit generation-capability diagnostic.

Shared-contract name selections and wrapper values accept `local` using the
class namespace, `{uri}local` for an explicit expanded name, and `{}local` for
the empty namespace. Ordinary `name.<member>` settings retain their local-name
contract. Document
prefixes never participate in these identities. Ordinary attribute defaults use
the empty namespace. Configured names retain the existing portable ASCII NCName
restriction; the byte event reader independently accepts provider-validated
Unicode XML names. Namespace URI lists use XML whitespace, with commas treated
as part of a URI. Configured expanded-name/wrapper namespace values must escape
whitespace and URI characters rejected by the existing namespace checks.

The compiler-side contracts in `src/xml_content_contract.hpp`,
`src/xml_container_contract.hpp` and `src/xml_schema.hpp` borrow resolved native
type/member metadata. Keep the parsed AST alive and its storage stable for their
entire lifetime. These are compiler internals, rather than installed runtime
headers. Configured generic/reference owner targets reject these shared
contracts. `schema::bind()` validates a contract; `validate_backend()` rejects its
unimplemented generated semantics. Fixed XSD bindings reject these INI overrides.
XSD export and directional XML compatibility checking also reject these new
contracts until corresponding implementations exist.

## Presence, absence and nil

Select `presence.<member> = required|absent|nil|absent_or_nil`:

| Policy | Native storage | Accepted states |
| --- | --- | --- |
| `required` | `T` | Present ordinary value only. |
| `absent` | `optional<T>` | Absent element or present ordinary value. |
| `nil` | `optional<T>` | Present nil element or present ordinary value; absence rejects. |
| `absent_or_nil` | `optional<optional<T>>` | Absent, present nil, or present ordinary value. |

For `absent_or_nil`, an empty outer optional represents absence, an engaged outer
with empty inner optional represents nil, and an engaged value represents the
ordinary value. Empty text and an empty collection wrapper are ordinary values;
neither implies absence or nil. Optional depth must match the policy exactly.

```text
serializer version 1.7.0;
namespace example {
  class value {
    public optional<optional<string>> label;
    public optional<array<string>> items;
  }
}
```

```ini
[xml]
enabled = true

[xml.class.example::value]
namespace = urn:example
presence.label = absent_or_nil
presence.items = absent
wrapper.items = items
name.items = item
```

The `label` states correspond to no `<label>` element,
`<label xsi:nil="true"/>`, and an ordinary `<label>` element, including
`<label/>` for an empty string. `xsi` denotes the expanded namespace
`http://www.w3.org/2001/XMLSchema-instance`; any bound prefix may spell it.
Nil lexical values accept `true`, `1`, `false` and `0` after trimming XML
whitespace. True nil forbids all text, including whitespace, and child elements.
False nil denotes an ordinary value. Any `xsi:nil` attribute rejects when the
selected contract does not permit nil. Duplicate or malformed nil attributes
reject; attributes named `nil` in other namespaces do not select nil.

Nil requires an element, so attributes and direct text cannot use nil policies.
Direct optional text is ambiguous and rejects. Explicit collection presence
requires a wrapper; optional items, nested collections, maps, expected values and
managed references require other contracts and reject here. Legacy nonoptional,
single-optional and lexical-list mappings keep their established defaults when
no explicit presence policy is supplied. The reference classifier establishes
presence only; scalar conversion and nested record validation remain separate.

## Ordered mixed content

One native sequence owns the complete text/child/text order. Its fragment record
has exactly one durable owning variant member: one `string` alternative and at
least one ordinary owning-record child alternative.

```text
serializer version 1.4.0;
namespace example {
  class emphasis { public string value; }
  class fragment {
    public variant(string = text, emphasis = emphasis) value;
  }
  class paragraph { public array fragment content; }
}
```

```ini
[xml]
enabled = true

[xml.class.example::paragraph]
namespace = urn:paragraph
mixed = content
mixed.variant = value
mixed.text = text
mixed.child.emphasis = {urn:format}em
```

`mixed.variant` defaults to `value`; `mixed.text` defaults to `text`.
`mixed.child.<alternative>` overrides a child expanded name; otherwise the child
record name and configured class namespace apply. Unknown alternatives,
colliding expanded child names, multiple string branches, optional/nested
fragment sequences and non-record child branches reject. Separate element/text
fields cannot coexist with the mixed sequence; scalar attribute fields may.
The sequence field cannot also have a regular name, wrapper, attribute, presence,
map or variant mapping.

The reference `mixed_item_view` borrows either text or one child element.
`validate_mixed_trace()` preserves sequence order, including empty/adjacent text
fragments; it never sorts or coalesces them. `observe_mixed_content()` checks a
normalized tree's `text_segments` against aggregate text and child positions,
then returns borrowed ordered views. The tree must outlive these views and stay
stable. Defaults bound the sequence to 100,000 fragments and 8 MiB of text;
these bounds do not include already parsed child subtrees. Share an operation
context for cumulative work/allocation charges and interruption.

## Map entries and native key identity

A map has one explicit collection wrapper and repeated entries, each containing
exactly one present key and one present value. Keys may be elements or scalar
attributes. Both legacy `map(K) V` and generic `map<K,V>` storage bind. Binding
always requires a configured wrapper; a single optional generic container may
omit that wrapper from the XML input, while a present empty wrapper represents
an empty map. Optional keys/values and nested collection values are outside this
contract.

```text
serializer version 1.7.0;
namespace example {
  class catalog { public map<uint32, string> entries; }
}
```

```ini
[xml]
enabled = true

[xml.class.example::catalog]
namespace = urn:catalog
wrapper.entries = entries
map.entries = entry
map.entries.key = id
map.entries.value = data
map.entries.key_placement = attribute
```

The reference shape is
`<entries xmlns="urn:catalog"><entry id="7"><data>text</data></entry></entries>`.
`map.<member>` selects the entry name. Key/value names default to `key`/`value`;
`key_placement` defaults to `element`. Attribute keys default to the empty
namespace. Element key/value expanded names must differ. Dedicated wrappers
cannot also use text, attribute, lexical-list or explicit presence selections.
Map and variant fields also reject ordinary `name.<member>`, field-level
namespace, QName and prefix-list overrides; use their dedicated payload names
and configured class namespace instead. Wrapper selections remain supported.
Attribute keys cannot be named `xmlns` in the empty namespace or belong to the
XMLNS or XML Schema instance namespaces, where they would collide with
namespace/presence/type metadata.

Keys support Boolean, fixed-width signed/unsigned integer, string and enum
identity. Floating-point and record keys reject. Boolean/numeric keys trim XML
whitespace and canonicalize aliases before duplicate detection: `1` and `true`
are one Boolean key; `+007` and `7` are one integer key; negative zero is zero.
Values outside the declared integer width reject. String keys remain byte-exact;
enum keys require an exact declared enumerator. Unknown/duplicate entry members,
missing key/value, nil entries/keys/values and nonformatting wrapper text reject.
Scalar keys cannot carry child elements or extra attributes.

`validate_map_entries()` establishes wrapper, entry, presence and key-uniqueness
rules. Value scalar conversion and recursive record validation remain the
consumer's responsibility. Defaults allow 1,024 entries and 1 MiB of cumulative
key bytes, charging the larger lexical/canonical size for each key. Parsed XML
and allocator overhead are outside these logical limits.

## Variant and type tags

An owning `variant` or native union maps one alternative into a required field
wrapper containing exactly one payload child. Alternatives need stable, unique
schema aliases; generated naming profiles do not change their XML identities.

```text
serializer version 1.4.0;
namespace example {
  class choice { public variant(uint32 = code, string = text) payload; }
}
```

```ini
[xml]
enabled = true

[xml.class.example::choice]
namespace = urn:choice
wrapper.payload = choice
variant.payload = element
variant.payload.tag.code = {urn:choice}number
variant.payload.tag.text = {urn:choice}text
```

Element mode defaults omitted tags to each alternative alias in the wrapper's
namespace. The field-wrapper forms are `<choice><number>7</number></choice>` or
`<choice><text>value</text></choice>` in `urn:choice`. Duplicate expanded tags,
unknown aliases, absent/multiple payloads, nil and nonformatting wrapper text
reject. An optional `variant.<member>.type.<alias>` supplies a static expanded
type identity. If a payload also supplies `xsi:type`, it must agree with the
element-selected alternative's identity. Primitive defaults use their XML Schema
type; enum/record defaults derive from stable native schema names.
Selecting only a `.type` override preserves the default element alias tag.

For explicit QName type selection, replace the variant settings with:

```ini
variant.payload = xsi_type
variant.payload.tag.code = {http://www.w3.org/2001/XMLSchema}unsignedInt
variant.payload.tag.text = {http://www.w3.org/2001/XMLSchema}string
```

Every alternative requires an explicit tag/type inventory in `xsi_type` mode.
The payload element has the **same expanded name as the field wrapper** and its
`xsi:type` resolves in the payload's original full namespace scope. Thus the
reference unsigned-integer form is `<choice><choice xsi:type="xsd:unsignedInt">7</choice></choice>`
with the corresponding `xsi`, `xsd` and default bindings. An additional `.type`
override must equal its `.tag` identity. Unbound, missing, conflicting or unknown
type QNames reject; matching local spelling alone is insufficient. The selector
returns an alternative index only after structural selection checks; payload
conversion and complete-document validation remain separate.

## Positioned extension slots and required capabilities

Unknown content requires explicit foreign namespace slots. Each slot chooses
`reject`, `skip` or `preserve`, its attribute/element kind, occurrence limits and
an element position. Unknown core names, empty-namespace names and known fields
cannot enter these slots, even under a permissive operation-context policy.

```text
serializer version 1;
namespace example {
  class document { public uint32 known; }
}
```

```ini
[xml]
enabled = true

[xml.class.example::document]
namespace = urn:core
extension.foreign.kind = element
extension.foreign.before = known
extension.foreign.namespaces = urn:extension urn:other
extension.foreign.policy = preserve
extension.foreign.minimum = 0
extension.foreign.maximum = 2
extension.metadata.kind = attribute
extension.metadata.namespaces = urn:metadata
extension.metadata.policy = skip
extensions.required = urn:extension
```

`kind`, `namespaces` and `policy` are required. `before` defaults to `$end`;
otherwise it selects an ordinary element member in bound field order. Attribute
slots have no member position and reject a named `before` selector. `minimum`
defaults to zero; `maximum` defaults to unbounded and also accepts `unbounded`.
Counts are nonnegative decimal integers; impossible intervals reject. Reject
slots cannot have a nonzero minimum. Slot IDs must be unique. A foreign URI may
appear in only one attribute slot, or one element slot at a given position;
overlaps and duplicate namespace tokens reject. `extensions.required` is a list
of vocabulary URIs that must be understood.

The public `<rohit/xml_extensions.hpp>` API is available independently of the
optional parser. A raw caller constructs `extension_contract` and submits unknown
items to `collect_extension_slots()`. Each `extension_item_view` borrows exactly
one attribute or child and its precise field/child position. Child positions must
be nondecreasing. Skip validates permission/counts and retains nothing. Preserve
returns an owning `extension_store`, including subtree mixed-text ordering and
full namespace context. The caller must supply full in-scope bindings on the
owner and every subtree node; local declarations alone cannot preserve lexical
QName meaning after prefix shadowing. The helper trusts parsed XML character/name
validity and does not fetch resources or validate an external vocabulary.

```cpp
#include <rohit/xml_extensions.hpp>
#include <array>

namespace xml = rohit::serializer::xml;

// Retain one explicitly permitted foreign child in a local candidate.
xml::extension_store retain_extension(const xml::element& owner,
    const xml::element& child, xml::operation_context& context) {
  const xml::extension_contract contract{
      .core_namespace = "urn:core",
      .slots = {{"extra", false, 1U, {"urn:extension"},
          xml::unknown_policy::preserve, 0U, 1U}}};
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  return xml::collect_extension_slots(contract, owner, items, {}, {}, {}, &context);
}
```

Keep the candidate local until complete typed decoding and byte EOF validation
succeed. Exceptions leave existing caller-owned output untouched; shared context
charges are cumulative and are not refunded. Preservation is semantic, rather
than lexical byte, formatting, signature or canonicalization preservation.

`validate_required_capabilities()` checks configured required URIs and a document
prefix list in its original scope against the caller's `understood_namespaces`.
The core vocabulary is understood implicitly. Unbound prefixes and unsupported
required vocabularies reject. **Opaque preservation or skipping never establishes
understanding.** Merely listing a URI in a slot does not satisfy a required
capability. No provider or remote lookup is performed.

Default extension bounds are 128 slots, 100,000 submitted items and 8 MiB of
logical preserved storage, plus the ordinary document limits for retained
nodes, depth, attributes, names, text and namespace bindings. Preserved bytes
include owned payload/descriptors, mixed-text copies and retained scopes;
provider/source memory, spare capacities and allocator bookkeeping are excluded.
One operation context carries work, allocation and preservation charges,
cancellation and deadlines across composition. These are logical admissions,
rather than a process heap quota or hard provider preemption guarantee.
