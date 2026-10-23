# Implemented requirements and qualification catalog

[Feature list](feature_status.md) · [Pending work](../pending.md) ·
[Ownership decisions](application_responsibilities.md#disposed-requirements)

This feature catalog contains **821 implemented requirement records**, including
the 816 records moved out of the pending backlog on 2026-10-23. Each stable ID retains its capability scope,
dependencies and original verification links. Completion applies to the stated
backend, profile and dated checks; it does not close a broader parent requirement
or establish qualification on every platform. New records link their executed
implementation or qualification evidence. A completed design or qualification gate
is cataloged as such; it does not by itself claim a runtime feature.

Only unfinished requirements belong in [the pending list](../pending.md).
The three closed application/maintainer ownership decisions remain in their
separate reference; they are not implemented product features. Historical work-unit
estimates are omitted here because this is a feature/evidence catalog.

## C++ build minimum

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cpp01"></a>CPP01 | Compiler/runtime 2.30.1 requires C++23 across compiler, runtime, managed/generated consumers, provider targets and installed exports. Public language/library admission, explicit older-mode rejection, synchronized usage/migration guidance and Windows MSVC/clang-cl plus Linux GCC/Clang installed-consumer qualification preserve APIs, schemas and wire identities. Explicit GCC/Clang C++26 configuration and installed consumers retain the newer mode; [executed scope and limits](../verification/verification-cpp23-2026-10-23.md). | — |

## OPC and 3MF requirements

The 2.28.0 update completes `OPC-R04`'s explicit ordered owning storage across all
eleven outputs, including fixed-XSD grouped/repeated sequence/choice binding,
exact fixed cardinality, grammar-preserving export and bounded compatibility.
See the [grouped-XSD acceptance checks](../verification/verification-ordered-xsd-2026-10-22.md).
The same update completes `OPC-R01` namespace identities and `OPC-R03`
scalar/list/union restrictions in all eleven outputs; see the
[scalar identity and actual SDK record](../verification/verification-scalar-identities-2026-10-22.md)
and [Unicode pattern checks](../verification/verification-unicode-xml-patterns-2026-10-22.md).
`OPC-R06` now composes XML/JSON/BINARY_NONE profiles, typed references, runtime
selection and repeated-role relocation in all eleven outputs, including native
XML companions; see the [package pipeline checks](../verification/verification-package-pipelines-2026-10-22.md).
`OPC-R02` completes namespace-aware wildcard admission and pinned strict/lax
declaration validation across all eleven outputs, including required capabilities,
shared resource policies, compositor combinations and native package companions.
All **20 rows in this section are complete**. The [wildcard qualification](../verification/verification-xml-wildcards-2026-10-22.md)
records the SDK matrices and all 299 verified repository CTest cases, including
the targeted replays after integration fixes.

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="opc-r01"></a>OPC-R01 | All eleven outputs preserve qualified attributes, Unicode names, QName/list/prefix-list occurrence scopes, equivalent prefixes, local shadowing and default-namespace absence. Native C/Rust document companions cover wrapped/nil, mixed/ordered/all, map/variant and lifecycle graphs without native model/wire changes. Namespace constraints/defaults use schema-owned expanded identities; [scalar checks](../verification/verification-scalar-identities-2026-10-22.md), [native graph checks](../verification/verification-native-namespace-graphs-2026-10-22.md). | — |
| <a id="opc-r01a"></a>OPC-R01a | C++/Python scalar-child namespace scopes survive local declarations, shadowing, default namespace resets, optional/repeated occurrences and nested QName/QName-list unions. Native strings and wire codecs retain their contract; [2.16 checks](../verification/verification-qname-scopes-2026-10-20.md). | — |
| <a id="opc-r01b"></a>OPC-R01b | Shared UTF-8 XML NCName validation accepts Unicode mapping names and QName prefixes, with invalid encoding/name rejection; all eleven generators accept Unicode mappings. JavaScript/TypeScript independent XML checks verify qualified Unicode attributes, QName lists, prefix lists and child-local scopes; [2.16 checks](../verification/verification-qname-scopes-2026-10-20.md). | — |
| <a id="opc-r01c"></a>OPC-R01c | Go/Java/C# retain QName/list/prefix-list occurrence scopes, qualified Unicode attributes, copied native-authoring bindings and cumulative retention bounds. Schema-scoped outer union enumeration/default/fixed and restricted atomic/union list items are implemented; [2.28 actual SDK evidence](../verification/verification-scalar-identities-2026-10-22.md). | — |
| <a id="opc-r01d"></a>OPC-R01d | Kotlin/Swift retain local/shadowed QName/list/prefix-list declarations and explicit default absence through optional/repeated values and qualified attributes. Direct and union enumeration/default/fixed, restricted list items and foreign schema-default isolation pass actual SDK/provider controls; [2.28 evidence](../verification/verification-scalar-identities-2026-10-22.md). | — |
| <a id="opc-r01e"></a>OPC-R01e | C/Rust XML document companions preserve occurrence namespaces across wrapped/nil records, mixed/ordered/all groups, lifecycle metadata, maps and element/xsi:type variants. Complete scalar/list/union restrictions and schema-owned defaults/fixed preserve native model structs and protocol bytes; [namespace graphs](../verification/verification-native-namespace-graphs-2026-10-22.md), [native constraints](../verification/verification-native-qname-constraints-2026-10-22.md). | — |
| <a id="opc-r02"></a>OPC-R02 | All eleven outputs enforce namespace-aware reject/skip/preserve slots, exact XSD predicates/positions/counts, required capabilities and pinned strict/lax declaration validation. Foreign fallback stays closed to unknown core/local names; private validators share the caller's resource and identity policies. Compositor combinations, large fallback owners, empty inventories, edited retained content and legacy C++ Core tree mappings pass actual SDK/provider checks. [Wildcard matrix and integrated verification](../verification/verification-xml-wildcards-2026-10-22.md). | — |
| <a id="opc-r03"></a>OPC-R03 | All eleven outputs compile exact primitive/list/union facets, first-selected outer identities, schema-scoped QName enumeration/default/fixed and restricted atomic/union list-item contracts from the same XSD inventory. Export retains declaration scopes and restrictions. Bounded Unicode category/block/name escapes and recursive class subtraction run through prepared portable programs; numeric writers validate canonical spelling before publication. Native-width floating overflow remains an explicit representability boundary; external prefix-list defaults/export require a separate capability contract. [Scalar SDK/provider checks](../verification/verification-scalar-identities-2026-10-22.md), [Unicode pattern matrix](../verification/verification-unicode-xml-patterns-2026-10-22.md). | — |
| <a id="opc-r03a"></a>OPC-R03a | First-successful union selection and recursive primitive/list item identities now apply in all eleven outputs, including outer enumeration/default/fixed and schema-scoped QName/list alternatives; [2.28 executed scope](../verification/verification-scalar-identities-2026-10-22.md). | — |
| <a id="opc-r04"></a>OPC-R04 | All eleven owning outputs bind explicit [element-only ordered arrays](../serialization/ordered_xml.md) to multiple typed choices and nested/repeated XSD sequence/choice groups. Readers and writers enforce complete grammar and exact fixed storage without hidden order metadata or native format changes. XSD export retains group occurrences; bounded reader-direction comparison rejects narrowing before publication. Actual SDKs pass 340 input decisions, edited-output rejection and native protocol parity; C++ adds 511 independently classified traces. [2.28 grouped-XSD evidence](../verification/verification-ordered-xsd-2026-10-22.md). | — |
| <a id="opc-r05"></a>OPC-R05 | Selected published Core 1.4/XML bundle qualified against 175 official model parts with unchanged independent XSD oracles: 156 edited round trips per C++/Python consumer, six correct schema rejections, four bounded-resource rejections and nine explicit 16 MiB profile exclusions. Nine other backend selectors reject the unsupported rich graph precisely. Package/geometry conformance and three upstream positive/XSD disagreements remain distinct; [complete inventory](../verification/verification-core-conformance-2026-10-21.md). | — |
| <a id="opc-r06"></a>OPC-R06 | All eleven outputs compose immutable model/MIME/container identities with fixed/default/runtime XML/JSON/BINARY_NONE and stored/deflate profiles, typed references, exact internal/external target modes, root vocabulary/type checks and repeated/dynamic role rename provenance. C/Rust retain per-part XML namespace/foreign companions without native model/wire changes. Selected callbacks share the operation; rejected loads/Saves preserve prior publication. [Actual SDK and independent archive evidence](../verification/verification-package-pipelines-2026-10-22.md). | — |
| <a id="opc-r06a"></a>OPC-R06a | Generated fixed singleton roles support preserved source/internal-target canonical rebinding across eleven outputs, with opt-in C/Rust writers and Go loaders; original relative scopes, selectors, external URI strings and opaque bytes retain their values. Removed moved roles, collisions and dangling references reject. The later OPC-R06 completion adds repeated/dynamic explicit renames and selected profiles; [2.28 pipeline checks](../verification/verification-package-pipelines-2026-10-22.md), [2.19 scope](../verification/verification-package-relocation-2026-10-21.md), [original 2.15 scope](../verification/verification-pending-xml-2026-10-20.md). | — |
| <a id="opc-r07"></a>OPC-R07 | Implement ZIP/OPC metadata, reader/writer, shared limits, preservation and full-Save APIs for every generated language, with independent producer/consumer tests; [2.14 checkpoint](../verification/verification-backlog-2026-10-20.md), [eleven-output completion and limits](../verification/verification-managed-reference-faults-2026-10-21.md#package-parent-reconciliation). | — |
| <a id="opc-r08"></a>OPC-R08 | Mandatory fifth-edition URI/content-type/Relationships and Annex B complete-rewrite contract qualified in all eleven actual SDKs: 196 shared physical-record/MCE/URI/IRI/encoding controls per backend, independent Windows/Linux native readers and metadata replay. Escaped/equivalent names, relative targets, collisions, physical metadata, string whitespace, UTF-16 and scoped Relationships compatibility retain their declared contracts. Optional Unicode part naming/interleaving/growth hints remain explicitly unsupported; [metadata qualification](../verification/verification-opc-metadata-2026-10-21.md), [Annex B and replay](../verification/verification-opc-annex-b-2026-10-21.md). | — |
| <a id="opc-r09"></a>OPC-R09 | Selected 75,000-tag owning XML workloads measured in all eleven Windows SDKs with actual providers, CPU/process peaks and encoded/generated sizes: 216 isolated profiles/1,080 timed batches and 132 large cancellation/deadline/recovery controls. Actual Linux C++ adds 54 profiles/270 calls and 12 controls. Public C#/Kotlin/Swift policies and bounded deferred Swift failure are implemented; final source bridges retain earlier timing provenance. Broader graphs/platform/provider qualification stays separate; [executed scope](../verification/verification-large-xml-2026-10-21.md). | — |
| <a id="opc-r10"></a>OPC-R10 | All eleven actual Windows outputs qualify namespace-sensitive scoped translators, borrowed native getters/storage, exact/one-tight depth and byte bounds, failure publication and fresh recovery, alongside 25,000-value stress controls. C++ borrows caller scope metadata; mobile outputs retain assigned source occurrence scopes; C/Rust provide encode-only borrowed companions. Durable-address XML has a separate explicit rejection policy, verified atomically in every output. [Graph/resource and lifetime evidence](../verification/verification-readonly-xml-graphs-2026-10-22.md), [large stress controls](../verification/verification-readonly-xml-stress-2026-10-21.md). | — |
| <a id="opc-r11"></a>OPC-R11 | One cumulative context spans the supported generated owning operation's XML, DEFLATE, ZIP, scoped identities, preservation and publication in all eleven outputs. Three 25,000-entry owners pass exact/one-tight reference/work/allocation admission, real cancellation/deadline boundaries, old-file preservation and fresh recovery; actual prepared-provider completion has separate checks. Public cross-call budget arguments differ by backend; [full executed scope](../verification/verification-large-package-operations-2026-10-21.md). | — |
| <a id="opc-r12"></a>OPC-R12 | Fixed-XSD association/import/export and checked local bundles pass the rich text/wrapper/enum, namespace/particle and ambiguity/publication inventory. Authored names, IDs, defaults and behavior retain their contracts through all eleven ordinary Windows/Linux export/rebind outputs; imports remain reviewed. [2.20.0 scope and evidence](../verification/verification-xsd-tooling-2026-10-21.md). | — |
| <a id="opc-r13"></a>OPC-R13 | One-source XSD/inline exception workflows pass staged new-field/rename imports, protected ambiguity failures and authored native-model XML rename/reorder in all eleven outputs on Windows/Linux. Native names, IDs, defaults, behavior and literal positional/integer/string bytes retain their contracts; imported evolution remains reviewed. [2.19.2 evidence](../verification/verification-xsd-evolution-2026-10-21.md). | — |


`OPC-R01` and `OPC-R03` are complete within their declared native-width and
bounded portable execution contracts. The eleven-output SDK matrices preserve
expanded namespace identities and native protocol bytes; QName defaults remain
distinct from explicitly authored no-namespace values. The `prefix_list` adapter
is an external capability vocabulary: its default/fixed semantics and XSD export
need that separate contract, since an ordinary XSD string default does not assign
namespace value semantics. This does not gate QName or QName-list constraints.
Independent provider presentation disagreements and native floating-width bounds
are recorded separately from implemented scalar validation.

## Managed references and migration

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="mr01"></a>MR01 | Broaden native identity/reference lifecycle, corruption, resource and fault tests across allocation, complete-candidate validation, deletion, Undo/Redo, persistence and collaboration; [eight-profile Windows/Linux checks](../verification/verification-managed-reference-faults-2026-10-21.md). | — |
| <a id="mr01a"></a>MR01a | Eight existing C++ identity/storage profiles qualify cyclic deletion, complete-candidate edge cleanup, historical pins, Undo/Redo, persisted history, candidate allocation failure and post-reservation snapshot-admission rollback with nonreuse/recovery; [2.16 batch evidence](../verification/verification-pending-batch-2026-10-20.md). Broader collaboration/resource faults are qualified by MR01. | — |
| <a id="mr02"></a>MR02 | Broaden independent-import ownership/external-mapping failures, editable child moves/retirement, preview resource failures and journal/acceptance faults; preserve IDs and atomic publication; [eight-profile Windows/Linux checks](../verification/verification-managed-reference-faults-2026-10-21.md). | — |
| <a id="mr02a"></a>MR02a | Eight existing C++ profiles reject six external-map failures, five ownership-attachment failures and corrupt zero/duplicate source IDs without publishing state/history, then recover successfully. Five added tests per profile are part of 184 passing GTests; [2.16 batch evidence](../verification/verification-pending-batch-2026-10-20.md). Wider preview/journal/acceptance gates are qualified by MR02. | — |
| <a id="mr03a"></a>MR03a | Compiler/runtime 2.31.0 and language 1.23 bind exact generic self targets in copied direct C++ managed token fields and supported wrappers to each named instantiation. Older declaration provenance, distinct named class IDs, shared-record rejection and byte-identical older output remain compatible. Eight Windows/Linux runtime profiles, four installed toolchains and current editor packages pass; [executed qualification](../verification/verification-named-managed-references-2026-10-24.md). Other-output token codecs and broader recursive generic selection remain open under MR03. | MR04, MR05 |
| <a id="mr04"></a>MR04 | Typed C++ managed_ref, get_id, stable class_id and registry reference/resolution; document/type scope and complete-candidate validation (2.7 Windows direct/separated fixed/compact uint32/uint64 profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | Existing C++ managed runtime |
| <a id="mr05"></a>MR05 | Initialize required/self/cyclic managed references after native identity assignment and before first validation/publication/history snapshot (2.7 C++ typed-reference profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| <a id="mr06"></a>MR06 | Native two-pass independent import; remap declared tokens, apply explicit external mappings, validate ownership/references and attach transactionally; ordinary integer payloads retain their values (2.7 C++ reference/import profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| <a id="mr07"></a>MR07 | C++ apply_editable retains entity identity and reconciles owned children, with zero IDs for genuinely new children; separately, active-transaction registry.reserve and registry.adopt_reserved admit unused identities reserved by that transaction through checked attachment (2.7 generated editable/reference profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | MR04, MR05 |
| <a id="mr08"></a>MR08 | Owner-thread previews reserve IDs through the source allocator before exposure; canceled IDs stay burned and exact-base/exact-ID acceptance preserves native Undo/Redo and recovery (2.7 four C++ registry profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | MR07 |
| <a id="mr09"></a>MR09 | Identical collaborative document/object IDs with authority leases and reserve_ids; durable reservations, nonreuse, exact acceptance and forged/exhausted-range rejection (2.7 native strict-collaboration profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| <a id="mig01"></a>MIG01 | Qualify ID conversion against genuinely rebuilt 2.6.0 compiler/runtime artifacts; all three raw binary formats and supported managed history/identity/allocation profiles convert exactly (Windows direct uint32/separated uint64, 92 conversions; [evidence](../verification/verification-managed-legacy-migration-2026-10-20.md)). | — |
| <a id="mig04"></a>MIG04 | Explicit same-schema fixed/compact-varint ID conversion helper/CLI for supported managed saves and raw binary formats; preserve exact identities, allocation marks, history, leases and native layout (2.7 direct uint32/separated uint64 reference profiles; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | Existing same-schema C++ codecs |

## Managed state and storage

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cache01"></a>CACHE01 | Owning C++ cache declarations, optional priority defaulting to zero, bounded provider/rebuild services, exact-source guarded publication and shared layout/migration/close preserving caches and required state. Four generated profiles, native Windows/Linux storage, recovery/failure boundaries and matching editor packages have [executed qualification](../verification/verification-cache-2026-10-23.md); WSL mounted-storage replacement failures and unsupported graphs/providers remain explicit limits. | — |

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="mg02"></a>MG02 | Version-one native C++ feature activation/exclusions, mandatory identity/authorization, forty actual engine combinations and all ordered history-policy conversions across direct/separated identities; [implemented contract and Windows/Linux checks](../verification/verification-managed-capabilities-2026-10-21.md). Proposed schema selectors remain distinct from this native API. | — |
| <a id="mg03"></a>MG03 | Inherited entity/field authorization, editable/read-only exceptions and separate content/structure/policy checks are implemented, with native edit/import/history/publication enforcement and the same candidate helper at authority acceptance. Authentication, policy storage/administration and principal binding are host responsibilities, will not fix in Serializer. [Contract and ownership](application_responsibilities.md#security-and-authorization), [executed checks](../verification/verification-managed-authorization-2026-10-21.md). | — |
| <a id="mg04"></a>MG04 | Native C++ retained-revision three-way merging, typed conflicts, two-parent ancestry, opt-in authority negotiation and exact-target/cursor requests use complete-candidate policy/locks and accepted field/dependency deltas. Selective Undo/Redo, exact retries and both journal fault/recovery layouts pass in all four Windows/Linux storage/ID profiles; [contract](../managed/branch_merging.md), [executed evidence and exclusions](../verification/verification-collaboration-merging-2026-10-21.md). Client-authored branch graph import and automatic stale-edit merging are distinct contracts. | — |
| <a id="mg05"></a>MG05 | Native session records exercise uint32/uint64/string keys, failed submission/polling, lost acknowledgement, checkpoint reload and epoch changes; 384 presence/cache cycles preserve strict identity and authority ownership. [Windows/Linux qualification](../verification/verification-managed-capabilities-2026-10-21.md); network deployment/authentication and presence UI are application responsibilities. | — |
| <a id="mg09a"></a>MG09a | Existing C++ managed-variant field merging and selective collaboration Undo/Redo are qualified in all four Windows/Linux direct/separate uint32/uint64 profiles: child-field conflicts/retries, same-tag identity replacement, appended/sidecar full/delta recovery, ancestry and later-author protection. Each host passes 88 complete merge cases; [executed scope](../verification/verification-managed-variants-2026-10-24.md). Virtual-edit dispatch and portable ownership/error contracts remain open under MG09. | MG04 |

## Cached-origin document bridge

Accepted on 2026-10-21 and implemented in 2.26.0. These generic C++ document-bridge
additions form the completed priority batch below. Each leaf and its acceptance
check is capped at five hours;
split a leaf before exceeding that cap. Application storage policy, external
publication/authentication and reconciliation remain host responsibilities under
MG06. An application may attach native differences to a durable original or
first publish its own complete recovery main. Shared immutable-base resolution
and cache-dependency inventory are optional future work, not prerequisites.

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="d25"></a>D25 | **C++**: 2.26 external-document sidecars retain bounded opaque origin metadata outside authored content through edits/history/Save/recovery. Corruption, wrong generation bindings and exceeded bounds reject before admission; [API](../managed/document_origin.md), [executed checks](../verification/verification-document-origin-2026-10-22.md). Ordinary native containers/custom sinks do not implicitly acquire this capability. | D24 |
| <a id="d26"></a>D26 | **C++**: Durable external Save preparation pins the accepted checkpoint, allocator/history/paired session state and expected old activation. A bounded opaque token and validated staged bytes survive restart as an explicit unresolved decision; durable mutations and client transport cannot race it. [Qualification](../verification/verification-document-origin-2026-10-22.md). | D25 |
| <a id="d27"></a>D27 | **C++**: Confirmation checks the exact token, old activation and independently supplied publication evidence; wrong/stale/tampered inputs reject, exact retries do not reactivate or mark later edits saved. Native selection precedes dictionary replacement/retirement. Partial host-copy failure, lost acknowledgements, unchanged authored bytes and abrupt native publication controls pass; [evidence](../verification/verification-document-origin-2026-10-22.md). Host effects/reconciliation remain MG06. | D26 |
| <a id="d28"></a>D28 | **C++**: Maintained application-controlled owned-copy/durable-original example; Windows/NTFS and Linux/ext4 each pass 129 native tests plus one inapplicable-mode skip and 480 process-recovery checks. Deleted source/capture, IDs/watermarks/Undo/session state and physical main/manifest/companion sizes are exercised. Final Windows regression passes 281/281; [scope, versions and limits](../verification/verification-document-origin-2026-10-22.md). WSL mounted-filesystem failures remain QA01. | D25, D26, D27 |

## Compiler and backend proposals

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="sc01"></a>SC01 | Define proposed signed/floating payload contracts, rounding, overflow, NaN/infinity, zero and scaled arithmetic before keywords or wire values; current syntax remains unavailable (76 vectors, exhaustive16-bit and independent conversion checks; [contract](../design/compact_scalar_contract.md), [evidence](../verification/verification-compact-scalar-contract-2026-10-20.md)). | — |
| <a id="sc02a"></a>SC02a | Diagnose out-of-range canonical decimal/hex integer literals in explicit language 1.14+ defaults and retain original member paths/positions through includes and generic binding. Preserve native expressions and legacy defaults; broader validation and bit fields remain pending; [2.15 scope](../verification/verification-pending-xml-2026-10-20.md). | — |
| <a id="sc02b"></a>SC02b | Explicit language 1.14+ canonical decimal float/double overflow and declared-type-qualified unknown enum defaults diagnose the original member, including included/generic source. Atomic compiler rejection and generated Python four-protocol checks pass; native expressions, numeric Boolean defaults, underflow and legacy spelling remain compatible; [2.16 batch evidence](../verification/verification-pending-batch-2026-10-20.md). Bit fields and broader validation remain pending. | — |
| <a id="sc02c"></a>SC02c | Compiler 2.31.1 recognizes the existing canonical integer-default grammar in linear work and constant stack space, replacing the Linux recursive-regex crash. Long in-range hex, native/legacy spelling, original member positions and atomic output publication retain their contracts. Windows/Linux core and CLI controls, byte-identical generation and installed GCC consumers pass; [executed scope](../verification/verification-integer-defaults-2026-10-24.md). Other portable backend recognizers and broader SC02 validation remain separate. | SC02a |
| <a id="sc02d"></a>SC02d | Compiler 2.31.2 uses iterative portable literal/identifier recognition, preserving grammar, range/error policy, typed aggregate binding and generated bytes. Windows/Linux core/constructor/atomic CLI and 512 KiB-stack controls pass; all eleven Windows SDKs, selected larger SDK profiles and 41 baseline/cross-host file comparisons qualify the stated boundary. Target SDK token limits and broader SC02 validation remain separate; [executed evidence](../verification/verification-portable-literals-2026-10-24.md). | SC02c |
| <a id="be04"></a>BE04 | Compiler-only installed CMake discovery avoids unused runtime-provider lookup; preserve runtime exports, relocation, component selection and cross-toolchain host lookup (2.14 Windows/Linux neutral and multiarch install profiles; [evidence](../verification/verification-backlog-2026-10-20.md)). | — |
| <a id="opc-opt01"></a>OPC-OPT01 | Reusable C++ fifth-edition core-property model, standalone XML and OPC writer/loader adapters; applications supply all fifteen standard values, including localized mixed keywords and normative W3CDTF dates. [Usage](../serialization/core_properties.md) and [independent verification](../verification/verification-core-properties-2026-10-21.md). | — |
| <a id="opc-opt03"></a>OPC-OPT03 | Public C++ Pack-IRI composition, parsing, relative resolution and policy-directed outer-package equivalence; optional non-ASCII part naming/interleaving/growth hints have explicit unsupported policy. [Independent Windows/Linux checks](../verification/verification-pack-iri-2026-10-21.md) and [usage](../serialization/pack_iri.md); mandatory Annex B work remains OPC-R08. | — |

## Qualification and distribution

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="rel01"></a>REL01 | Inspect published VSIX archive/hash/identity contents and independently build/install the upstream package with an isolated installed consumer (published 2.0.0 archives and 2.0.1 x64-windows, nine consumer checks; [evidence](../verification/verification-distribution-2026-10-20.md)). | — |

## XML and package backlog

<a id="implemented-xml-and-package-capability-tasks"></a>

### Implemented XML and package capabilities

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="xml-c01"></a>XML-C01 | Two-stage XSD declaration import: emit an editable native schema, then compile that schema; separately bind existing declarations with using schema; preserve explicit member IDs, authored behavior and protected existing source (2.7 eleven-SDK automatic/manual consumers; [evidence](../verification/verification-opc-xsd-2026-10-19.md)). | S12, S24, S25 |
| <a id="xml-c02"></a>XML-C02 | Generated C++/Python owning package facades and selected XML/opaque roles; standard content-type/relationship metadata, integrity, preservation and full-Save round trips (2.7–2.14 selected C++/Python package profiles; [evidence](../verification/verification-package-reference-2026-10-20.md)). | S31, S32 |
| <a id="xml-c03"></a>XML-C03 | Pinned official Core 1.4 plus XML vocabulary bundle import/binding and authored-model consumers; unsupported constructs reject (2.8 selected C++/Python Core consumers; [evidence](../verification/verification-opc-backends-2026-10-19.md)). | XML-C01 |
| <a id="xml-c04"></a>XML-C04 | Readonly value-reference and scoped-projection XML output preserves source lifetime and avoids duplicate owning payloads; durable address XML has an explicit rejection boundary (selected eleven-output reference contracts; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30 |

<a id="xml-and-package-requirement-tasks"></a>

### XML and package capability records

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="xml-b05"></a>XML-B05 | Public same-ledger selected-codec composition and the large owning XML/package operation qualify in all eleven actual SDKs, with copied standalone C#/Kotlin/Swift policies, provider completion fences and per-operation publication/recovery. The three-owner input contains 150,003 model elements and 225,000 identity tuples; [combined qualification](../verification/verification-large-package-operations-2026-10-21.md), [prepared-provider/public-policy checks](../verification/verification-operation-publication-2026-10-21.md). Unsupported rich graphs and opaque provider CPU/process limits remain separate. | — |
| <a id="xml-b06"></a>XML-B06 | Repeatable ordinary-native throughput and memory measurements pass across eleven Windows SDKs with XML-disabled controls: 132 positional-binary operation profiles, 660 samples, exact cross-language bytes and separate residency/traced-allocation categories. [Executed baseline and limits](../verification/verification-native-codec-baseline-2026-10-21.md). | — |
| <a id="xml-b11"></a>XML-B11 | Expanded XSD tooling passes 300 decisions per host: exact bundle/path/identity bounds, directional comparisons, eight rich export families, six invalid enum-binding profiles and protected publication. All eleven ordinary exported/rebound SDK outputs pass Windows/Linux interoperability; rich runtime graphs retain their recorded scope. [2.20.0 evidence](../verification/verification-xsd-tooling-2026-10-21.md). | — |
| <a id="xml-b12"></a>XML-B12 | Inline association and staged declaration import preserve authored behavior, IDs, defaults and native ordering through reviewed schema evolution. This duplicates the completed OPC-R13 interface gate; broader XSD graph/import combinations stay in XML-B02/B10. [Executed eleven-output authoring/evolution matrix](../verification/verification-xsd-evolution-2026-10-21.md). | — |
| <a id="xml-b13"></a>XML-B13 | Actual prepared providers, bounded input, concurrent reuse, caller validator interfaces and prohibited remote schema/entity loading qualified in all eleven outputs. Physical namespace admissions pass 44,892 independent tree/token boundary decisions; fresh C++ provider 4/4 and late-completion regressions pass. Broader combined-operation gates remain separate; [capture provenance and scope](../verification/verification-runtime-boundaries-2026-10-21.md). | — |
| <a id="xml-b16"></a>XML-B16 | Compression parity: Complete whole-object source/sink wrappers and identical framing/error/limit contracts across every output; qualify optional Zstandard/LZ4 adapters; [qualification](../verification/verification-compression-parity-2026-10-20.md). | — |
| <a id="xml-b17"></a>XML-B17 | Package parity: Complete ZIP/OPC source/sink, metadata, shared budgets and full-Save guarantees across every output; [eleven-output completion and limits](../verification/verification-managed-reference-faults-2026-10-21.md#package-parent-reconciliation). | — |
| <a id="xml-b20"></a>XML-B20 | Typed package loading: Complete generated transactional profiles and automatic codec/reference budgets in every backend; caller-owned callbacks compose selected codecs with the same shared context; [2.18 SDK verification and graph limits](../verification/verification-native-xml-operations-2026-10-21.md). | — |
| <a id="xml-b21"></a>XML-B21 | All eleven actual SDKs qualify the selected large OPC/DEFLATE/scoped-reference/preservation/publication chain, including cumulative quotas, cancellation/real deadlines, previous-file preservation and recovery. Java/C#/Go readonly late-success prepared-provider cancellation has separate actual-provider evidence; Python returning Save hooks now fence both pre-publication rollback and post-publication uncertainty. [Large operation results](../verification/verification-large-package-operations-2026-10-21.md), [completion regressions](../verification/verification-operation-publication-2026-10-21.md); opaque provider preemption/process ceilings remain host deployment limits. | — |
| <a id="xml-b22"></a>XML-B22 | Preservation: XML/package policy and collision qualification covers supported graphs in every backend, including simultaneous fixed-role rebinding, removed-role tombstones, exact selector/external values and failure publication; broader graph gates, signatures and lexical preservation remain separate. [Executed matrix and historical XML-policy evidence](../verification/verification-package-relocation-2026-10-21.md). | — |
| <a id="xml-b23"></a>XML-B23 | Independent canonical ordering/metadata policies pass across all eleven outputs on Windows/Linux: 2,112 actual archives, six input permutations, two fresh processes and both ZIP64 selections. Provider-qualified compressed equality preserves existing wire policies; Python plain ZIP is a private diagnostic. [2.19.1 evidence and scope](../verification/verification-package-reproducibility-2026-10-21.md). | — |

<a id="work-units-capped-at-five-hours"></a>

## Language and runtime qualification

<a id="shared-compiler-and-contract-tasks"></a>

### Shared compiler and contract capabilities

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="s01"></a>S01 | Define token/event ownership and limits; golden event traces and malformed boundaries pass (2.9 provider-independent shared event contract; [evidence](../verification/verification-xml-events-2026-10-19.md)). | Existing XML mapping |
| <a id="s02"></a>S02 | Define incremental UTF-8/name/namespace handling; split-byte, scope and DTD rejection fixtures pass (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s03"></a>S03 | Define explicit nil/presence IR and configuration; ambiguous absent/empty/nil mappings reject (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s04"></a>S04 | Define ordered mixed-content IR; text/child/text order fixtures pass (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s05"></a>S05 | Define map entry/key/value mapping; duplicate-key and entry-presence rules have fixtures (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s06"></a>S06 | Define variant/type tags; ambiguous or conflicting tags reject during generation (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s07"></a>S07 | Define XML reject/skip/preserve extension slots; namespace context and required-capability fixtures pass (2.10 defined shared-contract subset; [evidence](../verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| <a id="s08"></a>S08 | Compile numeric inclusive/exclusive facets into IR; exact boundary tests pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| <a id="s09"></a>S09 | Compile Unicode length/enumeration facets; empty/code-point/enumeration tests pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| <a id="s10"></a>S10 | Select an explicit portable XSD-regex subset; accepted/rejected corpus and unsupported diagnostics pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S09 |
| <a id="s11"></a>S11 | Compile only that regex subset to portable checks; independent reference matches agree (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S10 |
| <a id="s12"></a>S12 | Derive simpleContent/text and wrappers from XSD; authoritative binding/conflict tests pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| <a id="s13"></a>S13 | Compile choice models and ambiguity diagnostics; alternative/occurrence fixtures pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s14"></a>S14 | Compile all models; permutation and duplicate fixtures pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s15"></a>S15 | Define supported identity selector/field XPath; unsupported expressions reject (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| <a id="s16"></a>S16 | Compile key/unique scopes and tuples; duplicate/incomplete-key fixtures pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S15 |
| <a id="s17"></a>S17 | Resolve keyref targets/types; missing and wrong-scope references reject (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S16 |
| <a id="s18"></a>S18 | Implement chameleon namespace adoption; one file adopted into two namespaces stays isolated (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing bundle loader |
| <a id="s19"></a>S19 | Handle finite cyclic bundle graphs; legal cycles deduplicate and budgets still bound growth (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S18 |
| <a id="s20"></a>S20 | Bind anonymous nested complex types; ambiguous associations reject (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| <a id="s21"></a>S21 | Export text/wrappers; independent validation and reimport agree (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S12 |
| <a id="s22"></a>S22 | Export enumerations/facets; independent valid/invalid document checks agree (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S08, S09, S10, S11 |
| <a id="s23"></a>S23 | Export multiple namespaces as a pinned bundle; reimport preserves names and dependencies (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing export |
| <a id="s24"></a>S24 | Import a scalar/attribute XSD subset into Serializer declarations; generated schemas compile and round-trip (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S08, S09, S10, S11, S12 |
| <a id="s25"></a>S25 | Extend import to named nested records/collections; nested/include fixtures compile (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S24 |
| <a id="s26"></a>S26 | Extend compatibility diagnostics to added/retired fields/models; both comparison directions have tests (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing compare tool |
| <a id="s27"></a>S27 | Define concrete generic XML bindings; distinct instances and ambiguous names have tests (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s28"></a>S28 | Define inherited field order/flattening; collisions and ordering have tests (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s29"></a>S29 | Define revision/magic XML contracts; old/current/invalid layouts have fixtures (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s30"></a>S30 | Define digest and managed/view data projections; ownership/lifetime and unsupported states are explicit (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s31"></a>S31 | Define generated compression/package profile IR/configuration; defaults and runtime overrides validate (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing package APIs |
| <a id="s32"></a>S32 | Generate vocabulary/expected-type dispatch metadata; wrong roots/types select no decoder (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S31, S06 |
| <a id="s33"></a>S33 | Define separate HTML syntax/provider boundaries; HTML and XML fixtures remain distinct (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| <a id="s34"></a>S34 | Implement the approved minimal HTML event/output subset; independent HTML checks pass (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | S33 |
| <a id="s35"></a>S35 | Maintain a shared producer/consumer fixture manifest; all eleven languages are represented and missing SDKs fail (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing SDK runners |
| <a id="s36"></a>S36 | Apply release, README, usage, migration and skill updates; review editor impact and verify affected packages when touched (2.11 defined shared-contract subset; [evidence](../verification/verification-xml-advanced-contracts-2026-10-19.md)). | Each landed slice |

<a id="xml-tasks-for-each-language"></a>

### XML capabilities by language

#### C++ XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cp-x01"></a>CP-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="cp-x02"></a>CP-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | CP-X01 |
| <a id="cp-x03"></a>CP-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="cp-x04"></a>CP-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | CP-X03 |
| <a id="cp-x05"></a>CP-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, CP-X04 |
| <a id="cp-x06"></a>CP-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, CP-X02, CP-X04 |
| <a id="cp-x07"></a>CP-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, CP-X04 |
| <a id="cp-x08"></a>CP-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, CP-X04 |
| <a id="cp-x09"></a>CP-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, CP-X04 |
| <a id="cp-x10"></a>CP-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, CP-X04 |
| <a id="cp-x11"></a>CP-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, CP-X04 |
| <a id="cp-x12"></a>CP-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, CP-X04 |
| <a id="cp-x13"></a>CP-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, CP-X04 |
| <a id="cp-x14"></a>CP-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, CP-X04 |
| <a id="cp-x15"></a>CP-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, CP-X14 |
| <a id="cp-x16"></a>CP-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, CP-X04 |
| <a id="cp-x17"></a>CP-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, CP-X04 |
| <a id="cp-x18"></a>CP-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, CP-X04 |
| <a id="cp-x19"></a>CP-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, CP-X04 |
| <a id="cp-x20"></a>CP-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, CP-X04 |
| <a id="cp-x21"></a>CP-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="cp-x22"></a>CP-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C++ XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Python XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="py-x01"></a>PY-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="py-x02"></a>PY-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | PY-X01 |
| <a id="py-x03"></a>PY-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="py-x04"></a>PY-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | PY-X03 |
| <a id="py-x05"></a>PY-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, PY-X04 |
| <a id="py-x06"></a>PY-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, PY-X02, PY-X04 |
| <a id="py-x07"></a>PY-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, PY-X04 |
| <a id="py-x08"></a>PY-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, PY-X04 |
| <a id="py-x09"></a>PY-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, PY-X04 |
| <a id="py-x10"></a>PY-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, PY-X04 |
| <a id="py-x11"></a>PY-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, PY-X04 |
| <a id="py-x12"></a>PY-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, PY-X04 |
| <a id="py-x13"></a>PY-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, PY-X04 |
| <a id="py-x14"></a>PY-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, PY-X04 |
| <a id="py-x15"></a>PY-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, PY-X14 |
| <a id="py-x16"></a>PY-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, PY-X04 |
| <a id="py-x17"></a>PY-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, PY-X04 |
| <a id="py-x18"></a>PY-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, PY-X04 |
| <a id="py-x19"></a>PY-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, PY-X04 |
| <a id="py-x20"></a>PY-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, PY-X04 |
| <a id="py-x21"></a>PY-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="py-x22"></a>PY-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Python XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Java XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="jv-x01"></a>JV-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="jv-x02"></a>JV-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | JV-X01 |
| <a id="jv-x03"></a>JV-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="jv-x04"></a>JV-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | JV-X03 |
| <a id="jv-x05"></a>JV-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, JV-X04 |
| <a id="jv-x06"></a>JV-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, JV-X02, JV-X04 |
| <a id="jv-x07"></a>JV-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, JV-X04 |
| <a id="jv-x08"></a>JV-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, JV-X04 |
| <a id="jv-x09"></a>JV-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, JV-X04 |
| <a id="jv-x10"></a>JV-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, JV-X04 |
| <a id="jv-x11"></a>JV-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, JV-X04 |
| <a id="jv-x12"></a>JV-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, JV-X04 |
| <a id="jv-x13"></a>JV-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, JV-X04 |
| <a id="jv-x14"></a>JV-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, JV-X04 |
| <a id="jv-x15"></a>JV-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, JV-X14 |
| <a id="jv-x16"></a>JV-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, JV-X04 |
| <a id="jv-x17"></a>JV-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, JV-X04 |
| <a id="jv-x18"></a>JV-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, JV-X04 |
| <a id="jv-x19"></a>JV-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, JV-X04 |
| <a id="jv-x20"></a>JV-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, JV-X04 |
| <a id="jv-x21"></a>JV-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="jv-x22"></a>JV-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Java XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Kotlin XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="kt-x01"></a>KT-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="kt-x02"></a>KT-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | KT-X01 |
| <a id="kt-x03"></a>KT-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="kt-x04"></a>KT-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | KT-X03 |
| <a id="kt-x05"></a>KT-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, KT-X04 |
| <a id="kt-x06"></a>KT-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, KT-X02, KT-X04 |
| <a id="kt-x07"></a>KT-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, KT-X04 |
| <a id="kt-x08"></a>KT-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, KT-X04 |
| <a id="kt-x09"></a>KT-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, KT-X04 |
| <a id="kt-x10"></a>KT-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, KT-X04 |
| <a id="kt-x11"></a>KT-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, KT-X04 |
| <a id="kt-x12"></a>KT-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, KT-X04 |
| <a id="kt-x13"></a>KT-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, KT-X04 |
| <a id="kt-x14"></a>KT-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, KT-X04 |
| <a id="kt-x15"></a>KT-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, KT-X14 |
| <a id="kt-x16"></a>KT-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, KT-X04 |
| <a id="kt-x17"></a>KT-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, KT-X04 |
| <a id="kt-x18"></a>KT-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, KT-X04 |
| <a id="kt-x19"></a>KT-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, KT-X04 |
| <a id="kt-x20"></a>KT-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, KT-X04 |
| <a id="kt-x21"></a>KT-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="kt-x22"></a>KT-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Kotlin XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### C# XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cs-x01"></a>CS-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="cs-x02"></a>CS-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | CS-X01 |
| <a id="cs-x03"></a>CS-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="cs-x04"></a>CS-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | CS-X03 |
| <a id="cs-x05"></a>CS-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, CS-X04 |
| <a id="cs-x06"></a>CS-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, CS-X02, CS-X04 |
| <a id="cs-x07"></a>CS-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, CS-X04 |
| <a id="cs-x08"></a>CS-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, CS-X04 |
| <a id="cs-x09"></a>CS-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, CS-X04 |
| <a id="cs-x10"></a>CS-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, CS-X04 |
| <a id="cs-x11"></a>CS-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, CS-X04 |
| <a id="cs-x12"></a>CS-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, CS-X04 |
| <a id="cs-x13"></a>CS-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, CS-X04 |
| <a id="cs-x14"></a>CS-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, CS-X04 |
| <a id="cs-x15"></a>CS-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, CS-X14 |
| <a id="cs-x16"></a>CS-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, CS-X04 |
| <a id="cs-x17"></a>CS-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, CS-X04 |
| <a id="cs-x18"></a>CS-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, CS-X04 |
| <a id="cs-x19"></a>CS-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, CS-X04 |
| <a id="cs-x20"></a>CS-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, CS-X04 |
| <a id="cs-x21"></a>CS-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="cs-x22"></a>CS-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C# XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Go XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="go-x01"></a>GO-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="go-x02"></a>GO-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | GO-X01 |
| <a id="go-x03"></a>GO-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="go-x04"></a>GO-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | GO-X03 |
| <a id="go-x05"></a>GO-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, GO-X04 |
| <a id="go-x06"></a>GO-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, GO-X02, GO-X04 |
| <a id="go-x07"></a>GO-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, GO-X04 |
| <a id="go-x08"></a>GO-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, GO-X04 |
| <a id="go-x09"></a>GO-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, GO-X04 |
| <a id="go-x10"></a>GO-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, GO-X04 |
| <a id="go-x11"></a>GO-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, GO-X04 |
| <a id="go-x12"></a>GO-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, GO-X04 |
| <a id="go-x13"></a>GO-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, GO-X04 |
| <a id="go-x14"></a>GO-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, GO-X04 |
| <a id="go-x15"></a>GO-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, GO-X14 |
| <a id="go-x16"></a>GO-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, GO-X04 |
| <a id="go-x17"></a>GO-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, GO-X04 |
| <a id="go-x18"></a>GO-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, GO-X04 |
| <a id="go-x19"></a>GO-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, GO-X04 |
| <a id="go-x20"></a>GO-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, GO-X04 |
| <a id="go-x21"></a>GO-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="go-x22"></a>GO-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Go XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### JavaScript XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="js-x01"></a>JS-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="js-x02"></a>JS-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | JS-X01 |
| <a id="js-x03"></a>JS-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="js-x04"></a>JS-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | JS-X03 |
| <a id="js-x05"></a>JS-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, JS-X04 |
| <a id="js-x06"></a>JS-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, JS-X02, JS-X04 |
| <a id="js-x07"></a>JS-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, JS-X04 |
| <a id="js-x08"></a>JS-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, JS-X04 |
| <a id="js-x09"></a>JS-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, JS-X04 |
| <a id="js-x10"></a>JS-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, JS-X04 |
| <a id="js-x11"></a>JS-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, JS-X04 |
| <a id="js-x12"></a>JS-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, JS-X04 |
| <a id="js-x13"></a>JS-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, JS-X04 |
| <a id="js-x14"></a>JS-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, JS-X04 |
| <a id="js-x15"></a>JS-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, JS-X14 |
| <a id="js-x16"></a>JS-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, JS-X04 |
| <a id="js-x17"></a>JS-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, JS-X04 |
| <a id="js-x18"></a>JS-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, JS-X04 |
| <a id="js-x19"></a>JS-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, JS-X04 |
| <a id="js-x20"></a>JS-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, JS-X04 |
| <a id="js-x21"></a>JS-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="js-x22"></a>JS-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected JavaScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### TypeScript XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="ts-x01"></a>TS-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="ts-x02"></a>TS-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | TS-X01 |
| <a id="ts-x03"></a>TS-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="ts-x04"></a>TS-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | TS-X03 |
| <a id="ts-x05"></a>TS-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, TS-X04 |
| <a id="ts-x06"></a>TS-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, TS-X02, TS-X04 |
| <a id="ts-x07"></a>TS-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, TS-X04 |
| <a id="ts-x08"></a>TS-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, TS-X04 |
| <a id="ts-x09"></a>TS-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, TS-X04 |
| <a id="ts-x10"></a>TS-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, TS-X04 |
| <a id="ts-x11"></a>TS-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, TS-X04 |
| <a id="ts-x12"></a>TS-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, TS-X04 |
| <a id="ts-x13"></a>TS-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, TS-X04 |
| <a id="ts-x14"></a>TS-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, TS-X04 |
| <a id="ts-x15"></a>TS-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, TS-X14 |
| <a id="ts-x16"></a>TS-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, TS-X04 |
| <a id="ts-x17"></a>TS-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, TS-X04 |
| <a id="ts-x18"></a>TS-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, TS-X04 |
| <a id="ts-x19"></a>TS-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, TS-X04 |
| <a id="ts-x20"></a>TS-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, TS-X04 |
| <a id="ts-x21"></a>TS-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="ts-x22"></a>TS-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected TypeScript XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Rust XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="rs-x01"></a>RS-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="rs-x02"></a>RS-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | RS-X01 |
| <a id="rs-x03"></a>RS-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="rs-x04"></a>RS-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | RS-X03 |
| <a id="rs-x05"></a>RS-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, RS-X04 |
| <a id="rs-x06"></a>RS-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, RS-X02, RS-X04 |
| <a id="rs-x07"></a>RS-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, RS-X04 |
| <a id="rs-x08"></a>RS-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, RS-X04 |
| <a id="rs-x09"></a>RS-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, RS-X04 |
| <a id="rs-x10"></a>RS-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, RS-X04 |
| <a id="rs-x11"></a>RS-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, RS-X04 |
| <a id="rs-x12"></a>RS-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, RS-X04 |
| <a id="rs-x13"></a>RS-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, RS-X04 |
| <a id="rs-x14"></a>RS-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, RS-X04 |
| <a id="rs-x15"></a>RS-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, RS-X14 |
| <a id="rs-x16"></a>RS-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, RS-X04 |
| <a id="rs-x17"></a>RS-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, RS-X04 |
| <a id="rs-x18"></a>RS-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, RS-X04 |
| <a id="rs-x19"></a>RS-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, RS-X04 |
| <a id="rs-x20"></a>RS-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, RS-X04 |
| <a id="rs-x21"></a>RS-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="rs-x22"></a>RS-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Rust XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Swift XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="sw-x01"></a>SW-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="sw-x02"></a>SW-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | SW-X01 |
| <a id="sw-x03"></a>SW-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="sw-x04"></a>SW-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | SW-X03 |
| <a id="sw-x05"></a>SW-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, SW-X04 |
| <a id="sw-x06"></a>SW-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, SW-X02, SW-X04 |
| <a id="sw-x07"></a>SW-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, SW-X04 |
| <a id="sw-x08"></a>SW-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, SW-X04 |
| <a id="sw-x09"></a>SW-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, SW-X04 |
| <a id="sw-x10"></a>SW-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, SW-X04 |
| <a id="sw-x11"></a>SW-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, SW-X04 |
| <a id="sw-x12"></a>SW-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, SW-X04 |
| <a id="sw-x13"></a>SW-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, SW-X04 |
| <a id="sw-x14"></a>SW-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, SW-X04 |
| <a id="sw-x15"></a>SW-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, SW-X14 |
| <a id="sw-x16"></a>SW-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, SW-X04 |
| <a id="sw-x17"></a>SW-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, SW-X04 |
| <a id="sw-x18"></a>SW-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, SW-X04 |
| <a id="sw-x19"></a>SW-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, SW-X04 |
| <a id="sw-x20"></a>SW-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, SW-X04 |
| <a id="sw-x21"></a>SW-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="sw-x22"></a>SW-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Swift XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### C XML

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="c-x01"></a>C-X01 | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="c-x02"></a>C-X02 | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | C-X01 |
| <a id="c-x03"></a>C-X03 | Scalar token input; split-token lexical/resource failures pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| <a id="c-x04"></a>C-X04 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | C-X03 |
| <a id="c-x05"></a>C-X05 | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S03, C-X04 |
| <a id="c-x06"></a>C-X06 | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S04, C-X02, C-X04 |
| <a id="c-x07"></a>C-X07 | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S05, C-X04 |
| <a id="c-x08"></a>C-X08 | Variant mapping; wrong tag/type combinations reject (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S06, C-X04 |
| <a id="c-x09"></a>C-X09 | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S08, C-X04 |
| <a id="c-x10"></a>C-X10 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, C-X04 |
| <a id="c-x11"></a>C-X11 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S12, C-X04 |
| <a id="c-x12"></a>C-X12 | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S13, C-X04 |
| <a id="c-x13"></a>C-X13 | All traversal; valid permutations pass and duplicates reject (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S14, C-X04 |
| <a id="c-x14"></a>C-X14 | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S16, C-X04 |
| <a id="c-x15"></a>C-X15 | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S17, C-X14 |
| <a id="c-x16"></a>C-X16 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S07, C-X04 |
| <a id="c-x17"></a>C-X17 | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S27, C-X04 |
| <a id="c-x18"></a>C-X18 | Inherited mappings; field collisions and ordering tests pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S28, C-X04 |
| <a id="c-x19"></a>C-X19 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S29, C-X04 |
| <a id="c-x20"></a>C-X20 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S30, C-X04 |
| <a id="c-x21"></a>C-X21 | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| <a id="c-x22"></a>C-X22 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C XML reference profile; [evidence](../verification/verification-xml-tokens-2026-10-19.md)). | S34 |

<a id="compression-tasks-for-each-language"></a>

### Compression capabilities by language

#### C++ compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cp-z01"></a>CP-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | S31 |
| <a id="cp-z02"></a>CP-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z01 |
| <a id="cp-z03"></a>CP-Z03 | Gzip output; an independent decompressor accepts it (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z04"></a>CP-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z05"></a>CP-Z05 | Zlib output; an independent decompressor accepts it (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z06"></a>CP-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z07"></a>CP-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z08"></a>CP-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z09"></a>CP-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z10"></a>CP-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows C++/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| <a id="cp-z11"></a>CP-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures; [qualification](../verification/verification-compression-parity-2026-10-20.md). | CP-Z03, CP-Z04, CP-Z05, CP-Z06, CP-Z07, CP-Z08, CP-Z09, CP-Z10 |

#### Python compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="py-z01"></a>PY-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | S31 |
| <a id="py-z02"></a>PY-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z01 |
| <a id="py-z03"></a>PY-Z03 | Gzip output; an independent decompressor accepts it (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z04"></a>PY-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z05"></a>PY-Z05 | Zlib output; an independent decompressor accepts it (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z06"></a>PY-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z07"></a>PY-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z08"></a>PY-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z09"></a>PY-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z10"></a>PY-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows Python/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| <a id="py-z11"></a>PY-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | PY-Z03, PY-Z04, PY-Z05, PY-Z06, PY-Z07, PY-Z08, PY-Z09, PY-Z10 |

#### Java compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="jv-z01"></a>JV-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | S31 |
| <a id="jv-z02"></a>JV-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z01 |
| <a id="jv-z03"></a>JV-Z03 | Gzip output; an independent decompressor accepts it (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z04"></a>JV-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z05"></a>JV-Z05 | Zlib output; an independent decompressor accepts it (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z06"></a>JV-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z07"></a>JV-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z08"></a>JV-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows Java/provider profile; [evidence](../verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z09"></a>JV-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 Windows JDK 26 optional-provider profiles; [evidence](../verification/verification-java-compression-providers-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z10"></a>JV-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 Windows JDK 26 optional-provider profiles; [evidence](../verification/verification-java-compression-providers-2026-10-20.md)). | JV-Z02 |
| <a id="jv-z11"></a>JV-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | JV-Z03, JV-Z04, JV-Z05, JV-Z06, JV-Z07, JV-Z08, JV-Z09, JV-Z10 |

#### Kotlin compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="kt-z01"></a>KT-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | S31 |
| <a id="kt-z02"></a>KT-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z01 |
| <a id="kt-z03"></a>KT-Z03 | Gzip output; an independent decompressor accepts it (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z04"></a>KT-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z05"></a>KT-Z05 | Zlib output; an independent decompressor accepts it (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z06"></a>KT-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z07"></a>KT-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z08"></a>KT-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Windows Kotlin/JVM 17 profile; [evidence](../verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| <a id="kt-z09"></a>KT-Z09 | Optional Zstandard adapter; lazy runtime jar, present/absent/failed-native admission, independent frames and actual Windows JVM17/Linux JRE replay pass; [evidence](../verification/verification-kotlin-compression-providers-2026-10-20.md). | KT-Z02 |
| <a id="kt-z10"></a>KT-Z10 | Optional safe-Java LZ4-frame adapter; linked history, checksums, present/absent deployment, independent frames and actual Windows JVM17/Linux JRE replay pass; [evidence](../verification/verification-kotlin-compression-providers-2026-10-20.md). | KT-Z02 |
| <a id="kt-z11"></a>KT-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | KT-Z03, KT-Z04, KT-Z05, KT-Z06, KT-Z07, KT-Z08, KT-Z09, KT-Z10 |

#### C# compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cs-z01"></a>CS-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | S31 |
| <a id="cs-z02"></a>CS-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z01 |
| <a id="cs-z03"></a>CS-Z03 | Gzip output; an independent decompressor accepts it (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z04"></a>CS-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z05"></a>CS-Z05 | Zlib output; an independent decompressor accepts it (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z06"></a>CS-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z07"></a>CS-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z08"></a>CS-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 generated .NET 8 profile; [evidence](../verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| <a id="cs-z09"></a>CS-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-csharp-compression-providers-2026-10-20.md). | CS-Z02 |
| <a id="cs-z10"></a>CS-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-csharp-compression-providers-2026-10-20.md). | CS-Z02 |
| <a id="cs-z11"></a>CS-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | CS-Z03, CS-Z04, CS-Z05, CS-Z06, CS-Z07, CS-Z08, CS-Z09, CS-Z10 |

#### Go compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="go-z01"></a>GO-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | S31 |
| <a id="go-z02"></a>GO-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z01 |
| <a id="go-z03"></a>GO-Z03 | Gzip output; an independent decompressor accepts it (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z04"></a>GO-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z05"></a>GO-Z05 | Zlib output; an independent decompressor accepts it (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z06"></a>GO-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z07"></a>GO-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z08"></a>GO-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](../verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| <a id="go-z09"></a>GO-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 actual Go 1.26/pinned module profile; [evidence](../verification/verification-go-compression-providers-2026-10-20.md)). | GO-Z02 |
| <a id="go-z10"></a>GO-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 actual Go 1.26/pinned module profile; [evidence](../verification/verification-go-compression-providers-2026-10-20.md)). | GO-Z02 |
| <a id="go-z11"></a>GO-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | GO-Z03, GO-Z04, GO-Z05, GO-Z06, GO-Z07, GO-Z08, GO-Z09, GO-Z10 |

#### JavaScript compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="js-z01"></a>JS-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | S31 |
| <a id="js-z02"></a>JS-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z01 |
| <a id="js-z03"></a>JS-Z03 | Gzip output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z04"></a>JS-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z05"></a>JS-Z05 | Zlib output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z06"></a>JS-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z07"></a>JS-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z08"></a>JS-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| <a id="js-z09"></a>JS-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-javascript-compression-providers-2026-10-20.md). | JS-Z02 |
| <a id="js-z10"></a>JS-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-javascript-compression-providers-2026-10-20.md). | JS-Z02 |
| <a id="js-z11"></a>JS-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | JS-Z03, JS-Z04, JS-Z05, JS-Z06, JS-Z07, JS-Z08, JS-Z09, JS-Z10 |

#### TypeScript compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="ts-z01"></a>TS-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | S31 |
| <a id="ts-z02"></a>TS-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z01 |
| <a id="ts-z03"></a>TS-Z03 | Gzip output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z04"></a>TS-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z05"></a>TS-Z05 | Zlib output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z06"></a>TS-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z07"></a>TS-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z08"></a>TS-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Node 22/strict TypeScript profile; [evidence](../verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| <a id="ts-z09"></a>TS-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-javascript-compression-providers-2026-10-20.md). | TS-Z02 |
| <a id="ts-z10"></a>TS-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](../verification/verification-javascript-compression-providers-2026-10-20.md). | TS-Z02 |
| <a id="ts-z11"></a>TS-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | TS-Z03, TS-Z04, TS-Z05, TS-Z06, TS-Z07, TS-Z08, TS-Z09, TS-Z10 |

#### Rust compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="rs-z01"></a>RS-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | S31 |
| <a id="rs-z02"></a>RS-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z01 |
| <a id="rs-z03"></a>RS-Z03 | Gzip output; an independent decompressor accepts it (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z04"></a>RS-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z05"></a>RS-Z05 | Zlib output; an independent decompressor accepts it (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z06"></a>RS-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z07"></a>RS-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z08"></a>RS-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](../verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| <a id="rs-z09"></a>RS-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux2.14 SDK profiles; [evidence](../verification/verification-rust-compression-providers-2026-10-20.md). | RS-Z02 |
| <a id="rs-z10"></a>RS-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux2.14 SDK profiles; [evidence](../verification/verification-rust-compression-providers-2026-10-20.md). | RS-Z02 |
| <a id="rs-z11"></a>RS-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | RS-Z03, RS-Z04, RS-Z05, RS-Z06, RS-Z07, RS-Z08, RS-Z09, RS-Z10 |

#### Swift compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="sw-z01"></a>SW-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | S31 |
| <a id="sw-z02"></a>SW-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z01 |
| <a id="sw-z03"></a>SW-Z03 | Gzip output; an independent decompressor accepts it (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z04"></a>SW-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z05"></a>SW-Z05 | Zlib output; an independent decompressor accepts it (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z06"></a>SW-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z07"></a>SW-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z08"></a>SW-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 native Linux Swift 6.1.3 profile; [evidence](../verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| <a id="sw-z09"></a>SW-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux 2.14 SDK profiles; [evidence](../verification/verification-swift-compression-providers-2026-10-20.md). | SW-Z02 |
| <a id="sw-z10"></a>SW-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux 2.14 SDK profiles; [evidence](../verification/verification-swift-compression-providers-2026-10-20.md). | SW-Z02 |
| <a id="sw-z11"></a>SW-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | SW-Z03, SW-Z04, SW-Z05, SW-Z06, SW-Z07, SW-Z08, SW-Z09, SW-Z10 |

#### C compression

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="c-z01"></a>C-Z01 | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | S31 |
| <a id="c-z02"></a>C-Z02 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z01 |
| <a id="c-z03"></a>C-Z03 | Gzip output; an independent decompressor accepts it (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z04"></a>C-Z04 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z05"></a>C-Z05 | Zlib output; an independent decompressor accepts it (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z06"></a>C-Z06 | Zlib input; framing/checksum boundary fixtures pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z07"></a>C-Z07 | Raw-DEFLATE output; independent decompression agrees (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z08"></a>C-Z08 | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 native Windows/Linux C11 SDK profiles; [evidence](../verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| <a id="c-z09"></a>C-Z09 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 actual Windows/Linux C11 optional-provider profiles; [evidence](../verification/verification-c-compression-providers-2026-10-20.md)). | C-Z02 |
| <a id="c-z10"></a>C-Z10 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 actual Windows/Linux C11 optional-provider profiles; [evidence](../verification/verification-c-compression-providers-2026-10-20.md)). | C-Z02 |
| <a id="c-z11"></a>C-Z11 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](../verification/verification-compression-parity-2026-10-20.md). | C-Z03, C-Z04, C-Z05, C-Z06, C-Z07, C-Z08, C-Z09, C-Z10 |

<a id="package-tasks-for-each-language"></a>

### Package capabilities by language

#### C++ packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cp-p01"></a>CP-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | S31 |
| <a id="cp-p02"></a>CP-P02 | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P01 |
| <a id="cp-p03"></a>CP-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P01 |
| <a id="cp-p04"></a>CP-P04 | DEFLATE entry output; independent inflation agrees (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P02, CP-Z07 |
| <a id="cp-p05"></a>CP-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P03, CP-Z08 |
| <a id="cp-p06"></a>CP-P06 | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P02 |
| <a id="cp-p07"></a>CP-P07 | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P03 |
| <a id="cp-p08"></a>CP-P08 | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P06 |
| <a id="cp-p09"></a>CP-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 Windows/Linux reference profiles, sparse offsets above 4 GiB and real 65K counts; [evidence](../verification/verification-package-zip64-input-2026-10-20.md)). | CP-P07 |
| <a id="cp-p10"></a>CP-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 native Windows/Linux reference profiles, independent signature-CRC controls; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | CP-P05, CP-P07 |
| <a id="cp-p11"></a>CP-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 native Windows/Linux profiles; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | CP-P03 |
| <a id="cp-p12"></a>CP-P12 | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 native Windows/Linux static profiles; [evidence](../verification/verification-package-save-faults-2026-10-20.md)). | CP-P02 |
| <a id="cp-p13"></a>CP-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P03 |
| <a id="cp-p14"></a>CP-P14 | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P13 |
| <a id="cp-p15"></a>CP-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 generated C++ Windows/Linux reference profiles; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | CP-P14, S32 |
| <a id="cp-p16"></a>CP-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | CP-P15 |
| <a id="cp-p16a"></a>CP-P16a | Generated C++ package/XML operations share OPC-edge and supported XSD key/unique/keyref tuple admission. Instrumented native validation/reference admission retains part/field/link diagnostics; caller exception types and atomic load/Save behavior pass focused regression. Deferred presentation/provider attribution keeps CP-P16 open; [2.16 batch](../verification/verification-pending-batch-2026-10-20.md). | CP-P15 |
| <a id="cp-p17"></a>CP-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 native Windows/Linux reference profiles; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | CP-P15 |
| <a id="cp-p18"></a>CP-P18 | Preserved whole-part namespace/link context; edited-package references are revalidated (2.14 native Windows/Linux reference profiles; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | CP-P17 |
| <a id="cp-p19"></a>CP-P19 | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected C++ reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | CP-P02, CP-P14 |
| <a id="cp-p20"></a>CP-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, CP-P15 |
| <a id="cp-p21"></a>CP-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | CP-P16, CP-P20 |

#### Python packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="py-p01"></a>PY-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | S31 |
| <a id="py-p02"></a>PY-P02 | Stored ZIP writer; an independent ZIP reader opens its output (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | PY-P01 |
| <a id="py-p03"></a>PY-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P01 |
| <a id="py-p04"></a>PY-P04 | DEFLATE entry output; independent inflation agrees (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P02, PY-Z07 |
| <a id="py-p05"></a>PY-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P03, PY-Z08 |
| <a id="py-p06"></a>PY-P06 | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P02 |
| <a id="py-p07"></a>PY-P07 | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P03 |
| <a id="py-p08"></a>PY-P08 | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P06 |
| <a id="py-p09"></a>PY-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 Windows/Linux reference profiles, sparse offsets above 4 GiB and real 65K counts; [evidence](../verification/verification-package-zip64-input-2026-10-20.md)). | PY-P07 |
| <a id="py-p10"></a>PY-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P05, PY-P07 |
| <a id="py-p11"></a>PY-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | PY-P03 |
| <a id="py-p12"></a>PY-P12 | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | PY-P02 |
| <a id="py-p13"></a>PY-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P03 |
| <a id="py-p14"></a>PY-P14 | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Python reference profile; [evidence](../verification/verification-package-reference-2026-10-20.md)). | PY-P13 |
| <a id="py-p15"></a>PY-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | PY-P14, S32 |
| <a id="py-p16"></a>PY-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | PY-P15 |
| <a id="py-p16a"></a>PY-P16a | Generated Python package/XML operations share OPC-edge and supported XSD identity-tuple admission, with instrumented field/constraint/link diagnostics, namespace-admission forwarding and same-instance caller exceptions. Package37/37, identity6/6 and rich17/17 checks pass, including atomic load/Save. Deferred presentation/provider attribution keeps PY-P16 open; [2.16 batch](../verification/verification-pending-batch-2026-10-20.md). | PY-P15 |
| <a id="py-p17"></a>PY-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | PY-P15 |
| <a id="py-p18"></a>PY-P18 | Preserved whole-part namespace/link context; edited-package references are revalidated (2.14 Windows/Linux generated reference profile; [evidence](../verification/verification-package-profiles-2026-10-20.md)). | PY-P17 |
| <a id="py-p19"></a>PY-P19 | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 Windows/Linux unordered part/link sets with stable identities; [evidence](../verification/verification-package-lifecycle-2026-10-20.md)). | PY-P02, PY-P14 |
| <a id="py-p20"></a>PY-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, PY-P15 |
| <a id="py-p21"></a>PY-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | PY-P16, PY-P20 |

#### Java packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="jv-p01"></a>JV-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | S31 |
| <a id="jv-p02"></a>JV-P02 | Stored ZIP writer; an independent ZIP reader opens its output (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P01 |
| <a id="jv-p03"></a>JV-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P01 |
| <a id="jv-p04"></a>JV-P04 | DEFLATE entry output; independent inflation agrees (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P02, JV-Z07 |
| <a id="jv-p05"></a>JV-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P03, JV-Z08 |
| <a id="jv-p06"></a>JV-P06 | ZIP64 entry writer; a forced-small independent fixture passes (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P02 |
| <a id="jv-p07"></a>JV-P07 | ZIP64 entry reader; Python-produced fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| <a id="jv-p08"></a>JV-P08 | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P06 |
| <a id="jv-p09"></a>JV-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P07 |
| <a id="jv-p10"></a>JV-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P05, JV-P07 |
| <a id="jv-p11"></a>JV-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| <a id="jv-p12"></a>JV-P12 | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P02 |
| <a id="jv-p13"></a>JV-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| <a id="jv-p14"></a>JV-P14 | Scoped relationships/targets; dangling and escaping targets reject (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P13 |
| <a id="jv-p15"></a>JV-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P14, S32 |
| <a id="jv-p16"></a>JV-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | JV-P15 |
| <a id="jv-p17"></a>JV-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P15 |
| <a id="jv-p18"></a>JV-P18 | Preserved relationship/namespace context; edited-package references are revalidated (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P17 |
| <a id="jv-p19"></a>JV-P19 | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](../verification/verification-java-packages-2026-10-20.md)). | JV-P02, JV-P14 |
| <a id="jv-p20"></a>JV-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, JV-P15 |
| <a id="jv-p21"></a>JV-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | JV-P16, JV-P20 |

#### Kotlin packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="kt-p01"></a>KT-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | S31 |
| <a id="kt-p02"></a>KT-P02 | Stored ZIP writer; an independent ZIP reader opens its output. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P01 |
| <a id="kt-p03"></a>KT-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P01 |
| <a id="kt-p04"></a>KT-P04 | DEFLATE entry output; independent inflation agrees. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P02, KT-Z07 |
| <a id="kt-p05"></a>KT-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P03, KT-Z08 |
| <a id="kt-p06"></a>KT-P06 | ZIP64 entry writer; a forced-small independent fixture passes. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P02 |
| <a id="kt-p07"></a>KT-P07 | ZIP64 entry reader; Python-produced fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| <a id="kt-p08"></a>KT-P08 | ZIP64 directory/locator output; an independent directory inspector agrees. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P06 |
| <a id="kt-p09"></a>KT-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P07 |
| <a id="kt-p10"></a>KT-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P05, KT-P07 |
| <a id="kt-p11"></a>KT-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| <a id="kt-p12"></a>KT-P12 | Staged full-Save replacement; injected faults leave an old or complete new file. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P02 |
| <a id="kt-p13"></a>KT-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| <a id="kt-p14"></a>KT-P14 | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P13 |
| <a id="kt-p15"></a>KT-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P14, S32 |
| <a id="kt-p16"></a>KT-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P15 |
| <a id="kt-p17"></a>KT-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P15 |
| <a id="kt-p18"></a>KT-P18 | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P17 |
| <a id="kt-p19"></a>KT-P19 | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | KT-P02, KT-P14 |
| <a id="kt-p20"></a>KT-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, KT-P15 |
| <a id="kt-p21"></a>KT-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | KT-P16, KT-P20 |

#### C# packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cs-p01"></a>CS-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | S31 |
| <a id="cs-p02"></a>CS-P02 | Stored ZIP writer; an independent ZIP reader opens its output. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P01 |
| <a id="cs-p03"></a>CS-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P01 |
| <a id="cs-p04"></a>CS-P04 | DEFLATE entry output; independent inflation agrees. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P02, CS-Z07 |
| <a id="cs-p05"></a>CS-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P03, CS-Z08 |
| <a id="cs-p06"></a>CS-P06 | ZIP64 entry writer; a forced-small independent fixture passes. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P02 |
| <a id="cs-p07"></a>CS-P07 | ZIP64 entry reader; Python-produced fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| <a id="cs-p08"></a>CS-P08 | ZIP64 directory/locator output; an independent directory inspector agrees. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P06 |
| <a id="cs-p09"></a>CS-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P07 |
| <a id="cs-p10"></a>CS-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P05, CS-P07 |
| <a id="cs-p11"></a>CS-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| <a id="cs-p12"></a>CS-P12 | Staged full-Save replacement; injected faults leave an old or complete new file. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P02 |
| <a id="cs-p13"></a>CS-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| <a id="cs-p14"></a>CS-P14 | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P13 |
| <a id="cs-p15"></a>CS-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P14, S32 |
| <a id="cs-p16"></a>CS-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P15 |
| <a id="cs-p17"></a>CS-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P15 |
| <a id="cs-p18"></a>CS-P18 | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P17 |
| <a id="cs-p19"></a>CS-P19 | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/.NET 8 owning profile; [evidence](../verification/verification-pending-50-2026-10-20.md). | CS-P02, CS-P14 |
| <a id="cs-p20"></a>CS-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, CS-P15 |
| <a id="cs-p21"></a>CS-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | CS-P16, CS-P20 |

#### Go packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="go-p01"></a>GO-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | S31 |
| <a id="go-p02"></a>GO-P02 | Stored ZIP writer; an independent ZIP reader opens its output. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P01 |
| <a id="go-p03"></a>GO-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P01 |
| <a id="go-p04"></a>GO-P04 | DEFLATE entry output; independent inflation agrees. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P02, GO-Z07 |
| <a id="go-p05"></a>GO-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P03, GO-Z08 |
| <a id="go-p06"></a>GO-P06 | ZIP64 entry writer; a forced-small independent fixture passes. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P02 |
| <a id="go-p07"></a>GO-P07 | ZIP64 entry reader; Python-produced fixtures pass. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P03 |
| <a id="go-p08"></a>GO-P08 | ZIP64 directory/locator output; an independent directory inspector agrees. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P06 |
| <a id="go-p09"></a>GO-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P07 |
| <a id="go-p10"></a>GO-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P05, GO-P07 |
| <a id="go-p11"></a>GO-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P03 |
| <a id="go-p12"></a>GO-P12 | Staged full-Save replacement; injected faults leave an old or complete new file. Selected 2.14 SDK profile; [evidence](../verification/verification-go-packages-2026-10-20.md). | GO-P02 |
| <a id="go-p13"></a>GO-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P03 |
| <a id="go-p14"></a>GO-P14 | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P13 |
| <a id="go-p15"></a>GO-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P14, S32 |
| <a id="go-p16"></a>GO-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | GO-P15 |
| <a id="go-p17"></a>GO-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P15 |
| <a id="go-p18"></a>GO-P18 | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P17 |
| <a id="go-p19"></a>GO-P19 | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](../verification/verification-go-opc-packages-2026-10-20.md). | GO-P02, GO-P14 |
| <a id="go-p20"></a>GO-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, GO-P15 |
| <a id="go-p21"></a>GO-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | GO-P16, GO-P20 |

#### JavaScript packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="js-p01"></a>JS-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | S31 |
| <a id="js-p02"></a>JS-P02 | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P01 |
| <a id="js-p03"></a>JS-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P01 |
| <a id="js-p04"></a>JS-P04 | DEFLATE entry output; independent inflation agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P02, JS-Z07 |
| <a id="js-p05"></a>JS-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P03, JS-Z08 |
| <a id="js-p06"></a>JS-P06 | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P02 |
| <a id="js-p07"></a>JS-P07 | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| <a id="js-p08"></a>JS-P08 | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P06 |
| <a id="js-p09"></a>JS-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P07 |
| <a id="js-p10"></a>JS-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P05, JS-P07 |
| <a id="js-p11"></a>JS-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| <a id="js-p12"></a>JS-P12 | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P02 |
| <a id="js-p13"></a>JS-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| <a id="js-p14"></a>JS-P14 | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P13 |
| <a id="js-p15"></a>JS-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P14, S32 |
| <a id="js-p16"></a>JS-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | JS-P15 |
| <a id="js-p17"></a>JS-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P15 |
| <a id="js-p18"></a>JS-P18 | Preserved relationship/namespace context; edited-package references are revalidated (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P17 |
| <a id="js-p19"></a>JS-P19 | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | JS-P02, JS-P14 |
| <a id="js-p20"></a>JS-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, JS-P15 |
| <a id="js-p21"></a>JS-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | JS-P16, JS-P20 |

#### TypeScript packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="ts-p01"></a>TS-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | S31 |
| <a id="ts-p02"></a>TS-P02 | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P01 |
| <a id="ts-p03"></a>TS-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P01 |
| <a id="ts-p04"></a>TS-P04 | DEFLATE entry output; independent inflation agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P02, TS-Z07 |
| <a id="ts-p05"></a>TS-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P03, TS-Z08 |
| <a id="ts-p06"></a>TS-P06 | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P02 |
| <a id="ts-p07"></a>TS-P07 | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| <a id="ts-p08"></a>TS-P08 | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P06 |
| <a id="ts-p09"></a>TS-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P07 |
| <a id="ts-p10"></a>TS-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P05, TS-P07 |
| <a id="ts-p11"></a>TS-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| <a id="ts-p12"></a>TS-P12 | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P02 |
| <a id="ts-p13"></a>TS-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| <a id="ts-p14"></a>TS-P14 | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P13 |
| <a id="ts-p15"></a>TS-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P14, S32 |
| <a id="ts-p16"></a>TS-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | TS-P15 |
| <a id="ts-p17"></a>TS-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P15 |
| <a id="ts-p18"></a>TS-P18 | Preserved relationship/namespace context; edited-package references are revalidated (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P17 |
| <a id="ts-p19"></a>TS-P19 | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](../verification/verification-javascript-packages-2026-10-20.md)). | TS-P02, TS-P14 |
| <a id="ts-p20"></a>TS-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, TS-P15 |
| <a id="ts-p21"></a>TS-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | TS-P16, TS-P20 |

#### Rust packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="rs-p01"></a>RS-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31 |
| <a id="rs-p02"></a>RS-P02 | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P01 |
| <a id="rs-p03"></a>RS-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P01 |
| <a id="rs-p04"></a>RS-P04 | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P02, RS-Z07 |
| <a id="rs-p05"></a>RS-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P03, RS-Z08 |
| <a id="rs-p06"></a>RS-P06 | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P02 |
| <a id="rs-p07"></a>RS-P07 | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| <a id="rs-p08"></a>RS-P08 | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P06 |
| <a id="rs-p09"></a>RS-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P07 |
| <a id="rs-p10"></a>RS-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P05, RS-P07 |
| <a id="rs-p11"></a>RS-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| <a id="rs-p12"></a>RS-P12 | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P02 |
| <a id="rs-p13"></a>RS-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| <a id="rs-p14"></a>RS-P14 | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P13 |
| <a id="rs-p15"></a>RS-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P14, S32 |
| <a id="rs-p16"></a>RS-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P15 |
| <a id="rs-p17"></a>RS-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P15 |
| <a id="rs-p18"></a>RS-P18 | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P17 |
| <a id="rs-p19"></a>RS-P19 | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P02, RS-P14 |
| <a id="rs-p20"></a>RS-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, RS-P15 |
| <a id="rs-p21"></a>RS-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | RS-P16, RS-P20 |

#### Swift packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="sw-p01"></a>SW-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31 |
| <a id="sw-p02"></a>SW-P02 | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P01 |
| <a id="sw-p03"></a>SW-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P01 |
| <a id="sw-p04"></a>SW-P04 | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P02, SW-Z07 |
| <a id="sw-p05"></a>SW-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P03, SW-Z08 |
| <a id="sw-p06"></a>SW-P06 | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P02 |
| <a id="sw-p07"></a>SW-P07 | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| <a id="sw-p08"></a>SW-P08 | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P06 |
| <a id="sw-p09"></a>SW-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P07 |
| <a id="sw-p10"></a>SW-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P05, SW-P07 |
| <a id="sw-p11"></a>SW-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| <a id="sw-p12"></a>SW-P12 | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P02 |
| <a id="sw-p13"></a>SW-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| <a id="sw-p14"></a>SW-P14 | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P13 |
| <a id="sw-p15"></a>SW-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P14, S32 |
| <a id="sw-p16"></a>SW-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P15 |
| <a id="sw-p17"></a>SW-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P15 |
| <a id="sw-p18"></a>SW-P18 | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P17 |
| <a id="sw-p19"></a>SW-P19 | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P02, SW-P14 |
| <a id="sw-p20"></a>SW-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, SW-P15 |
| <a id="sw-p21"></a>SW-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | SW-P16, SW-P20 |

#### C packages

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="c-p01"></a>C-P01 | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31 |
| <a id="c-p02"></a>C-P02 | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P01 |
| <a id="c-p03"></a>C-P03 | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P01 |
| <a id="c-p04"></a>C-P04 | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P02, C-Z07 |
| <a id="c-p05"></a>C-P05 | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P03, C-Z08 |
| <a id="c-p06"></a>C-P06 | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P02 |
| <a id="c-p07"></a>C-P07 | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P03 |
| <a id="c-p08"></a>C-P08 | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P06 |
| <a id="c-p09"></a>C-P09 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P07 |
| <a id="c-p10"></a>C-P10 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P05, C-P07 |
| <a id="c-p11"></a>C-P11 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P03 |
| <a id="c-p12"></a>C-P12 | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P02 |
| <a id="c-p13"></a>C-P13 | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P03 |
| <a id="c-p14"></a>C-P14 | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P13 |
| <a id="c-p15"></a>C-P15 | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P14, S32 |
| <a id="c-p16"></a>C-P16 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P15 |
| <a id="c-p17"></a>C-P17 | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P15 |
| <a id="c-p18"></a>C-P18 | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P17 |
| <a id="c-p19"></a>C-P19 | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P02, C-P14 |
| <a id="c-p20"></a>C-P20 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | S31, S32, C-P15 |
| <a id="c-p21"></a>C-P21 | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | C-P16, C-P20 |

<a id="qualification-tasks-for-each-language"></a>

### Qualification records by language

#### C++ qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cp-q01"></a>CP-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cp-q02"></a>CP-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="cp-q03"></a>CP-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cp-q04"></a>CP-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cp-q05"></a>CP-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected MSVC Windows owning workloads; [evidence](../verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| <a id="cp-q06"></a>CP-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected MSVC Windows process high-water residency; [evidence](../verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| <a id="cp-q07"></a>CP-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected MSVC Windows owning workloads; [evidence](../verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| <a id="cp-q08"></a>CP-Q08 | Supported-platform and reproducibility replay passes for every claimed platform. Selected Windows/Linux owning profiles; [evidence](../verification/verification-cpp-xml-performance-2026-10-20.md). | — |

#### Python qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="py-q01"></a>PY-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="py-q02"></a>PY-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="py-q03"></a>PY-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="py-q04"></a>PY-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="py-q05"></a>PY-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Python 3.14 Windows owning workloads; [evidence](../verification/verification-python-xml-performance-2026-10-20.md). | — |
| <a id="py-q06"></a>PY-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Python 3.14 Windows owning workloads; [evidence](../verification/verification-python-xml-performance-2026-10-20.md). | — |
| <a id="py-q07"></a>PY-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Python 3.14 Windows owning workloads; [evidence](../verification/verification-python-xml-performance-2026-10-20.md). | — |
| <a id="py-q08"></a>PY-Q08 | Supported-platform and reproducibility replay passes for every claimed platform. Selected Windows/Linux owning profiles; [evidence](../verification/verification-python-xml-performance-2026-10-20.md). | — |

#### Java qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="jv-q01"></a>JV-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="jv-q02"></a>JV-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="jv-q03"></a>JV-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="jv-q04"></a>JV-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="jv-q05"></a>JV-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="jv-q06"></a>JV-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="jv-q07"></a>JV-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="jv-q08"></a>JV-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### Kotlin qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="kt-q01"></a>KT-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="kt-q02"></a>KT-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="kt-q03"></a>KT-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="kt-q04"></a>KT-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="kt-q05"></a>KT-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="kt-q06"></a>KT-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="kt-q07"></a>KT-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="kt-q08"></a>KT-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### C# qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="cs-q01"></a>CS-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cs-q02"></a>CS-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="cs-q03"></a>CS-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cs-q04"></a>CS-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="cs-q05"></a>CS-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="cs-q06"></a>CS-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="cs-q07"></a>CS-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="cs-q08"></a>CS-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### Go qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="go-q01"></a>GO-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="go-q02"></a>GO-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="go-q03"></a>GO-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="go-q04"></a>GO-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="go-q05"></a>GO-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="go-q06"></a>GO-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="go-q07"></a>GO-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="go-q08"></a>GO-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### JavaScript qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="js-q01"></a>JS-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="js-q02"></a>JS-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="js-q03"></a>JS-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="js-q04"></a>JS-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="js-q05"></a>JS-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="js-q06"></a>JS-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="js-q07"></a>JS-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="js-q08"></a>JS-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### TypeScript qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="ts-q01"></a>TS-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="ts-q02"></a>TS-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="ts-q03"></a>TS-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="ts-q04"></a>TS-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="ts-q05"></a>TS-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="ts-q06"></a>TS-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="ts-q07"></a>TS-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-portable-xml-performance-2026-10-20.md). | — |
| <a id="ts-q08"></a>TS-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### Rust qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="rs-q01"></a>RS-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="rs-q02"></a>RS-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="rs-q03"></a>RS-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="rs-q04"></a>RS-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="rs-q05"></a>RS-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="rs-q06"></a>RS-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="rs-q07"></a>RS-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="rs-q08"></a>RS-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### Swift qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="sw-q01"></a>SW-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="sw-q02"></a>SW-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="sw-q03"></a>SW-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="sw-q04"></a>SW-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="sw-q05"></a>SW-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="sw-q06"></a>SW-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="sw-q07"></a>SW-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="sw-q08"></a>SW-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

#### C qualification

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="c-q01"></a>C-Q01 | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="c-q02"></a>C-Q02 | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |
| <a id="c-q03"></a>C-Q03 | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="c-q04"></a>C-Q04 | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](../verification/verification-xml-exchange-2026-10-20.md). | — |
| <a id="c-q05"></a>C-Q05 | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="c-q06"></a>C-Q06 | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="c-q07"></a>C-Q07 | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](../verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| <a id="c-q08"></a>C-Q08 | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](../verification/verification-pending-105-2026-10-21.md). | — |

<a id="c-document-state-bridge-tasks"></a>

### C++ document-state bridge capabilities

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="d01"></a>D01 | Saved-baseline identity/layout/digest format; altered-identity byte fixtures reject. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | S31 |
| <a id="d02"></a>D02 | Sidecar generation manifest; stale/foreign documents fail before payload decoding. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D01 |
| <a id="d03"></a>D03 | Object-ID snapshot restoration; IDs survive Save/reopen unchanged. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D01 |
| <a id="d04"></a>D04 | Allocation high-water restoration; issued IDs are never reused. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D03 |
| <a id="d05"></a>D05 | Retained-history checkpoint transfer; undo/redo survives reopen. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D03 |
| <a id="d06"></a>D06 | Branch/cursor transfer; forked-history/current-cursor fixtures pass. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D05 |
| <a id="d07"></a>D07 | Data-only document encoder; independent inspection finds no side-channel records. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D01, CP-P12 |
| <a id="d08"></a>D08 | Stage document/sidecars under a new generation; staging failure preserves publication. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D02, D04, D05, D06, D07 |
| <a id="d09"></a>D09 | Publication state machine/commit marker; every transition has a fixture. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D08 |
| <a id="d10"></a>D10 | Windows durable replacement; injected faults preserve one coherent generation. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D09 |
| <a id="d11"></a>D11 | Linux durable replacement; matching fault/synchronization tests pass. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D09 |
| <a id="d12"></a>D12 | Precommit recovery; each interruption reopens the old baseline/history. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D10, D11 |
| <a id="d13"></a>D13 | Postcommit recovery; each interruption reopens the new baseline/history. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D10, D11 |
| <a id="d14"></a>D14 | Journal baseline fence/replay; mismatches reject before decoding/replay. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D02, D12, D13 |
| <a id="d15"></a>D15 | Collaboration/session/outbox fences; pending state cannot cross saved baselines. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D02, D12, D13 |
| <a id="d16"></a>D16 | Full Save/reopen integration; IDs/history/cursors/journal/session remain coherent. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D04, D05, D06, D12, D13, D14, D15 |
| <a id="d17"></a>D17 | Legacy native compatibility replay; existing bytes/APIs retain their contract. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| <a id="d18"></a>D18 | Final-file isolation audit; independent inspection finds no side channels. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| <a id="d19"></a>D19 | Bounded pre-publication crash matrix; old-generation recovery passes. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| <a id="d20"></a>D20 | Bounded post-publication crash matrix; new-generation recovery passes. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| <a id="d21"></a>D21 | Executable usage/migration example; Save/load instructions and limitations match results. Selected C++ native/external-JSON Windows/Linux profile; [evidence](../verification/verification-managed-document-bridge-2026-10-20.md). | D17, D18, D19, D20 |
| <a id="d22"></a>D22 | Checked first-time identified typed storage, stored document namespace and native allocation watermark; preserve IDs/references/burned gaps and one history baseline; validate before publication/scoped resource hooks, then qualify native Save/recovery (2.14 eight Windows/Linux C++ profiles; [evidence](../verification/verification-managed-identified-baseline-2026-10-20.md)). | MR04, MR05 |
| <a id="d23"></a>D23 | Bounded caller-supplied external-document codecs and exact untouched-baseline attachment; identity/layout/digest binding rejects stale, foreign and lossy candidates (2.13 C++ JSON and selected binary callback profiles; [evidence](../verification/verification-sidecar-journal-2026-10-20.md)). | Existing C++ managed runtime |
| <a id="d24"></a>D24 | Native v3 differential copy/literal sidecars retain identities, allocation marks, history and session state through selected Save/recovery and v2 upgrade; main files contain document data only (2.13 selected native document/journal profiles; [evidence](../verification/verification-sidecar-journal-2026-10-20.md)). | D23 |

<a id="shared-release-gates"></a>

### Release qualification records

| ID | Implemented capability and recorded verification | Dependencies |
| --- | --- | --- |
| <a id="g01"></a>G01 | Independent XML/XSD oracle corpus passes for the landed subset. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g02"></a>G02 | Independent ZIP/OPC corpus passes for the landed subset. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g03"></a>G03 | Current selected Windows source/configuration passes the native regression wrapper; failures are resolved. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g04"></a>G04 | Current selected Linux source/configuration passes native regression with failures resolved. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g05"></a>G05 | XML-disabled build and explicit unavailability behavior pass. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g06"></a>G06 | Every bundled provider disabled: build/link and supported identity/stored paths pass. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g07"></a>G07 | CLI version, release metadata and same-major CMake discovery compatibility pass. Selected 2.14 combined Windows/Linux profiles; [evidence](../verification/verification-current-regression-2026-10-20.md). | — |
| <a id="g08"></a>G08 | Editor impact is reviewed; if touched, both versions/changelogs/listings/packages are synchronized and validated; [packaged resolver verification](../verification/verification-editor-field-navigation-2026-10-20.md). | — |
| <a id="g09"></a>G09 | Coverage, README, skill, migration, links, ignore rules and task-artifact cleanup match executed results; [verified checkpoint](../verification/verification-current-regression-2026-10-20.md). | — |

## Historical qualification checkpoints

The 2.24.0 worktree completes `MG04`'s retained native/authority branch-merge
contract: typed conflicts, two-parent ancestry, opt-in capability advertisement,
exact-target/cursor requests, current policy/locks, accepted field deltas, selective
Undo/Redo and Save/journal recovery. All four storage/ID profiles pass on Windows
and Linux; [scope and executed evidence](../verification/verification-collaboration-merging-2026-10-21.md).
The later 2.28.0 OPC/3MF completions and their acceptance evidence are recorded
in the requirement table below. Unrelated active and deferred requirements
remain separate from this batch.

The 2.22.0 batch completes native capability activation/history-policy
conversion (`MG02`), session/cache fault qualification (`MG05`) and outer Pack-IRI
addressing (`OPC-OPT03`). See the [managed checks](../verification/verification-managed-capabilities-2026-10-21.md)
and [independent Pack-IRI checks](../verification/verification-pack-iri-2026-10-21.md).
The integrated build passes **275/275 CTests**; subsequent isolated fixes have
separate targeted checks in the [aggregate verification record](../verification/verification-integrated-opc-2026-10-21.md).
These results do not close unrelated OPC/3MF requirements. The selected official Core corpus now
completes `OPC-R05`: [175 model parts accounted for, 156 round trips per supported consumer](../verification/verification-core-conformance-2026-10-21.md).
The mandatory fifth-edition metadata and Annex B corpus completes `OPC-R08`;
At that checkpoint six main OPC/3MF requirements remained open; the latest
requirement table below includes subsequent completions.
`XML-B13` now completes the eleven-output prepared-provider and physical namespace
admission matrix: [44,892 boundary decisions and actual provider checks](../verification/verification-runtime-boundaries-2026-10-21.md).
`OPC-R11`, `XML-B05` and `XML-B21` now complete the selected large owning
operation chain in all eleven actual Windows SDKs: XML identities, DEFLATE,
preservation, cumulative quotas, cancellation/deadlines and atomic Save/recovery.
See the [large combined-operation qualification and backend boundaries](../verification/verification-large-package-operations-2026-10-21.md).

Compiler/runtime **2.21.0** adds the reusable fifth-edition C++ OPC core-property
adapter, completing `OPC-OPT01`; see its
[independent schema/OPC checks and twenty-three-item audit](../verification/verification-core-properties-2026-10-21.md).
The concurrent batch also implements
[managed authorization, capability and allocation slices](../verification/verification-managed-authorization-2026-10-21.md)
and [Python QName-list schema identities](../verification/verification-qname-lists-2026-10-21.md).
Their broader managed/XML parent requirements remain open with explicit acceptance
boundaries. All 55 initially active requirements were assigned and audited together;
an audit is not evidence that an unfinished requirement is implemented.
The combined 2.21.0 native build and **271/271 CTests pass**; see the
[integrated result and checkpoint provenance](../verification/verification-integrated-pending-2026-10-21.md).

The **2.20.1 measurement checkpoint** completes `XML-B06` across eleven actual
Windows SDKs: 132 native operation profiles, 660 timing samples and exact native
bytes agree with XML-disabled controls. See the
[executed baseline and measurement limits](../verification/verification-native-codec-baseline-2026-10-21.md).
`XML-B12` is reconciled with the already completed `OPC-R13` authoring/evolution
contract and its eleven-output Windows/Linux evidence. That implementation
checkpoint left **54 requirements unchecked**, with 793 of 847 rows checked.
Selected Java/C#/Kotlin/Swift namespace
accounting formed the initial `XML-B13` slice; the later eleven-output closure
above supersedes that checkpoint. See
[8,256 boundary decisions and provider checks](../verification/verification-namespace-budgets-2026-10-21.md).

Compiler/runtime **2.20.0** completes `XML-B11` and `OPC-R12`: 300 XSD
compiler-tooling decisions pass on Windows and Linux, including checked bundles,
UTF-8 paths, directional comparisons, rich exports and protected enum-binding
failures. All eleven outputs pass the ordinary exported/rebound model matrix on
both hosts. See the [scope and final audit](../verification/verification-xsd-tooling-2026-10-21.md).
That checkpoint left **57 requirements unchecked**, with 790 of 847 rows checked.

Compiler/runtime **2.19.2** completes `OPC-R13`: staged XSD imports preserve
surviving expanded-identity IDs and protect reviewed source, while authored
models retain native names/IDs/order/defaults/behavior through supported XML
rename/reorder exceptions. All eleven outputs pass the Windows/Linux
[actual evolution and interoperability matrix](../verification/verification-xsd-evolution-2026-10-21.md).
That checkpoint left 59 unchecked, with 788 of 847 rows checked.

Compiler/runtime **2.19.1** completes `XML-B23`: independent ordering, ZIP/XML
metadata and provider-qualified byte reproducibility checks across all eleven
outputs on Windows and Linux. The matrix preserves each backend's existing wire
policy; Python's generic ZIP probe remains diagnostic/internal. See the
[actual SDK matrix and provider boundaries](../verification/verification-package-reproducibility-2026-10-21.md).
That checkpoint left 60 unchecked, with 787 of 847 rows checked.

Compiler/runtime **2.19.0** completes `XML-B22` preservation/policy/collision
qualification in supported XML/package graphs across all eleven outputs. Fixed
singleton role paths rebind preserved links simultaneously, with opt-in canonical
C/Rust writers and Go loaders. At that checkpoint, dynamic renames, broader XML
graphs and `OPC-R06` profile work remained separate; the 2.28.0 pipeline checks
below supersede those gates. Lexical XML and signatures retain their explicit
capability boundaries. See the
[actual SDK matrix and scope](../verification/verification-package-relocation-2026-10-21.md).
That checkpoint left 61 unchecked, with 786 of 847 rows checked.

Compiler/runtime **2.18.0** completes `XML-B20`: additive C/Rust native XML
operations, shared XSD identity budgets and public selected-codec composition in
all eleven outputs. Windows/Linux SDKs, strict native checks and sanitizers pass
within the [recorded profiles and provider limits](../verification/verification-native-xml-operations-2026-10-21.md).
That checkpoint left 62 unchecked, with 785 of 847 rows checked.

Compiler/runtime **2.17.1** completes managed reference/resource/journal and
collaboration qualification (`MR01`, `MR02`) on Windows and Linux, including an
initial-join namespace fix. The completed eleven-output package evidence also
closes API parity parents `OPC-R07` and `XML-B17`, leaving 63 unchecked at that
checkpoint. See the [managed batch and parent reconciliation](../verification/verification-managed-reference-faults-2026-10-21.md).

Compiler/runtime **2.17.0** and schema language **1.21.0** completed the next
**105 existing bounded IDs**: 85 package work units and 20 malformed/provider
and platform/reproducibility checkpoints, leaving 67 broader requirements at that
checkpoint. The [batch scope and verification](../verification/verification-pending-105-2026-10-21.md)
and [provider/platform record](../verification/verification-q02-q08-2026-10-21.md)
separate implemented owning profiles from deferred graphs, hard resource
ceilings, broad corpora and publication.

The 2.16 implementation slices below are recorded separately from their broader
requirements. Checked slices do not close an entire backend/corpus/platform gate;
see the [2.16 QName scope and verification](../verification/verification-qname-scopes-2026-10-20.md)
and [compiler/package/managed batch](../verification/verification-pending-batch-2026-10-20.md),
the [five-SDK measurements](../verification/verification-portable-xml-performance-2026-10-20.md),
and [earlier 2.15 implementation scope](../verification/verification-pending-xml-2026-10-20.md).

The previous completed dependency batch covers **50 existing IDs**: `CS-P01`–`CS-P19`,
`KT-P01`–`KT-P19`, and `Q05`–`Q07` for `KT`, `RS`, `SW` and `C`.
Their prerequisites and acceptance checks pass for the documented Windows owning
profiles. Each checked row links its evidence; broader parent and platform gates
remain separately tracked.
See the [batch dependencies and verification](../verification/verification-pending-50-2026-10-20.md).
