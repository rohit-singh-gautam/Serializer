# XML and package work still pending

Current source **2.7.0**, schema language **1.20.0**. Earlier **2.4.0**
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

The 2.7.0 implementation adds the following checked slices. General completion
still requires the remaining backend and release gates; a C++ result does not
close another backend's task.

| Addition | Implemented and checked | Still pending |
| --- | --- | --- |
| Schema-first XML | Inline namespace/class defaults and field exceptions; relative `using schema`; parser version/include checks. | Broader vocabulary and ambiguous association tooling. |
| XSD declaration import | Real CLI import, stable explicit member IDs, dependency tracking, protected existing source; automatic and manual examples pass all eleven SDKs and 264 XML exchanges. | Arbitrary XSD content models, identity constraints, chameleon includes, portable pattern compilation and schema migration automation. |
| Rich XSD/XML | C++ generated scalar facets/defaults, singleton choices, typed lists, QName/prefix context and declared foreign extension tests pass. Unsupported non-C++ rich contracts reject before publication. | Equivalent rich emitters and qualification in the other outputs. |
| Generated packages | C++ declaration generates borrowed-input Save and owning typed load; dynamic attachments and shared-limit tests pass. Existing independent ZIP/OPC interoperability checks pass. | Other-language package facades, declarative payload reference checks, target-mode/profile selection and broad corpus/performance qualification. |
| Operation context | C++ runtime shared allocation/work, cancellation, deadlines and preservation tests pass; generated package XML uses the same budget. | Full backend propagation and large-model/provider qualification. |
| Ordinary native path | All eleven ordinary generated outputs are byte-identical to the captured 2.6.0 baseline fixture. | Broader repeatable throughput/peak-memory measurements; source equality is not a universal performance claim. |
| Windows native checkpoint | Full XML/managed/provider-enabled `make.ps1 test` passes 169/169 CTests; minimal installed header/provider checks also pass. | Latest Linux full build/test, broad performance and complete OPC/3MF qualification. |

See [the new XSD examples](../example/xsd/README.md) and
[the current verification record](verification/verification-opc-xsd-2026-10-19.md).

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
                +-- C++ implementation
                +-- other backend parity
```

| Area | Remaining requirement |
| --- | --- |
| XML performance | Tree-free token codecs; measured CPU, memory, encoded and generated-code size. Current mapping is static; several backends retain bounded trees. |
| XML vocabulary | `xsi:nil`, general ordered mixed content, maps/variants, and non-C++ namespace-aware unknown-extension policies. C++ foreign extension retention is an explicit bounded subset, not general mixed-content modeling. |
| XSD compilation | Broader facets/pattern dialect, complex-content derivation, repeated/grouped choices, chameleon includes and identity constraints. Port the richer C++ subset to every emitter. Unsupported constructs/backends fail generation. |
| XSD tooling | Extend export to text/wrappers/enums/foreign field namespaces and choice/wildcard contracts. Expand bundle/tool stress and platform qualification. Changing a fixed schema still requires regeneration; differing imported source requires review. |
| Schema-first authoring | Broaden the implemented inline association and declaration importer beyond its checked subset. Preserve authored behavior and native ordering through reviewed schema evolution. |
| Runtime XSD | Qualify prepared providers, limits and concurrency on every backend. Align output/input namespace-declaration accounting across adapters. Some outputs use an explicit caller-prepared validator interface. No automatic remote schema loading. |
| Model coverage | Generics, inheritance, managed/view/revision/magic/digest mappings require explicit XML contracts. |
| Referenced XML output | C++ borrowed/shared value and scoped projection tests pass. Broaden their stress/resource qualification and add other-backend support without duplicate owning payloads. Durable field-address XML requires an explicit contract and remains rejected. |
| Compression parity | Whole-object stream wrappers and identical framing/error/limit contracts for every output; optional Zstandard/LZ4 providers where absent. |
| Package parity | ZIP/OPC source/sink APIs, metadata, shared budgets and full-Save guarantees across outputs. Current package implementation is C++. |
| ZIP extensions | Broaden large-archive, stress and provider coverage beyond the bounded C++ ZIP64/spooling cases checked on Windows/Linux; tighten peak-memory/cancellation guarantees and implement other backend paths. Unsupported features keep failing explicitly. |
| Typed profiles | Broaden the implemented C++ package declaration with pipeline/runtime selection, vocabulary profiles and generated payload reference checks before publication; implement other backends. |
| Typed package loading | Port generated transactional profiles and automatic codec/reference budgets beyond C++. Caller-authored callbacks must explicitly compose their selected codecs with the shared context. |
| Complete-operation context | Port the implemented C++ shared XML/package context and deadline across outputs; qualify projection/compression/provider cancellation latency and resource boundaries on large inputs. |
| Preservation | Broaden C++ XML/package policy and collision qualification and implement equivalent policies on every backend. No signature or lexical-preservation claim. |
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
