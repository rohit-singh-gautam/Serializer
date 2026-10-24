---
name: serializer-integration
description: "Integrate Serializer's schema compiler and generated runtimes into applications. Use for schemas and release migration, CMake generation, native codecs, caller-owned Java array/NIO output windows, streams/channels, fixed-scalar Java packed buffers, C++ bit fields, native dependency layouts, pinned OPC signatures, portable behavior, owner-bound defaults and bindings, callable availability, arrays/generics, borrowed/shared/weak references and typed field addresses, XML/XSD, schema-local ordered XML/package profiles and identity contracts, restricted HTML, document profiles, bounded compression, ZIP/OPC packages, rebuildable caches, or C++ managed identity, transactions, registries, indexes, history, journals, collaboration and persistence. Review the installed version's complete supported feature set and apply every capability relevant to the user's contract; preserve APIs and wire identities, and qualify unsupported combinations."
---

# Serializer Integration

[Documentation index](../../../docs/README.md)

For derived uint64 disk lookup, use the
[paged index](../../../docs/managed/paged_index.md) with a complete source binding.
Retain the exact immutable source and query pins; reject stale, foreign or corrupt
generations before returning results. Configure external-run, temporary-file,
page/cache and cursor budgets. Native database wrappers pin whole eager roots;
they supply no lazy authoritative values or automatic persistent placement.

For standalone C++ managed scalar lookup, use language 1.24
[index declarations](../../../docs/schema/indexes.md). Choose ordered/unordered
and optional unique keys at local/inherited placement. Query the generated field
descriptor through the pinned registry. Uniqueness applies before acceptance,
including loaded history, independently of query-cache materialization. Preserve
the documented key/profile bounds; database placement and composite keys reject.

For bounded eager durable C++ roots, select
[`native_database<Root>`](../../../docs/managed/native_database.md). Prepare
against the exact epoch/revision and canonical request bytes, then accept
durably. Keep external effects outside edits; uncertain writes fence mutation
until reopen. Native migration requires an identity-preserving converter, a
new epoch and an absent archive path. Preserve the source generation through
publication and coordinate archive retention explicitly. A restored archive
needs an externally established epoch and allocation watermark. Review current
eager-memory, platform and recovery limits; this API supplies no lazy authority.

For repeated C++ history, consider explicit
[`compact_history`](../../../docs/managed/compact_history.md) retention. It
shares exact immutable byte blocks while keeping existing Save formats and
logical history limits. Review measured retention, full-root CPU/export costs,
resource lifetime and the selected platform qualification before adoption.

For dependency-aware C++ projections, opt into bounded `.field_paths = true`
[publication metadata](../../../docs/managed/edit_dispatch_and_notifications.md).
Retain event namespace and immutable roots with stable IDs/paths. Preparation
limits reject before acceptance; observers run only after publication.

Use the [payload allocation primitives](../../../docs/managed/payload_allocation_runtime.md)
when manually binding owned C++ containers and exact native decoding to a shared
resource. Retain the domain for every allocation's lifetime and distinguish
requested-byte bounds from process memory. Generated record selection requires
its separately qualified profile; default generated containers remain unchanged.

Check the [managed representation matrix](../../../docs/managed/representation_contracts.md)
before selecting C++ storage/codecs. Use unpacked owning records and distinct
public base occurrences. Direct native identity cannot map into Protobuf's field
domain; explicitly separated values preserve the existing wrapper mapping.
Packed/views, recursive owning storage and ambiguous inheritance retain explicit
rejections. Consult the dated compiler/runtime evidence for the selected profile.

For native immutable generation publication, use
[`replace_file_generation`](../../../docs/serialization/file_generations.md)
after syncing the replacement, then sync affected parents. Retained readers keep
their original native file object. Unsupported platform/volume capabilities fail;
the host still supplies the durable acceptance/recovery protocol.

For rebuildable data, review the [owning C++ cache contract](../../../docs/design/cache_attributes.md)
and [usage example](../../../docs/guides/usage.md#rebuildable-c-cache-fields).
Language 1.22.0 accepts `cache` and `cache(priority = 200)`; omitted priority is `0`
(lowest), and `255` remains disposable. Use generated `cache::fields<Owner>`
descriptors, explicit recipe/dependency identity and an application-selected
`cache::provider`/`directory_provider`. Consult readiness separately from defaults.
Use `model_store::cache_key` and owner-thread `publish_cache` for exact-source
guarded updates. Preserve existing memory-only `transient` behavior.
New journals use `.<filename>.serializer/`; explicitly migrate older layouts with
`migrate_journal_layout()`. Invoke fallible `close_journal()` after stopping work
and coordinating required synchronization; destruction does not save. Keep caches,
required external-document `state/` metadata and migration guards. Never manually
delete required companions or treat the shared directory as `.tmp`. Non-C++ cache
declarations diagnose; arbitrary inline/appended formats need a host provider.

Use the [central pending backlog](../../../docs/pending.md) for every documented
remaining requirement and proposal. Design guides retain contracts and rationale;
completed capability IDs and original evidence live in the
[implemented feature catalog](../../../docs/project/completed_requirements.md).
Check exact backend/profile scope and dated evidence before claiming completion;
completed slices do not close broader pending parents.
The [XML/package resolution ledger](../../../docs/project/xml_package_backlog_resolution.md)
records the explicit closure of thirteen XML/package parents, including OPC
reconciliation and excluded extensions. Use its selected OPC fifth-edition
profile; the full Core 1.4 combined graph is C++/Python only. Other outputs retain
capability rejection for that graph. Dependencies outside that section keep their
existing status; reconciliation is not new implementation evidence.
Apply the [application ownership contract](../../../docs/project/application_responsibilities.md):
keep authentication/principal binding, policy storage/administration, business
side effects, custom historical layout interpretation and authorized publication
with the host or maintainer. Those responsibilities are explicitly will not fix
in the library. Missing compiler/runtime mechanisms remain implementation work;
do not reclassify an unsupported Serializer feature as application responsibility.

Serializer — **State Framework** combines multi-language generated codecs and
schema evolution with C++ application-state management. Choose ordinary generated
models for data exchange or the managed runtime for editable C++ models. Native
managed engines outside C++ remain unimplemented; exchanging managed wire records
does not provide those engines. See the
[positioning and scope reference](../../../docs/project/product_positioning.md).

Implement the user's requested Serializer integration using the version present
in their project. Preserve established wire IDs, names, protocol choices, and
application ownership requirements.

Always try to express the complete behavior in language-independent Serializer
features so the schema can generate code for Java and other future consumers,
even when the initial application selects only C++. Prefer typed functions with
`expression(...)` or `predicate(...)`, portable constructor/factory bindings and
defaults, and portable generic constraints whenever they support the required
contract. When migrating a method such as `complete()`, move its supported logic
into the schema as well: `readonly predicate(completed == expected)` generates
the Boolean check in every selected backend with the documented predicate failure
behavior. Generate each language's implementation from that shared schema.

Use target-specific native blocks or external per-backend implementations only
when supported portable features cannot express the required contract. Explain
the exact limitation and keep the native adaptation scoped to it. Authored C++
bodies remain C++ implementations and require a separate implementation for other
selected targets. A qualifier such as `noexcept(cpp)` is target-specific API
metadata, not a portable nonthrowing guarantee; other outputs ignore it and retain
their own error behavior. Prefer inferred guarantees for portable schema bodies.

This is the canonical integration entry point linked from the repository's README
and AGENTS.md. Apply it when loaded as a skill or read directly from a supplied
checkout; direct use does not require copying the skill into the consuming project.

## Use the full framework

Compiler/runtime **2.34.0** also extends the closed HTML fragment adapter to
thirty tags, including explicitly balanced `sub` and `sup` phrasing containers.
Use only their ordinary global attributes and the existing byte/event/depth/text
limits. Generate the opted-in native SDK or use C++ `<rohit/html.hpp>`; the HTML
adapter is separate from XML/XHTML and adds no schema-language syntax. Arbitrary
HTML, browser recovery and unrestricted mixed-content XHTML remain outside this
contract. Read the [complete tag and adapter contract](../../../docs/serialization/xml_advanced_contracts.md#html-is-a-separate-fragment-adapter).

Python advanced ordered/token graphs in this release can retain bounded lexical
namespace scopes for record attributes, including QName/list/union values without
scalar defaults/fixed values or owner extension storage. Pure namespace mappings
keep their established tree adapter. Scalar child/text scopes and broader default/
extension combinations retain their capability gates. Consult the
[executed scope and performance measurements](../../../docs/verification/verification-xml-authoring-2026-10-24.md)
before selecting a combined graph or claiming lower CPU/process-memory costs.
The full Python Core graph retains its safe owning-tree adapter. Chunk input
requires token-compatible storage; do not force tree fallback for mixed, presence,
map/variant, inheritance or lifecycle graphs whose storage requires token codecs.

For compiler/runtime **2.34.0** and schema language **1.24.0**, select ordered XML
beside the owner: `xml(ordered("items"), ordered_child("branch", "{uri}local"));`.
Keep one owning variant array; the pinned XSD supplies repeated/grouped occurrence
grammar. Matching INI selections remain valid; conflicting fields/child names
reject. Import now supports named/inline/union atomic lists and selected ordered
complex groups without duplicating payload arrays. Review imports in fresh files;
flat existing storage and old headers retain their meanings. Scalar/nil/wildcard/
all/mixed-text ordered imports and recursive owning models remain gated. See the
[ordered contract](../../../docs/serialization/ordered_xml.md) and
[import guide](../../../docs/serialization/xsd_tooling.md#import-declarations-from-xsd).

Declare package `target_mode(internal|external|both)`, `requires_type(true|false)`
and `vocabularies("uri", ...)` directly in a language-1.24 part, or retain explicit
INI overrides. All eleven owning outputs use the same compiled contract and
existing runtime codec/compression selectors. External targets remain inert byte
references and never trigger fetching. Invalid final codec/target/source profiles
reject even without INI configuration. Read the
[generated pipeline contract](../../../docs/serialization/xml_packages.md#generated-package-pipelines).
Use the resolution ledger above for closed XML/package parents and explicitly
deferred dependencies; retain the exact backend/profile qualification limits.

For compiler/runtime 2.34.0, follow the
[compiler/backend contracts](../../../docs/guides/compiler_backend_updates.md).
Language 1.24 scalar defaults require portable literals; retain older headers for
native C++ expressions. Use `bits(width)` only for supported C++ owning native
fields: it narrows stored values without changing scalar wire width. Bind independent
generic calls from typed arguments; supply explicit native input types for fully
omitted constructor/factory defaults. Keep other-backend generic API rejections.
Opt native dependency bundles in with `--include-mode separate`, publish every
`generate_files` artifact, keep Python's sibling package, and compile all Swift/
Kotlin companions together. Preserve compiler source/output navigation comments.
Java `stream_output` borrows a synchronous sink through two stable-model traversals;
allow partial prefixes on I/O failure and require channel progress. Packed buffers
specify dense positional bytes, not JVM object memory. For OPC signatures,
require `signatures::available()`, pin the trusted DER certificate and explicitly
require payload/relationship coverage. Remove signatures before editing and
verify newly signed bytes; opaque preservation never supplies authentication.
Consult the [dated qualification](../../../docs/verification/verification-compiler-backends-2026-10-24.md)
before claiming a provider/platform or performance result.

For fixed-XSD extension content in 2.28.0, use the
[generated wildcard contract](../../../docs/serialization/xml_tokens.md#xsd-wildcards-and-positioned-extensions-in-2280).
All eleven owning outputs retain namespace predicates, element/attribute kind,
particle position and exact occurrence bounds. `strict` requires a matching
global declaration from the pinned local bundle; `lax` validates declared names;
`skip` bypasses declaration validation. Private generated validators enforce
supported scalar/particle/attribute contracts, QName scopes, nil rules and finite
permitted named type derivations, without runtime schema acquisition. Keep
retention separate: skipped declared payloads still validate, and edited retained
payloads validate again on output. Share the original operation and cancellation
through private replay; temporary validation work/storage does not consume another
preserved-byte quota. Test skip with zero preserved-byte admission.
Global unknown fallbacks remain foreign-only and close core/absent namespaces,
known names and explicit reject URI families. Authored slots keep their own
foreign-only URI permissions. Scalar-text and singleton `all`/choice owners accept
attributes/end-child slots; do not infer unsupported interior heterogeneous
storage. Preserve original opaque namespace scopes and native companion occurrence
associations. Schema validity and preservation never establish application
understanding of a required vocabulary.

For legacy C++ repeated choices with one known collection and wildcard branches,
use the whole-tree XML facade. Shared declaration/capability/slot checks and
explicit `extension_bounds.document` limits apply; direct token entry points
remain unavailable for that storage representation. Use a supported explicit
ordered mapping when direct token traversal is required. Follow the
[legacy tree checks](../../../docs/verification/verification-legacy-cpp-wildcards-2026-10-22.md).

The current compiler/runtime release is **2.34.0**, with schema language
**1.24.0**. Release 2.31.0 requires a C++23 compiler and matching standard library
with `std::byteswap` and `std::span`. Source builds reject an explicit standard
below 23; public CMake targets propagate `cxx_std_23` to generated and managed
consumers. Direct compiler invocations must select C++23 or later. Regenerate C++ headers
for the `if consteval` checked-arithmetic helper; retain runtime nonfinite-value errors. Both editor
packages are 2.0.20. Existing valid schemas retain their headers and payload revisions;
use language 1.23 for new named managed self bindings. See the
[named binding contract](../../../docs/schema/named_managed_references.md) and
[runnable example](../../../example/managed/named_references/README.md).
Patch 2.32.1 preserves embedded Java source-token bounds on older MSVC hosts;
emitted Java remains byte-identical and existing generated files need no migration.
See the [build qualification](../../../docs/verification/verification-java-runtime-literals-2026-10-24.md).
Patch 2.28.1 restores strict GNU compilation of XML metadata,
declaration prefixes and large embedded runtime literals without changing
generated contracts or application integration.
Release 2.29.0 adds opt-in external-document journal retirement. Use
`journal_attached()` for read-only attachment queries and
`retains_collaboration_state()` before host recovery cleanup. Set
`journal_options::allow_document_retirement` only when the saved external format
independently preserves required native IDs and allocation metadata; ordinary
generated public JSON with normalized IDs does not establish this promise.
Call `model_store::retire_journal()` rather than its detail backend. It refuses
dirty/shared/pending/uncertain or unsaved-allocation state and never synchronizes.
Default success is terminal. With `keep_in_memory=true`, preserve session history
in RAM and explicitly acquire/revalidate a fresh journal before any later durable
edit or reservation. See [retirement](../../../docs/guides/usage.md#explicit-retirement-of-self-contained-documents).
Keep Linux, hostile path-swap, power-loss and cross-process race qualification
separate from the selected Windows controls.
Patch 2.28.2 prepares extended-length Windows file/journal I/O through transient
native path arguments, including drive/UNC paths, stable locks, companion metadata
and atomic publication. Keep logical path identity and opaque origin metadata;
do not prefix authored/source-origin bytes or require a machine registry change.
Ordinary short absolute arguments remain borrowed and stream read/write paths are
unchanged. Six selected Windows x64 MSVC 19.51 Release binary/JSON checks passed,
including exact 249/259/260/271/320-character Unicode I/O, intermediate literal
component identity, lock/publication failure controls and managed recovery/history.
The moved owning build used short per-project MSBuild tracking paths; generic
native build paths remain a separate concern. XML-ON, live UNC shares, power-loss
and consumer adoption are not qualified by those checks. A lexical UNC conversion
is not network durability proof.
For external C++ documents, let the application choose an owned full
main for temporary sources or attach native differences to a durable original.
Use bounded `journal_options::origin` and `journal_origin()` for native opaque
provenance, `save_journal(origin)` for atomic replacement, and
`prepare_journal_save` / `prepared_journal_save` / `confirm_journal_save` for host
publication and explicit restart reconciliation. Never perform host effects in
fault hooks. See [origin and confirmed Save](../../../docs/managed/document_origin.md)
for evidence, exact retries, unresolved mutation barriers and supported scope.
Require `find_package(Serializer 2.26 CONFIG REQUIRED)` for these additions.
For new element-only ordered models in any of the eleven owning outputs, select
[`ordered=member`](../../../docs/serialization/ordered_xml.md) explicitly in the
owner's XML configuration. Use an array of owning record variants; retain native
collection edits as the source of order, without a text branch or hidden sidecar.
Do not migrate existing separate arrays implicitly. With `--xml.xsd`, map typed
children to the pinned XSD's nested/repeated sequence/choice groups; the generated
cursor enforces complete group occurrences in both directions. XSD export retains
the grammar and omits fragment helpers. Regenerate with 2.28.0
for non-C++ outputs. Fixed arrays require exact cardinality on both input and
output; formatting XML whitespace consumes no fragment or mixed-text allowance.
Keep normal scalar limits for text inside typed child records. Follow the
[portable verification record](../../../docs/verification/verification-ordered-xml-2026-10-22.md)
for actual SDK checks and remaining parent requirements.
C/Rust plain QName companions support wrapped/nil scalar and record collections,
mixed/ordered children, all-group permutations and lifecycle metadata. Use the
document-companion APIs and keep scopes paired with their original occurrences;
native model fields and wire bytes do not acquire XML metadata. See the
[namespace graph checks](../../../docs/verification/verification-native-namespace-graphs-2026-10-22.md).
Expanded QName scalar/list enumeration, first-successful scoped union selection,
outer enum/fixed identities and schema-owned defaults are checked in C/Rust
companions. Retain their schema-origin metadata with the native value; unchanged
defaults may emit as absent attributes or empty elements rather than rebinding
another field's instance namespace. The
[native constraint record](../../../docs/verification/verification-native-qname-constraints-2026-10-22.md)
separates generated decisions from independent-provider presentation boundaries.
Foreign preservation follows wrapped/nil records, lifecycle metadata and typed
children; `xsi:type` dispatch compares expanded identities and preserves unrelated
aliases. See the [foreign graph checks](../../../docs/verification/verification-native-extension-graphs-2026-10-22.md)
for tested combinations and canonical map-key replay. Positioned content follows
the current wildcard/extension contract above. Restricted atomic/union list items and scoped outer constraints are now
implemented in all eleven outputs; see the scalar SDK matrix below.
For namespace-sensitive scoped translators, use the
[borrowed graph APIs and lifetime rules](../../../docs/verification/verification-readonly-xml-graphs-2026-10-22.md).
C++ namespace maps/occurrence histories and C/Rust sidecars are borrowed for the
traversal; keep them and the source alive. Kotlin/Swift derive QName occurrence
scopes from assigned source paths. Other outputs expose view-local namespace
binding methods. All eleven outputs qualify exact/one-tight depth and byte
limits alongside large borrowed collections. Durable field addresses retain an
explicit XML-generation rejection policy; use their native formats.
Use [C++ core-property adapters](../../../docs/serialization/core_properties.md)
for application-supplied OPC metadata, following the selected fifth-edition
property/date/keyword contract. Use the [managed authorization boundary](../../../docs/managed/authorization.md)
for complete-candidate publication checks, binding the principal and policy
administration in the host. Use `model_store::configure_capabilities` to activate
version-one native profiles, enforce exclusions and require authorization without
an unprotected transition. Use `migration::convert_history_policy` with explicit
branch/history-loss decisions when changing native history profiles; see the
[executed combinations and conversions](../../../docs/verification/verification-managed-capabilities-2026-10-21.md).
Schema capability selectors remain proposed syntax. The bounded PMR resource caps routed requested bytes; generated standard
payload containers and decoding allocations retain their existing allocator behavior.

For retained C++ tree revisions, use `merge_base`, `merge_revision` and
`revision_parents` under the [native merge contract](../../../docs/managed/branch_merging.md).
Resolve conflicts explicitly and retain complete-candidate validation and current
authorization. Undo follows the target parent; both parents participate in ancestry.
For retained authority branches, opt in through `collaboration_options::allow_branch_merges`
on a tree authority. Check `branch_merges_enabled()`, capture `current_revision()`
with the replica's accepted cursor, and author `replica.merge_request` with source
and target revisions. Submit through the host-authenticated `submit_change` path;
apply its ordinary accepted snapshot/field deltas. Retain the request for exact
retries. A changed target or rejection requires reconciliation and a new operation
ID. Only the host installs `set_branch_merge_resolver`; reentry and callback replay
during recovery remain prohibited. General history overlays, client-authored branch
imports and standalone conversion of the merge wrapper remain outside this contract.
For selective collaboration Undo/Redo, unchanged managed variant tag/identity
contributions now merge their child fields independently; tag/identity replacement
remains atomic. Complete publication, reference and contribution-version checks
remain mandatory; the [Windows/Linux merge qualification](../../../docs/verification/verification-collaboration-merging-2026-10-21.md)
records the actual native storage/ID profiles and fault boundaries. The
[managed variant qualification](../../../docs/verification/verification-managed-variants-2026-10-24.md)
adds child-field conflict/retry, atomic same-tag identity replacement, full/delta
journal recovery and later-author Undo protection across all four Windows/Linux profiles.
This does not enable virtual edit dispatch or new portable ownership/error contracts.
New non-C++ managed engines, paged/lazy storage and native journal/storage
migration remain deferred in linked issues. Native storage/index support is
[DB02 / #6](https://github.com/rohit-singh-gautam/Serializer/issues/6), not a completed
database/client feature. Third-party database engines and sink adapters are out
of scope; use the native integration contract without adding those dependencies.
For one-expression C++ serialization, call `serialize_as(value)` for an owning
compact-JSON `std::string`, or `serialize_as<Result, Protocol>(value)` for another
supported owning result/native protocol. Borrowed views require explicit storage:
`serialize_as<std::string_view>(value, buffer)` borrows a caller-owned `string_stream`.
Keep that buffer alive and unchanged; unbuffered view results are unavailable.
Owning and borrowed customization traits have separate lifetime contracts.
Use the [string-buffer guide](../../../docs/guides/usage.md#string-backed-output-buffers)
for direct output, embedded-NUL lengths, formatting and buffer reuse. The helper
transfers default string storage; other owning results copy completed bytes.
It adds no wire fields or generated API changes and has no `serialize_string` alias.
For generated C++ Protobuf fixed arrays, use the existing repeated-field mapping
and retain exact cardinality in every complete decoded owner message. Missing,
null, short or excess arrays reject; fully format-omitted fields keep their schema
defaults. See [the mapping boundary](../../../docs/serialization/protobuf.md#mapping-contract)
before combining fragmented embedded messages or schema default initialization.

Regenerate all eleven XML outputs for complete [scalar/list/union constraints](../../../docs/verification/verification-scalar-identities-2026-10-22.md).
First-selected primitive families control outer enumeration/default/fixed values;
restricted atomic/union list items retain complete facets and recursive ordered
identities. QName/list literals retain schema-owned expanded names when instance
prefixes shadow them. Preserve pattern-sensitive lexical aliases and prefix-list
string enumeration spellings. Keep occurrence scope metadata with edited or
reordered values and native C/Rust document companions. Native fields, wire
identities and payload versions remain compatible. `prefix_list` default/fixed
semantics and XSD export need an external capability contract; ordinary XSD string
defaults do not infer namespace value semantics.
Supported private-field projections isolate no-namespace schema defaults from
an active instance default while retaining other fields' expanded identities.
Check the linked graph/backend limits before choosing a combination.
Native IDs/wire formats and schema language are unchanged.

Use the [readonly XML graph qualification](../../../docs/verification/verification-readonly-xml-graphs-2026-10-22.md)
for namespace-sensitive scoped translators, borrowed native getters, exact/one-tight
depth and byte bounds, failed publication and recovery in all eleven outputs.
C++ borrows caller namespace metadata; mobile outputs retain assigned source
occurrence scopes; C/Rust supply encode-only borrowed companions. Keep every source
and borrowed metadata owner alive and unchanged through synchronous output.
The [25,000-value stress controls](../../../docs/verification/verification-readonly-xml-stress-2026-10-21.md)
also check native storage identity. Durable-address XML has a separate explicit
rejection policy. These results do not imply allocation-free output.

Regenerate affected XML codecs for the
[eleven-output namespace/provider boundary fixes](../../../docs/verification/verification-runtime-boundaries-2026-10-21.md).
Select the adapter's documented structural/text policy; physical declarations
consume that policy while inherited scope copies retain work/allocation costs.
Pass the same borrowed context through custom codecs and prepared validators.
Generated completion checks observe cancellation before publishing results;
opaque provider heap ceilings and hard preemption require external process limits.
For C#/Kotlin/Swift standalone XML operations, use `Limits.WithCancellation` or
`XmlLimits.withCancellation`. These return policy copies, retain caller callback
state and compose the existing ledger without resetting it. Throw the caller's
own cancellation/deadline error; nested work preserves its original identity.
Create a fresh policy wrapper for each top-level operation and after failure;
do not share one wrapped policy concurrently. Swift's synchronous FoundationXML
failure path retains the first error and rejects after the already byte-admitted
provider call returns, avoiding unsafe delegate reentry. This is cooperative
failure handling, not hard provider preemption.

Use the [C++ Pack-IRI API](../../../docs/serialization/pack_iri.md) for an outer
package locator. Supply the outer scheme's identity/equivalence policy in the host;
keep internal relationship resolution separate. Address parsing never opens a
file or network resource. Selected optional non-ASCII part naming, interleaving
and growth-hint production remain explicitly unsupported.

Package Save builds a complete replacement archive. Follow the
[ZIP physical metadata contract](../../../docs/serialization/zip_metadata.md)
when physical attributes matter: logical opaque-part preservation does not imply
unchanged headers, compressed bytes or signatures. C++ callers may explicitly
replay inert metadata through `zip_reader::entry_metadata`, `archive_metadata`
and writer metadata options, using the same operation budget. Regenerate size,
offset and CRC fields; never apply retained attributes as filesystem permissions.
Use the [mandatory OPC metadata qualification](../../../docs/verification/verification-opc-metadata-2026-10-21.md)
for the eleven-output URI/IRI, infrastructure encoding, Relationships compatibility
and Annex B contract. It does not establish unsupported rich model graphs,
signature trust or optional Unicode part naming.

For published Core 1.4, use the pinned local Core/XML XSD bundle and the admitted
C++/Python owning graph. The [official corpus qualification](../../../docs/verification/verification-core-conformance-2026-10-21.md)
accounts for 175 models, including arbitrary model filename extensions and
explicit resource/schema rejections. Discover parts from OPC content types and
relationships. Other nine backend selectors reject this rich graph; ordinary
XML support does not imply rich Core support. Validate OPC package rules and
application geometry separately from XSD validity; preserve the recorded
upstream positive-label/schema disagreements rather than weakening the schema.

Regenerate Java/C#/Kotlin/Swift XML codecs to apply corrected
namespace-declaration admissions, including qualified attributes, default resets
and type declarations. Tight output bounds can reject earlier when generated
declarations exhaust the policy. C# readers count declaration prefix/URI bytes
consistently with the direct writer. Native models, APIs and wire identities retain
their contracts. Use the [namespace admission test](../../../test/xml_namespace_budget_test.py)
for the selected SDK boundary inventory; broader XML resource/provider qualification
remains tracked under `XML-B13`.
Use the [eleven-output native baseline](../../../docs/verification/verification-native-codec-baseline-2026-10-21.md)
when assessing ordinary positional-binary costs. Replay
`qualification/native_codec_baseline.py` with actual SDKs and pinned local providers;
retain XML-disabled controls and exact wire hashes. Process residency and Python
traced allocation have different scopes; the checkpoint supplies no hard memory
cap or earlier-release performance guarantee.

Preserve the supported native managed identity, transaction,
preview, journal and collaboration contracts when composing those features.
For local collaboration, attach a fresh placeholder store before editing or
creating its client journal. Initial join validates typed references against the
staged authority namespace and retains the original client policy. A rejected
join must not publish the incoming root or adopt its document namespace; correct
the rejected input or policy before retrying. Rebuild C++ consumers for the
2.17.1 initial-join fix; schema language and saved formats are unchanged.
Use the [native reference qualification record](../../../docs/verification/verification-managed-reference-faults-2026-10-21.md)
for actual ID/storage profiles and failure/recovery boundaries. Root/control-block
allocation hooks and resident admission estimates do not replace allocators in
generated standard-library payload containers.

When requiring reproducible package bytes, retain the backend's documented writer
policy, exact authored payloads/names/IDs and compression options. Release 2.19.1
provides an [independent eleven-output inventory](../../../qualification/package_reproducibility/README.md)
for input permutations, fresh processes and ZIP64 metadata. Stored ZIP equality
and provider-independent OPC/payload projections are distinct from compressed
byte equality, which requires matching provider fingerprints or a pinned
self-contained encoder. `opc-stored` controls payload storage; infrastructure may
still use DEFLATE. Python's plain ZIP probe is a diagnostic of its generated
private helper, not a new public generic ZIP writer. Public schema-bound OPC
Save retains its existing contract. No model/API/schema migration is required.

For fixed singleton package canonicalization in 2.19.0, keep the loaded
extensions with the edited aggregate. Generated facades retain moved-role origin
provenance and resolve preserved internal links against the original source before
rebinding both endpoints. C/Rust use additive canonical writer APIs; Go selects
additive canonical loading. Existing defaults remain valid. Do not reconstruct
extensions from their public arrays when private moved-role provenance is needed.
Removed moved origins, occupied destinations and source-local ID collisions must
reject before publication, even when a path swap gives another role the old name.
For dynamic/repeated moves in 2.28, supply the generated explicit rename plan so
loaded occurrence provenance and preserved links rebind together. Preserve opaque
bytes, external URI references and raw query/fragment suffixes; this contract does
not imply lexical XML or signature preservation. Follow the
[package preservation contract](../../../docs/serialization/xml_packages.md#fixed-singleton-path-canonicalization)
and [actual SDK verification](../../../docs/verification/verification-package-relocation-2026-10-21.md).

For C/Rust XML composition in 2.18.0, use the additive operation descriptor/trait
APIs with the same caller-owned ledger. Package codecs automatically share native
XML and XSD identity tuple admissions; equal keyrefs spend references before
deduplication. Use the selected-codec package entry points for custom native
composition, retaining the supplied context and synchronous callback lifetime.
Go callbacks receive already bridged public `XmlOptions`; JavaScript/TypeScript
role codecs receive bridged `XmlLimits` and immutable location metadata. Kotlin
consumers in another module can call the facade's public `xmlLimits(context)`
inside public Composer/Loader callbacks. Follow the
[C](../../../docs/languages/c_packages.md),
[Rust](../../../docs/languages/rust_packages.md),
[Go](../../../docs/languages/go_packages.md),
[JS/TS](../../../docs/languages/javascript_packages.md) and
[Kotlin](../../../docs/languages/csharp_kotlin_packages.md) usage contracts.
Regenerate affected outputs. Preserve existing native model/limit APIs and wire
identities. Logical admissions cover instrumented generated operations and known
default construction; provider and opaque authored factory internals do not gain
hard peak-allocation or preemption guarantees.
Regenerate Python package facades for the Save callback completion checks.
Before publication, cancellation preserves the previous file. After replacement,
handle `publication_uncertain` and inspect/reopen the complete new generation;
do not retry under the assumption that the previous generation is still present.
Use the [native operation verification record](../../../docs/verification/verification-native-xml-operations-2026-10-21.md)
for actual SDK, field-origin, first-failure and publication checks.
The [large owning package qualification](../../../docs/verification/verification-large-package-operations-2026-10-21.md)
covers the selected eleven-output XML/DEFLATE/identity/preservation/Save chain.
Keep each backend's public operation boundary and explicit finite profile;
cross-call budget parameters and opaque provider preemption differ by backend.

For current generated package pipelines, preserve each role's static model,
XML root/type, MIME and container identity. Select only declared default-policy
`xml`, `json` or `binary_none` codecs and `stored`/`deflate` compression; fixed
roles reject overrides. An XML caller codec remains XML-only and rejects a native
codec envelope. Supply explicit rename provenance for moved dynamic/repeated
parts, preserving original physical occurrence associations. For C/Rust keep the
aggregate's XML companion store with its native values; selected callbacks use
the supplied operation/context and document adapters. Never fetch inert External
byte relationships. See the [pipeline usage and API details](../../../docs/serialization/xml_packages.md#generated-package-pipelines)
and [actual eleven-output checks](../../../docs/verification/verification-package-pipelines-2026-10-22.md).

Compiler/runtime **2.17.0** adds language **1.21.0** package `policy(fixed|defaults)`,
`key("scope", field)` and `reference("scope", field, target_role)` declarations.
Use generated checks for direct public persistent string/integer selectors before
adding domain callbacks. Forward references are supported; duplicate, missing and
wrong-model targets fail before aggregate/file publication. Selectors do not add
native fields or change wire IDs. Runtime overrides select stored/deflate ZIP
storage and cannot replace model/XML identities. Regenerate affected outputs and
rebuild C++ consumers against matching headers/runtime. Follow the
[package selector contract and boundaries](../../../docs/serialization/package_application_contracts.md)
and each backend's package guide; preserve explicit unsupported-graph gates.
Generated [C](../../../docs/languages/c_packages.md),
[Rust](../../../docs/languages/rust_packages.md) and
[Swift](../../../docs/languages/swift_packages.md) outputs also provide owning
ZIP/OPC facades with bounded typed loading, preservation and staged Save. C/Rust
package graphs use per-part XML document companions from 2.28; retain those
companions with their original physical occurrences when editing/reordering.
Both editor packages are **2.0.10** and understand the new declarations.

Compiler/runtime **2.16.0** retains each C++/Python namespace-dependent scalar
child's XML scope through local declarations, shadowing and default namespace
resets. Optional/repeated QName, QName-list and nested union values preserve their
expanded identities. Treat XML namespace metadata as associated with its owning
field/occurrence when editing; ordinary native strings do not contain expanded
namespace identity. Native JSON/binary codecs omit that XML-only state. Go/Java/C# add
plain QName/list/prefix-list codecs and validate/copy authored bindings through
Go `SetXMLNamespaces`, Java `setXmlNamespaces` and C# `SetXmlNamespaces`;
these setters replace loaded scopes on that record and nested records bind
independently. Release 2.24 adds scalar/list enumeration/default/fixed constraints
and selected QName/list union members for these three backends. Release 2.28
completes their scoped outer union/default/fixed and restricted list-item contracts.
Valid Unicode XML mapping names and prefixes are accepted.
Kotlin/Swift provide equivalent plain scalar scope retention on owning records;
2.24 adds expanded scalar/list enumeration; 2.28 also enables namespace-dependent
default/fixed values, outer unions and restricted list-item contracts.
C/Rust keep scopes in XML document companions; use document decode and
encode-with-extensions APIs, and respect their additional graph capability gates.
Regenerate affected outputs and rebuild C++ consumers with matching headers/runtime.
Use the [current scope and checks](../../../docs/verification/verification-qname-scopes-2026-10-20.md)
and the [2.28 scalar matrix](../../../docs/verification/verification-scalar-identities-2026-10-22.md)
when selecting namespace constraints.

Release **2.15.0** extended selected C++/Python XML and OPC contracts.
Regenerate affected outputs and rebuild C++ consumers with matching headers/runtime.
C++ QName/QName-list child unions require the first successful member and expanded
names to agree in child and owning-record scopes in that release. Version 2.16
retains child-local/shadowed scopes separately. Python permits outer enumeration/fixed values on namespace-independent
unions and compares their selected primitive/list value identities. Namespace-dependent
outer union restrictions and QName-list enumeration/fixed values were gated at that
checkpoint and are implemented by the 2.28 scalar update.

Pass loaded C++/Python package extensions into generated Save. Their additive
`relocations` metadata binds discovered fixed singleton typed roles to declared
canonical paths. Preserved sources and internal targets rebind simultaneously after
resolving old relative scopes, retaining query/fragment suffixes. External URI text
and opaque bytes stay intact; conflicts, missing targets and duplicate source-local
IDs reject before replacement. Other package backends and dynamic role renames
still require explicit application rebinding. Logical budgets do not measure hard
process peaks. See the [package contract](../../../docs/serialization/xml_packages.md#caller-authored-aggregate-profiles)
and [executed scope](../../../docs/verification/verification-pending-xml-2026-10-20.md).

Compiler 2.31.2 also recognizes portable strings/decimals and backend identifiers
with constant stack space, including typed aggregate binding before backend
selection. Preserve each backend's existing literal/range and target SDK token
limits; [resource and SDK checks](../../../docs/verification/verification-portable-literals-2026-10-24.md)
qualify those boundaries separately.
Compiler 2.31.1 recognizes integer literals with a linear, constant-stack scan;
long canonical values do not require recursive regex stack space. Accepted native
expressions and source spelling remain unchanged. See the
[resource checks](../../../docs/verification/verification-integer-defaults-2026-10-24.md).
Compiler 2.32.2 also locates backend field-default failures in all ten non-C++
outputs, including pre-1.14 included declarations and concrete generic copies.
Use the original declaration location to repair the field; the existing reason
and exception category remain. C++ native acceptance is unchanged, and public
ASTs without coordinates retain their earlier error text. See the
[diagnostic checks](../../../docs/verification/verification-backend-default-locations-2026-10-24.md).
Explicit language 1.14+ canonical decimal/hex integer literal defaults receive
member-located overflow diagnostics, including original included/generic source.
Native expressions, suffixed/octal literals, numeric Boolean defaults and legacy
headers retain their behavior. Aggregate/generic default failures also name their
original declaration. Version 2.16 additionally checks canonical unsuffixed
decimal float/double overflow and unknown enum defaults explicitly qualified by
their declared enum type. Underflow and opaque native spellings retain their
existing handling; see [owning defaults](../../../docs/schema/owning_api.md).

Generated C++/Python package codecs share OPC-edge and supported XSD
key/unique/keyref tuple admission on the package reference budget. Failed/repeated
work is cumulative. Use `budget.xml_context(part, relationship_id)` when adapting
caller-owned XML codecs; its original one-argument overload remains available.
Locations identify instrumented native validation/reference admission;
deferred presentation/provider work still has documented field attribution and
error-classification gaps. Keep diagnostic callback
nodes as synchronous borrows. See the
[batch scope](../../../docs/verification/verification-pending-batch-2026-10-20.md).
Schema language **1.20.0** and native wire identities stay
unchanged; the wider backlog remains pending.

Release **2.14.0** added compiler-only installed CMake discovery. For
generation without C++ runtime linkage, require
`find_package(Serializer 2.14 CONFIG REQUIRED COMPONENTS compiler)` and use
`serializer_generate_source` or `serializer_generate_java`. It imports only the
`Serializer::serializer` executable and loads the generation helpers, without
discovering runtime-provider packages. Use normal discovery or `COMPONENTS runtime`
before calling the C++ `serializer_generate`/`serializer_generate_variants`
helpers, which link `Serializer::runtime`. The source helper can also generate
C++ when only source output is needed. Keep the compiler's own deployed shared
libraries available and select a host-runnable package when cross-compiling;
see [compiler-only CMake integration](../../../docs/guides/cmake_integration.md#compiler-only-source-generation).
Unchanged prepared generated files and depfiles retain their timestamps, allowing
Ninja restat to prune dependent compilation. Alias/source preflight and formatting
still run; changed files retain the existing publication failure behavior.
Schema language **1.20.0** and wire formats are unchanged. Both editor source
packages use **2.0.9**, with generated package-facade navigation and bounded
indexing of marked runtime support; older outputs retain fallback indexing.

Regenerate Python modules with 2.14 for the additive
`module.__serializer_compression__` API. `Compression` is a convenience alias when
no generated model has that name; preserve existing `Compression` model types.
Use `encode(value, protocol, format, limits=EncodeLimits(...))` only with a model
from that same module, or `compress(value.encode(protocol), format)` for already
encoded bytes. `decode(Model, source, protocol, DecodeOptions(format), limits)`
returns a fresh model after exact frame and inner-message validation. Existing
model `encode`/`decode` calls stay uncompressed. Require explicit framing and keep
serialized/expanded byte, history/block, and native object limits separate.
Python gzip/zlib/raw DEFLATE use optional stdlib `zlib`; Zstandard uses optional
`compression.zstd` on Python 3.14+, and LZ4 uses separately installed `lz4.frame`.
Check `available(format)` before selecting a provider. Source/sink wrappers borrow
their streams, need explicit `finish()`, and fence after failures; output may retain
an external prefix. Keep models, mutable source buffers and borrowed streams
unchanged through a call, and finish nested outputs from the codec outward.
Cancellation remains active while consuming staged input and after the final
sink write. It is cooperative between bounded calls, not provider preemption.
See [Python compression contracts](../../../docs/serialization/compression.md#generated-python-api).

Regenerate Java output with 2.14 for its nested `SerializerCompression` helper
(the generated suffix avoids collisions with existing model names). It supports
identity, gzip, zlib and raw DEFLATE with bounded whole-object callback and
borrowed-stream APIs. Optional runtime dependencies add Zstandard
(`com.github.luben:zstd-jni:1.5.7-16`) and LZ4 frames
(`at.yawk.lz4:lz4-java:1.12.0`); generation and compilation need neither jar.
Check availability before choosing a provider and preserve explicit frame,
checksum and window policies. Missing providers fail before callbacks or I/O.
Use the generated native codec inside the callback and retain exact finish and
fresh-value publication; cancellation is checked again after the decoder returns.
Check the [Java compression contract](../../../docs/languages/java.md#whole-message-compression)
for checked failures, source/sink ownership and resource policies.

Regenerate Go output with 2.14 for the `SerializerCompression` helper family
(a collision selects a numeric suffix). Use its default encode/decode options
and explicit outer format; typed decoding accepts the existing generated decoder
and returns its exact fresh result type. Identity/gzip/zlib/raw DEFLATE use the
Go standard library on Go 1.22+. Optional Zstandard/LZ4 adapters are caller-owned
per-operation `Provider` values; the maintained adapter pins klauspost/compress
1.20.3 and pierrec/lz4/v4 4.1.33 and requires Go 1.25+. Keep optional imports out
of the generated core and do not install a global provider registry. Borrowed
wrappers require
explicit finish, retain the first failure and never close their streams. Keep
native decode limits separate from compressed/expanded/window bounds and retain
cooperative cancellation through the final source/sink/native callback. See the
[Go API](../../../docs/languages/go_compression.md) and
[executed SDK checks](../../../docs/verification/verification-go-compression-2026-10-20.md).

Regenerate Go for its separate collision-selected `SerializerPackage` family
when using physical ZIP. Use owned byte snapshots or an explicit stable borrowed
`SerializerPackageSource{Reader, SizeBytes}`; sequential input requires explicit
private spooling. Share one budget across operations, finish explicitly and keep
OPC validation separate. Go 1.22 standard APIs support ZIP32/ZIP64, descriptor
forms and staged Save; filesystem replacement/directory-sync guarantees are
platform-specific. Schema packages additionally emit typed owning XML OPC
facades with transactional loading, explicit extension policies and canonical
infrastructure. Loading routes by source/type/media identity; required unknown
capabilities override optional preservation, and edited retained links must still
resolve. Supply complete-aggregate application validation. Native XML retains its
independently narrowed document limits; automatic cumulative allocation/reference
accounting, runtime codec dispatch and declarative application references are
implemented by the current generated profile adapters. See [Go ZIP/OPC usage](../../../docs/languages/go_packages.md) and the
[actual Windows/Linux OPC proof](../../../docs/verification/verification-go-opc-packages-2026-10-20.md).

Regenerate C# and Kotlin/JVM with 2.16 for collision-selected physical ZIP helpers
and typed schema OPC facades, using .NET 8 or JVM 17 standard providers. Ordinary
XML-disabled models retain provider independence. Share one snapshotted package
`Budget` through nested XML/identity work, domain validation and staged Save;
logical admission counters are not exact managed-heap measurements. Borrow stable
random sources or explicitly admit bounded private spooling for sequential input.
For Kotlin on Windows, supply access-restricted spool/Save directories; its files
inherit directory ACLs. POSIX providers receive owner-only file permissions.
Keep loaded extensions when editing, validate complete unpublished aggregates,
and respect platform array limits and host-specific replacement durability.
Runtime profile dispatch and declarative application reference checks remain
separate pending contracts. See the
[C#/Kotlin package API and examples](../../../docs/languages/csharp_kotlin_packages.md)
and [actual acceptance record](../../../docs/verification/verification-pending-50-2026-10-20.md).

Regenerate C# output with 2.14 for its nested `SerializerCompression` helper
(with a generated suffix on collision). The .NET 8 profile supports
identity/gzip/zlib/raw DEFLATE with actual `CompressionLevel` enum options;
optional Zstandard/LZ4 use caller-selected `Provider.ZstdSharp(assembly)` or
`Provider.Lz4(assembly)`, pinned to ZstdSharp.Port 0.8.8 and K4os.Compression.LZ4
1.3.8 in the qualified profile. Select `EncodeOptions.WithProvider` and
`DecodeOptions.WithProvider`; keep the original constructors and BCL enum levels
for core formats. Missing/mismatched selections fail before callbacks or I/O.
Supply exact native encode/decode
callbacks and choose both outer byte/window caps and native `Limits`. Borrowed
`InputStream`/`OutputStream` wrappers need explicit `Finish`; disposal retains
stream ownership and emits no trailer. Keep cooperative cancellation active
through final callbacks and publish only the returned fresh decoded value.
See the [C# API](../../../docs/languages/csharp_compression.md) and
[executed SDK checks](../../../docs/verification/verification-csharp-compression-2026-10-20.md).

Regenerate JavaScript and its TypeScript declarations together for the asynchronous
collision-selected `SerializerCompression` container. Load `await nodeProvider()`
explicitly and supply its result per operation for gzip/zlib/raw DEFLATE. Ordinary
browser imports, native codecs and identity need no Node modules. Select
`await zstdProvider()` for public Node Zstandard transforms, or
`lz4Provider(blockModule)` for caller-supplied synchronous lz4js 0.2.0 exports.
Pass the handle per operation; no global registration or implicit module loading
occurs. Zstandard defaults to 3; LZ4 fast mode accepts only level 0. Existing
TypeScript provider/option defaults retain their literal types; new option
constructors infer optional provider types. See the
[provider proof](../../../docs/verification/verification-javascript-compression-providers-2026-10-20.md).
Await every
wrapper operation sequentially; adapt actual source EOF and completed sink writes
to the documented borrowed transport contract. See the
[JS/TS API](../../../docs/languages/javascript_compression.md).

Regenerate Kotlin for its collision-selected `SerializerCompression` object and
standard JDK identity/gzip/zlib/raw DEFLATE providers. Use exact native codec
callbacks, independent native/outer policies and explicit borrowed-stream finish.
The selected actual SDK profile targets JVM 17. Runtime jars
`com.github.luben:zstd-jni:1.5.7-16` and `at.yawk.lz4:lz4-java:1.12.0` lazily
enable optional Zstandard/LZ4 without compile-time dependencies. Check availability;
missing jars or failed native deployment reject before callbacks or I/O. Preserve
the five-field positional options API and explicit native/outer limits. See the
[Kotlin API](../../../docs/languages/kotlin_compression.md) and
[Windows/Linux provider proof](../../../docs/verification/verification-kotlin-compression-providers-2026-10-20.md).

For selected Python owning XML CPU, traced allocation and source/encoded-size
measurements, use the [maintained benchmark](../../../qualification/python_xml_benchmark.py)
and [executed record](../../../docs/verification/verification-python-xml-performance-2026-10-20.md).
Compare the same values with XML disabled and all native protocols. Traced Python
allocation excludes native provider workspace and process RSS; keep those limits
explicit and do not infer a universal performance threshold.

For selected owning C++ XML measurements, use the
[portable benchmark](../../../qualification/cpp_xml_benchmark.py) and
[executed record](../../../docs/verification/verification-cpp-xml-performance-2026-10-20.md).
Supply the actual compiler, frozen runtime library and XML provider prefix.
Windows uses the MSVC developer environment; native Linux selects `--cxx`,
frozen `--headers` and the actual `--xml-library`. The selected host replay
produces identical generated sources and encoded bytes.
Compile XML-enabled and native-only profiles, independently check XML values,
and require identical ordinary native bytes. The OS process peak includes
setup, warmup and native libraries; its change does not isolate codec allocation.
Keep raw wall/CPU samples and generated/encoded sizes, with the actual SDK and
provider versions. Complete owning workloads establish no streaming guarantee.

For the full selected XML producer/consumer matrix, use
[xml_exchange_qualification.py](../../../qualification/xml_exchange_qualification.py)
with explicit actual SDKs, native helper binaries and locked offline caches.
The [executed record](../../../docs/verification/verification-xml-exchange-2026-10-20.md)
covers all eleven outputs under configured and fixed-XSD mapping, plus actual
XML-disabled builds/runs for the four ordinary native protocols. A missing SDK
must fail qualification instead of reducing the matrix. Preserve independent
expanded-name/value checks; keep this small-document evidence separate from
uniform malformed/resource, vocabulary, platform and performance qualification.

Regenerate Swift for its standalone collision-selected `SerializerCompression`
enum. Identity/gzip/zlib/raw DEFLATE require no added modules. Explicit caller-owned
Zstandard/LZ4 providers use separate required `provider:` option initializers;
original initializer signatures, defaults and function-value types stay compatible.
The original availability overload retains its provider-free behavior. The
[reference C adapter](../../../qualification/swift_compression_providers/README.md)
requires application-owned native linkage only when selected. Keep contexts private
to one synchronous operation; cleanup waits for an active step to return. Logical
byte/window limits exclude native workspace, which can exceed 32 KiB despite bounded
API spans. Supply exact owning native codec callbacks and separate native limits.
Borrowed closure-based streams require explicit finish; close abandons output
without closing the transport. See the
[Swift API](../../../docs/languages/swift_compression.md),
[core SDK proof](../../../docs/verification/verification-swift-compression-2026-10-20.md) and
[Windows/Linux provider proof](../../../docs/verification/verification-swift-compression-providers-2026-10-20.md).

Regenerate C11 headers for the collision-selected `serializer_compression` prefix.
Identity needs only C11; define `SERIALIZER_COMPRESSION_ZLIB=1` consistently before
including generated headers and link zlib for gzip/zlib/raw DEFLATE. Prefer the
prefixed typed model helpers to retain fresh transactional native decode and its
separate `srl_status`. Initialize buffers/wrappers with zeroes, explicitly finish
borrowed callbacks and release owned storage. Native encoding stages ordinary
bytes before their compression size check; bound models independently. See the
[C API](../../../docs/languages/c_compression.md) and
[actual Windows/Linux SDK proof](../../../docs/verification/verification-c-compression-2026-10-20.md).
For optional C Zstandard/LZ4, define `SERIALIZER_COMPRESSION_ZSTD=1` or
`SERIALIZER_COMPRESSION_LZ4=1` consistently and link the corresponding native
library. Select a sufficient `max_window_bytes` explicitly; the default stays
32 KiB. Provider-specific level/window admission occurs before callbacks or I/O.
See the [optional provider contract](../../../docs/languages/c_compression.md#optional-zstandard-and-lz4)
and [executed profiles](../../../docs/verification/verification-c-compression-providers-2026-10-20.md).

Regenerate Rust for its collision-selected `serializer_compression` module.
Identity/gzip/zlib/raw DEFLATE use only the standard library. Optional formats
use an explicitly borrowed `&dyn Provider` with `*_with_provider`,
`Output::with_provider` or `Input::with_provider`; keep the original exhaustive
option fields unchanged. Ordinary APIs retain their provider-free behavior.
The [adapter crate](../../../qualification/rust_compression_providers/README.md)
pins zstd 0.14.0 and lz4_flex 0.14.0 with independent opt-in Cargo features.
Its hygienic macros target the actual generated helper path, including suffixes.
Select Zstandard level 3 or LZ4 fast level 0 explicitly; the existing option default
remains 6. Keep providers alive and exclusively borrowed through their wrappers;
no global registry is installed. Supply exact owning native callbacks and independent native
limits. Wrappers take adapters by value: pass `&mut source`/`&mut sink` to retain
transport ownership, and finish explicitly. Their typed methods preserve error
identity; standard `Read`/`Write` trait nesting needs an application adapter.
See the [Rust API](../../../docs/languages/rust_compression.md) and
[actual Windows/Linux proof](../../../docs/verification/verification-rust-compression-2026-10-20.md).

For large ZIP inputs, configure archive/count/I/O/metadata bounds explicitly.
The C++/Python readers qualify ZIP64 offsets and classic representable boundary
values, including 65,535 entries. Python borrows checked seekable sources from
their current base; genuine nonseekable sources still spool under bounded limits.
Keep the source stable and open through decoding. Consult the
[ZIP64 proof](../../../docs/verification/verification-package-zip64-input-2026-10-20.md)
before claiming physical large-file stress, memory or other backend coverage.

For Java ZIP/OPC, regenerate with Java 17 and use the collision-selected nested
`SerializerPackage` container and generated facade `Input`/`Value` types. The
core needs only standard Java APIs. Snapshot limits through one `Budget`, use
bounded `RandomSource` or explicitly enabled private spooling, and supply complete
fresh-aggregate validation. Save stages, validates, syncs and atomically replaces
where supported. Keep preserved source/target context valid after known-role
renames; use provenance or an explicit rename plan for dynamic occurrences.
Typed roles select declared XML/JSON/BINARY_NONE pipelines; nested codec/reference
budgets and declarative application references share the package operation. See the
[Java package API](../../../docs/languages/java_packages.md) and
[actual SDK/runtime proof](../../../docs/verification/verification-java-packages-2026-10-20.md).

For JavaScript/TypeScript ZIP/OPC, regenerate matching ES modules/declarations
and use the collision-selected `SerializerPackage` support container plus each
flattened package facade's `Input`/`Value`. Await operations sequentially. Stored
ZIP needs no Node imports; select `await SerializerCompression.nodeProvider()`
for DEFLATE and `await SerializerPackage.nodeHost()` for private spooling/Save.
Keep shared archive/I/O/metadata/expanded/count/link policies explicit. Typed
roles use generated XML; validate the complete fresh aggregate before publication.
Preserved unknown parts/links are revalidated after edits. Supply the documented
pinned XML dependencies. See the [package API](../../../docs/languages/javascript_packages.md)
and [actual proof](../../../docs/verification/verification-javascript-packages-2026-10-20.md).
Use the generated bridge for the same cumulative ledger through supported XML,
reference, preservation and publication paths. Direct scalar application
key/reference checks and role-keyed caller codecs are implemented; follow the
[public composition contract](../../../docs/verification/verification-native-xml-operations-2026-10-21.md).
Broader selector/profile graphs retain their documented capability gates.

Keep cancellation active through staged input, transport callbacks and fresh
decoded publication. Callbacks must not operate on or close/dispose the same
active wrapper. Such lifecycle changes fence the active operation, including a
swallowed nested failure; later calls keep the backend's existing inactive error
contract. Do not resume borrowed I/O after failure. Consult the
[dated backend proofs](../../../docs/verification/verification-backlog-2026-10-20.md)
before claiming SDK/provider/platform coverage or hard memory/preemption bounds.

Compiler/runtime **2.13.0** adds differential C++ sidecar journals and
`<rohit/managed_journal_document.hpp>`. Require
`find_package(Serializer 2.13 CONFIG REQUIRED)` for
`make_journal_document_codec<Root>(format_id, encode, decode)` and assign the
result to `journal_options.document_codec`. Explicitly select `sidecar`; the API's
default remains `appended`. New sidecars use version-three copy/literal records
against the saved baseline, without periodic full-snapshot fallback. Version-two
recovery remains supported; its writer upgrades only on explicit full Save.
For an external main file, supply bounded owning callbacks that exactly preserve
durable values. Recovery verifies its raw-byte SHA-256 binding, companion
generation and complete first baseline-frame extent/checksum before calling the
decoder, restores persistent identities from side-channel metadata, and retains
history and pending collaboration through Save. New journals keep auxiliaries in
one `.<filename>.serializer/` directory (hidden on Windows), separating journal,
cache, state and staging. Keep required metadata/coordination with the main.
Older directory and flat layouts continue in place until explicit migration;
do not mix older writers with the new lock layout or remove live lock files.
Callbacks own format-specific
parsing, resource bounds and borrowed lifetimes; do not claim automatic XML/OPC
profile adaptation or complete standard-specific qualification. See the
[Windows/Linux document-bridge proof](../../../docs/verification/verification-managed-document-bridge-2026-10-20.md),
[working API example and file contract](../../../docs/guides/usage.md#external-documents-with-a-differential-sidecar)
and [remaining qualification](../../../docs/pending.md).
Use the [runnable external JSON example](../../../example/managed/external_document/README.md)
to verify bounded callbacks, exact native state restoration, retained redo and
burned allocation gaps. Generated public JSON may retain zero identity
placeholders; the companion owns authoritative IDs and recovery/session state.

Compiler/runtime **2.12.1** fixes C++23 XML floating-point parsing with
warnings treated as errors. Rebuild consumers against the updated runtime header;
`numeric_limits<T>::denorm_min()` preserves signed-zero underflow and overflow
rejection without deprecated traits. No schema regeneration or wire migration is
required; schema language remains **1.20.0**.

Compiler/runtime **2.12.0** adds [generated XML token codecs](../../../docs/serialization/xml_tokens.md).
Require `find_package(Serializer 2.12 CONFIG REQUIRED)` and regenerate C++ models
before using `<rohit/xml_tokens.hpp>` with `xml::serialize_tokens` or
`deserialize_tokens`. Keep input and owner storage live through synchronous use;
publish only after exact EOF. Explicit C++/Python/Kotlin presence mappings distinguish
absent, nil and empty values, including Kotlin native optional layers. Go/Java/C#
map policies bound entries and canonical keys; Java/C# also execute prepared
portable scalar/pattern programs, exact decimal/integer lexical families,
lists/unions and default/fixed adapters. Release 2.28 adds complete scoped QName
outer constraints and restricted item rules to all eleven outputs. JavaScript/TypeScript use bounded portable
pattern programs with exact numeric comparisons. Owning composed inheritance
supports every output, including private composed bases in C/Rust/Kotlin/Swift.
C++ key/unique/keyref observers validate during
token traversal with optional borrowed `operation_context.identity_bounds`.
Python revision/magic, owning digest and positioned-extension mappings retain
their documented graph restrictions. All eleven outputs' selected ordered mixed
fragment mappings preserve native private storage and enforce fragment/text budgets.
JavaScript/TypeScript
`decodeXmlChunks` requires pinned `saxes` 6.0.0 and consumes fatal-decoded UTF-8
chunks directly into private typed fields. Check the selected record graph's supported token
subset. Rust XML generation additionally requires pinned `xmlparser` 0.13.6;
retain `roxmltree` for legacy helpers and use `decode_xml_bytes` for external
UTF-8 bytes. C uses a LibXml2 pull reader and preserves an initialized destination
on failure. All eleven outputs support the checked element and `xsi:type` variant
inventories. Java/C#/Go support authoritative XSD
presence; Swift native frames support nil/optional collections and singleton
`all` groups; C and Rust also accept independently checked `all` permutations.
Rust `Option` layers and C explicit optional storage preserve absent, nil and
empty scalars, records and wrapped collections through all native protocols.
Kotlin/Swift/C/Rust also traverse authoritative simpleContent/wrappers and
singleton scalar/record choices; exactly one native alternative is selected,
or none when the XSD group is optional. Grouped/repeated heterogeneous choices
require a separately supported ordered-storage contract.
For Kotlin/Swift maps, pass the optional trailing `mapLimits` companion argument
to XML encode/decode methods. Existing `XmlLimits` constructors are unchanged;
the map policy applies independently to every wrapper and propagates to nested
records. Canonical native key identity rejects numeric and Boolean aliases.
For Kotlin/Swift ordered mixed content, pass `mixedLimits=XmlMixedLimits(...)`
(Kotlin) or `mixedLimits: XmlMixedLimits(...)` (Swift). It propagates independently
through nested owners and counts empty authored output fragments. Input joins
adjacent provider text between typed children. Use their separate `Html` adapters
for the approved ordinary HTML fragment subset; those scanners do not call XML providers.
Java/C#/Go/Kotlin/Swift also validate scoped key/unique/keyref tuples directly, including
forward references and independently bounded owner-local indexes.
C/Rust validate the same reference contract in a separate bounded streaming pass
over original input or private output bytes. Their `with_identity_limits` companion
entrypoints retain existing native/XML limit layouts and ordinary method arities.
Every output's selected scalar bindings include exact numeric/list/union facets and bounded
portable pattern programs. Owning revision/magic and digest mappings
preserve native historical layouts. Kotlin accepts `digestLimits=XmlDigestLimits(...)`
and Swift accepts `digestLimits: XmlDigestLimits(...)`
to bound each nested owning digest independently. All outputs' supported readonly value references
and scoped translators export original source values; their input graphs reject.
C/Rust/Go/Swift real prepared XSD callbacks are qualified by the Windows-only native
provider fixture; it uses a test bridge, not an added installed runtime dependency.
Check the selected graph's remaining advanced restrictions before
choosing the token path.

Python exposes `decode_xml_chunks`, mixed/map/variant/inherited traversal,
generated XSD all/key/unique/keyref validation
and XML-only `Limits(xml_identity=XmlIdentityLimits(...))` tuple bounds. Forward
references resolve before publication. Use `Html` separately for the approved
ordinary HTML fragment subset. Concrete owning generic instances and C++ private/
multiple owning inheritance use generated field bindings; preserve named-instance
type identities and check each mapping's backend limits. The [typed OPC reference example](../../../example/opc/README.md)
documents which XML and attachment part each application value fills, generates
C++/Python package metadata, and checks independent ZIP/XML readers. Consult the
[per-backend verification record](../../../docs/verification/verification-xml-tokens-2026-10-19.md)
for the remaining X01-X22 instances; this checkpoint does not close all languages.
Schema language stays **1.20.0**, both editor packages stay **2.0.5**, and native
wire values are unchanged. C++/Python maps reject duplicate canonical keys and
expose per-wrapper bounds. The qualified variant subset in every output supports
element tags and `xsi:type` with exact expanded-name inventories.
Python mixed input joins provider-split
text between children; adjacent/empty text boundaries cannot survive XML bytes.
Use `Limits(xml_map=XmlMapLimits(...), xml_mixed=XmlMixedLimits(...))` to tighten
these Python admissions. Use `Limits(xml_pattern=XmlPatternLimits(...))` for
portable Python pattern state/input/work bounds; supported XSD category/block/name
escapes and recursive subtraction run without lxml in 2.28. Java, C#,
Go and Kotlin use native pull input for supported owning graphs. Prepared Java,
C# and Kotlin XSD providers reject undeclared roots and external schema acquisition;
each validation owns its execution state. Go supports flattened owning bases and
approved portable pattern programs with bounded non-backtracking execution. Regenerate
XML outputs; no native payload migration is needed. Prefix/declaration
presentation may change, so compare expanded names.

Compiler/runtime **2.11.0** adds [advanced shared XML contracts](../../../docs/serialization/xml_advanced_contracts.md).
Require `find_package(Serializer 2.11 CONFIG REQUIRED)` for
`<rohit/xml_pattern.hpp>`, `<rohit/xml_identity.hpp>`, `<rohit/document_profile.hpp>`
or `<rohit/html.hpp>`. Prepare immutable portable regex programs once; unsupported
dialect constructs reject on the explicit portable path. Release 2.28 lowers XSD
Unicode category/block escapes, XML name classes and recursive subtraction into
portable programs in all eleven outputs; see the [independent pattern matrix](../../../docs/verification/verification-unicode-xml-patterns-2026-10-22.md).
Keep exact scalar value spaces and Unicode code-point lengths. Host lookaround,
special groups and backreferences are outside XSD 1.0 grammar. Numeric writers
validate canonical spellings before publication; native floating-width overflow
retains its explicit representability rejection.

Prefer authoritative XSD text/wrappers and supported declaration import over
repeating placement lists. Chameleon includes and finite cycles retain separate
namespace instances and cumulative local bundle bounds. Use
`--xml.export-xsd-bundle new-directory` for checked schemas and a pinned manifest;
the parent must exist and differing existing directories reject. Review imported
stable IDs/native ordering before migrating an application. Compatibility checks
remain directional and conservative.

Identity helpers implement only the documented strict XPath/scalar subset and
same-owner key/unique targets. Nested local owners, ambiguous types and wildcard
projections reject. Validate complete normalized trees and forward references
before publication. Expected model/root/type and understood vocabularies precede
decoder selection; preservation does not establish understanding. Package metadata
does not open providers or supply every backend dispatcher. Keep borrowed model
storage live through synchronous output; decode into owning models. HTML accepts
its explicit fragment subset separately from XML/XHTML. Use the shared eleven-language
fixture inventory: missing SDKs are errors and a selected partial run does not
qualify all consumers. Shared IR/reference checks do not enable remaining generated
advanced codecs. Check [verification status](../../../docs/verification/verification-xml-advanced-contracts-2026-10-19.md)
before claiming qualification. Schema language stays **1.20.0** and editor packages
stay **2.0.5**. Regenerate and rebuild clients of changed compiler metadata and
additive generated C++ package profile metadata factories, normally named
`document_profiles()`; a package with that rendered name uses
`document_profiles_metadata()` to preserve its constructor name.

Compiler/runtime **2.10.0** adds the incremental C++
[XML event reader](../../../docs/serialization/xml_events.md#incremental-byte-reader)
and checked [shared XML contracts](../../../docs/serialization/xml_shared_contracts.md).
Require `find_package(Serializer 2.10 CONFIG REQUIRED)` for
`<rohit/xml_event_reader.hpp>` or `<rohit/xml_extensions.hpp>`; the provider-independent
`<rohit/xml_events.hpp>` boundary remains available from 2.9. Keep borrowed views
alive through synchronous use and copy retained data. Feed arbitrary UTF-8 byte
slices, then call `finish()` before publishing a candidate; root closure alone
does not certify EOF. The reader requires the optional XML provider and rejects
DTD/user/external entities, other encodings and unsupported XML versions. Share
one operation context and cumulative input/event/state budgets. Provider-private
allocations and native limits are distinct from logical event-state accounting.

Use the shared guide's exact INI selections for explicit presence/nil, ordered
mixed content, map entry/key/value names, variant/type inventories and positioned
foreign extension slots. Compiler/runtime 2.12 adds selected presence, mixed,
map and variant mappings across all eleven outputs; use the token guide's exact supported subset. Unimplemented
backend contracts still reject with an explicit capability diagnostic. Do not
substitute a fixed-XSD override. Consult the current token guide for implemented
C++/Python/JavaScript/TypeScript/Java/C#/Go/Kotlin/Swift extension subsets and
C/Rust owning document companions. For mobile preservation, pass `extensionOptions` after
the other companion policies; declare namespace understanding explicitly.
Java/C# accept `XmlExtensionOptions` in trailing XML overloads; Go uses
`WithExtensionOptions`/`WithAllBounds` companions so existing option literals and
method signatures remain valid. Their slots and logical retained bytes are owner-local;
document bounds remain cumulative. C uses initialized `Type_xml_document`
companions with `Type_xml_document_init/free`, `Type_decode_xml_document` and
`Type_encode_xml_document`; Rust returns `XmlDocument<T>` from
`Type::decode_xml_document` and borrows its `extensions` for
`value.encode_xml_with_extensions`. C/Rust extension item and retained-byte
budgets apply cumulatively over the document. Keep the sidecar with its decoded
native value and release the C document through its generated free function.
Their bounded private known-XML transport adds buffers and scans; the token guide
documents its framing limit and supported graph restrictions.
Preserve original opaque namespace bindings
and allow for additional inherited context on the first nested canonical replay.
Treat `urn:serializer:xml:model` as closed Serializer metadata even when a slot
lists it explicitly; opaque preservation cannot bypass lifecycle validation.
Keep the
parsed AST stable while compiler contracts borrow it. For raw extension helpers,
provide full in-scope bindings, explicit foreign URI permissions and precise
positions; retain a complete candidate before publication. Required vocabulary
support means understood, never merely skipped/preserved. See the
[executed checks](../../../docs/verification/verification-xml-shared-contracts-2026-10-19.md).
Existing generated entry points retain their documented whole-object paths.
Schema language stays **1.20.0** and both editor packages stay **2.0.5**.

Compiler/runtime **2.8.0** retains schema language **1.20.0** and enables XML
support by default in CMake and the vcpkg manifest. Supply libxml2, or explicitly
select `-DSERIALIZER_WITH_XML=OFF` for a build without it. Existing caches retain
their previous selection. Inline XML declarations and fixed-XSD associations
already enable generated XML entry points automatically. Python adds OPC package
facades and richer XML bindings; consult the backend-specific contract and
executed checks before assuming parity. The
[2.8 verification record](../../../docs/verification/verification-opc-backends-2026-10-19.md)
identifies the selected OPC edition, Core schema fixture and actual backend checks.

Compiler/runtime **2.7.0**, language **1.20.0**, adds schema-authored XML mappings,
inline fixed-XSD association and reviewable XSD declaration import. Prefer one
namespace `using schema "local.xsd";` for compatible application-authored models,
or `serializer --import-xsd local.xsd --namespace models --output model.serializer`
to create declarations before ordinary generation. These are two separate steps:
`using schema` binds existing declarations, while `--import-xsd` creates them.
Compile the imported `.serializer` file afterward. Keep generated declarations
in the build directory and obtain their actual names from compiler output.
Import protects differing existing source files; use the maintained CMake
example's exact build-owned destination cleanup and depfile tracking when
regenerating. Review changed output and retain existing native IDs.
For changed XSDs, import to a fresh destination and compare the resulting model.
IDs derive from expanded XML identity and placement; a new unrelated field does
not renumber surviving identities, but an XML rename creates a new imported
identity. Declaration reordering can change positional bytes despite stable IDs.
For an existing application model, retain native field names, IDs, order,
defaults and behavior and use a supported inline `xml(name("client"))` exception
for the changed XML name. The XSD still determines XML child order. Select the
matching XML contract or supply an application migration; this does not make
renamed XML vocabularies interchangeable. Release 2.19.2's
[actual eleven-output inventory](../../../qualification/xsd_evolution/README.md)
checks this distinction and protected ambiguity failures without a second INI
field inventory or automatic rename inference.
For XSD tooling in 2.20.0, export the actual bound contract before rebinding it,
and use `--xml.compare-xsd previous.xsd` only for its documented reader direction.
Both XSDs must bind the same native model; an unavailable association or backend
capability is not a compatibility verdict. A sequence can widen to a matching
`all` regardless of declaration order; reverse and sequence-order checks remain
distinct. Retain native enum values/wire names when exporting supported enum
fields as `xs:string` restrictions with empty/preserve whitespace. Defaults/fixed
values must identify admitted enumerators, and the restriction must admit at
least one native enumerator; invalid bindings reject before generation publishes.
Token/collapsing/normalized spaces,
XML list-lexical enum collections and numeric/QName/list bindings remain rejected.
Enum text requires one nonoptional scalar. Fresh imports of exported enum XSDs
produce constrained strings; rebind authored models to retain native enums,
constructor defaults and application methods.
The [tooling inventory](../../../qualification/xsd_tooling/README.md) and
[executed Windows/Linux proof](../../../docs/verification/verification-xsd-tooling-2026-10-21.md)
qualify supported enum/tooling profiles. Its rich compiler/AST/XSD cases differ
from the ordinary eleven-output export/rebind runtime profile; retain each
backend's graph gates. See
[Windows UTF-8 CLI paths](../../../docs/serialization/xsd_tooling.md#windows-utf-8-cli-paths)
for the compiler manifest's platform scope.
Without XSD, use namespace/class `xml(namespace("uri"), scalars(attribute));`
defaults and field `xml(...)` exceptions instead of a second INI field inventory.
See the [automatic/manual examples for all outputs](../../../example/xsd/README.md)
and [XSD contract](../../../docs/serialization/xsd_tooling.md). Imported declarations
do not imply arbitrary XSD coverage or identical native layouts to authored models.
Both editor packages use **2.0.5**.

Use language-1.20+ `package` declarations in all eleven outputs to generate complete OPC assembly
and typed loading, sharing the runtime's cumulative budget, cancellation and
optional deadline. Keep models borrowed for synchronous Save and supply domain
validation before typed publication. Read
[package declarations and limits](../../../docs/serialization/xml_packages.md#declarative-c-package-profiles).
Selected whole-message compression framing/error/limit parity is
[qualified across all eleven outputs](../../../docs/verification/verification-compression-parity-2026-10-20.md).
Generated XML/package pipelines share cumulative contexts. Provider internals,
unsupported XSD/storage combinations and tree-free guarantees retain their exact
documented boundaries; do not substitute an unsupported mapping silently.
Do not infer complete 3MF or full-standard OPC conformance from a package round trip.
Ordinary native schemas must retain their output/layout and runtime efficiency.

Compiler/runtime **2.6.0**, language **1.19.0**, adds
[inline value references](../../../docs/schema/value_references.md) for export.
Start with `public borrowed(payload) value (id);` when output must match an ordinary
owning field. C++ `consumer.d = owner.b` stores exactly a const value pointer;
the owner stays ordinary and unaware of the consumer. The application keeps its
storage alive at a stable address, and temporary payload assignment rejects.
The one-argument `bind` API is also available. Encode the selected value directly,
with no document/object/generation/path metadata. These references are export-only:
reference-field decoders reject input, and matching ordinary owning schemas read
the bytes. Ordinary missing-field rules can leave a containing record's reference
unbound; they do not create owned replacement storage.
There is no owned fallback or default payload; missing/expired bindings fail access.
Use scoped reads instead of copying large payloads. Review the selected backend's
native lifetime contract rather than assuming a C++ representation in other outputs.
Shared/weak modes are optional: `shared(payload)` retains an independent payload,
while `shared(owner.field)` aliases an inline field and retains its complete owner.
Neither mode requires shared ownership of the consumer. Put collections in
an ordinary payload record and map it through a language 1.19 scoped
translator. Preserve source consistency during count/write passes and keep the
view within the callback. Direct legacy collection terminal fields remain
unsupported. The older named `reference` declaration below is a separate durable
address feature; do not choose it for value-only export. That release used editors 2.0.4.
Start with the [reference export examples](../../../example/references/README.md)
for one-argument binding and ordinary value output.

Compiler/runtime **2.5.0**, language **1.18.0**, adds
[typed field references and scoped translators](../../../docs/schema/references.md).
Declare `reference name = borrowed(owner.field);`, `shared(...)` or `weak(...)`,
then use the named address type in ordinary fields. Preserve explicit stable
member/base IDs and document/owner/generation identity. Bind live owners explicitly;
load owners before resolving decoded addresses. Shared C++ bindings retain the
whole storage owner. Read the backend's lvalue/weak/scope/boxed-owner lifetime
contract; bindings add no synchronization or automatic object-graph persistence.
Address codecs cover JSON and the three native binary protocols; XML generation
still rejects durable address fields. Current C++ XML supports selected value
projections and scoped translator output under the XML/package contract below.
That language release used editor packages 2.0.3.
Owners and targets must satisfy the supported ordinary owning/public projection
subset; diagnose unsupported managed, collection-element, optional/variant,
packed/view and reference-to-reference shapes. Do not reinterpret a numeric ID as
a checked reference. Use existing managed registry/editor APIs for managed state.
For one synchronous projected output, declare `public translator Target as name
{ field = source.path; };`. Use its scoped callback and generated target traversal
instead of constructing a whole owning target. Existing `conversion` remains an
independent value copy. Never retain a scoped projection or borrow a temporary;
preserve source consistency across count/write passes. Arbitrary transformations,
reverse setters and cached lazy ownership are not implied. Regenerate and compile
every selected backend; review the recorded qualification before claiming support.
Use the separate [durable address examples](../../../example/address_references/README.md)
as a small integration baseline: bind `consumer.d` to `owner.b`, edit the owner,
nest the reference and translator callbacks for immediate output, then explicitly
resolve a loaded address. The runner selects all eleven SDKs or a language subset;
the C++ folder also supports an installed Serializer 2.5 CMake package.

Compiler/runtime **2.4.0** supports XML simple text with scalar attributes and
explicit collection wrappers across every generated language. Configure
`[xml.class.source::type] text = member` for one required scalar and
`wrapper.member = element` for a repeated field. Text mappings permit attributes
but no child fields; wrappers are required even for empty collections. Regenerate
every selected output and verify the application's external XML contract.
Use [pinned XSD bundles and tools](../../../docs/serialization/xsd_tooling.md)
for local import/include allowlists, SHA-256 identity, dependency tracking,
supported nested complex types, export and directional reader compatibility.
Use the supported eleven-output scalar, namespace, ordered-content and wildcard
contracts for facets, unions, patterns and static complex-content extension.
Generated key/unique/keyref observers enforce the documented owner-local subset; the shared
[scoped identity validator](../../../docs/serialization/xml_advanced_contracts.md#identity-constraints)
is separate. Runtime providers do not inherit the compiler bundle resolver. Validate original input lexical values before
native conversion; canonical output must independently satisfy any pattern.

Select `[output] protocols = binary_none` or `--protocols binary_none` to restrict
native formats across all backends; defaults remain all formats. XML support is
built by default; XML generation is selected by XML declarations, XSD association
or explicit configuration. Shared runtime helpers may remain in generated source;
selection does not promise complete code removal. C++ ZIP64, explicitly bounded
nonseekable spooling, caller-authored transactional OPC loads, opaque extension
policies and canonical ordering are documented in the
[package guide](../../../docs/serialization/xml_packages.md). Codec callbacks must
share the package budget explicitly. Compression cancellation hooks borrow a named
callable that must remain alive through the last operation on either wrapper,
including cached input finish; see the
[compression guide](../../../docs/serialization/compression.md).
Generated package profiles are available in all eleven outputs with shared
payload/archive contexts. Automatic format-specific managed document bridges
retain their separate application integration contract; keep
[ownership and qualification boundaries](../../../docs/project/application_responsibilities.md) explicit.

For an OPC-based format, follow the
[OPC/3MF authoring requirements and restricted build example](../../../docs/design/opc_3mf_support.md).
The application defines payload models, XML mappings, content types, relationship
roles and domain validation. Generated OPC facades generate/parse standard
`[Content_Types].xml` and `.rels`; do not require application schemas for that
infrastructure. Use generated package declarations or C++ builders for
the application's declared profile. Select reject/skip/preserve separately for
unknown parts and relationships; required-capability callbacks override skip and
preserve. Retain the returned opaque bytes and scoped links for editing, and let
final Save revalidate their source/target membership. Removing a target requires
updating its retained links before Save. Typed loading returns a complete fresh
aggregate only after every selected part succeeds. See the
[executed policy matrix](../../../docs/verification/verification-package-profiles-2026-10-20.md).
Do not describe the restricted mesh
example as a complete 3MF reader. Consult the current eleven-output verification. When an
external XSD is available, bind its supported subset through `using schema` or
`[xml] xsd = path`, or import declarations first. Match expanded names and retain
existing native member IDs; do not infer stable wire identity from declaration
order. Keep local XML defaults/exceptions with their Serializer fields when no
XSD supplies placement; manual INI mappings remain an explicit application choice.
Python package streams are borrowed: short source reads are supported, short
sink writes fail, and no caller stream is closed or flushed. Keep mutable input
stable, bound noncontiguous byte-view copies, and retain cancellation through the
last sink callback. Staged Save closes owned descriptors even when stream
construction fails; postpublication durability errors report uncertainty. Follow
the [package boundary contract](../../../docs/serialization/xml_packages.md#declarative-python-package-profiles)
and [executed descriptor/lifecycle checks](../../../docs/verification/verification-package-lifecycle-2026-10-20.md).
The selected official Core 1.4 and W3C XML schema bundle now has executed
declaration-import, binding and consumer checks in the
[OPC backend record](../../../docs/verification/verification-opc-backends-2026-10-19.md).
Use the current supported-construct matrix when evaluating an external schema;
older blanket importer limitations do not describe that selected profile.
XML lexical validation and domain validation remain separate contracts.

For same-document managed entity references, static class identity, exact replica
IDs and editing previews, consult the
[requirements and ownership design](../../../docs/design/managed_entity_references.md).
The C++ implementation supplies these distinct operations:

| Requirement | Native API and integration rule |
| --- | --- |
| Live entity reference | Declare `managed_ref<T>` and obtain checked tokens through `get_id()` or `registry.reference<T>(id)`; resolve through `registry.resolve(token)`. Tokens retain native object identity, document scope and static `class_id`. Ordinary integer fields are ordinary data. |
| Initial required reference graph | Use the store constructor's final reference-initializer callback after native allocation and before validation/first history publication. Its `reference_builder` is callback-scoped. Use `optional<managed_ref<T>>` for an absent edge. |
| Independent import | Use `registry.import_storage<Plain>(source_document, storage, attach)` to allocate fresh native IDs and rewrite declared tokens in two passes. Supply an explicit external-target mapping or reject it; choose a declared owned closure that can attach to the destination root. |
| Edit an existing entity | Use generated `apply_editable(candidate)`. The target and surviving owned children keep their IDs, removed children retire, and new children enter with zero IDs. For pre-reserved objects, use active-transaction `registry.reserve()` and `registry.adopt_reserved(id, value, attach)`; each reservation belongs to that transaction and can be consumed once. |
| Isolated editing preview | Use `store.preview()`, edit `proposal.working()`, then `cancel()` or `accept_preview(proposal)`. Keep the source store alive and every operation on its owner thread. New IDs are reserved through the source allocator before exposure; cancellation burns them, and acceptance retains them exactly while checking the captured base. |
| Collaborative creation | Preserve identical document/object IDs across participants. Custom transports forward `reserve_ids`; standalone replicas initialize their native lease before creation. Configure the authority journal for crash-durable range issuance. |

Required transient/omitted edges reject. Language 1.23 binds copied direct self-token
fields and supported container wrappers to each named managed instantiation. Its
instantiate declaration retains the declaring file's version, including includes;
older declarations and shared nested/base recursive targets retain their restrictions.
Callable signatures keep their existing bindings. Follow the
[named managed reference contract](../../../docs/schema/named_managed_references.md);
other-language managed token generation is pending. Preview cancellation discards
value/history edits but can retain allocator reservations and their journal
records. A preview workspace's owner-thread lifetime is part of its API contract.

Choose initialization according to the baseline's provenance. New ordinary roots
enter the store with zero owning IDs and receive native allocation. Native
save/journal recovery restores identified state from its own metadata. The public
`document()` and `allocated_id()` accessors expose the document identity and native
allocation watermark for a coherent owner-thread capture. From runtime **2.14.0**,
first identified admission uses
`Store{rohit::managed::identified_baseline, storage, document, stored_watermark, options, validator}`.
Supply the exact generated `Store::storage_type`, including separated storage,
and the recorded watermark even when it exceeds the largest live ID. The native
storage codec checks bounds and durable-byte equality, resets runtime-only fields,
and validates complete identities/references before establishing one initial
history state. It generates no new IDs and attaches no live service or file.
See [initialization and its executed profiles](../../../docs/managed/cpp_runtime.md#first-initialization-from-identified-storage).
The [selected Windows and Linux proofs](../../../docs/verification/verification-managed-identified-baseline-2026-10-20.md)
each pass 197 cases across nine CTests;
keep that admission scope separate from complete release qualification.
Independent import intentionally allocates fresh identities; it cannot substitute
for identity-preserving reopening.

Generated persistent IDs and token object IDs default to `compact_varint` in all
three native binary codecs; memory width remains `uint32`, optionally `uint64`.
Select `[managed] id_encoding = fixed` or `--managed.id_encoding fixed` explicitly
for fixed-width payloads. All participants agree on the policy. Ordinary integer
fields, document/class identity and JSON numbers keep their contracts. Follow the
[migration guide](../../../docs/guides/persistent_id_migration.md). Build an
explicit offline converter through `serializer_add_migration_tool(TARGET name
SCHEMA model.serializer ROOT app::document [CONFIG model.ini])`. The generated
CLI handles managed saves and raw models with explicit source/destination ID
policies; raw input/output binary formats can differ. Normal loading does not
detect or convert old encodings. Resolve pending collaborative work and recover
journals through their original reader before converting a coherent save.
The [historical migration qualification](../../../docs/verification/verification-managed-legacy-migration-2026-10-20.md)
checks rebuilt 2.6.0 direct uint32 and separated uint64 artifacts. Match its model
and supported container contract; other historical layouts need their own reader
and explicit migration.

Compiler/runtime **2.3.1** preserves input schema copyright/license notices and
adds only the minimal Serializer provenance banner. Do not add a Serializer
license identifier or permission statement to generated application files.

Compiler/runtime **2.3.0** adds optional generated XML codecs and a fixed build-time
XSD subset across all generated outputs. Read the
[XML/package usage contract](../../../docs/serialization/xml_packages.md), use
the [generated examples](../../../example/xml/README.md), and review
[pending coverage](../../../docs/pending.md) before choosing a profile.
Enable the compiler's optional XML provider, select `[xml] enabled = true`, and
configure source-qualified classes. Children are the default; explicit scalar
attributes or a supplied supported XSD define placement. Regenerate after changing
the fixed XSD. Runtime prepared validation is explicit opt-in; some outputs require
a caller-prepared provider. Preserve existing schema grammar and protocol IDs.
General features require every output backend; the documented compression/package
profiles now cover all eleven. Whole-object C++ compression
wrappers borrow arbitrary compatible streams and require explicit `finish()`.
C++ ZIP/OPC file helpers perform complete staged Saves. To attach an external
document to a managed journal, use the separate 2.13 caller-supplied document codec
adapter; these helpers do not automatically supply those callbacks or a generated
profile/recovery integration. Keep history/session state in the side channel.

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

Compiler/runtime 2.1.1 supports schema language 1.17.0. Source builds and CMake
integration require CMake 4.0+. Every maintained language example has an optional
standalone CMake entry point. Configure a folder, build it, then run CTest; use
`SERIALIZER_EXAMPLE_USE_INSTALLED=ON` for package discovery. Non-Go SDK wrappers
also require Python 3.10+. See
[individual example builds](../../../example/README.md#build-an-individual-example-with-cmake)
and [2.1.1 migration](../../../migration.md#compilerruntime-211).
Go examples are native modules. With the installed `serializer` CLI on `PATH`,
run `go generate .`, `go build -o build/ .`, and `go test ./...` in the example
folder. Keep the maintained `go.mod`, `main.go`, `main_test.go`, `generate.go`, and entry schema;
generate the ignored `internal/model/model_gen.go` and depfile, then import that
model package from the application. Regenerate after schema changes; Go builds
and tests do not automatically run generation. Native tests exercise fixtures,
codec behavior, and rejection paths. No CMake, Python, C++ runtime,
or cgo is required for this native workflow. Optional CMake wrappers run the
same native commands and accept `SERIALIZER_EXAMPLE_COMPILER` for an explicit CLI.
Python matrix runners build isolated validation copies, separate from the native
source project. See [Go examples](../../../example/go/README.md).
For standalone compression examples, installed shared runtimes require 2.1.0+
backend metadata; older 2.x static packages retain dependency-based detection.
See [compression example requirements](../../../example/compression/README.md#build-and-run).

Compiler/runtime 2.0.2 supports schema language 1.17.0. The patch fixes private
arithmetic helper collisions in strict Rust builds and execution of self-checking
conversion examples through the shared example runner. Regenerate affected Rust
output and rebuild consumers; private helper names may change, while schemas,
public model/function names and wire formats retain their contracts. See
[2.0.2 compatibility](../../../migration.md#compilerruntime-202) and
[native Windows Rust qualification](../../../docs/verification/verification-rust-windows-2026-10-15.md).
C++ schema includes
generate separate dependency headers by default; use `--include-mode inline`,
`[output] include_mode = inline`, or CMake `INCLUDE_MODE inline` for a combined
header. Preserve the entry header's adjacent `.includes/` directory when copying
or installing generated APIs. C/Rust/Python/Swift/Kotlin retain inline defaults
and accept explicit dependency bundles in 2.34.0; copy the complete bundle and
compile all Swift/Kotlin companions in one module. Other backends retain combined
entry units. See
[include layout](../../../docs/guides/usage.md#share-declarations-with-includes).
For a 1.x compiler/runtime upgrade, update versioned package discovery to
`find_package(Serializer 2.0 CONFIG REQUIRED)` and regenerate through the
consuming build rules. `SameMajorVersion` discovery rejects 1.x package requests,
even without `EXACT`; unversioned discovery remains available. Keep supported
1.x schema headers and their meanings, model names, runtime APIs and wire contracts.
The original `serializer version 1;` remains exactly the 1.0.0 language baseline;
do not select schema language 2.0.0 to match this compiler release. That compiler
release used editor packages 2.0.2. See [2.0 migration](../../../migration.md#compilerruntime-200).
For new integrations, review [derived wire sizes](../../../docs/schema/derived_sizes.md),
[explicit value conversions](../../../docs/schema/conversions.md), and
[stateless generic constraints](../../../docs/schema/generics.md#stateless-type-constraints).

For language `1.17.0`, review [array and callable APIs](../../../docs/schema/array_and_callable_api.md).
Select the backing field with `expose array(public)`; add `readonly` to disable
writes and `const_value` only when the existing const indexing API returns by
value. State axes with `dimensions(Rows, Cols)` and preserve native unchecked
C++ bounds and temporary-owner borrow lifetimes. Logical get/set helpers check
each coordinate. Managed-language values snapshots allocate shallow slots; C
and C++ borrow native storage with owner lifetime. Do not substitute snapshots
or codec copies on native hot paths. Review naming collisions and dynamic
C++ vector<bool> limitations. Preserve exact identity defaults with owner-type
field `default(identity)`, without adding class-wide factory/defaultability
constraints. Use callable-local `when` for restricted factories, typed array
bindings/generation and structural delegation where supported. Static,
const_ref, fixed-array and expected callable extensions currently require C++;
expected additionally requires the selected C++23 host. Prefer portable
schema bodies and diagnose unsupported targets. New
[qualification](../../../docs/verification/verification-array-callable-api-2026-10-13.md)
passed the native 134-entry suite, all eleven SDKs and editor/package checks; retain earlier release verification as historical.
Native/external ordinary readonly C++ functions may declare unconstrained independent inputs in language 1.17. Portable numeric expressions still need declared proof; do not infer a numeric, factory or defaultability constraint. Constructor delegation can project known schema records or applications, including independent types inside their arguments; unconstrained host inputs do not establish inspectable members.

For language `1.16.0`, use a constructor or named-factory parameter such as
`Frame placement = {}` when `Frame` is an owning class's type parameter and
omission should use normal C++ `Frame{}` initialization. Keep that default trailing;
do not add a factory or class-wide defaultability constraint. Default construction
is required only when that argument is omitted: explicitly supplied nondefault,
nonassignable representations retain direct binding. Normal codec/default
construction keeps its separate representation requirements. Known supported
owning generated record arguments reuse the portable `1.15.0` default contract;
open native arguments and concrete primitive/enum/container substitutions require
C++ output. Independent callable input defaults cannot supply type deduction,
and empty fixed-array or ordinary function defaults remain unsupported. Host
default exceptions and constant-evaluation eligibility remain specialization-specific.
See [generic owner defaults](../../../docs/schema/construction.md#generic-owner-parameter-defaults-in-language-116)
and [qualification](../../../docs/verification/verification-generic-owner-defaults-2026-10-12.md).

For language `1.15.0`, use an owning generated class-valued constructor or named
factory parameter default such as `area input = {}` when omission should create
a fresh value using the class's schema field defaults. Full positional defaults
such as `area input = {10, 20}` remain available since `1.14.0` and require exactly
the record's supported fields in declaration order. Keep all defaults trailing.
Use `default(factory_name)` when omission should invoke an explicit public
zero-argument factory instead. Empty defaults do not invoke parameterized authoring
constructors or their hooks, add ordinary function defaults, support partial/named
object literals,
or, under that language contract, default an unqualified generic type parameter.
See [construction](../../../docs/schema/construction.md#empty-class-valued-parameter-defaults-in-language-115)
and [qualification](../../../docs/verification/verification-empty-record-defaults-2026-10-12.md)
before claiming verified backend support.

For language `1.14.0`, review [owning record APIs](../../../docs/schema/owning_api.md).
Use `expose readonly(public) as getter` for ordinary owning C++ storage that must
remain private while callers borrow a const reference from an lvalue owner.
Preserve owner/reference lifetime and diagnose naming-profile collisions; select
`naming = preserve` for authored backing underscores when needed. Existing native
friends remain sufficient; do not add a separate friend syntax or mutable getter.
Use direct constructor bindings for copyable nondefault/nonassignable inputs,
and typed recursive fixed-array/record defaults for fresh unit-Z values while
retaining supplied coordinates and fresh/missing-field decode parity. Open
coordinate representations need no new factory constraint for these literals.
Reciprocal constructor parameter types resolve after finite layouts; storage
cycles still reject. Use `public cpp { constexpr } constructor(...);` or the
corresponding readonly function declaration when the host supplies a visible
matching constant definition. Schema-defined eligible C++ constructors/functions
become constant-capable automatically; keep `constexpr` and `noexcept` independent.
Map only explicit typed operations through `arithmetic(unary(-) -> negated)` or
an owning constructor expression. Binary inline forms declare their typed right
operand. Preserve distinct quantity/result identities and shared checked errors;
do not infer field selectors or automatic unit operations. Other outputs use
portable helper methods; unsupported getter/external-constructor/ownership shapes
diagnose. Run the [four focused examples](../../../example/README.md#owning-record-apis)
and consult [qualification](../../../docs/verification/verification-owning-api-2026-10-12.md)
before claiming a backend or performance property.

For language `1.13.0`, when the required contract needs a native/external C++
signature, migrate readonly signatures requiring `noexcept` to
`function ... readonly noexcept(cpp);` for external definitions or append a
native `cpp { ... };` body. Use the typed qualifier on namespace/member functions
and dispatched member contracts; keep external definitions and behavior
overrides consistent with it. Other outputs ignore the C++ qualifier and retain
their own error behavior. It does not prove purity or inspect native bodies;
an escaping C++ exception terminates. Do not add it to `edit`, `expression(...)`
or `predicate(...)` functions, whose generated guarantees remain inferred.
For ordinary readonly native bodies, combine it with `constexpr(cpp)` in either
order when both promises hold. Independently generic functions with their own
input parameter list require a native `cpp` body for `noexcept(cpp)`; external
declarations using only class generic parameters remain supported. Automatic
external-definition checks cannot qualify every independent specialization.
Open-generic dispatched C++ qualification covers parameterless signatures.
Renamed/dependent parameter signatures inherited across open generic bases retain
existing backend contract-resolution limitations; parser qualifier validation
after substitution is not proof of generated dispatch support. Preserve native
adapters for affected models and compile each selected generated model.
Conditional `noexcept(...)` and arbitrary native
postfix tokens are unavailable. See [behavior](../../../docs/schema/behavior.md#explicit-c-noexcept-contracts)
and [verification status](../../../docs/verification/verification-noexcept-2026-10-12.md).

For language `1.12.0`, review [authoring construction](../../../docs/schema/construction.md)
before replacing native adapters. C++ supports overloaded checked constructors,
record arguments containing independent input types, constrained readonly method
inputs, and explicit `common_type<T, Input>` scalar result representations. Check
both operands into that representation before arithmetic; preserve strong record
identities. Generic readonly input deduction is available to host C++ callers;
schema expression calls to independently generic functions diagnose as unsupported.
Other backends diagnose these C++ API extensions. Qualify opaque
method-only C++ blocks with `storage(none)` rather than interpreting their tokens;
use `constexpr(cpp)` only for ordinary readonly native bodies that actually support
constant evaluation. Keep native storage/lifetime hooks unqualified.
Generic owning-record parameter defaults require a guaranteed `factory(name)`
constraint and `default(name)`, creating a fresh value per call. Concrete owning
schema specializations retain portable defaults; open native templates require C++.
Bind fields before
constructor validation, then run `initialize cpp { ... };` only for authoring;
codec/default construction and copies do not repeat host registration. Host
side effects and rollback remain application responsibilities. Preserve native
adapters when their contract exceeds these declarations.
Use leased external registries only with an owning lifetime/synchronization guard,
immutable provider version, and one-way invalidation state; validate each access.
Existing owning external registries retain their API. Leases do not make external
state transactional, undoable, or journaled. See the
[verification record](../../../docs/verification/verification-authoring-2026-10-11.md).

For language `1.11.0`, use `checked<T>(value)` for lossless numeric representation
bindings and preserve exact integer arithmetic without promotion through double.
Review [behavior](../../../docs/schema/behavior.md) for projected readonly calls and
lazy owning-record conditionals, [construction](../../../docs/schema/construction.md)
for fixed-array/default/checked bindings and C++ constrained input parameters,
and [generics](../../../docs/schema/generics.md) for record map keys. Diagnose unsupported
backend ownership or open generic-method combinations instead of approximating
the contract. Existing schemas keep their language-selected expression behavior.
Use the C++ [state registry](../../../docs/managed/state_registry.md) for pinned typed
entity lookup, stable field/base paths and optional application-key indexes.
Custom keys retain compact scalar persistent IDs; lookup never grants permission.
The [combined storage/index specification](../../../docs/design/database_storage_requirements.md)
retains the qualified C++23 minimum and covers standalone indexing, database data
authority, local/database/both index placement, typed queries and specialized
operator families. `storage(...)`, `index(...)`, `find_by`, placement and adapter
APIs are proposals. Compiler/runtime 2.30.1 requires C++23 for every C++ consumer;
follow the [migration steps](../../../migration.md#c23-minimum-in-2301). Use current explicit
registry/index APIs until implemented. Index placement never moves data or makes
a local cache authoritative for database uniqueness or authorization.
Treat a paged or customer index as derived state with explicit snapshot/lifetime
and commit boundaries; [paged indexing](../../../docs/design/paged_indexing.md) remains
a proposal rather than an available storage backend.
Use the [custom-ID examples](../../../example/custom_ids/README.md) for portable
application-owned scoped/type-tag keys and the C++ managed companion. Portable
host indexes do not supply Serializer's native history, journal or collaboration
engines; keep those boundaries explicit when adapting the examples.
Consult the [enhancement verification record](../../../docs/verification/verification-registry-numeric-2026-10-11.md)
for executed native, SDK and editor checks and their platform limits.

Language `1.10.0` also supports declared readonly calls and namespace functions.
Use [Boolean type constraints](../../../docs/schema/generics.md#boolean-type-constraints),
such as `T: (integral || floating_point) && !bool`, for category restrictions or
`T: int32 || double` for exact allowed types. The parser validates known/default
arguments and concrete nested applications; dependent category arguments are
checked after substitution. C++ native templates carry equivalent `requires`
clauses. Other outputs use the already checked concrete models. Do not replace a
portable constraint with an arbitrary native C++ concept name. Select only the
C++ output when the consuming application requires only C++.
Consult the [verification record](../../../docs/verification/verification-functions-constraints-2026-10-11.md)
for completed native, all-language and editor checks and their platform limits.
Use fixed unsigned `auto_size` headers only for native binary; default record
scope includes the header itself. Select `wire_only` when transport metadata
needs no application storage, and `size_scope(payload)` only when the agreed
wire value excludes its entire own key/value. Both choices are schema contracts.
Choose `auto_size_validation=verify|retain` per language/class in INI; retention
must keep structural header and normal decoder limits. Use
`omit(all_except(format,...))` for a nonempty comma-separated inclusion list.
Conversions copy exact public value projections into fresh target defaults;
qualify documented unsupported shapes and native ownership. Stateless constraints
prove generic defaults/arguments and forwarding eligibility without new wire data.
Follow [usage](../../../docs/guides/usage.md) and
[qualification](../../../docs/verification/verification-deferred-features-2026-10-11.md).
Raw object memory is not `binary_none`; do not infer an ABI dump from encoded sizes.
C++ constant-evaluation and emission-only profiles derive positional size headers
with both scopes and nested wire-only metadata. Use the
[compile-time byte-image workflow](../../../docs/serialization/constant_evaluation.md), qualify
its existing shape restrictions, and verify runtime byte parity.
For value creation,
review [typed construction and layout](../../../docs/schema/construction.md),
[checked behavior](../../../docs/schema/behavior.md), and
[generic inheritance/reusable headers](../../../docs/schema/generics.md).
Use schema constructors/factories to bind the actual generated owner of concrete
generic instances; keep opaque native blocks verbatim. Use `default(factory)`
for structured record defaults and `transient layout(empty)` only for stateless
passive records. The layout hint has no durable meaning and physical overlap is
backend-dependent; qualify size/alignment on the selected compiler.
C++ managed lowering retains ordinary-field construction in both storage
profiles; construct managed child occurrences through transaction APIs.

When replacing native C++ constructors on owning models with schema
`constructor(...)` declarations, rely on the generated public default
constructor. Do not add a zero-argument schema `constructor()` or duplicate
native `owner() = default;`.
Default construction initializes fresh schema field defaults; it does not make
arguments to a parameterized constructor optional. Language `1.11.0` supports
trailing checked literal or `default(factory)` parameter defaults for concrete
types. Language `1.14.0` adds full typed positional record/fixed-array literals and language
`1.15.0` adds empty defaults for owning generated record parameters. Language
`1.12.0` also allows class parameters with a guaranteed
`factory(name)` contract. Language `1.16.0` permits `Frame input = {}` for an
owning class's type parameter, with omission-only native C++ default construction
and no additional class-wide constraint. Independently generic input types still
reject literal defaults because an omitted argument cannot deduce them. Fixed
arrays require their full typed cardinality rather than `{}`.
`Value input = default(factory)` continues to need the explicit factory constraint.
Keep native construction when the exact unsupported call
signature is required. From language `1.14.0`, generated C++ constructor bindings
initialize members directly; earlier headers retain their default-then-assignment
lowering. Verify the declared language contract and each representation's binding
semantics when migrating native member-initializer construction. Opaque
`cpp { ... }` constructors alone do not trigger a generated default-constructor
declaration; preserve any native default constructor required by C++ there.

Use [class-specific comparisons](../../../docs/schema/record_comparisons.md) to generate
memberwise equality and lexicographic ordering through
`[language.class.source::qualified_name]` INI sections. Select durable/base values
or an exact own-field list; do not manually duplicate that field traversal.
Preserve NaN's unordered result, native C++ comparison categories and unsigned
widths. Portable selected collections, optionals, digests, union/variant and dotted
revision types remain unsupported; qualify each selected backend. Nested records
need matching comparison settings. Named instances inherit source settings or
select their own; compiler-internal C++ aliases share the template's selection.
Direct C++ managed values exclude hidden persistent identity; managed
`separate_values` comparison opt-in is unsupported and diagnoses.
Existing outputs remain unchanged without opt-in.

For bounded native output observations, use `binary_size<KeyType>(value, limit)`,
`binary_integer_size`, or `binary_string_size`; counting allocates no encoded
buffer. Use the existing `fixed_buffer` transfer API for actual message ownership.
Select `binary_scalar_normalization::positive_zero` explicitly for cache/digest
encodings needing equal signed-zero bytes. The default `preserve` retains current
wire bytes and source values; array and fixed-batch traversal remains generated.
Borrow native binary values directly with `std::span` and
`std::reference_wrapper`, including the bounded counting pass. Keep all referents
alive and unchanged until synchronous traversal finishes; borrowing adds no
ownership or synchronization and does not provide borrowed decode destinations.
Use generated `edit_<collection>()` callbacks and scoped borrowed lookups instead
of copying complete ordinary collections or mutating a live read snapshot.
Independently identified members continue to use identity-based typed editors.
Follow [output usage](../../../docs/guides/usage.md) and
[collection guarantees](../../../docs/managed/cpp_runtime.md).

For every Serializer task, review the installed revision's
[feature status](../../../docs/project/feature_status.md) and
[schema reference](../../../docs/schema/schema_reference.md). **Use every implemented
feature that applies to the requested contract.** Do not assume Serializer is
only a codec generator or manually duplicate capabilities it already supplies.

- Define all durable model, operation and journal fields in schemas. Use stable
  IDs/names, reservations, payload versions and compatibility checks for released
  formats. Keep runtime-only members explicit with `transient` when supported.
- Declare behavior in schemas. Use typed portable expressions for supported pure
  arithmetic, backend bodies for language-specific implementations, and external
  declaration hooks for application-owned code. Preserve access/order and check
  generated definitions with each selected language's compiler.
  Use the focused [custom C++ example](../../../example/cpp/custom_code/README.md)
  for preambles, access-qualified native declarations, inline methods, and external
  `.cpp` definitions. Select `cpp.naming = preserve` for authored native identifiers;
  keep durable data in schema fields rather than injecting opaque storage.
- Use generics, dimensioned/fixed arrays, owning variants, digests, compact fields,
  magic/omit and lifecycle metadata whenever the model requires their semantics.
  Select owning/view/packed representations and naming/output profiles deliberately.
- For editable state, use managed occurrences, generated checked editors,
  transactions, identity, undo/redo/navigation, notifications and runtime update
  scopes. Select the history mode and storage budget for the product contract.
- For durability and shared editing, use the implemented Save/load, journal
  recovery, migration, collaboration authority/replica, local sessions and
  synchronization facilities. Apply bounded exact-message decoding to untrusted
  input; test recovery and rejection before claiming support.
- Use the CMake/runtime targets, compiler diagnostics, formatting and editor
  integration appropriate to the build; run the maintained examples for every
  selected backend. Consult [state feature examples](../../../docs/managed/state_enhancements.md)
  when using language 1.6 or the new managed APIs.

Before implementation, record a concise capability selection in the existing
design or task notes: **used**, **not applicable** (with a concrete reason), or
**unavailable in the pinned revision** (with the exact required enhancement).
Update this selection and validation evidence when implementation changes.
Every relevant implemented feature must be used; exclusions must be explicit.
Mutually exclusive representations are alternatives, and an unavailable feature
must never be claimed as supported or silently replaced with a second framework.
This requirement does not authorize dependency updates, unrelated product
features, external publication, or mandatory enablement of features outside the
user's scope.

## Locate the library and the requested work

Find the application's existing Serializer dependency, schemas, generated-header
build rules, and callers before adding another copy. In this bundled layout,
the Serializer repository root is three directories above this skill. If the
skill has been copied or installed elsewhere, locate the matching checkout from
the application's dependency/build configuration; resolve the following files
there instead of relying on the relative links.

- Start with [README.md](../../../README.md) for the project overview and first-use workflow.
  Follow the [schema reference](../../../docs/schema/schema_reference.md),
  [build and generation guide](../../../docs/guides/build_and_generation.md), and
  [feature status](../../../docs/project/feature_status.md) for detailed contracts and limitations.
  For a first introduction to history, journals, collaboration, and host authorization,
  read the [managed features overview](../../../docs/managed/getting_started.md) and
  [authorization guide](../../../docs/managed/authorization.md).
  See [agent setup](../../../docs/guides/agent_integration.md) for discovery and use from another project.
- Check the dependency's actual release, build options, installed targets, and
  headers before using source-documentation features. The root `vcpkg.json` is a
  development dependency manifest, separate from the upstream
  `rohit-singh-gautam-serializer` port. Follow
  [distribution guidance](../../../docs/project/distribution.md) when package availability
  differs; prefer a source build of the needed revision rather than assuming a
  published port contains current managed APIs or generation helpers.
- For compiler/runtime 1.7.0 and newer, follow the
  [licensing contract](../../../docs/project/licensing.md): the schema/application
  author selects the generated output's license, including proprietary terms.
  Preserve the exact authored copyright/license notice text and required notices
  from included schemas; adapt only target-language comment framing and necessary
  syntax escaping. Current generation adds minimal provenance, no output license
  or permission preamble, and no inferred license when schema notices are absent.
  `LICENSE-GENERATED` supplies upstream 0BSD permission for Serializer-authored
  emitted support; this does not label the whole generated file, transfer ownership
  of that support, or override included-schema or third-party conditions.
  Application runtime headers/compiled helpers and managed support use
  `LICENSE-RUNTIME` (0BSD). Use `Serializer::runtime`
  for application linkage or `Serializer::managed` when needed. The compiler and
  `Serializer::serializer_lib` parser/generator API remain GPL-3.0-or-later. Check
  third-party dependency licenses separately, and do not apply these permissions
  retroactively to an older installed package.
  Put notices in leading schema comments, before the version header. Compiler
  library integrations should pass the full `parser::parsed_schema` to the
  C++, Java, or portable `generate_schema` function to retain file-level notices even
  when a schema declares no models; the CLI does this automatically.
- Use the [C++ managed interface](../../../docs/managed/cpp_runtime.md) for
  schema-driven identity and local snapshot history. `SERIALIZER_BUILD_MANAGED` defaults ON;
  link `Serializer::managed`, and declare bare `managed` on eligible leaf classes
  and independently identified members. Classes carry `persistent_id` directly by
  default; `[managed] separate_values = true` opts into ID-free values and storage
  wrappers. The compiler generates `model_traits` and typed editors such as
  `transaction.root().entries().edit(id)`.
  It provides manual/scoped/callback transactions, linear/tree snapshots, and
  bounded memory save/load. Select history at compile time with
  `model_store<Root, history_mode::linear|tree|disabled, Labels, Traits>` (choose one enum
  value; linear is the default). Each specialization stores only its selected
  representation. `store_options` has no mode field; `reset_history()` clears
  enabled history without switching modes. Runtime journaling is independent of
  history mode. A fifth template argument independently selects `store_features::none`,
  `journal`, `collaboration`, or `all` (the compatible default). Use `none` with linear/tree
  history for history-only stores; pair disabled history with `journal` or `collaboration`
  for those isolated configurations. Disabled features have no attachment storage/checks
  and their store APIs are unavailable. Identity, validation and transactions remain core.
  The authority's seventh argument and session facade's fifth authority argument accept
  `collaboration` to remove journals. Authentication remains host-owned within collaboration;
  standalone authentication is not a store feature. Built-in authorization policies and
  schema capability selectors remain proposals. See
  [feature selection](../../../docs/managed/cpp_runtime.md#independent-store-features).
  Labels default to `history_labels::disabled`, with no string member or serialized
  label field. Opt in with the third argument `history_labels::enabled`; custom
  traits are the fourth argument. Enabled stores accept named and unnamed
  transactions and provide `undo_label()` plus linear `redo_label()` or tree
  `redo_label(revision)`. Disabled history cannot enable labels.
  Linear history uses `std::deque` entries containing snapshots (optional labels) and a
  cursor, with parameterless `undo()`/`redo()` and no revision IDs or checkout.
  Only tree stores provide `redo(revision)`, `redo_children()`, and `checkout()`.
  Use `Store::outcome_type` for manual generic code; only tree outcomes include
  `revision`. Linear history evicts oldest
  states to meet count/byte limits; `max_revisions = 100` includes the current
  state and allows at most 99 undo steps. A changed commit after undo drops redo;
  failed, canceled, and no-op edits preserve it. Tree mode retains its map and
  rejects over-budget commits. Loads reject excess history without pruning;
  default label-free saves use version 3. Label-enabled linear/tree stores preserve
  versions 2/1. Loads must match history mode and label policy; cross-policy saves
  require explicit migration. Linear saves contain ordered entries and a cursor.
  Use `create_journal(path, journal_storage_mode::appended|sidecar, options)` for a
  new durable baseline, `recover_journal(path, options)` to reopen into an
  unattached store, and `save_journal()` for full Save. Commits, undo/redo/checkout,
  history reset, and ID reservations flush before publication. Both representations,
  ID widths, history modes, and optional labels are supported.
  `journal_dirty()` compares values with the full-Save baseline; memory `save()`
  does not clear it. `journal_sequence()` advances independently of undo position.
  Handle `transaction_status::indeterminate` / `journal_indeterminate_error` by
  destroying the fenced store and recovering into a fresh one before further writes;
  never blindly retry editing callbacks. `load()` cannot bypass an attached journal.
  The default appended mode writes each edit's already serialized snapshot once with compact length/CRC
  framing (snapshot bytes + 41 bytes for unlabeled linear history, + 33 for
  unlabeled tree/disabled). Navigation/reservations use 33-byte control records.
  Only base creation/full Save encodes the complete envelope and retained history.
  That appended path has no per-edit envelope encoding, byte-diff pass, snapshot
  output-buffer copy, or history deque copy. A retained native handle avoids reopening/seeking
  for each append. Use journal record/file budgets with store decoding limits. See the [journal guide](../../../docs/managed/journal.md)
  for versioned framing, single-writer locking, platform flush assumptions, and
  limitations. New sidecar generations instead encode copy/literal differences
  against immutable saved bytes, including collaboration wrappers, with no
  periodic full-snapshot fallback. Genuine new/history/pending values still consume
  storage; this is not field/entity delta history or a bounded-size journal.
  Set `journal_options.max_replay_bytes` for the cumulative expanded baseline and
  logical-record budget per version-three generation (default 1 GiB), independently
  of physical-file and per-record limits. Full Save resets it to the new baseline
  size; lower reader budgets reject before publication. Matching indexes are capped
  at 16 MiB each through adaptive sampling, not a hard total process-memory cap.
  External document codecs require sidecar mode and keep the main file data-only;
  follow the [document adapter workflow](../../../docs/guides/usage.md#external-documents-with-a-differential-sidecar).
  Compiler/runtime 1.11 adds opt-in verified `delta_journal` frames
  with `max_delta_chain`, resident admission via `max_resident_bytes`,
  `resident_bytes()`, and a shared `allocation_resource` hook for roots/control
  blocks. Account pinned roots and callback growth; do not describe admission
  accounting as a hard process peak limit. Generated containers/decode scratch
  still use their own allocators. Selectors, exclusions, allocator-wide limits,
  background checkpoints, built-in authorization policies and other-language
  managed runtimes remain future work. Do not present the complete proposals below as shipped behavior.
  Omit the store constructor's document argument for automatic namespace creation;
  explicit namespaces remain supported. Root/managed child IDs are allocated 1, 2,
  3... within each document, not globally. Undo/deletion never renumber or recycle
  consumed IDs; load restores the saved namespace and allocator high-water mark.
  Default clones retain IDs; opt-in separate-values clones remove them. Switching
  representations changes the wire binding and requires migration. Direct managed
  Protobuf output is unsupported. Consult the runtime guide before accessing storage.
- For explanations of the current C++ implementation, use the
  [managed transaction walkthrough](../../../docs/internals/managed_transactions.md).
  It follows the generated point example through store/candidate ownership,
  nested transaction types, the borrowed callback facade, editor forwarding,
  publication, failure, and cleanup. Keep diagrams vertical and distinguish
  implemented behavior from the broader managed proposals.
  The companion [editor walkthrough](../../../docs/internals/managed_editors.md)
  explains channel type erasure, resolver captures, generated getter/setter calls,
  and map/array identity checks. Editors avoid retaining element addresses;
  they resolve the target afresh and reject missing entities or closed transactions.
- For database persistence, read the
  [database integration guide](../../../docs/guides/database_integration.md). Treat its
  backend matrix as candidate storage mappings, not tested Serializer adapters.
  Establish the database product, edition/version, SDK, and JSON/typed-document
  or opaque-byte representation. Preserve integer widths, map entry arrays,
  presence semantics, and metadata boundaries; reject unsupported values or use
  an explicit reversible mapping. Use existing exact-message decoding on reads.
  The proposed sink operations are not public APIs or generator options. Keep
  dependencies optional and report actual provider verification separately from
  codec or mock tests.
- For indexes larger than RAM, read the
  [paged indexing study](../../../docs/design/paged_indexing.md). Disk-backed indexes and
  lazy model/history storage are proposed, not current runtime options. The eager
  store retains the complete model, identity table and history snapshots; existing
  resident admission accounting is not a hard process-memory limit. Distinguish
  bounded index cache memory from model, scratch, pinned-reader and OS memory.
  Preserve document-scoped IDs and the authoritative journal contract; a derived
  index must be validated against its exact storage generation and sequence before
  lookup, and rebuilt or caught up when stale. Do not add a database dependency or
  claim a paged managed backend merely because a provider supports disk indexes.
- Use the [managed design index](../../../docs/managed/README.md) for the proposals.
  The [capability contract](../../../docs/managed/capabilities.md) now defines
  history, collaboration, authorization, and journal. Every managed
  storage instance has a generated persistent ID, default `uint32`, centrally
  configurable through `[managed] id_type` / `--managed.id_type` (`uint32` or `uint64`).
  Its native binary payload defaults to `compact_varint`; `[managed] id_encoding`
  / `--managed.id_encoding` also accepts `fixed`. Do not confuse the encoding
  with numeric width or rename IDs when changing it.
  Use the same setting for every shared output. Separate ID-free values are opt-in; map keys and
  sidecar indexes cannot replace the managed ID field. Document scope, non-reuse,
  durable allocation reservations, and collision-free replica allocation apply.
  The [journal design](../../../docs/managed/journal.md) separates base/journal
  durability from undo history and defines appended and sidecar storage modes.
  Full Save durably publishes a replacement before retiring covered records;
  preserve newer edits, retained history dependencies, and allocation metadata.
  Changes may remain unsaved in the UI while durably journaled for recovery.
  These storage modes now have the synchronous C++ implementation described above;
  selector syntax and broader delta/checkpoint/distributed contracts remain proposals.
  Active/required authorization cannot
  be bypassed by member selectors. Imported/generated models must agree on ID type.
  The [data structures](../../../docs/managed/data_structures.md) and
  [language bindings](../../../docs/managed/language_bindings.md) describe optional
  store components, `model_store<Root, support>` / `managed` in C++, other-language
  composition, revision graphs, version records, and deleted-entity retention.
  Use the [C++ class walkthrough](../../../docs/managed/data_structures.md#c-class-walkthrough)
  for actual ordinary class excerpts from the [walkthrough schema](../../../docs/managed/walkthrough.serializer),
  with codec functions omitted, and their proposed runtime ownership mapping.
  Preserve generated access sections, qualified field types, initialization, and
  storage metadata; do not present handwritten templates, optional fields, or
  runtime pointers as current schema output. The example uses supported data
  syntax, but its record IDs are illustrative. Opt-in generated C++ companions use
  `managed_<type>_storage` with fixed `persistent_id`/`value` metadata and preserve
  payload field IDs inside `value`. Optimized history formats remain proposals. The snapshot
  runtime uses its own generated versioned envelope, described in the runtime guide.
- Treat [transactional history](../../../docs/managed/history.md) as the broader
  design contract. Bare `managed` and typed C++ setters are implemented;
  `transient` and guarded runtime updates are implemented from 1.11;
  `exclude(...)` and schema capability selectors remain unimplemented. Generation supports
  unpacked owning classes with public unrelated bases, nonpublic durable fields,
  owning variants and named concrete generic specializations; raw unions, virtual/
  repeated bases and recursive managed ownership remain restricted;
  unsupported shapes and non-C++ managed backends are rejected explicitly.
  Existing `stable_ids` identifies schema fields, not objects.
  Proposed change addresses combine a namespaced entity ID with a relative field-ID
  path; field IDs do not create independent entities or require delta storage.
  Scoped C++ transactions provide auto-commit on successful exit with an explicit
  outcome, rollback on failure/cancellation, and deterministic resource cleanup.
  Prefer the [callback transaction](../../../docs/managed/history.md#callback-based-transaction-execution)
  `execute_transaction(callback)` for a single synchronous action (supply a leading
  label only with `history_labels::enabled`). Pass a
  borrowed edit context, capture external IDs, and return the outcome after scope
  completion. The wrapper owns commit; callback revert/failure prevents it. Reject
  accidental non-void/async C++ callbacks and never retry the callback implicitly.
  Keep `begin_transaction` for caller-controlled lifetimes. Both entry points exist
  in the C++ runtime. Use generated `root()` editors; raw `update(callback)` remains
  a trusted low-level escape hatch and must not leak aliases or rewrite IDs.
  Preserve all three forms: begin with explicit commit, begin with automatic
  completion on healthy normal scope exit, and callback execution. Commit closes
  once; revert cancels. Guard deletion is not a distinct cancellation signal.
  Compact child addressing can omit repeated IDs only while preserving recoverable
  identity bindings; deleted entities can remain in retained history without live
  mutable objects. Storage budgets and measured latency guide optimization.
  The proposed [ownership/allocation contract](../../../docs/managed/history.md#ownership-and-custom-allocation)
  keeps lifetime control in the store while permitting state/history/scratch
  resources. These hooks are unimplemented, do not automatically redirect payload
  container allocations, and require explicit backend support. Internal reference
  counts remain optional and are separate from persistent entity identity.
- Use the [managed examples](../../../docs/managed/managed_examples.md) only for design
  discussion: hollow-cylinder differences, accounting ledgers, wordpad documents,
  and other models illustrate a generic facility, not implemented sample programs.
  Start beginners with the [nine point examples](../../../example/managed/README.md),
  each in its own folder and using a schema-generated managed point: store creation,
  transaction callbacks, generated editors, explicit/scoped commit, undo/redo,
  cancellation, and optional labels. Only `labeled_history` opts into names;
  the first eight and the ledger example use unnamed transactions.
  Use the real runtime throughout; do not replace the generated
  point, editor, or access machinery with handwritten teaching implementations.
  The callback example shows the existing low-level update API; generated editors
  remain the recommended application interface.
  Build all nine with `managed_point_examples`; each README shows expected output.
  The separate [draft ledger example](../../../example/managed/ledger/README.md) is runnable
  using actual `managed` declarations and generated editors without handwritten adapters.
  Use its annotated source and walkthrough to explain document namespaces versus
  object IDs/map keys/field IDs, transaction lifetimes, undo, and memory save/load.
  The [hollow-cylinder](../../../example/managed/design/README.md) and
  [wordpad](../../../example/managed/wordpad/README.md) schema examples have their
  own folders and are compiled in integration tests.
- Treat [managed state](../../../docs/managed/managed_state.md) as a companion proposal.
  Its broader generated `managed` interfaces, scoped authorization, merging, advanced collaboration,
  distributed transactions, and external-effect handling are unimplemented.
  Inferred companion generation is implemented in C++; selectors such as
  `managed(history)` and `managed(all except history)` remain proposed syntax.
  Plain containment does not activate nested managed annotations or infer managed
  support for the containing class; generated capability and occurrence differ.
  Managed targets must qualify through their own class marker or managed members;
  an unmarked leaf target is a compiler error and is never implicitly promoted.
  `exclude(history, collaboration)` is a proposed ordinary-value exclusion that
  preserves serialization and mandatory policy checks; it does not confer identity.
  Authentication and invitation management remain application responsibilities.
  Use the implemented [C++ collaboration runtime](../../../docs/managed/collaboration_runtime.md)
  and [runnable examples](../../../example/managed/collaboration/README.md) for
  actual integration. Include `<rohit/managed_collaboration.hpp>`, link
  `Serializer::managed`, and regenerate model headers for `visit_collaboration`,
  `visit_collaboration_fields`, and `merge_collaboration`.
  `collaboration_authority<Root, Mode, Labels, Traits>` privately owns the store;
  construct it without a command handler for ordinary generated edits.
  For immediate local editing, follow the [store integration guide](../../../docs/managed/local_collaboration.md):
  attach with `store.collaborate(session, options)`, bind a host-authenticated transport,
  and join a fresh placeholder before editing. Keep using normal generated editors
  and all existing `model_store` transaction forms. The store owns its typed session,
  local undo/redo, outbox, receive cursor and identity mappings. Journals flush each
  committed transaction with its client state; synchronization cadence is independent.
  Call `store.synchronize()` explicitly/on reconnect, or drive `synchronize_if_due(now_ms)`
  from the owner-thread event loop. The default interval is 5000 ms; zero is explicit
  only. `send_pending()` sends one transaction; `receive_changes()` only receives.
  Receive accepted changes before sending a successor; retry unresolved delivery
  exactly before pull-only receiving. Bind transports by reference and keep them alive.
  The supplied `collaboration_transport` is in-process; the host supplies network I/O.
  Callbacks are never replayed. Disjoint fields rebase; container membership and order
  remain atomic fields. Conflicts/rejections keep the complete local view, expose
  `acknowledged_read()` separately, and retain drafts in `state()`. After fixing a
  known denial/lease failure use `retry_pending()`; resolve conflicting intentions
  explicitly with retained drafts, `discard_pending()`, and a normal new transaction.
  Lost replies retain exact request bytes; a new epoch marks work uncertain for host
  reconciliation. Never claim durable authority deduplication across epochs.
  `store.undo()` and linear `store.redo()` create local inverse transactions.
  Prefer these store methods and `execute_transaction` for application modifications;
  the [store undo example](../../../example/managed/collaboration/undo.cpp) journals both
  clients through ordinary `create_journal`/`save_journal` calls. `synchronize()` handles
  request submission and accepted changes; applications need no `propose`/`deliver` helper.
  The internal journal adapter pairs model records with session/outbox metadata in
  the existing durable file mechanism, so recovery cannot separate an edit from its
  pending send or acknowledgement. No application-owned store wrapper is needed.
  For host lock requests, call `store.collaboration().reserve_operation_id()` after
  joining. Locks and document submissions share the persisted operation counter;
  never supply an independent counter for the same session. The host must retain
  each lock request for exact retries; reserving an ID does not enqueue that request.
  See the [store locks example](../../../example/managed/collaboration/locks.cpp).
  Tree branch checkout/reset are unavailable while attached; collaborative redo on
  a tree store uses `store.collaboration().undo(true)`. Disabled history retains only
  outstanding synchronization work; it has no undo stacks, inverse archive or history labels.
  Completed client transactions are removed once their accepted sequence is received;
  local transaction numbers remain monotonic. History-free authorities reject inverse
  requests and omit undo stacks, inverse snapshots and contribution-version tables.
  Enabled history labels survive sync/recovery. An attached journal uses the existing
  store journal pointer and file adapter with one model/session wrapper per frame.
  Attach the same session to a fresh store before load/recovery, then synchronize;
  ordinary model readers cannot open client checkpoint format 2. Save preserves
  pending work. One active writer owns each session within an authority epoch.
  Offline persistent IDs come directly from disjoint authority-issued native
  ranges and retain their exact values everywhere. Mapping slots stay empty;
  no translation mode is available. Do not remap arbitrary integer
  fields, map keys or application references. Client transactions/state bytes and
  authority command/history budgets are bounded. Enabled histories retain records;
  disabled histories compact completed client work. History-free client checkpoints use
  `serializer.collaboration.pending.v2`; history-enabled checkpoints use
  `serializer.collaboration.client.v2`. Older/mapped sessions reject; see
  [migration](../../../migration.md#compilerruntime-270). The store encodes
  each candidate once for authority publication. Non-journal synchronization uses a
  generated-field budget visitor instead of encoding/decoding checkpoints. Unpinned local
  appends without a journal reuse retained buffers; pins and journal callbacks keep
  copy-on-write preparation. Checkpoints borrow their state during encoding. Tree history
  checks incremental retained-byte totals rather than rescanning revisions. See the [local example](../../../example/managed/collaboration/local_sync.cpp).
  The lower-level acknowledged-state API remains available. Use
  `replica.propose(session, operation, callback[, grants])`; the callback runs once
  on an isolated draft using generated transaction editors. No application command
  schema or dispatcher is required. With default uint64 sessions, the authority uses protocol version 6
  with a version-one `model_change` snapshot payload. Custom synchronous host
  command handlers remain optional and use protocol version 5; contexts reject
  mixing the two. Rebuild the managed target for the new coordination record.
  Bind the claimed session to a separate trusted host session.
  Applications own session creation, storage, connection binding, and lifetime.
  Use `collaboration_session<Session, SessionTraits>` and its `authority<Root>`,
  `replica<Root>`, `records`, `result`, and `lock_cache` aliases for custom session
  types. String and unsigned integer policies are supplied; uint64 remains default.
  No registry or numeric mapping is imposed. Custom policies provide a stable
  `wire_name`, `max_encoded_bytes`, `valid`, `less`, and bounded canonical
  `encode`/`decode` functions. IDs must own their data and be default-constructible
  and copyable; equivalent identities must encode identically. Sessions retain their
  type in changes, locks, presence, retries, trusted arguments, and policy hooks.
  Use the collaboration codec helpers for typed records: protocols 7/8 carry a
  generated session envelope around existing command/model records; peers must agree
  on the session policy and codec. Default uint64 endpoints use protocols 5/6.
  Upgrade both peers: protocols 1..4 lack history fields and are rejected.
  Rebuild managed support for the added envelope records. See the
  [session policy contract](../../../docs/managed/collaboration_runtime.md#application-owned-sessions).
  Open an ID once per participation lifetime; after closure use a new ID or a new
  application generation within the ID. Closed IDs cannot be reused in an epoch.
  Ordinary `submit_change` requires the accepted base sequence and a nonzero operation ID;
  exact retries return the retained result, different-payload reuse throws, and
  conflicts require reconciliation plus a new operation ID. Never blindly retry
  callbacks or perform external effects before acceptance.
  `collaboration_replica` receives ordered trusted accepted snapshots; authoring a
  proposal does not change that state or grant edit permission. Draft IDs are
  provisional until acceptance. Resynchronize after a conflict even if the accepted
  sequence is unchanged: rejected creation may consume authority IDs.
  Use `snapshot`/`synchronize` for join and explicit
  recovery, `accepted_since` for delivery/retry, and treat gaps as resync requests.
  `change_lock` manages optional entity/subtree leases; proposals supply current
  `{target, generation}` references. Final generated ownership diffs include
  deletions and moves. Drive `advance_expiry` with host monotonic milliseconds.
  `lock_snapshot`/`locks_since` feed `collaboration_lock_cache`; prune acknowledged
  lock events explicitly. Incomplete caches confer no edit rights. Presence is
  advisory; ended/expired lifetimes require new IDs and cannot be resurrected.
  Host policy hooks gate final changes and lock requests but are not a built-in
  inherited permission engine. Transport, authenticated session binding, policy
  synchronization, and leader/epoch fencing remain host responsibilities.
  Save/load and journal recovery preserve document/history only; recover before
  opening sessions in a fresh fenced epoch. Rights and retry outcomes do not
  survive restart, so uncertain replies across epochs need application reconciliation.
  Configure retry/byte/session/presence/lock budgets. Default proposals carry the
  complete candidate snapshot, bounded by `max_command_bytes` (default 1 MiB);
  `max_created_ids` bounds new-ID reservations (default 4096). The runnable example
  combines the server authority and client replicas in one process. Session IDs
  carry no inherent local/remote/read-only role; the authority enforces write access.
  Retained retries and tombstones
  count for the epoch; no automatic deduplication compaction exists. This version
  sends full snapshots, rejects stale ordinary edits at document scope, and scans active locks for
  overlap/expiry. General merging, deltas, grouped leases, and other-language
  engines in the [broader design](../../../docs/managed/collaboration.md) remain proposals.
  Use `replica.undo(session, operation[, target_operation, grants])` and `redo`
  for conditional per-session history on generated model endpoints. Submit through
  the same trusted authority path and replicate only accepted results. A nonzero
  target must match the stack tip; zero chooses the latest visible tip. The inverse
  comes from retained authority history, not client-supplied values. Every accepted
  batch carries `history`: stable entity/field addresses, before/after presence and
  codec values, active contribution versions, and ownership dependencies. The outer
  session/operation/sequence supplies authorship and transaction grouping. Preserve
  unrelated edits; reject the complete inverse on conflicting field versions or
  ancestry, including ordinary write-away/write-back ABA changes. Structural map
  membership and array order are currently atomic at the containing field level.
  Ordinary nested values remain atomic fields. Recheck current policy and leases;
  failure/no-op preserves history, new local accepted edits clear that session's
  redo, and retries retain the original result. Use `undo_operation`/`redo_operation`
  only as stack inspection, never as permission checks. Version tokens restore
  earlier contributions on undo; accepted sequence always advances.
  Bound retained before snapshots and metadata with `max_retained_bytes`, and field
  projection/version tombstones with `max_tracked_fields` (default 100000). The
  document journal persists undo's result through restoration tag 5, including
  original deleted IDs, but not collaborative stacks or version tombstones across
  authority epochs. Upgrade journal readers for tag 5. Normal transactions cannot
  resurrect retired IDs. Store clients implement immediate local editing/undo and
  durable periodic synchronization as described above; general conflict-free merging
  and peer-to-peer authority ordering remain unimplemented. See the
  [store undo example](../../../example/managed/collaboration/undo.cpp) and
  [history contract](../../../docs/managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).
- Use [docs/guides/usage.md](../../../docs/guides/usage.md) for the schema, CMake, and codec examples.
- Use [docs/languages/portable_languages.md](../../../docs/languages/portable_languages.md) for JS/TypeScript,
  Go, C#, target SDKs, type mappings, limits, and interoperability.
- Use [docs/languages/native_languages.md](../../../docs/languages/native_languages.md) for Rust, Python,
  Swift, Kotlin/JVM, and C owning codecs, SDKs, limits, and ownership.
- Use [docs/languages/java.md](../../../docs/languages/java.md) for pure Java 17+ generation, profiles,
  type mappings, supported schema features, limits, and protocol compatibility.
- Use [docs/guides/cmake_integration.md](../../../docs/guides/cmake_integration.md) for the shipped
  generation helper, source dependencies, installed packages, and shared schemas.
- Use [docs/serialization/views.md](../../../docs/serialization/views.md) for view generation, mapping, and mutation.
- Use [docs/languages/output_configuration.md](../../../docs/languages/output_configuration.md) for
  language sections, coding profiles, naming, formatter configuration, and examples.
- Use [docs/editors/intellisense.md](../../../docs/editors/intellisense.md) for generated include
  errors, editor configuration, and generating headers without compiling consumers.
- Use [docs/editors/editor_extension.md](../../../docs/editors/editor_extension.md) for the optional
  VS Code extension, local VSIX installation, and CMake header assistance.
- Both editor navigation actions open the included schema when invoked on an
  `include`; a schema include can contribute multiple generated declarations and
  has no single generated definition. Type-reference definitions still target the
  matching generated declaration when available.
- Use [Visual Studio extension](../../../editors/visual_studio/README.md) for the
  separate Visual Studio 2022/2026 x64 navigation package and its build/install steps.
- Read [docs/serialization/wire_format.md](../../../docs/serialization/wire_format.md) when choosing protocols,
  limits, failure handling, or compatibility behavior.
- Read [migration.md](../../../migration.md) when updating older headers or APIs.

Use local instructions from the repository being edited. When a path or message
contract cannot be determined from the project, ask for that missing information.
Do not turn an integration request into a library redesign or implement a proposed
feature as a prerequisite without the user's request.

## Author or evolve the schema

- Use [schema generics](../../../docs/schema/generics.md) for reusable owning models.
  C++ emits native templates even without concrete schema uses: nested
  `result<T>`, `response<T>`, and `message<T>` can be used as
  `response<std::uint32_t>` with the ordinary generated codecs. Host type arguments
  must satisfy the selected codec and owning operations; do not add handwritten codecs.
  Keep optional `instantiate person_result = result<person>;` or concrete schema
  fields when other languages or compatibility checks need a concrete contract.
  All eleven backends expand those contracts; other languages do not expose open
  native generic APIs. Definitions and argument types must precede use.
- Use positive `uint64 N` parameters, trailing type/value defaults, and
  `array[N] T` or `array[Rows * Cols] T` for fixed owning C++ storage. Defaults may
  reference earlier parameters. Expression arithmetic is checked uint64 with
  decimal literals, parentheses, addition, and multiplication only. Extents are
  1..65,536, with 32 expression levels and 256 parse nodes. Generic parse/expansion
  limits remain 32 levels, 1,024 concrete applications, and 4,096 identity bytes.
  C++ emits std::array with compile-time checks for application-only specializations.
  With language `1.1.0`, use `array[] T field {values};` to infer a fixed extent
  from a nonempty initializer list. Count top-level elements; nested aggregate
  values and quoted commas do not create additional elements. A single char text
  literal uses decoded byte count, including escaped NUL and excluding any
  implicit terminator. Missing/empty defaults or extents above 65,536 reject.
  Preserve the explicit element type. Each declaring dependency needs its own
  `1.1.0` header; the original `1` alias remains exactly `1.0.0`.
  Compiler/runtime 1.6.0 also supports fixed arrays without explicit initializers
  in JavaScript/TypeScript and Python. Their owning arrays/lists start with exactly
  the schema extent and reject short/extra elements during encoding and decoding.
  TypeScript retains ordinary array annotations; the JS codec enforces cardinality.
  All eleven ordinary backends enforce fixed arrays in compiler 1.11; direct
  Protobuf output still rejects them. Qualify the selected SDK and count boundaries.
  Preserve existing runtime decode limits and exact cardinality. Missing keyed
  fields retain defaults; explicitly empty fixed arrays fail. Generic view
  declarations, raw managed unions, recursive managed ownership, and user specialization
  are unsupported. Ordinary generic values may occur inside managed roots; use
  generated replacement setters and the existing history/journal APIs.
  Consult [usage](../../../docs/guides/usage.md), [migration](../../../migration.md), and
  [qualification](../../../docs/verification/verification-dimensions-2026-10-04.md).
- Use `.serializer` files beginning with `serializer version 1.17.0;`, before declarations
  (leading comments are allowed). Rename older `.def`/`.struct` inputs and update
  build references. The compiler requires this header; library fragment parsing
  remains available through `parser::parse(input)`. Schema language version 1.17.0 is
  independent of compiler release 2.1.1 and wire protocols. Bug fixes and minor
  updates increment the patch number; new features increment the minor number.
  Minor and patch releases preserve full compatibility. Developers change the
  major number manually, with best-effort compatibility across major releases.
  Installed CMake packages accept earlier minor and patch versions in the same
  major release. Compiler/runtime 2.x packages reject 1.x discovery requests;
  update versioned consumers to `find_package(Serializer 2.0 CONFIG REQUIRED)`.
  Keep schema language headers at their supported 1.x contracts.
  Use `serializer --version` to inspect the compiler and schema
  versions; see [CLI and versioning](../../../docs/guides/command_line.md).
  The original `serializer version 1;` is exactly the `1.0.0` language baseline,
  not an alias for the latest language release. Only version 1 has that integer
  shorthand; future versions require all three decimal components. See the
  independent [language versioning policy](../../../docs/guides/command_line.md#schema-language-version-policy).
- Use `class`, `enum`, and `namespace`. Every member needs an explicit access
  modifier and a trailing semicolon. Classes/enums have no trailing semicolon.
- Share schema declarations with `include common;` after the version
  header and before declarations. Paths are unquoted, relative to the including
  file, and use forward slashes; `./` and `../` work, while spaces, backslashes,
  absolute paths, and quoted paths are rejected. A filename without an extension
  resolves directly to `.serializer`; explicit `include common.serializer;` remains
  valid. Use the explicit form for dotted stems such as `order.v2.serializer` or
  when targeting an older compiler. Keep physical files and CLI/CMake entry paths
  suffixed `.serializer`; no extensionless file lookup is performed. Both include
  spellings share the same dependency identity and do not affect wire data.
  Every dependency needs its own version header. Files load once per entry
  compilation; cycles are errors.
- Reopened namespaces reuse their scope during parsing. Duplicate qualified
  classes/enums and namespace/type conflicts are errors; same leaf names in
  different namespaces are valid. Referenced types still precede their use.
- Generate the requested entry schemas once. C++ defaults to one header per
  source file, with dependency headers beneath `<entry-output-filename>.includes/`
  beside the unchanged entry destination. Child headers use the entry extension
  and preserve the source tree relative to the common canonical ancestor of all
  dependencies and the entry. Equal source basenames in distinct directories stay
  distinct; relative source includes do not escape the generated output bundle.
  Canonical source paths must share a filesystem root in separate mode; use
  inline mode for dependencies resolved onto different filesystem roots.
  Headers include earlier sources in dependency-first parsing order, preserving
  declaration visibility and diamond deduplication. Definitions split by source;
  shared forward declarations may remain for supported callable signatures. Their
  reusable guards are described in the output configuration guide. File-level
  `cpp preamble { ... }` blocks retain compilation-unit scope outside
  `cpp.emission_only`: each artifact emits them before generated includes, so
  dependencies can use entry preamble helpers.
  Select `--include-mode inline`
  or `[output] include_mode = inline` for the previous combined C++ header.
  Other backends retain their combined entry units; their separate-file generation
  is unimplemented. Copy/install the dependency tree with the entry header or use
  inline mode. Matching overlapping C++ graphs retain individual type and managed
  companion guards; different definitions of one type produce a preprocessor
  error. Use matching naming/runtime/codec profiles.
  All generation helpers track transitive inputs through depfiles; C++ bundle
  depfiles list every generated file as a target. Library callers use
  `parser::parse_file(path)` and pass the complete result, including dependency-first
  `source_files`, to `writer::cpp::generate_files(schema, destination, options, mode)`
  to get `generated_file` records containing `path` and `contents`.
  `writer::include_mode::separate` is the default;
  `writer::include_mode::inline_output` requests one header. The API generates and
  validates all artifacts without writing files; callers publish the returned
  bundle. Existing single-string/stream writers retain inline output.
  Pass the parsed result to other writers to preserve source notices.
  See [include usage](../../../docs/guides/usage.md#share-declarations-with-includes) and
  [paired C++/Java examples](../../../example/includes/README.md).
- In owning representations, map `array T` to `std::vector<T>` and `map(K) T` to `std::map<K, T>`.
- With language `1.7.0` and compiler/runtime `1.12.0`, use built-in generic
  `optional<T>` for standard owning presence. C++ must use `std::optional<T>`
  directly; do not implement an application optional container. Use
  `optional<optional<T>>`, `optional<array<T>>`, `optional<map<K,V>>`, existing
  `array optional<T>` / `map(K) optional<T>`, and generic `optional<T>` fields
  as required. Each declaring included file needs its own `1.7.0` header.
  User-declared generic types named `optional` retain precedence. Initial values
  are absent; only empty schema initializers are supported. JSON uses `[]` or
  `[value]`, preserving every nested presence state; `null` rejects. All native
  binary protocols use a checked byte 0/1 plus a present payload, with existing
  target decode limits; C++ additionally accounts for logical allocation and work.
  C++ in-place readers retain missing keyed fields;
  explicit absence clears an engaged optional. Use fresh exact decoding before
  whole-object replacement. Preserve the old standalone C++ JSON null/value
  generic-argument contract when evolving an existing application.
  Owning standard mappings exist for all eleven outputs; C uses generated
  presence/value storage because C has no standard optional generic. Native
  managed optionals use existing occurrence traversal and typed editors; qualify
  identity, history, journal recovery and collaboration for the selected shapes.
  View/packed and direct Protobuf optional mappings reject. Changing presence,
  nesting or payload type is a wire migration; use the compatibility checker
  and retained payload revisions. Read [the optional contract](../../../docs/schema/optional.md)
  and run the [all-language optional examples](../../../example/README.md#optional-values).
  Consult [optional qualification](../../../docs/verification/verification-optionals-2026-10-11.md)
  for the executed all-SDK, native, managed recovery and editor checks.
- With language `1.3.0` and compiler/runtime `1.8.0`, use `digest` for variable
  application-supplied bytes, `digest[N]` for a decimal fixed byte extent
  (1..65,536), or `digest(algorithm)` for a supported standard output length.
  The bare form implies no algorithm. Preserve existing declared types named
  `digest`, which take precedence for bare references. Require `1.3.0` in each
  declaring included file; keep older compatible headers for unrelated schemas.
  C++ fixed values use `std::array<uint8_t, N>` and start with zero bytes;
  bare storage is an empty `std::vector<uint8_t>`. All eleven native outputs
  support storage/codecs with fixed cardinality checks. Map keys, nonempty digest
  defaults, C++ views, and Protobuf mappings reject explicitly.
  With behavior language `1.6.0` or newer, all three digest forms also work in
  native/external method return and parameter signatures. Keep native byte
  ownership, lifetime and result lengths explicit; methods neither hash nor add
  wire fields, and digest values remain outside portable expressions.
  Compute hashes explicitly through `<rohit/digest.hpp>` and
  `rohit::make_digest<rohit::digest_algorithm::sha256>(input)` with
  `Serializer::runtime`; portable applications choose their own provider.
  Native binary keeps the byte-array count, and JSON uses numeric byte arrays.
  Use `digest_to_hex`/`digest_from_hex` for application text conversion only.
  Serializer's creation helpers are 0BSD and introduce no OpenSSL dependency.
  Treat MD5/SHA-1 as legacy algorithms, and preserve the agreed meaning when
  migrating equal-width digest types. Follow [digest usage](../../../docs/schema/digest.md),
  [licensing rationale](../../../docs/project/licensing.md#why-the-license-was-separated),
  and [migration](../../../migration.md#digest-fields-and-creation-9-october-2026).

- Language `1.8.0` portable comparisons retain matching native signed/unsigned
  widths, float/double, boolean equality and declared enum identity. Adjacent
  integer literals bind to the declared width and reject out-of-range values;
  never convert uint64 identity or ordering through binary64. Public nested and
  inherited fields, sequence/fixed-array indices, exact uint64 `size`, qualified
  enum constants and total `math.is_finite` observations support readonly checks.
  Use `predicate(...)` only for bool readonly methods when reached semantic
  failures should return false. It validates reached paths only; `expression(...)`
  retains throwing/status failure and eager scalar floating input checks, except
  values observed by `math.is_finite`. Bounds and arithmetic inside finite
  observations still fail normally. C semantic predicate failure publishes false
  with `srl_ok`; null receiver/output plumbing remains `srl_invalid`.
- Use `all(items, literal_bound, item => boolean)` or `any(...)` for bounded
  readonly traversal. Literal bounds are 1..1,000,000 and length is checked before
  visiting elements. Every invocation shares a 1,000,000 visited-item budget across
  nested reductions, with short circuiting; empty all is true and empty any false.
  Binder names cannot shadow fields, parameters or outer binders. Maps, optional/
  variant projections, integer arithmetic, mutation and undeclared calls are not
  supported by this profile. With language `1.10.0`, declare namespace helpers as
  `function double total(double left, double right) readonly expression(left + right);`
  and call them or declared readonly member functions inside expressions. Calls
  keep exact signature types, evaluate arguments once in source order, and retain
  lazy branches. Declare a matching native body for each selected target; native
  effects remain the application's responsibility. Java emits namespace helpers
  as static methods and class functions as instance methods. Review the
  [function call contract](../../../docs/schema/behavior.md#declared-function-calls),
  including external-definition limitations by output language. Eligible C++ readonly typed methods preserve
  constexpr/noexcept. Checked arithmetic can constant-evaluate valid inputs while
  retaining runtime exceptions; C++20 square root/trigonometry stay runtime.
  Qualify native bodies separately. See [behavior domains and bounded
  checks](../../../docs/schema/behavior.md#portable-arithmetic) and run
  `test/typed_behavior_languages.py` against the required SDKs.
- For new portable byte-keyed maps, prefer `map(uint8)`. Existing `map(char)`
  output retains signed Java `byte` ordering and compiler-dependent C++ `char`
  ordering; other backends use unsigned bytes. High-byte keys decode across
  backends but do not promise identical encoded ordering. Preserve existing
  contracts; changing key types is a schema migration, especially for JSON.
  See [map ordering](../../../docs/serialization/wire_format.md#map-ordering).
- Keep literal spaces in quoted defaults, such as `public string label { "schema default" };`.
  The parser preserves quoted whitespace, escapes, and braces without rewriting the literal.
  Preserve wire names and defaults. Check the output naming policy before writing
  callers; `naming = preserve` retains existing schema-defined C++ identifiers.
- Put class attributes after the name, before a parent list. `stable_ids` requires
  explicit IDs on every member and parent; it does not assign IDs or compare old
  schemas. For new evolving integer-key contracts, prefer this check.
- When enabling `stable_ids` on an existing class, determine the previously emitted
  IDs first and write those exact numbers. Implicit slots count parents followed
  by members in declaration order; an explicit override does not shift later
  implicit slots. Parent/member IDs share the containing class's ID space.
- Keep IDs unique in `1..0x3fffffff`. Preserve wire names with metadata such as
  `public string name ("fullname", 3);` when renaming a C++ field. Do not reuse old
  IDs/names or renumber enum/union alternatives in an existing contract.
- For revision checks, run `serializer --input current.serializer --check-against
  previous.serializer --compatibility-protocol binary_integer` with the agreed
  native or Protobuf-binary protocol. Select `backward`, `forward`, or `both`
  through `--compatibility-direction` (default both), and retain old include files.
  Maintain `--compatibility-policy policy.json` with retired class field/parent
  IDs and wire names. Exit 2 means a compatibility hazard, 1 invalid input, and 0
  no hazard detected for the selected direction. This is a conservative separate
  checker, not a new `reserved` keyword or proof of application semantics.
  Read [schema evolution](../../../docs/schema/schema_evolution.md) before choosing policy
  scope or interpreting ordinal and unknown-field diagnostics.
- Native keyed readers reject unknown fields by default. For JSON, explicitly select
  `read_policy::flexible` and regenerate classes to skip additive fields.
  Explicit IDs alone do not change input policy. Positional binary still requires
  matching declaration order/types.
  Resolve incompatible changes through the application's versioning contract.
- Check supported types before choosing raw unions: generated alternatives must
  be trivially destructible. Ordinary generated enums use names in JSON/string-key
  fields; binary collection enum elements use numeric values.
- Use language `1.4.0` `variant(T = alternative, ...) field;` for owning alternatives.
  C++ generates `std::variant`, an enum derived from its active index,
  `emplace_field<Record::e_field::alternative>(...)`, and const/nonconst
  `visit_field(visitor)`; assign generated class members directly. No second mutable
  discriminator or generic JSON field builder is needed. JavaScript/TypeScript expose
  `field: {kind: 'alternative', value: typedValue}` and construct only the active value.
  The other language backends retain their existing tagged union mappings for both
  keywords. Keep `union(...)` for existing raw C++ contracts; its APIs are unchanged.
  Both keywords use the same four native wire contracts. Declaring files, including
  dependencies, require `1.4.0` only for selector syntax; existing bare user types
  named `variant` retain their meaning. C++ Protobuf and emission-only generation
  explicitly reject owning variants; managed C++ supports owning variants, while
  raw managed unions still reject.
  C++ binary views retain existing union access over the same positional bytes.
  See [unions and variants](../../../docs/schema/unions_and_variants.md) and the
  [paired all-language examples](../../../example/README.md#union-and-variant).
- Select only the requested representations: no keywords or `owning` means an
  owning class; `view` means both views; `view readonly` or `view mutable` restricts
  the views. Adding `owning` retains owning storage too. Attributes are unordered;
  `readonly`/`mutable` require `view`. One mode generates a plain class; multiple
  modes require `storage_mode::owning`, `read_only_view`, or `mutable_view`.
- Nested classes/parents must enable each mode their owner needs; map keys require
  read-only nested views. Declare referenced types before use. `packed` cannot be
  combined with `view`. `stable_ids` is independent of mapping, which is positional.
- `inplace` and `simd` are not implemented keywords. Views and little-endian
  defaults are implemented; use the dated
  [verification record](../../../docs/verification/verification-2026-09-17.md) for tested
  revisions and configurations rather than assuming all platforms are qualified.

For a new schema, a minimal explicit-ID example is:

```text
serializer version 1;

class person stable_ids {
  public string name (1);
  public uint32 age (2);
}
```

Adding `public string country;` is rejected; `public string country (3);` assigns
an unused ID. Existing explicit IDs work even without `stable_ids`.

- With language `1.2.0`, use [compact unsigned scalars](../../../docs/schema/compact_integers.md):
  `public compact_prefix strict uint32 value (3) {32};` or `compact_varint uint64`.
  Both keep the host type and ordinary JSON/Protobuf mappings. Prefix has a 30-bit
  maximum; strict checks binary writes, lenient deliberately retains the low 30
  bits without an overflow branch. Varint is full-width unsigned LEB128. Dynamic
  lenient varint values retain the low declared-width bits. All eleven outputs
  support owning unsigned scalars; collections, signed/floating types, enums,
  views, packing, magic and payload versions reject. Defaults must be unsigned
  literals fitting the host type; strict prefix defaults must fit the wire range.
  Each declaring include needs its own `1.2.0` header. Changing the encoding is a
  native binary migration; do not silently compact existing contracts.
  The [signed/floating payload design](../../../docs/design/compact_scalar_contract.md)
  now specifies proposed rounding, overflow, NaN, zero and scaled arithmetic.
  Its five names remain unsupported schema keywords; do not invent their syntax
  or advertise generated codecs from the reference-vector qualification.

## Integrate generated headers

Use the consuming project's existing dependency mechanism. When using CMake,
`add_subdirectory` and installed `find_package(Serializer CONFIG REQUIRED)` expose
`Serializer::runtime`, `Serializer::serializer`, and `serializer_generate`.
Use `serializer_generate(TARGET app SCHEMAS schemas/person.serializer)` after creating the
consumer. It supplies generation dependencies, the generated include directory,
runtime linkage, and the C++23 minimum. CMake 4.0+ is required; keep a newer C++
mode if the application already uses one. Source dependencies build the generator
as needed; installed packages use their installed executable. `SERIALIZER_INSTALL`
controls installation and defaults off when Serializer is embedded.
For `LANGUAGES NONE` source-generation projects, select the installed `compiler`
component in release 2.14 or newer to skip runtime-provider discovery. Maintained
installed SDK-only examples choose it automatically and add runtime discovery if
their scope later enables a native language. Normal
discovery and `COMPONENTS runtime` still load all installed targets and enabled
runtime providers. A later runtime discovery in the same scope can add those
targets after compiler-only discovery. For a cross build, use a host package with
`NO_CMAKE_FIND_ROOT_PATH` or explicit `Serializer_DIR`, or pass a host executable
with `GENERATOR`. See the [CMake guide](../../../docs/guides/cmake_integration.md#compiler-only-source-generation).
The generation helpers link the permissively licensed runtime rather than the
GPL compiler API library. Keep `Serializer::serializer_lib` only for deliberate
compiler/parser API use; building or running the GPL generator as a build tool
does not impose its license on application output.

With compiler/runtime 1.5.0 or newer, group several C++ targets needing different
INI configurations of the same schemas with `serializer_generate_variants`:

```cmake
serializer_generate_variants(
  TARGETS models32 models64 models_separate64
  SCHEMAS schemas/model.serializer
  CONFIGS profiles/managed32.ini profiles/managed64.ini profiles/separate64.ini)
```

Create the local targets first. Pair `TARGETS` and `CONFIGS` by order, one INI per
target; optional `OUTPUT_DIRECTORIES` must have that same length. Defaults are
`generated/<target>` in the current build directory. The helper parses each entry
schema once for the whole batch, tracks transitive schema/config inputs, and keeps
the existing generated-header properties and per-target/aggregate generation
targets. Shared helper overrides apply to every config. Keep one consistent model
definition within each consuming binary, especially when changing managed ID
width, separate-values representation, or naming profiles. See the
[variants helper contract](../../../docs/guides/cmake_integration.md#generate-several-configurations-from-one-parse)
and [usage examples](../../../docs/guides/usage.md#generate-several-configurations-from-one-parse).

For this repository's own build, use GNU `make` on Linux and `./make.ps1` on
Windows. Keep their common `configure`, `all`, `test`, `clean`, `rebuild`, and
`extension` commands, defaults, corresponding options, and behavior synchronized, and
verify both when changing either. Both default to Release and four build jobs,
build all enabled CMake targets, and use the `VCPKG_ROOT` toolchain when set.
The test target builds before running CTest; configure stops before building.
Run Windows MSVC tests from Visual Studio Developer PowerShell x64: compile-rejection
checks invoke MSVC directly and need its standard-library and Windows SDK include paths.
Clean verifies the expected repository root and runs `git clean -fdx` from that
root. **Clean and rebuild delete untracked source files and local configuration
along with ignored build caches, dependencies, and editor packages.** They
preserve tracked files, local tracked edits, the index, Git metadata, and nested
Git repositories protected by Git's single-force semantics; never use `git reset`
for cleanup. Build-directory and configuration options do not restrict the
repository-wide cleanup, and external build/toolchain paths are outside its scope.
Clean requires Git, not CMake, and performs no configuration or vcpkg validation.
Verify destructive cleanup only in disposable Git repositories.
Rebuild must complete repository cleanup before configuration, then use CMake's
`--clean-first` before building, including `make -j rebuild`. Reacquire dependencies
removed from inside the repository and reapply nondefault options previously kept
only in the deleted cache.
Normal builds require the same compiler, GoogleTest 1.18.0+, compression libraries,
and clang-format dependencies as direct CMake; Java, benchmarks, and fuzzers remain
opt-in. Use Make's `CMAKE`/`CTEST` or PowerShell's `-CMakeCommand`/`-CTestCommand`
to select executables outside `PATH`. See [wrapper options](../../../docs/guides/cmake_integration.md#build-this-repository)
for configurations, separate build directories, and CMake overrides.
GoogleTest is test-only: the repository manifest requires vcpkg `gtest` 1.18.0 or
newer, and enabled tests use `find_package(GTest 1.18.0 CONFIG REQUIRED)`.
Keep it out of application
runtime dependency requirements; applications link `Serializer::runtime` or
`Serializer::managed` with their enabled compression dependencies.
On Windows, reconfigure and rebuild existing test targets after updating this
checkout. Post-build staging copies their transitive imported shared DLLs,
including shared GoogleTest dependencies, beside the test executable. It derives
paths from CMake targets and selects the current Debug/Release configuration;
static-only dependency lists need no copies. This applies to repository test
executables, with verification status recorded separately in the build guide.
If a cached `GTest_DIR` selects an older package, update the dependency installation
and reconfigure with `cmake -S . -B <build-directory> -U GTest_DIR`, preserving the
build's toolchain and other options, then rebuild the tests. Follow
[test package migration](../../../docs/guides/cmake_integration.md#windows-test-runtime-dlls)
for DLL staging and targeted test commands.
If a cached Visual Studio instance no longer exists, use
`./make.ps1 configure -CMakeArgs '--fresh', '<required -D overrides>'` and reapply
the build's nondefault settings before building again. Inspect/back up the cache
first; do not preserve stale compiler paths or overwrite only the instance entry.
See [cache recovery](../../../docs/guides/cmake_integration.md#recover-a-stale-visual-studio-instance).
On Windows, `./make.ps1 all` and `./make.ps1 rebuild` additionally package both
editor extensions after a successful CMake build. They require Node.js 22+, npm
and Visual Studio MSBuild, restore locked npm dependencies, and reject unequal
extension versions. Packages go to `out/extensions`; the commands do not install
them. Use `./make.ps1 extension` for both packages without CMake configuration,
native builds, or repository cleanup; `./editors/build.ps1` is its packaging script.
WSL `make extension` delegates to that Windows script through `powershell.exe`
after `wslpath` conversion. Require Windows Node.js, npm, and Visual Studio MSBuild;
native Linux cannot build the Visual Studio VSIX and must report that limitation.
Linux `all`, `test`, and `rebuild` build native CMake targets; Windows `configure`
and `test` remain CMake-only. Clean uses Git and removes untracked/ignored extension
packages with the other repository artifacts.

The repository's Windows presets explicitly use Ninja and x64 architecture with
`strategy: external`. Visual Studio supplies the compiler environment; command-line
preset builds require an x64 developer environment and Ninja on `PATH`, plus
`VCPKG_ROOT`. Do not pass `-A x64` to Ninja or change the architecture strategy to
`set`. Clear an older CMake cache before reconfiguring after a generator/platform
change. See [Visual Studio folder builds](../../../docs/guides/cmake_integration.md#visual-studio-folder-builds).

If the default build reports missing clang-format, install a host clang-format
19+ and rerun configuration; vcpkg's GoogleTest dependency does not supply it.
On Ubuntu/Debian with the package available, use `sudo apt install clang-format-19`.
For other installation paths, pass `-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=...`
through `CMAKE_ARGS` or PowerShell's `-CMakeArgs`. Do not disable tests merely to
hide a missing formatter when a full build is requested. See
[formatter setup](../../../docs/guides/cmake_integration.md#formatter-setup).

For dependency or vcpkg CI checks, review the complete
[build requirements](../../../docs/guides/cmake_integration.md#build-this-repository)
and the actual port's options and host dependencies. Package builds should disable
all development targets; they then need no GoogleTest, formatter, JDK, Protobuf
runtime, or sanitizers. Consumer schema generation has separate host-tool needs,
especially when cross-compiling. See
[vcpkg package builds](../../../docs/guides/cmake_integration.md#vcpkg-package-builds)
for configuration, installation checks, and Windows static linkage. Do not infer
other platforms' verification from a successful local package build.

Use a revision with the explicit stream-offset return type, warning-clean pointer
accessors, and native-width identifier hash seed for GCC, Clang, and 32-bit MSVC
builds. Regenerate LLVM-profile headers after the storage-donor naming fix; only
generated local names change, with public APIs and wire data preserved.
Updated runtime headers support `binary_none` encoder `serialize_out(view)`
dispatch for mapped views; keyed/JSON protocols remain unsupported for views.
The [historical four-toolchain verification](../../../docs/verification/verification-toolchains-2026-10-07.md)
records full default C++ builds, CTest suites, and installed consumers on Windows
x64 (MSVC and clang-cl) and Ubuntu WSL2 x64 (GCC and Clang), using Release with a
C++20 baseline and managed records, SIMD, and all compression backends enabled.
All four full CTest suites and installed consumers passed; consult the record for
exact compiler versions and test counts. Earlier Windows x86 package/consumer checks are separate.
Current C++23 migration checks and their remaining platform limits are recorded
in the [2.30.1 qualification](../../../docs/verification/verification-cpp23-2026-10-23.md).
Native macOS, Android, and ARM Linux verification remains pending.

Schema scanning and runtime codecs enable SIMD by default through `SERIALIZER_ENABLE_SIMD`.
Supported x64 builds use SSE2 and select isolated AVX2 backends after CPU/OS checks;
short spans and other architectures retain scalar processing. Set this CMake option
to `OFF` before adding the source dependency when explicit SIMD must be disabled.
Installed generators/libraries retain their build-time choice. Runtime helpers
accelerate compact/formatted JSON strings and C++ fixed-width binary numeric arrays
on input and output in every key mode; matching-endian arrays and binary views use bulk copies.
Native JSON and ProtoJSON also scan long whitespace runs with bounded SSE2/AVX2
helpers, retaining scalar short gaps/tails and exactly space, tab, LF, and CR.
Input/work budgets and ProtoJSON message bounds constrain scanning; consumed bytes
are charged once per run with existing failure positions. Rebuild the runtime
library and consumers with matching headers; no schema regeneration is needed.
See [JSON whitespace scanning](../../../docs/serialization/runtime_simd.md#simd-json-whitespace-scanning).
Bulk array input validates the complete payload before destination changes and
retains allocation, input, nesting, collection, and work limits. Work exhaustion
uses the scalar path to preserve partial results and failure positions. Boolean
and enum arrays retain per-element validation. Input must not overlap decoded storage.
Keep scalar handling for single values, variable-length values, and view setters.
Link `Serializer::runtime` for pre-generated headers too. Disabling SIMD
retains bulk array reads/writes and direct JSON escaping. See
[runtime SIMD](../../../docs/serialization/runtime_simd.md) for scope, aliasing, and limitations.
Wire bytes and schema syntax stay unchanged; do not add a `simd` keyword or promise
a measured speedup. The [verification record](../../../docs/verification/verification-2026-09-17.md)
records boundary tests and bounded SIMD ON/OFF sanitizer/fuzz runs; benchmarks remain outstanding.

Call the helper once per target with all its schemas. For a shared generated API,
use an interface library and link consumers to it so one target owns generation.
Keep outputs in the build tree. Add custom format files selected indirectly by
configs to `DEPENDS`, or use `FORMAT_FILE` to select and track them. Cross builds
need a host-runnable executable passed through `GENERATOR`. Do not hand-edit
generated headers. Override layout through CMake `INCLUDE_MODE separate|inline`.
When publishing a generated C++ API, install the entry header and its adjacent
`<entry-output-filename>.includes/` tree at the same relative paths; use inline
mode when the packaging contract requires a single header. Consult the CMake
guide for options and public-header installation.

For IntelliSense include errors, check both the real generated header's existence
and the source target's include directories. In VS Code, use CMake Tools as the
C/C++ configuration provider; different profile headers share filenames and must
retain per-target include paths. A normal build generates headers automatically.
For generation without compiling consumers, build `<target>_serializer_headers`
or the aggregate `serializer_generated_headers` target. Configuration alone does
not create headers. No custom VS Code task or Serializer editor extension is
required, and `.vscode/*` remains ignored. Editor provider settings may be user-level.
The optional Rohit Serializer extension (`rohitjairajsingh.serializer-language`)
in `editors/vscode` highlights `.serializer`, offers
32Ã—32 language icons for Explorer/editor tabs where the file icon theme permits them,
versioned snippets, and invokes the same targets through CMake Tools. Run the root
`install_extension.ps1` with Node.js 22+, npm, and the VS Code CLI to build and
install it; `-SkipBuild` installs an existing VSIX. This installs the editor
extension only; application dependencies remain managed by the consumer. Extension
version 2.0.9 is shared with the Visual Studio extension and is independent of
compiler and schema versions. Keep both editor extension versions equal.
Embedded target blocks reuse available editor language grammars for highlighting;
install a grammar provider for targets the host does not bundle. This does not
provide IntelliSense or semantic diagnostics inside native blocks. Follow
[embedded source highlighting](../../../docs/editors/editor_extension.md#embedded-source-highlighting).
Both package descriptions and READMEs identify the
[Serializer repository](https://github.com/rohit-singh-gautam/Serializer). Configure
the consumer first, then use `Serializer: Generate Headers` or `Serializer:
Diagnose Missing Header`. Set `serializer.headersTarget` for one consumer; keep
profile include paths separate. Its IntelliSense command explicitly updates the
selected folder's C/C++ provider. Generation saves dirty schema/INI/CMake inputs
in that folder and requires workspace trust. Declaration/definition navigation
and `Serializer: Open Generated Header` only read available files. VS Code Git
index/history tabs use the displayed snapshot for local symbols and offsets;
includes and generated destinations resolve against the current workspace,
not a historical checkout. Never invoke
generation or configuration to satisfy navigation, or offer generation for a
missing destination. Declaration opens the originating schema, while definition
prefers existing generated output in any supported language and falls back to the original schema type
declaration if no generated definition matches. Generated C declarations use a direct tag index instead of tokenizing bundled
runtime bodies; preserve exact type/member offsets and the native interpreter
resource limits when extending navigation. Enum type prefixes in defaults
such as `AccountState::WaitingForReview` participate in both lookups; enum value
navigation is not provided. Follow schema includes to their actual entry
output. Separate C++ headers carry `// Serializer source: <absolute-schema-path>`;
banner-marked files use that exact source identity before depfile/basename lookup.
Follow existing quoted generated-header includes with bounded depth/file counts,
including child headers linked from entry-only active CMake registration. Keep
those children within their entry's selected configuration/profile, accept only
banner-marked generated destinations, and leave unrelated includes to the native
language service. Use existing `<output>.d` or workspace multi-output `.d`
dependencies when present; expose ambiguous legacy basename/profile matches as choices. An already active CMake model can
narrow lookup but navigation must also work without it and in Restricted Mode.
Use the built-in `Go to Declaration` menu action, including for whole-name selections.
Use `Go to Type Definition` in schemas to reach the source class/enum declaration
or included schema. All three actions accept declaration keywords and forward/reversed
`class ledger` selections. On a declaration itself, declaration/type definition select
that same schema name. Field names and primitives have no schema type destination;
generated-language type lookup remains with the native language provider.
Caller type references in C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python,
Swift, Kotlin, and C use their installed language definition providers.
For reliable ownership of renamed/flattened output, retain the compiler's
`--depfile` output in the workspace; native generators without a banner require
dependency metadata. Use `Serializer: Go to Schema Declaration` from the Command
Palette when other providers add non-schema declaration locations. That explicit
command also follows a native type-definition result for aliases/variables when
ordinary definition stops at their local declaration; native commands retain
their language semantics. For this repository's managed ledger tests/examples,
keep `SERIALIZER_BUILD_MANAGED=ON` in the active CMake configuration. Fresh builds
enable it by default; existing caches retain OFF values until reset or overridden
with `./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_MANAGED=ON'`.
Use the same build directory/configuration in CMake Tools as the command-line build;
do not combine include paths from unrelated targets. Do not override
VS Code's global commands or disable other language services.
For file-level navigation, right-click a `.serializer` file in Explorer or its
editor tab and choose `Serializer: Go to Implementation`. It uses the clicked
file's URI and the same available-output lookup without building or prompting.
See [navigation and limits](../../../docs/editors/editor_extension.md#navigate-available-schemas-and-headers).
Semantic schema diagnostics remain unavailable.
From a typed external schema function declaration, use Go to Definition on the
function name to locate available authored C++ out-of-class definitions. Both
2.0.0 editors match qualified templated owners and overload signatures, read
live unsaved buffers, and retain ambiguous candidates as choices. Lookup is
bounded and passive; do not build, generate, save or activate CMake to satisfy it.
Native schema blocks remain opaque. This lexical resolver does not implement
arbitrary C++ alias/macro resolution, dependent type equivalence or general
expression/member-call navigation. Keep ordinary native language services in
control of caller expressions. Follow
[deferred function navigation](../../../docs/editors/editor_navigation.md#deferred-function-definitions-1147)
and its [qualification record](../../../docs/verification/verification-deferred-navigation-2026-10-13.md)
for actual checks and editor-host boundaries.
The previous 1.1.48 editor source passed 141 shared/provider tests, 39,219
fresh-compiler bidirectional type/field navigation checks across all eleven outputs,
1,167 .NET/Jint resolver checks and 424 Visual Studio grammar asset/boundary
cases across 48 scopes. Split-header cases cover transitive includes, exact
source ownership, equal schema basenames, qualified cursor boundaries and
active CMake entry-only registration. Missing authored definitions fall back
to the exact schema function name. Both packages rebuilt through
`./make.ps1 extension` and passed package checks. The 1.1.48 packages were not
installed, and no current native editor-host run is claimed.
The earlier 1.1.47 qualification passed 138 shared/provider tests, 37,967
fresh-compiler checks, 1,116 .NET/Jint checks and the isolated native VS Code
definition host. At that verification VS Code 1.1.47 was installed in the normal
profile; the Visual Studio package was not installed. Keep those results
historical and distinct from the current package verification.
For Visual Studio 2022/2026 on Windows x64, use `editors/visual_studio` instead.
Run its `build.ps1` with Visual Studio's MSBuild to package and validate the VSIX,
then install it using Visual Studio's VSIX Installer. It shares the canonical
grammar, custom-type highlighting and navigation resolver. Native Go to Declaration
opens schema includes/types and maps generated declarations in all 11 languages
back to their schemas. Go to Definition/Ctrl+click on schemas prefer existing output.
Go to Type Definition on schemas opens the source type or included schema too.
For repository C# editing, select `editors/visual_studio/serializer_editors.sln`
using the workspace's `dotnet.defaultSolution`; do not load temporary project copies
from `out/`. The extension/test projects declare identical Windows runtime targets
for Visual Studio and C# language-server restores.
From caller code, use its language service to reach the generated declaration first.
The VS package does not supply VS Code's CMake commands or snippets; Node.js is
needed to build the shared bundle, but is not required at runtime.
Continue to use the consumer's CMake targets for generation and include paths.
See the [verification record](../../../docs/editors/editor_extension.md#verification-performed)
for tested editor hosts and remaining verification boundaries.
These targets still build the generator when needed and perform real generation:
honor any instruction to defer configuration, generation, or builds. Install/package
consumption has been smoke-tested on Windows for compiler 0.1.0, including versioned
package discovery and both generation helpers. The extension's Windows VS Code
host smoke test covers header generation, opening, and build failure handling;
live C/C++ IntelliSense reparsing and other editor platforms remain unverified.

Keep output settings in a generator INI config, with `[output] language = cpp`
and a `[cpp]` section. Java uses its own section as described below. C++ `coding_standard` values
are `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, and
`qt`. These are presentation profiles, not whole-guide compliance guarantees.
See the [profile examples](../../../example/coding_styles/README.md).

Default `naming = profile` renames schema-derived C++ types, fields, enum values,
union helpers, and view accessors. Wire names/IDs and runtime-required hooks keep
their original spelling. Resolve generated naming collisions; do not silently
change wire names to fix a C++ naming problem. Enum defaults referring to declared
values are translated; opaque C++ default expressions require `naming = preserve`
or an explicitly chosen literal/default change.

Regenerate C++ headers with this compiler to combine consecutive declarations with
the same access while retaining schema declaration order. Preserve intentional
blank lines in the schema between fields; adjacent fields generate compact lists.
Opaque native hooks still require generated access restoration. See
[C++ output layout](../../../docs/languages/output_configuration.md#names-compatibility-and-generated-syntax)
and [usage](../../../docs/guides/usage.md#generated-c-declaration-layout).

Formatting requires clang-format 19+ at generation time. Repository test/example
builds detect a suitable host tool through CMake's program search and, on Windows,
standard LLVM and all Visual Studio installations reported by `vswhere`. Discovery
runs again for a fresh build cache; installing LLVM or Visual Studio's C++ Clang
tools is still required if no suitable formatter exists. For reproducibility, pin its version and pass
`--cpp.clang_format` or set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` for this repository's
CMake rules; the shipped helper also accepts `CLANG_FORMAT`. `format_file` can
replace layout rules. Config paths are relative to
the config; CLI paths are relative to the working directory. CLI overrides take
precedence over config values. Add configs and custom format files to generation
dependencies. Use `format = false` only when another pipeline formats the emitted
source. The test build includes all profile examples; with tests disabled,
`SERIALIZER_BUILD_STYLE_EXAMPLES=ON` enables them independently.

The CLI uses short/long options (`-i`/`--input`, `-o`/`--output`, `-c`/`--config`,
`-l`/`--language`); bare argument names are no longer supported:

```sh
serializer --input schemas/person.serializer --output person.hpp
```

Repeat `--language cpp --language java` or use `--language cpp,java` to generate
both backends. Supply `--cpp.output person.hpp --java.output PersonSchema.java`
instead of `--output` for multiple languages. Prefix backend overrides with `--`,
such as `--cpp.coding_standard google --java.coding_standard oracle` and
`--java.package app.models`. All backend config settings have CLI overrides.
`--java.package=` and `--cpp.format_file=` clear configured optional values.

For several configurations from one schema parse, keep separate existing INIs
and repeat `--config` with corresponding output paths:

```sh
serializer --input schemas/model.serializer \
  --config profiles/managed32.ini --config profiles/managed64.ini \
  --config profiles/separate64.ini \
  --output out/managed32/model.hpp --output out/managed64/model.hpp \
  --output out/separate64/model.hpp --depfile out/model.d
```

Create destination directories first. Each config starts from defaults; configs
do not overlay one another. Scalar CLI overrides apply to all configurations,
and explicit CLI languages replace every config's selection. Generic outputs
require one selected language per config and exactly one path per config in
config order. For multiple languages per config, repeat language-specific output
options once per config selecting that language, also in config order. Never mix
generic and language-specific outputs. The shared depfile lists all outputs and
configuration/schema inputs. All backends finish generation/formatting before
writes start; parse/validation/backend failures preserve existing outputs, while
filesystem writes across files are not atomic. Batching preserves INI syntax and
each selected schema contract; one INI with named configurations is unsupported. See
[batch CLI details](../../../docs/guides/command_line.md#generate-several-configurations-from-one-parse).

Use the actual built executable path when it is not on `PATH`. Regenerate through
the real build pipeline when execution is part of the task; preserve an explicit
user instruction to defer generation/builds/tests and report what remains unverified.

## Integrate pure Java output

- Select `[output] language = java`, with `[java] coding_standard = serializer`,
  `google`, or `oracle`. All use conventional Java type/field/enum naming; Oracle
  uses four-space indentation and the other two use two spaces. They are limited
  presentation profiles, not full compliance tools. `naming = preserve` keeps
  valid schema identifiers. `package` optionally selects a Java package.
- Run `serializer --input account.serializer --output AccountSchema.java --config java.ini`.
  The output filename supplies the public outer class. Schema namespaces become
  static nested namespace containers. Each generated file includes its own codec
  helpers and needs only Java 17+, with no JNI, reflection, or third-party runtime.
- Compile with `javac --release 17 -encoding UTF-8 -d classes AccountSchema.java`.
  For CMake source/installed dependencies, `serializer_generate_java(TARGET name
  SCHEMA file OUTPUT Schema.java CONFIG java.ini)` creates a generation target;
  `serializer_generated_java` builds all registered Java outputs. Attach Java
  compilation in the consumer. Track custom dependencies with `DEPENDS` and use
  a host `GENERATOR` for cross builds. Java examples are opt-in through
  `SERIALIZER_BUILD_JAVA_EXAMPLES`; every example has its own input folder.
- Generate owning schemas only. Reject view/packed requests rather than promising
  Java buffer views. Parents use `base0`, `base1`, ... composition, not subtyping.
  Arrays use Java arrays; maps use ordered maps and box primitive keys/values.
  Unsigned scalars retain their bits in Java signed primitives. Preserve generated
  map ordering. Object/floating-point keys and opaque C++ defaults are unsupported.
- Call `value.encode(Schema.Protocol.BINARY_INTEGER)` and
  `Schema.Namespace.Type.decode(bytes, protocol[, limits])`. All four protocols
  are implemented; decode validates an exact message before returning a fresh
  object. JSON uses UTF-8, and binary scalars are little-endian. Java strings
  reject invalid UTF-8 byte sequences, matching C++ binary codecs and all other
  backends. Use `array uint8` for arbitrary payload bytes. See the Java guide
  before mapping unusual defaults or union payloads.
- For compiler 2.33+, independently opt into `[java] nio_output_buffer_reuse = true`
  or `--java.nio_output_buffer_reuse true` to add `int encode(Protocol, java.nio.ByteBuffer)`
  on owning codecs and readonly projections. Writable heap/direct/sliced windows
  use position through limit; success advances position, while failures preserve
  cursor metadata and may partially change only inside bytes. Preserve limit,
  order and mark; keep exclusive/non-aliased storage and avoid concurrent model
  mutation. Read-only buffers reject before writes, including empty records.
  Default false preserves native overloads; the array option remains independent.
  Writer/view/UTF-8 temporaries remain; this does not imply streaming, native
  acceleration or a whole-process allocation bound. Follow the
  [NIO contract](../../../docs/languages/java.md#caller-owned-nio-buffers).
- For compiler 2.32+, opt into `[java] output_buffer_reuse = true` or
  `--java.output_buffer_reuse true` to add `int encode(Protocol, byte[], offset, length)`
  to owning codecs and readonly projections. Default false preserves existing
  method sets and application overloads. Grant exclusive access to the window;
  it must not alias mutable model payloads or race model edits. Use the returned
  byte count after success. Capacity/model failure may leave partial bytes inside
  the window; outside bytes remain unchanged. Ordinary `encode(Protocol)` still
  returns independent exact-size storage. Temporary codec allocations remain;
  this is not a whole-process peak or allocation-free guarantee. See the
  [Java contract](../../../docs/languages/java.md#caller-owned-output-windows) and
  [dated measurements](../../../docs/verification/verification-java-output-buffer-2026-10-24.md).
- Use the generated `Limits` to bound message bytes, string bytes, cumulative
  collection entries, and nesting. Default JSON readers reject unknown fields;
  opt into `json<serialize_type::in, Stream, read_policy::flexible>` for
  evolving documents. Compatible readers skip additional fields, reject duplicate
  names (including escaped aliases), and preserve decode limits. Regenerate classes
  to enable this protocol hook. Native JSON also supports C++ `std::optional<T>`
  generic arguments as null or typed values; this does not add a schema type or
  binary optional encoding in older releases. For schema-native optional values
  use language 1.7.0 `optional<T>` and the contract below. See `docs/guides/usage.md#json-compatibility`.
  Malformed values fail with
  `IllegalArgumentException`. Do not share mutable input/object storage across
  concurrent codec calls. No explicit SIMD or native acceleration is implemented.
- Keep Java runtime/build verification distinct from C++ source-only status notes.
  With tests and Java examples enabled, CTest exercises compiled Java and two-way
  interoperability, including exact binary bytes. Qualify customer workloads;
  retain the scope of dated measurements instead of generalizing their timings.

## Integrate JavaScript, Go, or C# output

- Keep the `.serializer` compiler and generators in C++. Target SDKs execute or
  compile generated consumers only. The tracked `.inc` files contain embedded
  runtime source and must ship with compiler source distributions.
- Select `js`, `go`, or `csharp`; add `typescript` for JS declarations. Use
  language-specific output paths for multi-language generation. Pair `.mjs` with
  `.d.mts`, or ES-module `.js` with `.d.ts`, using identical JS naming settings.
- Read [portable language usage](../../../docs/languages/portable_languages.md) before
  choosing native types. JS 64-bit fields require `bigint`, Go constructors apply
  schema defaults, and C# uses an output-file outer class. Namespace flattening
  must remain collision-free. Preserve wire names and IDs across all targets.
- Generate one combined schema per Go package. Use includes for shared types.
  For native Go projects, keep a `go.mod`, an entry `model.serializer`, and a
  `generate.go` directive invoking `serializer --input model.serializer
  --language go --go.package model --output internal/model/model_gen.go
  --depfile internal/model/model_gen.go.d`. Create `internal/model` first in a
  new project: the CLI requires the output and depfile parent directory to exist.
  Maintained examples keep `README.md` there so checkout preserves the directory.
  Import the generated internal package from the application; preserve the module identity and keep generated output
  ignored. Prefer the native generate/build/test workflow; CMake and Python are
  optional integration and qualification tools. The managed wire Go client
  defaults to history; regenerate feature variants only in isolated matrix copies.
  Native maps are sorted during output. Use owning schemas with portable literal
  defaults, valid UTF-8 strings, supported map keys, and ASCII JSON characters.
  Views, packed storage, arbitrary C++ expressions, native acceleration, Protobuf
  and configurable output-buffer reuse are not implemented. Go supplies the
  [documented compression helpers](../../../docs/languages/go_compression.md).
  Python, Java, C#, JavaScript/TypeScript and Kotlin also have owning helpers;
  use their backend-specific provider and asynchronous/lifecycle contracts.
- Use exact-message decode and explicit limits for untrusted input. Generated
  decoders return a fresh object only after validation; Go returns nil on failure.
  Missing fields retain defaults, nested duplicates merge, and collections replace.
  Do not route JS messages through ordinary JSON.parse/stringify because full-width
  integers and duplicate-field semantics require generated codecs.
- When using CMake, integrate with `serializer_generate_source(TARGET name SCHEMA file LANGUAGE go
  OUTPUT generated/schema.go OPTIONS --go.package app)` or invoke the C++ compiler
  directly from npm, Go, or MSBuild build steps. The CMake helper tracks includes;
  supply a host `GENERATOR` when cross-compiling.
- Enable `SERIALIZER_BUILD_INTEROP_EXAMPLES=ON` to compile the
  [shared interoperability example](../../../example/interoperability/README.md).
  Test every producer/consumer pair, all four protocols, and all union variants;
  compare complete values and native binary bytes. Run malformed-input tests and
  relevant TypeScript/browser checks when changing target runtimes.
- Measure before making speed or allocation claims. The examples expose local
  throughput checks and Go has an allocation benchmark. Preserve direct field
  codecs, pre-encoded keys, checked capacity reservation, switch dispatch, and
  exact wire bytes while optimizing. Report untested platforms explicitly.

## Implement the C++ codec calls

With compiler/runtime 1.15.0, ordinary generated protocol-level `serialize_out`
and `serialize_in`, field dispatch and uncompressed stream entrypoints are
`constexpr` without enabling `constant_evaluation`. Verify the actual protocol,
stream and value operations: a declaration alone does not make runtime I/O,
allocation paths or every decoder constant-evaluable. Follow the
[direct-codec contract](../../../docs/serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers)
for the native scalar subset and its tests. Generated value factories, conversions,
readonly typed methods and observers use `[[nodiscard]]`; intentionally discard
results with `static_cast<void>`. Preserve throwing validation and allocation;
`noexcept` applies only to proven nonthrowing operations, conditionally where needed.
Keep ordinary constructors/methods `constexpr` so runtime inputs remain supported;
retain `consteval` for compile-time-only dimensions and exact-array factories.

For compile-time positional sizing or emission, use compiler/runtime 1.4.0 or
newer and read the [constant-evaluation guide](../../../docs/serialization/constant_evaluation.md)
and [usage example](../../../docs/guides/usage.md#count-and-emit-constant-positional-bytes).
Include `<rohit/constant_binary.hpp>`. Enable generated owning models with
`[cpp] constant_evaluation = true` or `--cpp.constant_evaluation true`;
the default is false. Regenerate through the normal schema/CMake pipeline and keep
one consistent enabled definition per qualified model across translation units.

Use `binary_none_size(value)` for an explicit validated count without output
allocation and `serialize_binary_none_to(span<uint8_t>, value)` for existing-memory
output. For ordinary owning values the latter uses the original optimized encoder
at runtime, without an implicit sizing pass. Use `make_binary_none_bytes<Factory>()` when the byte-array
extent must depend on variable transient fields; the factory constructs the value
twice and must be deterministic. Do not persist heap-owning constexpr model objects
or return views into destroyed local storage. Source and output memory must be
independent. Decode with the ordinary matching positional reader and exact framing.

The generator adds `serialize_constant_out`, a static marker and exact-type
alias. It leaves regular runtime `serialize_out` unchanged. Do not replace
runtime memcpy/memmove, batching or SIMD paths to achieve constant output. The
independent `[cpp] protocols = binary_none` / `--cpp.protocols binary_none`
selector removes JSON/keyed output; its default is all. It rejects requested Protobuf,
managed and view combinations. Retain all when those runtime capabilities are needed.
Maps, managed operations, views, compression, I/O and nonconstant customizations are
outside the C++20 constant subset. Raw unions require legal initialized active members.

For allocation-free construction, optionally select `emission_only = true`; it
requires constant_evaluation true, protocols binary_none and Protobuf false. Emission
DTOs borrow string_view/span fields under a terminal emission namespace and expose
only non-template `serialize_out(binary_none_output&)`. Construct inline owners
whose output method creates temporary views while buffers stay alive; avoid retaining
views into a copied/returned owner. Use the concrete writer for both count and supplied
memory. Generated emission headers include the lean constant_binary_output.hpp
support without the full runtime serializer/I/O/JSON parser closure; include
constant_binary.hpp when using the owning runtime-memory overload. Generate an
ordinary owning header separately for runtime reading. Maps,
managed/views, nonempty array defaults and multidimensional arrays are rejected.
Native generic type arguments retain their supplied C++ representation. Emission
writes fields logically, so copyability is not a wire requirement; generated headers
do not repeat field-level copyability assertions. Serializer tests verify supported
borrowed representations. Preserve the separate raw-union destruction constraints.

Verify actual generated models, independent expected bytes and runtime reading;
report the tested compiler/library combination and unavailable cases. Do not infer
all-toolchain support, zero compilation cost or exhaustive performance qualification
from one probe. The guide distinguishes implemented behavior from remaining regression
and measurement targets.

- For optional compression, read [the compression contract](../../../docs/serialization/compression.md)
  and [usage examples](../../../docs/guides/usage.md#compress-complete-messages). All three
  `SERIALIZER_WITH_ZSTD`, `SERIALIZER_WITH_LZ4`, and `SERIALIZER_WITH_ZLIB` backends
  default ON, and the repository manifest acquires their libraries by default.
  Disable unused backends explicitly for a reduced build. Select typed compression
  options at the whole-message boundary;
  keep inner protocol/endian/schema agreements explicit. Regenerate owning headers
  for member/static overloads, or use free helpers with existing generated headers.
  Input requires an explicit compression format and exactly one frame/member.
  Bound compressed input, expanded output, and backend window independently of
  object decode limits. Calls stage whole messages and finish the inner decoder;
  member parse errors can still partially update fields. Use the fresh exact
  helper for replacement semantics. Views must map stable decompressed storage.
  Keep named C++ cancellation callables alive through the last wrapper operation,
  including cached input `finish()`. Source staging polls around validated reads;
  final external writes/native candidates check again before successful completion.
  Swallowed writer reentry fences the active call. Failed input finish retains its
  original nested cause; unread-byte premature finish remains recoverable.
  Small zlib-advertised windows remain binding even with a larger caller allowance;
  targeted preflight preserves ordinary 32 KiB zlib/gzip/raw decode paths.
  Generated Python, Java, Go, C#, JavaScript/TypeScript, Kotlin, Swift, Rust and C supply the
  [documented whole-message helpers](../../../docs/serialization/compression.md#generated-language-availability).
  Dictionaries and concatenated streams remain unsupported. Custom C++
  `compression::backend` adapters can extend
  format support without generator changes. Refer to the separate
  [compression verification record](../../../docs/verification/verification-compression-2026-09-17.md).
  Start from the [runnable compression examples](../../../example/compression/README.md)
  for each built-in format, identity output, or the custom-backend interface.
  Set `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON` and `SERIALIZER_BUILD_TESTS=OFF`
  to build them without GoogleTest, enable the matching `SERIALIZER_WITH_*`
  dependencies, and run CTest with `-L serializer_compression_examples`. Regular
  test builds include the enabled examples; disabled backends have no example target.

- For Protobuf binary, ProtoJSON, or TextProto, read
  [the Protobuf guide](../../../docs/serialization/protobuf.md). Enable `[cpp] protobuf = true`
  or `--cpp.protobuf true`, regenerate, and select `protobuf_binary`, `protojson`,
  or `textproto` with the existing compile-time protocol-template calls. No
  external Protobuf runtime or `.proto` export is required. These codecs currently
  support C++ owning objects only; Java retains its four established protocols.
  Match the documented schema mapping with the peer, including union wrapper
  messages, field-number limits, map keys, and the absence of a `bytes` schema type.
  Input uses Protobuf defaults and commits a replacement object only after exact
  decoding succeeds. Unknown binary fields are skipped and discarded; unknown
  named fields/enums fail. Do not promise preservation of field/union presence,
  unknown fields, well-known-type mappings, or arbitrary Protobuf schemas.
  ProtoJSON duplicate ordinary fields replace prior values rather than merging;
  a later `null` resets the field to its Protobuf default. Binary messages still
  merge. Regenerate Protobuf-enabled C++ headers for corrected union-wrapper
  handling. TextProto checks decoded string storage before growth; temporary
  token/numeric scratch is outside `max_allocation_bytes`, so also bound input,
  string/token length, and work. See
  [finalization verification](../../../docs/verification/verification-finalization-2026-09-18.md).

- Select `json`, `binary_none`, `binary_integer`, or `binary_string` according to
  the agreed message contract. Do not silently change protocols to improve size
  or speed. `format::compress` means JSON whitespace suppression.
- Binary fixed-width scalars default to little-endian. Inspect `Protocol::wire_endian`;
  an explicit third `binary` template argument selects another byte order when
  required. Compact prefixes keep their own encoding. No endian/protocol marker,
  auto-detection, or compatibility aliases are provided; agree on the format with
  the peer. The schema-language version identifies neither message byte order nor
  wire version. Independent legacy/current fixtures cover explicit selection in
  all three native binary modes. JSON is unaffected by numeric byte order.
- Keep wire byte order independent of host CPU byte order. The shared profile is
  little-endian on both little- and big-endian machines; only C++ currently offers
  explicit big-endian wire selection. Use the interoperability runner's
  `--big-endian` option for QEMU s390x C/C++ participants and shared frozen
  positional bytes. See [qualification commands](../../../example/interoperability/README.md).
- Require valid UTF-8 for binary strings in every language, including C++ owning
  codecs, mapped views, and mutable string setters. Preserve embedded NULs and
  do not normalize Unicode. Use `array uint8` for arbitrary bytes; migrate older
  C++ raw strings using the [migration guide](../../../migration.md#binary-string-validation).
  Preserve leading U+FEFF as string data too. Regenerate Swift output for strict,
  platform-independent UTF-8 decoding and Rust/C output for corrected large
  floating defaults; see [finalization fixes](../../../migration.md#finalization-correctness-fixes).
- Keep C++ binary text validation strict by default. Only for text whose validity
  is guaranteed elsewhere, select `binary_text_validation::unchecked` as the
  third `binary_none`/`binary_integer`/`binary_string` argument (after `Stream`),
  or the fifth `binary` argument. This compile-time policy propagates through
  nested owning values and stream rebinding and removes runtime text scans only.
  Bounds, limits, Boolean/enum checks, and exact-message checks remain active.
  Invalid UTF-8 is still not interoperable; peers may remain strict when text is
  valid. JSON, generated view mapping/setters, and other runtimes have no new
  opt-out. See [usage](../../../docs/guides/usage.md#choose-c-binary-text-validation).
- Rebuild the runtime and consumers for the dedicated binary UTF-8 SIMD backend;
  schema regeneration is not needed. AVX2 validates complete Unicode sequences,
  with bounded SSE2/word ASCII and scalar Unicode fallback. The existing
  `SERIALIZER_ENABLE_SIMD` setting controls explicit vector acceleration, not
  whether validation is enabled. Consult [UTF-8 verification](../../../docs/verification/verification-utf8-2026-09-18.md)
  for measurements and test scope; do not promise zero strict-validation overhead.
- Regenerate C++ headers for structural stream concepts and implicit standard-stream
  adapters. No `rohit::stream` inheritance is required: use `input_buffer` or
  `output_buffer` for the contiguous fast path, or standard-style byte-stream
  concepts for external I/O. Generated calls accept standard streams directly.
  Memory input (`istringstream`/`stringstream`) borrows the unread suffix; known
  file streams use 64 KiB batches and erased/custom byte streams use 8 KiB batches.
  Standard memory output uses generic buffered writes. No measured speedup is claimed.
- For complete stream-specific consumers, use the seven
  [iostream example folders](../../../example/iostream/README.md). They share a
  52-class, 645-field schema and verify large messages through memory, file,
  explicitly buffered, erased, and custom non-seekable streams. Enable
  `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON` to build them independently without
  GoogleTest, then run CTest with `-L serializer_iostream`. The standard test build
  includes them too. Keep their concrete stream types where specialization matters.
- Regenerate owning C++ classes to use matching static entrypoints:
  `Type::serialize<Protocol>(stream, value)` writes a const borrowed value and
  returns void; `Type::deserialize<Protocol>(stream[, limits])` returns a newly
  initialized owning value marked `[[nodiscard]]`. Defaults and stream adaptation
  match the existing member APIs. Failures throw without returning a partial
  object, but may consume input. Keep `serialize_in` for destination storage reuse.
  The static factory retains buffer-input semantics without an implicit `finish()`;
  byte streams still receive exact-message validation. Views retain `map`.
  Both new names are reserved in owning class scope; preserve wire names and IDs
  with metadata when resolving schema member collisions.
- Byte-stream input means one bounded EOF-delimited message and includes `finish()`
  validation; it is not incremental parsing and adds no wire framing. Use a bounded
  source or exact-size buffer for framed traffic. Regenerated owning classes expose
  `destination.serialize_in<Protocol>(input, limits)` for explicit limits; the
  one-argument overload retains default behavior. The free
  `serialize_from<Protocol>(input, destination, limits)` remains an alternative.
  Low-level codec templates accept a concrete buffer type; existing buffer calls
  remain valid. Keep memory-stream
  storage stable while decoding. Never imply that concepts prove lifetime or
  alias safety. See [stream contracts](../../../docs/guides/usage.md#stream-concepts-and-implicit-adapters).
- Use `<rohit/file_stream.hpp>` for a reusable owning file stream supporting both
  generated serialization and journals. `file_stream` satisfies existing byte-stream
  concepts and adds `sync()`, absolute `seek()`, `truncate()`, and `size()`.
  `file_open_mode::create` is exclusive; `update` preserves existing file bytes;
  `read` is read-only; `lock` owns a stable exclusive writer lock. Keep it
  thread-confined. Serialization and destruction never implicitly sync.
  Memory streams in `stream.hpp` remain memory-only implementations.
  `managed_journal_stream.hpp` provides bounded `write_journal_frame` /
  `read_journal_frame` helpers accepting native/custom buffers, iostreams, and
  file streams. Buffer reads borrow payloads; byte-stream reads own bounded
  payloads. Preserve input lifetimes and use the managed path APIs when a full
  document container, locking, sequence allocation, and recovery are required.
  Optional persistence concepts do not change ordinary stream requirements.
  `durable_output_adapter` requires a real host synchronization policy for the
  exact borrowed destination; plain iostream flushing is insufficient. Never
  claim a memory stream is persistent merely because framing succeeds.
  The managed store owns a journal sink; its implemented file adapter uses these
  stream facilities. Database sinks require an implemented and tested Serializer
  database adapter. None currently exists; do not add speculative database support.
  See [file streams](../../../docs/guides/usage.md#file-streams-and-journal-records) and the
  [runnable journal example](../../../example/managed/journal/README.md).
- Open binary files in binary mode. Adapters borrow streams, retain exception masks,
  and leave explicit flushing/closing to the caller. I/O failures can consume input
  or write an output prefix; decode errors may partially update destinations.
  Encoded staging storage is separate from decoder allocation accounting. Custom
  protocols retain their constructors unless they explicitly provide compatible
  `stream_type` and `rebind_stream` hooks; inherited hooks do not replace derived
  protocol behavior. Schema parser/writer entrypoints use the same stream concepts.
- Encode into an appropriate stream. Reuse `full_stream_auto_alloc` capacity when
  useful, resetting only after readers of the previous message have finished.
- Use `rohit::string_stream` from `<rohit/stream.hpp>` for direct string-backed
  output, including `json_out<true, rohit::string_stream>` for formatted JSON.
  `view()` borrows only written bytes; lvalue `str()` copies them, while
  `std::move(output).str()` transfers the trimmed string and empties the stream.
  Growth resizes writable characters and rebases internal aliases; borrowed
  pointers/views must not survive growth, moves, extraction, or buffer reuse.
  Construct from a byte count for an initial writable extent, or move a string
  to continue after its existing prefix. No schema regeneration is needed for
  already concept-enabled headers. See
  [string streams](../../../docs/guides/usage.md#string-backed-output-buffers).
- Construct input views with the actual message length, such as
  `make_constant_full_stream(output.begin(), output.current_offset())`, never the
  spare allocation capacity. Keep input storage alive and independent of output
  destinations or any mutation that could invalidate it.
- For explicit limits and whole-message validation, construct the input protocol
  with `(input, decode_limits)`, call `decoder.serialize_in(destination)`, and then
  `decoder.finish()`. Convenience object input calls omit the final exact-message
  check for contiguous buffers; byte streams always receive that check.
  Share decoder sessions by reference; they are noncopyable.
- Choose limits from the application's message sizes and workload. Storage/work
  accounting is cumulative per decoder and is not an exact process-memory limit.
- Strings/vectors/maps replace contents. Missing keyed fields retain destination
  values; duplicate fields apply in order and maps keep the last complete duplicate
  entry. Decode failure can leave partial updates and consumed input.
- Reuse an owning destination across messages when useful: native C++ JSON and all
  three binary key modes recycle eligible nested buffers and map nodes automatically.
  Regenerate owning headers for typed storage donation inside generated collection
  elements and parents; no schema option or explicit donor argument is needed.
  Incoming elements still start from fresh schema defaults, including missing
  keyed fields. Shorter/empty collections discard removed elements; custom or
  throwing-assignment types retain fresh decoding. Reused storage remains subject
  to logical resource accounting and may increase temporary retained memory.
  Java and Protobuf codecs keep their existing replacement paths. Consult
  [destination reuse](../../../docs/guides/usage.md#reuse-destination-storage) for scope
  and limitations; see the [verification record](../../../docs/verification/verification-2026-09-17.md)
  for generation and focused test results. Performance measurements remain outstanding.
- When the application requires atomic replacement, decode/finish a temporary
  object before committing it. Keep structured parse error handling at the message
  boundary and leave payload excerpts disabled unless the task needs them.
- Use the optional `deserialize_exact<Value, Protocol>(input[, limits])` free
  function for a fresh value returned only after `finish()` succeeds. It supports
  native and optional Protobuf codecs, concrete buffers, and standard byte sources;
  no schema regeneration is needed. Input can be consumed on failure, a later
  assignment can still throw, and fresh decoding does not reuse the previous
  destination. Bound input to one message: Protobuf binary can merge valid
  concatenated encodings. Keep existing buffer convenience APIs when one-value
  decoding or destination reuse is intended. See [exact decoding](../../../docs/guides/usage.md#decode-one-exact-message-into-a-fresh-value).
- Regenerate owning headers for automatic fixed-width field batching: native
  binary output groups 2 through 16 consecutive scalar fields per reservation;
  positional binary input shares range/budget checks and falls back to scalar
  reads on a failed check or invalid Boolean. Keyed input keeps individual
  dispatch. Fields retain their wire order, IDs/names, endian, and work charges.
  Variable-length fields, enums, unions, parents, and objects bound runs; nested
  serializers group their own fields. JSON/custom protocols without batch hooks,
  Java, and Protobuf retain existing paths. No schema option or SIMD setting is
  required. Failed output reservations leave the current batch unwritten, with
  previous output intact. See [field batching](../../../docs/guides/usage.md#batch-generated-fixed-width-fields)
  for custom-stream policies and tests. See the
  [verification record](../../../docs/verification/verification-2026-09-17.md) for results;
  performance measurements remain outstanding.

- Regenerate owning headers for pre-encoded constant field names in native C++
  JSON and string-key binary output, including fixed-width batches. Schema wire
  spellings and bytes stay unchanged; JSON formatting and dynamic-string validation
  remain active. No schema option is needed. Custom protocols without the static
  `encoded_field_name<Name>()` hook receive ordinary `std::string_view` names.
  Constant arrays add compiler work/read-only data, not per-object storage; no
  measured speedup is claimed. Isolated binary names reserve their length and text
  together, leaving both unwritten on rejection; values may fail separately.
  Input, views, Java, and Protobuf keep their existing paths. See
  [constant field names](../../../docs/guides/usage.md#pre-encode-constant-field-names)
  for details and tests, and the [verification record](../../../docs/verification/verification-2026-09-17.md)
  for results. Benchmarks remain outstanding.

- Use the updated runtime headers for JSON string scan reuse; schema regeneration
  and new options are unnecessary. Validated plain strings copy directly, while
  escaped input/output reuse boundaries to skip rescanning plain prefixes/suffixes.
  Full validation, resource charges, destination reuse, and stream alias/failure
  behavior remain unchanged. Input still must not overlap decoded storage. Shared
  ProtoJSON input and ProtoJSON/TextProto quoted output benefit; separate parsers
  and other codecs retain their existing algorithms. See
  [JSON scan reuse](../../../docs/guides/usage.md#reduce-repeated-json-scans) for remaining
  passes and coverage. Consult the [verification record](../../../docs/verification/verification-2026-09-17.md)
  for unit-test and bounded sanitizer/fuzz results; do not claim measured performance gains.

## Map generated views

Use `View::map(span, limits)` with the exact little-endian `binary_none` message.
Mapping validates the full message and builds inline field offsets. Getters return
scalars, borrowed strings, or nested views. Mutable mappings require writable
spans; read-only mappings never expose setters. Buffer owners must outlive every
view and must not relocate or change the layout while views remain live.

Use setters only for existing scalar/string fields or collection elements with
unchanged encoded sizes. Do not promise insertion, resizing, union switching,
or JSON/keyed/big-endian mapping. Map keys remain read-only; map entries retain
wire order and duplicates. Prefer range-based loops or `begin()`/`end()` for sequential
array/map access: iterators cache entry boundaries, while variable-width indexed
access traverses preceding entries each time. Arrays yield values or nested views;
maps yield key/value pairs. Use iterator `set(value)` for mutable scalar/string array
elements and `set_value(value)` for mutable map values; dereference returns a value,
not a writable scalar reference. Nested object values expose their existing setters.
Copies advance independently and retain their own span/limits, so temporary collection
wrappers are safe while the underlying buffer remains alive and unchanged in layout.
Nested field access still constructs a validated view. Refer to the view guide for
parent accessors, union access, and complete examples.

## Verify and document the result

For cross-language managed work, use the
[managed wire matrix](../../../example/managed/multilanguage/README.md) to qualify
the canonical history, journal-baseline, collaboration and session record codecs.
It runs every selected producer/consumer pair and rejects truncated/trailing input.
Do not confuse passing record tests with native managed feature support: this
matrix does not implement non-C++ undo/redo, durable journals, collaboration engines,
or authentication. Non-C++ managed declarations remain explicitly rejected.
Regenerate Kotlin records with this compiler when a schema contains a property
named `field`; that contextual name is supported without renaming its wire key.

Use the task's permitted validation scope. For an integration, exercise a generated
record through the selected protocol with an exact-size input, destination reuse,
and relevant malformed/limited input. For schema migration, include the relevant
old/new schema behavior. Consult [qualification](../../../qualification/README.md)
for library-wide tests, fuzzing, or benchmarks only when that work is in scope.

Use the dated [four-toolchain verification](../../../docs/verification/verification-toolchains-2026-10-07.md)
for current default C++ build and test results, and the
[earlier verification record](../../../docs/verification/verification-2026-09-17.md) for its
sanitizer, fuzzing, and interoperability checks. Keep each record's source revision,
configuration, and outstanding-check boundaries when describing support; historical
failures and earlier source-only notes do not describe the current tested revision.

For fuzz qualification, enable `SERIALIZER_BUILD_FUZZERS=ON` with a Clang GNU-style
driver providing libFuzzer, ASan, and UBSan. It builds an isolated instrumented
runtime, including SIMD translation units, without changing the normal library or
generator. Build the four fuzz executables and `fuzz_corpus_generator`, then run
`ctest --test-dir <build> -L serializer_fuzz --output-on-failure` to generate and
replay deterministic seeds. Follow [the fuzzing workflow](../../../qualification/README.md#fuzzing)
for control-byte encoding, separate mutation/artifact directories, fixed seeds,
and SIMD ON/OFF runs. Seed replay is not a mutation campaign or a security claim.

Report files changed, how to generate/build/use the result, and what was actually
validated. When editing Serializer itself, keep its README and this skill current
together with affected linked guides. When editing a consumer, update that
application's relevant usage docs without modifying an upstream checkout merely
to restate an unchanged library contract. Keep public examples specific to
Serializer and public sources.

## Integrate Rust, Python, Swift, Kotlin, or C output

- Keep schema compilation in the C++ executable. Select `rust`, `python`, `swift`,
  `kotlin`, or `c`; use corresponding `.rs`, `.py`, `.swift`, `.kt`, or `.h` outputs.
  Select all desired targets in one invocation. `.inc` files are tracked runtime
  source templates embedded by C++, never temporary output or separate compilers.
- Apply [native API and SDK guidance](../../../docs/languages/native_languages.md). These
  are owning codecs for the four original wire protocols, with portable literal
  defaults; C++ views, packing, compression, and Protobuf are separate features.
- Use `Default` in Rust and constructors in Python/Swift/Kotlin. Preserve full
  unsigned widths and Swift `WireString` map-key identity. Python underscores
  express nonpublic fields by convention; C structs cannot enforce access control.
- For strict native Windows Rust builds with `rustc 1.99` and `-D warnings`,
  regenerate collision-prone arithmetic output with compiler/runtime 2.0.2 or
  newer. Private helpers use snake_case collision suffixes; keep authored public
  APIs unchanged. See [Rust qualification](../../../docs/verification/verification-rust-windows-2026-10-15.md).
- Initialize or zero C destinations and buffers; free owned strings, collections,
  and models using generated lifecycle functions. Never shallow-copy owning C
  models. Decode replaces the destination only after exact-message success.
- Use explicit decode limits at input boundaries. Repeated objects merge,
  collections replace, and the last complete duplicate map value wins. Do not
  claim graceful allocation recovery where the target runtime cannot provide it.
- Every language has `basic`, `collections`, `complex`, and `interoperability`
  folders under `example/<language>`. Use [the example runner](../../../example/README.md)
  to generate from shared schemas and validate all producer/consumer pairs.
  TypeScript is a typed JS consumer, not another wire format or runtime.
- Enable `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` only when all selected SDKs are
  available. Missing SDKs must be reported, not silently skipped. CTest compiles
  those consumers and runs the shared boundary suites; the ordinary compiler
  build requires no target SDK. Consult the [dated verification record](../../../docs/verification/verification-native-2026-09-18.md).

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
[declaration guide](../../../docs/guides/command_line.md#typed-command-line-declarations) for complete examples, getter types, ownership,
error behavior, and positional/tail syntax. This source utility is not an
installed runtime header.


## Payload revisions and schema evolution

For file identification, use [magic and format omissions](../../../docs/schema/magic_and_omission.md).
`private magic (99) { 'SRLFILE' };` defines static immutable header bytes, not
mutable payload storage. Native binary prefixes them before the version; JSON
includes the fixed `magic` string. Every included format verifies and discards
the header and rejects missing, wrong, or duplicate magic. Freeze the numeric
magic ID for durable Protobuf contracts. Default IDs use the first free identity.
C++ byte magic is emitted as readable character literals with a comment showing
its escaped contents. Preserve the exact array extent and bytes; do not replace
the brace list with a string literal that adds a NUL terminator.
Use trailing `omit(json, binary_positional)` on magic or any ordinary member of an
owning class to exclude that field from the selected protocol. View modes reject
omissions explicitly. Version discriminators cannot
omit formats. Omitted fields keep schema defaults on fresh decode; keyed input
must reject explicitly supplied excluded fields. Canonical format names are
`json`, `binary_none`, `binary_integer`, `binary_string`, `protobuf`, `protojson`,
and `textproto`; `binary_positional` and `protobuf_binary` are accepted aliases.
C++ also exposes `binary_positional` as an unchanged alias of `binary_none`.
Magic requires an unmanaged owning class. Legacy byte literals require valid UTF-8.
With schema language `1.1.0`, use `private magic uint32 (99) {42};` or a
declared enum value. Supported types are `char`, `bool`, signed/unsigned
integer widths, finite `float`/`double`, and enums; strings, classes, collections,
type parameters and payload-version component types reject. Typed char values
128..255 require `omit(json)`; ProtoJSON/TextProto retain numeric char mappings.
Each declaring file
needs its own `1.1.0` header. Emit and validate the immutable typed constant
using the selected codec's scalar/enum mapping, with no per-object storage.
Native binary prefixes it without an extra magic key, field ID or collection
count: enum values use compact ordinals in positional/integer-key binary and
length-prefixed names in string-key binary. JSON carries the scalar value or
enum name; Protobuf uses the corresponding scalar/enum field mapping. Retain
legacy byte-magic declarations unchanged unless migration is explicitly requested.
Regenerate all relevant language and editor outputs when changing these contracts.

Consult the [versioning verification record](../../../docs/verification/verification-versioning-2026-10-06.md) and [release-policy verification record](../../../docs/verification/verification-release-policies-2026-10-06.md) for completed language, compiler, codec, and editor checks. Release catalogs and nested allow policies are evaluated entirely during schema compilation; generated readers and writers contain only resolved version bounds.

Use [payload versioning](../../../docs/schema/versioning.md) and the [all-language examples](../../../example/README.md#versioning) for `version`, `compatibility`, `created`, `obsolete`, `replaced`, and class-scoped `reserve` syntax. Distinguish the supported schema language header (`1.17.0`, older `1.16.0`/`1.15.0`/`1.14.0`/`1.13.0`/`1.12.0`/`1.11.0`/`1.10.0`/`1.9.0`/`1.8.0`/`1.7.0`/`1.6.0`/`1.5.0`/`1.4.0`/`1.3.0`/`1.2.0`/`1.1.0`/`1.0.0`, or the original `1` alias for `1.0.0`) from a class's payload discriminator. Freeze durable discriminator identities explicitly when the default first-free ID could change.

Declare payload revisions with Serializer's `version` member instead of an ordinary
counter field. The paired `union_variant` examples declare
`public version uint32 (1) { 1 };`, preserve the fixture revision across
all native protocols, and require every
generated language's JSON reader to reject unsupported version 2. Their C++ and
JavaScript/TypeScript examples also exercise version rejection during encoding.

Review [simplicity warnings](../../../docs/schema/schema_reference.md#simplicity-warnings)
when parsing schemas with compiler `1.10.0` or newer. Initialized ordinary fields
whose source/wire name is `magic` suggest the `magic` keyword; supported revision
scalar fields named `version` or `revision` suggest the `version` keyword. Names
are compared case-insensitively. The parser also suggests omitting an explicit
default `version` member name, which preserves its generated API and bytes.
Warnings apply to older supported language headers and never perform a rewrite.
Review the application's wire contract before converting ordinary fields into
metadata; heuristic names cannot establish intent or compatibility.
For deliberate ordinary fields or explicit default names, upgrade the declaring
file to language `1.5.0` and place `ignore(warning magic, version)` before `;`,
using only the applicable rule names. Suppression is member-local and wire-neutral;
empty, duplicate and unknown rules reject. It does not silence schema errors or
release-policy warnings. The CLI reports warnings without failing; library callers
receive them through the synchronous `parser::parse_options::warning` callback.

Keep retained historical definitions and relative positional order intact. Set the object's revision to write an older supported layout. Use `read_policy::compatible` for declared history, `strict` for the current revision, and `flexible` to skip safe JSON extensions within the declared interval. The former `json_read_policy::compatible` spelling is removed; its unknown-field behavior is `read_policy::flexible`. Native generated languages select `ReadPolicy` through `Limits`; C uses `srl_read_policy`.

Choose uint8/16/32/64, finite nonnegative float/double, or version2/3/4 with canonical bounded uint16 components. Dotted revisions are C++ value types and strings in other native APIs. Conversion for `replaced` is explicit application code. Do not infer forward support for unknown revisions or automatic decoding of previously unversioned bytes. Versioned unmanaged owning models support all eleven languages and all four native codecs; managed owning C++ models also support lifecycle metadata in compiler 1.11;
views and Protobuf mappings still reject this feature. Explicit whole-history
conversion and collaboration fencing are required for released-schema upgrades. Verify the consuming application's historical bytes and the relevant language examples.

### Release-date and count policies

Follow [release policies](../../../docs/schema/versioning.md#release-dates-and-compile-time-policies)
for `releases`, `policy`, nested `any` / `all`, `max_age`, `keep_last`, `released_since`,
`expires_on`, and compatibility leaves. These are allow conditions, not deny lists.
Require ordered release identities and dates with a dated current revision. Preserve
retained historical definitions; release entries do not supply missing decoder layouts.

Pin `--version-policy-as-of YYYY-MM-DD` in reproducible generation and compatibility
checks; otherwise the schema compiler captures UTC today once per invocation.
All three CMake helpers accept `VERSION_POLICY_AS_OF` and the flag
`VERSION_POLICY_WARNINGS_AS_ERRORS`. An incremental build does not rerun generation
just because the date changes. Already-built readers retain the compiled floor.

Inside a tree, compatibility is a normal leaf; outside it, compatibility overrides the
final tree. Date/count leaves need a catalog; compatibility-only trees do not. The
runtime accepts the folded version interval rather than a catalog whitelist. The
current version remains readable, with generation warnings for exceeded time leaves;
`--version-policy-warnings-as-errors` promotes these before output publication.

Library callers may pass `parser::parse_options` to `parse_file(path, options)` or
`parse(input, require_header, options)` and supply synchronous diagnostic callbacks.
Use `parser::version_policy_reference_date()` when multiple calls must share a UTC day.
Do not add clocks, dates, release arrays, or policy objects to application codecs.
