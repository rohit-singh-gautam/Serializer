# Completed tracked requirements

[Pending work](../pending.md) | [Feature catalog](completed_requirements.md) | [Feature status](feature_status.md)

These **26 closed parent requirements** were moved from the pending tracker
on 24 October 2026 at the maintainer's request. Their scope, dependencies and
verification links are preserved below. They include implemented contracts,
completed qualification execution and explicitly recorded scope dispositions;
closure does not certify excluded profiles or erase separately tracked bugs.

The feature catalog retains **838 capability and qualification records**.
This parent-task ledger indexes existing work and dispositions; its count is
not added to that catalog. Historical versions, test counts and references to
the current host describe the recorded qualification, not every later release.

## First priority: C++23 minimum

C++23 is mandatory for Serializer itself, including standalone use. The migration
is complete within the recorded Windows/Linux toolchain and installed-consumer
scope; new schema-index and database implementation may build on this minimum.

| ID | Completed contract or disposition | Dependencies |
| --- | --- | --- |
| CPP01 | **Completed.** Adopt the [C++23 minimum](../design/database_storage_requirements.md#c23-minimum-and-one-qualified-dependency-source) for compiler, runtime, managed runtime, generated C++, provider/object targets and installed/exported targets. Synchronize build defaults, consumer guidance and toolchain checks; qualify supported Windows/Linux builds and installed consumers. Preserve schemas, public APIs and wire formats; no C++20 fallback is required. [Qualification and platform limits](../verification/verification-cpp23-2026-10-23.md); [catalog record](completed_requirements.md#cpp01). | — |

## Managed state and storage

| ID | Completed contract or disposition | Dependencies |
| --- | --- | --- |
| MG07 | **Completed.** Resource-owning C++ payload containers, exact clone/decode, generated owning native binary domains and managed callback/history propagation provide hard requested-byte resource hooks. Current/peak domain counters remain distinct from resident admission and process RSS. The [qualified profile and acceptance evidence](../verification/verification-managed-completions-2026-10-24.md#mg07-allocator-aware-payloads-and-hard-allocation-hooks) records supported shapes, explicit exclusions, 15 Windows runtime/generated cases and nine Linux kernel cases; additional platform/profile coverage is separate. | — |
| MG08 | **Completed.** The [managed representation/editor matrix](../managed/representation_contracts.md) defines supported owning direct/separate storage, identities, private fields and distinct public bases, with explicit packed/view/recursive/ambiguous inheritance rejection. Direct managed Protobuf rejects before lowering; separate wrappers preserve golden bytes in four regenerated profiles. [Acceptance evidence](../verification/verification-managed-completions-2026-10-24.md#mg08-supported-representation-and-editor-boundaries) includes 647 merged core cases and eight managed Protobuf cases, with unexecuted host/profile coverage stated separately. | — |

## Compiler and backend proposals

All six requirements are complete within the defined 2.34.0 backend/provider
profiles. See the [implemented contracts](../guides/compiler_backend_updates.md),
[executed qualification and limits](../verification/verification-compiler-backends-2026-10-24.md)
and [catalog records](completed_requirements.md#compiler-and-backend-contracts-in-234).
Earlier slices such as SC02a through SC02e and BE02a/BE02b retain their historical evidence.

| ID | Completed contract or disposition | Dependencies |
| --- | --- | --- |
| SC02 | **Completed.** Language 1.24 broadens scalar default validation and defines unsigned C++ owning bit-field storage, preserving scalar wire bytes and qualifying decode ranges and directional compatibility with MSVC/GCC. Unsupported backend/storage/address mappings reject explicitly. | — |
| SC03 | **Completed.** Exact independent generic function/member calls and constructor/factory input defaults compile and execute with MSVC/GCC. Fixed-array Protobuf retains exact packed/unpacked/text cardinality and format-specific default rules; independent Google SDK exchanges pass. | — |
| BE01 | **Completed.** C/Rust/Python/Swift/Kotlin native dependency bundles preserve declaration order, naming, includes/modules, depfiles and editor navigation. All five actual SDK consumers pass; inline defaults and existing single-string APIs remain compatible. | — |
| BE02 | **Completed.** Java fixed-scalar packed buffers and opt-in direct native-codec OutputStream/channel APIs qualify wire parity, size patches, projections, caller ownership and failure bounds. Existing [BE02a](completed_requirements.md#be02a)/[BE02b](completed_requirements.md#be02b) array/NIO contracts remain independent. | — |
| BE03 | **Completed.** Actual Windows/POSIX borrowed mapped views qualify platform/lifetime/aliasing, persisted fixed-size edits, bounds and measured allocation/performance boundaries. Other formats require their own mapping contract; recorded timings do not promise a universal speedup. | — |
| OPC-OPT02 | **Completed.** A Windows Packaging API signature/canonicalization provider requires explicit pinned certificates and signed coverage. Independent native/.NET producers and validators qualify tampering, trust, coverage, removal/edit/resigning behavior; unsupported provider/algorithm profiles reject. | — |

## Qualification and distribution

| ID | Completed contract or disposition | Dependencies |
| --- | --- | --- |
| QA01 | **Completed qualification execution and triage for the documented profiles.** Four native C++23 toolchains, installed consumers and actual SDKs are replayed; Windows passes all 310 original commands across retained runs, Linux passes 284/288, and both hosts pass all 16 provider combinations/576 checks. Four strict generated-C failures remain open as QA-B02; full failed inventories are retained. WSL mounted-filesystem journal failures remain outside the passing native-filesystem profile. [Current results and exact limits](../verification/verification-qualification-distribution-2026-10-24.md), [filesystem controls](../verification/verification-document-origin-2026-10-22.md#toolchains-and-limits), [catalog](completed_requirements.md#qa01). | — |
| QA02 | **Completed corpus execution and triage for the documented implemented contracts.** Independent XSD tooling, malformed/resource/fault cases, eleven-SDK Windows XML/evolution/compression/ZIP/OPC inventories and the pinned 175-model Core corpus execute. Repaired Python Core outputs independently validate on Windows and Linux. Linux full inventories retain the strict-C failure; actual eight-SDK platform and ten-SDK package comparisons remain explicit diagnostics. QA-B01 and QA-B02 remain open; this execution does not certify the failed Linux C gate or deferred XML/package features. [Executed counts, hashes and scope](../verification/verification-qualification-distribution-2026-10-24.md), [catalog](completed_requirements.md#qa02). | — |
| QA03 | **Completed for the documented reproducible Windows workloads.** All eleven SDKs pass 132 native operation profiles/660 wall samples and 220 independently checked timed ZIP/OPC archives, with generated/encoded sizes, wall/CPU and separately identified memory peaks. C++ retains 80 ordinary timing and 80 requested-allocation profiles; Python compares all six compression formats with independent provider baselines. Ordinary native/XML-disabled and plain ZIP controls pass. [Native measurements](../verification/verification-native-baselines-2026-10-24.md), [package measurements and limits](../verification/verification-package-baselines-2026-10-24.md), [catalog](completed_requirements.md#qa03). | — |
| QA04 | **Completed for the current Windows host profile.** Matching 2.0.19 packages build and validate; the installed Visual Studio assembly matches the local build. Actual VS Code and Visual Studio hosts qualify all eleven generated declaration outputs, reversed selections, live unsaved sources, includes and native command dispatch; real CMake/C++ caller checks and complete shared/native resolver checks pass. Repaired the absent-grammar declaration fallback without intercepting authored plain-text files. [Executed hosts, exact versions and limits](../verification/verification-editor-hosts-2026-10-24.md#qa04-actual-editor-hosts-and-matching-distribution-payloads); [catalog](completed_requirements.md#qa04). No Marketplace publication is claimed. | — |

## XML and package backlog

### XML and package requirement tasks

All **13 items are closed** for the selected contracts. The
[resolution ledger](xml_package_backlog_resolution.md) distinguishes new
implementation, completed OPC work and explicit scope decisions. Generic owning
XML/package support covers eleven outputs; the selected full **3MF Core 1.4 graph
is C++/Python only**, with the documented OPC fifth-edition profile. Dependencies
in other sections are deferred from this batch and retain their existing states.
Historical verification records retain their original source/platform scope.

| ID | Completed contract or disposition | Dependencies |
| --- | --- | --- |
| XML-B01 | **Closed.** Schema-first association/evolution tools and new schema-local ordered mapping complete the supported authoring workflow. Ambiguous associations require explicit selection. [Evidence and limits](xml_package_backlog_resolution.md#xml-b01). | — |
| XML-B02 | **Closed.** Supported local/chameleon bundles, identity/pattern combinations, reviewed evolution and new ordered/list declaration imports are complete. Arbitrary automatic historical conversion is declined. [Evidence and limits](xml_package_backlog_resolution.md#xml-b02). | MIG03 |
| XML-B03 | **Closed.** Rich generic XML mappings have eleven-output parity. Full combined Core 1.4 support is scoped to C++/Python; the other nine retain explicit capability errors. [Evidence and limits](xml_package_backlog_resolution.md#xml-b03). | QA01 |
| XML-B04 | **Closed.** Owning package profiles, references, envelope selection and selected corpus/resource contracts are implemented across eleven outputs, including the new schema-local replay. [Evidence and limits](xml_package_backlog_resolution.md#xml-b04). | QA01/QA02/QA03 |
| XML-B07 | **Closed.** The documented OPC fifth-edition profile and Core 1.4 C++/Python profile are qualified within their recorded corpus/resource boundaries. Broader edition/platform/provider and signature claims are excluded. [Evidence and limits](xml_package_backlog_resolution.md#xml-b07). | QA01/QA02; OPC-OPT02 |
| XML-B08 | **Closed.** Admitted token graphs and reproducible CPU/memory/XML/generated-size measurements are complete. Token/tree tradeoffs and provider/process limits are explicit. [Evidence and limits](xml_package_backlog_resolution.md#xml-b08). | MG07; QA01/QA03 |
| XML-B09 | **Closed.** Supported presence/nil, ordered/mixed, map/variant and positioned foreign-retention contracts are implemented and bounded. Arbitrary interleaving without native storage remains rejected. [Evidence and limits](xml_package_backlog_resolution.md#xml-b09). | — |
| XML-B10 | **Closed.** Supported facet/derivation, scalar/list/union/QName, pattern/identity and local-bundle combinations are implemented, with grouped-choice import and explicit unsupported-graph gates. Unrestricted XSD/XPath is excluded. [Evidence and limits](xml_package_backlog_resolution.md#xml-b10). | — |
| XML-B14 | **Closed.** Concrete owning generic/inheritance and lifecycle XML contracts are implemented. Managed projections are data-only; borrowed output and recursive/combined graphs retain their explicit capability boundaries. [Evidence and limits](xml_package_backlog_resolution.md#xml-b14). | MG01/MG08; BE03 |
| XML-B15 | **Closed.** Eleven-output borrowed XML stress/lifetime/resource checks avoid duplicate owning payloads. Durable field-address XML is deliberately disabled pending a separately selected identity contract. [Evidence and limits](xml_package_backlog_resolution.md#xml-b15). | BE03/MG08/MR03 |
| XML-B18 | **Closed.** Selected ZIP64, bounded source/spooling, metadata, large-operation cancellation/publication and recovery contracts are qualified. Unsupported encryption/split/interleaving and process-memory limits remain explicit. [Evidence and limits](xml_package_backlog_resolution.md#xml-b18). | MG07; QA01/QA03 |
| XML-B19 | **Closed.** Schema-local codec/compression, target modes and vocabulary/type inventories complete existing owning profile/runtime/reference contracts in eleven outputs: 176 inputs and 88 independently inspected archives. [Evidence and limits](xml_package_backlog_resolution.md#xml-b19). | — |
| XML-B24 | **Closed.** The explicit 30-tag restricted HTML profile, including sub/sup phrasing containers, passes all eleven actual SDKs and independent readers. Arbitrary HTML and unrestricted XHTML are excluded. [Evidence and limits](xml_package_backlog_resolution.md#xml-b24). | — |
