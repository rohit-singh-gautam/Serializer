# Scalar identities and namespace constraints — 2026-10-22

This record qualifies the 2.28 scalar compiler and generated XML changes for
`OPC-R01` and `OPC-R03`. Native model fields, schema IDs and the four native
protocols retain their existing representations. XML-only occurrence metadata
retains instance namespace scopes and schema-owned literal scopes separately.

## Implemented contracts

Union members see the original lexical input in declaration order. The first
successful member determines its normalized value, primitive family and list
item identities. Boolean, decimal/integer, float, double, string, anyURI and
QName families remain distinct where XSD value spaces require it. Decimal and
integer comparisons retain exact values beyond binary floating precision.

Enumeration and fixed/default literals retain their schema declaration scope.
An equivalent instance alias can satisfy a QName restriction; shadowing the
schema alias cannot change the stored literal identity. Absent prefixes remain
absent for string fallback selection. Lists retain item order and independently
selected atomic union members. Restrictions on an item type apply to each item;
list cardinality and outer enumeration apply to the complete list.

Prepared static runtime rules and compiler contracts share these identities.
The builtin list path remains available without per-item prepared metadata.
Restricted atomic and union item types use a prepared item rule rather than a
second INI declaration inventory.

The XSD exporter reconstructs ordered union members, restricted list item types,
inherited pattern intersections and value constraints with their declaration
scopes. It reserves source literal aliases before choosing component aliases
and does not write illegal nondefault-prefix undeclarations. A no-namespace
component needed beneath a schema literal default uses an inline type where
necessary.

## Actual SDK checks

The maintained consumers generate fresh code through the real compiler and
compile it with actual local SDKs. The scalar corpora compare expected rejection,
successful replay, edited output rejection and native identity. C++ exercises
both tree and token input/output. Python, JavaScript and TypeScript compare all
four native protocol encodings; the compact managed consumers compare binary
identity. The separate mobile QName consumer compares all four native protocols.

| Maintained corpus | Actual executed scope |
| --- | --- |
| `portable_xml_outer_union_test.py` | 29 primitive/list outer-union decisions each in Python, JavaScript, TypeScript, Java, C#, Go, Kotlin, Swift, C and Rust; C++ also has the focused `xml_outer_union_test` tree/token contract. |
| `portable_xml_outer_qname_union_test.py` | 29 schema-scoped QName/list union decisions in each of C++, Python, JavaScript, TypeScript, Java, C#, Go, Kotlin and Swift. |
| The same corpus with `--owner-text` | 29 decisions each in C++, Python, JavaScript, TypeScript, Java, C#, Go and Kotlin, with schema-default simple content and owner attributes. |
| The same corpus with `--attributes` | 29 decisions each in C++, Java, C# and Go. |
| `mobile_xml_qname_constraints_test.py` | All 37 direct QName/list enumeration, pattern, default/fixed, owner and nested-scope probes in each actual Kotlin and Swift SDK; each run includes 54 independent JAXP source/output controls. |
| `portable_xml_restricted_list_test.py` | 35 restricted atomic/union item decisions each in C++, Python, JavaScript, TypeScript, Java, C#, Go, Kotlin and Swift. The native C/Rust companion consumer executes the same 35 decisions per SDK. All eleven outputs pass. |
| `portable_xml_foreign_qname_default_test.py` | Seven decisions each in C++, Python, JavaScript, TypeScript, Java, C#, Go, Kotlin and Swift. Empty schema defaults retain their foreign expanded identity; explicit no-namespace instance values remain distinct. Every SDK checks original/exported JAXP input and output controls. |

The QName union corpus validates original and exported declarations separately
through JAXP, then validates every emitted valid document through both schemas.
An independent expanded-name comparison additionally checks restricted list
QName/string item selection. Native C/Rust companion coverage is recorded in
[the native QName constraints record](verification-native-qname-constraints-2026-10-22.md).

All 23 focused C++ tests in `xml_scalar_contract_test.cpp` pass. They cover primitive family
selection, exact numeric aliases, schema-owned QName enumeration, restricted
union list item constraints and public static list comparison. The XSD contract
tests additionally cover inherited pattern intersections and no-namespace root
component identity beneath a foreign schema literal default.

## Provider and capability boundaries

The independent XSD 1.0 providers differ on some lexical aliases and on the
presentation of schema QName defaults under instance prefix shadowing. Direct
QName default oracle controls materialize the already checked expanded value
using compatible aliases. The SDK input and emitted bytes remain unchanged;
the control does not redefine their expected value. This boundary predates the
current update and is recorded in
[the earlier QName constraint checks](verification-qname-constraints-2026-10-21.md).

The explicit `prefix_list` adapter binds a string's tokens to namespace URIs.
Its default/fixed declaration and XSD export require an external capability
contract: an ordinary XSD string default does not specify those namespace value
semantics. Ordinary prefix lists, lexical enumeration/pattern restrictions and
their retained occurrence bindings are separate implemented contracts.

The portable pattern engine is bounded and does not backtrack. Supported XSD
set/category/name syntax is defined by the current pattern contract and its
independent provider corpus. Host-regex lookaround and backreferences are not
XSD 1.0 grammar. Platform/provider combinations outside the actual SDKs and
versions exercised here remain separate qualification work.
