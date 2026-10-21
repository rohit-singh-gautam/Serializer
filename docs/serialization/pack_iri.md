# Outer package and Pack-IRI addressing

The C++ runtime supplies the fifth-edition OPC external addressing layer through
`<rohit/package.hpp>`. This layer composes and parses locators and resolves relative
references; it does not open archives, interpret filesystem paths or fetch network
resources. It is independent of XML and compression providers. Existing
`resolve_target(source, target)` continues to resolve internal OPC relationships
to part names; it does not become an outer package resolver.

```cpp
namespace package = rohit::serializer::package;
auto locator = package::compose_pack_iri(
    "https://example.org/models/package.opc", "/models/main.xml");
auto address = package::parse_pack_iri(locator);
// address.package_iri == "https://example.org/models/package.opc"
// address.part_name == "/models/main.xml"
auto sibling = package::resolve_pack_reference(locator, "other.xml#selection");
```

`pack_address` owns `package_iri`, `part_name`, optional `query` and optional
`fragment`. The embedded package IRI retains its own query. An outer query is
retained separately and is ignored for part lookup. A fragment identifies part of
the addressed resource under the application's vocabulary. Empty `part_name`
identifies the entire package; absent path and `/` parse to that same identity.
Optional selectors preserve present-empty `?` and `#` distinctions.

`compose_pack_iri(package_iri, part_name)` requires an absolute package IRI,
discards its fragment, encodes the embedded package's reserved delimiters and
replaces its slashes with commas. The overload taking `pack_address` additionally
emits the stored outer query and fragment. Parsing reverses comma replacement
before decoding ASCII percent escapes, exactly once. Encoded commas remain
commas, and `%252F` does not become `/`. UTF-8 outer IRIs are accepted. ASCII-only
part names use the same grammar and identities as the OPC runtime; raw and escaped
non-ASCII part names report `unsupported_feature`. A package locator whose scheme
is itself `pack` is prohibited by the fifth-edition rule against embedded packages.

The implementation follows clauses 6.3.3-6.3.4 of
[ECMA-376 Part 2, fifth edition](https://ecma-international.org/publications-and-standards/standards/ecma-376/)
and [RFC 3986 percent encoding](https://www.rfc-editor.org/rfc/rfc3986#section-2.1).
The published fifth-edition examples spell colon as `%3c`; this is inconsistent
with both the text's percent-encoding algorithm and RFC 3986: colon's hexadecimal
octet is `3A`, while `3C` denotes `<`. The API emits `%3A` and rejects an example
whose decoded package locator contains `<`. It also rejects the older IANA/.NET
authority spelling with unescaped colon. It does not silently reinterpret either
spelling as another package.

`resolve_pack_reference(base, reference)` applies the complete RFC 3986 section
5.2 algorithm, including empty references, inherited queries, external absolute
IRIs, dot-segment removal, network-path references and unchanged query/fragment
content. Its output is a resolved IRI, which can be external or a directory rather
than an addressable OPC part. Call `parse_pack_iri` before looking up a part; this
separate check rejects directories, encoded unreserved characters and other
non-PartName paths. Neither function resolves a locator through the filesystem,
DNS, HTTP or another external service.

`equivalent_pack_iri(left, right, compare_packages)` checks pack syntax and OPC's
ASCII case-insensitive part identity, ignores outer query/fragment for resource
identity, and invokes the required caller policy on the two decoded package IRIs.
The caller implements scheme-specific outer package equivalence, such as HTTP's
normalization or an application's private identifier scheme. The runtime cannot
infer private scheme semantics or resource aliases from byte strings. A missing
policy rejects; exceptions from a supplied policy propagate unchanged. This is
an explicit [application responsibility](../project/application_responsibilities.md),
separate from OPC's library-defined part comparison.

Each function accepts an optional `package::budget*` for one synchronous
operation. The caller retains that non-thread-safe budget until completion.
Otherwise the function uses a fresh default budget. Retained addressing metadata,
decoder allocation admissions and work share cumulative counters. Output lengths
are overflow-checked, interruption is checked during long transforms, and failed
work is not refunded. These are logical admissions, not allocator hooks or measured
process peak guarantees. Invalid locators use structured package errors; unsupported
optional naming/nesting reports its own category.

## Optional features and selected production policy

Fifth-edition OPC deliberately makes the following optional:

| Capability | Selected Serializer policy | Standard boundary |
| --- | --- | --- |
| Non-ASCII part names | Unsupported; outer package IRIs may still contain Unicode. | Clause 6.2.2.2 does not require non-ASCII part-name support. |
| Interleaved physical parts | Unsupported by the current package profile. | Clause 7.2.4 recommends support but explicitly does not require it. |
| Growth-hint production | Not produced by the ordinary fresh-package writer. | Clause 6.2.4 makes a growth hint optional; 7.3.8 describes its physical representation. |

These production/naming choices are documented exclusions, rather than missing
mandatory fifth-edition addressing requirements. They do not waive other Annex B
consumption or editing pass-through requirements, which remain separately tracked
under `OPC-R08`. Applications select payload vocabularies, transport and trusted
resource locations; those host/business policies will not be inferred by the
library. See [verification](../verification/verification-pack-iri-2026-10-21.md)
and the [general package API](xml_packages.md).
