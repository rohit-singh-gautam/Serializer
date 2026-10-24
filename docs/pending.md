# Pending requirements across Serializer

This list tracks **36 requirements: 26 closed and 10 unfinished
(7 active and three with deferred database work)**. Keep completed items in this file and mark their
state `[x]`; do not delete their rows or sections when they are completed.
The [implemented feature catalog](project/completed_requirements.md) retains
**838 IDs and verification records**, including historical items archived before
this tracking policy. The [feature list](project/feature_status.md)
summarizes supported capabilities. The three closed ownership decisions live in
[application responsibilities](project/application_responsibilities.md#disposed-requirements),
not in this requirement inventory.

`MG01` and native `MIG03` are included in the active implementation scope,
with their existing issues retained below. On 24 October 2026 the remaining
database-dependent portions of `PG01`, `PG02` and `DB02` were deferred at the
maintainer's direction; their implemented subsets and unfinished gates remain
recorded below. Deferral is not completion or a will-not-fix disposition.
Native storage support is active `DB02`; third-party database engines and sink
adapters are outside scope. Dependencies may refer to completed IDs in the
feature catalog. Keep partial parent requirements open even when a narrower
implementation or qualification record is complete. Explicit scope dispositions
and reconciliation of duplicate parents are documented separately from newly
implemented capabilities.

## First priority: C++23 minimum

C++23 is mandatory for Serializer itself, including standalone use. The migration
is complete within the recorded Windows/Linux toolchain and installed-consumer
scope; new schema-index and database implementation may build on this minimum.

| State | ID | Estimate | Requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [x] | CPP01 | — | **Completed.** Adopt the [C++23 minimum](design/database_storage_requirements.md#c23-minimum-and-one-qualified-dependency-source) for compiler, runtime, managed runtime, generated C++, provider/object targets and installed/exported targets. Synchronize build defaults, consumer guidance and toolchain checks; qualify supported Windows/Linux builds and installed consumers. Preserve schemas, public APIs and wire formats; no C++20 fallback is required. [Qualification and platform limits](verification/verification-cpp23-2026-10-23.md); [catalog record](project/completed_requirements.md#cpp01). | — |

## Managed references and migration

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | MR03 | — | Define and qualify managed token codecs for other generated outputs; keep these distinct from full native managed engines. Language 1.23 direct named generic self binding is qualified as [MR03a](project/completed_requirements.md#mr03a); shared nested/base and mutual named-generic selection remain restricted until their explicit contracts are implemented. | — |
| [ ] | MIG03 | — | **Active subsystem — [GitHub #5](https://github.com/rohit-singh-gautam/Serializer/issues/5), native portion.** Define versioned native journal/storage/schema migration and rebaseline separately from managed-save/raw-model conversion. The bounded native engine now qualifies [actual old/new schema conversion](verification/verification-native-schema-migration-2026-10-24.md) using stable class tags; broader native journal/history upgrades remain open. Breaking managed/storage redesign is allowed when required, with appropriate release/format versions and explicit conversion or rejection. Retain a recoverable source generation until publication succeeds; this safety copy does not require permanent legacy-format readers. Third-party sink conversion is excluded. | DB02 for native database profiles |

## Managed state and storage

These two managed sections retain ten parent requirements: **MG07 and MG08 are
completed**, and eight remain unfinished. See the [acceptance audit](verification/verification-managed-completions-2026-10-24.md). Current Windows
qualification includes guarded C++ dispatch, field/dependency notifications and
separate-value Protobuf wrappers; see the [executed matrix](verification/verification-managed-dispatch-notifications-2026-10-24.md).
The [managed state checkpoint](verification/verification-managed-state-2026-10-24.md)
maps all eight storage parents to implemented evidence and their remaining gates.
Passing subsets of the other eight parents leave those requirements open. The current
Windows checkpoint also qualifies allocation-aware payloads, native generation
migration and immutable persistent derived indexes; see the
[database](verification/verification-native-database-2026-10-24.md) and
[index](verification/verification-paged-index-2026-10-24.md) records. Broader
compiler, platform and capability qualification remains active. These are not
additional completed parent items.

The [combined storage/index specification](design/database_storage_requirements.md)
defines standalone native indexing, native database authority, independent
local/database/both index placement, explicit snapshot loading and the mandatory
C++23 minimum. The build minimum is qualified as
[CPP01](project/completed_requirements.md#cpp01); the [local scalar index subset](schema/indexes.md) and
[bounded eager engine](managed/native_database.md) are implemented separately.
Broader placement, persistent-index and lazy APIs remain proposed.
Native indexing can proceed independently of database paging work.

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | MG01 | — | **Active subsystem — [GitHub #1](https://github.com/rohit-singh-gautam/Serializer/issues/1).** Implement native managed engines for every non-C++ output: Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin and C. Require ownership, immutable reads, transactions, identity, history, journals and collaboration under each native lifetime/error/concurrency contract; token codecs alone do not close this requirement. [Language bindings](managed/language_bindings.md). | MR03 codecs before native engine exchange |
| [x] | MG07 | — | **Completed.** Resource-owning C++ payload containers, exact clone/decode, generated owning native binary domains and managed callback/history propagation provide hard requested-byte resource hooks. Current/peak domain counters remain distinct from resident admission and process RSS. The [qualified profile and acceptance evidence](verification/verification-managed-completions-2026-10-24.md#mg07-allocator-aware-payloads-and-hard-allocation-hooks) records supported shapes, explicit exclusions, 15 Windows runtime/generated cases and nine Linux kernel cases; additional platform/profile coverage is separate. | — |
| [x] | MG08 | — | **Completed.** The [managed representation/editor matrix](managed/representation_contracts.md) defines supported owning direct/separate storage, identities, private fields and distinct public bases, with explicit packed/view/recursive/ambiguous inheritance rejection. Direct managed Protobuf rejects before lowering; separate wrappers preserve golden bytes in four regenerated profiles. [Acceptance evidence](verification/verification-managed-completions-2026-10-24.md#mg08-supported-representation-and-editor-boundaries) includes 647 merged core cases and eight managed Protobuf cases, with unexecuted host/profile coverage stated separately. | — |
| [ ] | MG09 | — | Complete virtual/abstract/override edit dispatch and portable owned/borrowed method or typed-error contracts. Existing same-tag/identity variant field merging and selective collaboration Undo/Redo have [Windows/Linux qualification](verification/verification-managed-variants-2026-10-24.md), cataloged as MG09a; this does not close the broader method contract. See [State enhancements](managed/state_enhancements.md#remaining-work). | — |
| [ ] | MG10 | — | Extend projection/dependency notification policies and compact nested identity/history records where proposed; measure retained/deleted-entity costs and edit latency; [Identity/history](managed/history.md), [data structures](managed/data_structures.md). | — |
| [ ] | PG01 | — | **Database remainder deferred — [GitHub #2](https://github.com/rohit-singh-gautam/Serializer/issues/2).** The [immutable derived uint64 disk index](managed/paged_index.md) implements bounded external construction, page/cache pins, source binding, corruption checks and atomic generation replacement. Dirty authoritative-page mutation/writeback and incremental database recovery remain deferred until the native database page authority is implemented; [Paged indexing](design/paged_indexing.md). | DB02 incremental page authority |
| [ ] | PG02 | — | **Database-dependent work deferred — [GitHub #3](https://github.com/rohit-singh-gautam/Serializer/issues/3).** Add lazy value/storage loading and proposed paged capability selectors after authoritative database page lifetime, recovery and I/O failure contracts are implemented and measured. Current native snapshots pin complete eager roots, and derived index traversal does not supply lazy model storage; [Paged indexing](design/paged_indexing.md). | PG01; DB02 incremental page authority |
| [ ] | DB02 | — | **Remaining database work deferred — [GitHub #6](https://github.com/rohit-singh-gautam/Serializer/issues/6).** The [bounded eager native engine](managed/native_database.md), prepared durable acceptance, exact retries/recovery, immutable typed snapshots, native generation migration and standalone scalar/derived disk indexes are implemented within their recorded profiles. Complete the [combined storage/index contract](design/database_storage_requirements.md), including declared data/index placement, persistent database query integration and incremental authoritative page storage. No third-party database engine/sink dependency; standalone indexing remains independently usable. | CPP01 complete; PG01/PG02 for the incremental backend; MIG03 for broader migrations |

## Compiler and backend proposals

All six requirements are complete within the defined 2.34.0 backend/provider
profiles. See the [implemented contracts](guides/compiler_backend_updates.md),
[executed qualification and limits](verification/verification-compiler-backends-2026-10-24.md)
and [catalog records](project/completed_requirements.md#compiler-and-backend-contracts-in-234).
Earlier slices such as SC02a through SC02e and BE02a/BE02b retain their historical evidence.

| State | ID | Estimate | Requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [x] | SC02 | — | **Completed.** Language 1.24 broadens scalar default validation and defines unsigned C++ owning bit-field storage, preserving scalar wire bytes and qualifying decode ranges and directional compatibility with MSVC/GCC. Unsupported backend/storage/address mappings reject explicitly. | — |
| [x] | SC03 | — | **Completed.** Exact independent generic function/member calls and constructor/factory input defaults compile and execute with MSVC/GCC. Fixed-array Protobuf retains exact packed/unpacked/text cardinality and format-specific default rules; independent Google SDK exchanges pass. | — |
| [x] | BE01 | — | **Completed.** C/Rust/Python/Swift/Kotlin native dependency bundles preserve declaration order, naming, includes/modules, depfiles and editor navigation. All five actual SDK consumers pass; inline defaults and existing single-string APIs remain compatible. | — |
| [x] | BE02 | — | **Completed.** Java fixed-scalar packed buffers and opt-in direct native-codec OutputStream/channel APIs qualify wire parity, size patches, projections, caller ownership and failure bounds. Existing [BE02a](project/completed_requirements.md#be02a)/[BE02b](project/completed_requirements.md#be02b) array/NIO contracts remain independent. | — |
| [x] | BE03 | — | **Completed.** Actual Windows/POSIX borrowed mapped views qualify platform/lifetime/aliasing, persisted fixed-size edits, bounds and measured allocation/performance boundaries. Other formats require their own mapping contract; recorded timings do not promise a universal speedup. | — |
| [x] | OPC-OPT02 | — | **Completed.** A Windows Packaging API signature/canonicalization provider requires explicit pinned certificates and signed coverage. Independent native/.NET producers and validators qualify tampering, trust, coverage, removal/edit/resigning behavior; unsupported provider/algorithm profiles reject. | — |

## Qualification and distribution

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [x] | QA01 | — | **Completed qualification execution and triage for the documented profiles.** Four native C++23 toolchains, installed consumers and actual SDKs are replayed; Windows passes all 310 original commands across retained runs, Linux passes 284/288, and both hosts pass all 16 provider combinations/576 checks. Four strict generated-C failures remain open as QA-B02; full failed inventories are retained. WSL mounted-filesystem journal failures remain outside the passing native-filesystem profile. [Current results and exact limits](verification/verification-qualification-distribution-2026-10-24.md), [filesystem controls](verification/verification-document-origin-2026-10-22.md#toolchains-and-limits), [catalog](project/completed_requirements.md#qa01). | — |
| [x] | QA02 | — | **Completed corpus execution and triage for the documented implemented contracts.** Independent XSD tooling, malformed/resource/fault cases, eleven-SDK Windows XML/evolution/compression/ZIP/OPC inventories and the pinned 175-model Core corpus execute. Repaired Python Core outputs independently validate on Windows and Linux. Linux full inventories retain the strict-C failure; actual eight-SDK platform and ten-SDK package comparisons remain explicit diagnostics. QA-B01 and QA-B02 remain open; this execution does not certify the failed Linux C gate or deferred XML/package features. [Executed counts, hashes and scope](verification/verification-qualification-distribution-2026-10-24.md), [catalog](project/completed_requirements.md#qa02). | — |
| [x] | QA03 | — | **Completed for the documented reproducible Windows workloads.** All eleven SDKs pass 132 native operation profiles/660 wall samples and 220 independently checked timed ZIP/OPC archives, with generated/encoded sizes, wall/CPU and separately identified memory peaks. C++ retains 80 ordinary timing and 80 requested-allocation profiles; Python compares all six compression formats with independent provider baselines. Ordinary native/XML-disabled and plain ZIP controls pass. [Native measurements](verification/verification-native-baselines-2026-10-24.md), [package measurements and limits](verification/verification-package-baselines-2026-10-24.md), [catalog](project/completed_requirements.md#qa03). | — |
| [x] | QA04 | — | **Completed for the current Windows host profile.** Matching 2.0.19 packages build and validate; the installed Visual Studio assembly matches the local build. Actual VS Code and Visual Studio hosts qualify all eleven generated declaration outputs, reversed selections, live unsaved sources, includes and native command dispatch; real CMake/C++ caller checks and complete shared/native resolver checks pass. Repaired the absent-grammar declaration fallback without intercepting authored plain-text files. [Executed hosts, exact versions and limits](verification/verification-editor-hosts-2026-10-24.md#qa04-actual-editor-hosts-and-matching-distribution-payloads); [catalog](project/completed_requirements.md#qa04). No Marketplace publication is claimed. | — |
| [ ] | QA-B01 | — | **New bug, main's namespace-token feature:** `_xml_token_has_namespace(type(model))` cannot inspect `_XmlProjectionAccess` wrapper-class metadata. Restore actual readonly/scoped Python XML projection and namespace/limit behavior; retain all eight projection tests. Main `60468b0` introduces four errors versus the passing 2.33.0 profile; no follow-up repair is present at inspected main `650b5ea`. The focused Core-only backport excludes this unrelated feature. [Reproduction and scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b01-main-namespace-token-projection-metadata). | — |
| [ ] | QA-B02 | — | **New bug, generated C strict build:** initialize the complete `srl_xml_scalar_rule` contract in both identity helpers. GCC 15.2 strict C11 rejects omitted namespace/identity fields under `-Wextra -Werror`, blocking four Linux CTests and C SDK matrices. Preserve field defaults, validation and wire bytes; require actual strict C package/resource/XML replays without warning suppression. No existing main repair was found. [Failure and acceptance scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b02-strict-generated-c-scalar-rule-initialization). | — |

## XML and package backlog

### XML and package requirement tasks

All **13 items are closed** for the selected contracts. The
[resolution ledger](project/xml_package_backlog_resolution.md) distinguishes new
implementation, completed OPC work and explicit scope decisions. Generic owning
XML/package support covers eleven outputs; the selected full **3MF Core 1.4 graph
is C++/Python only**, with the documented OPC fifth-edition profile. Dependencies
in other sections are deferred from this batch and retain their existing states.
Historical verification records retain their original source/platform scope.

| State | ID | Estimate | Completed contract or disposition | Dependencies deferred outside this section |
| --- | --- | --- | --- | --- |
| [x] | XML-B01 | — | **Closed.** Schema-first association/evolution tools and new schema-local ordered mapping complete the supported authoring workflow. Ambiguous associations require explicit selection. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b01). | — |
| [x] | XML-B02 | — | **Closed.** Supported local/chameleon bundles, identity/pattern combinations, reviewed evolution and new ordered/list declaration imports are complete. Arbitrary automatic historical conversion is declined. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b02). | MIG03 |
| [x] | XML-B03 | — | **Closed.** Rich generic XML mappings have eleven-output parity. Full combined Core 1.4 support is scoped to C++/Python; the other nine retain explicit capability errors. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b03). | QA01 |
| [x] | XML-B04 | — | **Closed.** Owning package profiles, references, envelope selection and selected corpus/resource contracts are implemented across eleven outputs, including the new schema-local replay. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b04). | QA01/QA02/QA03 |
| [x] | XML-B07 | — | **Closed.** The documented OPC fifth-edition profile and Core 1.4 C++/Python profile are qualified within their recorded corpus/resource boundaries. Broader edition/platform/provider and signature claims are excluded. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b07). | QA01/QA02; OPC-OPT02 |
| [x] | XML-B08 | — | **Closed.** Admitted token graphs and reproducible CPU/memory/XML/generated-size measurements are complete. Token/tree tradeoffs and provider/process limits are explicit. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b08). | MG07; QA01/QA03 |
| [x] | XML-B09 | — | **Closed.** Supported presence/nil, ordered/mixed, map/variant and positioned foreign-retention contracts are implemented and bounded. Arbitrary interleaving without native storage remains rejected. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b09). | — |
| [x] | XML-B10 | — | **Closed.** Supported facet/derivation, scalar/list/union/QName, pattern/identity and local-bundle combinations are implemented, with grouped-choice import and explicit unsupported-graph gates. Unrestricted XSD/XPath is excluded. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b10). | — |
| [x] | XML-B14 | — | **Closed.** Concrete owning generic/inheritance and lifecycle XML contracts are implemented. Managed projections are data-only; borrowed output and recursive/combined graphs retain their explicit capability boundaries. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b14). | MG01/MG08; BE03 |
| [x] | XML-B15 | — | **Closed.** Eleven-output borrowed XML stress/lifetime/resource checks avoid duplicate owning payloads. Durable field-address XML is deliberately disabled pending a separately selected identity contract. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b15). | BE03/MG08/MR03 |
| [x] | XML-B18 | — | **Closed.** Selected ZIP64, bounded source/spooling, metadata, large-operation cancellation/publication and recovery contracts are qualified. Unsupported encryption/split/interleaving and process-memory limits remain explicit. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b18). | MG07; QA01/QA03 |
| [x] | XML-B19 | — | **Closed.** Schema-local codec/compression, target modes and vocabulary/type inventories complete existing owning profile/runtime/reference contracts in eleven outputs: 176 inputs and 88 independently inspected archives. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b19). | — |
| [x] | XML-B24 | — | **Closed.** The explicit 30-tag restricted HTML profile, including sub/sup phrasing containers, passes all eleven actual SDKs and independent readers. Arbitrary HTML and unrestricted XHTML are excluded. [Evidence and limits](project/xml_package_backlog_resolution.md#xml-b24). | — |
