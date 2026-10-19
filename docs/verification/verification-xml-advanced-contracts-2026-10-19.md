# Advanced XML shared-contract verification - 19 October 2026

Compiler/runtime **2.11.0**, schema language **1.20.0**, editor packages **2.0.5**.
This record covers S08-S36 from [the backlog](../pending.md), building on the
[S02-S07 checkpoint](verification-xml-shared-contracts-2026-10-19.md).
The [advanced contract guide](../serialization/xml_advanced_contracts.md)
documents the implemented APIs and their generated-backend boundaries.

Source identity is the uncommitted working tree based on `2c29917`
(`Implement S02-S07 shared XML contracts and incremental parsing`). No new
commit is created automatically.

## Scope and completion evidence

Shared tasks **S08-S36 are complete** and their rows have been removed from the
backlog. Their dependent backend, provider and broader qualification tasks remain
open under their existing IDs. This record closes only the documented shared scope.

| Tasks | Implemented contract and required evidence |
| --- | --- |
| S08-S11 | Compiled scalar restrictions, exact value-space bounds, Unicode lengths/enumeration, bounded portable regex programs and independent reference comparisons. |
| S12-S14 | Authoritative XSD text/wrappers, shared choice particles and unordered all models, ambiguity/cardinality/order fixtures. |
| S15-S17 | Strict identity XPath, key/unique tuples and scoped keyrefs, exact canonical values, invalid scope/type and resource-bound fixtures. |
| S18-S20 | Chameleon namespace instances, finite-cycle deduplication and anonymous nested complex-type association. |
| S21-S23 | Text/wrapper/facet and multi-namespace XSD export, pinned local inventory, independent validation, reimport and safe publication. |
| S24-S25 | Imported scalar/attribute/text/nested/include/collection declarations through generation, native compilation and round-trip execution. |
| S26 | Directional added/retired/reordered/model compatibility diagnostics. |
| S27-S30 | Concrete generic identity, inherited order, revision/magic root metadata and explicit digest/managed/borrowed projection contracts. |
| S31-S32 | Strict document profiles, generated C++ role/default/expected-type metadata and pre-decode vocabulary/type selection. |
| S33-S34 | Separate restricted HTML fragment/event/output adapter and independent HTML parser checks. |
| S35 | Shared eleven-language fixture inventory, hard missing-SDK failures and actual producer/consumer exchanges. |
| S36 | Release, README, usage, migration, skill and wire documentation; editor impact, link and artifact review. |

The focused native suites cover eight portable-pattern, thirteen scalar,
fourteen runtime-identity, six identity-binding, twenty-one XSD-contract,
ten model-contract, four runtime-profile, one profile-binding and five HTML
cases. Two further cases import XSD declarations, generate C++, compile it and
execute valid/invalid round trips. Existing schema/import/rich/Core/package
regressions bring the Linux selection to **143 native cases**. The particle
validator agrees with an independent prepared XSD provider on **374 documents**,
including repeated heterogeneous alternatives and paired grouped branches.

## Executed checks

Windows passed **190/190 CTests in 203.84 seconds**, including installed consumers,
package-version compatibility, generated codecs and the existing native regression
suite. The full native build used `./make.ps1 test`; final compiler/runtime repairs
and affected generated targets were refreshed through CMake before the complete
CTest rerun. The configuration enables XML, zlib, Zstandard, LZ4 and managed state,
using MSVC 19.51.36260, CMake 4.4.2 and native LibXml2 2.15.3.

Linux final-source verification passed **22/22 focused CTests in 1.48 seconds**.
The strict GCC build used GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, Python 3.14.4,
LibXml2 2.15.2 and zlib 1.3.1, with Zstandard, LZ4 and managed state disabled.
Both the Linux snapshot and the active worktree matched all **1,655 build-input
hashes**, with zero mismatches. This is a focused Linux check, not a full Linux
suite or qualification of every compression provider.

The XML/compression/managed-disabled Windows configuration passed **11/11
selected CTests in 80.92 seconds**, including the actual installed-package check.
XML-dependent event-reader and model cases report their explicit provider skips;
portable regex, identity validation, document profiles and HTML remain available.
The installed check compiles public headers separately and builds/runs standalone
C++, C and Python consumers. The advanced guide's two C++ examples also compiled,
linked and ran against the installed XML-disabled runtime under MSVC `/W4 /WX`.
All checked compilers report release **2.11.0**, language **1.20.0**.

The manifest-driven SDK run built all eleven consumers and passed **11 producer
checks plus all 121 producer/consumer exchanges**. SDK probes reported Java 26,
Kotlin 2.2.21 (JRE 17.0.18), .NET SDK 10.0.402 targeting .NET 8, Go 1.26.4,
Node 22.23.1, npm 11.18.0, Cargo 1.99.0 and WSL Swift 6.1.3. C and C++ use the
native Windows MSVC/LibXml2 build. Python uses 3.14.3 and pinned lxml 6.0.2.
Swift's selected platform is Linux through WSL, not native Windows.

The final rebuilt compiler passed **5/5 XSD bundle/tool CLI tests** with independent
lxml 6.0.2 / LibXml2 2.11.9 validation. These include cross-namespace valid/invalid
documents, exact facet export, schema-only unordered-content export/comparison,
reimport/code generation, pin recomputation, idempotent timestamps, edited-output
preservation and rejection before publication. Six HTML fragments agreed with
both Python `HTMLParser` traces and independent lxml HTML trees, with stable
canonical replay.

README, usage/CLI guides, migration notes, wire contract, XSD/package guides and
the canonical integration skill were updated together. Both extension READMEs,
changelogs and Marketplace payloads were reviewed. Schema syntax, supported
authored navigation and generated include destinations are unchanged, so no
editor files or packages changed; both remain version **2.0.5**. Documentation
link/UTF-8/fence checks passed for 13 Markdown files, 980 local destinations
and 251 anchors, with zero errors or warnings. Git whitespace checks passed.
The existing `.gitignore`
already covers every task-created build, cache and temporary artifact.

## Boundaries

Shared IR/reference validation does not close dependent generated-language
codecs. Grouped/repeated choice, unordered all, identity and advanced model
generation remain backend tasks unless explicitly qualified. Only the documented
fixed-XSD import/export and identity XPath subsets are supported. Nested local
identity owners and unsupported projection/types reject explicitly.

The HTML adapter is a restricted fragment subset with owning events, independent
of XML. It is not general HTML5 recovery, source editing or sanitization.
Logical allocation/work accounting does not claim allocator capacity or a hard
process peak-memory/provider-preemption guarantee.

NaN bound facets expose a provider discrepancy: LibXml2 accepts some finite
values that the existing generated XSD 1.0 checks reject. Independent Java
26/Xerces validation confirms all eight float/double single-NaN-bound schemas
and sixteen NaN/zero decisions used by the runtime regression. This does not
claim universal floating-conversion/provider parity. A separate five-case
Java/Xerces check confirms distinct `anyURI` and string identity families, while
string-derived tokens remain comparable to strings. Identity projection rejects
schema default/fixed augmentation because reference validation consumes explicit
normalized values.

## Artifacts

Task-specific configuration, final build/test logs, pinned dependencies and all
exchange output remain under ignored `out/build/s08-s36`. Useful native output
remains in the existing build caches; the matching Linux source/build snapshot is
`/var/tmp/serializer-shared-20261019-rThvfU`. Final evidence includes
`full-build.log`, `focused-final-build.log`, `full-final-tests.log`,
`off-final-tests.log`, `bundle-oracle.log`, `html-tree-oracle.log`,
`sdk-exchange.log`, `docs-cpp.log`, `doc-links.log`, the two Java oracle logs,
and Linux `qualification.json`, commands, test logs and source-hash audits.
Temporary probes, redundant SDK archives, scratch scripts and superseded
diagnostic output were removed. No files were staged and no commit was created.
