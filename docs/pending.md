# Pending requirements across Serializer

Following the explicit application-ownership review, `MIG02`, `MG06` and `REL02`
are **will not fix — application/maintainer responsibility** (`[-]`), not completed
implementations. `MG03`'s existing inherited authorization and publication boundary
is complete within Serializer; principal binding, policy persistence and policy
administration belong to the host. See the
[ownership decisions and remaining mixed requirements](project/application_responsibilities.md).
The current inventory is **815 implemented/verified, 3 will not fix and 33 pending**
out of 851 rows. **28 are active and five are deferred subsystem requirements**.
By maintainer direction on 2026-10-21, new non-C++ managed engines (`MG01`),
the page store/index (`PG01`), lazy model/history storage (`PG02`), database sinks
(`DB01`) and sink migration/rebaseline (`MIG03`) are deferred to linked GitHub
issues. The latter two were already held. Deferred means unimplemented and
outside the current batch, not completed or will not fix.

The 2.24.0 worktree completes `MG04`'s retained native/authority branch-merge
contract: typed conflicts, two-parent ancestry, opt-in capability advertisement,
exact-target/cursor requests, current policy/locks, accepted field deltas, selective
Undo/Redo and Save/journal recovery. All four storage/ID profiles pass on Windows
and Linux; [scope and executed evidence](verification/verification-collaboration-merging-2026-10-21.md).
The later 2.28.0 OPC/3MF completions and their acceptance evidence are recorded
in the requirement table below. Unrelated active and deferred requirements
remain separate from this batch.

The 2.22.0 batch completes native capability activation/history-policy
conversion (`MG02`), session/cache fault qualification (`MG05`) and outer Pack-IRI
addressing (`OPC-OPT03`). See the [managed checks](verification/verification-managed-capabilities-2026-10-21.md)
and [independent Pack-IRI checks](verification/verification-pack-iri-2026-10-21.md).
The integrated build passes **275/275 CTests**; subsequent isolated fixes have
separate targeted checks in the [aggregate verification record](verification/verification-integrated-opc-2026-10-21.md).
These results do not close unrelated OPC/3MF requirements. The selected official Core corpus now
completes `OPC-R05`: [175 model parts accounted for, 156 round trips per supported consumer](verification/verification-core-conformance-2026-10-21.md).
The mandatory fifth-edition metadata and Annex B corpus completes `OPC-R08`;
At that checkpoint six main OPC/3MF requirements remained open; the latest
requirement table below includes subsequent completions.
`XML-B13` now completes the eleven-output prepared-provider and physical namespace
admission matrix: [44,892 boundary decisions and actual provider checks](verification/verification-runtime-boundaries-2026-10-21.md).
`OPC-R11`, `XML-B05` and `XML-B21` now complete the selected large owning
operation chain in all eleven actual Windows SDKs: XML identities, DEFLATE,
preservation, cumulative quotas, cancellation/deadlines and atomic Save/recovery.
See the [large combined-operation qualification and backend boundaries](verification/verification-large-package-operations-2026-10-21.md).

Compiler/runtime **2.21.0** adds the reusable fifth-edition C++ OPC core-property
adapter, completing `OPC-OPT01`; see its
[independent schema/OPC checks and twenty-three-item audit](verification/verification-core-properties-2026-10-21.md).
The concurrent batch also implements
[managed authorization, capability and allocation slices](verification/verification-managed-authorization-2026-10-21.md)
and [Python QName-list schema identities](verification/verification-qname-lists-2026-10-21.md).
Their broader managed/XML parent requirements remain open with explicit acceptance
boundaries. All 55 initially active requirements were assigned and audited together;
an audit is not evidence that an unfinished requirement is implemented.
The combined 2.21.0 native build and **271/271 CTests pass**; see the
[integrated result and checkpoint provenance](verification/verification-integrated-pending-2026-10-21.md).

The **2.20.1 measurement checkpoint** completes `XML-B06` across eleven actual
Windows SDKs: 132 native operation profiles, 660 timing samples and exact native
bytes agree with XML-disabled controls. See the
[executed baseline and measurement limits](verification/verification-native-codec-baseline-2026-10-21.md).
`XML-B12` is reconciled with the already completed `OPC-R13` authoring/evolution
contract and its eleven-output Windows/Linux evidence. That implementation
checkpoint left **54 requirements unchecked**, with 793 of 847 rows checked.
Selected Java/C#/Kotlin/Swift namespace
accounting formed the initial `XML-B13` slice; the later eleven-output closure
above supersedes that checkpoint. See
[8,256 boundary decisions and provider checks](verification/verification-namespace-budgets-2026-10-21.md).

Compiler/runtime **2.20.0** completes `XML-B11` and `OPC-R12`: 300 XSD
compiler-tooling decisions pass on Windows and Linux, including checked bundles,
UTF-8 paths, directional comparisons, rich exports and protected enum-binding
failures. All eleven outputs pass the ordinary exported/rebound model matrix on
both hosts. See the [scope and final audit](verification/verification-xsd-tooling-2026-10-21.md).
That checkpoint left **57 requirements unchecked**, with 790 of 847 rows checked.

Compiler/runtime **2.19.2** completes `OPC-R13`: staged XSD imports preserve
surviving expanded-identity IDs and protect reviewed source, while authored
models retain native names/IDs/order/defaults/behavior through supported XML
rename/reorder exceptions. All eleven outputs pass the Windows/Linux
[actual evolution and interoperability matrix](verification/verification-xsd-evolution-2026-10-21.md).
That checkpoint left 59 unchecked, with 788 of 847 rows checked.

Compiler/runtime **2.19.1** completes `XML-B23`: independent ordering, ZIP/XML
metadata and provider-qualified byte reproducibility checks across all eleven
outputs on Windows and Linux. The matrix preserves each backend's existing wire
policy; Python's generic ZIP probe remains diagnostic/internal. See the
[actual SDK matrix and provider boundaries](verification/verification-package-reproducibility-2026-10-21.md).
That checkpoint left 60 unchecked, with 787 of 847 rows checked.

Compiler/runtime **2.19.0** completes `XML-B22` preservation/policy/collision
qualification in supported XML/package graphs across all eleven outputs. Fixed
singleton role paths rebind preserved links simultaneously, with opt-in canonical
C/Rust writers and Go loaders. At that checkpoint, dynamic renames, broader XML
graphs and `OPC-R06` profile work remained separate; the 2.28.0 pipeline checks
below supersede those gates. Lexical XML and signatures retain their explicit
capability boundaries. See the
[actual SDK matrix and scope](verification/verification-package-relocation-2026-10-21.md).
That checkpoint left 61 unchecked, with 786 of 847 rows checked.

Compiler/runtime **2.18.0** completes `XML-B20`: additive C/Rust native XML
operations, shared XSD identity budgets and public selected-codec composition in
all eleven outputs. Windows/Linux SDKs, strict native checks and sanitizers pass
within the [recorded profiles and provider limits](verification/verification-native-xml-operations-2026-10-21.md).
That checkpoint left 62 unchecked, with 785 of 847 rows checked.

Compiler/runtime **2.17.1** completes managed reference/resource/journal and
collaboration qualification (`MR01`, `MR02`) on Windows and Linux, including an
initial-join namespace fix. The completed eleven-output package evidence also
closes API parity parents `OPC-R07` and `XML-B17`, leaving 63 unchecked at that
checkpoint. See the [managed batch and parent reconciliation](verification/verification-managed-reference-faults-2026-10-21.md).

Compiler/runtime **2.17.0** and schema language **1.21.0** completed the next
**105 existing bounded IDs**: 85 package work units and 20 malformed/provider
and platform/reproducibility checkpoints, leaving 67 broader requirements at that
checkpoint. The [batch scope and verification](verification/verification-pending-105-2026-10-21.md)
and [provider/platform record](verification/verification-q02-q08-2026-10-21.md)
separate implemented owning profiles from deferred graphs, hard resource
ceilings, broad corpora and publication.

The 2.16 implementation slices below are recorded separately from their broader
requirements. Checked slices do not close an entire backend/corpus/platform gate;
see the [2.16 QName scope and verification](verification/verification-qname-scopes-2026-10-20.md)
and [compiler/package/managed batch](verification/verification-pending-batch-2026-10-20.md),
the [five-SDK measurements](verification/verification-portable-xml-performance-2026-10-20.md),
and [earlier 2.15 implementation scope](verification/verification-pending-xml-2026-10-20.md).

The previous completed dependency batch covers **50 existing IDs**: `CS-P01`–`CS-P19`,
`KT-P01`–`KT-P19`, and `Q05`–`Q07` for `KT`, `RS`, `SW` and `C`.
Their prerequisites and acceptance checks pass for the documented Windows owning
profiles. Each checked row links its evidence; broader parent and platform gates
remain separately tracked.
See the [batch dependencies and verification](verification/verification-pending-50-2026-10-20.md).

## OPC and 3MF requirements

The 2.28.0 update completes `OPC-R04`'s explicit ordered owning storage across all
eleven outputs, including fixed-XSD grouped/repeated sequence/choice binding,
exact fixed cardinality, grammar-preserving export and bounded compatibility.
See the [grouped-XSD acceptance checks](verification/verification-ordered-xsd-2026-10-22.md).
The same update completes `OPC-R01` namespace identities and `OPC-R03`
scalar/list/union restrictions in all eleven outputs; see the
[scalar identity and actual SDK record](verification/verification-scalar-identities-2026-10-22.md)
and [Unicode pattern checks](verification/verification-unicode-xml-patterns-2026-10-22.md).
`OPC-R06` now composes XML/JSON/BINARY_NONE profiles, typed references, runtime
selection and repeated-role relocation in all eleven outputs, including native
XML companions; see the [package pipeline checks](verification/verification-package-pipelines-2026-10-22.md).
`OPC-R02` completes namespace-aware wildcard admission and pinned strict/lax
declaration validation across all eleven outputs, including required capabilities,
shared resource policies, compositor combinations and native package companions.
All **20 rows in this section are complete**. The [wildcard qualification](verification/verification-xml-wildcards-2026-10-22.md)
records the SDK matrices and all 299 verified repository CTest cases, including
the targeted replays after integration fixes.

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | OPC-R01 | — | All eleven outputs preserve qualified attributes, Unicode names, QName/list/prefix-list occurrence scopes, equivalent prefixes, local shadowing and default-namespace absence. Native C/Rust document companions cover wrapped/nil, mixed/ordered/all, map/variant and lifecycle graphs without native model/wire changes. Namespace constraints/defaults use schema-owned expanded identities; [scalar checks](verification/verification-scalar-identities-2026-10-22.md), [native graph checks](verification/verification-native-namespace-graphs-2026-10-22.md). | — |
| [x] | OPC-R01a | — | C++/Python scalar-child namespace scopes survive local declarations, shadowing, default namespace resets, optional/repeated occurrences and nested QName/QName-list unions. Native strings and wire codecs retain their contract; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01b | — | Shared UTF-8 XML NCName validation accepts Unicode mapping names and QName prefixes, with invalid encoding/name rejection; all eleven generators accept Unicode mappings. JavaScript/TypeScript independent XML checks verify qualified Unicode attributes, QName lists, prefix lists and child-local scopes; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01c | — | Go/Java/C# retain QName/list/prefix-list occurrence scopes, qualified Unicode attributes, copied native-authoring bindings and cumulative retention bounds. Schema-scoped outer union enumeration/default/fixed and restricted atomic/union list items are implemented; [2.28 actual SDK evidence](verification/verification-scalar-identities-2026-10-22.md). | — |
| [x] | OPC-R01d | — | Kotlin/Swift retain local/shadowed QName/list/prefix-list declarations and explicit default absence through optional/repeated values and qualified attributes. Direct and union enumeration/default/fixed, restricted list items and foreign schema-default isolation pass actual SDK/provider controls; [2.28 evidence](verification/verification-scalar-identities-2026-10-22.md). | — |
| [x] | OPC-R01e | — | C/Rust XML document companions preserve occurrence namespaces across wrapped/nil records, mixed/ordered/all groups, lifecycle metadata, maps and element/xsi:type variants. Complete scalar/list/union restrictions and schema-owned defaults/fixed preserve native model structs and protocol bytes; [namespace graphs](verification/verification-native-namespace-graphs-2026-10-22.md), [native constraints](verification/verification-native-qname-constraints-2026-10-22.md). | — |
| [x] | OPC-R02 | — | All eleven outputs enforce namespace-aware reject/skip/preserve slots, exact XSD predicates/positions/counts, required capabilities and pinned strict/lax declaration validation. Foreign fallback stays closed to unknown core/local names; private validators share the caller's resource and identity policies. Compositor combinations, large fallback owners, empty inventories, edited retained content and legacy C++ Core tree mappings pass actual SDK/provider checks. [Wildcard matrix and integrated verification](verification/verification-xml-wildcards-2026-10-22.md). | — |
| [x] | OPC-R03 | — | All eleven outputs compile exact primitive/list/union facets, first-selected outer identities, schema-scoped QName enumeration/default/fixed and restricted atomic/union list-item contracts from the same XSD inventory. Export retains declaration scopes and restrictions. Bounded Unicode category/block/name escapes and recursive class subtraction run through prepared portable programs; numeric writers validate canonical spelling before publication. Native-width floating overflow remains an explicit representability boundary; external prefix-list defaults/export require a separate capability contract. [Scalar SDK/provider checks](verification/verification-scalar-identities-2026-10-22.md), [Unicode pattern matrix](verification/verification-unicode-xml-patterns-2026-10-22.md). | — |
| [x] | OPC-R03a | — | First-successful union selection and recursive primitive/list item identities now apply in all eleven outputs, including outer enumeration/default/fixed and schema-scoped QName/list alternatives; [2.28 executed scope](verification/verification-scalar-identities-2026-10-22.md). | — |
| [x] | OPC-R04 | — | All eleven owning outputs bind explicit [element-only ordered arrays](serialization/ordered_xml.md) to multiple typed choices and nested/repeated XSD sequence/choice groups. Readers and writers enforce complete grammar and exact fixed storage without hidden order metadata or native format changes. XSD export retains group occurrences; bounded reader-direction comparison rejects narrowing before publication. Actual SDKs pass 340 input decisions, edited-output rejection and native protocol parity; C++ adds 511 independently classified traces. [2.28 grouped-XSD evidence](verification/verification-ordered-xsd-2026-10-22.md). | — |
| [x] | OPC-R05 | — | Selected published Core 1.4/XML bundle qualified against 175 official model parts with unchanged independent XSD oracles: 156 edited round trips per C++/Python consumer, six correct schema rejections, four bounded-resource rejections and nine explicit 16 MiB profile exclusions. Nine other backend selectors reject the unsupported rich graph precisely. Package/geometry conformance and three upstream positive/XSD disagreements remain distinct; [complete inventory](verification/verification-core-conformance-2026-10-21.md). | — |
| [x] | OPC-R06 | — | All eleven outputs compose immutable model/MIME/container identities with fixed/default/runtime XML/JSON/BINARY_NONE and stored/deflate profiles, typed references, exact internal/external target modes, root vocabulary/type checks and repeated/dynamic role rename provenance. C/Rust retain per-part XML namespace/foreign companions without native model/wire changes. Selected callbacks share the operation; rejected loads/Saves preserve prior publication. [Actual SDK and independent archive evidence](verification/verification-package-pipelines-2026-10-22.md). | — |
| [x] | OPC-R06a | — | Generated fixed singleton roles support preserved source/internal-target canonical rebinding across eleven outputs, with opt-in C/Rust writers and Go loaders; original relative scopes, selectors, external URI strings and opaque bytes retain their values. Removed moved roles, collisions and dangling references reject. The later OPC-R06 completion adds repeated/dynamic explicit renames and selected profiles; [2.28 pipeline checks](verification/verification-package-pipelines-2026-10-22.md), [2.19 scope](verification/verification-package-relocation-2026-10-21.md), [original 2.15 scope](verification/verification-pending-xml-2026-10-20.md). | — |
| [x] | OPC-R07 | — | Implement ZIP/OPC metadata, reader/writer, shared limits, preservation and full-Save APIs for every generated language, with independent producer/consumer tests; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md), [eleven-output completion and limits](verification/verification-managed-reference-faults-2026-10-21.md#package-parent-reconciliation). | — |
| [x] | OPC-R08 | — | Mandatory fifth-edition URI/content-type/Relationships and Annex B complete-rewrite contract qualified in all eleven actual SDKs: 196 shared physical-record/MCE/URI/IRI/encoding controls per backend, independent Windows/Linux native readers and metadata replay. Escaped/equivalent names, relative targets, collisions, physical metadata, string whitespace, UTF-16 and scoped Relationships compatibility retain their declared contracts. Optional Unicode part naming/interleaving/growth hints remain explicitly unsupported; [metadata qualification](verification/verification-opc-metadata-2026-10-21.md), [Annex B and replay](verification/verification-opc-annex-b-2026-10-21.md). | — |
| [x] | OPC-R09 | — | Selected 75,000-tag owning XML workloads measured in all eleven Windows SDKs with actual providers, CPU/process peaks and encoded/generated sizes: 216 isolated profiles/1,080 timed batches and 132 large cancellation/deadline/recovery controls. Actual Linux C++ adds 54 profiles/270 calls and 12 controls. Public C#/Kotlin/Swift policies and bounded deferred Swift failure are implemented; final source bridges retain earlier timing provenance. Broader graphs/platform/provider qualification stays separate; [executed scope](verification/verification-large-xml-2026-10-21.md). | — |
| [x] | OPC-R10 | — | All eleven actual Windows outputs qualify namespace-sensitive scoped translators, borrowed native getters/storage, exact/one-tight depth and byte bounds, failure publication and fresh recovery, alongside 25,000-value stress controls. C++ borrows caller scope metadata; mobile outputs retain assigned source occurrence scopes; C/Rust provide encode-only borrowed companions. Durable-address XML has a separate explicit rejection policy, verified atomically in every output. [Graph/resource and lifetime evidence](verification/verification-readonly-xml-graphs-2026-10-22.md), [large stress controls](verification/verification-readonly-xml-stress-2026-10-21.md). | — |
| [x] | OPC-R11 | — | One cumulative context spans the supported generated owning operation's XML, DEFLATE, ZIP, scoped identities, preservation and publication in all eleven outputs. Three 25,000-entry owners pass exact/one-tight reference/work/allocation admission, real cancellation/deadline boundaries, old-file preservation and fresh recovery; actual prepared-provider completion has separate checks. Public cross-call budget arguments differ by backend; [full executed scope](verification/verification-large-package-operations-2026-10-21.md). | — |
| [x] | OPC-R12 | — | Fixed-XSD association/import/export and checked local bundles pass the rich text/wrapper/enum, namespace/particle and ambiguity/publication inventory. Authored names, IDs, defaults and behavior retain their contracts through all eleven ordinary Windows/Linux export/rebind outputs; imports remain reviewed. [2.20.0 scope and evidence](verification/verification-xsd-tooling-2026-10-21.md). | — |
| [x] | OPC-R13 | — | One-source XSD/inline exception workflows pass staged new-field/rename imports, protected ambiguity failures and authored native-model XML rename/reorder in all eleven outputs on Windows/Linux. Native names, IDs, defaults, behavior and literal positional/integer/string bytes retain their contracts; imported evolution remains reviewed. [2.19.2 evidence](verification/verification-xsd-evolution-2026-10-21.md). | — |

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

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | MR01 | — | Broaden native identity/reference lifecycle, corruption, resource and fault tests across allocation, complete-candidate validation, deletion, Undo/Redo, persistence and collaboration; [eight-profile Windows/Linux checks](verification/verification-managed-reference-faults-2026-10-21.md). | — |
| [x] | MR01a | — | Eight existing C++ identity/storage profiles qualify cyclic deletion, complete-candidate edge cleanup, historical pins, Undo/Redo, persisted history, candidate allocation failure and post-reservation snapshot-admission rollback with nonreuse/recovery; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Broader collaboration/resource faults are qualified by MR01. | — |
| [x] | MR02 | — | Broaden independent-import ownership/external-mapping failures, editable child moves/retirement, preview resource failures and journal/acceptance faults; preserve IDs and atomic publication; [eight-profile Windows/Linux checks](verification/verification-managed-reference-faults-2026-10-21.md). | — |
| [x] | MR02a | — | Eight existing C++ profiles reject six external-map failures, five ownership-attachment failures and corrupt zero/duplicate source IDs without publishing state/history, then recover successfully. Five added tests per profile are part of 184 passing GTests; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Wider preview/journal/acceptance gates are qualified by MR02. | — |
| [ ] | MR03 | — | Define and qualify managed token codecs for other generated outputs; keep these distinct from full native managed engines. Resolve currently rejected recursive generic/reference combinations only under an explicit supported contract. | — |
| [x] | MR04 | — | Typed C++ managed_ref, get_id, stable class_id and registry reference/resolution; document/type scope and complete-candidate validation (2.7 Windows direct/separated fixed/compact uint32/uint64 profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | Existing C++ managed runtime |
| [x] | MR05 | — | Initialize required/self/cyclic managed references after native identity assignment and before first validation/publication/history snapshot (2.7 C++ typed-reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MR06 | — | Native two-pass independent import; remap declared tokens, apply explicit external mappings, validate ownership/references and attach transactionally; ordinary integer payloads retain their values (2.7 C++ reference/import profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MR07 | — | C++ apply_editable retains entity identity and reconciles owned children, with zero IDs for genuinely new children; separately, active-transaction registry.reserve and registry.adopt_reserved admit unused identities reserved by that transaction through checked attachment (2.7 generated editable/reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04, MR05 |
| [x] | MR08 | — | Owner-thread previews reserve IDs through the source allocator before exposure; canceled IDs stay burned and exact-base/exact-ID acceptance preserves native Undo/Redo and recovery (2.7 four C++ registry profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR07 |
| [x] | MR09 | — | Identical collaborative document/object IDs with authority leases and reserve_ids; durable reservations, nonreuse, exact acceptance and forged/exhausted-range rejection (2.7 native strict-collaboration profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MIG01 | — | Qualify ID conversion against genuinely rebuilt 2.6.0 compiler/runtime artifacts; all three raw binary formats and supported managed history/identity/allocation profiles convert exactly (Windows direct uint32/separated uint64, 92 conversions; [evidence](verification/verification-managed-legacy-migration-2026-10-20.md)). | — |
| [-] | MIG02 | — | **Will not fix — application responsibility.** Application-specific historical layouts, custom containers, checkpoint/outbox meaning and epoch transitions require an explicit application migration. Existing supported Serializer-format conversion/compatibility remains a library responsibility. [Decision](project/application_responsibilities.md#disposed-requirements). | — |
| [ ] | MIG03 | — | **Deferred subsystem — [GitHub #5](https://github.com/rohit-singh-gautam/Serializer/issues/5).** Define any journal/database-sink migration/rebaseline adapter separately from managed-save/raw-model conversion and test recovery after interrupted publication. | DB01 for database sinks |
| [x] | MIG04 | — | Explicit same-schema fixed/compact-varint ID conversion helper/CLI for supported managed saves and raw binary formats; preserve exact identities, allocation marks, history, leases and native layout (2.7 direct uint32/separated uint64 reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | Existing same-schema C++ codecs |

## Managed state and storage

The proposed [durable database integration requirements](design/database_storage_requirements.md)
refine PG01, PG02, DB01 and MIG03 with publication, operation identity,
incremental validation, retention and integration acceptance contracts. They do
not change these deferred statuses or close MG02/MG03 integration gaps by
inference. C++23 minimum/toolchain and any new public API/schema grammar require
explicit maintainer release/scope decisions before implementation; this proposal
does not change the current C++20 baseline or the inventory counts below.

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | MG01 | — | **Deferred subsystem — [GitHub #1](https://github.com/rohit-singh-gautam/Serializer/issues/1).** Implement native managed engines for selected non-C++ backends: ownership, immutable reads, transactions, identity, history, journals and collaboration under native lifetime/error/concurrency contracts; [Language bindings](managed/language_bindings.md). | — |
| [x] | MG02 | — | Version-one native C++ feature activation/exclusions, mandatory identity/authorization, forty actual engine combinations and all ordered history-policy conversions across direct/separated identities; [implemented contract and Windows/Linux checks](verification/verification-managed-capabilities-2026-10-21.md). Proposed schema selectors remain distinct from this native API. | — |
| [x] | MG03 | — | Inherited entity/field authorization, editable/read-only exceptions and separate content/structure/policy checks are implemented, with native edit/import/history/publication enforcement and the same candidate helper at authority acceptance. Authentication, policy storage/administration and principal binding are host responsibilities, will not fix in Serializer. [Contract and ownership](project/application_responsibilities.md#security-and-authorization), [executed checks](verification/verification-managed-authorization-2026-10-21.md). | — |
| [x] | MG04 | — | Native C++ retained-revision three-way merging, typed conflicts, two-parent ancestry, opt-in authority negotiation and exact-target/cursor requests use complete-candidate policy/locks and accepted field/dependency deltas. Selective Undo/Redo, exact retries and both journal fault/recovery layouts pass in all four Windows/Linux storage/ID profiles; [contract](managed/branch_merging.md), [executed evidence and exclusions](verification/verification-collaboration-merging-2026-10-21.md). Client-authored branch graph import and automatic stale-edit merging are distinct contracts. | — |
| [x] | MG05 | — | Native session records exercise uint32/uint64/string keys, failed submission/polling, lost acknowledgement, checkpoint reload and epoch changes; 384 presence/cache cycles preserve strict identity and authority ownership. [Windows/Linux qualification](verification/verification-managed-capabilities-2026-10-21.md); network deployment/authentication and presence UI are application responsibilities. | — |
| [-] | MG06 | — | **Will not fix — application responsibility.** The application coordinates distributed participants, durable business intents, idempotent external file/network effects and reconciliation. Serializer retains atomic local transactions/journals and does not execute business effects or promise exactly-once service delivery. [Decision](project/application_responsibilities.md#disposed-requirements). | — |
| [ ] | MG07 | — | Provide allocator-aware payload containers/decoding and hard peak-allocation hooks; distinguish admission estimates from measured process peaks; [Ownership/allocation](managed/history.md#ownership-and-custom-allocation), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG08 | — | Define supported managed packed/view/recursive/Protobuf and broader inheritance/editor contracts, preserving explicit rejection until real code and qualification exist; [C++ runtime](managed/cpp_runtime.md), [generics](schema/generics.md), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG09 | — | The 2.23.0 worktree adds field-level merging and selective collaboration Undo/Redo within an unchanged managed variant tag/identity. Complete qualification, virtual/abstract/override edit dispatch and portable owned/borrowed method or typed-error contracts; [State enhancements](managed/state_enhancements.md#remaining-work). | — |
| [ ] | MG10 | — | Extend projection/dependency notification policies and compact nested identity/history records where proposed; measure retained/deleted-entity costs and edit latency; [Identity/history](managed/history.md), [data structures](managed/data_structures.md). | — |
| [ ] | PG01 | — | **Deferred subsystem — [GitHub #2](https://github.com/rohit-singh-gautam/Serializer/issues/2).** Implement the proposed page store/cache, persistent index keys, dirty-page writeback/recovery, bounded construction and corruption handling before claiming a mostly-disk index; [Paged indexing](design/paged_indexing.md). | — |
| [ ] | PG02 | — | **Deferred subsystem — [GitHub #3](https://github.com/rohit-singh-gautam/Serializer/issues/3).** Add lazy value/storage loading and the proposed paged capability selectors only after index/page lifetime and recovery contracts are implemented and measured; [Paged indexing](design/paged_indexing.md). | PG01 |
| [ ] | DB01 | — | **Deferred subsystem — [GitHub #4](https://github.com/rohit-singh-gautam/Serializer/issues/4).** Implement and qualify an explicit database document/journal sink adapter with ownership, transactions, schema mapping and failure/recovery semantics; [Database integration](guides/database_integration.md#proposed-document-sink-boundary). | — |

## Cached-origin document bridge

Accepted on 2026-10-21 and implemented in 2.26.0. These generic C++ document-bridge
additions form the completed priority batch below. Each leaf and its acceptance
check is capped at five hours;
split a leaf before exceeding that cap. Application storage policy, external
publication/authentication and reconciliation remain host responsibilities under
MG06. An application may attach native differences to a durable original or
first publish its own complete recovery main. Shared immutable-base resolution
and cache-dependency inventory are optional future work, not prerequisites.

| Status | ID | Backend | Requirement and acceptance check | Dependencies |
| --- | --- | --- | --- | --- |
| [x] | D25 | C++ | 2.26 external-document sidecars retain bounded opaque origin metadata outside authored content through edits/history/Save/recovery. Corruption, wrong generation bindings and exceeded bounds reject before admission; [API](managed/document_origin.md), [executed checks](verification/verification-document-origin-2026-10-22.md). Ordinary native containers/custom sinks do not implicitly acquire this capability. | D24 |
| [x] | D26 | C++ | Durable external Save preparation pins the accepted checkpoint, allocator/history/paired session state and expected old activation. A bounded opaque token and validated staged bytes survive restart as an explicit unresolved decision; durable mutations and client transport cannot race it. [Qualification](verification/verification-document-origin-2026-10-22.md). | D25 |
| [x] | D27 | C++ | Confirmation checks the exact token, old activation and independently supplied publication evidence; wrong/stale/tampered inputs reject, exact retries do not reactivate or mark later edits saved. Native selection precedes dictionary replacement/retirement. Partial host-copy failure, lost acknowledgements, unchanged authored bytes and abrupt native publication controls pass; [evidence](verification/verification-document-origin-2026-10-22.md). Host effects/reconciliation remain MG06. | D26 |
| [x] | D28 | C++ | Maintained application-controlled owned-copy/durable-original example; Windows/NTFS and Linux/ext4 each pass 129 native tests plus one inapplicable-mode skip and 480 process-recovery checks. Deleted source/capture, IDs/watermarks/Undo/session state and physical main/manifest/companion sizes are exercised. Final Windows regression passes 281/281; [scope, versions and limits](verification/verification-document-origin-2026-10-22.md). WSL mounted-filesystem failures remain QA01. | D25, D26, D27 |

## Compiler and backend proposals

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SC01 | — | Define proposed signed/floating payload contracts, rounding, overflow, NaN/infinity, zero and scaled arithmetic before keywords or wire values; current syntax remains unavailable (76 vectors, exhaustive16-bit and independent conversion checks; [contract](design/compact_scalar_contract.md), [evidence](verification/verification-compact-scalar-contract-2026-10-20.md)). | — |
| [ ] | SC02 | — | Broaden default-value validation, preserve useful input/member positions in diagnostics and define bit-field support with wire/compatibility tests; [Feature roadmap](project/feature_status.md#other-roadmap-items). | — |
| [x] | SC02a | — | Diagnose out-of-range canonical decimal/hex integer literals in explicit language 1.14+ defaults and retain original member paths/positions through includes and generic binding. Preserve native expressions and legacy defaults; broader validation and bit fields remain pending; [2.15 scope](verification/verification-pending-xml-2026-10-20.md). | — |
| [x] | SC02b | — | Explicit language 1.14+ canonical decimal float/double overflow and declared-type-qualified unknown enum defaults diagnose the original member, including included/generic source. Atomic compiler rejection and generated Python four-protocol checks pass; native expressions, numeric Boolean defaults, underflow and legacy spelling remain compatible; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Bit fields and broader validation remain pending. | — |
| [ ] | SC03 | — | The 2.23.0 worktree adds generated C++ fixed-array Protobuf mapping, exact packed/unpacked/text cardinality and format-specific default retention; [mapping boundary](serialization/protobuf.md#mapping-contract). Complete generic-function calls/defaults and final SDK qualification; [Generics](schema/generics.md). | — |
| [ ] | BE01 | — | Implement native-backend separate-file layout where currently unavailable; preserve naming, dependency includes/modules and editor navigation; [Native languages](languages/native_languages.md). | — |
| [ ] | BE02 | — | Define and implement Java packed output and the proposed stream/channel, native-acceleration and output-buffer-reuse APIs, with ownership/resource tests; [Java output](languages/java.md#defaults-and-unsupported-schema-features). | — |
| [ ] | BE03 | — | Qualify borrowed/mapped-view platform, lifetime, allocation and performance boundaries; broader formats require their own mapping contract; [Views](serialization/views.md). | — |
| [x] | BE04 | — | Compiler-only installed CMake discovery avoids unused runtime-provider lookup; preserve runtime exports, relocation, component selection and cross-toolchain host lookup (2.14 Windows/Linux neutral and multiarch install profiles; [evidence](verification/verification-backlog-2026-10-20.md)). | — |
| [x] | OPC-OPT01 | — | Reusable C++ fifth-edition core-property model, standalone XML and OPC writer/loader adapters; applications supply all fifteen standard values, including localized mixed keywords and normative W3CDTF dates. [Usage](serialization/core_properties.md) and [independent verification](verification/verification-core-properties-2026-10-21.md). | — |
| [ ] | OPC-OPT02 | — | Define a signature/canonicalization/trust provider and independently qualify editing/signed-package behavior before claiming signature support; [OPC design](design/opc_3mf_support.md). | — |
| [x] | OPC-OPT03 | — | Public C++ Pack-IRI composition, parsing, relative resolution and policy-directed outer-package equivalence; optional non-ASCII part naming/interleaving/growth hints have explicit unsupported policy. [Independent Windows/Linux checks](verification/verification-pack-iri-2026-10-21.md) and [usage](serialization/pack_iri.md); mandatory Annex B work remains OPC-R08. | — |

## Qualification and distribution

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | QA01 | — | Replay changed contracts on other supported platforms and remaining provider combinations with real SDKs; retain exact versions, configurations and failures; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md). WSL Windows-mounted appended-journal discard failures remain outside the passing native Windows/Linux filesystem profile; [recorded controls](verification/verification-document-origin-2026-10-22.md#toolchains-and-limits). | — |
| [ ] | QA02 | — | Execute the independent XML/XSD/ZIP/OPC/3MF corpus, malformed-input/fault/resource cases and cross-language exchanges for each newly implemented slice. | — |
| [ ] | QA03 | — | Measure throughput, allocations/peak memory, encoded size and generated-code size against reproducible workloads/baselines; preserve ordinary non-OPC paths; [Python compression measurements](verification/verification-python-compression-performance-2026-10-20.md). | — |
| [ ] | QA04 | — | Qualify current editor-host behavior where a dated record identifies only shared/provider/native-interpreter checks; keep packaged/listing versions and all supported navigation synchronized when changed; [Editor navigation](editors/editor_navigation.md), [qualification records](verification/README.md). | — |
| [x] | REL01 | — | Inspect published VSIX archive/hash/identity contents and independently build/install the upstream package with an isolated installed consumer (published 2.0.0 archives and 2.0.1 x64-windows, nine consumer checks; [evidence](verification/verification-distribution-2026-10-20.md)). | — |
| [-] | REL02 | — | **Will not fix — maintainer responsibility.** Release/Marketplace/upstream-port publication, credentials and signing approval are separately authorized maintainer operations. Build/package correctness remains repository work. [Decision](project/application_responsibilities.md#disposed-requirements). | — |

## XML and package backlog

### Implemented XML and package capability tasks

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | XML-C01 | — | Two-stage XSD declaration import: emit an editable native schema, then compile that schema; separately bind existing declarations with using schema; preserve explicit member IDs, authored behavior and protected existing source (2.7 eleven-SDK automatic/manual consumers; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | S12, S24, S25 |
| [x] | XML-C02 | — | Generated C++/Python owning package facades and selected XML/opaque roles; standard content-type/relationship metadata, integrity, preservation and full-Save round trips (2.7–2.14 selected C++/Python package profiles; [evidence](verification/verification-package-reference-2026-10-20.md)). | S31, S32 |
| [x] | XML-C03 | — | Pinned official Core 1.4 plus XML vocabulary bundle import/binding and authored-model consumers; unsupported constructs reject (2.8 selected C++/Python Core consumers; [evidence](verification/verification-opc-backends-2026-10-19.md)). | XML-C01 |
| [x] | XML-C04 | — | Readonly value-reference and scoped-projection XML output preserves source lifetime and avoids duplicate owning payloads; durable address XML has an explicit rejection boundary (selected eleven-output reference contracts; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30 |

### XML and package requirement tasks

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | XML-B01 | — | Broaden schema-first XML vocabulary and ambiguous-association tooling. | — |
| [ ] | XML-B02 | — | Extend declaration import to additional XSD content-model, identity, chameleon and pattern combinations; qualify schema migration automation beyond the supported subset. | — |
| [ ] | XML-B03 | — | Implement equivalent rich XSD/XML emitters beyond C++/Python and qualify broader numeric, QName, content-model and provider combinations. | — |
| [ ] | XML-B04 | — | Owning package facades exist in all eleven outputs (OPC-R07). Complete declarative payload references, target-mode/profile/runtime selection and broader corpus/performance qualification before closing this parent. | — |
| [x] | XML-B05 | — | Public same-ledger selected-codec composition and the large owning XML/package operation qualify in all eleven actual SDKs, with copied standalone C#/Kotlin/Swift policies, provider completion fences and per-operation publication/recovery. The three-owner input contains 150,003 model elements and 225,000 identity tuples; [combined qualification](verification/verification-large-package-operations-2026-10-21.md), [prepared-provider/public-policy checks](verification/verification-operation-publication-2026-10-21.md). Unsupported rich graphs and opaque provider CPU/process limits remain separate. | — |
| [x] | XML-B06 | — | Repeatable ordinary-native throughput and memory measurements pass across eleven Windows SDKs with XML-disabled controls: 132 positional-binary operation profiles, 660 samples, exact cross-language bytes and separate residency/traced-allocation categories. [Executed baseline and limits](verification/verification-native-codec-baseline-2026-10-21.md). | — |
| [ ] | XML-B07 | — | Qualify additional platforms/provider combinations, broad performance and complete selected OPC/3MF profiles. | — |
| [ ] | XML-B08 | — | XML performance: Broaden tree-free token coverage and qualify provider/platform and retained-tree helper costs; measure CPU, memory, encoded size and generated-code size on reproducible workloads. | — |
| [ ] | XML-B09 | — | XML vocabulary: Extend combinations of presence/nil, ordered fragments, maps/variants and positioned extensions; qualify bounded foreign retention, grouped/repeated heterogeneous choices and broader mixed content. | — |
| [ ] | XML-B10 | — | XSD compilation: Extend facets/derivations, multiple-typed-branch/grouped choices and scalar/pattern/identity/local-bundle combinations; qualify union/QName boundaries and explicit unsupported-graph capability gates. | — |
| [x] | XML-B11 | — | Expanded XSD tooling passes 300 decisions per host: exact bundle/path/identity bounds, directional comparisons, eight rich export families, six invalid enum-binding profiles and protected publication. All eleven ordinary exported/rebound SDK outputs pass Windows/Linux interoperability; rich runtime graphs retain their recorded scope. [2.20.0 evidence](verification/verification-xsd-tooling-2026-10-21.md). | — |
| [x] | XML-B12 | — | Inline association and staged declaration import preserve authored behavior, IDs, defaults and native ordering through reviewed schema evolution. This duplicates the completed OPC-R13 interface gate; broader XSD graph/import combinations stay in XML-B02/B10. [Executed eleven-output authoring/evolution matrix](verification/verification-xsd-evolution-2026-10-21.md). | — |
| [x] | XML-B13 | — | Actual prepared providers, bounded input, concurrent reuse, caller validator interfaces and prohibited remote schema/entity loading qualified in all eleven outputs. Physical namespace admissions pass 44,892 independent tree/token boundary decisions; fresh C++ provider 4/4 and late-completion regressions pass. Broader combined-operation gates remain separate; [capture provenance and scope](verification/verification-runtime-boundaries-2026-10-21.md). | — |
| [ ] | XML-B14 | — | Model coverage: Broaden concrete generic, owning inheritance, revision/magic and digest combinations; define supported managed/view and recursive/combined graph contracts. | — |
| [ ] | XML-B15 | — | Referenced XML output: Broaden stress/resource and ownership qualification without duplicate owning payloads; define any durable field-address XML contract before enabling it. | — |
| [x] | XML-B16 | — | Compression parity: Complete whole-object source/sink wrappers and identical framing/error/limit contracts across every output; qualify optional Zstandard/LZ4 adapters; [qualification](verification/verification-compression-parity-2026-10-20.md). | — |
| [x] | XML-B17 | — | Package parity: Complete ZIP/OPC source/sink, metadata, shared budgets and full-Save guarantees across every output; [eleven-output completion and limits](verification/verification-managed-reference-faults-2026-10-21.md#package-parent-reconciliation). | — |
| [ ] | XML-B18 | — | ZIP extensions: Broaden large-archive, stress and provider coverage; qualify ZIP64/spooling, peak-memory and cancellation boundaries in every backend; unsupported features reject explicitly. | — |
| [ ] | XML-B19 | — | Typed profiles: Extend C++/Python declarations with pipeline/runtime selection, vocabulary profiles and generated payload reference checks before publication; implement equivalent contracts in other backends. | — |
| [x] | XML-B20 | — | Typed package loading: Complete generated transactional profiles and automatic codec/reference budgets in every backend; caller-owned callbacks compose selected codecs with the same shared context; [2.18 SDK verification and graph limits](verification/verification-native-xml-operations-2026-10-21.md). | — |
| [x] | XML-B21 | — | All eleven actual SDKs qualify the selected large OPC/DEFLATE/scoped-reference/preservation/publication chain, including cumulative quotas, cancellation/real deadlines, previous-file preservation and recovery. Java/C#/Go readonly late-success prepared-provider cancellation has separate actual-provider evidence; Python returning Save hooks now fence both pre-publication rollback and post-publication uncertainty. [Large operation results](verification/verification-large-package-operations-2026-10-21.md), [completion regressions](verification/verification-operation-publication-2026-10-21.md); opaque provider preemption/process ceilings remain host deployment limits. | — |
| [x] | XML-B22 | — | Preservation: XML/package policy and collision qualification covers supported graphs in every backend, including simultaneous fixed-role rebinding, removed-role tombstones, exact selector/external values and failure publication; broader graph gates, signatures and lexical preservation remain separate. [Executed matrix and historical XML-policy evidence](verification/verification-package-relocation-2026-10-21.md). | — |
| [x] | XML-B23 | — | Independent canonical ordering/metadata policies pass across all eleven outputs on Windows/Linux: 2,112 actual archives, six input permutations, two fresh processes and both ZIP64 selections. Provider-qualified compressed equality preserves existing wire policies; Python plain ZIP is a private diagnostic. [2.19.1 evidence and scope](verification/verification-package-reproducibility-2026-10-21.md). | — |
| [ ] | XML-B24 | — | HTML: Broaden restricted HTML adapters only under an explicit supported contract; keep arbitrary HTML and unrestricted mixed-content XHTML outside that contract. | — |

## Work units capped at five hours

### Shared compiler and contract tasks

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | S01 | 3 h | Define token/event ownership and limits; golden event traces and malformed boundaries pass (2.9 provider-independent shared event contract; [evidence](verification/verification-xml-events-2026-10-19.md)). | Existing XML mapping |
| [x] | S02 | 3 h | Define incremental UTF-8/name/namespace handling; split-byte, scope and DTD rejection fixtures pass (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S03 | 3 h | Define explicit nil/presence IR and configuration; ambiguous absent/empty/nil mappings reject (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S04 | 4 h | Define ordered mixed-content IR; text/child/text order fixtures pass (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S05 | 3 h | Define map entry/key/value mapping; duplicate-key and entry-presence rules have fixtures (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S06 | 3 h | Define variant/type tags; ambiguous or conflicting tags reject during generation (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S07 | 3 h | Define XML reject/skip/preserve extension slots; namespace context and required-capability fixtures pass (2.10 defined shared-contract subset; [evidence](verification/verification-xml-shared-contracts-2026-10-19.md)). | S01 |
| [x] | S08 | 4 h | Compile numeric inclusive/exclusive facets into IR; exact boundary tests pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| [x] | S09 | 3 h | Compile Unicode length/enumeration facets; empty/code-point/enumeration tests pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| [x] | S10 | 3 h | Select an explicit portable XSD-regex subset; accepted/rejected corpus and unsupported diagnostics pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S09 |
| [x] | S11 | 4 h | Compile only that regex subset to portable checks; independent reference matches agree (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S10 |
| [x] | S12 | 3 h | Derive simpleContent/text and wrappers from XSD; authoritative binding/conflict tests pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| [x] | S13 | 3 h | Compile choice models and ambiguity diagnostics; alternative/occurrence fixtures pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S14 | 3 h | Compile all models; permutation and duplicate fixtures pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S15 | 3 h | Define supported identity selector/field XPath; unsupported expressions reject (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| [x] | S16 | 4 h | Compile key/unique scopes and tuples; duplicate/incomplete-key fixtures pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S15 |
| [x] | S17 | 3 h | Resolve keyref targets/types; missing and wrong-scope references reject (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S16 |
| [x] | S18 | 3 h | Implement chameleon namespace adoption; one file adopted into two namespaces stays isolated (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing bundle loader |
| [x] | S19 | 3 h | Handle finite cyclic bundle graphs; legal cycles deduplicate and budgets still bound growth (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S18 |
| [x] | S20 | 3 h | Bind anonymous nested complex types; ambiguous associations reject (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing XSD binder |
| [x] | S21 | 3 h | Export text/wrappers; independent validation and reimport agree (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S12 |
| [x] | S22 | 3 h | Export enumerations/facets; independent valid/invalid document checks agree (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S08, S09, S10, S11 |
| [x] | S23 | 4 h | Export multiple namespaces as a pinned bundle; reimport preserves names and dependencies (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing export |
| [x] | S24 | 4 h | Import a scalar/attribute XSD subset into Serializer declarations; generated schemas compile and round-trip (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S08, S09, S10, S11, S12 |
| [x] | S25 | 4 h | Extend import to named nested records/collections; nested/include fixtures compile (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S24 |
| [x] | S26 | 3 h | Extend compatibility diagnostics to added/retired fields/models; both comparison directions have tests (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing compare tool |
| [x] | S27 | 3 h | Define concrete generic XML bindings; distinct instances and ambiguous names have tests (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S28 | 3 h | Define inherited field order/flattening; collisions and ordering have tests (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S29 | 3 h | Define revision/magic XML contracts; old/current/invalid layouts have fixtures (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S30 | 3 h | Define digest and managed/view data projections; ownership/lifetime and unsupported states are explicit (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S31 | 4 h | Define generated compression/package profile IR/configuration; defaults and runtime overrides validate (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing package APIs |
| [x] | S32 | 3 h | Generate vocabulary/expected-type dispatch metadata; wrong roots/types select no decoder (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S31, S06 |
| [x] | S33 | 3 h | Define separate HTML syntax/provider boundaries; HTML and XML fixtures remain distinct (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S01 |
| [x] | S34 | 4 h | Implement the approved minimal HTML event/output subset; independent HTML checks pass (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | S33 |
| [x] | S35 | 3 h | Maintain a shared producer/consumer fixture manifest; all eleven languages are represented and missing SDKs fail (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Existing SDK runners |
| [x] | S36 | 2 h | Apply release, README, usage, migration and skill updates; review editor impact and verify affected packages when touched (2.11 defined shared-contract subset; [evidence](verification/verification-xml-advanced-contracts-2026-10-19.md)). | Each landed slice |

### XML tasks for each language

#### C++ XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CP-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | CP-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | CP-X01 |
| [x] | CP-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | CP-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | CP-X03 |
| [x] | CP-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, CP-X04 |
| [x] | CP-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, CP-X02, CP-X04 |
| [x] | CP-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, CP-X04 |
| [x] | CP-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, CP-X04 |
| [x] | CP-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, CP-X04 |
| [x] | CP-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, CP-X04 |
| [x] | CP-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, CP-X04 |
| [x] | CP-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, CP-X04 |
| [x] | CP-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, CP-X04 |
| [x] | CP-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, CP-X04 |
| [x] | CP-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, CP-X14 |
| [x] | CP-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, CP-X04 |
| [x] | CP-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, CP-X04 |
| [x] | CP-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, CP-X04 |
| [x] | CP-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, CP-X04 |
| [x] | CP-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, CP-X04 |
| [x] | CP-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | CP-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C++ XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Python XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | PY-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | PY-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | PY-X01 |
| [x] | PY-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | PY-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | PY-X03 |
| [x] | PY-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, PY-X04 |
| [x] | PY-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, PY-X02, PY-X04 |
| [x] | PY-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, PY-X04 |
| [x] | PY-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, PY-X04 |
| [x] | PY-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, PY-X04 |
| [x] | PY-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, PY-X04 |
| [x] | PY-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, PY-X04 |
| [x] | PY-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, PY-X04 |
| [x] | PY-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, PY-X04 |
| [x] | PY-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, PY-X04 |
| [x] | PY-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, PY-X14 |
| [x] | PY-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, PY-X04 |
| [x] | PY-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, PY-X04 |
| [x] | PY-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, PY-X04 |
| [x] | PY-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, PY-X04 |
| [x] | PY-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, PY-X04 |
| [x] | PY-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | PY-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Python XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Java XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JV-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | JV-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | JV-X01 |
| [x] | JV-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | JV-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | JV-X03 |
| [x] | JV-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, JV-X04 |
| [x] | JV-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, JV-X02, JV-X04 |
| [x] | JV-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, JV-X04 |
| [x] | JV-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, JV-X04 |
| [x] | JV-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, JV-X04 |
| [x] | JV-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, JV-X04 |
| [x] | JV-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, JV-X04 |
| [x] | JV-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, JV-X04 |
| [x] | JV-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, JV-X04 |
| [x] | JV-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, JV-X04 |
| [x] | JV-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, JV-X14 |
| [x] | JV-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, JV-X04 |
| [x] | JV-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, JV-X04 |
| [x] | JV-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, JV-X04 |
| [x] | JV-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, JV-X04 |
| [x] | JV-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, JV-X04 |
| [x] | JV-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | JV-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Java XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Kotlin XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | KT-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | KT-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | KT-X01 |
| [x] | KT-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | KT-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | KT-X03 |
| [x] | KT-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, KT-X04 |
| [x] | KT-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, KT-X02, KT-X04 |
| [x] | KT-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, KT-X04 |
| [x] | KT-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, KT-X04 |
| [x] | KT-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, KT-X04 |
| [x] | KT-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, KT-X04 |
| [x] | KT-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, KT-X04 |
| [x] | KT-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, KT-X04 |
| [x] | KT-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, KT-X04 |
| [x] | KT-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, KT-X04 |
| [x] | KT-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, KT-X14 |
| [x] | KT-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, KT-X04 |
| [x] | KT-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, KT-X04 |
| [x] | KT-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, KT-X04 |
| [x] | KT-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, KT-X04 |
| [x] | KT-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, KT-X04 |
| [x] | KT-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | KT-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Kotlin XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### C# XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CS-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | CS-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | CS-X01 |
| [x] | CS-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | CS-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | CS-X03 |
| [x] | CS-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, CS-X04 |
| [x] | CS-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, CS-X02, CS-X04 |
| [x] | CS-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, CS-X04 |
| [x] | CS-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, CS-X04 |
| [x] | CS-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, CS-X04 |
| [x] | CS-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, CS-X04 |
| [x] | CS-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, CS-X04 |
| [x] | CS-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, CS-X04 |
| [x] | CS-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, CS-X04 |
| [x] | CS-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, CS-X04 |
| [x] | CS-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, CS-X14 |
| [x] | CS-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, CS-X04 |
| [x] | CS-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, CS-X04 |
| [x] | CS-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, CS-X04 |
| [x] | CS-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, CS-X04 |
| [x] | CS-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, CS-X04 |
| [x] | CS-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | CS-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C# XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Go XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | GO-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | GO-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | GO-X01 |
| [x] | GO-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | GO-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | GO-X03 |
| [x] | GO-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, GO-X04 |
| [x] | GO-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, GO-X02, GO-X04 |
| [x] | GO-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, GO-X04 |
| [x] | GO-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, GO-X04 |
| [x] | GO-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, GO-X04 |
| [x] | GO-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, GO-X04 |
| [x] | GO-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, GO-X04 |
| [x] | GO-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, GO-X04 |
| [x] | GO-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, GO-X04 |
| [x] | GO-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, GO-X04 |
| [x] | GO-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, GO-X14 |
| [x] | GO-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, GO-X04 |
| [x] | GO-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, GO-X04 |
| [x] | GO-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, GO-X04 |
| [x] | GO-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, GO-X04 |
| [x] | GO-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, GO-X04 |
| [x] | GO-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | GO-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Go XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### JavaScript XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JS-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | JS-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | JS-X01 |
| [x] | JS-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | JS-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | JS-X03 |
| [x] | JS-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, JS-X04 |
| [x] | JS-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, JS-X02, JS-X04 |
| [x] | JS-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, JS-X04 |
| [x] | JS-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, JS-X04 |
| [x] | JS-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, JS-X04 |
| [x] | JS-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, JS-X04 |
| [x] | JS-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, JS-X04 |
| [x] | JS-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, JS-X04 |
| [x] | JS-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, JS-X04 |
| [x] | JS-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, JS-X04 |
| [x] | JS-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, JS-X14 |
| [x] | JS-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, JS-X04 |
| [x] | JS-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, JS-X04 |
| [x] | JS-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, JS-X04 |
| [x] | JS-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, JS-X04 |
| [x] | JS-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, JS-X04 |
| [x] | JS-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | JS-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected JavaScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### TypeScript XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | TS-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | TS-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | TS-X01 |
| [x] | TS-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | TS-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | TS-X03 |
| [x] | TS-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, TS-X04 |
| [x] | TS-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, TS-X02, TS-X04 |
| [x] | TS-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, TS-X04 |
| [x] | TS-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, TS-X04 |
| [x] | TS-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, TS-X04 |
| [x] | TS-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, TS-X04 |
| [x] | TS-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, TS-X04 |
| [x] | TS-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, TS-X04 |
| [x] | TS-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, TS-X04 |
| [x] | TS-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, TS-X04 |
| [x] | TS-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, TS-X14 |
| [x] | TS-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, TS-X04 |
| [x] | TS-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, TS-X04 |
| [x] | TS-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, TS-X04 |
| [x] | TS-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, TS-X04 |
| [x] | TS-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, TS-X04 |
| [x] | TS-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | TS-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected TypeScript XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Rust XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | RS-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | RS-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | RS-X01 |
| [x] | RS-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | RS-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | RS-X03 |
| [x] | RS-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, RS-X04 |
| [x] | RS-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, RS-X02, RS-X04 |
| [x] | RS-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, RS-X04 |
| [x] | RS-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, RS-X04 |
| [x] | RS-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, RS-X04 |
| [x] | RS-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, RS-X04 |
| [x] | RS-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, RS-X04 |
| [x] | RS-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, RS-X04 |
| [x] | RS-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, RS-X04 |
| [x] | RS-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, RS-X04 |
| [x] | RS-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, RS-X14 |
| [x] | RS-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, RS-X04 |
| [x] | RS-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, RS-X04 |
| [x] | RS-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, RS-X04 |
| [x] | RS-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, RS-X04 |
| [x] | RS-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, RS-X04 |
| [x] | RS-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | RS-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Rust XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### Swift XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SW-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | SW-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | SW-X01 |
| [x] | SW-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | SW-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | SW-X03 |
| [x] | SW-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, SW-X04 |
| [x] | SW-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, SW-X02, SW-X04 |
| [x] | SW-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, SW-X04 |
| [x] | SW-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, SW-X04 |
| [x] | SW-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, SW-X04 |
| [x] | SW-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, SW-X04 |
| [x] | SW-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, SW-X04 |
| [x] | SW-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, SW-X04 |
| [x] | SW-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, SW-X04 |
| [x] | SW-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, SW-X04 |
| [x] | SW-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, SW-X14 |
| [x] | SW-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, SW-X04 |
| [x] | SW-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, SW-X04 |
| [x] | SW-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, SW-X04 |
| [x] | SW-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, SW-X04 |
| [x] | SW-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, SW-X04 |
| [x] | SW-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | SW-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected Swift XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

#### C XML

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | C-X01 | 4 h | Direct scalar token output; escaping/namespace traces pass without a retained tree (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | C-X02 | 4 h | Direct generated record/attribute/nested output; contract and output bounds pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | C-X01 |
| [x] | C-X03 | 4 h | Scalar token input; split-token lexical/resource failures pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S01, S02 |
| [x] | C-X04 | 4 h | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | C-X03 |
| [x] | C-X05 | 3 h | Explicit nil mapping; absent, empty and nil remain distinguishable (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S03, C-X04 |
| [x] | C-X06 | 4 h | Ordered mixed content; ordering, escaping and resource tests pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S04, C-X02, C-X04 |
| [x] | C-X07 | 3 h | Map mapping; key, duplicate and cardinality fixtures pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S05, C-X04 |
| [x] | C-X08 | 3 h | Variant mapping; wrong tag/type combinations reject (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S06, C-X04 |
| [x] | C-X09 | 3 h | Generated numeric facets; inclusive/exclusive boundary corpus passes (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S08, C-X04 |
| [x] | C-X10 | 3 h | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S09, S11, C-X04 |
| [x] | C-X11 | 3 h | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S12, C-X04 |
| [x] | C-X12 | 3 h | Choice traversal; exactly one allowed alternative is accepted (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S13, C-X04 |
| [x] | C-X13 | 3 h | All traversal; valid permutations pass and duplicates reject (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S14, C-X04 |
| [x] | C-X14 | 3 h | Key/unique state; tuple/index budgets and failure tests pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S16, C-X04 |
| [x] | C-X15 | 3 h | Keyref completion; forward/missing/wrong-scope references are checked before publication (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S17, C-X14 |
| [x] | C-X16 | 4 h | XML extension slots; reject/skip/preserve and namespace-context round-trips pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S07, C-X04 |
| [x] | C-X17 | 3 h | Concrete generic XML instances; generated instance fixtures pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S27, C-X04 |
| [x] | C-X18 | 3 h | Inherited mappings; field collisions and ordering tests pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S28, C-X04 |
| [x] | C-X19 | 4 h | Revision/magic mappings; historical/current/invalid layouts pass their expected checks (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S29, C-X04 |
| [x] | C-X20 | 3 h | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S30, C-X04 |
| [x] | C-X21 | 3 h | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | Provider prerequisites |
| [x] | C-X22 | 4 h | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing (2.12 selected C XML reference profile; [evidence](verification/verification-xml-tokens-2026-10-19.md)). | S34 |

### Compression tasks for each language

#### C++ compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CP-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | S31 |
| [x] | CP-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z01 |
| [x] | CP-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows C++/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | CP-Z02 |
| [x] | CP-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures; [qualification](verification/verification-compression-parity-2026-10-20.md). | CP-Z03, CP-Z04, CP-Z05, CP-Z06, CP-Z07, CP-Z08, CP-Z09, CP-Z10 |

#### Python compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | PY-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | S31 |
| [x] | PY-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z01 |
| [x] | PY-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 selected Windows Python/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | PY-Z02 |
| [x] | PY-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | PY-Z03, PY-Z04, PY-Z05, PY-Z06, PY-Z07, PY-Z08, PY-Z09, PY-Z10 |

#### Java compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JV-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | S31 |
| [x] | JV-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z01 |
| [x] | JV-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 selected Windows Java/provider profile; [evidence](verification/verification-backlog-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 Windows JDK 26 optional-provider profiles; [evidence](verification/verification-java-compression-providers-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 Windows JDK 26 optional-provider profiles; [evidence](verification/verification-java-compression-providers-2026-10-20.md)). | JV-Z02 |
| [x] | JV-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | JV-Z03, JV-Z04, JV-Z05, JV-Z06, JV-Z07, JV-Z08, JV-Z09, JV-Z10 |

#### Kotlin compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | KT-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | S31 |
| [x] | KT-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z01 |
| [x] | KT-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Windows Kotlin/JVM 17 profile; [evidence](verification/verification-kotlin-compression-2026-10-20.md)). | KT-Z02 |
| [x] | KT-Z09 | 4 h | Optional Zstandard adapter; lazy runtime jar, present/absent/failed-native admission, independent frames and actual Windows JVM17/Linux JRE replay pass; [evidence](verification/verification-kotlin-compression-providers-2026-10-20.md). | KT-Z02 |
| [x] | KT-Z10 | 4 h | Optional safe-Java LZ4-frame adapter; linked history, checksums, present/absent deployment, independent frames and actual Windows JVM17/Linux JRE replay pass; [evidence](verification/verification-kotlin-compression-providers-2026-10-20.md). | KT-Z02 |
| [x] | KT-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | KT-Z03, KT-Z04, KT-Z05, KT-Z06, KT-Z07, KT-Z08, KT-Z09, KT-Z10 |

#### C# compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CS-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | S31 |
| [x] | CS-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z01 |
| [x] | CS-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 generated .NET 8 profile; [evidence](verification/verification-csharp-compression-2026-10-20.md)). | CS-Z02 |
| [x] | CS-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-csharp-compression-providers-2026-10-20.md). | CS-Z02 |
| [x] | CS-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-csharp-compression-providers-2026-10-20.md). | CS-Z02 |
| [x] | CS-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | CS-Z03, CS-Z04, CS-Z05, CS-Z06, CS-Z07, CS-Z08, CS-Z09, CS-Z10 |

#### Go compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | GO-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | S31 |
| [x] | GO-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z01 |
| [x] | GO-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Windows Go 1.26.4/Linux Go 1.26.0 profiles; [evidence](verification/verification-go-compression-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 actual Go 1.26/pinned module profile; [evidence](verification/verification-go-compression-providers-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 actual Go 1.26/pinned module profile; [evidence](verification/verification-go-compression-providers-2026-10-20.md)). | GO-Z02 |
| [x] | GO-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | GO-Z03, GO-Z04, GO-Z05, GO-Z06, GO-Z07, GO-Z08, GO-Z09, GO-Z10 |

#### JavaScript compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JS-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | S31 |
| [x] | JS-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z01 |
| [x] | JS-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | JS-Z02 |
| [x] | JS-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-javascript-compression-providers-2026-10-20.md). | JS-Z02 |
| [x] | JS-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-javascript-compression-providers-2026-10-20.md). | JS-Z02 |
| [x] | JS-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | JS-Z03, JS-Z04, JS-Z05, JS-Z06, JS-Z07, JS-Z08, JS-Z09, JS-Z10 |

#### TypeScript compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | TS-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | S31 |
| [x] | TS-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z01 |
| [x] | TS-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 Node 22/strict TypeScript profile; [evidence](verification/verification-javascript-compression-2026-10-20.md)). | TS-Z02 |
| [x] | TS-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-javascript-compression-providers-2026-10-20.md). | TS-Z02 |
| [x] | TS-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected 2.14 SDK profile; [evidence](verification/verification-javascript-compression-providers-2026-10-20.md). | TS-Z02 |
| [x] | TS-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | TS-Z03, TS-Z04, TS-Z05, TS-Z06, TS-Z07, TS-Z08, TS-Z09, TS-Z10 |

#### Rust compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | RS-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | S31 |
| [x] | RS-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z01 |
| [x] | RS-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 actual Windows/native Linux Rust SDK profiles; [evidence](verification/verification-rust-compression-2026-10-20.md)). | RS-Z02 |
| [x] | RS-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux2.14 SDK profiles; [evidence](verification/verification-rust-compression-providers-2026-10-20.md). | RS-Z02 |
| [x] | RS-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux2.14 SDK profiles; [evidence](verification/verification-rust-compression-providers-2026-10-20.md). | RS-Z02 |
| [x] | RS-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | RS-Z03, RS-Z04, RS-Z05, RS-Z06, RS-Z07, RS-Z08, RS-Z09, RS-Z10 |

#### Swift compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SW-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | S31 |
| [x] | SW-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z01 |
| [x] | SW-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 native Linux Swift 6.1.3 profile; [evidence](verification/verification-swift-compression-2026-10-20.md)). | SW-Z02 |
| [x] | SW-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux 2.14 SDK profiles; [evidence](verification/verification-swift-compression-providers-2026-10-20.md). | SW-Z02 |
| [x] | SW-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. Selected Windows/Linux 2.14 SDK profiles; [evidence](verification/verification-swift-compression-providers-2026-10-20.md). | SW-Z02 |
| [x] | SW-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | SW-Z03, SW-Z04, SW-Z05, SW-Z06, SW-Z07, SW-Z08, SW-Z09, SW-Z10 |

#### C compression

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | C-Z01 | 2 h | Provider/API and dependency-absence behavior; unavailable providers fail clearly (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | S31 |
| [x] | C-Z02 | 3 h | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z01 |
| [x] | C-Z03 | 3 h | Gzip output; an independent decompressor accepts it (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z04 | 3 h | Gzip input; malformed, concatenated and expansion-boundary fixtures pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z05 | 3 h | Zlib output; an independent decompressor accepts it (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z06 | 3 h | Zlib input; framing/checksum boundary fixtures pass (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z07 | 3 h | Raw-DEFLATE output; independent decompression agrees (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z08 | 3 h | Raw-DEFLATE input; truncation and trailing-input cases reject (2.14 native Windows/Linux C11 SDK profiles; [evidence](verification/verification-c-compression-2026-10-20.md)). | C-Z02 |
| [x] | C-Z09 | 4 h | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass (2.14 actual Windows/Linux C11 optional-provider profiles; [evidence](verification/verification-c-compression-providers-2026-10-20.md)). | C-Z02 |
| [x] | C-Z10 | 4 h | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass (2.14 actual Windows/Linux C11 optional-provider profiles; [evidence](verification/verification-c-compression-providers-2026-10-20.md)). | C-Z02 |
| [x] | C-Z11 | 4 h | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. Selected Windows SDK/provider profiles; [evidence](verification/verification-compression-parity-2026-10-20.md). | C-Z03, C-Z04, C-Z05, C-Z06, C-Z07, C-Z08, C-Z09, C-Z10 |

### Package tasks for each language

#### C++ packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CP-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | S31 |
| [x] | CP-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P01 |
| [x] | CP-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P01 |
| [x] | CP-P04 | 3 h | DEFLATE entry output; independent inflation agrees (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P02, CP-Z07 |
| [x] | CP-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P03, CP-Z08 |
| [x] | CP-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P02 |
| [x] | CP-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P03 |
| [x] | CP-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P06 |
| [x] | CP-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 Windows/Linux reference profiles, sparse offsets above 4 GiB and real 65K counts; [evidence](verification/verification-package-zip64-input-2026-10-20.md)). | CP-P07 |
| [x] | CP-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 native Windows/Linux reference profiles, independent signature-CRC controls; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | CP-P05, CP-P07 |
| [x] | CP-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 native Windows/Linux profiles; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | CP-P03 |
| [x] | CP-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 native Windows/Linux static profiles; [evidence](verification/verification-package-save-faults-2026-10-20.md)). | CP-P02 |
| [x] | CP-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P03 |
| [x] | CP-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P13 |
| [x] | CP-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 generated C++ Windows/Linux reference profiles; [evidence](verification/verification-package-profiles-2026-10-20.md)). | CP-P14, S32 |
| [x] | CP-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | CP-P15 |
| [x] | CP-P16a | — | Generated C++ package/XML operations share OPC-edge and supported XSD key/unique/keyref tuple admission. Instrumented native validation/reference admission retains part/field/link diagnostics; caller exception types and atomic load/Save behavior pass focused regression. Deferred presentation/provider attribution keeps CP-P16 open; [2.16 batch](verification/verification-pending-batch-2026-10-20.md). | CP-P15 |
| [x] | CP-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 native Windows/Linux reference profiles; [evidence](verification/verification-package-profiles-2026-10-20.md)). | CP-P15 |
| [x] | CP-P18 | 3 h | Preserved whole-part namespace/link context; edited-package references are revalidated (2.14 native Windows/Linux reference profiles; [evidence](verification/verification-package-profiles-2026-10-20.md)). | CP-P17 |
| [x] | CP-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected C++ reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | CP-P02, CP-P14 |
| [x] | CP-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, CP-P15 |
| [x] | CP-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | CP-P16, CP-P20 |

#### Python packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | PY-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | S31 |
| [x] | PY-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | PY-P01 |
| [x] | PY-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P01 |
| [x] | PY-P04 | 3 h | DEFLATE entry output; independent inflation agrees (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P02, PY-Z07 |
| [x] | PY-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P03, PY-Z08 |
| [x] | PY-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P02 |
| [x] | PY-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P03 |
| [x] | PY-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P06 |
| [x] | PY-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 Windows/Linux reference profiles, sparse offsets above 4 GiB and real 65K counts; [evidence](verification/verification-package-zip64-input-2026-10-20.md)). | PY-P07 |
| [x] | PY-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P05, PY-P07 |
| [x] | PY-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | PY-P03 |
| [x] | PY-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | PY-P02 |
| [x] | PY-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P03 |
| [x] | PY-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Python reference profile; [evidence](verification/verification-package-reference-2026-10-20.md)). | PY-P13 |
| [x] | PY-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-profiles-2026-10-20.md)). | PY-P14, S32 |
| [x] | PY-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | PY-P15 |
| [x] | PY-P16a | — | Generated Python package/XML operations share OPC-edge and supported XSD identity-tuple admission, with instrumented field/constraint/link diagnostics, namespace-admission forwarding and same-instance caller exceptions. Package37/37, identity6/6 and rich17/17 checks pass, including atomic load/Save. Deferred presentation/provider attribution keeps PY-P16 open; [2.16 batch](verification/verification-pending-batch-2026-10-20.md). | PY-P15 |
| [x] | PY-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-profiles-2026-10-20.md)). | PY-P15 |
| [x] | PY-P18 | 3 h | Preserved whole-part namespace/link context; edited-package references are revalidated (2.14 Windows/Linux generated reference profile; [evidence](verification/verification-package-profiles-2026-10-20.md)). | PY-P17 |
| [x] | PY-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 Windows/Linux unordered part/link sets with stable identities; [evidence](verification/verification-package-lifecycle-2026-10-20.md)). | PY-P02, PY-P14 |
| [x] | PY-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, PY-P15 |
| [x] | PY-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | PY-P16, PY-P20 |

#### Java packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JV-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | S31 |
| [x] | JV-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P01 |
| [x] | JV-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P01 |
| [x] | JV-P04 | 3 h | DEFLATE entry output; independent inflation agrees (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P02, JV-Z07 |
| [x] | JV-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P03, JV-Z08 |
| [x] | JV-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P02 |
| [x] | JV-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| [x] | JV-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P06 |
| [x] | JV-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P07 |
| [x] | JV-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P05, JV-P07 |
| [x] | JV-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| [x] | JV-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P02 |
| [x] | JV-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P03 |
| [x] | JV-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P13 |
| [x] | JV-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P14, S32 |
| [x] | JV-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | JV-P15 |
| [x] | JV-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P15 |
| [x] | JV-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P17 |
| [x] | JV-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 actual Java17 SDK and selected LinuxJRE runtime profile; [evidence](verification/verification-java-packages-2026-10-20.md)). | JV-P02, JV-P14 |
| [x] | JV-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, JV-P15 |
| [x] | JV-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | JV-P16, JV-P20 |

#### Kotlin packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | KT-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | S31 |
| [x] | KT-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P01 |
| [x] | KT-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P01 |
| [x] | KT-P04 | 3 h | DEFLATE entry output; independent inflation agrees. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P02, KT-Z07 |
| [x] | KT-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P03, KT-Z08 |
| [x] | KT-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P02 |
| [x] | KT-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| [x] | KT-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P06 |
| [x] | KT-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P07 |
| [x] | KT-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P05, KT-P07 |
| [x] | KT-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| [x] | KT-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P02 |
| [x] | KT-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P03 |
| [x] | KT-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P13 |
| [x] | KT-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P14, S32 |
| [x] | KT-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P15 |
| [x] | KT-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P15 |
| [x] | KT-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P17 |
| [x] | KT-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/Kotlin-JVM 17 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | KT-P02, KT-P14 |
| [x] | KT-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, KT-P15 |
| [x] | KT-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | KT-P16, KT-P20 |

#### C# packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CS-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | S31 |
| [x] | CS-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P01 |
| [x] | CS-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P01 |
| [x] | CS-P04 | 3 h | DEFLATE entry output; independent inflation agrees. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P02, CS-Z07 |
| [x] | CS-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P03, CS-Z08 |
| [x] | CS-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P02 |
| [x] | CS-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| [x] | CS-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P06 |
| [x] | CS-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P07 |
| [x] | CS-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P05, CS-P07 |
| [x] | CS-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| [x] | CS-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P02 |
| [x] | CS-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P03 |
| [x] | CS-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P13 |
| [x] | CS-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P14, S32 |
| [x] | CS-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P15 |
| [x] | CS-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P15 |
| [x] | CS-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P17 |
| [x] | CS-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/.NET 8 owning profile; [evidence](verification/verification-pending-50-2026-10-20.md). | CS-P02, CS-P14 |
| [x] | CS-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, CS-P15 |
| [x] | CS-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | CS-P16, CS-P20 |

#### Go packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | GO-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | S31 |
| [x] | GO-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P01 |
| [x] | GO-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P01 |
| [x] | GO-P04 | 3 h | DEFLATE entry output; independent inflation agrees. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P02, GO-Z07 |
| [x] | GO-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P03, GO-Z08 |
| [x] | GO-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P02 |
| [x] | GO-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P03 |
| [x] | GO-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P06 |
| [x] | GO-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P07 |
| [x] | GO-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P05, GO-P07 |
| [x] | GO-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P03 |
| [x] | GO-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file. Selected 2.14 SDK profile; [evidence](verification/verification-go-packages-2026-10-20.md). | GO-P02 |
| [x] | GO-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P03 |
| [x] | GO-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P13 |
| [x] | GO-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P14, S32 |
| [x] | GO-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | GO-P15 |
| [x] | GO-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P15 |
| [x] | GO-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P17 |
| [x] | GO-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations. Selected Windows/Linux Go 1.22-compatible OPC profile; [evidence](verification/verification-go-opc-packages-2026-10-20.md). | GO-P02, GO-P14 |
| [x] | GO-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, GO-P15 |
| [x] | GO-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | GO-P16, GO-P20 |

#### JavaScript packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JS-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | S31 |
| [x] | JS-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P01 |
| [x] | JS-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P01 |
| [x] | JS-P04 | 3 h | DEFLATE entry output; independent inflation agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P02, JS-Z07 |
| [x] | JS-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P03, JS-Z08 |
| [x] | JS-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P02 |
| [x] | JS-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| [x] | JS-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P06 |
| [x] | JS-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P07 |
| [x] | JS-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P05, JS-P07 |
| [x] | JS-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| [x] | JS-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P02 |
| [x] | JS-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P03 |
| [x] | JS-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P13 |
| [x] | JS-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P14, S32 |
| [x] | JS-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | JS-P15 |
| [x] | JS-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P15 |
| [x] | JS-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P17 |
| [x] | JS-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | JS-P02, JS-P14 |
| [x] | JS-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, JS-P15 |
| [x] | JS-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | JS-P16, JS-P20 |

#### TypeScript packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | TS-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | S31 |
| [x] | TS-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P01 |
| [x] | TS-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P01 |
| [x] | TS-P04 | 3 h | DEFLATE entry output; independent inflation agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P02, TS-Z07 |
| [x] | TS-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P03, TS-Z08 |
| [x] | TS-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P02 |
| [x] | TS-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| [x] | TS-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P06 |
| [x] | TS-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P07 |
| [x] | TS-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P05, TS-P07 |
| [x] | TS-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| [x] | TS-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P02 |
| [x] | TS-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P03 |
| [x] | TS-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P13 |
| [x] | TS-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P14, S32 |
| [x] | TS-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | TS-P15 |
| [x] | TS-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P15 |
| [x] | TS-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P17 |
| [x] | TS-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations (2.14 selected Node/strict TypeScript and Windows filesystem profile; [evidence](verification/verification-javascript-packages-2026-10-20.md)). | TS-P02, TS-P14 |
| [x] | TS-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, TS-P15 |
| [x] | TS-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | TS-P16, TS-P20 |

#### Rust packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | RS-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31 |
| [x] | RS-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P01 |
| [x] | RS-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P01 |
| [x] | RS-P04 | 3 h | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P02, RS-Z07 |
| [x] | RS-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P03, RS-Z08 |
| [x] | RS-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P02 |
| [x] | RS-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| [x] | RS-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P06 |
| [x] | RS-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P07 |
| [x] | RS-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P05, RS-P07 |
| [x] | RS-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| [x] | RS-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P02 |
| [x] | RS-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P03 |
| [x] | RS-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P13 |
| [x] | RS-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P14, S32 |
| [x] | RS-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P15 |
| [x] | RS-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P15 |
| [x] | RS-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P17 |
| [x] | RS-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P02, RS-P14 |
| [x] | RS-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, RS-P15 |
| [x] | RS-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | RS-P16, RS-P20 |

#### Swift packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SW-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31 |
| [x] | SW-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P01 |
| [x] | SW-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P01 |
| [x] | SW-P04 | 3 h | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P02, SW-Z07 |
| [x] | SW-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P03, SW-Z08 |
| [x] | SW-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P02 |
| [x] | SW-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| [x] | SW-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P06 |
| [x] | SW-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P07 |
| [x] | SW-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P05, SW-P07 |
| [x] | SW-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| [x] | SW-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P02 |
| [x] | SW-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P03 |
| [x] | SW-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P13 |
| [x] | SW-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P14, S32 |
| [x] | SW-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P15 |
| [x] | SW-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P15 |
| [x] | SW-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P17 |
| [x] | SW-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P02, SW-P14 |
| [x] | SW-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, SW-P15 |
| [x] | SW-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | SW-P16, SW-P20 |

#### C packages

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | C-P01 | 3 h | Source/sink/metadata/shared-budget foundation; source boundaries have tests; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31 |
| [x] | C-P02 | 3 h | Stored ZIP writer; an independent ZIP reader opens its output; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P01 |
| [x] | C-P03 | 4 h | ZIP32 directory reader; independent and malformed-offset fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P01 |
| [x] | C-P04 | 3 h | DEFLATE entry output; independent inflation agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P02, C-Z07 |
| [x] | C-P05 | 3 h | DEFLATE entry input; CRC/size/trailing-data failures reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P03, C-Z08 |
| [x] | C-P06 | 3 h | ZIP64 entry writer; a forced-small independent fixture passes; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P02 |
| [x] | C-P07 | 3 h | ZIP64 entry reader; Python-produced fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P03 |
| [x] | C-P08 | 3 h | ZIP64 directory/locator output; an independent directory inspector agrees; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P06 |
| [x] | C-P09 | 3 h | ZIP64 directory/locator input; large-offset/count boundary fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P07 |
| [x] | C-P10 | 3 h | Descriptor forms; signed/unsigned 32/64-bit fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P05, C-P07 |
| [x] | C-P11 | 3 h | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P03 |
| [x] | C-P12 | 3 h | Staged full-Save replacement; injected faults leave an old or complete new file; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P02 |
| [x] | C-P13 | 3 h | OPC names/content types; normalization, duplicate and conflict fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P03 |
| [x] | C-P14 | 3 h | Scoped relationships/targets; dangling and escaping targets reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P13 |
| [x] | C-P15 | 3 h | Transactional typed aggregate loading; a middle-part failure publishes nothing; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P14, S32 |
| [x] | C-P16 | 3 h | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P15 |
| [x] | C-P17 | 3 h | Opaque part policies; optional/required reject/skip/preserve fixtures pass; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P15 |
| [x] | C-P18 | 3 h | Preserved relationship/namespace context; edited-package references are revalidated; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P17 |
| [x] | C-P19 | 3 h | Canonical ordering/metadata; independent byte equality survives input permutations; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P02, C-P14 |
| [x] | C-P20 | 3 h | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | S31, S32, C-P15 |
| [x] | C-P21 | 3 h | Generated application reference checks; duplicate/missing/wrong-type keys reject; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | C-P16, C-P20 |

### Qualification tasks for each language

#### C++ qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CP-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CP-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | CP-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CP-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CP-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected MSVC Windows owning workloads; [evidence](verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| [x] | CP-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected MSVC Windows process high-water residency; [evidence](verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| [x] | CP-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected MSVC Windows owning workloads; [evidence](verification/verification-cpp-xml-performance-2026-10-20.md). | — |
| [x] | CP-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform. Selected Windows/Linux owning profiles; [evidence](verification/verification-cpp-xml-performance-2026-10-20.md). | — |

#### Python qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | PY-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | PY-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | PY-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | PY-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | PY-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Python 3.14 Windows owning workloads; [evidence](verification/verification-python-xml-performance-2026-10-20.md). | — |
| [x] | PY-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Python 3.14 Windows owning workloads; [evidence](verification/verification-python-xml-performance-2026-10-20.md). | — |
| [x] | PY-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Python 3.14 Windows owning workloads; [evidence](verification/verification-python-xml-performance-2026-10-20.md). | — |
| [x] | PY-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform. Selected Windows/Linux owning profiles; [evidence](verification/verification-python-xml-performance-2026-10-20.md). | — |

#### Java qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JV-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JV-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | JV-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JV-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JV-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JV-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JV-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JV-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### Kotlin qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | KT-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | KT-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | KT-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | KT-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | KT-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | KT-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | KT-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | KT-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### C# qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | CS-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CS-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | CS-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CS-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | CS-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | CS-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | CS-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | CS-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### Go qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | GO-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | GO-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | GO-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | GO-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | GO-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | GO-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | GO-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | GO-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### JavaScript qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | JS-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JS-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | JS-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JS-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | JS-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JS-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JS-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | JS-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### TypeScript qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | TS-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | TS-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | TS-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | TS-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | TS-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | TS-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | TS-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-portable-xml-performance-2026-10-20.md). | — |
| [x] | TS-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### Rust qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | RS-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | RS-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | RS-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | RS-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | RS-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | RS-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | RS-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | RS-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### Swift qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SW-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | SW-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | SW-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | SW-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | SW-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | SW-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | SW-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | SW-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

#### C qualification

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | C-Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | C-Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |
| [x] | C-Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | C-Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. Selected Windows eleven-language profiles; [evidence](verification/verification-xml-exchange-2026-10-20.md). | — |
| [x] | C-Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | C-Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. Selected Windows process high-water residency and explicitly limited allocation counters; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | C-Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. Selected Windows owning workloads; [evidence](verification/verification-native-mobile-xml-performance-2026-10-20.md). | — |
| [x] | C-Q08 | 3 h | Supported-platform and reproducibility replay passes for every claimed platform; [2.17 qualified scope](verification/verification-pending-105-2026-10-21.md). | — |

### C++ document-state bridge tasks

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | D01 | 3 h | Saved-baseline identity/layout/digest format; altered-identity byte fixtures reject. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | S31 |
| [x] | D02 | 3 h | Sidecar generation manifest; stale/foreign documents fail before payload decoding. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D01 |
| [x] | D03 | 3 h | Object-ID snapshot restoration; IDs survive Save/reopen unchanged. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D01 |
| [x] | D04 | 3 h | Allocation high-water restoration; issued IDs are never reused. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D03 |
| [x] | D05 | 4 h | Retained-history checkpoint transfer; undo/redo survives reopen. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D03 |
| [x] | D06 | 3 h | Branch/cursor transfer; forked-history/current-cursor fixtures pass. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D05 |
| [x] | D07 | 3 h | Data-only document encoder; independent inspection finds no side-channel records. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D01, CP-P12 |
| [x] | D08 | 3 h | Stage document/sidecars under a new generation; staging failure preserves publication. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D02, D04, D05, D06, D07 |
| [x] | D09 | 3 h | Publication state machine/commit marker; every transition has a fixture. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D08 |
| [x] | D10 | 3 h | Windows durable replacement; injected faults preserve one coherent generation. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D09 |
| [x] | D11 | 3 h | Linux durable replacement; matching fault/synchronization tests pass. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D09 |
| [x] | D12 | 3 h | Precommit recovery; each interruption reopens the old baseline/history. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D10, D11 |
| [x] | D13 | 3 h | Postcommit recovery; each interruption reopens the new baseline/history. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D10, D11 |
| [x] | D14 | 3 h | Journal baseline fence/replay; mismatches reject before decoding/replay. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D02, D12, D13 |
| [x] | D15 | 3 h | Collaboration/session/outbox fences; pending state cannot cross saved baselines. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D02, D12, D13 |
| [x] | D16 | 3 h | Full Save/reopen integration; IDs/history/cursors/journal/session remain coherent. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D04, D05, D06, D12, D13, D14, D15 |
| [x] | D17 | 3 h | Legacy native compatibility replay; existing bytes/APIs retain their contract. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| [x] | D18 | 2 h | Final-file isolation audit; independent inspection finds no side channels. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| [x] | D19 | 3 h | Bounded pre-publication crash matrix; old-generation recovery passes. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| [x] | D20 | 3 h | Bounded post-publication crash matrix; new-generation recovery passes. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D16 |
| [x] | D21 | 2 h | Executable usage/migration example; Save/load instructions and limitations match results. Selected C++ native/external-JSON Windows/Linux profile; [evidence](verification/verification-managed-document-bridge-2026-10-20.md). | D17, D18, D19, D20 |
| [x] | D22 | 4 h | Checked first-time identified typed storage, stored document namespace and native allocation watermark; preserve IDs/references/burned gaps and one history baseline; validate before publication/scoped resource hooks, then qualify native Save/recovery (2.14 eight Windows/Linux C++ profiles; [evidence](verification/verification-managed-identified-baseline-2026-10-20.md)). | MR04, MR05 |
| [x] | D23 | — | Bounded caller-supplied external-document codecs and exact untouched-baseline attachment; identity/layout/digest binding rejects stale, foreign and lossy candidates (2.13 C++ JSON and selected binary callback profiles; [evidence](verification/verification-sidecar-journal-2026-10-20.md)). | Existing C++ managed runtime |
| [x] | D24 | — | Native v3 differential copy/literal sidecars retain identities, allocation marks, history and session state through selected Save/recovery and v2 upgrade; main files contain document data only (2.13 selected native document/journal profiles; [evidence](verification/verification-sidecar-journal-2026-10-20.md)). | D23 |

### Shared release gates

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | G01 | 2 h | Independent XML/XSD oracle corpus passes for the landed subset. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G02 | 2 h | Independent ZIP/OPC corpus passes for the landed subset. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G03 | 3 h | Current selected Windows source/configuration passes the native regression wrapper; failures are resolved. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G04 | 3 h | Current selected Linux source/configuration passes native regression with failures resolved. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G05 | 2 h | XML-disabled build and explicit unavailability behavior pass. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G06 | 2 h | Every bundled provider disabled: build/link and supported identity/stored paths pass. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G07 | 2 h | CLI version, release metadata and same-major CMake discovery compatibility pass. Selected 2.14 combined Windows/Linux profiles; [evidence](verification/verification-current-regression-2026-10-20.md). | — |
| [x] | G08 | 3 h | Editor impact is reviewed; if touched, both versions/changelogs/listings/packages are synchronized and validated; [packaged resolver verification](verification/verification-editor-field-navigation-2026-10-20.md). | — |
| [x] | G09 | 2 h | Coverage, README, skill, migration, links, ignore rules and task-artifact cleanup match executed results; [verified checkpoint](verification/verification-current-regression-2026-10-20.md). | — |
