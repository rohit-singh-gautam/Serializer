# Pending requirements across Serializer

This list tracks **34 requirements: 20 closed and 14 unfinished
(10 active and four deferred)**. Keep completed items in this file and mark their
state `[x]`; do not delete their rows or sections when they are completed.
The [implemented feature catalog](project/completed_requirements.md) retains
**832 IDs and verification records**, including historical items archived before
this tracking policy. The [feature list](project/feature_status.md)
summarizes supported capabilities. The three closed ownership decisions live in
[application responsibilities](project/application_responsibilities.md#disposed-requirements),
not in this requirement inventory.

Deferred requirements are `MG01`, `PG01`, `PG02` and native `MIG03`, linked to
their issues below. Deferral means unimplemented and outside the active batch.
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
| [ ] | MIG03 | — | **Deferred subsystem — [GitHub #5](https://github.com/rohit-singh-gautam/Serializer/issues/5), native portion.** Define native journal/storage/schema migration and rebaseline separately from managed-save/raw-model conversion; preserve the old recoverable generation through interrupted publication. Third-party sink conversion is excluded. | DB02 for native database profiles |

## Managed state and storage

The [combined storage/index specification](design/database_storage_requirements.md)
defines standalone native indexing, native database authority, independent
local/database/both index placement, explicit snapshot loading and the mandatory
C++23 minimum. The build minimum is qualified as
[CPP01](project/completed_requirements.md#cpp01); the proposed storage/index APIs
remain a contract rather than implementation evidence.
Native indexing can proceed independently of the deferred database paging work.

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | MG01 | — | **Deferred subsystem — [GitHub #1](https://github.com/rohit-singh-gautam/Serializer/issues/1).** Implement native managed engines for selected non-C++ backends: ownership, immutable reads, transactions, identity, history, journals and collaboration under native lifetime/error/concurrency contracts; [Language bindings](managed/language_bindings.md). | — |
| [ ] | MG07 | — | Provide allocator-aware payload containers/decoding and hard peak-allocation hooks; distinguish admission estimates from measured process peaks; [Ownership/allocation](managed/history.md#ownership-and-custom-allocation), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG08 | — | Define supported managed packed/view/recursive/Protobuf and broader inheritance/editor contracts, preserving explicit rejection until real code and qualification exist; [C++ runtime](managed/cpp_runtime.md), [generics](schema/generics.md), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG09 | — | Complete virtual/abstract/override edit dispatch and portable owned/borrowed method or typed-error contracts. Existing same-tag/identity variant field merging and selective collaboration Undo/Redo have [Windows/Linux qualification](verification/verification-managed-variants-2026-10-24.md), cataloged as MG09a; this does not close the broader method contract. See [State enhancements](managed/state_enhancements.md#remaining-work). | — |
| [ ] | MG10 | — | Extend projection/dependency notification policies and compact nested identity/history records where proposed; measure retained/deleted-entity costs and edit latency; [Identity/history](managed/history.md), [data structures](managed/data_structures.md). | — |
| [ ] | PG01 | — | **Deferred subsystem — [GitHub #2](https://github.com/rohit-singh-gautam/Serializer/issues/2).** Implement the proposed page store/cache, persistent index keys, dirty-page writeback/recovery, bounded construction and corruption handling before claiming a mostly-disk index; [Paged indexing](design/paged_indexing.md). | — |
| [ ] | PG02 | — | **Deferred subsystem — [GitHub #3](https://github.com/rohit-singh-gautam/Serializer/issues/3).** Add lazy value/storage loading and the proposed paged capability selectors only after index/page lifetime and recovery contracts are implemented and measured; [Paged indexing](design/paged_indexing.md). | PG01 |
| [ ] | DB02 | — | **Native storage support — [GitHub #6](https://github.com/rohit-singh-gautam/Serializer/issues/6).** Implement the [combined storage/index contract](design/database_storage_requirements.md): mandatory C++23 minimum, data authority and independent index placement, typed snapshots/explicit loading, prepared durable acceptance and qualified native querying/recovery. No third-party database engine/sink dependency; standalone indexing remains usable independently. | CPP01 complete; PG01/PG02 for the incremental backend; MIG03 for migrations |

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
| [ ] | QA01 | — | Replay changed contracts on other supported platforms and remaining provider combinations with real SDKs; retain exact versions, configurations and failures; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md). WSL Windows-mounted appended-journal discard failures remain outside the passing native Windows/Linux filesystem profile; [recorded controls](verification/verification-document-origin-2026-10-22.md#toolchains-and-limits). | — |
| [ ] | QA02 | — | Execute the independent XML/XSD/ZIP/OPC/3MF corpus, malformed-input/fault/resource cases and cross-language exchanges for each newly implemented slice. | — |
| [ ] | QA03 | — | Measure throughput, allocations/peak memory, encoded size and generated-code size against reproducible workloads/baselines; preserve ordinary non-OPC paths; [Python compression measurements](verification/verification-python-compression-performance-2026-10-20.md). | — |
| [ ] | QA04 | — | Qualify current editor-host behavior where a dated record identifies only shared/provider/native-interpreter checks; keep packaged/listing versions and all supported navigation synchronized when changed. Editors 2.0.14/2.0.16 repair generated C/C++ indexing within the existing native interpreter budget; [native adapter checks](verification/verification-native-cpp-navigation-2026-10-24.md) do not close interactive-host qualification. See [Editor navigation](editors/editor_navigation.md), [qualification records](verification/README.md). | — |

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
