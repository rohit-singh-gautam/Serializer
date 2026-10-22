# Native QName restrictions and schema defaults — 22 October 2026

Compiler/runtime **2.28.0**, schema language **1.21.0**. C/Rust owning XML
companions validate QName/list restrictions, ordered namespace-dependent union
selection, outer enumeration/fixed identities and schema-owned defaults. Native
model structs and their four wire protocols retain their existing contract.

Each union member is tested in the occurrence's actual namespace scope. An
unbound QName prefix can fail that member and select a later string member;
resource failures do not become lexical fallback. Immutable schema literals
retain their selected primitive families and expanded names, so instance aliases
cannot redefine an enumeration or fixed value. Union lexical spelling is retained
after admission to keep subsequent member selection stable.

Defaults carry a bounded XML-only origin flag per scalar/attribute occurrence.
An unchanged schema-origin value can serialize as the original missing attribute
or empty element, reapplying the schema literal without importing its prefixes
into another field's instance scope. Explicit instance values are validated in
their occurrence scope. Edited values cannot acquire schema-origin treatment
merely by sharing a local name.

## Executed Windows SDK checks

| Maintained corpus | Result per actual C/Rust SDK |
| --- | --- |
| `native_xml_qname_constraints_test.py` | 25 QName/list enumeration decisions, aliases, ordered items, local scopes and edited output rejection |
| `native_xml_outer_qname_union_test.py --schema-defaults` | 35 child-element union/enum/default/fixed decisions |
| Same corpus, `--attributes` | 29 owner-attribute decisions, including absent schema defaults |
| Same corpus, `--owner-text` | 29 owner-text/default decisions alongside independent QName attributes |
| `native_xml_qname_defaults_test.py` | 41 direct QName/list default/fixed decisions, including active instance default namespaces and conflicting schema-prefix aliases |
| `native_xml_restricted_list_test.py` | 35 restricted atomic/union item decisions, per-item selected families, list enum/fixed/default identities and scoped QName/string fallback |
| `portable_xml_outer_union_test.py --language c,rust` | 29 primitive outer-union regression decisions, edited-output rejection and four native protocols |

These invocations pass with Rust warnings denied and MSVC C11 warnings-as-errors.
Consumers release original XML bytes before replay, compare all four native
protocols, require exact/one-tight preservation admission and recover with fresh
policies. C also checks unchanged native destinations after rejected input.
Logs are `out/opc-native-{leaf-scoped-c,union-defaults,union-attribute-defaults,union-owner-defaults,direct-defaults}.log`;
the original Rust leaf result is in `out/opc-native-leaf-scoped.log`.

## Independent provider boundary

The direct default corpus checks expanded values using independent lxml namespace
maps. Its 41 input decisions and 17 emitted documents per SDK also pass JAXP
against both original and exported XSDs. **Eleven positive default presentations
per corpus require equivalent presentation controls**: schema-owned defaults are
materialized with compatible namespace aliases in separate oracle files. An
assertion verifies unchanged expanded values; the original SDK inputs and outputs
remain untouched. This does not claim that JAXP accepts every original empty
QName-default presentation when the instance shadows the schema prefix.

This is the same provider boundary documented in the earlier
[QName constraint qualification](verification-qname-constraints-2026-10-21.md#independent-provider-boundaries).
The historical [W3C default-QName issue](https://www.w3.org/XML/2004/07/xs11-pre-lc-issues/#wd-22)
also records the ambiguity of supplying a schema prefix that is absent or rebound
in an instance. Provider disagreement is retained separately from generated
codec decisions.

Restricted list items compile to immutable recursive scalar rules. Each token
passes the complete named or inline item restriction before its first successful
union member contributes an ordered value identity. List-level enum/fixed
comparisons retain those per-item families and expanded names instead of
reinterpreting the entire list as one primitive type. Nested lists remain invalid
under the XML Schema list contract.

The restricted-list corpus independently validates all 35 input decisions and
17 emitted documents per SDK against the original and exported schemas using
JAXP. Logs are `out/opc-native-restricted-list.log` (Rust),
`out/opc-native-restricted-list-c.log` (C),
`out/opc-native-union-list-regression.log` (Rust regression) and
`out/opc-native-union-list-c.log` (C regression). The two initial combined runs
finished Rust successfully before an unset CMake generator prevented C toolchain
discovery; the separate C invocations set Ninja Multi-Config and pass.
These focused checks do not substitute for the final repository regression.
