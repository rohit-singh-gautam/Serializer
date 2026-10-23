# Native database storage and standalone indexing

Status: existing native registry guidance plus proposed native database integration.
Serializer currently provides generated codecs and C++ managed state; the new
schema placement, query and lazy database APIs remain unimplemented. Native
support is tracked as **DB02 / [GitHub #6](https://github.com/rohit-singh-gautam/Serializer/issues/6)**
in [the central backlog](../pending.md#managed-state-and-storage).

The database direction is a native structured-document engine over Serializer's
supported state/storage contracts. Third-party database engines, document drivers
and sink adapters are outside scope. Standalone Serializer indexing needs no
database. A cloud file/object provider is a separately qualified storage transport,
not a substitute database engine.

Use the [consolidated specification](../design/database_storage_requirements.md)
for required C++23 qualification, proposed syntax, index families, resource bounds,
publication and native engine acceptance. Compiler/runtime 2.30.1 implements the
C++23 minimum independently of database support. See the
[migration](../../migration.md#c23-minimum-in-2301) and
[qualification record](../verification/verification-cpp23-2026-10-23.md).
The database APIs remain unimplemented.

## Current native workflow

Use ordinary generated models for exchange, and the existing C++ managed store
for identity, editing, history and journals. The
[state registry guide](../managed/state_registry.md) documents explicit typed
ID lookup, ordered/hash index construction and runtime unique-key registration.
The [custom-ID example](../../example/custom_ids/managed_cpp/README.md) shows that
workflow; its application key is optional and does not replace persistent identity.

Current whole-root operations and snapshot indexes do not establish persistent
native database indexes, lazy objects or larger-than-RAM payload/history support.
See [paged indexing](../design/paged_indexing.md) for those separate dependencies.

## Proposed data and index placement

All syntax in this section is illustrative, not accepted schema grammar:

```text
class document managed stable_ids storage(database) {
  public index(ordered, on = local) string title (1);
  public index(unordered, unique, on = database) uint64 catalog_number (2);
  public index(bitmap, on = both) uint32 category (3);
  public index(ordered) int64 recorded_at_unix_ms (4);
}
```

`storage(local)` selects native local document authority; `storage(database)`
requires a native database binding. Owned descendants inherit it. A database root
is not an automatically loaded C++ cache; it needs the qualified snapshot/load API.
Mixed local and database authority uses separate document roots initially.

Omitting `on` means `on = inherit`: local data gets a local index, database-backed
data gets a database index. `local`, `database` and `both` explicitly require those
query-index sites. Placement does not move authoritative values, bypass uniqueness
or grant permission. Local indexes over database data must bind its exact snapshot.
A partial payload cache cannot establish complete query results.

## Binding, query and load sequence

The proposed database path is:

```text
open native engine -> bind typed document -> pin snapshot
  -> query declared index -> stream typed IDs
  -> explicitly load objects/pages under leases
  -> optionally materialize an owning value within explicit limits
```

Ordinary generated values retain normal member access without hidden I/O.
Database entity handles, loaded page/object leases and complete owning copies
are distinct representations. Queries may fetch fields for residual checks or
read policy; an ID projection is not a promise of zero payload I/O.

Required unavailable indexes report not-ready/capability/resource errors.
An explicitly permitted scan remains visible as a scan. Pinned content is
historical data, not proof of current authorization. Mutations use the document's
transaction channel; changing a detached copy never updates accepted state.

Standalone descriptors/indexes can precede a database. A usable database client,
persistent engine and lazy storage need a jointly qualified implementation;
prototype mocks establish no crash/cloud capability. The initial embedded API
requires no server or remote transport.

## Proposed document sink boundary

The former third-party sink direction was retired; [issue #4](https://github.com/rohit-singh-gautam/Serializer/issues/4)
is closed as not planned. This historical anchor is retained for existing links.
The replacement is a native state/storage boundary, not a generic driver API.

Prepare a candidate against an exact revision, validate invariants and prepare
publication resources, accept all authoritative facts durably, then publish
without allocation or callbacks. Recover accepted results without replaying
application effects. A second independent journal cannot decide a conflicting
commit. Derived index materialization failure never rolls back an accepted result.

Native journal/schema generation migration remains separately tracked as MIG03
in [issue #5](https://github.com/rohit-singh-gautam/Serializer/issues/5), native
portion only. Preserve the prior recoverable generation through failed/interrupted
migration and keep allocator/reference/epoch domains explicit.

## Verification before claiming native support

- Qualify matched C++23 compiler/runtime/generated/installed targets on the claimed
  Windows/Linux toolchains; preserve schema/public API/wire compatibility.
- Verify native scope/placement, duplicate and unique queries against scans,
  bounded backfill/readiness, and same-snapshot query/load behavior.
- Fault accepted writes, checkpoint selection, migration and restart; preserve
  exact retry outcomes, identity nonreuse and uncertain-outcome fencing.
- Verify loaded value/page leases through eviction, cursor expiry and compaction.
- Measure whole-root eager costs separately from incremental object/history work,
  and require actual paging for larger-than-RAM claims.
- Qualify storage/cloud failure domains and diagnostics independently. A provider
  transport or codec round trip alone establishes no database engine capability.

This guide update runs no database implementation, C++23 build, crash qualification
or performance benchmark. Use the specification and backlog for requirements,
and dated verification records for executed evidence.
