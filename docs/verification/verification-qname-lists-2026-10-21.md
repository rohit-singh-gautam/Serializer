# QName-list schema identities — 21 October 2026

[Pending requirements](../pending.md) · [XSD tooling contract](../serialization/xsd_tooling.md)

The implementation batches add C++, Python, JavaScript and TypeScript XML QName-list enumeration,
default and fixed constraints, including an enumerated list inside a union
member. Compiler binding owns each schema item's expanded name and original
order. Instance prefixes never reinterpret schema-owned lexical values.
Scalar attributes, typed list children and global simple-content text are
qualified. Native field IDs and wire layouts are unchanged; affected generated
modules must be regenerated.

## Implemented boundaries

The schema binder retains expanded enumeration/default/fixed item identities.
Prepared compiler scalar validation compares lists item by item without copying
an owning instance list. Contract comparison diagnoses changes to expanded
QName scalar/list default/fixed identities even when their lexical spelling is
unchanged. XSD export rejects these namespace-dependent list constraints before
publishing a partial or misleading schema.

Defaults allocate collision-free prefixes in the corresponding XML scope.
List order, duplicate items, empty lists, Unicode local names and no-namespace
items remain meaningful. Private field scopes preserve schema-owned defaults
even under an active nonempty input default namespace. Root attribute/text
presentation reconciles conflicting declarations while preserving other QName
identities and the first selected union member. C++ normalizes a root lexical
value only when the destination namespace scope cannot preserve its original
expanded identity; this happens while constructing a fresh input candidate.
Python/JavaScript/TypeScript output leaves native payloads unchanged.
Existing seven-backend gates, namespace-dependent outer
union restrictions and union defaults remain in place. These Python graphs keep
the existing whole-document decoding path. Automatic import now maps builtin
QName lists to string arrays and preserves their list metadata; attached
constraints still pass through each selected backend's capability checks.
Existing empty QName-list defaults still export
because an empty list requires no schema namespace identities.

Schema identity expansion charges each retained name object and its local/URI
bytes. Defaults/fixed values have a 16 MiB logical metadata bound; list
enumerations share their scalar resolution's existing 16 MiB facet-expansion
bound. These bounds prevent small lexical lists from duplicating a long
namespace URI without admission. They are not measured process-memory ceilings
or guarantees about allocations inside an optional XML provider.

## Initial Python verification

The actual Release compiler was built through the root Windows configure
wrapper and CMake's native build pipeline in
`out/build/pending-xml-batch-2026-10-21/`. MSVC toolset 14.51 uses C++20; its
imported GTest and compression libraries and runtime DLLs resolve to the
provider installation's Release configuration. Final native test launches
inherit Windows error-dialog suppression and capture diagnostics to logs.

| Inventory | Actual result |
| --- | --- |
| `python_xml_rich_test.py` | 19/19 tests pass |
| QName-list positive combinations in that suite | 12 list/union/default/optional-fixed combinations plus a standalone child default and two global-text defaults/fixed cases |
| Negative list checks | Four independent wrong-order/length/namespace/unbound controls; edited fixed attribute/text output; exact caller-exception identity |
| Protected generation/export | Three URI-amplification rejections preserve existing Python output and dependency files; unsupported XSD export preserves its destination |
| `xml_scalar_contract_test` | 18/18 GTests pass, including owned schema list identity, empty enumeration value and callback failure |
| `xsd_import_test` | 24/24 GTests pass, including nested-list backend selection, changed expanded-default compatibility diagnostics and compatible empty-default export |
| `xsd_import_cli_test.py` | Staged stable IDs, reviewed imports, protected files and exact ambiguity checks pass |
| `xml_schema_tools_test.py` | Bundle pins, dependency files, contract tools and protected publication checks pass |

The Python runtime is **3.14.3**; the independent XML oracle is lxml **6.1.3**
with libxml2 **2.11.9**. Original output scopes are inspected using the provider's
per-element namespace maps and separately authored expected expanded identities;
the oracle imports no generated reader. All four native protocols are checked
for unchanged output before and after XML encoding, with native reload controls.

The selected libxml2 oracle has three observed QName-list limitations. Redundant
empty default-namespace declarations affect its list comparisons; shadowed
instance prefixes can incorrectly reinterpret element defaults; and global
QName-list fixed text can compare through the schema's lexical spelling. Tests
keep the original-scope identity assertions and use equivalent, independently
authored controls only for those provider decisions. Redundant resets are
removed only after proving every default namespace is empty. The global fixed
fallback requires the exact fixed-value diagnostic and separately validates the
equivalent schema-spelled value. The provider also cannot compile the empty
QName-list enumeration fixture; the prepared C++ contract tests that value
directly, while generated Python empty-list controls use unrestricted list
types. Passing this suite does not qualify that provider's unsupported decisions
as a general runtime-XSD validation contract.

The executed compiler SHA-256 is
`0e53eda206d70ec473837c655e9f3a14462ac4a6f4552ad9505bb0964f9303d8`.
Final logs are `python-rich.log`, `native-contracts.log`, `xsd-import-cli.log`
and `schema-tools-cli.log` in the same ignored build directory. No Linux replay,
cross-backend QName-list restriction matrix, broad corpus or performance claim
is made by this record.

## C++ and JavaScript/TypeScript completion replay

The subsequent real Release compiler in
`out/build/pending-xml-completion-2026-10-21/` has SHA-256
`65e031d533f6c21c7bc4223fb48f76ade284bacdc04ff065e41c115051dae7c2`.
It was rebuilt through the Windows wrapper/CMake pipeline with the installed
Release provider libraries. All native launches inherited Windows error-dialog
suppression. These executed results supersede the initial active-default guard:

| Inventory | Actual result |
| --- | --- |
| `xml_qname_list_test` | Generated C++ list enums/fixed/defaults, fixed arrays, nested union members, root/child default resets and alias shadowing pass. Chunk sizes 1/65536 cover an early attribute default followed by a character-reference-encoded unbound union prefix. Four native protocols remain unchanged across XML output/reload. |
| `xml_test`, `xsd_import_test`, `xml_scalar_contract_test` | All three native targets pass, alongside the new generated test. Import now preserves plain builtin QName-list metadata and passes all eleven backend capability checks; compatible empty defaults retain their tree API/export path. |
| `python_xml_rich_test.py` | 20/20 tests pass, including independent root scalar/list defaults, inherited nonempty defaults, Unicode/list identity controls and cumulative namespace admission. |
| `javascript_xml_scalar_test.py` | 7/7 tests pass with actual Node 22.23.1 and strict TypeScript 5.9.3 consumers. The shared schema fixture covers all generated list constraints, ordered expanded identities, root scalar/list defaults, alias conflicts, untouched union fallback/native payloads and invalid output mutations. DOM readers are disabled. |
| `javascript_xml_provider_test.py` | 6/6 actual libxml2-wasm 0.7.1 provider tests pass. Generated JS/TS APIs poll the caller operation before and after validation; late cancellation prevents input/output publication, while an original provider exception remains unchanged. Four worker isolates retain independent prepared handles. |

Output identities are inspected with independent lxml per-element namespace
maps. The JS/TS output normalizer runs only when private fields require
conflicting declarations; ordinary output keeps its previous presentation.
C++ keeps existing lexical strings when their current scope still represents
the same expanded names, including a compatibility control for an empty-list
default under an inherited nonempty namespace. Prefix inventory and generated
private-scope storage have explicit cumulative work/allocation admission.

Final logs are `qname-native.log`, `xml-runtime.log`, `python-rich.log`,
`js-scalars.log` and `js-provider.log` in that ignored build directory.
Useful SDK installations/build outputs and final logs are retained; failed
initial dependency-copy scratch is removed. No eleven-backend QName-constraint
parity, Linux replay or performance claim is made by these four-backend checks.

## Assigned XML requirement audit

This implementation is a slice of the broader pending requirements. The audit
does not count a new slice as completion of an eleven-backend parent.

| Requirements | Outcome and remaining acceptance gate |
| --- | --- |
| `OPC-R01`, `OPC-R03`, `XML-B03`, `XML-B10` | C++/Python/JavaScript/TypeScript QName-list constraints above are implemented. Other backend restrictions, namespace-dependent outer unions/defaults, richer grouped particles and broader numerical/provider boundaries remain. |
| `OPC-R02`, `XML-B09` | Existing positioned-extension and content contracts remain intact. Broader mixed/all/presence combinations, typed foreign dispatch and required vocabulary capabilities require additional implementations. |
| `OPC-R04` | Multiple typed repeated branches and grouped heterogeneous storage remain gated where native storage cannot preserve occurrence order. |
| `OPC-R05` | Existing pinned Core 1.4/XML schema fixtures do not replace an independent complete official 3MF corpus or package/application conformance inventory. |
| `XML-B02` | Existing checked chameleon/bundle decisions remain valid. Additional content/identity/pattern combinations and application migration automation remain separate. |
| `XML-B08` | No new tree-free path or retained-tree/helper cost measurement is supplied by this slice. |
| `XML-B14` | Existing model lifecycle/generic/inheritance contracts retain their gates for unimplemented recursive, managed/view and combined graphs. |
| `OPC-R10`, `XML-B15` | Existing live-reference/projected-output implementations remain unchanged. Broader stress/resource/allocation qualification and any durable XML field-address contract remain separate. |
| `XML-B24` | Restricted HTML retains its documented adapter contract; arbitrary HTML/unrestricted XHTML is not enabled. No additional adapter is claimed here. |
| `XML-B12` | The existing complete [XSD evolution inventory](verification-xsd-evolution-2026-10-21.md) directly covers reviewed imports, inline rename exceptions, native order/defaults/behavior and eleven-output native/XML exchange. This is evidence for backlog reconciliation, separate from this runtime change. |
| `XML-B01` | Existing [XSD tooling](verification-xsd-tooling-2026-10-21.md) and evolution inventories qualify root/namespace/name/placement/wrapper authoring and protected ambiguous associations. Arbitrary XSD vocabulary graphs still retain their specific capability gates. |

`.gitignore` already covers the isolated build, logs and SDK caches. Generated
fixture directories are temporary and are removed by their tests. Useful native
build outputs and the final verification logs are retained for replay.
