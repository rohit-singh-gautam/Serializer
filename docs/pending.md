# Pending requirements across Serializer

**10 pending requirements: 7 active and 3 deferred database items.**
Database work is only part of the remaining backlog. Completed requirements
are documented in the [closed-task ledger](project/completed_backlog.md) and
[feature catalog](project/completed_requirements.md).

## Remaining work at a glance

| Area | Open IDs | Remaining work | State |
| --- | --- | --- | --- |
| Managed references | MR03 | Token codecs for six other outputs; shared nested/base and mutual named-generic selection. | Active |
| Native managed engines | MG01 | Go, C#, Rust, Swift, Kotlin and C engines; broader capability contracts for the implemented Python, Java and JavaScript/TypeScript profiles. | Active |
| Native migration | MIG03 | Broader journal/history format upgrades and rebaseline beyond the qualified bounded native schema converter. | Active; database profiles depend on DB02 |
| Managed behavior and history | MG09, MG10 | Remaining portable method/lifetime/error contracts, notification policies and normalized nested identity/history records. | Active |
| Reported bugs | QA-B01, QA-B02 | Python XML projection metadata and strict generated-C scalar-rule initialization. | Active |
| Database and paging | PG01, PG02, DB02 | Authoritative page mutation/recovery, lazy values, data/index placement and integrated persistent queries. | Deferred |

The database remainder was deferred at the maintainer's direction on 24 October
2026. Deferral does not mean completion or will-not-fix. Third-party database
engines and sink adapters are outside scope.

Keep only unfinished requirements here. Move completed tasks into the project
documentation with their scope and verification links; partial implementations
leave their parent requirement open. Closed ownership decisions remain in
[application responsibilities](project/application_responsibilities.md#disposed-requirements).

## Managed references and migration

| State | ID | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- |
| [ ] | MR03 | **Remaining:** token codecs for Go, C#, Rust, Swift, Kotlin and C, plus shared nested/base and mutual named-generic selection. [Python/Java/JavaScript/TypeScript token modes](managed/token_codecs.md) are qualified separately from native engines. Language 1.23 direct named generic self binding is complete as [MR03a](project/completed_requirements.md#mr03a). | — |
| [ ] | MIG03 | **Active subsystem — [GitHub #5](https://github.com/rohit-singh-gautam/Serializer/issues/5), native portion.** Define versioned native journal/storage/schema migration and rebaseline separately from managed-save/raw-model conversion. The bounded native engine now qualifies [actual old/new schema conversion](verification/verification-native-schema-migration-2026-10-24.md) using stable class tags; broader native journal/history upgrades remain open. Breaking managed/storage redesign is allowed when required, with appropriate release/format versions and explicit conversion or rejection. Retain a recoverable source generation until publication succeeds; this safety copy does not require permanent legacy-format readers. Third-party sink conversion is excluded. | DB02 for native database profiles |

## Managed state and storage

The implemented language profiles, guarded C++ dispatch, notifications and
compact snapshot blocks cover only part of the contracts below. Keep the
remaining native engines and broader method/history requirements active.

| State | ID | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- |
| [ ] | MG01 | **Active — [GitHub #1](https://github.com/rohit-singh-gautam/Serializer/issues/1). Remaining:** native engines for Go, C#, Rust, Swift, Kotlin and C, plus broader capability contracts. Python, Java and JavaScript/TypeScript have qualified restricted profiles. Every output still requires ownership, immutable reads, transactions, identity, history, journals and collaboration under its native lifetime/error/concurrency rules. Token codecs alone do not supply an engine. [Supported profiles and remaining work](managed/language_bindings.md). | MR03 codecs before native engine exchange |
| [ ] | MG09 | **Remaining:** broader portable owned/borrowed method, lifetime and typed-error contracts. [Guarded C++ virtual/abstract/override edit dispatch](managed/edit_dispatch.md) is implemented. Same-tag/identity variant merging and selective collaboration Undo/Redo are qualified as [MG09a](project/completed_requirements.md#mg09a); those subsets do not close the broader method contract. [Remaining scope](managed/state_enhancements.md#remaining-work). | — |
| [ ] | MG10 | **Remaining:** broader projection/dependency notification policies and normalized nested entity-version/identity history. [Notifications](managed/edit_dispatch_and_notifications.md) and [compact snapshot blocks with retention/latency measurements](verification/verification-compact-history-2026-10-24.md) are implemented; compact blocks do not establish normalized storage or changed-entity-proportional edit cost. [Identity/history](managed/history.md), [data structures](managed/data_structures.md). | — |

## Deferred database work

The [combined storage/index specification](design/database_storage_requirements.md)
is partially implemented. Local scalar indexes, immutable derived disk indexes
and the bounded eager native engine are available. The remaining authoritative
paging, lazy values and placement/query integration below are deferred.

| State | ID | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- |
| [ ] | PG01 | **Database remainder deferred — [GitHub #2](https://github.com/rohit-singh-gautam/Serializer/issues/2).** The [immutable derived uint64 disk index](managed/paged_index.md) implements bounded external construction, page/cache pins, source binding, corruption checks and atomic generation replacement. Dirty authoritative-page mutation/writeback and incremental database recovery remain deferred until the native database page authority is implemented; [Paged indexing](design/paged_indexing.md). | DB02 incremental page authority |
| [ ] | PG02 | **Database-dependent work deferred — [GitHub #3](https://github.com/rohit-singh-gautam/Serializer/issues/3).** Add lazy value/storage loading and proposed paged capability selectors after authoritative database page lifetime, recovery and I/O failure contracts are implemented and measured. Current native snapshots pin complete eager roots, and derived index traversal does not supply lazy model storage; [Paged indexing](design/paged_indexing.md). | PG01; DB02 incremental page authority |
| [ ] | DB02 | **Remaining database work deferred — [GitHub #6](https://github.com/rohit-singh-gautam/Serializer/issues/6).** The [bounded eager native engine](managed/native_database.md), prepared durable acceptance, exact retries/recovery, immutable typed snapshots, native generation migration and standalone scalar/derived disk indexes are implemented within their recorded profiles. Complete the [combined storage/index contract](design/database_storage_requirements.md), including declared data/index placement, persistent database query integration and incremental authoritative page storage. No third-party database engine/sink dependency; standalone indexing remains independently usable. | CPP01 complete; PG01/PG02 for the incremental backend; MIG03 for broader migrations |

## Qualification and distribution

These two reported bugs are independent of database work and each other.
Their Python and generated-C repairs can be implemented in parallel, with
separate build/test outputs. Original qualification results remain in the
[completed-task documentation](project/completed_backlog.md#qualification-and-distribution).

| State | ID | Remaining requirement and acceptance gate | Dependencies |
| --- | --- | --- | --- |
| [ ] | QA-B01 | **Python XML projection bug:** `_xml_token_has_namespace(type(model))` cannot inspect `_XmlProjectionAccess` wrapper-class metadata. Restore readonly/scoped projection output while preserving namespace retention, limits and native bytes; require all eight projection tests. [Original four-error reproduction and scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b01-main-namespace-token-projection-metadata). | — |
| [ ] | QA-B02 | **Strict generated-C build bug:** initialize the complete `srl_xml_scalar_rule` in both identity helpers. GCC 15.2 strict C11 rejects omitted namespace/identity fields under `-Wextra -Werror`. Preserve defaults, validation and wire bytes; require actual strict C package/resource/XML replays without warning suppression. [Failure and acceptance scope](verification/verification-qualification-bugs-2026-10-24.md#qa-b02-strict-generated-c-scalar-rule-initialization). | — |
