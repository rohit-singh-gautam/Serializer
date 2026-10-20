# Pending requirements across Serializer

Compiler/runtime **2.17.0** and schema language **1.21.0** complete the next
**105 existing bounded IDs**: 85 package work units and 20 malformed/provider
and platform/reproducibility checkpoints. **67 broader requirements remain
unchecked**. The [batch scope and verification](verification/verification-pending-105-2026-10-21.md)
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

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | OPC-R01 | — | Complete qualified attributes, QName values and prefix lists in every backend; child-local/shadowed QName storage, equivalent prefixes, Unicode names and regenerated declarations preserve expanded identity. | — |
| [x] | OPC-R01a | — | C++/Python scalar-child namespace scopes survive local declarations, shadowing, default namespace resets, optional/repeated occurrences and nested QName/QName-list unions. Native strings and wire codecs retain their contract; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01b | — | Shared UTF-8 XML NCName validation accepts Unicode mapping names and QName prefixes, with invalid encoding/name rejection; all eleven generators accept Unicode mappings. JavaScript/TypeScript independent XML checks verify qualified Unicode attributes, QName lists, prefix lists and child-local scopes; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01c | — | Go/Java/C# direct XML codecs retain plain QName, QName-list and prefix-list occurrence scopes and regenerate expanded identities, including qualified Unicode attributes, typed lists, copied native-authoring bindings and cumulative retention bounds. Thirteen independent namespace probes per backend plus 122 scalar probes and six binding gates pass; namespace-dependent unions/defaults/fixed/enumeration remain gated; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01d | — | Kotlin/Swift plain QName, QName-list and prefix-list scalar scopes retain local/shadowed declarations and explicit default absence; optional/repeated values, qualified attributes, native bindings and cumulative scope admission pass ten independent probes per actual SDK. Namespace-dependent union/default/fixed/enumeration combinations remain gated; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [x] | OPC-R01e | — | C/Rust XML document companions retain plain QName/list/prefix-list occurrence scopes without changing native model structs or wire bytes. Thirteen namespace probes, independent retained-scope admission accounting and 29 existing extension probes pass in both actual SDKs. Wrapped/nil, mixed/all, revision/magic and other documented graph combinations remain gated; [2.16 checks](verification/verification-qname-scopes-2026-10-20.md). | — |
| [ ] | OPC-R02 | — | Broaden namespace-aware reject/skip/preserve slots, exact wildcard positions/counts and required-capability handling beyond the selected generated graphs; unknown core names must still reject. | — |
| [ ] | OPC-R03 | — | Complete lexical lists, exact facets, patterns and unions without duplicate INI inventories; broaden outer union enumeration/fixed values beyond the selected Python subset, namespace-dependent union defaults, QName-list enumeration/fixed values, child-local QName storage and broader numeric/provider boundaries; canonical numeric output satisfies its pattern. | — |
| [x] | OPC-R03a | — | C++ inherited/equivalent-scope QName/QName-list child unions preserve the first successful member and expanded identities; Python namespace-independent outer union enumeration/fixed values compare primitive/list value identities; [2.15 scope](verification/verification-pending-xml-2026-10-20.md). Child-local C++/Python scopes are implemented separately in OPC-R01a; broader backend/restriction combinations remain pending. | — |
| [ ] | OPC-R04 | — | Complete multiple-typed-branch and grouped repeated choices plus ordered heterogeneous content; interleaving and occurrence semantics survive round trips. | — |
| [ ] | OPC-R05 | — | Qualify the selected official Core 1.4 and XML XSD bundle against independent official 3MF corpus/conformance checks; required scalar/content constructs bind and unsupported external schemas reject precisely. | — |
| [ ] | OPC-R06 | — | Complete generated profile/reference checks, target modes, vocabulary/type dispatch and fixed/default/runtime selection; automatically retarget preserved links when a loaded typed role changes to its declared canonical path; failures publish no aggregate/file. | — |
| [x] | OPC-R06a | — | Generated C++/Python fixed singleton role loads retain discovered paths; preserved sources/internal targets rebind simultaneously on write/Save with original relative scopes, selectors and external URI strings retained; conflicts and dangling references reject. Other backend/dynamic-role work remains pending; [2.15 scope](verification/verification-pending-xml-2026-10-20.md). | — |
| [ ] | OPC-R07 | — | Implement ZIP/OPC metadata, reader/writer, shared limits, preservation and full-Save APIs for every generated language, with independent producer/consumer tests; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md). | — |
| [ ] | OPC-R08 | — | Qualify ECMA-376 Part 2 fifth-edition URI/content-type/relationship contracts and full Annex B/pass-through corpus; cover escaped/equivalent names, relative targets, collisions, physical metadata names, target modes, UTF-16 and Relationships markup compatibility. | — |
| [ ] | OPC-R09 | — | Measure large-model CPU, peak memory, encoded/generated-code size and cancellation/provider behavior. Qualify supported platforms with the actual XML/compression providers enabled. | — |
| [ ] | OPC-R10 | — | Broaden value-reference/scoped-projection XML graph, resource and stress qualification in every backend, preserving source lifetime and avoiding owning payload copies; separately define any durable-address XML contract. | — |
| [ ] | OPC-R11 | — | Carry one cumulative operation context through generated codecs, XML, compression, ZIP, references, preservation and publication; test bounds, cancellation and deadlines across backends/providers. | — |
| [ ] | OPC-R12 | — | Broaden fixed-XSD association/import/export, local bundle resolution and ambiguity diagnostics while preserving explicit native member IDs and reviewed schema evolution. | — |
| [ ] | OPC-R13 | — | Retain one-source authoring: derive mapping from XSD or inline defaults/exceptions; qualify new/renamed-field and ambiguous-association workflows as coverage broadens. | — |

`OPC-R01` remains open beyond the checked slices: namespace-dependent
union/default/fixed/enumeration combinations still reject in the newly enabled
backends; C/Rust QName companion graphs additionally gate wrapped/nil scalars,
mixed/all and revision/magic graphs. These combinations require implementations
and actual SDK checks before the parent requirement can be checked.

## Managed references and migration

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | MR01 | — | Broaden native identity/reference lifecycle, corruption, resource and fault tests across allocation, complete-candidate validation, deletion, Undo/Redo, persistence and collaboration. | — |
| [x] | MR01a | — | Eight existing C++ identity/storage profiles qualify cyclic deletion, complete-candidate edge cleanup, historical pins, Undo/Redo, persisted history, candidate allocation failure and post-reservation snapshot-admission rollback with nonreuse/recovery; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Broader collaboration/resource faults remain pending. | — |
| [ ] | MR02 | — | Broaden independent-import ownership/external-mapping failures, editable child moves/retirement, preview resource failures and journal/acceptance faults; preserve IDs and atomic publication. | — |
| [x] | MR02a | — | Eight existing C++ profiles reject six external-map failures, five ownership-attachment failures and corrupt zero/duplicate source IDs without publishing state/history, then recover successfully. Five added tests per profile are part of 184 passing GTests; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Wider preview/journal/acceptance gates remain pending. | — |
| [ ] | MR03 | — | Define and qualify managed token codecs for other generated outputs; keep these distinct from full native managed engines. Resolve currently rejected recursive generic/reference combinations only under an explicit supported contract. | — |
| [x] | MR04 | — | Typed C++ managed_ref, get_id, stable class_id and registry reference/resolution; document/type scope and complete-candidate validation (2.7 Windows direct/separated fixed/compact uint32/uint64 profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | Existing C++ managed runtime |
| [x] | MR05 | — | Initialize required/self/cyclic managed references after native identity assignment and before first validation/publication/history snapshot (2.7 C++ typed-reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MR06 | — | Native two-pass independent import; remap declared tokens, apply explicit external mappings, validate ownership/references and attach transactionally; ordinary integer payloads retain their values (2.7 C++ reference/import profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MR07 | — | C++ apply_editable retains entity identity and reconciles owned children, with zero IDs for genuinely new children; separately, active-transaction registry.reserve and registry.adopt_reserved admit unused identities reserved by that transaction through checked attachment (2.7 generated editable/reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04, MR05 |
| [x] | MR08 | — | Owner-thread previews reserve IDs through the source allocator before exposure; canceled IDs stay burned and exact-base/exact-ID acceptance preserves native Undo/Redo and recovery (2.7 four C++ registry profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR07 |
| [x] | MR09 | — | Identical collaborative document/object IDs with authority leases and reserve_ids; durable reservations, nonreuse, exact acceptance and forged/exhausted-range rejection (2.7 native strict-collaboration profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | MR04 |
| [x] | MIG01 | — | Qualify ID conversion against genuinely rebuilt 2.6.0 compiler/runtime artifacts; all three raw binary formats and supported managed history/identity/allocation profiles convert exactly (Windows direct uint32/separated uint64, 92 conversions; [evidence](verification/verification-managed-legacy-migration-2026-10-20.md)). | — |
| [ ] | MIG02 | — | Add explicitly selected migrations for other historical layouts/container versions and collaboration client checkpoints/outboxes/epochs when required; preserve exact identity, lease state and retries. | — |
| [ ] | MIG03 | — | Define any journal/database-sink migration/rebaseline adapter separately from managed-save/raw-model conversion and test recovery after interrupted publication. | — |
| [x] | MIG04 | — | Explicit same-schema fixed/compact-varint ID conversion helper/CLI for supported managed saves and raw binary formats; preserve exact identities, allocation marks, history, leases and native layout (2.7 direct uint32/separated uint64 reference profiles; [evidence](verification/verification-opc-xsd-2026-10-19.md)). | Existing same-schema C++ codecs |

## Managed state and storage

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | MG01 | — | Implement native managed engines for selected non-C++ backends: ownership, immutable reads, transactions, identity, history, journals and collaboration under native lifetime/error/concurrency contracts; [Language bindings](managed/language_bindings.md). | — |
| [ ] | MG02 | — | Define versioned feature selectors/exclusions and complete component combinations, including history-policy conversion and mandatory identity/authorization rules; [Capabilities](managed/capabilities.md), [managed state](managed/managed_state.md). | — |
| [ ] | MG03 | — | Implement inherited authorization policies, editable/read-only subtree exceptions and separate structural/policy permissions; enforce the same boundary at edit, import and authority acceptance; [Authorization](managed/authorization.md), [managed state](managed/managed_state.md#authorization-on-the-design-hierarchy). | — |
| [ ] | MG04 | — | Define branch merging, conflict/dependency policies and retained-history/collaboration delta integration; qualify retries and invariant validation before publication; [Managed state](managed/managed_state.md#branch-merging), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG05 | — | Extend distributed transport/session, presence/lock-cache and fault/reconnect qualification without weakening strict native identity or authority ownership; [Collaboration](managed/collaboration.md), [implemented runtime](managed/collaboration_runtime.md). | — |
| [ ] | MG06 | — | Define distributed commits and external file/network effects, including durable intent, reconciliation and failure semantics; keep authentication and business-side effects in application services; [Managed state](managed/managed_state.md#distributed-transactions). | — |
| [ ] | MG07 | — | Provide allocator-aware payload containers/decoding and hard peak-allocation hooks; distinguish admission estimates from measured process peaks; [Ownership/allocation](managed/history.md#ownership-and-custom-allocation), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG08 | — | Define supported managed packed/view/recursive/Protobuf and broader inheritance/editor contracts, preserving explicit rejection until real code and qualification exist; [C++ runtime](managed/cpp_runtime.md), [generics](schema/generics.md), [state enhancements](managed/state_enhancements.md). | — |
| [ ] | MG09 | — | Define field-level merging inside managed variants, virtual/abstract/override edit dispatch and portable owned/borrowed method or typed-error contracts; [State enhancements](managed/state_enhancements.md#remaining-work). | — |
| [ ] | MG10 | — | Extend projection/dependency notification policies and compact nested identity/history records where proposed; measure retained/deleted-entity costs and edit latency; [Identity/history](managed/history.md), [data structures](managed/data_structures.md). | — |
| [ ] | PG01 | — | Implement the proposed page store/cache, persistent index keys, dirty-page writeback/recovery, bounded construction and corruption handling before claiming a mostly-disk index; [Paged indexing](design/paged_indexing.md). | — |
| [ ] | PG02 | — | Add lazy value/storage loading and the proposed paged capability selectors only after index/page lifetime and recovery contracts are implemented and measured; [Paged indexing](design/paged_indexing.md). | — |
| [ ] | DB01 | — | Implement and qualify an explicit database document/journal sink adapter with ownership, transactions, schema mapping and failure/recovery semantics; [Database integration](guides/database_integration.md#proposed-document-sink-boundary). | — |

## Compiler and backend proposals

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [x] | SC01 | — | Define proposed signed/floating payload contracts, rounding, overflow, NaN/infinity, zero and scaled arithmetic before keywords or wire values; current syntax remains unavailable (76 vectors, exhaustive16-bit and independent conversion checks; [contract](design/compact_scalar_contract.md), [evidence](verification/verification-compact-scalar-contract-2026-10-20.md)). | — |
| [ ] | SC02 | — | Broaden default-value validation, preserve useful input/member positions in diagnostics and define bit-field support with wire/compatibility tests; [Feature roadmap](project/feature_status.md#other-roadmap-items). | — |
| [x] | SC02a | — | Diagnose out-of-range canonical decimal/hex integer literals in explicit language 1.14+ defaults and retain original member paths/positions through includes and generic binding. Preserve native expressions and legacy defaults; broader validation and bit fields remain pending; [2.15 scope](verification/verification-pending-xml-2026-10-20.md). | — |
| [x] | SC02b | — | Explicit language 1.14+ canonical decimal float/double overflow and declared-type-qualified unknown enum defaults diagnose the original member, including included/generic source. Atomic compiler rejection and generated Python four-protocol checks pass; native expressions, numeric Boolean defaults, underflow and legacy spelling remain compatible; [2.16 batch evidence](verification/verification-pending-batch-2026-10-20.md). Bit fields and broader validation remain pending. | — |
| [ ] | SC03 | — | Extend currently unsupported generic-function calls/defaults and fixed-array initializer/Protobuf combinations with explicit portable contracts and actual SDK checks; [Generics](schema/generics.md). | — |
| [ ] | BE01 | — | Implement native-backend separate-file layout where currently unavailable; preserve naming, dependency includes/modules and editor navigation; [Native languages](languages/native_languages.md). | — |
| [ ] | BE02 | — | Define and implement Java packed output and the proposed stream/channel, native-acceleration and output-buffer-reuse APIs, with ownership/resource tests; [Java output](languages/java.md#defaults-and-unsupported-schema-features). | — |
| [ ] | BE03 | — | Qualify borrowed/mapped-view platform, lifetime, allocation and performance boundaries; broader formats require their own mapping contract; [Views](serialization/views.md). | — |
| [x] | BE04 | — | Compiler-only installed CMake discovery avoids unused runtime-provider lookup; preserve runtime exports, relocation, component selection and cross-toolchain host lookup (2.14 Windows/Linux neutral and multiarch install profiles; [evidence](verification/verification-backlog-2026-10-20.md)). | — |
| [ ] | OPC-OPT01 | — | Define reusable OPC core-property models/adapters if selected; applications supply property values; [OPC design](design/opc_3mf_support.md). | — |
| [ ] | OPC-OPT02 | — | Define a signature/canonicalization/trust provider and independently qualify editing/signed-package behavior before claiming signature support; [OPC design](design/opc_3mf_support.md). | — |
| [ ] | OPC-OPT03 | — | Define any outer package/Pack-IRI addressing API separately from internal part resolution; distinguish optional non-ASCII naming, interleaving and growth hints from mandatory selected-edition requirements; [OPC design](design/opc_3mf_support.md). | — |

## Qualification and distribution

| State | ID | Estimate | Task and acceptance gate | Depends on |
| --- | --- | --- | --- | --- |
| [ ] | QA01 | — | Replay changed contracts on other supported platforms and remaining provider combinations with real SDKs; retain exact versions, configurations and failures; [2.14 checkpoint](verification/verification-backlog-2026-10-20.md). | — |
| [ ] | QA02 | — | Execute the independent XML/XSD/ZIP/OPC/3MF corpus, malformed-input/fault/resource cases and cross-language exchanges for each newly implemented slice. | — |
| [ ] | QA03 | — | Measure throughput, allocations/peak memory, encoded size and generated-code size against reproducible workloads/baselines; preserve ordinary non-OPC paths; [Python compression measurements](verification/verification-python-compression-performance-2026-10-20.md). | — |
| [ ] | QA04 | — | Qualify current editor-host behavior where a dated record identifies only shared/provider/native-interpreter checks; keep packaged/listing versions and all supported navigation synchronized when changed; [Editor navigation](editors/editor_navigation.md), [qualification records](verification/README.md). | — |
| [x] | REL01 | — | Inspect published VSIX archive/hash/identity contents and independently build/install the upstream package with an isolated installed consumer (published 2.0.0 archives and 2.0.1 x64-windows, nine consumer checks; [evidence](verification/verification-distribution-2026-10-20.md)). | — |
| [ ] | REL02 | — | Publish separately authorized prepared releases, VSIX/listings, GitHub metadata and upstream package-port updates; preserve stable package/extension identities; [Repository specification](design/repository_extensions_vcpkg_spec.md). | — |

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
| [ ] | XML-B04 | — | Implement generated package facades for remaining outputs; add declarative payload references, target-mode/profile selection and broad corpus/performance qualification. | — |
| [ ] | XML-B05 | — | Propagate the shared XML/package operation context through remaining backends and qualify large-model/provider boundaries. | — |
| [ ] | XML-B06 | — | Measure repeatable ordinary-native throughput and peak memory against reproducible baselines. | — |
| [ ] | XML-B07 | — | Qualify additional platforms/provider combinations, broad performance and complete selected OPC/3MF profiles. | — |
| [ ] | XML-B08 | — | XML performance: Broaden tree-free token coverage and qualify provider/platform and retained-tree helper costs; measure CPU, memory, encoded size and generated-code size on reproducible workloads. | — |
| [ ] | XML-B09 | — | XML vocabulary: Extend combinations of presence/nil, ordered fragments, maps/variants and positioned extensions; qualify bounded foreign retention, grouped/repeated heterogeneous choices and broader mixed content. | — |
| [ ] | XML-B10 | — | XSD compilation: Extend facets/derivations, multiple-typed-branch/grouped choices and scalar/pattern/identity/local-bundle combinations; qualify union/QName boundaries and explicit unsupported-graph capability gates. | — |
| [ ] | XML-B11 | — | XSD tooling: Expand bundle/tool stress, ambiguous association and platform qualification across text/wrapper/enum, namespace and particle export; preserve reviewed fixed-schema regeneration and protected imported source. | — |
| [ ] | XML-B12 | — | Schema-first authoring: Extend inline association and declaration import while preserving authored behavior and native ordering through reviewed schema evolution. | — |
| [ ] | XML-B13 | — | Runtime XSD: Qualify prepared providers, limits and concurrency in every backend; align input/output namespace-declaration accounting, qualify caller-prepared validator interfaces and prohibit automatic remote schema loading. | — |
| [ ] | XML-B14 | — | Model coverage: Broaden concrete generic, owning inheritance, revision/magic and digest combinations; define supported managed/view and recursive/combined graph contracts. | — |
| [ ] | XML-B15 | — | Referenced XML output: Broaden stress/resource and ownership qualification without duplicate owning payloads; define any durable field-address XML contract before enabling it. | — |
| [x] | XML-B16 | — | Compression parity: Complete whole-object source/sink wrappers and identical framing/error/limit contracts across every output; qualify optional Zstandard/LZ4 adapters; [qualification](verification/verification-compression-parity-2026-10-20.md). | — |
| [ ] | XML-B17 | — | Package parity: Complete ZIP/OPC source/sink, metadata, shared budgets and full-Save guarantees across every output. | — |
| [ ] | XML-B18 | — | ZIP extensions: Broaden large-archive, stress and provider coverage; qualify ZIP64/spooling, peak-memory and cancellation boundaries in every backend; unsupported features reject explicitly. | — |
| [ ] | XML-B19 | — | Typed profiles: Extend C++/Python declarations with pipeline/runtime selection, vocabulary profiles and generated payload reference checks before publication; implement equivalent contracts in other backends. | — |
| [ ] | XML-B20 | — | Typed package loading: Complete generated transactional profiles and automatic codec/reference budgets in every backend; caller-owned callbacks compose selected codecs with the same shared context. | — |
| [ ] | XML-B21 | — | Complete-operation context: Propagate shared accounting/deadlines across remaining backends; qualify projection/compression/provider cancellation and large-input boundaries, including explicit provider-internal peak/preemption limits. | — |
| [ ] | XML-B22 | — | Preservation: Broaden XML/package policy and collision qualification in every backend; keep signatures and lexical preservation under separate contracts. | — |
| [ ] | XML-B23 | — | Reproducibility: Extend independently checked canonical ordering/metadata policies across backends; pin provider/version/options when qualifying compressed byte equality. | — |
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
