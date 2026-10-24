# Serializer

**State Framework**

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

Define models in `.serializer` schemas and generate language-specific code for
exchanging data. For editable C++ applications, add the managed runtime to group
changes into transactions, retain history, recover committed work, and coordinate
collaborative edits through an authority.

Generated codecs support multiple languages. C++ supplies the established managed
runtime. Explicit [Python](docs/managed/python_native_runtime.md),
[Java](docs/managed/java_native_runtime.md) and
[JavaScript/TypeScript](docs/managed/javascript_native_runtime.md) native engines
have qualified restricted owning profiles. Six other native engines and broader
managed contracts remain [pending](docs/pending.md#remaining-work-at-a-glance).
Managed wire-record exchange has its separate codec-only contract.

The [application responsibility contract](docs/project/application_responsibilities.md)
separates host authentication, policy administration, business effects and custom
migration decisions from Serializer's compiler/runtime guarantees. Closed
responsibility decisions, completed features and unfinished library work have
separate references.

[Project website](https://www.singh.org.in/serializer.html) ·
[Documentation index](docs/README.md) ·
[Pending requirements](docs/pending.md) ·
[Completed tasks](docs/project/completed_backlog.md) ·
[Implemented requirement catalog](docs/project/completed_requirements.md) ·
[Serialization quick start](#get-started) ·
[State-management quick start](docs/managed/getting_started.md) ·
[Feature status](docs/project/feature_status.md)

**Using a coding agent?** Give it the
[Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).

| Need | Component | Entry point |
| --- | --- | --- |
| Define and exchange data | Schema compiler and generated codecs | [Usage guide](docs/guides/usage.md) |
| Evolve serialized payloads | Payload revisions and compatibility checks | [Versioning](docs/schema/versioning.md), [schema evolution](docs/schema/schema_evolution.md) |
| Group editable-model changes | C++ managed transactions and persistent identity | [Managed quick start](docs/managed/getting_started.md) |
| Undo, redo, and retain branches | C++ linear or tree snapshot history | [Managed runtime](docs/managed/cpp_runtime.md) |
| Recover committed work | C++ appended or sidecar journals | [Journal contract](docs/managed/journal.md) |
| Coordinate edits across clients | C++ collaboration and authority policy hooks | [Local collaboration](docs/managed/local_collaboration.md) |
| Resolve typed entities and application keys | Opt-in C++ snapshot registries and field indexes | [State registries](docs/managed/state_registry.md) |
| Explore mostly-disk indexes | Proposed paged backend and database comparison | [Paged indexing](docs/design/paged_indexing.md) |
| Retain rebuildable C++ caches | Optional priority (default `0`, lowest), application-selected providers and graceful-close retention | [Cache contract](docs/design/cache_attributes.md) |

Selected compression framing, errors and byte/history admission now pass one shared corpus across all eleven outputs: 15,400 decisions, 2,904 owning exchanges and independent standard-frame checks. The [qualification record](docs/verification/verification-compression-parity-2026-10-20.md) identifies the actual compiler/runtime captures, provider deployments and scope.

## Releases and supported outputs

Compiler/runtime **2.34.1**, schema language **1.24.0**, repairs Python readonly/scoped
XML projection metadata and complete generated-C identity scalar-rule initialization.
Regenerate affected SDKs with this compiler; public APIs and native wire bytes retain
their contracts. See the [qualification repairs](docs/verification/verification-qualification-bugs-2026-10-24.md).

Compiler/runtime **2.34.0**, schema language **1.24.0**, includes the following
managed additions. Both editor packages are **2.0.23**.

[MG07 and MG08 acceptance](docs/verification/verification-managed-completions-2026-10-24.md)
closes allocator-aware payload hooks and the supported managed representation contract.

JavaScript and TypeScript have explicit [native owning SDK profiles](docs/managed/javascript_native_runtime.md).
Pair the ESM module with strict declarations for immutable byte snapshots, callback
transactions, typed identities, bounded linear history and exact authority/client
receipts. The [actual SDK replay](docs/verification/verification-javascript-managed-runtime-2026-10-24.md)
records Windows journals and the limits of Node-free memory checks.

Java has independent [token-only and native engine profiles](docs/managed/java_native_runtime.md).
Select `java.managed_engine = true` for finite concrete owning graphs with detached
immutable reads, transactions, identities, bounded history/journals and exact
authority receipts. Java 17 generated SDK replay covers eight identity/codec
profiles; ordinary streaming, packed and buffer APIs remain independently usable.

Python has explicit [managed token codecs](docs/managed/token_codecs.md) and
a separate [native owning engine](docs/managed/python_native_runtime.md).
Select `python.managed_engine = true` for qualified generated metadata, immutable
reads, transactions, identities, linear history, journals and exact receipts.
Both inline and dependency-file layouts have actual generated SDK replay.

Select `cpp.payload_allocator = true` for supported direct owning native
binary [payload graphs](docs/managed/payload_allocation.md). Generated records,
decode temporaries and managed callbacks retain their selected allocation domain;
explicit cloning transfers graphs. The hard bound covers routed requested bytes,
with separate resident admission and process-memory contracts.

Language 1.24 [guarded C++ edit dispatch](docs/managed/edit_dispatch.md)
supports virtual/abstract/override contracts through checked editor adapters.
Implementations borrow the transaction channel and preserve owned/borrowed
argument and typed-error contracts; closed adapters reject before user code.

The [immutable disk index](docs/managed/paged_index.md) bounds external
construction and its shared page cache. Bind every generation to the exact
document, schema, definition, epoch, revision and source digest. Native database
wrappers retain that eager source; they do not introduce lazy model authority.

Language 1.24 adds [local managed scalar indexes](docs/schema/indexes.md):
ordered/hash lookup and optional type-scoped uniqueness. Queries pin their source
snapshot; uniqueness validates complete candidates before publication. Database
placement, composite keys and lazy authoritative storage remain separate work.

The [bounded eager native database](docs/managed/native_database.md) owns one
managed root, durable exact retries and a prepared acceptance boundary. Native
migration publishes a validated new epoch while retaining a recoverable source
generation. Explicit stable class tags permit converted schemas with renamed
classes and changed payload layouts while preserving instance IDs and references.
Paging, persistent database placement and broader migration remain
separate requirements; see the recorded platform and recovery limits.

Select [`store_options::compact_history`](docs/managed/compact_history.md) to
share unchanged retained history blocks. Generated Windows profiles preserve
default Save bytes; [measured retention and latency](docs/verification/verification-compact-history-2026-10-24.md)
keep resident admission, routed allocations and process memory distinct.

Managed/storage redesign may change formats or APIs where required. Version
changed contracts and document conversion or explicit rejection; retaining
a recoverable source file protects publication without requiring permanent
historical-format readers. See [native upgrades](docs/managed/native_database.md#native-migration-and-rebaseline).

Opt-in [bounded dependency notifications](docs/managed/edit_dispatch_and_notifications.md)
prepare stable field paths and immutable before/after pins before acceptance.
Projection filters can select affected identities and paths while preserving
the default publication callback contract.

The [resource-owning payload runtime](docs/managed/payload_allocation_runtime.md)
provides allocator-bound strings, vectors, maps and exact native decoding.
Resource limits measure routed allocation requests; process RSS has a separate
contract. Existing default container APIs and codec bytes remain compatible.

The [managed representation contract](docs/managed/representation_contracts.md)
defines owning storage/editor support and explicit packed/view/recursive limits.
Direct managed Protobuf rejects before identity lowering; separate-value wrappers
preserve their existing mapping. See the [executed compiler/wrapper matrix](docs/verification/verification-managed-representations-2026-10-24.md).

[Pinned file-generation replacement](docs/serialization/file_generations.md)
preserves old native readers across atomic publication. Current managed profiles
have scoped [verification records](docs/verification/verification-managed-state-2026-10-24.md).
Database placement, persistent authority indexes, paging, lazy loading and broader
non-C++ native engines remain in the [pending requirements](docs/pending.md).

Compiler/runtime **2.34.0**, schema language **1.24.0**, adds schema-local ordered
XML selection and package vocabulary/type/target-mode settings in all eleven
owning outputs. XSD declaration import now retains named/inline/union list-item
constraints and imports supported grouped/repeated complex choices into one
ordered variant array. Existing imports retain their layout and language header
unless they use the new ordered storage. Both editor packages are **2.0.23**.
See [ordered XML](docs/serialization/ordered_xml.md),
[XSD import](docs/serialization/xsd_tooling.md#import-declarations-from-xsd)
and [package profiles](docs/serialization/xml_packages.md#generated-package-pipelines).
The closed [HTML fragment profile](docs/serialization/xml_advanced_contracts.md#html-is-a-separate-fragment-adapter)
also admits `sub` and `sup` phrasing containers; its other syntax and resource
boundaries remain explicit.
The [qualification record](docs/verification/verification-xml-authoring-2026-10-24.md)
closes XML-B19. The [resolution ledger](docs/project/xml_package_backlog_resolution.md)
closes all thirteen XML/package rows, including the independently qualified HTML
profile and reconciliation with completed OPC work. Full Core 1.4 is C++/Python
only; excluded features and dependencies outside this batch remain explicit.

Compiler/runtime **2.34.0**, schema language **1.24.0**, adds checked scalar
field defaults, C++ unsigned bit fields and exact independent generic calls/defaults.
Native backends gain explicit dependency source layouts; Java gains opt-in native
stream/channel output and fixed-scalar packed buffers. C++ OPC signatures use a
Windows provider with explicitly pinned signing certificates and reported coverage.
See the [compiler/backend contracts](docs/guides/compiler_backend_updates.md) and
[qualification status](docs/verification/verification-compiler-backends-2026-10-24.md).
Both editor packages are **2.0.23**. Existing declared language contracts and wire
formats remain independent of these additions.

Editor packages **2.0.23** include the declaration fallback qualified in a real
VS Code host across all eleven outputs and repair navigation when an optional
language grammar is absent. See the [current qualification record](docs/verification/verification-editor-hosts-2026-10-24.md)
for the original 2.0.19 host results, package hashes and qualification limits.

Compiler/runtime **2.33.0** adds independently opt-in Java heap/direct
`ByteBuffer` output with `java.nio_output_buffer_reuse = true`. A successful encode
advances the caller's position; failures preserve its cursor while confining any
partial contents to the original window. Existing owning/array APIs and wire
formats retain their contracts. See the [NIO API](docs/languages/java.md#caller-owned-nio-buffers)
and [qualification](docs/verification/verification-java-nio-output-2026-10-24.md).
Schema language remains **1.23.0**; both editor packages advance to **2.0.18**.

Compiler/runtime **2.32.2** attaches original file/line/column locations to portable
field-default errors, including older included declarations and generic definitions.
Existing rejection reasons, native C++ acceptance and valid generated bytes are
preserved; see the [diagnostic contract](docs/schema/construction.md) and
[qualification](docs/verification/verification-backend-default-locations-2026-10-24.md).
Schema language **1.23.0** and editor packages **2.0.17** stay unchanged.

Compiler/runtime **2.32.1** repairs the embedded Java runtime source-token bound
for older MSVC hosts while preserving emitted Java bytes. Schema language **1.23.0**
and editor packages **2.0.17** stay unchanged; see the
[build qualification](docs/verification/verification-java-runtime-literals-2026-10-24.md).

Compiler/runtime **2.32.0** adds opt-in Java caller-owned output windows for all
four native codecs and readonly projections. Set `java.output_buffer_reuse = true`
and encode into an exclusive byte-array window, receiving the written byte count.
Existing method sets stay unchanged by default. See the [Java contract](docs/languages/java.md#caller-owned-output-windows)
and [qualification](docs/verification/verification-java-output-buffer-2026-10-24.md).
Schema language remains **1.23.0**; both editor packages advance to **2.0.17**.

Compiler/runtime **2.31.2** uses linear, constant-stack recognition of portable
string/decimal defaults and backend identifiers. This repairs Linux crashes while
preserving accepted spellings, backend errors and generated bytes; target SDK
literal/token limits remain separate. Schema language **1.23.0** and editor
packages **2.0.16** are unchanged. See the
[portable resource checks](docs/verification/verification-portable-literals-2026-10-24.md).

Compiler/runtime **2.31.1** scans canonical integer defaults with constant stack
use. Long out-of-range decimal/hex literals now produce member-located diagnostics
on Linux rather than exhausting a recursive regex engine. Accepted literal/native
spellings, schema language **1.23.0**, generated APIs and wire formats are unchanged.
See the [qualification record](docs/verification/verification-integer-defaults-2026-10-24.md).

Compiler/runtime **2.31.0**, schema language **1.23.0**, adds
[named managed generic self references](docs/schema/named_managed_references.md)
for C++. A named instantiation binds its copied self-token fields to that named
record, including supported optional/array/map wrappers. Other named copies keep
independent identities. Older declarations retain their prior contract; shared
nested/base records still require explicit concrete targets. See the
[example](example/managed/named_references/README.md) and
[qualification status](docs/verification/verification-named-managed-references-2026-10-24.md).

Compiler/runtime **2.30.1** requires **C++23** and a matching standard library
with `std::byteswap` and `std::span` for the compiler, runtime, generated C++ and
managed consumers. Regenerate C++ headers for the `if consteval` arithmetic helper.
Public CMake targets propagate the minimum; direct C++20 use
rejects explicitly. Schemas, public APIs and serialized formats are unchanged.
Both editor packages are **2.0.18**. See the [migration steps](migration.md#c23-minimum-in-2301)
and [qualification record](docs/verification/verification-cpp23-2026-10-23.md).

Editor **2.0.14** uses a direct generated C declaration index to keep native
navigation within its existing interpreter allocation budget. See the
[native navigation checks](docs/verification/verification-native-c-navigation-2026-10-24.md).
Editor **2.0.16** also bounds generated C++ field navigation within the same native
interpreter budget, including separate-values managed layouts; see the
[C++ navigation checks](docs/verification/verification-native-cpp-navigation-2026-10-24.md).
Installed SDK-only examples use compiler-component discovery; enabling a native
language later upgrades discovery to the runtime without duplicating the compiler.

Compiler/runtime **2.30.0**, schema language **1.22.0**, adds owning C++ `cache`
fields, generated descriptors, bounded optional persistence, exact-source guarded
publication and priority reclamation. New journals use `.<filename>.serializer/`
with separate journal, cache and durable-state namespaces. Explicit
`close_journal()` saves required state before retiring recovery data, preserving
caches; legacy layouts have an explicit migration API. Existing payload IDs,
bytes and transient behavior remain compatible. Both editor packages are
**2.0.12**. See the [cache contract](docs/design/cache_attributes.md),
[usage example](docs/guides/usage.md#rebuildable-c-cache-fields) and
[qualification record](docs/verification/verification-cache-2026-10-23.md).

Compiler/runtime **2.29.0** adds explicit retirement of disposable C++ external
document journals. `journal_options::allow_document_retirement` is opt-in and
requires a self-contained saved format preserving native identity/allocation
metadata. `model_store::retire_journal()` preserves the main and refuses dirty,
shared, pending, uncertain or unsaved-allocation state. Its optional in-memory
mode preserves live session history; the host must reattach before further
durable mutations. Retirement never synchronizes or publishes collaboration
changes. See [the usage contract](docs/guides/usage.md#explicit-retirement-of-self-contained-documents).
The schema language and journal formats are unchanged; existing callers retain
their journals by default. Windows qualification and remaining boundaries are
recorded with the feature; no Linux or power-loss qualification is implied.

Compiler/runtime **2.28.2** prepares Windows extended-length native file and
journal I/O without a registry change. Ordinary logical paths and opaque journal
origin bytes stay unchanged; transient native drive/UNC spellings cover opening,
writer locks, companion metadata and atomic publication. Explicit device syntax
keeps its existing semantics. Six focused native boundary/journal checks passed
on Windows x64 MSVC 19.51 Release with XML OFF and unchanged machine policy,
including Unicode paths through 320 characters, native lock exclusion, replacement
refusal/retry and exact managed recovery/history pins. A short per-project MSBuild
tracking location was required for the moved owning build; this does not qualify
all native build paths. UNC spelling checks are lexical, with no live-share or
power-loss qualification. The schema language and serialized formats are unchanged.

Compiler/runtime **2.28.1** fixes strict GNU builds by explicitly initializing
temporary XML field descriptors and declaration prefixes, and uses the
compiler-known length of the large embedded C runtime literal. Public
APIs, generated formats and schema language 1.21.0 are unchanged.

Release **2.28.0** binds [explicit ordered storage](docs/serialization/ordered_xml.md)
to fixed-XSD sequence/choice groups in all eleven owning outputs, enforces their
complete occurrence grammar, and exports/compares those grammars. The release
also implements namespace-aware reject/skip/preserve slots, exact wildcard
positions and counts, and strict/lax validation against pinned declarations in
all eleven outputs; see the [wildcard qualification](docs/verification/verification-xml-wildcards-2026-10-22.md).
It also broadens
[C/Rust namespace companions](docs/verification/verification-native-namespace-graphs-2026-10-22.md)
through wrappers, nil values, mixed/ordered children, all groups and lifecycle
metadata, preserving native protocol bytes and bounded namespace retention.
It also completes [borrowed XML namespace graphs and depth qualification](docs/verification/verification-readonly-xml-graphs-2026-10-22.md)
in all eleven outputs, with source lifetime, native storage identity and
transactional output preserved.
The same release completes schema-scoped QName/list union constraints, defaults,
fixed values and restricted atomic/union list items in all eleven outputs.
Unicode categories, blocks, XML name escapes and class subtraction compile into
bounded portable pattern programs. Regenerate affected codecs; native payload
APIs and wire formats are unchanged. See the [scalar identity matrix](docs/verification/verification-scalar-identities-2026-10-22.md)
and [Unicode pattern qualification](docs/verification/verification-unicode-xml-patterns-2026-10-22.md).

Compiler/runtime **2.28.0** composes generated package profiles in all eleven
outputs: typed XML, native JSON or BINARY_NONE inside stored/DEFLATE parts,
fixed/default/runtime envelope selection, root vocabulary/type checks, inert
external byte relationships and explicit repeated-role renames. Model, root,
MIME and container identities stay bound to the schema. C/Rust package owners
retain XML namespace/foreign companions per physical occurrence; encoding borrows
the original native payload and sidecar. See the [pipeline contract](docs/serialization/xml_packages.md#generated-package-pipelines)
and [executed SDK evidence](docs/verification/verification-package-pipelines-2026-10-22.md).

Release **2.27.0** extends [element-only ordered XML](docs/serialization/ordered_xml.md)
to all eleven owning outputs. Regenerate models using `ordered=member` to retain
heterogeneous child order in native variant arrays, including exact fixed-array
cardinality and whitespace-only gaps with a zero mixed-text budget. Native wire
formats and schema language remain unchanged.

Release **2.26.0** adds [C++ native origin metadata and prepared/confirmed Save](docs/managed/document_origin.md).
The application chooses an owned full recovery main or a durable original with
differences-only sidecars. Opaque provenance stays outside authored content;
prepared Save pins native state until host publication evidence is confirmed.
New journals group auxiliary files in one hidden `.<filename>.journal/` directory.
Existing flat journals retain their layout and recovery support.

Release **2.25.0** adds opt-in [C++ ordered element-only XML](docs/serialization/ordered_xml.md).
An array of typed variants preserves child order directly, without a text
alternative or hidden order metadata. Existing models, mixed content and native
bytes retain their behavior. Its original fixed-XSD and export restrictions are
addressed by the 2.28.0 ordered-content update.

Release **2.24.0** adds opt-in [collaborative retained-branch merging](docs/managed/branch_merging.md#collaboration).
Tree authorities advertise `branch_merges_enabled()` after the host enables
`collaboration_options::allow_branch_merges`. Replica `merge_request` binds both
the accepted cursor and native target revision. Complete-candidate policy/locks,
accepted field deltas, selective Undo/Redo and native two-parent journal recovery
use the existing publication boundary. The host may install an explicit conflict
resolver; clients cannot supply ancestry or bypass the current policy.
All eleven XML outputs now retain schema-scope QName scalar/list enumeration,
default and fixed identities, including first-selected outer unions and restricted
list items. QName-pattern aliases and prefix-list string enumeration retain their
lexical contracts. The [2.28 scalar evidence](docs/verification/verification-scalar-identities-2026-10-22.md)
includes native wire identity and independent original/exported schema checks.
The [large readonly XML checks](docs/verification/verification-readonly-xml-stress-2026-10-21.md)
exercise 25,000 borrowed values in all eleven outputs, with storage identity,
exact byte bounds and recovery. Current namespace-sensitive graph coverage is
recorded in the [2.28 borrowed XML checks](docs/verification/verification-readonly-xml-graphs-2026-10-22.md).

Release **2.23.0** adds [native C++ branch merging](docs/managed/branch_merging.md)
for retained tree revisions, with explicit conflict choices, two-parent ancestry
and native journal recovery. The 2.24 qualification adds opt-in collaboration
requests and verifies all four native storage/ID profiles on Windows and Linux.
Unchanged managed variant identities also merge per field during native merging
and selective collaboration Undo/Redo; tag/identity replacements remain atomic.
The [variant qualification](docs/verification/verification-managed-variants-2026-10-24.md)
passes all four Windows/Linux identity/storage profiles, including conflicts,
journal recovery and later-author protection.
Use `rohit::serializer::serialize_as(object)` for
[one-expression compact JSON](docs/guides/usage.md#string-backed-output-buffers)
in a default owning `std::string`. Select another owning result/protocol explicitly;
borrowed views require caller-owned storage.
Generated C++ [Protobuf fixed arrays](docs/serialization/protobuf.md#mapping-contract)
use ordinary repeated fields with exact reader cardinality and existing schema defaults.
New managed engines and paged/lazy/database storage are
[deferred to GitHub issues](docs/pending.md), separately from active fixes.

Release **2.22.0** adds native C++
[capability activation/history conversion and session fault qualification](docs/verification/verification-managed-capabilities-2026-10-21.md),
[Pack-IRI addressing](docs/serialization/pack_iri.md) and explicit
[ZIP physical metadata replay](docs/serialization/zip_metadata.md).
Package Save follows the documented complete-rewrite contract. Targeted evidence
retains its dated scope; the [current resolution ledger](docs/project/xml_package_backlog_resolution.md)
records the selected OPC/3MF contract and exclusions.
The [mandatory fifth-edition OPC metadata qualification](docs/verification/verification-opc-metadata-2026-10-21.md)
covers URI/IRI syntax, infrastructure encodings, Relationships compatibility and
Annex B consumption across all eleven outputs.
The selected Core 1.4 C++/Python codecs now pass the
[official 175-model corpus qualification](docs/verification/verification-core-conformance-2026-10-21.md),
with explicit schema/resource dispositions. XSD validity does not certify package,
reference or geometry conformance.

Regenerate affected XML codecs for
[eleven-output namespace admission and provider completion fixes](docs/verification/verification-runtime-boundaries-2026-10-21.md).
Physical declarations consume the selected quota; inherited scope copies retain
their separate work/allocation admission. Prepared-validator cancellation is
checked before results are published.
C#/Kotlin/Swift expose copied cancellation policies through `WithCancellation`
or `withCancellation`, with external-library qualification. The same callback
can enforce a host-clock deadline. See the [operation context contract](docs/serialization/xml_packages.md#shared-xmlpackage-operation-context).
The selected [large owning package chain](docs/verification/verification-large-package-operations-2026-10-21.md)
passes XML/DEFLATE/identity/preservation/atomic Save qualification in all eleven outputs.
Regenerate Python package facades for cancellation checks after Save hooks:
pre-publication cancellation preserves the old file; a failure after replacement
reports `publication_uncertain` with the complete new archive retained.

The current compiler and runtime release is **2.34.1**. Bug fixes and minor updates
increase the patch number; new features increase the minor number. Both preserve
full compatibility. Developers change the major number manually, with best-effort
compatibility across major releases. The schema language version is **1.24.0**;
see [release and schema versions](docs/guides/command_line.md).

Release **2.33.1** backports the established Python Core owning-tree routing
repair from main. Regenerate affected XML codecs; complete-graph checks keep
token-only contracts on their required path. Native APIs, schema versions and
JSON/binary identities are unchanged. See the [migration notes](migration.md#python-core-xml-routing-repair-in-2331).
The [current qualification record](docs/verification/verification-qualification-distribution-2026-10-24.md)
separates four-toolchain builds, provider and SDK results, current patch replays,
measurements and outstanding bugs.

Release **2.21.0** adds [C++ OPC core-property adapters](docs/serialization/core_properties.md),
[complete-candidate managed authorization](docs/managed/authorization.md), explicit
C++ capability requirement profiles and a bounded PMR resource for routed
allocations. Policy administration and principal binding remain application-owned;
generated payload container/decoding allocators retain their existing contract.
Python [QName-list restrictions](docs/verification/verification-qname-lists-2026-10-21.md)
retain ordered schema-scope expanded identities for enumeration/default/fixed
values, including selected union members and global text. Regenerate affected
outputs. Native identities, wire formats and schema language **1.21.0** stay compatible;
the [pending list](docs/pending.md) retains broader engine/backend/provider gates.

Release **2.20.1** charges generated namespace declarations in Java/C#/Kotlin/Swift
XML output, including qualified attributes and default-namespace resets. Kotlin
and Swift charge type declarations once, including preserved scopes; C# input
counts declaration prefixes consistently with output. Regenerate affected codecs
when using tight XML limits. Native model APIs, wire bytes and schema language
retain their contracts. The [namespace budget test](test/xml_namespace_budget_test.py)
checks actual SDK admission against an independent declaration count.
The [eleven-output native baseline](docs/verification/verification-native-codec-baseline-2026-10-21.md)
records ordinary positional-binary throughput and memory with reproducible owning
workloads and XML-disabled controls; measured peak categories remain distinct.

Release **2.20.0** qualifies [XSD contract tooling](qualification/xsd_tooling/README.md),
including native-enum export/string-space binding, directional comparison and
checked local bundles. Existing native enum values, names and model layouts stay
unchanged. The compiler also fixes sequence-to-`all` widening and
[Windows UTF-8 CLI paths](docs/serialization/xsd_tooling.md#windows-utf-8-cli-paths).
Windows and Linux each pass 300 compiler-tooling decisions and the ordinary
eleven-output export/rebind matrix. Rich compiler cases and generated runtime
profiles retain [separate evidence](docs/verification/verification-xsd-tooling-2026-10-21.md).

Release **2.19.2** qualifies [XSD authoring and native-model evolution](qualification/xsd_evolution/README.md)
in all eleven outputs. Staged imports retain existing expanded-identity IDs;
authored models can preserve native names, IDs, defaults and behavior while XML
exceptions rename or reorder their mapped children. XML compatibility remains
separate from native wire compatibility; imports never infer a rename migration.

Release **2.19.1** adds an [independent package reproducibility inventory](qualification/package_reproducibility/README.md)
covering all eleven outputs. Existing writer policies, names and serialized
representations retain their contracts. The inventory checks unordered input
permutations, fresh processes, ZIP64 metadata and compression-provider boundaries;
it does not establish lexical XML or signature normalization.

Release **2.19.0** extends fixed singleton package path canonicalization across
all eleven outputs. Preserved links resolve in their original source scope before
simultaneous rebinding; selectors, external URI references and opaque bytes retain
their values. Removed moved roles and path/relationship collisions reject before
publication. C/Rust canonical writers and Go canonical loaders are additive opt-in
APIs; existing defaults retain their behavior. Explicit dynamic/repeated role renames are available in 2.28; supply the generated
rename plan so preserved scopes and links rebind together. See the [package preservation contract](docs/serialization/xml_packages.md#fixed-singleton-path-canonicalization)
and [executed relocation checks](docs/verification/verification-package-relocation-2026-10-21.md).

Release **2.18.0** adds caller-owned native XML operation contexts in
[C](docs/languages/c_packages.md) and [Rust](docs/languages/rust_packages.md),
including shared XSD identity tuple admission inside generated packages. Selected
codecs compose with the same package budget in Go, JavaScript/TypeScript and
Kotlin consuming applications. Existing model APIs, limit layouts, schema
language and serialized representations retain their contracts. Logical
admissions do not measure provider or opaque factory internals as hard process
peaks. Version 2.28 additionally supports document-companion package graphs.
See the [native operation and public codec verification](docs/verification/verification-native-xml-operations-2026-10-21.md).

Release **2.17.1** fixes initial C++ collaboration joins of documents containing
typed managed references. Incoming references use the staged authority namespace
while the client retains its application validation policy; rejected joins leave
the placeholder state intact. Existing schemas, APIs and saved formats retain
their contracts. See [local synchronization](docs/managed/local_collaboration.md)
and the [reference failure/recovery matrix](docs/verification/verification-managed-reference-faults-2026-10-21.md).

Release **2.17.0** adds [generated package policies and application references](docs/serialization/package_application_contracts.md).
Generated [C](docs/languages/c_packages.md), [Rust](docs/languages/rust_packages.md)
and [Swift](docs/languages/swift_packages.md) outputs now provide bounded owning
ZIP/OPC facades, typed loading, preservation and staged Save. Both editor packages
are **2.0.23** and recognize language **1.24.0** bit fields and generic calls,
**1.23.0** named self-reference contracts,
**1.22.0** cache fields and earlier package declarations.
Version 2.28 composes C/Rust XML document companions with typed package roles.
Language 1.21 declarations select fixed/default ZIP storage and scoped typed keys
and references. Runtime storage overrides retain declared XML/model identities;
automatic duplicate, missing and wrong-type checks run before aggregate/file
publication. Existing schema contracts and native wire identities remain supported.

Release **2.16.0** preserves C++/Python scalar-child QName scopes through local
declarations, prefix shadowing and default namespace resets, including repeated
values and nested QName/list unions. Go/Java/C#/Kotlin/Swift add direct
QName/list/prefix-list codecs with per-occurrence scope storage and copied
native-authoring bindings; C/Rust retain these scopes in XML document companions. Shared
XML mapping names and prefixes now accept valid Unicode NCNames. Native wire
formats and schema language **1.20.0** retain their contracts. See
[executed checks and remaining boundaries](docs/verification/verification-qname-scopes-2026-10-20.md);
the earlier `OPC-R01` scope is superseded by the
[2.28.0 scalar and graph qualification](docs/verification/verification-scalar-identities-2026-10-22.md).

The same release checks canonical decimal float/double default overflow and
explicitly qualified enum defaults in language 1.14+ declarations. C++/Python
package codecs share relationship and supported XSD identity-reference budgets
and retain active field/link diagnostics. Eight C++ managed profiles add import
failure and cyclic-deletion qualification. See the
[batch scope and verification](docs/verification/verification-pending-batch-2026-10-20.md).

C#/.NET 8 and Kotlin/JVM 17 also gain
[owning ZIP/OPC packages](docs/languages/csharp_kotlin_packages.md): bounded
sources, ZIP64/descriptors, private spooling, typed XML roles, extension policies
and staged Save. Their package/XML operations share a cumulative budget. The
[50-item dependency batch](docs/verification/verification-pending-50-2026-10-20.md)
records acceptance evidence and remaining gates. Existing schema language and
native codecs retain their contracts.

Release **2.15.0** adds selected C++ child QName unions and Python namespace-independent
union enumeration/fixed values. Generated C++/Python OPC facades retain discovered
fixed-role names and automatically rebind preserved links to their declared output
paths, including original relative scopes and query/fragment selectors. Regenerate
affected outputs and retain the loaded extensions when saving. Integer-default
overflow diagnostics now identify the declaring member and its source location.
See the [scope and verification](docs/verification/verification-pending-xml-2026-10-20.md);
the former QName and package-backend gates are superseded by the 2.28.0 contracts.

Release **2.14.0** adds [compiler-only installed CMake discovery](docs/guides/cmake_integration.md#compiler-only-source-generation).
Use `find_package(Serializer 2.14 CONFIG REQUIRED COMPONENTS compiler)` to load
the installed generator and generation helpers without discovering unused
runtime-provider packages. This supports `LANGUAGES NONE` source-generation
projects. Normal discovery and `COMPONENTS runtime` keep the full runtime exports
and provider lookup. The host executable still needs its deployed shared-library
dependencies. Generated Python modules also add
[whole-message compression helpers](docs/serialization/compression.md#generated-python-api)
for identity, gzip, zlib, raw DEFLATE and optional Zstandard/LZ4 providers, with
bounded borrowed-stream wrappers and exact frame/inner-message validation. C++
wrappers also check cancellation after final transport/native callbacks and during
cached input finalization; keep borrowed policy callables alive through their last
wrapper operation.
Generated Java files add a [whole-message helper](docs/languages/java.md#whole-message-compression)
for identity, gzip, zlib and raw DEFLATE, plus optional Zstandard and LZ4 runtime
providers. Generated source compiles without the optional provider jars.
Generated Go packages add [owning compression helpers](docs/languages/go_compression.md)
for identity, gzip, zlib and raw DEFLATE, with borrowed streams, explicit finish,
exact frame validation and fresh native decoding. The generated core stays
self-contained on Go 1.22+; explicit per-operation Zstandard/LZ4 adapters use
pinned optional modules on Go 1.25+.
Generated C# files add [whole-message compression](docs/languages/csharp_compression.md)
for .NET 8 identity/gzip/zlib/raw DEFLATE with explicit completion, bounded frame
validation and borrowed streams. Caller-selected managed assemblies add optional
Zstandard/LZ4 through compatibility-preserving option factories.
Generated [JavaScript/TypeScript helpers](docs/languages/javascript_compression.md)
provide asynchronous owning and borrowed-stream APIs. Load `nodeProvider()`
explicitly for gzip/zlib/raw DEFLATE; browser imports and identity need no Node
provider. Generated [Kotlin/JVM helpers](docs/languages/kotlin_compression.md)
provide identity/gzip/zlib/raw DEFLATE with standard JDK providers and lazy
optional Zstandard/LZ4 runtime jars. JS/TS explicitly select Node Zstandard
transforms or a caller-supplied LZ4 block module; the default module adds no imports.
These helpers check cancellation before
fresh decoded publication; active wrappers fence callback-driven close/reentry
and retain the first failure. See the [executed profiles](docs/verification/verification-backlog-2026-10-20.md).
Selected [Python XML measurements](docs/verification/verification-python-xml-performance-2026-10-20.md)
and [C++ XML measurements](docs/verification/verification-cpp-xml-performance-2026-10-20.md)
record CPU, encoded/generated sizes and exact native-only baselines. Python
records traced allocation; C++ records absolute process peak residency, including
setup and native libraries. Logical bounds do not represent hard allocation limits.
All eleven generated outputs also pass the selected
[XML exchange and native independence checks](docs/verification/verification-xml-exchange-2026-10-20.md):
121 configured and 121 fixed-XSD producer/consumer exchanges, plus actual
XML-disabled builds with all four ordinary native protocols.
Generated [Swift compression](docs/languages/swift_compression.md) is standalone
for identity/gzip/zlib/raw DEFLATE, with bounded pure-Swift codecs and borrowed
closure-based streams. Explicit caller-owned providers add Zstandard and LZ4
through separate option initializers, preserving the original initializer API.
The [core SDK proof](docs/verification/verification-swift-compression-2026-10-20.md)
and [provider proof](docs/verification/verification-swift-compression-providers-2026-10-20.md)
cover Windows Swift 6.4 and Linux Swift 6.1.3.
Generated [C11 helpers](docs/languages/c_compression.md) supply standalone identity
and optional zlib-backed gzip/zlib/raw DEFLATE, with transactional typed native
codecs and bounded borrowed callbacks. Their
[Windows/Linux SDK profiles](docs/verification/verification-c-compression-2026-10-20.md)
include provider-free, model/enum collision and binary-only builds.
Explicit C compile definitions and application linkage also enable
[optional Zstandard/LZ4](docs/languages/c_compression.md#optional-zstandard-and-lz4),
with independent history admission and
[Windows/Linux provider qualification](docs/verification/verification-c-compression-providers-2026-10-20.md).
Generated [Rust compression](docs/languages/rust_compression.md) supplies
standalone identity/gzip/zlib/raw DEFLATE with typed errors and exact native
callbacks. Pass mutable transport references to retain ownership, and finish
explicitly. The [Windows/Linux SDK proof](docs/verification/verification-rust-compression-2026-10-20.md)
also verifies native Linux generation and exclusive-borrow reentry rejection.
Caller-owned Rust providers add Zstandard and fast level-0 LZ4 through additive
`*_with_provider` APIs. The
[provider proof](docs/verification/verification-rust-compression-providers-2026-10-20.md)
covers actual Windows/Linux SDKs while original option literals remain compatible.
C++ stores add [checked identified-baseline initialization](docs/managed/cpp_runtime.md#first-initialization-from-identified-storage)
from typed storage, a stored document namespace and the native allocation watermark,
preserving exact IDs, references and burned allocation gaps.
External-document recovery now checks companion generation and baseline-frame
integrity before invoking the application decoder. The
[Windows/Linux document-bridge proof](docs/verification/verification-managed-document-bridge-2026-10-20.md)
and [runnable JSON example](example/managed/external_document/README.md) cover
Save/reopen, native history, collaboration metadata and crash recovery.
The [identified-admission proof](docs/verification/verification-managed-identified-baseline-2026-10-20.md)
passes 197 cases across nine CTests on each of Windows and Linux. Selected
C++/Python package checks also qualify [descriptor/spool/Save lifecycle](docs/verification/verification-package-lifecycle-2026-10-20.md)
and [typed rollback/preservation policies](docs/verification/verification-package-profiles-2026-10-20.md).
Native and Python [ZIP64 input boundaries](docs/verification/verification-package-zip64-input-2026-10-20.md)
include sparse offsets above 4 GiB, real 65K entry transitions and checked borrowed
seekable Python sources. Native directory indexing reuses existing budgeted scratch.
Generated [JavaScript/TypeScript packages](docs/languages/javascript_packages.md)
add bounded ZIP/OPC readers, writers and typed XML facades with asynchronous
load/write/Save. Stored ZIP works without Node imports; DEFLATE and private
filesystem operations require explicitly selected Node providers. The
[qualification](docs/verification/verification-javascript-packages-2026-10-20.md)
covers independent frames, preservation policies, C++ exchanges and atomic Save
faults in the selected Windows profile.
Generated [Java ZIP/OPC facades](docs/languages/java_packages.md) use Java 17
standard APIs for typed XML/opaque aggregates, bounded sources, preservation and
staged Save. Their [qualification](docs/verification/verification-java-packages-2026-10-20.md)
passes 224 decisions, independent outputs and native C++ exchange, with a separate
Linux runtime replay of the Java 17 classfiles.
Generated [Go physical ZIP helpers](docs/languages/go_packages.md) use Go 1.22
standard APIs for bounded ZIP32/ZIP64, descriptors, spooling and staged Save.
Their [Windows/Linux qualification](docs/verification/verification-go-packages-2026-10-20.md)
covers independent archives, large boundaries and native interchange.
[Go OPC facades](docs/languages/go_packages.md#typed-opc-facades) add bounded
metadata/relationship validation, transactional typed XML loading, preservation
and canonical output. Their [Windows/Linux proof](docs/verification/verification-go-opc-packages-2026-10-20.md)
includes quota regressions and Save faults. Current facades share a cumulative
ledger with supported generated XML codecs and direct scalar application
references; see [composition across eleven outputs](docs/verification/verification-native-xml-operations-2026-10-21.md).
Broader profile/graph combinations retain explicit capability gates.
Compiler output publication
[preserves unchanged files](docs/guides/cmake_integration.md#preserve-unchanged-generated-files),
so Ninja can skip dependent compilation when regeneration produces equal bytes.
The [external JSON document example](example/managed/external_document/README.md)
demonstrates bounded callbacks, native Save/reopen, retained redo and burned
allocation gaps with the existing sidecar API.
Schema language **1.20.0**, existing model APIs and wire formats are unchanged.
Both editor source packages are **2.0.9**, with navigation to the new generated
package facades and bounded indexing of marked generated runtime support.

Release **2.13.0** adds differential C++ sidecar journals and an
[external-document codec adapter](docs/guides/usage.md#external-documents-with-a-differential-sidecar).
Sidecar records reuse saved bytes through copy/literal instructions without
periodic full-snapshot fallback. Applications can keep a JSON, XML, package or
binary main file and supply its encoder/decoder independently of the native
journal. Existing documents can be attached without rewriting their bytes;
full Save retains history, identities and pending collaboration work in the
side channel. Exact-byte document digests and generation manifests bind recovery
to the matching original. Appended mode remains the default API choice, and
version-two journals remain readable. This runtime addition leaves schema
language **1.20.0** and editor packages **2.0.5** unchanged. Format-specific
callbacks and their parser limits remain application responsibilities; built-in
XML/OPC profile integration and broader platform qualification remain in the
[backlog](docs/pending.md).

Release **2.12.1** fixes C++23 XML floating-point parsing builds with warnings
treated as errors. Numeric conversion uses `numeric_limits<T>::denorm_min()`
without deprecated traits, preserving signed-zero underflow and overflow rejection.
Schema language **1.20.0**, generated APIs and wire formats are unchanged.

Release **2.12.0** adds [generated XML token paths](docs/serialization/xml_tokens.md),
explicit presence/nil, maps, selected variant mappings and ordered mixed
content, XSD identity checks and singleton `all` traversal in every output,
owning inheritance in every output,
authoritative text/wrappers and singleton choices in the checked native outputs,
portable scalar/facet and pattern programs and separate approved HTML adapters in every output. Python
adds revision/magic checks, owning digest values and positioned extension slots;
their supported combinations remain documented individually. JavaScript/TypeScript
add direct incremental token input through the pinned SAX provider. Java/C# add
exact scalar, scoped QName/list/union and default/fixed adapters.
Kotlin/Swift maps use independent companion limits that propagate through nested
records; `mixedLimits` independently bounds ordered fragments and text.
JavaScript/TypeScript also check revision/magic layouts, owning digest values and
positioned foreign extensions.
Java/C#/Go/Kotlin/Swift also preserve positioned foreign extensions with explicit
capability and independent owner-local admission policies.
In **2.28.0**, all eleven owning outputs also bind fixed-XSD wildcard namespace
predicates and strict/lax declaration inventories from pinned local bundles.
Validation and skip/preserve retention are independent; private replay shares the
original operation's budgets and cancellation. Scalar-text and singleton
`all`/choice mappings accept foreign attributes and end-child slots. See the
[wildcard, closed-core fallback and resource contract](docs/serialization/xml_tokens.md#xsd-wildcards-and-positioned-extensions-in-2280).
C/Rust expose owning XML document companions that keep native values separate
from opaque extension state; their item and retained-byte policies are cumulative
over the complete document.
These companions also check QName/list enums, namespace-aware union selection
and schema-owned defaults/fixed values across children, attributes and text.
Keep the companion with its native value when editing or re-encoding; see the
[native constraint and provider checks](docs/verification/verification-native-qname-constraints-2026-10-22.md).
Serializer lifecycle metadata stays closed under permissive extension policies.
Owning revision/magic, digest values, readonly reference output and scoped-translator
XML output pass in every output's selected native contract. Real prepared XSD callbacks
are qualified in Go, Swift, C and Rust through the maintained native provider fixture. All eleven
outputs have checked baseline direct input/output; advanced mapping completion
remains tracked per backend. Concrete-generic
XML support broadens in non-C++ outputs. The
[typed OPC example](example/opc/README.md) generates C++/Python package facades
and checks independent ZIP/XML readers. See the
[executed X01-X22 checks](docs/verification/verification-xml-tokens-2026-10-19.md)
for all eleven outputs. These reference tasks are complete; broader OPC/XSD
combinations and platform qualification remain in the backlog.

Release **2.11.0** adds checked [advanced shared XML contracts](docs/serialization/xml_advanced_contracts.md):
compiled scalar/pattern and identity validation, content-model metadata,
chameleon/cyclic local bundles, authoritative text/wrapper binding, expanded
XSD import/export and directional evolution checks. Export a pinned
multi-namespace inventory with `--xml.export-xsd-bundle`. C++ helpers provide
portable regex programs, scoped key/unique/keyref checks, document profiles and
a separate restricted HTML fragment adapter. Existing generated C++ package
facades also expose owning profile metadata, normally named `document_profiles()`.

Shared IR and reference validators do not establish generated-code parity.
Advanced identity, content-model and generic/inherited/revision/projection
codecs retain explicit backend capability gates. Existing native protocols and
payload layouts, schema language **1.20.0**, and editor packages **2.0.5** remain
unchanged. See [verification status and executed checks](docs/verification/verification-xml-advanced-contracts-2026-10-19.md).

Release **2.10.0** adds an incremental LibXml2-backed C++
[XML event reader](docs/serialization/xml_events.md#incremental-byte-reader)
without a retained tree, plus checked [shared XML contracts](docs/serialization/xml_shared_contracts.md)
for presence/nil, ordered mixed content, maps, variants and positioned extension
slots. The new configured contracts bind shared compiler IR and reference
validation; the corresponding selected generated owning codecs are now implemented
in all eleven outputs under the 2.28.0 contract. The public extension-slot helper supports
raw semantic preservation with namespace context. Schema language **1.20.0**,
both editor packages **2.0.5**, and existing generated/native wire contracts are
unchanged. See [executed checks](docs/verification/verification-xml-shared-contracts-2026-10-19.md).

XML support is enabled by default in CMake and the vcpkg manifest; use
`-DSERIALIZER_WITH_XML=OFF` for a build without it. Existing CMake caches retain
their previous selection until explicitly changed. XML declarations and supplied
XSDs automatically enable generated XML entry points. Release **2.8.0** also adds
Python OPC package facades and richer C++/Python XML bindings, including the
supported published Core 1.4 XSD subset. See the
[current executed checks](docs/verification/verification-opc-backends-2026-10-19.md); qualification and
remaining backend work are tracked in [the backlog](docs/pending.md).

Compiler/runtime **2.7.0**, language **1.20.0**, adds inline XML defaults and field
exceptions, `using schema "local.xsd";`, and reviewable XSD declaration import.
Start with the [automatic and manual XSD examples](example/xsd/README.md) for all
eleven outputs. Existing schemas keep their native codecs and wire identities.
C++ additionally generates typed OPC package facades and propagates one operation
budget, cancellation and deadline through XML and package I/O. Richer C++ XML
bindings do not establish backend parity; unsupported backend combinations reject.
See [implemented XML/package contracts](docs/serialization/xml_packages.md),
[XSD subset and import rules](docs/serialization/xsd_tooling.md) and
[remaining requirements](docs/pending.md). Both editor packages use **2.0.5**.

Compiler/runtime **2.6.0**, language **1.19.0**, adds
[inline value references](docs/schema/value_references.md), such as
`public borrowed(payload) d (1);`. In C++, `consumer.d = owner.b` stores a const
payload pointer. The owner stays ordinary and unaware of the consumer; the
application keeps its storage alive. Output has the same wire shape as an ordinary
owning field, without reference identity metadata. Scoped translators can borrow
ordinary payload records containing arrays for immediate output without copying
their buffers. Shared/weak ownership modes are optional; `shared(payload)` retains
an independent payload, while `shared(owner.field)` retains its complete owner.
These references are export-only; read their output with an ordinary owning schema.
Existing named durable address
references remain a separate supported API.
This feature was introduced with editor packages **2.0.4**.
The [reference export examples](example/references/README.md) demonstrate the
one-argument bindings, value-only wire equivalence and scoped translation.

Compiler/runtime **2.5.0**, language **1.18.0**, adds named borrowed, shared and
weak [field references and scoped translators](docs/schema/references.md).
References carry a portable typed address and bind to a live owner explicitly;
shared C++ bindings retain the owner through `std::shared_ptr`. Loading an address
requires application resolution after constructing its owners. Translators read
source projections during synchronous target output without constructing a whole
owning target. Backend lifetime rules and unsupported shapes are documented in
the feature guide; automatic graph traversal and other-language managed engines
are separate capabilities.
The separate [durable address examples](example/address_references/README.md) demonstrate live
field updates, shared owner retention, address resolution and one-time projected
output in all eleven languages, with a standalone installed-package C++ build.

Compiler/runtime **2.4.0** adds XML text with scalar attributes and explicit
collection wrappers across all generated languages, pinned local XSD bundles and
[XSD export/compatibility tools](docs/serialization/xsd_tooling.md).
[Native format selection](docs/languages/output_configuration.md) now supports
`binary_none` across all backends while XML remains independently selectable.
C++ adds cooperative compression cancellation, ZIP64, opt-in bounded spooling,
transactional OPC aggregate loading,
opaque extension policies and canonical package ordering. See
[the API contract](docs/serialization/xml_packages.md) and the
[remaining requirements](docs/pending.md). This was the initial XML/package
checkpoint; current 2.28.0 support is described in the linked contract.
Schema language 1.17.0 and existing wire formats were unchanged by that release.

For OPC application/library responsibilities, remaining 3MF authoring requirements
and a restricted schema/build example, see the
[OPC and 3MF authoring design](docs/design/opc_3mf_support.md). The C++ OPC runtime
generates standard content-type and relationship metadata; application schemas
describe payloads. Package declarations generate complete assembly and typed
loading in all eleven outputs under the documented shared pipeline contract.
The design also covers
[schema-first binding and XSD declaration import](docs/design/opc_3mf_support.md#schema-first-authoring-and-declaration-import),
which remove duplicate field placement lists for the supported XSD subset.
The [XML authoring workflow](docs/design/opc_3mf_support.md#schema-first-authoring-and-declaration-import)
declares placement once in XSD or alongside Serializer fields. Existing INI
mappings remain available for applications that explicitly choose them.

The [managed entity-reference contract](docs/design/managed_entity_references.md)
describes C++ typed tokens, stable class tags, identity-preserving editable values
and isolated previews. Collaboration now reserves native ID ranges and retains
identical document/object IDs across participants. Generated `persistent_id`
payloads default to `compact_varint`; select `[managed] id_encoding = fixed` when
fixed-width encoding is wanted. Memory width still defaults to `uint32`.
See the [persistent-ID migration guide](docs/guides/persistent_id_migration.md).
Build a schema-specific offline converter with
`serializer_add_migration_tool(TARGET migrate_model SCHEMA model.serializer ROOT app::document)`.
It supports explicit fixed/compact ID policies for managed saves and raw models;
raw input/output binary formats can also differ.
Migration is [qualified against rebuilt 2.6.0 artifacts](docs/verification/verification-managed-legacy-migration-2026-10-20.md)
for the recorded direct uint32 and separated uint64 profiles, preserving exact
identity, retained history and allocation state.

Compiler/runtime **2.3.1** preserves schema license notices without adding an
output-license banner. Generated files retain the minimal Serializer provenance
comments; see [generated-output licensing](docs/project/licensing.md).

Compiler/runtime **2.3.0** adds optional generated XML mappings and fixed XSD
bindings across all output languages, optional prepared runtime validation,
whole-object C++ compression streams, and codec-independent C++ ZIP/OPC APIs.
General format features require cross-language coverage; remaining package,
compression and document-state work is listed explicitly in the
[pending report](docs/pending.md). See [implemented XML/package APIs](docs/serialization/xml_packages.md)
and [generated XML examples](example/xml/README.md).

Compiler/runtime **2.2.0** adds `model_store::discard_journal_changes()` for ordinary
journaled C++ stores. Under the native writer lock it checks the physical file
budget but reads only the fixed header and bounded last full-Save frame, without
reading or buffering an appended unsaved suffix. It validates and restores the
saved values and complete history, and
atomically replaces the journal generation. Object/revision allocation high-water
marks never decrease, abandoned edits cannot reappear through redo/recovery, and
the durable operation sequence advances once. Creation is the baseline before the
first Save. Shared or unattached stores reject this operation. Preparation failures
preserve the live state; uncertain replacement fences the writer until fresh
recovery. This operation adds no saved-envelope copy or work to ordinary edits and
changes no schema language or existing wire format.

The native byte-stream `file_open_mode::read_shared` permits a bounded read while
an existing Windows writer is open. Callers must supply external synchronization;
the journal uses its stable writer lock. Ordinary `read`, `update`, `create` and
`lock` sharing behavior is unchanged.

Release 2.1.1 makes every maintained Go example a native Go project. With Go
1.22+ and the installed `serializer` CLI on `PATH`, run `go generate .`,
`go build -o build/ .`, and `go test ./...` in an example folder. Application code
imports its generated `internal/model` package. This workflow needs no CMake,
Python, C++ runtime, or cgo; the repository matrix runners remain independent
qualification tools. See [Go projects](example/go/README.md) and
[2.1.1 migration](migration.md#compilerruntime-211).

Release 2.1.0 requires CMake 4.0 or newer and adds standalone CMake entry points
for individual examples in every supported language. Configure a folder such as
`example/cpp/basic` directly, then build and run its checks with CTest. See
[individual example builds](example/README.md#build-an-individual-example-with-cmake)
and [2.1.0 migration](migration.md#compilerruntime-210).

Installed packages expose their enabled compression backends so standalone
examples can detect capabilities in both static and shared runtimes.

Release 2.0.2 fixes private arithmetic helper names in generated Rust output for
strict native Windows builds with `rustc 1.99` and `-D warnings`, and runs the
self-checking conversion examples correctly through the shared example runner.
Regenerate affected Rust output and rebuild its consumers; schemas, public model
and function names, and wire formats are unchanged. See
[2.0.2 compatibility](migration.md#compilerruntime-202) and
[Rust qualification](docs/verification/verification-rust-windows-2026-10-15.md).

Release 2.0.1 fixes strict Clang builds of the digest runtime on 32-bit ARM.
Schemas, public APIs, generated output, digest results and wire formats are unchanged;
see [2.0.1 compatibility](migration.md#compilerruntime-201).

Release 2.0.0 changes the default generated C++ artifact layout and the CMake
package major version. Copy or install each entry header's adjacent `.includes/`
tree, or select `--include-mode inline` for one combined header. Versioned
consumers should request `find_package(Serializer 2.0 CONFIG REQUIRED)` and
regenerate through their build rules. Supported 1.x schema headers, generated
model names, runtime APIs and wire contracts retain their existing behavior;
see [2.0 migration](migration.md#compilerruntime-200).

Use `serializer version 1.23.0;` for new schemas. Older compatible 1.x schemas
remain supported. The original
`serializer version 1;` is an exact compatibility alias for `1.0.0`, exclusive to
version 1. Future versions require all three components; see the
[schema language policy](docs/guides/command_line.md#schema-language-version-policy).

**Supported outputs:** C++, Java, JavaScript, TypeScript declarations, Go, C#,
Rust, Python, Swift, Kotlin, and C. TypeScript uses the JavaScript runtime.

Generated C++ retains schema declaration order, combines consecutive declarations
with the same access, and preserves source blank-line grouping between fields. See
[C++ output layout](docs/languages/output_configuration.md#names-compatibility-and-generated-syntax).

Language `1.17.0` adds [explicit array and callable APIs](docs/schema/array_and_callable_api.md):
`expose array(public)` selects mutable/const native C++ indexing and ranges;
`readonly` disables writes, `const_value` preserves value-returning const APIs,
and `dimensions(Rows, Cols)` states row-major axes. Portable output uses checked
logical helpers. Owner-type field `default(identity)` keeps omitted/fresh
factory behavior without class-wide constraints. Callable guards, array
bindings/generation, structural delegation and richer C++ signatures remove
supported host adapters. Backend boundaries and
[qualification status](docs/verification/verification-array-callable-api-2026-10-13.md)
are documented separately. Native **134/134**, all eleven SDKs, editor checks
and both **1.1.46** packages passed the recorded qualification.

Language `1.16.0` adds [generic owner-parameter defaults](docs/schema/construction.md#generic-owner-parameter-defaults-in-language-116):
`constructor(int32 count, Frame placement = {})` preserves open C++ representations
without adding a factory or defaultability constraint. Omission creates a fresh
`Frame{}`; explicitly supplied nondefault/nonassignable frames remain valid.
Independent input deduction and fixed-array cardinality keep their existing
boundaries. Existing `1.15.0` record defaults and `1.14.0` lowering remain unchanged.
See [qualification](docs/verification/verification-generic-owner-defaults-2026-10-12.md)
for actual verification status.

Language `1.15.0` adds [empty class-valued parameter defaults](docs/schema/construction.md#empty-class-valued-parameter-defaults-in-language-115):
`constructor(int32 a, int32 b, area input = {})` creates a fresh owning `area`
with its schema field defaults when omitted. Named factories support the same
syntax. Full positional defaults such as `area input = {10, 20}` remain available
since `1.14.0`; ordinary function parameter defaults are unavailable. No field
identity or wire format changes. See [qualification](docs/verification/verification-empty-record-defaults-2026-10-12.md)
for verification status.

Language `1.14.0` adds [owning record APIs](docs/schema/owning_api.md): readonly
lvalue C++ getters, direct member initialization, typed fixed-array/nested defaults,
reciprocal constructor signatures, declaration-site C++ additions, and explicit
arithmetic operator mappings with owning constructor calls in portable expressions.
Existing fields, wire formats and managed protocols retain their contracts.
Run the [four focused examples](example/README.md#owning-record-apis) and consult
[qualification](docs/verification/verification-owning-api-2026-10-12.md) for tested
backends and limits. Both current editor packages are **2.0.11**, with separate
C++ generated-header navigation. See
[editor verification](docs/editors/editor_extension.md#verification-performed)
for package checks and navigation qualification.

Language `1.13.0` adds the C++ function qualifier `noexcept(cpp)` for readonly
native bodies and external declarations, including dispatched member contracts.
It emits C++ `noexcept` as an author promise; other outputs retain their own
failure behavior. Portable expressions and predicates keep their inferred C++
guarantees. See [the contract and migration example](docs/schema/behavior.md#explicit-c-noexcept-contracts)
and [verification status](docs/verification/verification-noexcept-2026-10-12.md).

Language `1.12.0` adds C++ constructor overloads, nested independent input types,
constrained readonly functions with explicit `common_type<T, Input>` numeric
results, native `storage(none)` and `constexpr(cpp)` qualification, and authoring
validation/initialization hooks. Generic record constructor defaults require an
explicit `factory(name)` constraint. See [construction](docs/schema/construction.md),
[behavior](docs/schema/behavior.md), and [generics](docs/schema/generics.md) for contracts and
backend limits. The C++ registry also supports guarded, versioned borrowed
[external-provider leases](docs/managed/state_registry.md#borrowed-external-provider-leases).
See the [verification record](docs/verification/verification-authoring-2026-10-11.md).

Language `1.11.0` adds lossless `checked<T>(value)` numeric bindings,
representation-preserving integer arithmetic, projected readonly record calls,
lazy owning-record conditionals, generic record map keys, and extended typed
construction. See [behavior](docs/schema/behavior.md), [construction](docs/schema/construction.md),
and [generics](docs/schema/generics.md) for backend limits. C++ managed
[state registries](docs/managed/state_registry.md) add pinned typed lookups, stable field
paths, custom-key uniqueness and optional ordered/hash indexes while retaining
the existing identity, history, journal and collaboration protocols.
Mostly-disk indexing is a [separate design proposal](docs/design/paged_indexing.md).
The [combined storage and indexing specification](docs/design/database_storage_requirements.md)
retains the qualified C++23 minimum and defines standalone indexes, database-backed
data, independent local/database/both index placement, typed `find_by` and
specialized methods. Its schema/query APIs remain proposals. Compiler/runtime
2.30.1 requires C++23 for standalone and database use; see the
[C++23 migration](migration.md#c23-minimum-in-2301).
The [custom-ID examples](example/custom_ids/README.md) demonstrate application-owned
scoped keys in every language and a C++ managed registry companion.
See the [enhancement verification record](docs/verification/verification-registry-numeric-2026-10-11.md)
for executed native, SDK and editor checks and their platform limits.

Language `1.10.0` adds [namespace functions and declared readonly calls](docs/schema/behavior.md#declared-function-calls)
inside portable expressions and [Boolean generic type constraints](docs/schema/generics.md#boolean-type-constraints),
such as `T: (integral || floating_point) && !bool`. See the runnable [expression example](example/cpp/expressions/README.md).
C++ emits eligible functions and codecs as `constexpr`, useful results as
`[[nodiscard]]`, and proven nonthrowing operations as `noexcept`.
See the [verification record](docs/verification/verification-functions-constraints-2026-10-11.md)
for the native, all-language and editor checks completed for these additions.

Language `1.9.0` adds [derived native binary sizes](docs/schema/derived_sizes.md), including
`wire_only` headers and `size_scope(record|payload)`, comma-separated
[`omit(all_except(...))`](docs/schema/magic_and_omission.md), explicit
[record conversions](docs/schema/conversions.md), and generic
[`stateless` constraints](docs/schema/generics.md#stateless-type-constraints).
All language outputs share these schema contracts. Decoder size validation can
be configured per language and class. See [runnable examples](example/README.md)
and the [qualification record](docs/verification/verification-deferred-features-2026-10-11.md).
C++ also derives positional size headers in its
[compile-time and emission-only output](docs/serialization/constant_evaluation.md), producing
the same schema byte image as normal runtime encoding.

Language `1.8.0` adds [portable constructors, factories, structured defaults,
and empty transient layout hints](docs/schema/construction.md), owning generic
[inheritance](docs/schema/generics.md), and checked comparisons, boolean guards,
division and `math.sqrt/sin/cos` [expressions](docs/schema/behavior.md). Typed readonly
predicates add exact integer/enum checks, member/index access and bounded `all`/`any`
reductions, returning false for reached invalid checks.
Schema constructors retain a public C++ default constructor automatically;
default arguments are separate. Generic factory parameter defaults require the
explicit language `1.12.0` contract described above.
See the [construction guide](docs/schema/construction.md).
Compiler/runtime `1.13.0` adds allocation-free bounded keyed binary counting,
borrowed span/reference output, an opt-in signed-zero output policy, reusable C++
generated type guards, [class-specific record comparisons](docs/schema/record_comparisons.md), and
guarded transaction collection callbacks. Existing default codecs and wire
identities remain unchanged. See [usage](docs/guides/usage.md) and
[verification](docs/verification/verification-integration-enhancements-2026-10-11.md).
Algorithm-qualified digest return signatures retain the existing built-in digest
representation; see [digest values](docs/schema/digest.md).

### Borrowed native binary output

Native C++ binary output and bounded counting accept `std::span` and
`std::reference_wrapper` directly, including nested generated records, optionals,
and map payloads. They preserve the owning value's wire bytes and output policies
while borrowing source storage for each synchronous call. Keep the referents alive
and unchanged through traversal; see [borrowed output](docs/guides/usage.md#borrowed-native-binary-values).

**Start here:** [Usage guide](docs/guides/usage.md) · [Runnable examples](example/README.md) ·
[CMake integration](docs/guides/cmake_integration.md)

Payload [versioning and schema evolution](docs/schema/versioning.md) support `version`,
`compatibility`, `created`, `obsolete`, `replaced`, and class-scoped `reserve` declarations.
Release catalogs and nested allow policies (`any`/`all`, age/count/date limits) resolve
to compatibility bounds during generation; generated codecs contain no release-date
processing. All eleven language outputs share current/historical read policies and versioned
positional binary layouts. See the [versioning examples](example/README.md#versioning).

Schema-owned [magic headers and format omissions](docs/schema/magic_and_omission.md)
use `private magic { 'SRLFILE' };`, typed `private magic uint32 (99) {42};`,
and `omit(json, binary_positional)`.
These declarations require owning classes; view modes reject them explicitly.
Magic is static metadata: writers emit it, readers verify and discard it, and
JSON includes the fixed `magic` value by default. Native binary writes legacy
header bytes or the declared scalar/enum encoding before the separate version discriminator. C++ format selection
and omissions resolve at compile time without per-object flags.
C++ byte magic uses readable character literals and a comment showing its escaped
contents, preserving the exact bytes without an implicit terminator.

Language `1.2.0` adds [compact unsigned scalar fields](docs/schema/compact_integers.md):
`public compact_prefix strict uint32 value (3) {32};` uses the existing 30-bit
prefix encoding, while `compact_varint uint64 count;` uses full-width unsigned
LEB128. Explicit `lenient` selects deterministic truncation. All output languages
support these owning scalar annotations; JSON retains ordinary integer values.
The [signed/floating design contract](docs/design/compact_scalar_contract.md)
specifies future payload mathematics and reference vectors; its five proposed
keywords remain unavailable in the current schema language.

Language `1.3.0` adds [digest bytes and creation](docs/schema/digest.md): bare `digest`
stores a variable-length application-supplied value, `digest[32]` fixes its byte
length, and `digest(sha256)` selects a standard algorithm's length. All native
outputs support digest storage/codecs; C++ includes 0BSD creation and hex helpers
without an OpenSSL dependency. Fields do not compute hashes automatically.

Language `1.4.0` adds [owning variants](docs/schema/unions_and_variants.md):
`public variant(string = text, event = event) payload;` generates C++
`std::variant` storage with safe construction, copying, moving, and destruction.
JavaScript/TypeScript expose one `{kind, value}` payload. Existing `union`
declarations retain their generated APIs. Both keywords use the same native wire
indices and names, and [paired examples](example/README.md#union-and-variant)
cover every output language.

Compiler [simplicity warnings](docs/schema/schema_reference.md#simplicity-warnings) suggest
schema-owned `magic` for initialized fields named `magic`, and the `version`
keyword for supported scalar fields named `version` or `revision`. A `version`
declaration can omit its default member name: `public version uint32 (1) { 1 };`.
Language `1.5.0` adds field-local `ignore(warning magic, version)` before the
semicolon for intentional uses. Warnings do not change generated APIs or bytes;
review an existing wire contract before replacing an ordinary field.

Language `1.7.0` adds [generic optional values](docs/schema/optional.md):
`optional<T>` stores absence or an owning value and maps directly to C++
`std::optional<T>`. Nested optionals and optional arrays/maps have distinct
JSON and native binary representations across every output language.

C++23 [constant-evaluation binary output](docs/serialization/constant_evaluation.md) can count a
value's exact positional size, write caller-owned memory, or return an exact-size
byte array from a deterministic factory. Enable generated models with
`--cpp.constant_evaluation true`; its default is false. Ordinary runtime codecs
retain their existing traversal, and ordinary owning runtime memory writes use the
existing encoder.
The independent `--cpp.protocols binary_none` profile restricts C++ output to
positional codecs; the default remains `all`. Add `--cpp.emission_only true` with
those settings for borrowed string/span DTOs exposing only one concrete positional
`serialize_out(binary_none_output&)` method.

Release **1.15.0** also makes ordinary generated `serialize_out`, `serialize_in`,
and their uncompressed stream entrypoints `constexpr`, independently of that option.
Constant evaluation depends on the chosen protocol, stream, and value operations;
the direct native binary scalar subset is described in the
[C++ qualifier guide](docs/serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers).
Eligible checked arithmetic and dotted-version construction retain runtime calls.
Value results use `[[nodiscard]]`; proven nonthrowing operations use `noexcept`.

## Proprietary applications and licensing

The schema/application author selects the generated output's license, including
proprietary terms. Generated files preserve the schemas' copyright and license
notice text, including required notices from included schemas, with minimal
generation provenance. Serializer adds no output license or permission preamble;
a schema with no notices receives no inferred license notice.

[LICENSE-GENERATED](LICENSE-GENERATED) grants permission to use Serializer-authored
emitted support under your application's chosen terms. Its upstream 0BSD permission
does not label the whole generated file or override input/dependency conditions.
Generation does not transfer ownership of either the schema or Serializer support.

Application runtime headers, compiled helpers, and managed support use the
[Zero-Clause BSD license](LICENSE-RUNTIME). Link `Serializer::runtime` for generated
C++ models or `Serializer::managed` for managed applications. Generation helpers
choose the runtime target automatically. The compiler and its
`Serializer::serializer_lib` parser/generator API remain
[GPL-3.0-or-later](LICENSE). Frozen Unicode pattern data retains the
[Unicode data license](LICENSE-UNICODE), installed with the runtime licenses.
Third-party dependencies retain their own licenses.
See [licensing and schema notices](docs/project/licensing.md) before redistributing a package.
The [licensing rationale](docs/project/licensing.md#why-the-license-was-separated)
explains why application support has separate permissions while the compiler
retains GPL, and how those permissions differ from copyright ownership.

## How it works

1. Describe your types, fields, and defaults in a `.serializer` file.
2. Run the compiler, or let CMake generate code during your build.
3. Use the generated classes to read and write your chosen format.

The compiler and generators are written in C++. Generated non-C++ codecs run in
their target language without a native Serializer runtime dependency.

## Get started

### 1. Write a schema

Save this as `person.serializer`:

```text
serializer version 1.5.0;

namespace demo {
  class person {
    public string name;
    public uint32 age;
  }
}
```

Every schema begins with a supported language header. `array[]` and typed `magic`
require `serializer version 1.1.0;` in the file that declares them; the original
`1` alias remains exactly `1.0.0`.
Each field declares its access
level, type, and name. Start with [schema examples](docs/schema/schema_examples.md) for
arrays, maps, enums, and defaults.

### 2. Generate code

After [building or installing Serializer](docs/guides/cmake_integration.md), run the
compiler to generate a C++ header:

```sh
serializer --input person.serializer --output person.hpp
```

When the entry schema includes other schemas, C++ generation emits separate
dependency headers by default and includes them from the entry header. Use
`--include-mode inline` or `[output] include_mode = inline` to keep one combined
header. See [schema includes](docs/guides/usage.md#share-declarations-with-includes)
for output layout and migration details.

Use [output configuration](docs/languages/output_configuration.md) to choose a language or
naming profile. The [command-line guide](docs/guides/command_line.md) lists all options.

To generate several configurations of the same schema from one parse, repeat
`--config` and supply corresponding output paths. Existing INI files
remain independent; see [batch generation](docs/guides/command_line.md#generate-several-configurations-from-one-parse).

### 3. Add it to your build

For a C++ application with Serializer already added as a CMake dependency:

```cmake
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS person.serializer)
```

A normal build generates the header and supplies the include directory, runtime
library, and C++23 requirement. See [CMake integration](docs/guides/cmake_integration.md)
for complete source-dependency and installed-package setups.

Use [`serializer_generate_variants`](docs/guides/cmake_integration.md#generate-several-configurations-from-one-parse)
when several C++ targets need different configurations of the same schemas. Each
schema is parsed once and generates every target's variant in that build command.

Next, follow the [C++ usage guide](docs/guides/usage.md) to serialize a value and decode a
complete message with explicit limits, or run the [basic C++ example](example/cpp/basic/README.md).

### Build an editable C++ application

Follow the [managed-state quick start](docs/managed/getting_started.md) and
[runnable managed examples](example/managed/README.md) to generate typed editors,
link `Serializer::managed`, and edit a `model_store` through transactions. Start
with local history, then add a journal or collaboration when the application needs
them. History mode and journal/collaboration support can be selected at compile
time; the application supplies transport, synchronization scheduling, and trusted
sessions for collaboration.

The managed runtime is optional for serialization-only applications. Fresh source
builds expose it by default; ordinary schemas do not acquire managed identity or
history storage merely because it was built.

## Choose a language

Reusable [schema generics](docs/schema/generics.md), such as `class result<T>`, emit
native C++ templates: application code can use `result<std::uint32_t>` without
concrete schema declarations. Nested templates, trailing defaults, positive uint64
dimensions, and fixed `array[N] T` storage are supported in C++.
Language `1.1.0` adds `array[] T` with an extent inferred from nonempty defaults:
`public array[] uint32 values {1, 2, 3};` produces `std::array<std::uint32_t, 3>`.
`public array[] char signature {'SRLFILE'};` uses seven decoded bytes with no NUL terminator.
Optional `instantiate person_result = result<person>;` declarations and concrete
schema fields define contracts for all eleven generators. Other languages keep
concrete APIs. All eleven ordinary outputs support fixed arrays without explicit
initializers, enforcing the extent during encoding and decoding. Direct Protobuf
fixed-array output remains unsupported. See the [generic examples](example/generics/README.md)
and [qualification record](docs/verification/verification-dimensions-2026-10-04.md).

All languages use the same schema compiler. Their generated APIs and runtime
requirements are documented separately.

| Output | Guide | Examples |
| --- | --- | --- |
| C++ | [Usage and decoding](docs/guides/usage.md) | [C++ examples](example/cpp/README.md) |
| Java | [Java 17+ codecs](docs/languages/java.md) | [Java examples](example/java/README.md) |
| JavaScript / TypeScript | [Portable language APIs](docs/languages/portable_languages.md) | [JavaScript](example/javascript/README.md), [TypeScript](example/typescript/README.md) |
| Go / C# | [Portable language APIs](docs/languages/portable_languages.md) | [Go](example/go/README.md), [C#](example/csharp/README.md) |
| Rust / Python / Swift / Kotlin / C | [Native language APIs](docs/languages/native_languages.md) | [All language examples](example/README.md) |

The [build and generation guide](docs/guides/build_and_generation.md) includes compiler
commands for each language group and explains naming profiles.

## Schema behavior and managed state

Language 1.6 and compiler/runtime 1.11 add schema-declared methods, ordered native
blocks, typed portable arithmetic and transient memory fields. C++ managed models
support private data, unrelated public bases, owned variants, concrete generic
specializations and payload lifecycle metadata. Runtime APIs add guarded cache
updates, explicit reset clones, notifications, navigation, complete-history
migration and bounded snapshot patches.

The [custom C++ example](example/cpp/custom_code/README.md) demonstrates native
preambles, class declarations, inline method bodies, and an application-owned
method definition alongside generated codecs.

Read [concrete state framework examples](docs/managed/state_enhancements.md) and
[method generation](docs/schema/behavior.md) before choosing a combination. The
[integration skill](.agents/skills/serializer-integration/SKILL.md#use-the-full-framework)
requires reviewing the full feature set and using every applicable implemented
feature. Runnable document-tool examples cover generated APIs and failure paths;
remaining engine/storage combinations are listed explicitly.

## Choose a format

Serializer supports four native protocols across its generated language codecs.

| Format | How fields are represented |
| --- | --- |
| JSON | Named fields in readable text |
| Positional binary | Values in schema order, without field keys |
| Integer-key binary | A numeric field ID before each value |
| String-key binary | A wire field name before each value |

Native JSON supports `std::optional<T>` as a host type (`null` or a typed value).
For evolving JSON documents, use the opt-in `read_policy::flexible`
reader to skip additional fields while rejecting duplicate keys and enforcing
decode limits. Default JSON input remains strict. Regenerate classes to enable
the unknown-member protocol hook; see [JSON compatibility](docs/guides/usage.md#json-compatibility).

Native binary codecs default to little-endian byte order. Strings require valid
UTF-8; use `array uint8` for arbitrary bytes. Read the
[wire-format contract](docs/serialization/wire_format.md) before exchanging data between systems.

C++ also offers optional [Protobuf binary, ProtoJSON, and TextProto codecs](docs/serialization/protobuf.md).
These are separate from the four native protocols.

For evolving schemas, use [explicit field IDs](docs/schema/schema_reference.md#explicit-field-ids-with-stable_ids)
and the [schema compatibility checker](docs/schema/schema_evolution.md). Stable IDs alone
do not make native readers accept unknown fields. Evolving JSON documents can
explicitly select `read_policy::flexible` after regenerating their classes.

## Build and test

Fresh builds enable managed history, journaling, collaboration, SIMD, XML, and all
compression backends. Libxml2, Zstandard, LZ4, and zlib are required; the repository vcpkg
manifest acquires them through its default features. Existing caches retain their
settings; use `-U SERIALIZER_BUILD_MANAGED -U SERIALIZER_WITH_*` to adopt the new
defaults, or set each option explicitly.

The default development build needs CMake 4.0+, a C++23 compiler and standard
library, clang-format 19+, and GoogleTest 1.18.0 or newer. See the
[requirements and setup guide](docs/guides/cmake_integration.md#build-this-repository)
for dependency installation and optional tools.
GoogleTest is a repository-test dependency supplied by the vcpkg `gtest` package
or an installed GTest CMake package; it is separate from the application runtime.
Windows test builds automatically copy imported shared runtime DLLs beside each
test executable so CTest and direct launches can load them.

**`clean` and `rebuild` delete all untracked and ignored files and directories
through `git clean -fdx`, including untracked source, local configuration, build
caches, dependencies, and editor packages.** Tracked files and local tracked
edits remain intact. Cleanup covers the repository regardless of build-directory
or configuration options.

On Linux with GNU Make:

```sh
make all
make test
make clean
make rebuild
# WSL with a Windows/MSBuild host:
make extension
```

On Windows with Visual Studio Developer PowerShell (x64):

```powershell
./make.ps1 all
./make.ps1 test
./make.ps1 clean
./make.ps1 rebuild
./make.ps1 extension
```

The compiler rejection tests invoke MSVC directly. Developer PowerShell supplies
the compiler's standard-library and Windows SDK include paths for those tests.

Both wrappers provide synchronized `configure`, `all`, `test`, `clean`, `rebuild`,
and `extension` commands. `clean` verifies the repository root and runs Git cleanup;
Git is required and CMake is not. Git metadata, the index, and nested Git
repositories protected by Git's single-force semantics remain intact. External
build/toolchain directories are outside the cleanup scope. `rebuild` cleans the
repository first, then configures and uses CMake's `--clean-first` to finish native
cleanup before rebuilding.

Windows `all` and `rebuild` also package both editor extensions. They require
Node.js 22+, npm, and Visual Studio MSBuild; they do not install the packages.
Linux `all`, `test`, and `rebuild` build native CMake targets. Windows `test` and
`configure` remain CMake-only.

`./make.ps1 extension` builds only the two editor packages, without configuring
or building CMake targets or cleaning the repository. WSL `make extension` invokes
the same Windows packaging script through PowerShell. Both require the Windows
tools above; native Linux cannot build the Visual Studio VSIX and reports that
limitation.

For direct CMake commands, presets, builds without GoogleTest, and troubleshooting,
see [build and generation](docs/guides/build_and_generation.md#build-and-test).

The upstream vcpkg package is `rohit-singh-gautam-serializer`:

```sh
vcpkg install rohit-singh-gautam-serializer
```

Package availability can lag the source repository. Check the port version and
build options for the features you need; use the
[source-build guide](docs/guides/cmake_integration.md#build-this-repository) for the
documented current source capabilities. The root `vcpkg.json` acquires development
dependencies and is separate from the upstream port; see
[distribution guidance](docs/project/distribution.md).

## Editor support

Both **Rohit Serializer** extensions highlight `.serializer` files and provide
navigation between schemas and existing generated output in all 11 output languages.
The extensions are separate from the compiler and runtime.

| Editor | What it adds | Setup |
| --- | --- | --- |
| Visual Studio Code | Highlighting, snippets, schema navigation, CMake generation commands, and missing-include assistance | [VS Code guide](docs/editors/editor_extension.md) |
| Visual Studio 2022 / 2026, Windows x64 | Highlighting, editing configuration, native declaration/type-definition navigation, F12, and Ctrl+click | [Visual Studio guide](editors/visual_studio/README.md) |

Both extensions use source release version **2.0.9**, versioned independently from
the compiler/runtime. Embedded `cpp { ... }`, `java { ... }`, and other target blocks reuse
available language grammars for syntax highlighting. Install the language's grammar
provider where the editor does not bundle it; this does not add embedded IntelliSense.
See [embedded source highlighting](docs/editors/editor_extension.md#embedded-source-highlighting)
and the [navigation coverage matrix](docs/editors/editor_navigation.md#navigation-coverage-matrix)
for supported destinations and language-service prerequisites.
VS Code also supports navigation from read-only Git index/history tabs, using
the displayed snapshot and current workspace destinations.
From a typed external function declaration in a schema, **Go to Definition**
also locates available authored C++ out-of-class definitions. Lookup matches the
qualified owner and overload signature, reads live buffers, and never builds.
See [deferred function definitions](docs/editors/editor_navigation.md#deferred-function-definitions-1147)
for lexical matching limits and verification status.
See the [custom code and highlighting verification](docs/verification/verification-custom-code-2026-10-11.md)
for the completed native, editor, grammar-provider, and package checks.

## Explore advanced features

Start with these only when your application needs them.

For application editing, start with the
[history, journal, collaboration, and authorization guide](docs/managed/getting_started.md).
It explains what each feature does, how they work together, and what your
application must provide.

| Need | Guide |
| --- | --- |
| Share declarations across schemas | [Includes](docs/guides/usage.md#share-declarations-with-includes) |
| Read or update encoded C++ data through borrowed views | [Buffer views](docs/serialization/views.md) |
| Produce exact positional bytes during C++23 constant evaluation | [Constant binary APIs](docs/guides/usage.md#count-and-emit-constant-positional-bytes) |
| Encode directly into an owned `std::string` buffer | [String streams](docs/guides/usage.md#string-backed-output-buffers) |
| Use standard streams or durable files | [Streams and adapters](docs/guides/usage.md#stream-concepts-and-implicit-adapters), [file streams](docs/guides/usage.md#file-streams-and-journal-records) |
| Compress complete C++ messages | [Compression](docs/serialization/compression.md) |
| Group edits and support undo/redo in C++ | [Managed runtime](docs/managed/cpp_runtime.md), [small examples](example/managed/README.md) |
| Recover managed state after a crash | [Journaling](docs/managed/journal.md) |
| Synchronize edits between managed stores | [Local collaboration](docs/managed/local_collaboration.md) |
| Control which sessions may publish edits or acquire locks | [Authorization](docs/managed/authorization.md) |
| Understand runtime and generated-code optimizations | [Performance features](docs/serialization/performance.md) |
| Plan database persistence | [Database integration](docs/guides/database_integration.md) |
| Plan indexes larger than RAM | [Paged indexing research and proposed design](docs/design/paged_indexing.md) |

Native managed runtimes outside C++ and Serializer database adapters are not
implemented. The [feature status guide](docs/project/feature_status.md) separates available
features from proposals and records their limitations.

Disk-backed paged indexes are also proposed. The current managed store keeps its
model, identity table, and retained snapshots in memory; paging an index alone
would not make the model larger than RAM. See the
[paged indexing study](docs/design/paged_indexing.md) for existing database approaches,
cache limits, compatibility requirements, and staged implementation criteria.

## Integrate with a coding agent

Give your agent this request, adapting the dependency path:

```text
Read vendor/Serializer/.agents/skills/serializer-integration/SKILL.md
and use it to integrate Serializer into this application's CMake build.
```

The skill covers schemas, generation, codec selection, and bounded decoding.
Keep the tracked `.agents/skills/serializer-integration/` directory when copying
or packaging the source repository.

See [agent discovery and setup](docs/guides/agent_integration.md) for automatic discovery
and use from a consuming project.

## Typed command-line declarations

Use `src/command_line.hpp` and `cli::commandline_declaration` for common options,
subcommands and typed retrieval. Declare `command_options` and `command_entry`
values, call `decl.parse(argc, argv)`, then read `decl["command"].get_path("input")`
or other typed getters. The library owns variant values, validates required
options and numeric conversions, and generates global/command help. It supports
repeatable string/path lists, explicit positional inputs, command tails, and
native Windows argv. Failed reparsing preserves the previous successful result.

Keep option types, defaults and requirements in declarations; handlers consume
typed values without reparsing. Use `decl.common()` for common options and
`was_provided` to distinguish defaults from explicit input. Check
`help_requested()` and write help before calling `cli::dispatch_command(decl)`.
The original compiler string-map API remains compatible. See the
[declaration guide](docs/guides/command_line.md#typed-command-line-declarations) for complete examples, getter types, ownership,
error behavior, and positional/tail syntax. This source utility is not an
installed runtime header.

## Contributing and community

Start with the [contribution guidelines](CONTRIBUTING.md) to report bugs, propose
features, or submit a pull request. Participants follow the
[code of conduct](CODE_OF_CONDUCT.md). Report vulnerabilities privately using the
[security policy](SECURITY.md); see the [accessibility statement](ACCESSIBILITY.md)
for reporting barriers to using the project.

## Reference and project status

- [Product positioning](docs/project/product_positioning.md): shared descriptions and capability boundaries for documentation and distributions.
- [Schema and protocol reference](docs/schema/schema_reference.md): types, field IDs, views, and protocol examples.
- [Schema examples](docs/schema/schema_examples.md): small declarations you can adapt.
- [Migration guide](migration.md): upgrade requirements and compatibility changes.
- [Feature status and roadmap](docs/project/feature_status.md): implemented behavior and future work.
- [Constant-evaluation guide](docs/serialization/constant_evaluation.md): implemented C++23 positional
  size counting, existing-memory writing and exact-array factories, opt-in generation,
  runtime path preservation, supported values and compiler qualification.
- [Four-toolchain verification](docs/verification/verification-toolchains-2026-10-07.md): x64 Windows/MSVC and clang-cl, and Ubuntu WSL2/GCC and Clang, with default runtime features enabled.
- [Optional qualification](docs/verification/verification-optionals-2026-10-11.md): nested presence, all eleven language SDKs, managed recovery, examples, and editor checks.
- [Default runtime verification](docs/verification/verification-default-features-2026-10-07.md): managed/compression defaults and installed-header checks.
- [Compact scalar qualification](docs/verification/verification-compact-2026-10-07.md): all eleven SDKs, frozen wire fixtures, strict/lenient cases, native tests, and editor checks.
- [Verification record](docs/verification/verification-2026-09-17.md): tested revisions, configurations, and outstanding checks.
- [Versioning verification](docs/verification/verification-versioning-2026-10-06.md): historical layouts, all eleven language examples, and editor checks.
- [Release-policy verification](docs/verification/verification-release-policies-2026-10-06.md): compiler folding, calendar boundaries, nested policies, and generation options.
- [Qualification](qualification/README.md): interoperability, fuzzing, and performance workflows.
- [Coding standard](CodingStandard.md) and [agent instructions](AGENTS.md): repository contribution rules.
- [Licensing](docs/project/licensing.md): proprietary generated output, permissive application runtime, and GPL compiler scope.
- [Digest storage and creation](docs/schema/digest.md): byte contracts, supported algorithms, C++ helpers, and provider/licensing boundaries.
- [Digest verification](docs/verification/verification-digest-2026-10-09.md): recorded native, cross-language, and editor checks and their scope.

When updating an existing application, check the migration guide and regenerate
affected output before rebuilding. Verification records describe specific tested
configurations; they do not establish support for every platform or toolchain.
