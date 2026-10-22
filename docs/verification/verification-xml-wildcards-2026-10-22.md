# XML wildcard admission — 2026-10-22

This record separates namespace/position admission from declaration validation
and from the decision to preserve an admitted payload. It records actual SDK
checks for the 2.28 worktree; later source edits require their own regression
qualification.

## Namespace predicates and positions

`test/xml_wildcard_skip_test.py` independently classifies each original fixture
with its local XSD before running the real generated consumer. Six predicates
(`##any`, `##other`, `##local`, `##targetNamespace`, one foreign URI, and a
local/foreign URI set) are each exercised at prefix, middle and suffix positions.
The complete eighteen-variant corpus has **672 input decisions per output**.

All eleven outputs — C++, C, Rust, Python, JavaScript, TypeScript, Java, C#, Go,
Kotlin and Swift — have passed the complete corpus.
The C++ adapter exercises both public XML paths and compares all four native
protocols. C/Rust use their separate XML document companions; their native model
and wire representations remain unchanged. Successful output is independently
validated and compared by expanded names, attributes and opaque text/child order.

Controls include required and excessive wildcard occurrences, forbidden
namespaces, exact positions, required native fields, native lexical rejection,
same-named completed or upcoming native occurrences, and attribute/element kind
separation. Generated predicates retain their XSD meaning; authored explicit
foreign slots retain their closed core/known-name behavior.

The native three-output replay is retained in
`out/opc-wildcard-native-matrix-final.log`. The other completed SDK logs are
`out/opc-wildcard-{python,javascript,typescript,java,csharp,go,kotlin,swift}-final.log`.
These ignored build logs support the maintained fixture and this durable record.

## All groups and scalar text

All ten non-C++ outputs pass the 26-decision all-group/foreign-attribute/suffix
corpus and the 22-decision scalar-prefix-text/suffix corpus. C++ additionally
passes both `xml_suffix_extensions_test` GTests through tree/token public paths.
The checks retain unordered native all-group values, optional members and exact
opaque suffix positions. Scalar text preserves leading whitespace and CDATA;
text after the foreign suffix, including whitespace, rejects.

## Declaration validation

The strict/lax implementation uses immutable declarations from the pinned local
bundle and private generated validation DTOs. Temporary replay and decoding must
share the original operation's work/allocation admission. Skip validation must
not consume preservation quotas. Required capability checks remain independent
of foreign preservation.

The independent strict/lax corpus is maintained in
`test/xml_wildcard_validation_test.py`: **142 original XSD decisions per mode**,
with strict/lax independently combined with preserve/skip (**568 per output**).
All eleven outputs have passed all four modes through their actual SDKs and
four native protocols. C++ also calls both public XML facades and tests mutable
caller-owned callback counters, exact/one-tight bounds and edited declarations.
The integrated regression and final checkpoint differences are recorded below.

The corpus covers scalar enumeration/pattern/bounds, QName aliases and unbound
prefixes, restricted lists, complete sequence/all/choice DTOs, nested strict
wildcards, required/faceted attributes, explicit-empty global attribute values,
no-namespace attributes under foreign default scopes, fixed/default empty
elements, nil required attributes/content/fixed rules, exact and permitted
derived simple/complex types, abstract declarations/types, blocked derivations,
unknown lax declarations and attribute/element kind separation. Original inputs
and successful output are checked against independent local XSD processors.

Private validators are compiled from the originally pinned bundle, including
reachable nested declarations and permitted derived type inventories. Removing
or replacing source files after preparation does not change the inventory.
Compiler tests cover cache lifetime, registered inventory reuse, derived
declarations introduced in another namespace and nil attribute projections.
No runtime schema acquisition or `schemaLocation` dispatch occurs.

Java/Kotlin resource and collision details are recorded in the
[JVM wildcard checks](verification-jvm-wildcard-validation-2026-10-22.md).
Actual SDK work/allocation/preserved exact and one-tight probes use the original
operation in both directions; skip permits zero preservation allowance through
nested temporary DTOs. Failure probes retain the caller's original error and
leave native payloads and opaque companions unchanged. C/Rust use public native
document/operation bridges; opaque Rust state has no public mutation API.

`test/xml_wildcard_identity_policy_test.py` independently checks a declared
global with an XSD key and a second global which reaches that key through a
nested strict wildcard. All eleven actual SDKs pass the four valid/duplicate
input decisions and four native protocols. A caller-selected zero tuple limit
rejects valid direct and nested content on decode and encode; restoring the
default policy succeeds. C/Rust additionally exercise prepare-only admission
and the caller's concrete resource failure. Their policy propagation and fresh
568-decision resource replays are recorded in the
[native declaration checks](verification-native-declaration-operations-2026-10-22.md).

## Foreign fallback and compositor combinations

`test/xml_unknown_policy_test.py` checks **48 decisions per output/retention**:
general foreign skip/preserve fallback, exact authored reject/preserve precedence,
known foreign attributes/elements, cross-kind names, core/local rejection,
misplaced known fields, required known fields and native protocol parity. It
also compares original default namespace absence against the emitted opaque
scope. The 300-field variant exercises positions beyond small-field adapters.
All eleven outputs pass both five- and 300-field models with preserve and skip:
**192 decisions per output**.

`test/xml_group_extensions_test.py` combines singleton all/attribute wildcards,
singleton choice/XSD suffixes and singleton choice/authored suffixes with each
of skip/lax/strict: **nine variants and 294 independently classified decisions
per output**. All eleven actual SDKs pass the complete matrix. C++ checks both
public facades using the frozen compiler/runtime/header snapshot
`65a61bbf803f68c8c34da3193724b2bdb7ed40ee6dc472fd7d5ecccc4bc2e3b3`
and `/W4 /WX`; logs are `out/opc-final-group-cpp-<group>-<process>.log`.
Mid-group heterogeneous storage still requires
the explicit ordered-content representation rather than implicit hidden order.

The [legacy C++ wildcard matrix](verification-legacy-cpp-wildcards-2026-10-22.md)
adds 112 independent strict/lax preserve/skip decisions through existing
repeated-choice tree mappings. Required capability checks apply on decode and
encode even when skipped foreign state is empty. Independent foreign-document
limits, caller-bound restoration, exact/one-tight shared resource admission and
edited declaration rejection pass. The baseline generated 3MF Core, XML
operation, token and extension-slot repository targets also build and pass their
four focused CTests.

The integrated regression below completes `OPC-R02` alongside these SDK matrices.

## Empty declaration inventories

`test/xml_wildcard_empty_inventory_test.py` uses an import-free XSD whose foreign
wildcards have no matching global declarations. All eleven actual outputs pass
strict/lax combined with preserve/skip: **64 independent decisions per output**,
with original/emitted XSD checks and all four native protocols. Strict rejects
undeclared foreign attributes and elements; lax accepts them under the selected
retention policy. Unknown core names, misplaced/excess occurrences and missing
known fields still reject. C++ exercises both public facades.

C++ and the eight portable outputs use compiler snapshot
`fbb7245b35067563de65d205f3cdb51ee02b10ed2f68df7a87120ef181a43398`.
C and Rust use
`744e32a98729a0ef345ea3d48fab12bfa3eb07805a5a0760310f12e23bff739a`,
which additionally marks their empty-dispatch parameters used. C++/C compile
with `/W4 /WX`; Rust compiles with `-D warnings`. The final changes remove unused
names/parameters without changing declaration selection or native formats.
Default Python and opt-in portable CTests retain all four configurations.
Logs: `out/opc-empty-cpp-<process>-<retention>.log`,
`out/opc-empty-inventory-<language>.log`,
`out/opc-empty-inventory-go-final.log` and
`out/opc-wildcard-empty-{c,rust}-<process>-<retention>.log`.

## Integrated regression

The Windows x64 `RelWithDebInfo` build completes through the root
`./make.ps1 test -BuildDirectory out/build/ReleaseWindows -Configuration RelWithDebInfo -Jobs 4`
wrapper, with normal installed SDK/temporary-directory access and four parallel
CTest jobs. The full run passes **293 of 299 cases** in 572.31 seconds. After the
focused fixes, **all six remaining cases pass** in a 28.66-second targeted replay:
all **299 repository CTest cases are verified**.

The replay includes the XSD test's corrected internal JavaScript backend label,
the restored `invalid_argument` classification for XSD wildcard maxima, and three
tests interrupted by a compiler executable lock during a focused rebuild. One
existing managed Save-fault assertion also passes on replay; this record does
not claim that its transient failure was repaired by the OPC changes. Authored
foreign-slot resource ceilings retain their existing `length_error` behavior.

The run verifies actual regenerated Java/Kotlin/JS/TS package budget diagnostics,
Rust node/attribute member diagnostics, both native operation-policy SDKs,
positive C/Rust package companions, ordered generation, Python callback identity
and replacement, Core generation, release metadata and installed-package
compatibility. Runtime literal splitting preserves concatenated emitted bytes
and passes the bounded-literal inventory check. SDK permission failures from the
earlier sandboxed run are superseded by the normal-access run.

Compiler SHA-256 at the main run's start is
`045c5666d40ecbe614edb0a255a163294682d1a11e9ba3efe011045aaad2d1be`.
The focused runtime cardinality fix and final replay use
`25edfbbba01f85e140a3d769adecaccd15e8c7aca65b53e0340410f984d927aa`.
Both report compiler/runtime **2.28.0** and schema language **1.21.0**.
Final logs are `out/opc-final-windows-verified.log`,
`out/opc-final-ctest-replay.log`, `out/opc-xsd-final-build.log` and
`out/opc-occurrence-final-build.log`. Useful builds, SDK checkpoints and logs
remain under ignored `out/`; unused diagnostic scripts and task-created Python
caches were removed.
