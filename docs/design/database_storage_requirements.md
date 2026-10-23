# Storage, schema indexes and database integration requirements

[Documentation index](../README.md) · [Design index](README.md)

Status: **consolidated proposed requirements**, 2026-10-23. This is the common
contract for standalone Serializer indexing and optional database-backed storage.
Database-backed storage means a native structured-document engine using Serializer's
supported storage boundary. MongoDB and other third-party database engines,
drivers and sink adapters are outside this design; cloud file/object providers
remain separately qualified storage transports rather than database engines.
It consolidates the previous schema-index design and durable-adapter requirements.
All new `storage(...)`, `index(...)`, query and adapter syntax below is illustrative,
not currently accepted grammar or implemented APIs. Existing API examples are
explicitly identified. Compiler/runtime 2.30.1 implements the C++23 build minimum;
see its [qualification record](../verification/verification-cpp23-2026-10-23.md).
This design does not establish database, cloud or benchmark qualification.

**First implementation priority: C++23 is the minimum for Serializer itself**,
including compiler, runtime, generated C++ and installed targets. C++20 support
does not need to be preserved. Compiler/runtime 2.30.1 implements this minimum.
The [Windows/Linux qualification gate](../verification/verification-cpp23-2026-10-23.md)
is complete; new schema-index and database work must retain this baseline.

Use [the central pending backlog](../pending.md#managed-state-and-storage) as the
sole unfinished-work inventory. Completed IDs and evidence live in the
[implemented feature catalog](../project/completed_requirements.md).
Database storage combines active DB02 with deferred PG01, PG02 and MIG03; native
schema indexing can ship independently without a database.
Existing MG02/MG03 activation/authorization remains implemented only within its
documented boundaries. New syntax, placement, adapters and toolchain changes
require qualified implementation and release evidence.

| Need | Contract |
| --- | --- |
| Index an ordinary managed document without a database | [Schema indexes](#schema-indexes-and-typed-queries), native store and `on = local` |
| Decide what data belongs to which store | [Data and index placement](#data-and-index-placement) |
| Request local-only, database-only or both indexes | [Independent index placement](#independent-index-placement) |
| Integrate a durable native database engine or optional external adapter | [Prepare/accept/publish](#prepare-durable-acceptance-and-publication), [storage profiles](#database-storage-and-deployment-profiles) |
| Upgrade compiler/runtime and consumers together | [C++23 minimum](#c23-minimum-and-one-qualified-dependency-source) |
| Establish completion | [Acceptance and incremental delivery](#acceptance-and-incremental-delivery) |

## Baseline and ownership

Serializer already supplies schema-driven types, codecs, stable wire identities,
managed object identity, complete-candidate transactions, optional native indexes,
history, file journals, collaboration and native capability configuration. See
[the C++ runtime](../managed/cpp_runtime.md),
[capabilities](../managed/capabilities.md),
[authorization](../managed/authorization.md),
[file journaling](../managed/journal.md) and
[database integration](../guides/database_integration.md).

Current transaction begin decodes a complete root; commit validates and encodes
the complete candidate. History stores whole-root snapshots. Delta journal frames
can reduce encoded writes without making root validation, recovery or history
lazy. Snapshot indexes are not a qualified persistent database index. The
[paged-indexing proposal](paged_indexing.md#current-serializer-limits) documents
these limits. A bounded record reader or mapped file does not establish bounded
whole-model memory usage.

| Layer | Responsibility |
| --- | --- |
| Serializer | Schema/type/identity semantics, generated metadata and codecs, managed validation, native local indexes/planning, immutable handles, supported publication/recovery hooks, explicit resource accounting and errors. |
| Native database engine/storage adapter | Physical record/page layout, WAL or equivalent recovery authority, atomic publication of data and indexes, query execution, persistent indexes, durable operation outcomes, writer serialization, compaction, backups and storage admission. The supported boundary is shared with Serializer-owned mechanisms under PG01/PG02/DB02; it is not a third-party database sink API. |
| Host application | Authentication and principal binding, security policy, audit meaning, business commands, resource ownership, credential/cryptographic key lifecycle and external effects. |
| Deployment | Cloud provider and SDK selection, volume/object-store guarantees, writer fencing and failover, provider retention/versioning, encryption key service, monitoring and recovery objectives. |

The [application responsibility contract](../project/application_responsibilities.md)
continues to apply. Database page placement, a particular B+ tree or LSM design,
cloud consensus, authorization policy, an identity provider and audit archival
policy must not become mandatory Serializer core dependencies. Missing runtime
hooks remain library work, rather than being relabeled as application policy.

## C++23 minimum and one qualified dependency source

**Priority 1: adopt C++23; do not retain a C++20 compatibility mode.**
Compiler/runtime 2.30.1 defaults to C++23 and rejects explicit older modes.
Compiling one consumer in C++23 does not upgrade the library's supported minimum.
The implementation MUST retain compiler, runtime, managed runtime,
generated C++ and installed targets to a C++23 minimum, accepting newer standard
modes through explicit toolchain qualification. This applies to standalone use as
well as database integration; enabling a database is not what selects the minimum.
Require `cxx_std_23` on public CMake targets, a required standard with extensions
disabled, matching generator defaults and appropriate standard-library feature
checks. Apply the requirement to separately compiled provider/object targets too.
Require explicit standard-library/toolchain checks, synchronized Windows/Linux
wrappers and matching package/exported target requirements. Preserve public API
names, schema identities, generated names and bytes unless a reviewed migration
explicitly changes them. Follow [CMake integration](../guides/cmake_integration.md)
and [wire compatibility](../serialization/wire_format.md).

Removing C++20 support is intentional and approved for this requirement. Do not
delay the migration to preserve C++20 consumers or provide a fallback build.
Document the new compiler/standard-library prerequisites and consumer migration
steps in the release notes. Existing public APIs, schema identities and wire
formats remain unchanged. A toolchain upgrade alone does not require a
schema-language or payload revision bump. New schema syntax follows its own
versioning and editor qualification. Release numbering follows repository policy;
this document does not authorize an automatic major-version increase.

A combined application build must select one Serializer source revision and one
compatible runtime/generator/generated-header set, including dependencies used by
diagnostic providers. Reuse existing exported targets or a supported source-dir
override; detect divergent duplicate runtimes rather than linking incompatible
copies. Record source commits, build options, compiler/standard library and
generator provenance in qualification artifacts. Independent checkouts are not
automatically synchronized.

## Data and index placement

### Terms and independent choices

`local` means the application's native Serializer store or accelerator. It can
use memory or qualified file persistence. `database` means the selected database
engine/adapter, which may itself run embedded on the same machine. These names
describe storage responsibility, not physical distance or a network protocol.

| Choice | Values | Meaning |
| --- | --- | --- |
| Authoritative data | `storage(local)`, `storage(database)`, inherited binding | Where accepted document state is owned and recovered |
| Query index | `on = local`, `database`, `both`, `inherit` | Where the declared access path must be provisioned |
| Method | Generic kind plus optional exact `method` | How that access path supports predicates |
| Persistence/durability | Selected store/provider profile | Whether/how values and derived entries survive restart |
| Uniqueness | `unique` or absent | Constraint on accepted source data, independent of index placement |

Data does not move just because an index changes placement. A local index over
database data is a derived projection of one database snapshot. It does not make
the local process authoritative, prove current authorization, or guarantee reads
without database I/O. `local` never means that required durable data is disposable.
Indexing only the records currently decoded in a partial cache cannot prove that
other matches are absent. Build the complete declared scope, use a qualified
revision-bound engine candidate stream, or report the local index not ready.

### Marking authoritative data

Proposed class/root binding notation:

```text
class draft managed stable_ids {
  public index(ordered) string title (1); // Inherits the local root's index site.
}

class workspace managed stable_ids storage(local) {
  public managed array draft drafts (1);
  public string last_search (2);
}

class book managed stable_ids {
  public string title (1);
}

class library managed stable_ids storage(database) {
  public managed array book books (1); // Durable descendants inherit database backing.
}
```

An unannotated reusable type inherits the selected document binding. Existing
native-store construction remains local by default. An explicit root annotation
is a requirement: binding a database root without a qualified adapter fails;
binding an explicitly local root to a database cannot silently upload it.
Plain owning codec generation/import/export does not itself open a store.

Data placement initially applies to document roots and their complete owned
graphs. A nested annotation must agree with that authority. Splitting one managed
entity's durable fields across local/database owners is unsupported and diagnoses.
Use separate roots for independent local and database data; application UI state
or rebuildable projections may also use existing transient/cache contracts.

```text
class invalid_library managed stable_ids storage(database) {
  public storage(local) string authoritative_title (1); // Reject a split authority.
}
```

A future ownership-edge placement feature must explicitly establish a separate
document boundary, paged/proxy API, reference domain and failure semantics. It
cannot silently turn an ordinary generated vector into a remote collection.
Cross-root references require the explicit external-reference/lifetime contract;
they establish no shared atomic commit. Placement does not replace generated
`persistent_id` or make an application business key the managed identity.

### Independent index placement

```text
class book managed stable_ids storage(database) {
  public index(ordered, on = local) string title (1);
  public index(unordered, unique, on = database) uint64 catalog_number (2);
  public index(bitmap, on = both) uint32 category (3);
}
```

| Data authority | `on = local` | `on = database` | `on = both` | `on = inherit` (default) |
| --- | --- | --- | --- | --- |
| Local Serializer root | Required native index | Reject: no database source binding | Reject: no database source binding | Local index |
| Database root | Required local snapshot accelerator | Required engine index | Both materializations required | Database index |

Thus existing proposed `public index(ordered) string title (1);` works without a
database and provisions an engine index when its document is database-backed.
No annotation means no automatic secondary index. A database-only annotation
must not disappear in a standalone build: explicitly rebind/migrate the schema
or reject the unsupported store, preserving the declaration's meaning.

`on = local` does not provision a named database query index; `on = database`
does not build an equivalent native query accelerator. Backend object directories,
validation machinery and ordinary page caches remain implementation details.
`on = both` gives one logical descriptor two separately accounted physical
instances. It does not create two independently writable copies of model data.
With `method = auto`, each site may select a different qualified method with the
same declared semantics. An explicit method must be supported at every requested
site; failure at either site cannot silently weaken `both` to a single placement.
Local instances belong to a configured store/session binding, not to every
possible database client. Closing a native client releases its local pins/cache;
it neither drops the database index nor changes database uniqueness.

Use distinct declarations to select different methods at each site:

```text
class publication managed stable_ids storage(database) {
  public string title (1);
  index title_here (id = 101, ordered, on = local) on (title);
  index title_in_engine (id = 102, unordered, on = database) on (title);
}
```

`unique` remains authoritative even with `on = local`. A partial local cache
cannot prove database uniqueness or permit duplicates when evicted. The database
must enforce it inside acceptance using a qualified exact constraint mechanism;
that may use internal validation structures without exposing an extra declared
query index. If unavailable, reject activation. `on = both` shares this one
constraint and validates equality/null semantics at both sites.

### Typed scope and stable declaration identity

The default population is the exact managed type within one document. A selected
owned collection may be declared explicitly without making its key extraction
cross collection/entity boundaries:

```text
class library managed stable_ids storage(database) {
  public managed array book books (1);
  public managed array book archive (2);
  index active_titles (id = 201, ordered, on = database, scope = collection)
      over (books) on (title);
}
```

This index covers `books`, not `archive`. Its generated descriptor identifies the
root/owner, stable collection path, entity type and key path. Runtime scope includes
the owning occurrence identity. Per-collection uniqueness is distinct from
type-wide uniqueness. Unsupported container scopes reject; database-wide or
cross-document uniqueness requires a separate transactional design.

`id` is a nonzero uint32 index identity in a namespace separate from payload field
IDs. Require it for explicit named/composite/collection declarations; single-field
shorthand may derive its default identity from the stable owner/type and field
path. Multiple explicit projections need separate IDs. Presentation renames do
not change identity; method, placement, scope, null/equality or extraction changes
select a new definition generation and applicable backfill/validation. Physical
instance identity additionally includes site and database/document namespace.

### Query execution and readiness

```cpp
// Proposed options; index_location is a generated/runtime enum, not a string.
auto here = view.query<model::book>()
    .where(eq(book_fields::field_title(), "Dune"))
    .execute_on(index_location::local)
    .require_index()
    .execute();

auto there = view.query<model::book>()
    .where(eq(book_fields::field_catalog_number(), std::uint64_t{100}))
    .execute_on(index_location::database)
    .require_index()
    .execute();
```

Default execution is `auto`: select eligible declared, coherent indexes at one
site. Field-based `find_by` can infer the eligible site; explicit named descriptors
and site options remove ambiguity. Explicit site selection never silently moves
execution. A database may return a revision-bound candidate stream for bounded
native residual evaluation; cross-site intersections require exact matching
snapshot/scope/semantics and an explicit supported transfer plan. Initially prefer
single-site plans. `explain()` reports requested/actual site, transfer/payload I/O,
method, definition generation, indexed-through revision and scan/recheck work.

Every declared placement is required, not an optional hint. Provision it
automatically, backfill it, maintain it and expose readiness. Lazy first-use
construction is allowed for local accelerators under the native budget; database
declarations require persistent definition/lifecycle state. `on = both` records
each site's readiness; one site being ready does not complete both placements.
An explicitly selected ready site may serve reads while the other builds.
Required indexed reads at an unavailable site return `index_not_ready`,
`unsupported_capability` or a resource/I/O error, never a false miss. Scan fallback
requires `.allow_scan()` and remains visible as a scan in the plan.

```text
declared -> building -> ready -> retiring
                |         |
              failed   rebuilding a new generation
```

Backfill pins a source revision, scans with bounded memory, catches up accepted
writes or serializes activation, closes the build/write race and publishes a
matching generation. Until a newly declared unique policy is validated and
atomically activated, it is not an enforced unique contract: gate exposure under
the new schema, keep the old binding active, or fail activation. Retired pages
remain available to pinned readers. No failed build silently removes a constraint.

## Schema indexes and typed queries

### Binding, querying and loading database-backed data

**Database-backed is a logical ownership contract, not a whole-document cache.**
An ordinary generated C++ value remains an owning value with ordinary member
access. `storage(database)` does not turn that value's getters into hidden I/O
or make every nested container a remote proxy. Database access needs a distinct
qualified document/snapshot/lease API over the same schema descriptors.

```text
schema + stable paths
  -> document binding to native database authority
  -> immutable snapshot token
  -> index query / bounded candidate cursor
  -> typed entity identity handle
  -> explicit object/page load and lease
  -> optional owning copy, explicitly materialized within budgets
```

| Representation | What it owns | Loading and mutation |
| --- | --- | --- |
| Ordinary generated record | Complete ordinary owning representation | No hidden database I/O; modifying a detached copy does not update accepted state |
| Database document handle | Typed binding and database/document identity | Opens/recover-validates the binding; need not decode the whole ownership graph |
| Database snapshot | Exact revision/epoch and source-generation leases | All queries, IDs and loaded values remain bound to it |
| Entity identity handle | Document/type/object identity plus snapshot binding | Resolves through the object directory; it is not a process pointer |
| Loaded entity/page lease | Decoded selected storage and required pins | Typed scalar access; paged collections use an explicit facade, not a silently replaced vector |
| Owning materialization | Detached value/subtree copied from one snapshot | Explicit work/byte/object/depth limits; no authority until a validated transaction imports/edits it |

Illustrative client/storage surface, **not existing API names**:

```cpp
auto database = open_native_database(storage_options);
auto document = database.open_document<model::library>(document_id);
auto snapshot = document.read();

auto ids = snapshot.query<model::book>()
    .where(eq(book_fields::field_title(), "Dune"))
    .execute_on(index_location::database)
    .require_index()
    .select_ids()
    .execute();

for (auto id : ids) {
  auto handle = snapshot.entity<model::book>(id);
  auto loaded = handle.load(); // Explicit decode/I/O and a snapshot/page lease.
  display(loaded.member(book_fields::field_title()));
}
```

Opening binds schema/storage versions and authoritative state; `read()` pins a
coherent revision. Querying seeks the selected index and streams bounded
candidates. Residual predicates, exact rechecks or read policy may fetch fields
even for `select_ids()`; expose that cost rather than promising zero payload I/O.
Direct ID lookup uses the object directory and needs no secondary field index.
Missing identity, wrong type, unavailable storage, expired cursor and index-not-
ready are distinct outcomes. None becomes an empty result through an I/O failure.

```cpp
auto handle = snapshot.entity<model::book>(book_id);
auto owning_book = handle.materialize(materialization_limits);
// This may load the complete owned subtree needed by the ordinary book type.
// It is bounded and explicit; a detached edit is not an accepted database edit.

auto complete_library = snapshot.materialize_root(materialization_limits);
// Eager compatibility path, suitable only when the complete root fits the limits.
```

Loaded objects/pages may be cached as derived immutable data. A cache hit must
match the snapshot, schema/operator binding and access rules; cache miss reloads
from authoritative storage. Evict only unpinned storage. A partial local payload
cache is neither the document nor a complete index. Offline owning copies retain
their historical content but do not prove current authority or support implicit
offline writeback.

Mutations use the bound document's transaction/editor channel, not cached pointers:

```cpp
document.execute_transaction([&](auto& tx) {
  tx.edit<model::book>(book_id).set_title("Updated title");
}).throw_if_failed();
```

The transaction binds the acceptance base, validates constraints/current authority,
prepares durable facts and publishes only after proven acceptance. An old snapshot
remains readable; it cannot authorize overwriting newer accepted state. Neither
materialization nor loading reserves a new identity or performs a Save.

### What can precede the database engine?

```text
common schema/index descriptors and standalone native indexes
  -> supported prepare/accept/snapshot storage boundary
  -> bounded eager native database vertical slice
  -> persistent indexes, readiness and querying
  -> paged object/collection leases and incremental validation
  -> separately scoped remote transport/client, if required
```

The C++23 requirement applies to the qualified implementation set throughout.
Standalone native indexing needs no database client. Database-facing schemas and
API contracts can be specified now, but a usable lazy database client cannot be
claimed from those declarations alone. Eager prototype and lazy target have
different materialization costs and must be reported separately.

An in-memory test backend can qualify descriptor/query semantics and boundary
behavior; it does not establish persistent indexing, crash recovery, a database
format, remote operation or cloud durability. Qualify the database API jointly
with a real native engine implementation. The initial embedded API needs no
network server and no third-party database; transport is a later separate layer.

### Current code → proposed code

**Current:** explicitly register uniqueness, build an index, retrieve IDs, resolve.

```cpp
using book_fields = rohit::managed::fields<model::book>;
store_type store{initial};
store.declare_unique_key(book_fields::field_key());
auto view = store.registry();
auto keys = view.ordered(book_fields::field_key(), true);
auto id = keys.at(key(17, 100)).front();
auto book = view.find<model::book>(id);
```

**Proposed:** declare the policy once in the schema; query entities directly.

```text
class book managed stable_ids {
  public index(unordered, unique) book_id key (1);
  public index(ordered) string title (2);
  public index(unordered) uint32 category (3);
  public index(ordered) int64 published_at_unix_ms (4);
}
```

```cpp
using book_fields = rohit::managed::fields<model::book>;
store_type store{initial}; // Automatically activates declared uniqueness policies.
auto view = store.registry();
auto book = view.find_unique(book_fields::field_key(), key(17, 100));
auto books = view.find_by(book_fields::field_title(), "Dune");
```

The optional `key` in this custom-key example is an application lookup value.
Managed `persistent_id` remains the canonical entity identity. A model needing
only ID lookup requires no additional key field or secondary index. Application-
supplied primary managed identity would need a separate identity/allocator contract.

| Concern | Current | Proposed |
| --- | --- | --- |
| Identify searchable field | Generated descriptor passed to manual index construction | Same descriptor passed directly to lookup |
| Unique key | Runtime `declare_unique_key` | Generated managed-store invariant |
| Physical index | New `ordered`/`hashed` handle on request | Automatic snapshot cache, reused within budgets |
| Duplicate matches | Vector of IDs, then manual resolution | Iterable immutable entity results |
| Several predicates | Application logic | Typed query and inspectable plan |
| Non-C++ managed engine | Not implemented | Still not implied by portable schema metadata |

### Declaration rules

```text
public index string title (2);                   // ordered, duplicates allowed
public index(ordered) string title (2);          // equivalent explicit spelling
public index(unordered) string title (2);        // equality through hashing
public index(ordered, unique) book_id key (1);    // ordered unique key
public index(unordered, unique) book_id key (1);  // hashed unique key
```

These are alternatives for one declaration, not repeated declarations in a class.
These short forms express generic access intent: `ordered` initially selects
B-tree semantics; `unordered` initially selects hashing. Explicit `method` is the
precise spelling for all index families below; compatible generic intent may be
combined with it. Incompatible selections are errors. Existing runtime
`ordered()`/`hashed()` remain unchanged;
the current `ordered()` uses `std::map`, not a claimed B-tree implementation.

```text
public index(method = btree) string title (2);
public index(method = hash, unique) book_id key (1);
public index(method = bitmap) uint32 category (3);
public index(method = brin, summary = minmax) int64 published_at_unix_ms (4);
// Alternative declarations; availability depends on the selected backend.
```

| Option | Default | Contract |
| --- | --- | --- |
| `kind = ordered / unordered / bitmap / block_summary / generalized_tree / inverted / spatial` | `ordered` if nothing selects a kind | Generic access category; positional spelling allowed |
| `method = auto / btree / hash / bitmap / brin / gist / gin / rtree` | `auto` | Resolve generic intent, or require the explicit method; unsupported exact selections diagnose |
| `unique` | Absent | Equal complete keys cannot belong to different entities; initially supported with `btree`/`hash` |
| `name = identifier` | Derived from field name | Select among several declared indexes |
| `id = uint32` | Derived only for single-field shorthand | Stable identity for explicit declarations, separate from payload IDs |
| `on = inherit / local / database / both` | `inherit` | Required materialization sites, independent of data authority |
| `scope = type / collection` | `type` | Exact type population, or explicit owned-collection selector |
| `nulls = exclude` / `nulls = equal` | `exclude` | Exclude absent optional keys, or index absence as one key |
| `operators = identifier` | Inferred only if unambiguous | Versioned type/operator semantics for the method |
| `profile = timeseries` | Absent | Workload guidance; positional `timeseries` is shorthand |

Method-specific options are validated against that method's descriptor, not
accepted as an arbitrary string bag. Options that alter semantics belong to the
descriptor contract; tuning options remain backend configuration. `unique` on a
method without qualified exact uniqueness support is a diagnostic; declare a
separate exact unique index instead. Overlap exclusion is a different constraint.

`index` marks ordinary durable data; index entries are derived data. No payload
field is added for the index. `unique` applies even when an index is evicted or
never queried. It cannot be disabled by a cache setting.

Initial exact-key implementation: independently managed C++ entities,
scalar/enum/string keys,
supported ordinary record keys, optional keys, and tuples of those keys. Reject
`transient`/cache fields, arbitrary native values, floating-point keys, collection
keys, callable results, and paths crossing managed references/collections.
Floating-point support needs a separate NaN/equality/total-order contract.
The specialized stages in [index families](#index-families-and-rich-data) explicitly extend this type set: GIN extracts
collection elements; spatial operator families validate coordinate records.
Their declarations must diagnose until their type/operator/backend gates pass.

Nested ordinary values and nonmanaged bases may supply stable field/base paths.
Concrete generic specializations have separate typed index domains. A base index
does not silently include derived entity types; polymorphic indexes need an
explicit later contract. Declarations on ordinary classes without managed
occurrence identity are rejected in this first scope.

#### Named and composite indexes

```text
class book managed stable_ids {
  public index(name = by_key, id = 401, unordered, unique) book_id key (1);
  public index(name = by_title, id = 402, ordered) string title (2);
  public index(unordered) uint32 category (3);
  public index(ordered) int64 published_at_unix_ms (4);

  index by_customer_title (id = 301, ordered) on (key.scope.customer, title);
  index by_category_time (id = 302, ordered) on (category, published_at_unix_ms);
  index by_title_hash (id = 303, unordered) on (title);
}
```

Field modifiers are shorthand for a single-path class declaration. Class-level
declarations allow multiple access methods on one field and tuples spanning fields.
Duplicate names, unknown paths, conflicting options and equivalent redundant
declarations are diagnostics. Index names occupy a separate generated descriptor
namespace; a tuple's component order is significant.

```cpp
using book_indexes = rohit::managed::indexes<model::book>; // New generated metadata.
auto titles = view.index(book_indexes::by_title());
auto books = titles.equal_range("Dune");
auto scoped = view.find_by(book_indexes::by_customer_title(),
                           std::tuple{std::uint64_t{17}, std::string{"Dune"}});
```

Default type-scoped uniqueness is document-wide for the exact entity type, across
all its owned occurrences. It is not implicitly per array, map, parent, customer,
or tenant.
Use a composite key to encode the desired scope:

```text
index by_customer_number (id = 304, unordered, unique) on (key.scope.customer, key.number);
// (17, 100) and (18, 100) coexist; two books with (17, 100) fail.
```

Collection-scoped indexes require the explicit owner/path descriptor in
[placement and scope](#typed-scope-and-stable-declaration-identity). An index
never replaces the generated document-local `persistent_id` or typed references.

### Tell lookup which member to search

Prefer generated descriptors: type checked, rename aware, no string parsing.

```cpp
auto books = view.find_by(book_fields::field_title(), "Dune");
auto book = view.find_unique(book_fields::field_key(), key(17, 100));
bool exists = view.contains_by(book_fields::field_title(), "Dune");
```

Keep entity identity, field identity and key value distinct:

| Expression | Meaning |
| --- | --- |
| `view.find<model::book>(entity_id)` | Existing: find one entity by persistent ID |
| `view.member<model::book, std::string>(entity_id, 2)` | Existing: read field ID 2 from that entity |
| `view.find_by(book_fields::field_title(), "Dune")` | Proposed: find books whose indexed title equals the value |
| `view.index(book_indexes::by_title_hash())` | Proposed: explicitly choose the title hash index |

Numeric IDs and schema names can support dynamic callers as a secondary API:

```cpp
// Proposed dynamic equivalents; the class and key type are still explicit.
auto a = view.find_by<model::book>(std::uint32_t{2}, std::string{"Dune"});
auto b = view.find_by<model::book>(std::string_view{"title"}, std::string{"Dune"});
```

Names are exact schema member names, not JSON aliases or generated-language names.
ID `2` is meaningful only with its declaring type. Dynamic nested paths use stable
field/base ID sequences. Unknown fields, wrong key types and unindexed fields are
distinct errors. A string name is not a general expression or query language.

A field with several equivalent indexes may select an eligible access method;
use a named index to demand one. `find_unique(field, key)` requires an exact-key
unique declaration, not merely a currently singular result or a unique tuple
containing that field. Static descriptor misuse is a compile-time diagnostic;
dynamic misuse is a structured runtime error.

### Duplicates, absence and iteration

**`contains` returns `bool`. `find_by` returns a range, not one iterator.**

```cpp
auto matches = view.find_by(book_fields::field_title(), "Dune");
if (!matches.empty()) {
  for (const auto& book : matches) {
    display(book.id(), book.value().title);
  }
}

auto first = matches.begin(); // Compare against matches.end().
auto selected = view.find_unique(book_fields::field_key(), key(17, 100));
if (selected) { display(selected->id(), selected->value().title); }
```

`display` is an application callback. Proposed return contracts:

| Operation | Result | No match |
| --- | --- | --- |
| `find_by(descriptor, key)` / `equal_range(key)` | `match_range<T>` of `entity_read<T>` | Empty range |
| `find_unique(descriptor, key)` | `std::optional<entity_read<T>>` | `std::nullopt` |
| `contains_by(descriptor, key)` / index `contains(key)` | `bool`, stops at first match | `false` |
| Ordered index `range(lower, upper)` | `match_range<T>`, half-open `[lower, upper)` | Empty range |
| `match_range::count()` | Number of matching entities; may traverse | `0` |

Equal range bounds produce an empty range; reversed bounds are an error. Range
operations require an ordered-range capability, not merely an index that happens
to use a tree. A hash, bitmap, GIN or spatial index must not masquerade as a scalar
ordered index. Unsupported operations diagnose rather than silently scan.

Do not overload existing `find<T>(persistent_id)` or change its missing-ID exception.
Do not silently return the first duplicate. Unique violations are invariant errors,
not multiple successful results or absence. Hash collisions compare full keys;
they do not imply duplicate keys.

Ordered traversal uses `(key, persistent_id)` order; IDs break ties and do not
participate in uniqueness. Hash traversal has no promised ordering. Query results
without `order_by` have no order guarantee, regardless of the plan chosen.

Ordinary native ranges own source snapshot and index pins, including from
temporary views. Database cursors may use explicit expiring leases; expiration
prevents further cursor operations but never makes outstanding borrowed references
dangle. Hold their page leases until release, or return owning materializations.
Do not retrofit expiry onto ordinary iterators without a distinct checked API.
Native iterators are valid while their range lives; copied entity handles retain their
own snapshot lifetime. No references into mutable hash/vector storage escape.
Use a single-pass range contract initially; multipass iteration and constant-time
counts are not required. Resource/cancellation errors propagate, never become an
empty range. A streaming failure may occur after earlier rows were consumed.

#### Optional keys and equality

```text
public index(ordered, unique, nulls = exclude) optional string isbn (5);
// Many absent ISBNs; present ISBNs must be unique.

public index(ordered, unique, nulls = equal) optional string alternate_code (6);
// At most one absent alternate_code. Absence sorts before present values.
```

For tuples, `exclude` omits an entry if any component is absent; `equal` compares
optional components structurally. Null lookup on an excluding index returns empty;
finding absent entities requires a query scan or an index including absence.

Default string equality is exact UTF-8 content; ordering is unsigned UTF-8 byte
order. No locale, case folding or Unicode normalization is inferred. Composite
records compare recursively in stable field-ID order; tuples use declared component
order. Ordered and hash equality must agree. Compiler-generated compare/hash
support must not require a hand-written per-application hash or comparison profile.
Do not compare raw Serializer payload bytes to obtain numeric ordering.
Hash seeds may vary between builds/processes because hash layout is disposable;
bound collision work for untrusted keys and never persist a native hash as identity.

### Several predicates versus a composite index

```cpp
using namespace rohit::managed::query; // Proposed typed query expressions.
auto query = view.query<model::book>()
    .where(eq(book_fields::field_title(), "Dune") &&
           eq(book_fields::field_category(), std::uint32_t{7}))
    .order_by(asc(book_fields::field_published_at_unix_ms()))
    .limit(20);
auto plan = query.explain();
auto books = query.execute();
```

Predicates are typed expression nodes, not arbitrary callbacks for the planner to
reverse engineer. `eq`, `lt`, `le`, `gt`, `ge`, `between` (`[lower, upper)`),
`is_missing`, `&&`, and `||` form the first query scope. Collection predicates,
joins, grouping and user functions are deferred. Equality literals are converted
with checked, lossless rules; mixed entity types are rejected.

Example data for `title == "Dune" && category == 7`:

```text
title index       -> {11, 18, 23}
category index    -> {18, 23, 40}
AND/intersection  -> {18, 23}
OR/union          -> {11, 18, 23, 40}  // Each entity emitted once.
```

All predicates and ID sets refer to the same document, exact entity type and
snapshot. Membership uses persistent IDs, never array positions or native pointers.
An OR branch without index coverage requires explicit scan permission; dropping
that branch would produce incorrect results. AND may drive one index and evaluate
other conditions directly on those candidate entities.

| Available indexes | Candidate plan | Trade-off |
| --- | --- | --- |
| `title`, `category` | Drive smaller match set; test residual predicate | Often avoids constructing a second ID set |
| `title`, `category` | Intersect matching ID sets | Useful when both substantially narrow results |
| `(category, title)` | One composite lookup | Faster frequent combination; extra update/memory cost |
| `(category, published_at_unix_ms)` | Equality prefix, then time range | Also supplies ascending time within one category |
| No usable index | Explicit `.allow_scan()` | Correct linear fallback; never claim indexed complexity |

Do not automatically create indexes for all fields or all field combinations.
Create one structure per distinct declaration; reuse an equivalent immutable
structure where safe. Separate indexes support ad hoc combinations; composite
indexes target repeated access patterns. Schema structure supplies descriptors,
not evidence that every member merits an index.

#### Which order matters?

```text
index by_customer_title (id = 301, ordered) on (key.scope.customer, title);

customer == 17 AND title == "Dune"   -> exact tuple lookup
customer == 17                       -> contiguous prefix range
title == "Dune"                      -> no efficient leading-prefix seek
customer == 17 AND title >= "D"      -> prefix + ordered title range
```

Declaration order and predicate text order do not set execution priority.
For B-tree composites, component order does set lexicographic access order. First choose
leading equality constraints, then the first range component; later components
can filter entries without necessarily shrinking the scanned interval. Hash
composites require the complete tuple for direct lookup. Skip scans are deferred.
Do not apply the B-tree prefix rule to GIN, GiST or BRIN: their operator families
define multicolumn access. Initial specialized indexes are single-field; there
is no inferred Cartesian product of array elements or spatial dimensions.

#### Bounded optimizer, not a database engine in the first release

1. Match descriptors, operator support, equality/collation and snapshot identity.
   Specialized plans also check extraction rules, coordinate/time units,
   exact-versus-candidate output and compatible storage layout.
2. Prefer a unique exact lookup or useful composite prefix when its estimated
   total cost wins; otherwise compare single-index and bounded intersection plans.
3. Estimate build cost for cold indexes, candidate count, residual reads, sorting,
   limit, and temporary memory. A new index build may cost more than a scan.
4. Use snapshot-local counts/distinct-key statistics already collected during
   building. Do not eagerly build every index just to choose one. Stable descriptor
   order breaks cost ties; correlated predicates need conservative estimates.
5. Recheck lossy candidates against the pinned authoritative entity before
   counting, returning, sorting or applying a final limit. Never complement a
   lossy candidate set to implement NOT: that could discard true results.
6. Stream ordered results when possible. Otherwise sort under a stated memory
   budget; a limit alone must not truncate unsorted candidates before ranking.

```text
explain() example (illustrative, not measured):
  source: selected snapshot
  access: by_category_time, category = 7
  residual: title = "Dune"
  order: published_at_unix_ms ASC, persistent_id ASC
  limit: 20
  index_state: ready
  estimates: candidates=120; output=4; temporary_bytes=...
```

`explain()` must distinguish estimates from observations and avoid implicit cold
builds. Explicit index selection is useful for diagnostics; invalid hints fail
instead of weakening correctness. Planning, sorting, intersections and scans have
work/memory/cancellation budgets. Budget exhaustion returns a resource error or
uses an already permitted correct plan, never incomplete results as success.

`order_by` adds persistent ID as a final tie-breaker. Future continuation tokens
must bind snapshot, descriptor, predicate and last `(sort_key, id)`; an offset
across changing snapshots is not stable pagination. Initial scope needs no token API.

### Automatic lifecycle and transactional correctness

Automatic means **no application registration or manual rebuild calls**. It does
not mean all trees/hashes are built at construction or rebuilt for every read.

```text
schema declaration
  -> generated descriptor + invariant
  -> validate initial/load/recovery/history states
  -> first lookup builds selected snapshot's index
  -> subsequent lookups reuse it
  -> changed publication selects a new snapshot/cache key
  -> old ranges retain old values
```

```cpp
auto old_titles = store.registry().find_by(book_fields::field_title(), "Dune");
// A normal transaction changes a title and commits.
auto new_titles = store.registry().find_by(book_fields::field_title(), "Dune");
// old_titles still reflects its original snapshot; new_titles reflects the commit.
```

| Boundary | Required behavior |
| --- | --- |
| Construction/load/recovery | Activate declared constraints before exposing the new schema; native admission checks retained snapshots, while bounded database profiles require equivalent validated metadata/incremental checks or a staged migration audit |
| Transaction commit | Validate the complete candidate, including raw callback edits, imports, deletion and key changes, before journal acceptance/publication |
| Swap two unique keys | Validate final candidate; intermediate assignments may temporarily duplicate |
| Undo/redo/tree checkout | When enabled by the document profile, select matching indexes; never reuse latest-state entries for old state |
| Key rebind | Update derived key association; preserve entity identity and allocation watermark |
| Failed transaction | Publish neither candidate nor its index; existing readers remain valid |
| Optional index build fails | Fail the query or use an explicitly permitted scan; do not report absence |
| Index update fails after durable acceptance | Mark accelerator unavailable; preserve successful commit and never rerun the editing callback |

The generated invariant replaces manual `declare_unique_key` only for declared
keys. Existing runtime-only constraints and APIs continue to work. Deduplicate
equivalent schema/runtime policies; reject conflicting equality semantics.

The first native backend may rebuild immutable structures lazily per snapshot.
Incremental maintenance is an optimization requiring complete change information;
raw callback edits must still be covered. Persistent structural sharing is later
work, not an assumption that current whole-root transactions become incremental.

Cache identity includes document namespace, exact source root generation, concrete
entity type, stable field/base paths and index semantics/version. Never key reuse
only by entity count, history cursor number, mutable field value or native address.
Reused addresses after destruction cannot identify a snapshot.

Charge copied keys, ID postings, cache metadata, temporary sets, sorting/build
buffers and pins. Bound active builds and share concurrent builds of the same
index. Evict only unpinned derived structures. Published indexes are immutable;
new caching does not make the owner-thread store callable from arbitrary threads.
Preserve the current runtime's admission-estimate limitations; a strict allocator
peak bound requires separate implementation and evidence.

Unique-key validation remains mandatory if no accelerator fits. If validation
cannot complete within its own resource budget, fail before acceptance. Cache
eviction is not permission to accept duplicates. No read grants authorization;
the host's existing policy boundary still governs access and edits. Query domains
must distinguish accepted authority state from local pending collaboration state;
constraints run before authoritative acceptance and normal rejection/rebase rules
apply. Planner statistics and diagnostics must not create a new unauthorized data
exposure channel.

Native local indexes initially start memory-only; optional persistence is a
rebuildable cache. Database-placed indexes require persistent lifecycle and
automatic materialization. Both require exact
source/descriptor bindings and remain derived from the selected authority. Follow
the separate [paged-index proposal](paged_indexing.md) for disk pages, corruption,
recovery and budgets; missing or stale persisted entries must not prove absence.
Use the [cache-field design and contract](cache_attributes.md) as the optional
persistence integration boundary; qualify the available provider APIs separately.
Cache support does not establish these index methods, a paged payload engine, or
database adapters. Persist method/operator/extractor versions and exact source
bindings with optional index artifacts.

### Index families and rich data

#### Naming recommendation

**Provide a friendly name and an explicit algorithm selector.** Generic names
describe access intent; `method` requires a particular implementation family.

```text
public index(ordered) string title (2);                    // Friendly/default.
public index(ordered, method = btree) string title (2);    // Require B-tree.
public index(method = btree) string title (2);             // Precise-only form.

public index(timeseries) int64 recorded_at_unix_ms (3);
public index(timeseries, method = brin) int64 recorded_at_unix_ms (3);
public index(method = brin) int64 recorded_at_unix_ms (3);
// Each group shows alternatives for one field.
```

| Friendly spelling | Expanded intent | Initial method/default |
| --- | --- | --- |
| `ordered` | `kind = ordered` | `btree` |
| `unordered` | `kind = unordered` | `hash` |
| `bitmap` | `kind = bitmap` | `bitmap` |
| `block_summary` | `kind = block_summary` | `brin` |
| `generalized_tree` | `kind = generalized_tree` | `gist`, requires an operator family |
| `inverted` | `kind = inverted` | `gin`, requires an extractor/operator family |
| `spatial` | `kind = spatial` | `rtree`, requires coordinate/operator semantics |
| `timeseries` | `profile = timeseries` | Ordered lookup initially; BRIN only with qualified block storage |

The first seven rows are direct friendly-to-specific mappings in the initial
design. Keeping generic intent separately allows later compatible implementations;
an explicit method always remains a requirement. `timeseries` is deliberately a
profile: it can describe point lookup, time ranges or append-oriented scanning.
It is not an exact synonym for a block-summary algorithm.

```text
index by_time (id = 306, timeseries) on (recorded_at_unix_ms);
// Profile-only: use B-tree by default. It does not promise result ordering.

index by_time_blocks (id = 307, block_summary, method = brin, summary = minmax)
    on (recorded_at_unix_ms);
// Summary request: requires storage blocks and exact candidate rechecking.

index by_shape (id = 308, spatial, method = gist, operators = box2_cartesian_mm_v1)
    on (bounds);
// A future backend may implement spatial intent through a qualified GiST family.
```

Resolution rules: infer the category from an explicit method when no category was
specified; otherwise require compatibility. Without an explicit method, use the
category's qualified default. The time-series profile defaults to B-tree; a backend may
select BRIN only when its declared storage policy and supported query operators
permit it. Never infer BRIN from an integer field name or monotonic ID.
No automatic creation of several indexes follows from one profile. The resolved
method, operator version and any storage dependency appear in `explain()` and
artifact identity. Method changes invalidate physical artifacts, not logical data.

`timeseries, method = brin` fails on a backend without blocks; `timeseries` can
use its qualified B-tree default. There is no silent substitution for an explicit
method. Conflicts such as `ordered, method = gin` or `spatial, method = hash`
diagnose. A general GiST tree is not globally scalar-ordered.

#### Families and their capabilities

The descriptions below inform this proposal; they do not assert implementation
parity with PostgreSQL, Oracle or SQLite.

| Method | Suitable keys/operators | Candidate accuracy | Ordering and uniqueness in this proposal |
| --- | --- | --- | --- |
| `btree` | Scalars/tuples: equality, comparison, prefix/range | Exact | Scalar key order; exact unique supported |
| `hash` | Scalars/tuples: equality | Exact after full-key collision check | No key order; exact unique supported |
| `bitmap` | Repeated scalar values; equality and Boolean combinations | Exact with exact ordinal sets; lossy chunks require recheck | No scalar order; no `unique` initially |
| `brin` | Block-correlated values; range or other supported summary predicates | Lossy block candidates | No result ordering or uniqueness |
| `gist` | Operator-defined ranges, geometry and other domains | Exact or lossy according to operator family | No generic key order; qualified distance traversal possible; no `unique` initially |
| `gin` | Extracted elements/tokens/paths; membership, overlap, containment | Operator-dependent; phrase/path tests may require recheck | No scalar order or uniqueness |
| `rtree` | Multidimensional points/boxes; overlap, containment, qualified distance | Exact box tests or conservative geometry candidates | No scalar order or uniqueness |

BRIN summarizes block ranges, GIN indexes extracted keys, and GiST is an extensible
tree framework, not one fixed comparison algorithm. R-tree is a spatial access
method; it may be implemented independently or through a GiST operator family.
These distinctions follow the primary
[BRIN](https://www.postgresql.org/docs/18/brin.html),
[GIN](https://www.postgresql.org/docs/18/gin.html),
[GiST](https://www.postgresql.org/docs/18/gist.html) and
[R-tree](https://sqlite.org/rtree.html) contracts.

#### Types and operators are separate from tree names

Serializer can represent structured keys using ordinary schema records. Searching
them meaningfully additionally requires a registered, versioned operator family.
The names below are proposed profiles, not new currently supported primitive types.

| Data shape | Required semantics | Candidate methods |
| --- | --- | --- |
| Integer, enum, string, ordinary composite key | Equality, total ordering or hash, null policy | B-tree/hash/bitmap |
| UTC instant / date-only / duration | Epoch, units, calendar/time conversion, checked arithmetic | B-tree; BRIN with block layout |
| Fixed-scale decimal or large integer | Exact numeric comparison/hash; declared scale/precision and overflow | B-tree/hash; bitmap if appropriate |
| Interval/range record | Endpoint type, inclusion, empty/unbounded forms | GiST; qualified one-dimensional R-tree |
| Array/set of scalar elements | Extraction, element equality, duplicate and empty-set semantics | GIN |
| Text document | Versioned tokenizer, normalization, positions and optional ranking | GIN; qualified lossy GiST text family |
| Structured document paths | Typed path/value encoding; absent versus null; array containment semantics | Qualified GIN family |
| Point/box/polygon record | Dimensions, coordinate system, units, finite coordinates and exact predicates | R-tree/GiST |

No arbitrary native object becomes searchable merely by declaring `method = gist`.
No JSON string is a typed path index without an explicit validated extractor.
Primitive type additions need independent schema/wire contracts; adding an index
method alone must not introduce an unversioned decimal or geometry wire format.

Illustrative ordinary records:

```text
class time_span stable_ids {
  public int64 start_unix_ms (1);
  public int64 end_unix_ms (2);
}

class box2_mm stable_ids {
  public int64 min_x (1);
  public int64 min_y (2);
  public int64 max_x (3);
  public int64 max_y (4);
}
```

`interval_half_open_ms_v1` interprets the first record as `[start,end)` with
`start <= end`. `box2_cartesian_mm_v1` interprets the second as a closed 2D box
in one declared Cartesian millimeter frame, requiring each minimum <= maximum.
Record field IDs retain their existing payload meaning; an operator descriptor
binds those paths and validation rules. Names alone never select that meaning.
Floating coordinate profiles must explicitly reject NaN/infinity and define
rounding and conservative bounds; they do not enable ordinary float-key indexing.

#### Bitmap: stored index versus temporary execution bitmap

```text
class observation managed stable_ids {
  public index(bitmap) uint32 category (1);
  public index(bitmap) bool approved (2);
}
```

```text
snapshot ordinal -> persistent ID: [11, 18, 23, 40]
category == 7:                    [ 0,  1,  1,  0]
approved == true:                 [ 1,  0,  1,  1]
AND:                             [ 0,  0,  1,  0] -> entity 23
```

Store a per-key compressed/dense bitset or qualified equivalent over dense
snapshot-local ordinals. Never allocate one bit per raw sparse uint64 ID. Pin the
ordinal-to-ID map with the bitsets; new snapshots cannot reuse ordinal meanings
without an explicit matching generation. Negation requires an exact eligible
entity universe and the declared optional-value semantics.

A stored bitmap index is reusable key-to-membership data. A temporary bitmap may
also combine results from B-tree, GIN or spatial scans; its existence does not
mean those fields have bitmap indexes. PostgreSQL's
[index combination](https://www.postgresql.org/docs/18/indexes-bitmap-scans.html)
illustrates this execution distinction.

Low distinct-key counts and combined analytical filters are good starting
workloads, not a hard cardinality restriction. Measure compression, value skew,
update churn, copied snapshots and per-key overhead. Oracle's
[bitmap guidance](https://docs.oracle.com/en/database/oracle/oracle-database/19/dwhsg/data-warehouse-optimizations-techniques.html)
motivates analytical use; its database locking behavior is not automatically
Serializer's immutable-snapshot behavior. Prefer exact compressed postings before
introducing a lossy representation; lossy candidates always require rechecking.

#### BRIN and the time-series profile

```text
class reading managed stable_ids {
  public index(timeseries, method = brin, summary = minmax)
      int64 recorded_at_unix_ms (1);
  public uint32 sensor_id (2);
}
```

```text
qualified backend blocks     timestamp summary
blocks 0..63                 [1000, 1999]
blocks 64..127               [2000, 2999]
query [1800, 2200)           both ranges are candidates
exact predicate recheck     return only readings with 1800 <= time < 2200
```

The backend must expose stable block/range membership for the pinned source
generation. `blocks_per_range` is backend tuning, not a universal schema wire
constant. Summary construction must cover every eligible entity: unsummarized,
invalid or missing ranges are scanned/rebuilt, never treated as empty. Out-of-order
inserts and corrections must widen/rebuild affected summaries before use; an old
narrow summary cannot be trusted after a change. Reordering/compaction invalidates
layout-bound summaries even when all entity IDs remain unchanged.

Correlation with physical layout determines pruning quality. A B-tree can serve
selective lookup and ordered output; BRIN can cheaply exclude blocks in large
correlated scans. A timestamp does not guarantee that correlation. Current eager
Serializer roots have no qualified paged payload contract, so an explicit BRIN
request remains unavailable until the
[paged storage gate](paged_indexing.md) or an independently qualified database
adapter supplies one. In-memory chunk summaries are a separate declared layout,
not evidence of disk-page skipping or bounded model residency.

#### GiST and intervals

```text
class reservation managed stable_ids {
  public index(method = gist, operators = interval_half_open_ms_v1)
      time_span active_during (1);
}
```

```cpp
// Proposed specialized expressions after the interval operator family ships.
auto overlapping = view.query<model::reservation>()
    .where(overlaps(reservation_fields::field_active_during(), requested_span))
    .execute();
```

GiST needs a qualified operator family with consistent search tests, union/bounds,
split/penalty and optional distance behavior. Candidate tests must be conservative:
no true match may be pruned. Recheck lossy leaves against original values.
For `[a,b)` intervals, touching endpoints do not overlap and empty intervals
overlap nothing. Equality, overlap and containment are different operators.
Preventing overlapping reservations is an exclusion/business constraint, not
`unique`; this proposal adds no automatic overlap prohibition.

Ship bounded built-in families before exposing arbitrary host plugins. A later
plugin ABI needs deterministic semantics, versioning, cancellation/resource bounds
and failure handling; persisting native function addresses is forbidden. Ordinary
host comparator registration does not qualify a GiST implementation.

#### GIN: collections, text and structured paths

```text
class article managed stable_ids {
  public index(inverted, method = gin, operators = string_set_v1)
      array string tags (1);
  public index(method = gin, operators = text_words_v1) string body (2);
}
```

```cpp
auto tagged = view.query<model::article>()
    .where(contains_all(article_fields::field_tags(), {"cpp", "storage"}))
    .execute();
auto either = view.query<model::article>()
    .where(contains_any(article_fields::field_tags(), {"cpp", "database"}))
    .execute();
auto words = view.query<model::article>()
    .where(text_match(article_fields::field_body(), "indexed storage"))
    .execute();
```

```text
"cpp"     -> {11, 18}
"storage" -> {18, 23}
all       -> {18}
any       -> {11, 18, 23}
```

For `string_set_v1`, repeated tags in one entity contribute one posting per tag;
matching returns each entity once. `contains_all({})` matches all present arrays,
including empty arrays; `contains_any({})` matches none. Distinguish absent optional
arrays, empty arrays and any supported null elements through explicit markers.
Set membership does not implement sequence equality, multiplicity or order.
`find_by(tags, "cpp")` must not silently reinterpret whole-field equality as
membership: use the specialized operator.

`text_words_v1` must define token boundaries, case/normalization, language and
stop-word rules, and AND/OR interpretation of its query. The name here reserves
no actual tokenizer behavior. Phrase/ranking queries require positions/scoring
and recheck contracts; full-text lookup is not automatically substring search.
Bound extracted keys, token lengths, per-entity postings, query expansion and
temporary intersections. One edited large document can otherwise dominate memory.
Structured-path extraction additionally needs typed paths, null/missing rules
and nested-array semantics before it is selectable.

#### Spatial/R-tree: boxes, shapes and distance

```text
class region managed stable_ids {
  public index(spatial, method = rtree, operators = box2_cartesian_mm_v1)
      box2_mm bounds (1);
}
```

```cpp
auto touching = view.query<model::region>()
    .where(intersects(region_fields::field_bounds(), viewport_mm))
    .execute();
```

For this closed-box profile, edge/corner contact counts as intersection. Half-open
time interval rules do not transfer to spatial boxes. A point can be a degenerate
box; an inverted box is invalid. Generic record lexicographic ordering cannot
implement multidimensional intersection.

Indexing a polygon's bounding box produces candidates for polygon intersection,
not exact polygon matches: retain the original geometry and recheck it. Coordinate
systems, dimensions, axis order and units must agree; a planar millimeter profile
does not implement latitude/longitude distance. Geographic dateline wrapping and
geodesic predicates need separate qualified families.

Nearest-neighbor search is a further capability, not implied by `method = rtree`
or GiST. A qualified implementation needs lower-bound distance traversal, exact
distance evaluation and `(distance, persistent_id)` tie-breaking. It cannot take
the first `k` bounding-box candidates and claim they are the exact nearest shapes.
Bounds rounding must be conservative; adapter coercion must not shrink boxes,
narrow coordinates or signed-convert uint64 IDs without checked mappings.

#### Analysis and recommended scope

| Design question | Analysis and recommendation |
| --- | --- |
| Friendly versus precise naming | Keep both. Generic intent is stable application vocabulary; explicit method gives reproducibility and diagnostic control. Avoid `timeseries = brin` as a permanent identity. |
| Does structured data make special indexing automatic? | It supplies typed paths and extraction inputs. It does not supply geometry, tokenization, interval semantics or physical correlation. Generate descriptors, then require a qualified operator family. |
| Must every database type become a new primitive? | No. Begin with ordinary records plus versioned operator adapters; add primitives only when portable wire/type semantics justify them. |
| Is GiST a replacement for R-tree? | It can host spatial strategies; explicit R-tree remains useful for native/adapter implementations. Plan by capabilities, not a claim that every backend shares the same tree. |
| Can one generic query work across methods? | Yes, when operator semantics agree. Exact filtering is portable; ordering, ranking, distance and resource guarantees need explicit capabilities. |
| Should all methods arrive together? | Specify the interface now; qualify each backend independently. Do not turn a recognized keyword into a claim of a working engine. |
| What should follow B-tree/hash first? | Exact bitmap and GIN scalar-element membership offer useful Boolean/collection queries without a paged payload engine; measure snapshot update costs before selecting their order. |
| When should BRIN ship? | After stable block membership, source generation binding and conservative summary maintenance exist. Memory-only eager roots do not establish that dependency. |
| When should spatial/GiST ship? | After one small built-in interval or Cartesian box family passes exact reference tests; custom geometry and extensibility come later. |
| How does this help database adapters? | Export method-neutral type/operator descriptors, then report exact adapter capabilities. Database-native names/semantics are not assumed equivalent. |

A backend capability descriptor must declare key/query types, supported operators,
multicolumn support, null handling, uniqueness, ordered/distance traversal,
lossiness/recheck, layout dependency, build/update limits and semantic version.
The planner rejects an unqualified combination rather than approximating it silently.
An adapter may supply filtering while lacking native unique enforcement, the
requested null policy or matching collation. It must either preserve those
constraints through a qualified transactional mechanism or reject the binding;
similar index names do not establish semantic compatibility.
Special methods can improve pruning; they do not provide transactions, lazy models,
database publication, hard memory limits or faster serialization by themselves.

```text
mixed query: category = 7 AND tags contain "cpp" AND shape intersects viewport
  bitmap category -> eligible ordinals
  GIN tags        -> element candidates
  spatial tree    -> bounding candidates
  intersection    -> one snapshot's candidate entity IDs
  exact recheck   -> full predicate on authoritative values
  order + limit   -> only after recheck; sort unless a qualified plan supplies order
```

`explain()` should include `requested_kind`, `profile`, `resolved_method`,
`operator_version`, `layout_generation`, candidate/rechecked counts, posting/set
bytes and unavailable capabilities. This extends the native query planner's plan contract;
counts remain estimates until execution.

#### Date/time conventions and remaining special cases

```text
public index(ordered) int64 published_at_unix_ms (4);
index by_category_time (id = 302, ordered) on (category, published_at_unix_ms);
```

```cpp
auto events = view.index(book_indexes::field_published_at_unix_ms())
    .range(start_unix_ms, end_unix_ms); // [start, end), supplied by the host.
```

`field_<name>()` is the default index descriptor without an explicit name. Times
here are signed UTC milliseconds since the Unix epoch under the documented host
POSIX conversion. No built-in timestamp type is introduced. A local calendar day
needs timezone-resolved UTC boundaries; it need not last 24 hours. Date-only keys
need an explicit calendar/epoch; formatted locale strings are unsuitable keys.
Leap seconds and timezone database versions remain explicit host policies.

| Need | Additional contract |
| --- | --- |
| Normalized/case-insensitive text key | Versioned normalization/collation, equality/hash agreement |
| Prefix / substring query | Explicit prefix bounds / n-gram extraction; neither is implicit full-text behavior |
| Partial/computed index | Pure versioned predicate/extractor, dependency tracking and query-coverage checks |
| Expiration / TTL | Host-driven durable transaction with clock/history/collaboration policy; an index never deletes data itself |
| "Last 24 hours" | Capture `now` once and query a range; no clock-dependent membership in an immutable snapshot |

### Compatibility, delivery and acceptance

| Area | Required decision/gate |
| --- | --- |
| Language | New minor language release when grammar ships; reject the feature under older declared versions, including includes; preserve old schemas |
| Compiler/runtime | Retain the qualified 2.30.1 C++23 minimum; C++20 compatibility is not required. Additive index changes preserve existing schemas/APIs/bytes |
| Wire data | No index contents, descriptor names, native hashes or pointers in ordinary payloads; unchanged field IDs, wire names, payload revisions and entity IDs |
| Semantic fingerprint | Index layout/method is derived metadata; unique/null/equality rules also require an explicit validation-contract identity for collaborating participants |
| Existing documents | Adding uniqueness is an opt-in stricter schema contract; audit all retained histories, resolve duplicates explicitly, then migrate; never silently rewrite values or discard history |
| Other languages | Portable metadata is a goal; initially diagnose unsupported index generation clearly, without claiming native managed engines or silently ignoring uniqueness |
| Editors | Update both extensions together when syntax ships; grammar, diagnostics, completion, navigation to fields/indexes and generated outputs, synchronized patch versions and packages |
| Usage | Update README, integration skill, schema/usage/wire/migration guides and runnable custom-ID example with implemented scope and actual verification |

Recognize `index` contextually at declaration positions; preserve existing schema
identifiers named `index` and previously generated application APIs. Descriptor
names use the selected naming profile; descriptors retain stable field/base paths
independently of that generated spelling.

Native delivery sequence: generated declarations and constraint activation → direct
typed lookup with pinned ranges and snapshot cache → named/composite/range
access → bounded multi-predicate planning. Disk storage and special access methods
are separate qualification stages described above. Bitmap and element-GIN can be
evaluated against the eager model; BRIN requires a qualified block backend;
interval/spatial families require their exact operator tests. This design is recorded as discussion in the
[central backlog](../pending.md#managed-state-and-storage); it assigns no completed
implementation rows or performance claims.

Required acceptance examples:

```text
same title on 3 books         -> range of 3, contains=true
unknown title                -> empty range, contains=false
unknown unique key           -> nullopt
duplicate declared key       -> construction/load/commit/recovery rejects atomically
swap two unique keys         -> final valid candidate commits
old pinned title result      -> unchanged after rename, erase, undo, load, recovery
temporary registry view      -> retained range remains safe
field id 2 vs name "title"    -> same results as typed descriptor
wrong field/key/type          -> diagnostic, never a silent scan or empty result
same key in different docs   -> independent; no cross-document result reuse
same key in two owner arrays -> duplicate if document/type unique policy says so
optional absent tuple key    -> exclude/equal behavior exactly as declared
hash collision              -> full-key comparison preserves correct matches
composite prefix/range       -> same results as a reference full scan
AND / OR / residual filter   -> same entity set as full scan; OR removes duplicates
predicate/declaration order  -> same set; only explicit order_by controls order
time range [a,b)             -> a included, b excluded, duplicates retained
cache eviction/cold build    -> correct rebuild or explicit resource failure
uncommitted candidate        -> cannot leak into selected committed snapshot
failed/uncertain publication -> preserve journal fencing and accepted outcome
damaged optional disk cache  -> rebuild/fallback; never weaken unique validation
friendly/exact method        -> compatible selections agree; incompatible ones diagnose
timeseries on eager root     -> qualified B-tree; explicit BRIN is unavailable
bitmap with sparse uint64 ID -> bounded ordinal mapping, never ID-sized allocation
bitmap from old generation   -> cannot combine with a new generation's ordinal map
BRIN unsummarized range      -> scan/rebuild, never a false-negative skip
BRIN out-of-order correction -> expand/rebuild summaries before lookup
GIN repeated tag             -> one entity result, not duplicate postings
GIN all({}) / any({})        -> present-array universe / empty range
GIN null vs empty collection -> distinct behavior under the declared operator family
intervals [1,2), [2,3)       -> no overlap; an empty interval overlaps nothing
closed spatial boxes touch  -> intersect under the declared box profile
polygon bounds overlap      -> exact shape recheck may reject the candidate
nearest shape limit k       -> exact distances and deterministic ties, not first k boxes
wrong coordinates/operator  -> diagnostic; no silent reprojection or scalar comparison
lossy candidate + limit      -> recheck before counting/limiting final results
method/operator upgrade      -> derived artifact invalidation, unchanged entity identity
```

Measure cold/warm lookup, duplicate-heavy keys, several retained snapshots,
transaction validation, index churn, intersections, sorting and memory separately.
Key-to-ID lookup targets `O(log N + K)` for ordered and expected `O(1 + K)` for hash
after building. Entity resolution is additional: repeatedly using the current
ordered ID catalog costs `O(K log N)`; avoiding that needs snapshot-pinned resolved
locations or another measured optimization. Key size, collision worst cases and
cold registry/index construction remain costs. No benchmark is established here.

## Native and database execution boundary

| Contract | Native Serializer store | Database engine/adapter |
| --- | --- | --- |
| Planning | Local generated descriptors and bounded native planner | Push supported predicates/order/limits to engine; preserve exact operator semantics |
| Index maintenance | Snapshot-derived structures and native invariant validation | Atomic accepted change facts, versioned materializations and readiness gating |
| Unique constraint | Validate complete native candidate | Validate against the serialized acceptance base, including concurrent backfill activation |
| Reader lifetime | Ordinary pinned ranges/handles | Snapshot/page leases; explicitly checked expiry for cursor operations |
| Incremental storage | Eager compatible baseline until separately implemented | Qualified lazy records/collections and dependency-aware validation |
| Durable truth | Selected native journal/document profile | One database acceptance authority; no independent competing Serializer journal |

The schema defines semantics; each backend chooses a compatible physical plan.
An explicit index method constrains that choice and unsupported requests reject.
No database dependency is required for native indexes. The native database engine
and its supported storage adapters use the same capability/acceptance contract.

Constraint checking and required publication resources are prepared before durable
acceptance. The publish step is allocation-free and nonthrowing. Optional index
builds occur before acceptance or outside that critical step; a post-acceptance
materialization failure gates reads or selects an allowed correct fallback, never
rolls back an accepted transaction or reruns an editing callback.

Full validation is the correctness reference. A database may use equivalent
incremental checks or a bounded staged history audit before enabling a new binding.
It cannot claim a bounded lazy profile while decoding all history invisibly.
Security profiles disable navigation without disabling required invariants.

## Prepare, durable acceptance and publication

Expose a supported database boundary with the following semantics. The phase
names describe a proposed contract, not existing C++ symbols or fault hooks.

1. **Prepare:** validate a bounded candidate and all required dependencies against
   an exact base revision; produce immutable change/identity metadata and reserve
   all resources needed for publication. Failure has no externally accepted
   database effect. The prepared state pins its inputs and has explicit ownership,
   cancellation and lifetime rules.
2. **Accept durably:** the database atomically persists the authoritative data
   transition, index mutation facts or sufficient reconstruction references,
   identity high-water marks and operation outcome under its commit protocol.
   Derived index pages may be materialized later; gate queries on a matching
   indexed-through revision or use a correct authoritative fallback until then.
   This does not require flushing physical index pages with every commit. Include
   expected base revision and writer epoch. A flush or conditional publication
   must satisfy the selected durability profile before reporting success. No
   second independent Serializer journal may claim a conflicting authoritative
   commit decision.
3. **Publish:** after proven acceptance, install the prepared immutable state
   without allocation, user callbacks or throwing work. Observers and diagnostics
   run outside this critical publication step. A publication-process crash is
   resolved from durable state, not by rolling back an accepted transition.
4. **Recover or reconcile:** restore the exact accepted state and outcome without
   replaying authentication callbacks, business effects, audit sends or diagnostic
   callbacks. Reapply the required live capability/policy configuration before
   admitting requests. Validate namespace, schema/profile, generation, epoch and
   sequence bindings before publishing recovered state.

The internal `journal_sink` and bounded frame helpers are useful foundations,
not a supported general database prepare/accept API. The
[prepared/confirmed Save contract](../managed/document_origin.md) addresses
external document Save and must not be assumed to implement arbitrary database
transactions. Define an explicit supported adapter surface and ownership model.
Never implement production I/O through test fault hooks or make a diagnostic sink
part of the durable commit decision.

An uncertain write outcome must fence incompatible mutation until the database
resolves it. A durable operation ID binds the document namespace, authority epoch,
caller request digest and original outcome. Exact retry returns that outcome;
ID reuse with a different request rejects. Host-visible idempotency keys may need
longer retention or cross-epoch reconciliation than internal operation IDs.
Specify both horizons and reject expired retries that cannot be proven safe.
Lost acknowledgements, process restart and replacement-writer recovery must not
accept a duplicate logical transition.

## Structured changes and index metadata

Expose a versioned, immutable structured change feed at the prepared transaction
boundary: created/deleted entities, changed fields, collection operations, old/new
values or bounded accessors, object versions, namespace, base/result revision,
schema identity and operation ID. Define reference lifetimes, maximum output
bytes, ordering, coalescing and cancellation. Host adapters need not reverse
engineer full snapshot byte differences. Change feeds are not durable audit logs;
the host defines audit events and persists them in its authoritative transaction.

The [schema-index declarations](#schema-indexes-and-typed-queries) below supply
the proposed common metadata: stable field/base paths, type/optionality,
comparison/extraction semantics, index scope, placement and dependencies.
They cover unique/nonunique keys, ordered compounds, presence, collections and
evolution. Review language version, compatibility and every affected editor/backend
before implementation. Current `state_registry` metadata and application keys do
not automatically provision persistent database indexes.

For database execution, the selected engine/adapter owns physical indexes and
execution plans; the native Serializer backend owns its local equivalents.
Generated descriptors define shared semantics, not a mandatory database engine. A prepared transition must
allow it to validate uniqueness and update data/index roots under one atomic
acceptance boundary. Comparator versions, index definition hashes, source
generation and indexed-through revision must prevent reuse of a stale or
incompatible sidecar. Rebuildable indexes must remain distinct from authoritative
recovery records, as required by [PG01's design](paged_indexing.md).

## Incremental managed storage

PG02 needs immutable per-object records or an equivalent storage abstraction,
per-object/contribution versions, lazy typed handles, pageable collections and
bounded cursors. Reads must pin the exact snapshot and keep object addresses,
borrowed values, references and iterators valid for documented lifetimes. A
bounded cache may evict unpinned pages; a live handle cannot silently observe a
new revision after eviction/reload.

Change validation must retain the current complete-candidate semantics. Build an
explicit dependency set for identities, ownership, required references, affected
invariants and index constraints. Incremental validation must be equivalent to
the full validator for the supported schema subset; if an invariant requires an
unbounded scan, declare the cost or reject the bounded profile. Do not skip a
global invariant merely because its objects are not resident. Verify dependencies
against the acceptance base under the writer's serialization boundary.

Keep whole-root operation as the compatible fallback. Paging only secondary
indexes or applying delta journal encoding does not satisfy this requirement.
Measure decoded bytes, touched objects/pages, allocated and resident bytes,
write amplification and physical storage as total model/history size grows while
the modified set stays fixed. Bound eager fallback explicitly.

## Database storage and deployment profiles

These are optional database-engine requirements. Standalone indexing does not
require a catalog, cloud service, SQL engine or paged payload backend.

| Component | Required contract |
| --- | --- |
| Catalog/document lifecycle | Stable database/document/schema/index bindings; crash-safe creation/deletion; one document is the initial atomic boundary |
| Record/version storage | Immutable typed object versions/tombstones, checked directories and paged collections; movement preserves managed identity |
| Checkpoint | Coherent object/index/schema/allocator/history/session roots and accepted sequence; flush dependencies before selecting the new manifest |
| Accepted change record | Base/result revision, operation/epoch, changed entities/paths, required durable values or reconstructible references; no callback replay |
| Format evolution | Independently version envelopes, pages, index-key encoding, schema language and payloads; unsupported required versions reject before mutation |
| Backup/restore | Pin consistent checkpoint plus required replay; interrupted migration preserves the old usable generation; restore must reconcile authority and exposed IDs |

Define internal durable metadata in maintained Serializer schemas where supported
and generate codecs through the normal pipeline. Freeze exact byte layouts,
widths, endianness, checksums, completion evidence and key encodings before claiming
format compatibility. Source-derived binary payload ordering is not an index
ordering contract. Bound group-commit bytes/delay and acknowledgment semantics;
batching does not create cross-document atomicity.

### Native and cloud durability

Native file storage must qualify data/metadata flush, atomic selection, stable
writer exclusion, segment rotation, checkpoints and directory-entry persistence
on each claimed platform. Recovery distinguishes an incomplete uncommitted tail
from corruption of accepted records. An inspection tool is read-only by default;
explicit repair preserves evidence and reports any loss.

A cloud profile additionally identifies its durable catalog/records/recovery
locations, acknowledgment failure domain, exclusive-writer fencing and replacement
process recovery path. A flushed ephemeral disk or asynchronous backup upload is
not cloud-durable acceptance. State measured RPO/RTO, remote durable lag where
applicable, timeout/quota/unavailable outcomes and unsupported failure domains.
Cloud block-volume deployment may use a qualified native backend; object storage
requires separate immutable upload/conditional selection, integrity, download and
reclamation contracts. Do not assume filesystem append/rename/lock semantics there.
No particular provider, multi-writer or consensus capability is selected here.

### Storage efficiency and workload qualification

Use selected versioned compact codecs; retain only required representations and
share unchanged versions/pages where qualified. Measure codec choice, compression
unit size, random-read/decompression costs and update amplification. Descriptor
reuse must preserve per-record domain/integrity metadata. Required indexes are
provisioned only at declared sites; report local and database cost separately.

Each claimed deployment needs numerical workload/resource/recovery budgets before
qualification: entity and document size, key distribution/selectivity, edit mix,
writer queue/flush latency, reader/lease count, cold/warm behavior and failures.
Report payload, metadata, all index sites, journal, history, tombstones, staging,
diagnostics, retained generations and peak compaction/upload overlap. Larger-than-
RAM claims require actual payload/history paging, bounded recovery and measured
fixed-change-set scaling; an index benchmark alone does not qualify them.

## Security-sensitive profiles and current authority

Support a documented integration profile using native disabled history and
required policy configuration. Exclude ordinary Undo/Redo, tree checkout, branch
merge and unreviewed history conversion for security-sensitive authoritative
state. Configure and validate the profile before initial exposure and after
recovery; persisted content must not grant itself permission to weaken it.
Existing native capability selection is available, but the database adapter's
reopen, restore and migration enforcement still needs qualification.

Revocation, consumed credentials, allocated IDs, settlement facts and durable
idempotency outcomes must survive business reversals and compaction. The host
expresses reversals as explicit commands preserving those facts. Disabled history
alone cannot prevent an administrator from restoring an obsolete backup. Expose
namespace/epoch/high-water metadata so the host can enforce its restore gate,
invalidate credentials or reconcile independently retained evidence. Never claim
that lost post-backup facts can be inferred from old payloads.

Expose exact revision and authority-epoch tokens on immutable snapshots and a
supported way to acquire/validate the current accepted head in the selected
adapter. Distinguish historical content reads from admission against current
authority. The database establishes ordering with concurrent mutations and writer
fencing; the host decides authorization and binds principal/context. An old valid
snapshot must not be represented as proof that a new request sees current policy.
Replay/recovery does not reauthorize previously committed operations, and must
not admit new ones until current authority is established.

## Recovery, retention and erasure

Specify bounded checkpoint/replay batches, cancellation, malformed-record checks,
maximum startup/recovery work and resumable verification for a selected storage
profile. Recovery must not eagerly materialize all retained history while claiming
a lazy memory ceiling. Checkpoint changes must retain the durable operation/ID
facts needed after restart; a derived index can be rebuilt without changing them.

Snapshot pins, reader cursors, prepared candidates, replicas, backups and retained
history need explicit retention leases or an equivalent ownership contract. Report
which roots prevent reclamation, and bound pinned bytes, lease count/age and
compaction workspace. Reject admission or require explicit host action when
configured limits are reached. Never reclaim a page still reachable by a valid
handle. Define a safe cursor-expiry response rather than silently skipping data.

Expose a storage dependency inventory: live and obsolete records, journal and
checkpoint generations, retained revisions, snapshots, diagnostic payloads and
external object references. This lets an adapter report physical copies that may
contain deleted material. Logical deletion, no-history configuration and payload
compression do not erase old blocks, backups or provider object versions.
Include local/client index keys and persisted accelerators in that inventory;
database deletion does not prove those copies were reclaimed. Their access,
retention and erasure rules need the same explicit host/provider ownership.

The host/deployment owns erasure policy, cryptographic key destruction, backups,
cloud retention, credential secrecy and audit archival. Serializer should expose
bounded reclamation and inventory hooks, avoid retaining unneeded secret copies,
and document zeroization limits; it must not promise physical-media erasure or
cryptographic deletion without a qualified owner/key protocol. Host audit/outbox
watermarks and archival receipts must be persisted before pruning protected data.

## Diagnostics and measurement

Provide optional structured hooks or error/event adapters for transaction phases,
recovery, rejected limits, cache/lease pressure and uncertain outcomes. Keep them
bounded, redact payload/credential material and document thread/reentry rules.
The consuming application chooses its diagnostic library. Do not introduce a
mandatory dependency cycle when that library itself uses Serializer.

No hook may change acceptance, throw through nonthrowing publication or block
indefinitely on an exhausted queue. Report dropped events through bounded
counters or another documented policy. Measure disabled, enabled and saturated
diagnostics with real producer-thread/queue limits. Operational diagnostics,
transactional audit and database recovery records have different owners and
durability semantics.

## Acceptance and incremental delivery

These are design acceptance criteria linked to existing work, not a second
completion backlog. A future implementation must record exact versions, executed
commands, measured limits and known exclusions in verification records.

| Contract | Required evidence | Existing work / boundary |
| --- | --- | --- |
| C++23 and dependency coherence | Windows/MSVC and Linux/GCC or Clang generation/build/install/consumer tests, newer mode retained, unsupported toolchains rejected, legacy API/wire fixtures unchanged, one runtime/generator across a combined dependency build. | CPP01 is qualified in 2.30.1; [toolchains and limits](../verification/verification-cpp23-2026-10-23.md). |
| Prepare/accept/publish | Faults before/after every durable boundary, allocation failure during prepare, publication with allocation/callback guards, crash after acceptance before publication, recovery to precisely accepted revision. | DB02; integrate native managed validation rather than bypassing it. |
| Operation identity | Lost acknowledgements and exact retries before/after restart, request mismatch, epoch changes, dedup expiry and uncertain-outcome fencing. | DB02/MIG03 and host idempotency policy. |
| Structured changes/index metadata | Feed equals full before/after semantics, bounded lifetime/bytes, uniqueness races rejected atomically, stale comparator/schema/generation indexes rebuilt or rejected. | PG01/DB02; proposed metadata grammar separately reviewed. |
| Incremental objects/collections | Lazy read/edit/reopen with data larger than cache, evictions under pinned handles, reference/invariant parity with full validation, fixed-change-set scaling and explicit fallback limits. | PG02, depending on PG01 lifetime/recovery design. |
| Current authority and security profile | Historical-reader rejection for current admission, revocation ordering races, all navigation entry points denied, no ID/consumption/settlement resurrection after reopen/compaction/restore. | Existing MG02/MG03 plus DB02 and host restore/authorization policy. |
| Recovery/retention/erasure | Interrupted checkpoints/compaction, corrupt and truncated records, replay/peak-memory bounds, expired/stalled pins and replicas, retained-copy inventory, pruning only after safe watermarks. | PG01/PG02/MIG03; physical erasure/key/provider claims qualified by their owners. |
| Diagnostics/resources | Queue exhaustion, first-event registration, sink failure, shutdown, redaction and disabled/enabled timing; complete storage and peak compaction accounting. | Optional adapter hooks and consuming application's diagnostic integration. |

For the database-backed profile, deliver the required C++23 build and a small
durable adapter vertical slice first:
prepare one bounded transaction, atomically store the host's state/audit/outbox/
idempotency records, accept durably, interrupt, reopen and safely retry. This
establishes correctness using explicit whole-root size limits, not incremental
performance. Then expose structured changes and derived index bindings, implement
paged/lazy objects with validation parity, and measure retention growth and
diagnostic saturation. Qualify the selected database/deployment failure profile
only after those mechanisms exist. Optional editing history and collaboration can
be qualified separately from security-sensitive durable state.
The standalone native-index profile can be delivered independently, with its own
schema/query/constraint tests and without waiting for a database or cloud engine.

## Combined acceptance examples

| Scenario | Required outcome |
| --- | --- |
| Standalone local model, inherited index site | Native index works without loading/linking a database engine |
| Database-backed model, inherited site | Persistent engine index provisions/backfills automatically |
| Local-only index on database values | Native accelerator, exact database snapshot binding, no automatically provisioned named database query index |
| Database-only index | No equivalent local accelerator created; explicit local indexed query rejects |
| Both sites, one build fails | Separate readiness; ready explicit site may serve; combined declaration remains incomplete |
| Local-only unique projection, two clients try the same key | Authoritative database acceptance rejects the duplicate regardless of local cache contents |
| Local and database roots edited together | No implicit distributed commit; explicit host reconciliation or a separately qualified protocol |
| Collection scope versus type scope | Selected collection excludes siblings; type population includes all owned occurrences of that exact type |
| Add index while writes continue | Catch-up/serialized activation closes race; no readiness or uniqueness claim before validation |
| Index/method/site migration interrupted | Old binding remains usable; new artifacts never masquerade as ready/current |
| Data placement changes | Explicit import/migration, namespace/reference and history policy; no silent upload, deletion or identity rewrite |
| Cursor expires with borrowed value alive | Further cursor operations fail explicitly; page remains pinned until borrowed lease ends |
| Accepted commit followed by index I/O failure | Accepted outcome retained; required queries gated; no callback retry or false rollback |
| Restored security-sensitive data | Protected admission stays closed until current authority/epoch/retry/allocator evidence is reconciled |
| C++20 consumer against upgraded public targets | Clear unsupported-standard diagnostic; no silent fallback |
| C++23 and newer qualified consumers | Correct exported requirements, same schemas/APIs/bytes and one matched generator/runtime dependency set |

The mechanisms above support both a standalone native-store profile and a
structured managed-document engine profile. Database-wide indexes/transactions,
arbitrary server transport and distributed consensus remain separate contracts.

## Design references and verification

The following primary sources inform the choices; their implementations are not
Serializer capabilities or required dependencies:

- PostgreSQL [multicolumn indexes](https://www.postgresql.org/docs/18/indexes-multicolumn.html):
  leading equality/range access motivates the composite ordering rules.
- PostgreSQL [combining indexes](https://www.postgresql.org/docs/18/indexes-bitmap-scans.html):
  combining candidate sets costs work and can lose index ordering; one driver can win.
- PostgreSQL [index types](https://www.postgresql.org/docs/18/indexes-types.html):
  ordered/hash, inverted and page-summary methods serve different operators.
- PostgreSQL [partial indexes](https://www.postgresql.org/docs/18/indexes-partial.html):
  restricted membership needs query-predicate coverage, not arbitrary cache filtering.

Original verification for this consolidation: specification/source review, local Markdown
target/anchor checks, example inventory and whitespace checks only. Proposed
declarations are not executable compiler tests. The later
[C++23 upgrade qualification](../verification/verification-cpp23-2026-10-23.md)
is complete; no database adapter, placement engine, database crash test or database
performance result is claimed as completed.
