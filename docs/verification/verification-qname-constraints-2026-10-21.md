# QName constraints in five portable codecs — 21 October 2026

Compiler/runtime **2.24.0**, schema language **1.21.0**. This record qualifies a
further implementation slice of `OPC-R01`/`OPC-R03`; both parent requirements
remain open. No release was committed or published.

## Implemented contract

Generated Go, Java and C# codecs compare QName enumeration and fixed values by
namespace URI and local name, and QName-list values by ordered expanded items.
They materialize schema defaults in private schema bindings even when the
instance shadows those lexical prefixes or has an active default namespace.
An explicitly empty attribute does not activate its default. A missing required
fixed attribute rejects before publication.

QName/list union branches retain their own enumeration contracts. The first
complete successful member determines whether the value carries QName, list,
numeric or ordinary string semantics. Declaration regeneration must retain that
selection; a string-first member and an unbound string fallback stay ordinary
text. No XML encode mutates the caller's native fields. Metadata stays outside
native JSON/binary records and existing field IDs.

Kotlin/Swift additionally implement scalar/list expanded enumeration. Their
namespace-dependent defaults/fixed values and unions retain explicit gates.
All five actual SDKs qualify scalar QName and child QName-list pattern aliases,
lexical prefix-list string enumeration and collisions with generated element
prefixes. Go/Java/C# perform a bounded borrowed namespace reservation pass only
for graphs needing namespace-sensitive union or lexical preservation. Generated
helper overloads reuse existing private method names, including when a native
model declares `srl_xml_reserve_namespaces`. No owning payload model is built by
that preflight. Conflicting owner default vocabularies reject before bytes return.
Optional QName attributes equal to their schema-owned defaults may remain
physically absent when that is necessary to preserve an owner's union default;
XSD augmentation retains their values. Identity-observed attributes retain their
explicit output boundary.

## Executed checks

The actual compiler was rebuilt with MSVC 14.51/C++20 through the root Windows
configuration and CMake target pipeline in `out/build/ReleaseWindows/`.
Its SHA-256 at this checkpoint is
`ab74b6c55520570012822f639b6705813981871c33389d8eb72c6c02426d2780`.
The expanded three-codec pattern checkpoint used
`be9a117453f19b13476823287c3e0fe3d3c5248b7881dd3f6342ed5589b0616a`;
the final mobile checkpoint adds only the mobile enumeration paths and shared
backend qualification. Later raw-literal splitting preserves emitted runtime bytes.

The final compiler SHA-256 is
`fb02bfdfa90e894f8b891f1e7470ea4e14ee92f2097849a9347ec8d61a63a351`.
The root Windows build and CTest run passed 277/279 tests initially. The Kotlin
package test exposed an unqualified `Throwable` helper colliding with a valid
schema type; the generated helper now explicitly uses `kotlin.Throwable`.
The concurrent Kotlin compression runner could not fingerprint the executable
while that compiler rebuild replaced it. After the compiler stabilized, both
failed tests passed on replay (2/2, 119 seconds). All 279 registered tests are
therefore accounted for across the full run and replay, rather than one
uninterrupted all-pass run. Logs: `regression-final.log`,
`qname-kotlin-collision-build.log` and `regression-replay.log`.

| Check | Result |
| --- | --- |
| Maintained `go_xml_qname_constraints_test.py`, Go/Java/C# | 37 shared documents per SDK, 111/111 decisions pass; 17 successful outputs per SDK independently checked |
| Maintained `mobile_xml_qname_constraints_test.py`, Kotlin/Swift | 17 shared documents per SDK, 34/34 decisions pass; six successful outputs per SDK independently checked |
| Independent JAXP validator | Three-codec run: 37 input decisions and 51 actual outputs; mobile run: 17 inputs and 12 actual outputs; provider boundaries are qualified below |
| Existing namespace suite | 13 probes per SDK, 39/39 pass, including Unicode attributes, local/shadowed scopes, optional/repeated values, copied native bindings and retention bounds |
| Existing mobile namespace suite | 10 probes per SDK, 20/20 pass, including Unicode, copied bindings, explicit default absence and retention limits |
| Existing exact scalar suite | 122 probes per SDK, 366/366 pass; one supported QName-list generation check and four protected unsupported-binding checks |
| Existing mobile scalar suite | 183 probes per SDK, 366/366 pass; five binding checks per SDK retain supported enumeration and protected unsupported combinations |
| Typed variant regression | 110 probes per Go/Java/C# SDK pass, including empty type namespaces and inherited collisions |
| Lifecycle regression | 57 probes per Go/Java/C# SDK pass, including native protocols, revisions, magic and owning digests |
| Mixed-content regression | 21 probes per Go/Java/C# SDK pass, including private native mixed storage and resource checks |
| Native `xsd_import_test` | 26/26 GTests pass, including five-backend expanded enum binding and the two remaining backend gates |

The actual SDKs are Go **1.26.4 windows/amd64**, Oracle JDK **26** (generated code
compiled with `--release 17 -Xlint:all -Werror`) and .NET SDK **10.0.402** targeting
.NET 8 with nullable analysis and warnings as errors. Go controls compare all
four native protocols before/after XML encode; Java/C# controls compare native
positional binary. All consumers also decode successful generated XML and reject
mutated enumeration, fixed and union values before returning output.

The mobile SDKs are Kotlin **2.2.21** with JDK **26**, and Swift **6.4.0** on
Windows. Generated sources compile with warnings as errors. Both compare all
four native protocols before/after XML encode and after decoding the emitted XML.

The 37 documents cover equivalent aliases, no-namespace enumeration, wrong
attribute/child identities, unbound prefixes, ordered attribute/typed-child/
lexical-child lists, child scope changes, schema-prefix shadowing, absent defaults,
required fixed attributes, explicit fixed aliases, empty attributes, QName/list/
numeric union branches, string-first members, unbound string fallbacks,
generated-prefix capture, owner attribute/text defaults, nested owner graphs,
pattern aliases and prefix-list enumeration. Mobile tests reuse the same 17
enumeration/pattern fixtures and independent expected identities.

## Independent provider boundaries

The original XML is parsed with independent lxml namespace maps, and expected
expanded identities are authored separately from generated readers. lxml checks
ordinary enumeration documents directly. Its XSD implementation rejects the
otherwise valid unbound QName/string fallback, so JAXP validates the original
union documents instead. Neither provider imports generated XML code.

Both selected validators misinterpret some schema-owned QName-list element
defaults when their schema prefix is shadowed in the instance. Seven default/fixed
input fixtures therefore have independent equivalent presentation controls:
expanded element/attribute names, attribute presence, child order and explicit
expanded values are retained, while namespace aliases are normalized. Assertions
on the original generated output independently require the intended schema-owned
defaults, including the no-namespace value. JAXP validates all 63 original emitted
documents directly. These controls do not claim general conformance of either
provider's default handling. The existing
[QName-list record](verification-qname-lists-2026-10-21.md) documents earlier
observations of the same provider boundary.

lxml also misclassifies an allowed no-namespace QName enumeration in mobile
outputs that repeat `xmlns=""` on prefixed descendants. Original mobile input
decisions pass its XSD checks. For output, lxml independently checks the actual
expanded identities and unchanged lexical values; JAXP validates every original
mobile output without namespace normalization. No provider disagreement is
counted as a successful generated validation decision by itself.

An initial fixture reused an attribute/child selector ambiguously and was
corrected before generation succeeded. Subsequent native consumer mistakes
(Java list indexing and C# nullable annotations) were corrected in their maintained
sources/generator before the final three-SDK run passed. Failed commands do not
count toward these results.

## Remaining acceptance work

Kotlin/Swift namespace-dependent defaults/fixed values and unions, C/Rust scalar/list
constraints, broader namespace graphs, outer-union enumeration/fixed/default identities and
schema export combinations remain gated. This record does not close `OPC-R01`,
`OPC-R03`, other OPC parents, broad numeric/provider boundaries or Linux parity.
Broader lexical pattern/output graphs, including typed attribute lists sharing
conflicting owner defaults, need further implementation/qualification.

Logs and useful baseline generated consumers are retained under ignored
`out/build/pending-active-2026-10-21/`: `qname-pattern.log`, `mobile-qname.log`,
`namespace-final.log`, `mobile-namespace-final.log`, `scalars-final.log`,
`typed-variant-final.log`, `lifecycle-final.log`, `mixed-final.log`,
`mobile-scalars-final.log`, `xsd-import-final.log` and the native compiler build logs. The constraints runners remove their private generated
scratch after each run. Existing `.gitignore` covers all output.
