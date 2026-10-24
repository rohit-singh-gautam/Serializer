# XML authoring, ordered import and typed profiles: 24 October 2026

Compiler/runtime **2.34.0**, schema language **1.24.0**, editor packages **2.0.19**.
This record closes **XML-B19** for the implemented owning profile contract and
provides new authoring/import/performance evidence. The [resolution ledger](../project/xml_package_backlog_resolution.md)
closes the thirteen XML/package rows through implemented work, completed OPC
contracts and explicit scope decisions; those dispositions do not imply arbitrary
vocabulary, graph, platform or performance coverage.

## Implemented contracts

Class-local `xml(ordered("items"), ordered_child("branch", "{uri}local"))`
selects the existing ordered variant-array contract. Matching configuration
remains accepted; conflicting field/child selections reject. Package parts can
declare `target_mode`, `requires_type` and `vocabularies` in their schema. Explicit
INI profiles retain their existing override behavior. Final profile validation
runs even without configuration overrides, including codec, external-role and
relationship-source restrictions. Each declaring file requires language 1.24.

Declaration import reuses bounded scalar resolution for named, inline and union
atomic list items, retaining fixed lengths, item facets and QName identities.
Supported grouped/repeated sequence/choice graphs import into one native owning
variant array. Group occurrence grammar remains in the pinned XSD; no additional
payload arrays or order trace are created. Existing representable flat imports
retain their layout, IDs and language-1.20 header. Fresh ordered imports select
language 1.24. Import protects differing existing source and dependency files.

Ordered imports require at least two distinct nonnil complex-record alternatives.
Scalar, wildcard, `all`, mixed-text and recursive owning ordered branches remain
unsupported. Repeated names must select the same complex type. General historical
data conversion, dynamic model selection and durable field-address XML are not
enabled by this work.

Python's advanced token graphs now admit QName/prefix-list/union/list attributes
on records without scalar defaults/fixed values or owner extension storage.
Their bounded namespace snapshots accompany native values without a document
tree. Pure namespace mappings keep their established tree path. Scalar child/text
scopes, defaults and broader extension combinations retain their prior capability
gates. Exact namespace admission, one-tight rejection, one-byte UTF-8 chunking and
fresh-call recovery pass. The combined imported graph test replaces the tree
parser with a failure callback to prove that it is not used.

## Executed qualification

Windows 11 build 26300, x64 MSVC **19.51.36231**, C++23 minimum, LibXml2 **2.15.3**.
Configuration used the root `make.ps1` wrapper and existing vcpkg providers;
native builds used two jobs. No Linux or interactive editor-host result is claimed.

The package-profile replay compiler SHA-256 is
`ec29c15a6ab1c4eda985b0d91fe6a7f40e9f79b2a2d94c3cda45a95894420cf0`.
The earlier Python-only capture before the Kotlin warning repair is
`60a93f7e900c694b571fc151eb286fdb8071b362d993cfb82280a7491da17144`.
The benchmark and its generated Python source identify that earlier capture;
the immediately subsequent change affects only emitted Kotlin cancellation code.
Later HTML and Python Core-routing repairs have separate checks and compiler
snapshots; these captures are not relabeled as results for those later binaries.

| Check | Executed result |
| --- | --- |
| Focused native/language/import regressions | 11/11 CTest targets pass, including core schema-version tests, generated imported C++, legacy/future headers, protected CLI output, package version discovery and schema-local Python profiles. |
| Broader XML/package controls | 10/10 CTest targets pass: content/model contracts, restricted HTML, XSD contracts, operation accounting, ZIP/OPC, ordered multi-output publication, application references and bundle/CLI tools. |
| Imported rich graph | All eleven owning outputs generate; actual C++ and Python roundtrip, malformed groups, edited invalid values, item constraints and namespace boundaries pass. This does not claim execution of that graph in the other nine SDKs. |
| Schema-local ordered Python | 34 grouped-XSD decisions pass against the independent lxml XSD oracle; successful values preserve all four native protocols. |
| Schema-local package profiles | All eleven actual SDKs pass 16 independent inputs and eight inspected output archives each: **176 decisions / 88 archives**. |
| Kotlin cancellation | Separate generated library and consumer pass copied limits, stacked callbacks, real JAXP completion, first original exception identity, native controls and recovery. |
| Editor assets and packages | VS Code 166/166 tests; installed Visual Studio TextMate assets 546 checks across eleven tags. Both 2.0.19 VSIX packages rebuild and validate. No installation or Marketplace publication is claimed. |

The final compiler snapshot is SHA-256
`f6c92ac4b8bfbdb704dbdcabd9c56902482c41bf3a7c4d8162af10fbd66a9aaa`.
Its final focused CTest selection passes **15/15**, including schema-local package
and ordered input, protected import, Core Python routing, native HTML and the XSD
evidence guards. The separate final Python XML token suite passes **14 tests**;
the Core suite passes **three tests** and the native Core target passes.
Python Core retains its safe legacy owning-tree route when the combined graph
does not admit tokens. Unsafe fallback for token-only mixed/presence/map/variant/
inheritance storage rejects, and chunk decoding remains token-only.

The complete current `qualification/xsd_tooling.py` inventory passes **317
decisions** with unchanged compiler, source and provider/artifact fingerprints.
The maintained export tests previously expected union export to reject; the
current implementation supports it. The replay now validates integer/text union
values and missing/repeated-child failures against both original and exported
XSDs using independent lxml. Reviewed rebind/import IDs, idempotent publication,
edited-directory protection and unsupported prefix-list export remain checked.
This is compiler-tooling qualification; the report explicitly requires a separate
runtime SDK replay and does not replace the historical eleven-SDK results.
Evidence: `xml-final-regression.log`, `xml-core-python-guarded.log`,
`xml-python-contract-guards.log`, `xml-final-xsd-tooling-complete.log` and
`xml-final-xsd-tooling-complete/verification.json`. The final synchronized VSIX
rebuild after documentation changes is in `xml-extensions-final-build.log`.
The updated evidence guards also pass all **40 tests** after the union fixtures
are added (`xml-final-tooling-guards.log`). A two-record final-compiler benchmark
smoke run preserves the same independent values and native hashes in both modes
(`xml-final-benchmark-smoke.log`); its generated module is 315,981 bytes. The
20,000-record measurements below retain their earlier, explicitly pinned snapshot.

The package corpus checks XML/JSON/BINARY_NONE selection, stored/DEFLATE,
fixed/default policies, expanded root types, vocabulary inventories, inert
external targets, malformed/trailing input, owning Load/Encode/Save and protected
file publication. TypeScript additionally passes strict API compilation and
executes the generated JavaScript runtime. Existing generated key/reference checks
retain their [eleven-output qualification](verification-pending-105-2026-10-21.md)
and the current C++ application-reference regression. Together with the existing
[pipeline/rename contract](verification-package-pipelines-2026-10-22.md), the new
schema-local selections complete XML-B19. The resolution ledger reuses existing
OPC/large-operation qualification for duplicate parent requirements and defers
broader platform/provider/measurement campaigns to QA01/QA02/QA03.

Actual SDKs: Python **3.14.3**, Node **22.23.1**, TypeScript **7.0.2**, Java **26**,
.NET SDK **8.0.426**, Go **1.26.4**, Rust/Cargo **1.99.0**, Swift **6.4**, and Kotlin
**2.3.10** with JDK **17.0.18+8**. C uses MSVC C11; C++ uses the same native host.
Kotlin's installed batch launcher initially failed to locate its preloader under
its path. Direct SDK invocation resolved that environment issue. Strict Kotlin
compilation then exposed an unnecessary `!!`; the source now retains a nonnull
local and rethrows the same first exception. The final eleven-SDK replay passes.

Retained evidence is below ignored `out/build`: `xml-package-regression.log`,
`xml-import-cli-final.log`, `xml-package-broader-tests.log`,
`xml-editor-tests.log`, `xml-visual-studio-grammar.log`,
`xml-extensions-build.log`, and `xml-schema-local/replay-*/verification.json`.
The latter includes compiler hashes and exact command/output records.
`xml-schema-local/kotlin-cancellation/verification.json` records the external
module test; `tool-versions.json` identifies actual SDKs. Maintained source and
fixtures live outside generated directories; local evidence and SDK/build caches
remain ignored.

## Namespace/token performance

The reproducible [runner](../../qualification/xml_namespace_ordered_benchmark.py)
imports the actual shared XSD fixture, then measures each mode/workload in a fresh
process. It compares the same native model and data with the optional ordered
child absent: root QName/list attributes plus repeated owning item/price records.
The control explicitly selects the established retained-tree adapter. Independent
ElementTree checks values, and all four native wire hashes match between modes.
Five wall/CPU samples exclude generation, file I/O and preparation; a separate
tracemalloc call measures Python allocations. Other builds ran concurrently.

| 20,000 records; 1,220,106 input bytes | Token | Tree control |
| --- | ---: | ---: |
| Encode median wall / process CPU, seconds | 3.234 / 3.031 | 1.995 / 1.703 |
| Decode median wall / process CPU, seconds | 3.575 / 3.359 | 2.255 / 2.063 |
| Encode traced Python peak, bytes | 35,564,920 | 37,758,816 |
| Decode traced Python peak, bytes | 6,657,481 | 46,118,218 |
| Whole-worker process residency peak, bytes | 161,595,392 | 169,422,848 |
| Output XML bytes | 1,220,164 | 1,220,115 |

The token profile reduces traced decode memory and is slower on this workload;
these results establish no speed threshold. Extra namespace presentation accounts
for different XML sizes, with equal expanded values and native bytes. The generated
Python module is **315,084 bytes**, SHA-256
`442775ccfa87c50f01918d934f9241f9e2a6cb5326a4fa8a69809bca5702223c`.
Process residency includes setup, independent parsing, native controls and both
measurement phases; it is not an allocation limit or a per-operation delta.
Opaque provider allocation/preemption and broader platform costs remain open.

Raw samples, native hashes and source identities are retained in
`out/build/xml-schema-local/namespace-benchmark-large/verification.json`.
That report includes a two-record control; the separate `namespace-benchmark`
report additionally retains the 2,000-record diagnostic run. Reproduce with
`python -B qualification/xml_namespace_ordered_benchmark.py --compiler <compiler> --build out/build/<fresh-directory> --items 20000`.
