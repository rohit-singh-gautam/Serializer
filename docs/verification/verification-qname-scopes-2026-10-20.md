# QName namespace scopes — 20 October 2026

[Pending requirements](../pending.md) · [Migration](../../migration.md#compilerruntime-2160)

Compiler/runtime **2.16.0**, schema language **1.20.0**. This record tracks the
verified portions of `OPC-R01`; checking a scoped row does not close the broader
requirement. No publication or editor installation is part of this work.

## Implemented and verified scope

C++ and Python retain XML-only namespace state for each namespace-dependent
scalar child occurrence. Plain QName, QName-list, prefix-list and nested union
values use their original child scope through validation and output. Local
declarations, shadowed aliases and default namespace resets preserve expanded
identities. Native strings and JSON/binary encoding stay independent of this
state. C++ supports both direct and retained-tree paths; Python selects its
whole-document path for these graphs. Metadata follows field/occurrence positions;
applications must keep it associated with its corresponding values when editing
or reordering indistinguishable lexical strings.

Shared provider-independent UTF-8 NCName validation replaces the ASCII-only
mapping restriction. Complete Unicode names and prefixes remain byte-exact;
malformed, overlong, surrogate, truncated and out-of-range encodings reject.
Colon, invalid initial characters and code points outside XML's name ranges
remain invalid. Every generated backend accepts Unicode XML mappings.

Go, Java and C# direct codecs support plain QName, QName-list and prefix-list fields with
occurrence-specific scopes, optional/repeated values and typed lexical lists.
Go `SetXMLNamespaces`, Java `setXmlNamespaces` and C# `SetXmlNamespaces` validate
and copy native-authoring bindings, replacing captured scopes on that record.
Nested records bind independently. Only referenced prefixes are retained, so
new lexical prefixes require explicit bindings. Output
regenerates collision-free declarations and checks resulting lexical facets.
Namespace-dependent unions, defaults, fixed values and enumerations remain
explicit generation errors in these backends.

JavaScript/TypeScript already supported occurrence-specific namespace state.
Additional actual SDK tests qualify Unicode qualified attributes, QName locals
and prefixes, lists, child shadowing and native-codec independence. Independent
namespace resolution checks both output validity and exact expanded identities.
Their generated-output guard additionally checks the first selected union member
and expanded value against the actual emitted namespace scope. Inherited or
generated aliases that would reinterpret a fallback reject before publication.

Kotlin/Swift owning records retain XML-only scalar scopes, including explicit
empty default bindings when absent in the source. Plain QName/list/prefix-list
values, qualified attributes and optional/repeated scalar occurrences retain
their original meanings. Scope snapshots have cumulative admission even without
caller callbacks. Namespace-dependent unions/defaults/fixed/enumeration still
reject during generation; reference wrappers delegate to owning targets and
do not acquire separate namespace setters.

C/Rust retain namespace scopes in their existing XML document companions,
including synthetic owners for scalar occurrences. Native model structs and
ordinary wire values stay unchanged. Use the document decode and encode-with-
extensions entry points to retain namespace state. Plain owning entry points
reject these namespace graphs explicitly. Wrapped/nil scalars, mixed/all content,
revision/magic and map/variant compositions still have explicit capability gates.

## Executed checks

| Check | Result |
| --- | --- |
| C++ generated scalar SDK | Passed direct/tree four-way scope replay, optional/repeated/list/nested-union cases, independent expanded identities and tree-free byte-split input. |
| C++ rich XML SDK | 10 tests passed. |
| C++ lexical contract | 17 tests passed, including retained-scope allocation callback failure and empty QName-list union identity. |
| Shared Unicode runtime/compiler | 3 focused tests passed, including all eleven generator entrypoints. |
| Python rich XML SDK | 17 tests passed, including default absence, inherited/generated-prefix adversaries, cumulative scope admission and atomic unsupported QName-list-default preflight. |
| Python scalar SDK | 7 tests passed, including 89 independent union oracle decisions. |
| Python projection / extension / token SDKs | 5 / 4 / 12 tests passed. |
| Go / Java / C# namespace SDKs | 13 probes per backend passed with independent XSD and expanded-identity checks, imported Unicode qualified attributes and cumulative scope-retention exhaustion. |
| Existing Go / Java / C# scalar SDKs | 122 probes plus 6 atomic binding gates passed; Java `-Xlint:all -Werror` and C# warnings-as-errors compiled with no warnings. |
| JavaScript/TypeScript lexical SDK | 7 tests passed, including actual strict TypeScript compilation and generated-alias union identity rejection. |
| Kotlin / Swift namespace SDKs | 10 independent probes per actual SDK passed, including explicit absent-default bindings and cumulative scope admission. |
| Kotlin / Swift scalar baselines | 183 independent probes and 6 atomic binding gates passed per SDK. |
| Kotlin / Swift extension baselines | 10 malformed inputs, 3 binding fences and exact bounds passed per SDK. |
| C / Rust namespace SDKs | 13 namespace probes per SDK plus independent exact retained-scope accounting passed with real C/Rust compilation. |
| C / Rust extension baselines | 29 existing extension probes passed per SDK. |
| C++ independent scalar oracle | All 206 exact numeric, Unicode and prepared-pattern oracle probes passed. |

Windows uses the configured MSVC/CMake native build, Python/lxml independent
XML provider and actual language SDKs. The root `./make.ps1 test` regression
accounted for all 212 cases: 211 passed in the complete run; C# compression
completed its assertions but failed temporary-directory cleanup, then passed
when rerun in an isolated OS temporary directory. Both failed workspace scratch
directories and the successful isolated directory were removed; its verification
JSON is retained. That full run preceded the final emitted-namespace guards.
After those guards, all five native XML targets and all 206 independent C++
scalar oracle probes passed. The additional compiler/package/managed batch
requires a subsequent integrated regression; these results do not imply it.

Current logs are retained under ignored `out/build/pending-qname-2026-10-20/`
and `out/python-qname-check/`. The maintained Python SDK runners and
`test/go_xml_namespace_test.py` regenerate actual output using `--compiler` and
keep scratch beneath their explicit `--build` directory. The new Go runner and
JavaScript runner accept `--python-dependencies out/python-dependencies` for the
retained independent provider. No Linux or provider-free replay is yet claimed.

Remaining backend/graph combinations, QName union restrictions/defaults and
QName-list enumeration/fixed values remain separately tracked in `docs/pending.md`.
