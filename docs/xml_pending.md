# Pending requirements across Serializer

This is the authoritative backlog for pending requirements documented anywhere
under `docs/`. Its existing filename is retained so earlier links keep working.
It covers XML/XSD/OPC and 3MF, managed state and identity, migrations, compiler and
backend proposals, storage adapters, and remaining qualification/distribution work.
Design documents retain contracts and rationale; their remaining work is tracked
here rather than in independent completion lists.

## How to read this backlog

- **Pending** means implementation or a required verification result is missing.
- **In progress** means source work has started; it is not yet a verified feature.
- **Qualification pending** means an implementation exists but the stated check
  has not been completed for the specified backend, provider or platform.
- **Proposal** records a documented extension, not an accepted implementation
  commitment or an addition to the active OPC/backend completion task.
- **External** requires a separately authorized publication, remote change or
  external verification. Local packaging does not complete that work.

Do not turn every occurrence of the word `pending` into a new task: pending
transactions/outboxes are runtime states, and dated verification records describe
their original runs. Explicit format restrictions remain restrictions unless a
document proposes extending them. A new completion claim needs a source revision,
selected backend/provider/platform and executed evidence. Keep the original task
IDs below; close only the scope actually checked.

| Find pending work | Section |
| --- | --- |
| OPC/3MF design requirements `OPC-R01`–`OPC-R13` | [OPC and 3MF](#opc-and-3mf-requirements) |
| XML/XSD, compression and package work in every language | [XML and package backlog](#xml-and-package-backlog), [per-language work units](#work-units-capped-at-five-hours) |
| Native managed references, imports, editing previews and recovery | [Managed references and migration](#managed-references-and-migration) |
| Broader managed-state proposals and mostly-disk indexes | [Managed state and storage](#managed-state-and-storage) |
| Compiler/language/backend extension proposals | [Compiler and backend proposals](#compiler-and-backend-proposals) |
| Platform, provider, performance and external release checks | [Qualification and distribution](#qualification-and-distribution) |

## OPC and 3MF requirements

These preserve the IDs from the
[OPC/3MF design](design/opc_3mf_support.md#generic-framework-status-and-remaining-requirements).
The C++ checkpoint provides useful implementations, not complete OPC-standard or
official 3MF Core conformance. Application geometry and manufacturing rules remain
application responsibilities; this table tracks generic Serializer work and
qualification of the selected profile.

| ID | Remaining requirement and completion evidence | Status at the verified checkpoint |
| --- | --- | --- |
| OPC-R01 | Complete qualified attributes, QName values and prefix lists in each backend; test equivalent prefixes, shadowing, Unicode names and regenerated declarations. | C++/Python subsets checked. C++ child-local/shadowed QName bindings need per-field storage; unrepresentable inputs fail. Backend parity pending. |
| OPC-R02 | Port namespace-aware reject/skip/preserve slots, exact wildcard positions/counts and required-capability handling; unknown core names must still reject. | C++/Python positioned-extension subsets checked; broader preservation and remaining-backend qualification pending. |
| OPC-R03 | Complete lexical lists, exact scalar facets, patterns and union branches without duplicate application INI field inventories; qualify value-space boundaries and malformed lexemes. | C++/Python rich subsets checked. Outer union enumeration/fixed values, namespace-dependent union defaults, QName-list enumeration/fixed values and C++ union-QName scalar children reject explicitly. Broader numeric boundary/provider and other-emitter coverage pending. |
| OPC-R04 | Support the required repeated/grouped choices and ordered heterogeneous content, preserving interleaving and occurrence semantics. | C++/Python one-typed-branch repeated choices plus foreign slots checked against 1,275 independent oracle inputs. Multiple typed branches, grouped choices and broader ordered content remain pending. |
| OPC-R05 | Import and bind the selected official 3MF Core XSD bundle, including its imported XML vocabulary and required scalar/content constructs; unsupported external schemas must reject precisely. | Pinned Core 1.4 plus W3C XML bundle import/binding and C++/Python authored-model consumers pass. Independent official 3MF corpus/application conformance remains pending. |
| OPC-R06 | Complete generated profile/reference checks, supported target modes, vocabulary/type dispatch and fixed/default/runtime codec selection; failures publish no aggregate/file. | C++ typed XML/opaque roles checked; remaining profile contracts pending. |
| OPC-R07 | Implement ZIP/OPC metadata, reader/writer, shared limits, preservation and full-Save APIs for every generated language, with independent producer/consumer tests. | C++ and Python checked subsets; Python package suite passes 11 cases. Other nine backends, broad corpus and provider/resource parity remain pending. |
| OPC-R08 | Pin and qualify the supported OPC edition and URI/content-type/relationship contracts, including escaped/equivalent names, relative targets, collisions, metadata names and target modes. Cover required metadata encodings (including UTF-16) and Relationships markup-compatibility processing. | ECMA-376 Part 2 fifth edition selected; C++/Python metadata UTF-16, Relationships MCE and physical ZIP item mapping checked. Full Annex B/pass-through corpus and Python Unicode authority IRI coverage remain pending. |
| OPC-R09 | Measure large-model CPU, peak memory, encoded/generated-code size and cancellation/provider behavior. Qualify supported platforms with the actual XML/compression providers enabled. | Broad performance/provider/platform qualification pending. |
| OPC-R10 | Qualify value-reference/scoped-projection XML in the remaining backends, preserving source lifetime and avoiding owning payload copies; separately define any durable-address XML contract. | C++ borrowed/shared/scoped output checked; broader backend/stress coverage pending. |
| OPC-R11 | Carry one cumulative operation context through generated codecs, XML, compression, ZIP, references, preservation and publication; test bounds, cancellation and deadlines across backends/providers. | C++ subset checked; backend and large-input propagation pending. |
| OPC-R12 | Broaden fixed-XSD association/import/export, local bundle resolution and ambiguity diagnostics while preserving explicit native member IDs and reviewed schema evolution. | Inline binding/import baseline checked in eleven SDKs; broader XSD constructs pending. |
| OPC-R13 | Retain one-source authoring: derive mapping from XSD or inline defaults/exceptions; qualify new/renamed-field and ambiguous-association workflows as coverage broadens. | Existing minimum-coding subset checked; residual association work belongs to OPC-R12. |

The independent official 3MF application/corpus checks belong to OPC-R05/R09;
an ordinary XML exchange or tetrahedron export cannot close those gates. Optional
OPC core-property/signature adapters are separately proposed work below.
OPC-R06 also retains automatic retargeting of preserved links when a fixed-name
typed role was loaded from another path; current Save uses its declared canonical
name and fails if retained links would dangle. Patterned numeric values retain
their native value, so canonical output must independently satisfy the pattern;
automatic lexical-format selection is not implemented.
OPC-R03 also retains C++ floating conversion edge parity: native conversion
currently rejects out-of-range inputs, including underflow-to-zero lexemes that
XSD can round to zero. Checked width-aware facet comparisons do not close that gap.

## Managed references and migration

The implementation/ownership contracts remain in
[managed entity references](design/managed_entity_references.md),
[state registries](managed/state_registry.md),
[local collaboration](managed/local_collaboration.md) and the
[persistent-ID migration guide](guides/persistent_id_migration.md).
Identical document/object IDs at all collaborators are mandatory; translated-ID
operation is not a proposed alternative.

| ID | Remaining requirement and completion evidence | Status |
| --- | --- | --- |
| MR01 | Broaden native identity/reference lifecycle, corruption, resource and fault tests across allocation, complete-candidate validation, deletion, Undo/Redo, persistence and collaboration. | Qualification pending beyond the checked C++ profiles. |
| MR02 | Broaden independent-import ownership/external-mapping failures, editable child moves/retirement, preview resource failures and journal/acceptance faults; preserve IDs and atomic publication. | Qualification pending; core import/edit/preview paths are implemented and checked. |
| MR03 | Define and qualify managed token codecs for other generated outputs; keep these distinct from full native managed engines. Resolve currently rejected recursive generic/reference combinations only under an explicit supported contract. | Pending extension; native engine work is MG01. |
| MIG01 | Qualify the ID converter against separately produced older compiler artifacts, beyond independent fixed-wire fixtures and current supported containers. | Qualification pending. |
| MIG02 | Add explicitly selected migrations for other historical layouts/container versions and collaboration client checkpoints/outboxes/epochs when required; preserve exact identity, lease state and retries. | Proposal; current tool rejects unsupported containers. |
| MIG03 | Define any journal/database-sink migration/rebaseline adapter separately from managed-save/raw-model conversion and test recovery after interrupted publication. | Proposal; current conversion is offline and explicit. |

Fixed/compact ID conversion for supported managed saves and all three raw binary
formats is already checked. Normal `load` must not acquire automatic detection or
compatibility conversion as a consequence of this backlog.

## Managed state and storage

These proposals are collected from the managed design documents; recording them
here does not claim they exist or make them prerequisites for ordinary codecs.

| ID | Remaining requirement / proposal and completion evidence | Source and status |
| --- | --- | --- |
| MG01 | Implement native managed engines for selected non-C++ backends: ownership, immutable reads, transactions, identity, history, journals and collaboration under native lifetime/error/concurrency contracts. | [Language bindings](managed/language_bindings.md); proposal. |
| MG02 | Define versioned feature selectors/exclusions and complete component combinations, including history-policy conversion and mandatory identity/authorization rules. | [Capabilities](managed/capabilities.md), [managed state](managed/managed_state.md); proposal. |
| MG03 | Implement inherited authorization policies, editable/read-only subtree exceptions and separate structural/policy permissions; enforce the same boundary at edit, import and authority acceptance. | [Authorization](managed/authorization.md), [managed state](managed/managed_state.md#authorization-on-the-design-hierarchy); proposal beyond existing policy hooks. |
| MG04 | Define branch merging, conflict/dependency policies and retained-history/collaboration delta integration; qualify retries and invariant validation before publication. | [Managed state](managed/managed_state.md#branch-merging), [state enhancements](managed/state_enhancements.md); proposal. |
| MG05 | Extend distributed transport/session, presence/lock-cache and fault/reconnect qualification without weakening strict native identity or authority ownership. | [Collaboration](managed/collaboration.md), [implemented runtime](managed/collaboration_runtime.md); qualification/proposed extensions. |
| MG06 | Define distributed commits and external file/network effects, including durable intent, reconciliation and failure semantics. Authentication and business-side effects remain application services. | [Managed state](managed/managed_state.md#distributed-transactions); proposal. |
| MG07 | Provide allocator-aware payload containers/decoding and hard peak-allocation hooks; distinguish admission estimates from measured process peaks. | [Ownership/allocation](managed/history.md#ownership-and-custom-allocation), [state enhancements](managed/state_enhancements.md); proposal beyond current PMR/admission hooks. |
| MG08 | Define supported managed packed/view/recursive/Protobuf and broader inheritance/editor contracts, preserving explicit rejection until real code and qualification exist. | [C++ runtime](managed/cpp_runtime.md), [generics](schema/generics.md), [state enhancements](managed/state_enhancements.md); proposed extensions. |
| MG09 | Define field-level merging inside managed variants, virtual/abstract/override edit dispatch and portable owned/borrowed method or typed-error contracts. | [State enhancements](managed/state_enhancements.md#remaining-work); proposal. |
| MG10 | Extend projection/dependency notification policies and compact nested identity/history records where proposed; measure retained/deleted-entity costs and edit latency. | [Identity/history](managed/history.md), [data structures](managed/data_structures.md); proposal beyond checked callbacks and snapshots. |
| PG01 | Implement the proposed page store/cache, persistent index keys, dirty-page writeback/recovery, bounded construction and corruption handling before claiming a mostly-disk index. | [Paged indexing](design/paged_indexing.md); proposal. |
| PG02 | Add lazy value/storage loading and the proposed paged capability selectors only after index/page lifetime and recovery contracts are implemented and measured. | [Paged indexing](design/paged_indexing.md); later proposal. |
| DB01 | Implement and qualify an explicit database document/journal sink adapter with ownership, transactions, schema mapping and failure/recovery semantics. Guidance for external databases is not an implemented adapter. | [Database integration](guides/database_integration.md#proposed-document-sink-boundary); proposal. |

The XML/ZIP/OPC document-state bridge retains its existing detailed **D01–D21**
tasks below; it is not supplied by native save migration or ordinary full Save.

## Compiler and backend proposals

| ID | Remaining requirement / proposal and completion evidence | Source and status |
| --- | --- | --- |
| SC01 | Define proposed `compact_zigzag`, `compact_float16`, `compact_bfloat16`, `compact_float32` and `compact_scaled`, including rounding, overflow, NaN/infinity and signed zero, before adding schema keywords or wire values. | [Compact encodings](schema/compact_integers.md#proposed-signed-and-floating-encodings); proposal. |
| SC02 | Broaden default-value validation, preserve useful input/member positions in diagnostics and define bit-field support with wire/compatibility tests. | [Feature roadmap](project/feature_status.md#other-roadmap-items); proposal. |
| SC03 | Extend currently unsupported generic-function calls/defaults and fixed-array initializer/Protobuf combinations with explicit portable contracts and actual SDK checks. | [Generics](schema/generics.md); proposed extensions. |
| BE01 | Implement native-backend separate-file layout where currently unavailable; preserve naming, dependency includes/modules and editor navigation. | [Native languages](languages/native_languages.md); pending extension. |
| BE02 | Define and implement Java packed output and the proposed stream/channel, native-acceleration and output-buffer-reuse APIs, with ownership/resource tests. | [Java output](languages/java.md#defaults-and-unsupported-schema-features); proposed extensions. |
| BE03 | Qualify borrowed/mapped-view platform, lifetime, allocation and performance boundaries; broader formats require their own mapping contract. | [Views](serialization/views.md); qualification/proposed extensions. |
| BE04 | Add compiler-only installed CMake discovery so source-generation consumers need not resolve unused runtime-provider packages; preserve normal runtime exports and relocatable/cross-toolchain lookup. | [CMake integration](guides/cmake_integration.md); pending extension. Current `LANGUAGES NONE` consumers on multiarch Linux may need provider search prefixes or their selected toolchain's library architecture. |
| OPC-OPT01 | Define reusable OPC core-property models/adapters if selected; applications supply property values. | [OPC design](design/opc_3mf_support.md); optional proposal. |
| OPC-OPT02 | Define a signature/canonicalization/trust provider and independently qualify editing/signed-package behavior before claiming signature support. | [OPC design](design/opc_3mf_support.md); optional proposal. |
| OPC-OPT03 | Define any outer package/Pack-IRI addressing API separately from internal part resolution; distinguish optional non-ASCII naming, interleaving and growth hints from mandatory selected-edition requirements. | [OPC design](design/opc_3mf_support.md); optional extension/edition qualification. |

## Qualification and distribution

| ID | Remaining check and evidence required | Status / source |
| --- | --- | --- |
| QA01 | Replay changed contracts on other supported platforms and remaining provider combinations with real SDKs; retain exact versions, configurations and failures. | Windows full-provider, Linux XML/zlib and minimal-OFF checks pass for the 2.8 checkpoint. Linux Zstandard/LZ4 and broader platform/provider qualification remain pending. |
| QA02 | Execute the independent XML/XSD/ZIP/OPC/3MF corpus, malformed-input/fault/resource cases and cross-language exchanges for each newly implemented slice. | Qualification pending; per-language Q01–Q08 and G01–G09 below retain detailed gates. |
| QA03 | Measure throughput, allocations/peak memory, encoded size and generated-code size against reproducible workloads/baselines; preserve ordinary non-OPC paths. | Qualification pending; code equality is not a universal performance result. |
| QA04 | Qualify current editor-host behavior where a dated record identifies only shared/provider/native-interpreter checks; keep packaged/listing versions and all supported navigation synchronized when changed. | [Editor navigation](editors/editor_navigation.md), [qualification records](verification/README.md); qualification pending. |
| REL01 | Verify actual upstream package/Marketplace contents and installed consumers independently of current source/local packages. | [Distribution](project/distribution.md), [repository specification](design/repository_extensions_vcpkg_spec.md); external qualification. |
| REL02 | Publish prepared releases, VSIX/listing updates, GitHub metadata or upstream package-port changes only when separately authorized. Preserve stable package/extension identities. | [Repository specification](design/repository_extensions_vcpkg_spec.md); external, not authorized by this backlog. |

Historical reports under `verification/` retain their original results and limits.
Their old unrun checks do not override a later passing result for the same exact
contract. Use this backlog and the newest applicable evidence together.

## XML and package backlog

Compiler/runtime **2.8.0**, schema language **1.20.0**, adds checked C++/Python
rich XML and Core consumers, Python OPC profiles and XML enabled by default.
The [2.8 verification record](verification/verification-opc-backends-2026-10-19.md)
records focused/SDK checks, 171/171 Windows and 170/170 Linux native tests, plus
the current minimal installed-package checks and their provider limits.
The earlier **2.7.0** checkpoint remains recorded independently.
Earlier **2.4.0**
qualification remains linked below; it does not qualify newly changed code.
Read the [implemented APIs](serialization/xml_packages.md),
[examples](../example/xml/README.md) and [complete requirements](design/xml_support.md).
The [OPC/3MF authoring requirements](design/opc_3mf_support.md) separate framework
gaps from application models/validation and show the current restricted build route.
Backend verification is recorded separately; generated code alone is not qualification.
See the [2.4.0 checkpoint checks](verification/verification-xml-pending-2026-10-17.md)
and the separate [2.3.0 checks](verification/verification-xml-packages-2026-10-17.md).

The 2.4.0 changes add configured text with attributes and collection wrappers to
every XML emitter; bounded pinned local XSD imports/includes, named nested records,
export and directional compatibility tooling; general native `binary_none` format
selection; and C++ ZIP64, explicit nonseekable spooling, caller-authored transactional
OPC aggregate loading, extension policies and canonical ordering. These are
increments toward this specification. The remaining requirements below still
prevent calling the complete feature finished.

The following slices form the implemented baseline. Each verification record
names its executed revision; the remaining-work column reflects the 2.8 scope.
General completion still requires the remaining backend and release gates;
a C++ result does not close another backend's task.

| Addition | Implemented and checked | Still pending |
| --- | --- | --- |
| Schema-first XML | Inline namespace/class defaults and field exceptions; relative `using schema`; parser version/include checks. | Broader vocabulary and ambiguous association tooling. |
| XSD declaration import | Real CLI import, stable explicit member IDs, dependency tracking, protected existing source; automatic and manual examples pass all eleven SDKs and 264 XML exchanges. | Arbitrary XSD content models, identity constraints, chameleon includes, portable pattern compilation and schema migration automation. |
| Rich XSD/XML | C++/Python supported facets/defaults, patterns/unions, restricted repeated choices, typed lists, QName/prefix context, positioned foreign extensions and Core consumers pass. | Equivalent rich emitters in the other nine outputs, broader numeric/QName/content-model and provider qualification. |
| Generated packages | C++/Python declarations generate Save and owning typed load; dynamic attachments, integrity, preservation and shared-limit tests pass. Independent ZIP/OPC interchange checks pass. | Other nine language facades, declarative payload reference checks, target-mode/profile selection and broad corpus/performance qualification. |
| Operation context | C++/Python shared XML/package accounting, cancellation, deadlines and preservation tests pass. | Remaining-backend propagation and large-model/provider qualification. |
| Ordinary native path | All eleven ordinary generated outputs are byte-identical to the captured 2.6.0 baseline fixture. | Broader repeatable throughput/peak-memory measurements; source equality is not a universal performance claim. |
| Native checkpoint | Windows full-provider 171/171 and Linux XML/zlib 170/170 CTests pass; minimal installed header/provider checks also pass. | Other platforms/provider combinations, broad performance and complete OPC/3MF qualification. |

See [the new XSD examples](../example/xsd/README.md) and
[the earlier authoring record](verification/verification-opc-xsd-2026-10-19.md).

**Coverage rule:** general formats/features must support every generated language.
A missing backend remains pending. Only inherently language-specific features,
such as C++ constant evaluation and borrowed native views, may differ by language.

```text
ordinary owning model
        |
        +-- generated XML: all outputs
        |       |
        |       +-- SDK qualification
        |
        +-- compression / ZIP / OPC
                |
                +-- C++ / Python package implementations
                +-- other backend parity
```

| Area | Remaining requirement |
| --- | --- |
| XML performance | Tree-free token codecs; measured CPU, memory, encoded and generated-code size. Current mapping is static; several backends retain bounded trees. |
| XML vocabulary | `xsi:nil`, general ordered mixed content, maps/variants, and namespace-aware unknown-extension policies beyond C++/Python. Foreign extension retention is a bounded subset, not general mixed-content modeling. |
| XSD compilation | General facets/derivations, multiple-typed-branch or grouped choices, chameleon includes and identity constraints; precise union/QName limits are listed above. Port the richer C++/Python subset to other emitters. Unsupported constructs/backends fail generation. |
| XSD tooling | Extend export to text/wrappers/enums/foreign field namespaces, choice/wildcard, union and inherited pattern-group contracts. Expand bundle/tool stress and platform qualification. Changing a fixed schema still requires regeneration; differing imported source requires review. |
| Schema-first authoring | Broaden the implemented inline association and declaration importer beyond its checked subset. Preserve authored behavior and native ordering through reviewed schema evolution. |
| Runtime XSD | Qualify prepared providers, limits and concurrency on every backend. Align output/input namespace-declaration accounting across adapters. Some outputs use an explicit caller-prepared validator interface. No automatic remote schema loading. |
| Model coverage | Generics, inheritance, managed/view/revision/magic/digest mappings require explicit XML contracts. |
| Referenced XML output | C++ borrowed/shared value and scoped projection tests pass. Broaden their stress/resource qualification and add other-backend support without duplicate owning payloads. Durable field-address XML requires an explicit contract and remains rejected. |
| Compression parity | Whole-object stream wrappers and identical framing/error/limit contracts for every output; optional Zstandard/LZ4 providers where absent. |
| Package parity | ZIP/OPC source/sink APIs, metadata, shared budgets and full-Save guarantees beyond the implemented C++/Python subsets. |
| ZIP extensions | Broaden large-archive, stress and provider coverage beyond the bounded C++ ZIP64/spooling cases checked on Windows/Linux; tighten peak-memory/cancellation guarantees and implement other backend paths. Unsupported features keep failing explicitly. |
| Typed profiles | Broaden C++/Python declarations with pipeline/runtime selection, vocabulary profiles and generated payload reference checks before publication; implement other backends. |
| Typed package loading | Port generated transactional profiles and automatic codec/reference budgets beyond C++/Python. Caller-authored callbacks must explicitly compose their selected codecs with the shared context. |
| Complete-operation context | Port shared XML/package accounting/deadlines beyond C++/Python; qualify projection/compression/provider cancellation latency and resource boundaries on large inputs. Provider-internal allocations/calls are not hard peak or preemption guarantees. |
| Preservation | Broaden C++/Python XML/package policy and collision qualification and implement equivalent policies on every backend. No signature or lexical-preservation claim. |
| Reproducibility | Extend independently checked canonical ordering/metadata policies across backends; compressed byte identity depends on provider/version/options. |
| HTML | Separate HTML syntax/adapter. XML-form XHTML is limited by the implemented XML vocabulary; mixed-content XHTML is not supported. |

The document-state bridge remains a separate substantial item:

```text
editable application state
       |
       +-- full Save --> XML / ZIP / OPC file
       |
       +-- binary_none side channels
             history / journal / collaboration
```

It must preserve object identities, allocation marks, retained history and branch
cursors; bind side channels to the exact saved baseline/layout; and recover a
coherent old/new generation after interrupted publication. Stale/mismatched state
must fail before decoding. Final document files must contain no Serializer
history, active journal or session/outbox state. Native managed formats and
[explicit ID-encoding migration](guides/persistent_id_migration.md) have separate
contracts; they are not already this adapter.

Compression is for **one complete serialized object**. Backend byte chunks are
an implementation detail. Incremental record callbacks and incremental object
serialization are not requirements of this work.

Before marking the complete feature finished, qualify the remaining backend
contracts, independent XML/XSD/ZIP corpus, malformed-input/resource boundaries,
cross-language/platform exchanges and performance measurements. The current
eleven-SDK ordinary-XSD baseline, 169-test Windows native run and XML-disabled/minimal-provider build are
already checked in the linked record; repeat them when relevant inputs change.
No completion dates or unexecuted verification are implied by this list.

## Work units capped at five hours

Each task below is an independent active-work timebox, including its focused
tests and a brief result note. Every cap is at most **5 hours**. A cap is not a
promise that an unknown implementation will finish: if its completion check does
not pass within the cap, keep the feature pending and split the residual work
into another task capped at five hours. Do not close a task merely because its
timebox elapsed. Do not combine multiple languages into one backend task.

Start each task from the current implemented baseline and record its exact ID,
changed contract, executed checks and remaining failures. Reuse completed APIs;
do not rebuild a working reference implementation to fill a checklist cell.
The caps are not a release schedule or a reliable total-duration estimate.

### All language backends

| Language | Task prefix | Implementation and qualification dependency |
| --- | --- | --- |
| C++ | `CP` | Reference implementation first; native managed bridge tracked separately below. |
| Python | `PY` | First non-C++ parity target after shared contracts and reference fixtures. |
| Java | `JV` | Its own owning APIs, JAXP/provider integration and SDK checks. |
| Kotlin | `KT` | Can reuse Java provider decisions; requires its own emitter, unsigned mappings and SDK checks. |
| C# | `CS` | Its own owning APIs, System.Xml/provider integration and SDK checks. |
| Go | `GO` | Its own source/sink, namespace, provider and SDK checks. |
| JavaScript | `JS` | Runtime/provider work, with Node/browser support boundaries stated explicitly. |
| TypeScript | `TS` | Depends on the matching JS runtime task; owns declarations, typed APIs and actual TS consumer checks. |
| Rust | `RS` | Its own ownership, provider dependency, source/sink and SDK checks. |
| Swift | `SW` | Its own Foundation/provider, ownership and platform qualification. |
| C | `C` | Its own explicit ownership, status/failure, provider and SDK checks. |

The shared implementation tasks have one instance each. Backend task templates have a separate
instance for **every** prefix in this table: for example `PY-Z03`, `GO-P07` and
`TS-X05`. A template row is not one task covering eleven outputs. Track each
instance independently as `pending`, `in progress`, or `verified`, linking its
verification evidence when closed. TypeScript runtime sharing never substitutes
for TypeScript declaration and SDK qualification.

Bare backend dependencies such as `X04`, `Z07` and `P15` resolve under the same
language prefix as the task being performed. Ranges are inclusive. `S`, `D` and
`G` IDs refer to the shared tables; an explicit prefix such as `CP-P12` names that
backend's task. Assign recurring documentation, qualification and release-gate
runs a slice/revision suffix, for example `S36-slice-001`, `PY-Q01-slice-001` or
`G03-slice-001`. Each run has its own cap and evidence; an older verified result
does not close a run against changed source or providers.

The 2.4.0 baseline already supplies native format selection on every backend,
configured simple text/wrappers, and the documented C++ compression/ZIP/OPC APIs.
For C++, compression `Z01`–`Z10` and package `P01`–`P15`, `P17`–`P19` generally
start as qualification or contract-alignment work against those APIs. C++ 2.7
already checks shared XML/package budgets and the generated package subset;
broader composition/profile contracts and other backends remain pending. JavaScript, Rust
and C already have direct XML output paths; preserve and qualify them rather than
replacing them merely to satisfy `X01`/`X02`. Other cells start from the exact
implemented subset in the usage guide. No future task is marked verified here.

### Shared compiler and contract tasks

Dependencies name earlier task IDs or the existing 2.4.0 implementation.

| ID | Cap | Depends on | Deliverable and completion check |
| --- | --- | --- | --- |
| S01 | 3 h | Existing XML mapping | Define token/event ownership and limits; golden event traces and malformed boundaries pass. |
| S02 | 3 h | S01 | Define incremental UTF-8/name/namespace handling; split-byte, scope and DTD rejection fixtures pass. |
| S03 | 3 h | S01 | Define explicit nil/presence IR and configuration; ambiguous absent/empty/nil mappings reject. |
| S04 | 4 h | S01 | Define ordered mixed-content IR; text/child/text order fixtures pass. |
| S05 | 3 h | S01 | Define map entry/key/value mapping; duplicate-key and entry-presence rules have fixtures. |
| S06 | 3 h | S01 | Define variant/type tags; ambiguous or conflicting tags reject during generation. |
| S07 | 3 h | S01 | Define XML reject/skip/preserve extension slots; namespace context and required-capability fixtures pass. |
| S08 | 4 h | Existing XSD binder | Compile numeric inclusive/exclusive facets into IR; exact boundary tests pass. |
| S09 | 3 h | Existing XSD binder | Compile Unicode length/enumeration facets; empty/code-point/enumeration tests pass. |
| S10 | 3 h | S09 | Select an explicit portable XSD-regex subset; accepted/rejected corpus and unsupported diagnostics pass. |
| S11 | 4 h | S10 | Compile only that regex subset to portable checks; independent reference matches agree. |
| S12 | 3 h | Existing XSD binder | Derive simpleContent/text and wrappers from XSD; authoritative binding/conflict tests pass. |
| S13 | 3 h | S01 | Compile choice models and ambiguity diagnostics; alternative/occurrence fixtures pass. |
| S14 | 3 h | S01 | Compile all models; permutation and duplicate fixtures pass. |
| S15 | 3 h | Existing XSD binder | Define supported identity selector/field XPath; unsupported expressions reject. |
| S16 | 4 h | S15 | Compile key/unique scopes and tuples; duplicate/incomplete-key fixtures pass. |
| S17 | 3 h | S16 | Resolve keyref targets/types; missing and wrong-scope references reject. |
| S18 | 3 h | Existing bundle loader | Implement chameleon namespace adoption; one file adopted into two namespaces stays isolated. |
| S19 | 3 h | S18 | Handle finite cyclic bundle graphs; legal cycles deduplicate and budgets still bound growth. |
| S20 | 3 h | Existing XSD binder | Bind anonymous nested complex types; ambiguous associations reject. |
| S21 | 3 h | S12 | Export text/wrappers; independent validation and reimport agree. |
| S22 | 3 h | S08–S11 | Export enumerations/facets; independent valid/invalid document checks agree. |
| S23 | 4 h | Existing export | Export multiple namespaces as a pinned bundle; reimport preserves names and dependencies. |
| S24 | 4 h | S08–S12 | Import a scalar/attribute XSD subset into Serializer declarations; generated schemas compile and round-trip. |
| S25 | 4 h | S24 | Extend import to named nested records/collections; nested/include fixtures compile. |
| S26 | 3 h | Existing compare tool | Extend compatibility diagnostics to added/retired fields/models; both comparison directions have tests. |
| S27 | 3 h | S01 | Define concrete generic XML bindings; distinct instances and ambiguous names have tests. |
| S28 | 3 h | S01 | Define inherited field order/flattening; collisions and ordering have tests. |
| S29 | 3 h | S01 | Define revision/magic XML contracts; old/current/invalid layouts have fixtures. |
| S30 | 3 h | S01 | Define digest and managed/view data projections; ownership/lifetime and unsupported states are explicit. |
| S31 | 4 h | Existing package APIs | Define generated compression/package profile IR/configuration; defaults and runtime overrides validate. |
| S32 | 3 h | S31, S06 | Generate vocabulary/expected-type dispatch metadata; wrong roots/types select no decoder. |
| S33 | 3 h | S01 | Define separate HTML syntax/provider boundaries; HTML and XML fixtures remain distinct. |
| S34 | 4 h | S33 | Implement the approved minimal HTML event/output subset; independent HTML checks pass. |
| S35 | 3 h | Existing SDK runners | Maintain a shared producer/consumer fixture manifest; all eleven languages are represented and missing SDKs fail. |
| S36 | 2 h | Each landed slice | Apply release, README, usage, migration and skill updates; review editor impact and verify affected packages when touched. |

### XML tasks for each language

Instantiate each suffix under a language prefix: `X04` becomes `CP-X04` or `PY-X04`.
Each row includes a generated-code compile/run using that language's actual SDK.

| Suffix | Cap | Depends on | Deliverable and completion check |
| --- | --- | --- | --- |
| X01 | 4 h | S01–S02 | Direct scalar token output; escaping/namespace traces pass without a retained tree. |
| X02 | 4 h | X01 | Direct generated record/attribute/nested output; contract and output bounds pass. |
| X03 | 4 h | S01–S02 | Scalar token input; split-token lexical/resource failures pass. |
| X04 | 4 h | X03 | Record/optional/repeated token input; exact-message transactional tests pass without a retained tree. |
| X05 | 3 h | S03, X04 | Explicit nil mapping; absent, empty and nil remain distinguishable. |
| X06 | 4 h | S04, X02, X04 | Ordered mixed content; ordering, escaping and resource tests pass. |
| X07 | 3 h | S05, X04 | Map mapping; key, duplicate and cardinality fixtures pass. |
| X08 | 3 h | S06, X04 | Variant mapping; wrong tag/type combinations reject. |
| X09 | 3 h | S08, X04 | Generated numeric facets; inclusive/exclusive boundary corpus passes. |
| X10 | 3 h | S09, S11, X04 | Text/enumeration/approved-pattern facets; Unicode and independent-oracle tests pass. |
| X11 | 3 h | S12, X04 | XSD-derived simpleContent/wrappers; generated authoritative contracts round-trip. |
| X12 | 3 h | S13, X04 | Choice traversal; exactly one allowed alternative is accepted. |
| X13 | 3 h | S14, X04 | All traversal; valid permutations pass and duplicates reject. |
| X14 | 3 h | S16, X04 | Key/unique state; tuple/index budgets and failure tests pass. |
| X15 | 3 h | S17, X14 | Keyref completion; forward/missing/wrong-scope references are checked before publication. |
| X16 | 4 h | S07, X04 | XML extension slots; reject/skip/preserve and namespace-context round-trips pass. |
| X17 | 3 h | S27, X04 | Concrete generic XML instances; generated instance fixtures pass. |
| X18 | 3 h | S28, X04 | Inherited mappings; field collisions and ordering tests pass. |
| X19 | 4 h | S29, X04 | Revision/magic mappings; historical/current/invalid layouts pass their expected checks. |
| X20 | 3 h | S30, X04 | Digest/data-projection adapters; exact values and genuine language-specific restrictions are tested. |
| X21 | 3 h | Provider prerequisites | Qualify a real prepared XSD provider; limits, concurrency, no-fetch and declared-root checks pass. |
| X22 | 4 h | S34 | Approved HTML adapter subset; ordinary HTML is checked independently of XML parsing. |

`X20` does not require native managed engines or borrowed views in languages that
cannot provide them. It does require an explicit portable data contract wherever
the feature is general; it must not convert missing backend support into an exception.

### Compression tasks for each language

These wrap one complete serialized object. Incremental record/object callbacks
remain outside this work's requirements.

| Suffix | Cap | Depends on | Deliverable and completion check |
| --- | --- | --- | --- |
| Z01 | 2 h | S31 | Provider/API and dependency-absence behavior; unavailable providers fail clearly. |
| Z02 | 3 h | Z01 | Identity source/sink wrappers; exact finish/trailing-input/failure tests pass. |
| Z03 | 3 h | Z02 | Gzip output; an independent decompressor accepts it. |
| Z04 | 3 h | Z02 | Gzip input; malformed, concatenated and expansion-boundary fixtures pass. |
| Z05 | 3 h | Z02 | Zlib output; an independent decompressor accepts it. |
| Z06 | 3 h | Z02 | Zlib input; framing/checksum boundary fixtures pass. |
| Z07 | 3 h | Z02 | Raw-DEFLATE output; independent decompression agrees. |
| Z08 | 3 h | Z02 | Raw-DEFLATE input; truncation and trailing-input cases reject. |
| Z09 | 4 h | Z02 | Optional Zstandard adapter; present/absent-provider and independent-frame checks pass. |
| Z10 | 4 h | Z02 | Optional LZ4-frame adapter; present/absent-provider and independent-frame checks pass. |
| Z11 | 4 h | Z03–Z10 | Shared limits, cancellation and failure fencing; every framing/error/limit contract matches S35 fixtures. |

### Package tasks for each language

| Suffix | Cap | Depends on | Deliverable and completion check |
| --- | --- | --- | --- |
| P01 | 3 h | S31 | Source/sink/metadata/shared-budget foundation; source boundaries have tests. |
| P02 | 3 h | P01 | Stored ZIP writer; an independent ZIP reader opens its output. |
| P03 | 4 h | P01 | ZIP32 directory reader; independent and malformed-offset fixtures pass. |
| P04 | 3 h | P02, Z07 | DEFLATE entry output; independent inflation agrees. |
| P05 | 3 h | P03, Z08 | DEFLATE entry input; CRC/size/trailing-data failures reject. |
| P06 | 3 h | P02 | ZIP64 entry writer; a forced-small independent fixture passes. |
| P07 | 3 h | P03 | ZIP64 entry reader; Python-produced fixtures pass. |
| P08 | 3 h | P06 | ZIP64 directory/locator output; an independent directory inspector agrees. |
| P09 | 3 h | P07 | ZIP64 directory/locator input; large-offset/count boundary fixtures pass. |
| P10 | 3 h | P05, P07 | Descriptor forms; signed/unsigned 32/64-bit fixtures pass. |
| P11 | 3 h | P03 | Bounded nonseekable spooling; cancellation, cleanup and private-temp tests pass. |
| P12 | 3 h | P02 | Staged full-Save replacement; injected faults leave an old or complete new file. |
| P13 | 3 h | P03 | OPC names/content types; normalization, duplicate and conflict fixtures pass. |
| P14 | 3 h | P13 | Scoped relationships/targets; dangling and escaping targets reject. |
| P15 | 3 h | P14, S32 | Transactional typed aggregate loading; a middle-part failure publishes nothing. |
| P16 | 3 h | P15 | Automatic shared codec/reference budgets and diagnostics; exhaustion identifies part/field/link. |
| P17 | 3 h | P15 | Opaque part policies; optional/required reject/skip/preserve fixtures pass. |
| P18 | 3 h | P17 | Preserved relationship/namespace context; edited-package references are revalidated. |
| P19 | 3 h | P02, P14 | Canonical ordering/metadata; independent byte equality survives input permutations. |
| P20 | 3 h | S31–S32, P15 | Generated fixed/default/runtime profiles; expected type is checked before decoding/publication. |
| P21 | 3 h | P16, P20 | Generated application reference checks; duplicate/missing/wrong-type keys reject. |

### Qualification tasks for each language

Run these for the affected implemented slice, not only after every feature is
written. A failing case creates a bounded repair task and leaves its feature open.

| Suffix | Cap | Completion check |
| --- | --- | --- |
| Q01 | 2 h | Actual SDK compilation/run and native format-selection/XML independence pass. |
| Q02 | 3 h | Malformed-input/resource/cancellation corpus passes with explicit providers. |
| Q03 | 2 h | This language produces fixtures consumed by every selected backend; no silent skips. |
| Q04 | 2 h | XML-disabled and minimal-provider generation/build/link checks pass. |
| Q05 | 2 h | Reproducible XML CPU/throughput measurements record workloads and baselines. |
| Q06 | 3 h | Peak-memory/allocation measurements record workload, SDK and measurement limits. |
| Q07 | 2 h | XML encoded-size and generated-code-size measurements are retained. |
| Q08 | 3 h | Supported-platform and reproducibility replay passes; unavailable platforms remain open. |

### C++ document-state bridge tasks

These native managed-engine tasks are additional to general backend parity.
Every external document must remain free of history, journals and session/outbox
state; internal `binary_none` side channels retain the existing native contract.

| ID | Cap | Depends on | Deliverable and completion check |
| --- | --- | --- | --- |
| D01 | 3 h | S31 | Saved-baseline identity/layout/digest format; altered-identity byte fixtures reject. |
| D02 | 3 h | D01 | Sidecar generation manifest; stale/foreign documents fail before payload decoding. |
| D03 | 3 h | D01 | Object-ID snapshot restoration; IDs survive Save/reopen unchanged. |
| D04 | 3 h | D03 | Allocation high-water restoration; issued IDs are never reused. |
| D05 | 4 h | D03 | Retained-history checkpoint transfer; undo/redo survives reopen. |
| D06 | 3 h | D05 | Branch/cursor transfer; forked-history/current-cursor fixtures pass. |
| D07 | 3 h | D01, CP-P12 | Data-only document encoder; independent inspection finds no side-channel records. |
| D08 | 3 h | D02, D04–D07 | Stage document/sidecars under a new generation; staging failure preserves publication. |
| D09 | 3 h | D08 | Publication state machine/commit marker; every transition has a fixture. |
| D10 | 3 h | D09 | Windows durable replacement; injected faults preserve one coherent generation. |
| D11 | 3 h | D09 | Linux durable replacement; matching fault/synchronization tests pass. |
| D12 | 3 h | D10–D11 | Precommit recovery; each interruption reopens the old baseline/history. |
| D13 | 3 h | D10–D11 | Postcommit recovery; each interruption reopens the new baseline/history. |
| D14 | 3 h | D02, D12–D13 | Journal baseline fence/replay; mismatches reject before decoding/replay. |
| D15 | 3 h | D02, D12–D13 | Collaboration/session/outbox fences; pending state cannot cross saved baselines. |
| D16 | 3 h | D04–D06, D12–D15 | Full Save/reopen integration; IDs/history/cursors/journal/session remain coherent. |
| D17 | 3 h | D16 | Legacy native compatibility replay; existing bytes/APIs retain their contract. |
| D18 | 2 h | D16 | Final-file isolation audit; independent inspection finds no side channels. |
| D19 | 3 h | D16 | Bounded pre-publication crash matrix; old-generation recovery passes. |
| D20 | 3 h | D16 | Bounded post-publication crash matrix; new-generation recovery passes. |
| D21 | 2 h | D17–D20 | Executable usage/migration example; Save/load instructions and limitations match results. |

### Shared release gates

| ID | Cap | Completion check |
| --- | --- | --- |
| G01 | 2 h | Independent XML/XSD oracle corpus passes for the landed subset. |
| G02 | 2 h | Independent ZIP/OPC corpus passes for the landed subset. |
| G03 | 3 h | Current Windows slice verified: 169/169 CTests through the full wrapper; repeat for changed source/configuration and repair failures separately. |
| G04 | 3 h | Linux native regression passes; failures receive separate repair tasks. |
| G05 | 2 h | XML-disabled build and explicit unavailability behavior pass. |
| G06 | 2 h | Every bundled provider disabled: build/link and supported identity/stored paths pass. |
| G07 | 2 h | CLI version, release metadata and same-major CMake discovery compatibility pass. |
| G08 | 3 h | Editor impact is reviewed; if touched, both versions/changelogs/listings/packages are synchronized and validated. |
| G09 | 2 h | Coverage, README, skill, migration, links, ignore rules and task-artifact cleanup match actual results. |

Recommended execution starts with the relevant shared contract, a C++ reference
slice and its checks, then the matching Python tasks. Port the accepted slice to
the remaining language prefixes before calling that general feature complete.
Java/Kotlin and JavaScript/TypeScript work may share prerequisites while retaining
their separate completion checks. The document-state bridge follows its own D01–D21
chain; it must not block independent backend parity tasks.
