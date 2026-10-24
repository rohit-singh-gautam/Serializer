# Pending requirements across Serializer

This list tracks **36 requirements: three completed and 33 unfinished
(29 active and four deferred)**. Keep completed items in this file and mark their
state `[x]`; do not delete their rows or sections when they are completed.
The [implemented feature catalog](project/completed_requirements.md) retains
**826 IDs and verification records**, including historical items archived before
this tracking policy. The [feature list](project/feature_status.md)
summarizes supported capabilities. The three closed ownership decisions live in
[application responsibilities](project/application_responsibilities.md#disposed-requirements),
not in this requirement inventory.

Deferred requirements are `MG01`, `PG01`, `PG02` and native `MIG03`, linked to
their issues below. Deferral means unimplemented and outside the active batch.
Native storage support is active `DB02`; third-party database engines and sink
adapters are outside scope. Dependencies may refer to completed IDs in the
feature catalog. Keep partial parent requirements open even when a narrower
implementation or qualification record is complete.

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

Completed slices such as SC02a/SC02b are cataloged separately; their
qualification does not close the broader SC02 requirement below.

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | SC02 | — | Broaden default-value validation and define bit-field support with wire/compatibility tests. SC02c/SC02d qualify constant-stack parser/backend recognition; [SC02e](project/completed_requirements.md#sc02e) adds declaration locations to legacy/backend field-default errors while preserving acceptance and atomic output; [resource scope](verification/verification-portable-literals-2026-10-24.md). See [Feature roadmap](project/feature_status.md#other-roadmap-items). | — |
| [ ] | SC03 | — | The 2.23.0 worktree adds generated C++ fixed-array Protobuf mapping, exact packed/unpacked/text cardinality and format-specific default retention; [mapping boundary](serialization/protobuf.md#mapping-contract). Complete generic-function calls/defaults and final SDK qualification; [Generics](schema/generics.md). | — |
| [ ] | BE01 | — | Implement native-backend separate-file layout where currently unavailable; preserve naming, dependency includes/modules and editor navigation; [Native languages](languages/native_languages.md). | — |
| [ ] | BE02 | — | Define and implement Java packed output and direct native-codec stream/channel or native-acceleration APIs. Opt-in caller-owned array windows and heap/direct NIO buffers are qualified as [BE02a](project/completed_requirements.md#be02a) / [BE02b](project/completed_requirements.md#be02b), with ownership/failure/resource tests; [Java output](languages/java.md#defaults-and-unsupported-schema-features). | — |
| [ ] | BE03 | — | Qualify borrowed/mapped-view platform, lifetime, allocation and performance boundaries; broader formats require their own mapping contract; [Views](serialization/views.md). | — |
| [ ] | OPC-OPT02 | — | Define a signature/canonicalization/trust provider and independently qualify editing/signed-package behavior before claiming signature support; [OPC design](design/opc_3mf_support.md). | — |

## Qualification and distribution

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | QA01 | — | Replay changed contracts on other supported platforms and remaining provider combinations with real SDKs; retain exact versions, configurations and failures; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md). WSL Windows-mounted appended-journal discard failures remain outside the passing native Windows/Linux filesystem profile; [recorded controls](verification/verification-document-origin-2026-10-22.md#toolchains-and-limits). | — |
| [ ] | QA02 | — | Execute the independent XML/XSD/ZIP/OPC/3MF corpus, malformed-input/fault/resource cases and cross-language exchanges for each newly implemented slice. | — |
| [x] | QA03 | — | **Completed for the documented reproducible Windows workloads.** All eleven SDKs pass 132 native operation profiles/660 wall samples and 220 independently checked timed ZIP/OPC archives, with generated/encoded sizes, wall/CPU and separately identified memory peaks. C++ retains 80 ordinary timing and 80 requested-allocation profiles; Python compares all six compression formats with independent provider baselines. Ordinary native/XML-disabled and plain ZIP controls pass. [Native measurements](verification/verification-native-baselines-2026-10-24.md), [package measurements and limits](verification/verification-package-baselines-2026-10-24.md), [catalog](project/completed_requirements.md#qa03). | — |
| [x] | QA04 | — | **Completed for the current Windows host profile.** Matching 2.0.19 packages build and validate; the installed Visual Studio assembly matches the local build. Actual VS Code and Visual Studio hosts qualify all eleven generated declaration outputs, reversed selections, live unsaved sources, includes and native command dispatch; real CMake/C++ caller checks and complete shared/native resolver checks pass. Repair the absent-grammar declaration fallback without intercepting authored plain-text files. [Executed hosts, exact versions and limits](verification/verification-editor-hosts-2026-10-24.md#qa04-actual-editor-hosts-and-matching-distribution-payloads); [catalog](project/completed_requirements.md#qa04). No Marketplace publication is claimed. | — |
| [ ] | QA-B01 | — | **New bug, main's namespace-token feature:** `_xml_token_has_namespace(type(model))` cannot inspect `_XmlProjectionAccess` wrapper-class metadata. Restore actual readonly/scoped Python XML projection and namespace/limit behavior; retain all eight projection tests. Main `60468b0` introduces four errors versus the passing 2.33.0 profile; no follow-up repair is present at inspected main `650b5ea`. The focused Core-only backport excludes this unrelated feature. [Reproduction and scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b01-main-namespace-token-projection-metadata). | — |
| [ ] | QA-B02 | — | **New bug, generated C strict build:** initialize the complete `srl_xml_scalar_rule` contract in both identity helpers. GCC 15.2 strict C11 rejects omitted namespace/identity fields under `-Wextra -Werror`, blocking four Linux CTests and C SDK matrices. Preserve field defaults, validation and wire bytes; require actual strict C package/resource/XML replays without warning suppression. No existing main repair was found. [Failure and acceptance scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b02-strict-generated-c-scalar-rule-initialization). | — |

## XML and package backlog

### XML and package requirement tasks

| State | ID | Estimate | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- | --- |
| [ ] | XML-B01 | — | Broaden schema-first XML vocabulary and ambiguous-association tooling. | — |
| [ ] | XML-B02 | — | Extend declaration import to additional XSD content-model, identity, chameleon and pattern combinations; qualify schema migration automation beyond the supported subset. | — |
| [ ] | XML-B03 | — | Implement equivalent rich XSD/XML emitters beyond C++/Python and qualify broader numeric, QName, content-model and provider combinations. | — |
| [ ] | XML-B04 | — | Owning package facades exist in all eleven outputs (OPC-R07). Complete declarative payload references, target-mode/profile/runtime selection and broader corpus/performance qualification before closing this parent. | — |
| [ ] | XML-B07 | — | Qualify additional platforms/provider combinations, broad performance and complete selected OPC/3MF profiles. | — |
| [ ] | XML-B08 | — | XML performance: Broaden tree-free token coverage and qualify provider/platform and retained-tree helper costs; measure CPU, memory, encoded size and generated-code size on reproducible workloads. | — |
| [ ] | XML-B09 | — | XML vocabulary: Extend combinations of presence/nil, ordered fragments, maps/variants and positioned extensions; qualify bounded foreign retention, grouped/repeated heterogeneous choices and broader mixed content. | — |
| [ ] | XML-B10 | — | XSD compilation: Extend facets/derivations, multiple-typed-branch/grouped choices and scalar/pattern/identity/local-bundle combinations; qualify union/QName boundaries and explicit unsupported-graph capability gates. | — |
| [ ] | XML-B14 | — | Model coverage: Broaden concrete generic, owning inheritance, revision/magic and digest combinations; define supported managed/view and recursive/combined graph contracts. | — |
| [ ] | XML-B15 | — | Referenced XML output: Broaden stress/resource and ownership qualification without duplicate owning payloads; define any durable field-address XML contract before enabling it. | — |
| [ ] | XML-B18 | — | ZIP extensions: Broaden large-archive, stress and provider coverage; qualify ZIP64/spooling, peak-memory and cancellation boundaries in every backend; unsupported features reject explicitly. | — |
| [ ] | XML-B19 | — | Typed profiles: Extend C++/Python declarations with pipeline/runtime selection, vocabulary profiles and generated payload reference checks before publication; implement equivalent contracts in other backends. | — |
| [ ] | XML-B24 | — | HTML: Broaden restricted HTML adapters only under an explicit supported contract; keep arbitrary HTML and unrestricted mixed-content XHTML outside that contract. | — |
