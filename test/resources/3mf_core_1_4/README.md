# Pinned 3MF Core 1.4 schema fixture

`core.xsd` is Appendix B.1.1 of the [3MF Core Specification, tag
1.4.0](https://github.com/3MFConsortium/spec_core/blob/1.4.0/3MF%20Core%20Specification.md).
Its only change is replacing the remote XML namespace `schemaLocation` with
`xml.xsd`. This keeps qualification offline and preserves the upstream grammar.
The extracted block includes a final LF. The original specification and schema
are covered by [LICENSE.core](LICENSE.core).

`xml.xsd` is an unchanged copy of the [W3C XML namespace
schema](https://www.w3.org/2001/xml.xsd), retrieved on 2026-10-19. See
[LICENSE.xml](LICENSE.xml). No transitive network access is required or enabled.

SHA-256 values:

| Input | SHA-256 |
| --- | --- |
| Upstream Core specification Markdown | `4db7a7a13e7f757c6afc1a9ad731b56d8d068b1a766025692ff757d0f7f16940` |
| Extracted Core XSD before the location rewrite | `9d431ec149c02bd31909aba6c4ef566799786488dc1402b30f5fc37f4b53fe9c` |
| Local `core.xsd` | `195905e9d3abb084ee9394fdd0bfeb6522d934e6eed9a5f6f6340c9c1e9e29e1` |
| Unchanged W3C `xml.xsd` | `61960fb3131e38022caad5360e2f33a3382578ab3c80cd58bd74320ede61b20c` |

This fixture qualifies schema import and binding. A successful import does not
establish all 3MF package, geometry, resource, or extension application rules.
`minimal.model` is a Serializer-authored tetrahedron model used for generated
codec qualification; it is not an upstream fixture or a complete 3MF package.

The compiler's closed grammar projection works around two provider limits.
libxml2 rejects the valid finite `maxOccurs="2147483647"` used by Core, so only
provider-unrepresentable finite maxima become `unbounded` in that temporary
projection. The compiler checks the original numeric spelling and interval first;
binding and generated checks retain the exact original maximum. This provider
projection is therefore a superset used for grammar checking, not the native
cardinality contract. Source bytes and bundle identity are unchanged.

The projection also canonicalizes checked XSD `mixed` and `abstract` booleans to
`true` or `false`, because the provider rejects otherwise valid surrounding XML
whitespace. Native metadata accepts `true`, `false`, `1`, and `0` after whitespace
collapse and rejects other spellings. Other provider facet limits are not widened.
