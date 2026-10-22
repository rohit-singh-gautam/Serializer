# Advanced shared XML contracts

Compiler/runtime **2.12.0** implements selected dependent generated codecs,
including C++/Python `all`, Python identity/mixed/inherited mappings and Go
portable pattern/inherited traversal. See [current token subsets](xml_tokens.md)
and their [per-backend evidence](../verification/verification-xml-tokens-2026-10-19.md).
The 2.11 shared checkpoint below does not itself qualify generated backends.

Compiler/runtime **2.11.0**, schema language **1.20.0**, extends shared tasks
S08-S36. This guide distinguishes compiler metadata and reference validators
from generated language codecs. The earlier [content contracts](xml_shared_contracts.md)
and [event boundary](xml_events.md) remain available.

The fixed-XSD tools add checked binding, local bundle resolution, export, import
and evolution checks. Public C++ helpers add portable regular expressions,
identity validation, document profile metadata and a restricted HTML fragment
adapter. New advanced record contracts bind compiler IR; unsupported generated
semantics still fail before output publication. Their dependent language tasks
remain in [the backlog](../pending.md).

No new `.serializer` grammar, native protocol number, field ID, payload revision
or managed journal layout is introduced. Both editor packages remain **2.0.5**:
schema navigation and the ordinary generated include layout are unchanged.

## Exact scalar and portable pattern contracts

Fixed-XSD scalar binding retains inclusive/exclusive bounds, Unicode lengths,
whitespace, enumeration and pattern groups. The compiler prepares an owning
`compiled_scalar_contract`; repeated validation does not recompile portable
patterns. Numeric comparisons use the declared value space, including exact
decimal/integer comparisons beyond the precision of `double`. Lengths count
Unicode code points, not UTF-8 bytes. Empty text is a value and participates in
length and enumeration checks.

`<rohit/xml_pattern.hpp>` provides an immutable Thompson-state program that
backends can inspect independently of a host regex dialect:

```cpp
#include <rohit/xml_pattern.hpp>

// Validate the complete value with a bounded portable program.
bool valid_code(std::string_view text) {
  const auto pattern = rohit::serializer::xml::compile_portable_pattern("[A-Z]{2}[0-9]{3}");
  return pattern.matches(text);
}
```

Portable patterns accept Unicode literals, dot, character ranges and negated
ranges, grouping, alternation, `?`, `*`, `+`, `{n}`, `{n,m}` and `{n,}`. Escaped
punctuation and `\n`, `\r`, `\t` are supported. Dot excludes carriage return and
line feed; `^` and `$` are literal characters. Matches cover the complete value,
as specified by [XSD 1.0 regular expressions](https://www.w3.org/TR/xmlschema-2/#regexs).
Unicode category/block escapes (`\p`, `\P`), XML name classes (`\i`, `\c`),
digit/space/word classes and their complements, and recursive character-class
subtraction lower to the same immutable range program in every owning output.
Character properties use frozen Unicode 17 data; historical XSD block spellings
are retained. Name classes follow XML 1.0 fifth-edition names. The compiler and
generated runtime do not consult the network or a host regex dialect.
Backreferences, lookaround and special groups are outside the XSD grammar and reject.

Bounds cover expression bytes, state count, nesting, repetition expansion,
input bytes and execution work. Execution does not backtrack. Pattern programs
share immutable instructions; supplied text/context are borrowed synchronously.
The default `pattern_limits` are part of the public header and can be tightened.

Older schemas using broader provider patterns remain valid. Compiler callers
can prepare those through `compile_scalar_contract(facets, false)` and inspect
`patterns_portable()`. A false result requires the existing XSD provider path;
it is never presented as a portable lowering. Existing C++/Python rich codecs
retain their documented provider requirements and native floating-conversion
limitations in [the XSD guide](xsd_tooling.md).

## Content particles, text and wrappers

The XSD binder derives simple-content text from a unique scalar member and
retains scalar attributes separately. It can infer a collection wrapper whose
single repeated item matches the native field. Conflicting authored placement,
namespace, text selection or ambiguous associations reject. Wrapped item
cardinality and the wrapper's own presence are distinct constraints.

Shared content-model compilation retains choice/sequence structure and exact
occurrences. Ambiguous alternatives reject before publication. The shared
reference validator checks ordering and occurrences independently of generated
codec support. Top-level XSD `all` groups retain unordered singleton children;
permutations are accepted while duplicates and missing required children reject.
XSD 1.0 limits each `all` child to a singleton and the group to zero or one.

Compiler callers use `compile_particle_contract()` and
`validate_particle_trace()` from `xml_particle_contract.hpp`. The latter checks
an ordered expanded-name trace; scalar values and wildcard acceptance policies
remain separate. Defaults bound the tree to 100,000 particles and 128 levels,
the trace and matching state to 100,000 entries each, and work to 64 Mi units.
A supplied operation context carries cancellation and cumulative accounting.

Existing generated C++/Python simple choices retain their behavior. Broader
grouped/repeated choices and unordered-content codecs require their per-language
implementation tasks; binding shared metadata alone does not enable them.

## Identity constraints

`<rohit/xml_identity.hpp>` owns compiled selectors, field projections and
key/unique/keyref metadata. Compiler binding reads declarations attached to a
global owner element. Nested local identity owners reject explicitly.

The supported XPath subset is deliberately small:

| Projection | Accepted paths |
| --- | --- |
| Selector | `.`, child QName paths, and an optional initial `.//`. |
| Field | `.`, child QName paths, and an optional terminal `@QName`; initial `./` is accepted. |

Wildcards, predicates, unions, general axes/functions and nested descendant
operators reject. Unprefixed steps have no namespace under the XSD 1.0 contract;
prefixed steps resolve in the original schema's scope. Type inference must be
unambiguous and cannot pass through a wildcard declaration.

Fields support string/name families, booleans, exact decimal/integer value
spaces, `anyURI` and expanded QNames. URI values have their own primitive family;
string-derived names and tokens share the string family. Floating, list and
union identity fields reject.
References resolve only to a key/unique in the same owning scope and must have
compatible tuple arity and value-space families. A missing or wrong-scope target
is an error, rather than a deferred external lookup.

Projected declarations with schema `default` or `fixed` values reject at binding,
including empty defaults. Identity helpers consume explicit normalized instance
values and do not construct schema-augmented values. Ordinary default/fixed
bindings outside an identity projection retain their existing behavior.

`validate_identity_scope()` borrows a complete normalized tree. Keys require
complete non-nil tuples. Incomplete unique/keyref tuples are omitted; complete
key/unique duplicates reject; repeated keyref tuples are allowed. Reference checks happen after all key/unique tables exist,
so forward references work. Canonical values detect aliases such as integer
`01` and `1`, independently of original lexical spelling. Failure publishes no
model or persistent index.

Limits cover constraints, fields, XPath bytes/steps, traversal depth, selected
nodes, tuples and work. The tuple-byte cap bounds a single lexical field, each
candidate (including incomplete tuples) and aggregate indexed canonical bytes.
One optional operation context
carries cancellation, deadlines and cumulative accounting. Generated identity
enforcement remains a separate backend task.

## Bundles, import, export and comparison

Local chameleon includes create separate component instances for each adopted
namespace. One physical source can be adopted into two namespaces without
sharing component identity. QName-valued defaults/facets keep their original
value contexts. Finite legal include/import cycles deduplicate by canonical
path and effective namespace. Documents outside the pinned local directory,
namespace mismatches and resource overflows still reject.

Limits remain 64 logical document instances, 32 reference levels and 16 MiB of
aggregate source bytes, including repeated chameleon instances. Source bytes
are retained unchanged for pinning. See [the exact identity contract](xsd_tooling.md).

Text/wrapper export preserves the actual bound shape. Supported scalar facets
and enumerations survive export and reimport. A nested native class may bind an
unambiguous anonymous complex type without inventing a global root. Declaration
import supports the documented scalar/attribute, simple-content, named nested
record and collection subset; imported source still requires review of stable
IDs and native ordering.

Use a new directory for a pinned multi-namespace export:

```powershell
serializer --input model.serializer --config model.ini `
  --xml.export-xsd-bundle out/model-xsd
```

The command emits flat `namespace_N.xsd` files with local imports and a
`manifest.json` containing the entry schema, SHA-256 identity and exact schema
inventory. The parent directory must exist. Validation and readback happen in
a reserved sibling directory before directory publication. An identical rerun
succeeds; an existing edited/different directory is preserved and rejected.
Serialize concurrent mutations of the same output directory.

Bundle export is a standalone command; it cannot be combined with generated
language outputs, comparison or a depfile. Single-namespace
`--xml.export-xsd file.xsd` remains available. Export reconstructs supported unions,
restricted list items, inherited patterns and declaration-owned QName default/fixed
identities. The separate external `prefix_list` contract retains its explicit
default/fixed and export capability requirement; see [XSD tools](xsd_tooling.md).

Directional comparison allows an added optional field and supported sequence-to-all
widening. Retired fields are diagnosed conservatively: an unknown-field policy
cannot prove acceptance of core names or a caller's runtime namespace whitelist.
Required additions/removals, changed placement, restrictive order,
narrowed occurrences and changed identity/model contracts produce diagnostics.
Conservative diagnostics do not claim proof for unsupported shapes.

## Concrete models, revisions and projections

Compiler-side `xml_model_contract.hpp` borrows immutable parsed AST storage.
Concrete generic identities retain actual type arguments rather than generated
alias names. Inheritance flattens bases depth-first in declaration order before
local fields. Cycles, repeated bases/diamonds and colliding field names reject;
native field IDs remain scoped to their original declarations.

Revision/magic metadata uses expanded attributes in
`urn:serializer:xml:model`: `revision` and `magic`. Historical/current layouts
follow the native declaration's accepted floor and revision rules; the
discriminator is not an ordinary data field. Raw magic uses lowercase hex;
typed magic uses its checked declaration literal. Helpers can produce these
attributes and select a layout directly from a normalized root, rejecting
missing, duplicate, unknown or invalid reserved metadata.

Managed projection exports data fields only, excluding compiler-owned persistent
identity. Opaque managed-reference and durable-address tokens remain unsupported
for XML. Borrowed/view projections are synchronous output-only and require live
owner storage; decode into an owning model. Digest projection represents exact
hexadecimal bytes with checked algorithm/extent and does not recompute a digest.
The 2.12 token guide qualifies non-C++ concrete generics and Go/Python owning
inheritance. C++ template/inherited mappings and remaining revision/projection
codecs retain their separate backend tasks.

## Document profiles and dispatch metadata

`<rohit/document_profile.hpp>` defines complete-object envelope metadata, including
an explicit opaque-byte payload identity for untyped package attachments. Its
strict map parser accepts only `format`, `compression`, `container`, `policy`
and `level`. A fixed profile rejects divergent runtime overrides; a defaults
profile permits only validated envelope changes. Model, root and vocabulary
identities are not runtime mapping overrides.

Existing C++ package declarations now generate `document_profiles()`. The
owning metadata factory describes roles, relationships, codecs/compression,
expected native type, bound XML root/vocabulary and static part shape/counts.
For a rendered package class named `document_profiles`, the factory is named
`document_profiles_metadata()` to avoid colliding with its constructor.
It does not copy model payloads or open providers. ZIP entry compression and
outer whole-package compression are separate decisions.

`select_decoder()` requires the caller's expected model identity, expanded root,
consistent optional `xsi:type` and understood vocabularies before selecting an
entry. Missing/wrong metadata selects no decoder. Returned pointers borrow the
inventory, which must remain stable through synchronous use. Required vocabulary
understanding is independent of raw extension preservation. Metadata and
configuration validation do not implement every backend's envelope dispatcher;
actual codec/provider availability is checked by the selected operation.

## HTML is a separate fragment adapter

`<rohit/html.hpp>` provides owning HTML events, `parse_fragment()` and
`write_fragment()`, independently of the optional XML provider:

```cpp
#include <rohit/html.hpp>

// Canonicalize the approved, explicitly balanced fragment subset.
std::string normalize_fragment(std::string_view source) {
  return rohit::serializer::html::write_fragment(
      rohit::serializer::html::parse_fragment(source));
}
```

The approved subset has 28 explicit flow/phrasing/list tags and the `br`, `hr`,
`img` void tags. Names fold ASCII case; quoted/unquoted attributes and the
`hidden` boolean attribute use HTML syntax. An explicit small attribute/entity
inventory, UTF-8, balanced content and resource bounds are enforced. Comments
are omitted. `supported_subset()` describes the runtime boundary.

Document types, implied closes, raw script/style text, foreign content, `pre`
and general browser recovery are outside this subset and reject. HTML namespace
or `xsi` metadata does not enter these events. This is a restricted fragment
adapter, not a general HTML5 parser or sanitizer; accepted URL strings still
require application policy when used in a security-sensitive context. XHTML
continues to use XML parsing as a separate vocabulary.

## Shared producer/consumer inventory

[`xml_exchange_manifest.json`](../../test/resources/xml_exchange_manifest.json)
records one schema, input, semantic oracle and all eleven language consumers.
`example/xml/run.py` reads it by default and executes every selected producer
against every selected consumer. Full inventory qualification is 11 by 11
exchanges, with an independent expanded-name/value check on every result.
TypeScript builds its own strict consumer; sharing JavaScript runtime output
does not replace its SDK check.

Missing SDKs or failing version/build/runtime probes are errors, never skips.
`--language` explicitly selects a partial run and does not qualify the full
inventory. Use `SERIALIZER_KOTLINC` for an installed compiler launcher and
`--wsl-languages` for explicitly selected Linux SDKs on Windows. Builds, package
caches and exchange output belong beneath the ignored build directory.

See the [verification record](../verification/verification-xml-advanced-contracts-2026-10-19.md)
for exact executed checks, providers, platform boundaries and retained artifacts.
