# Native scalar schema indexes

Compiler/runtime **2.34.0**, schema language **1.24.0**, adds standalone C++
managed scalar indexes. They require no database engine. This is the initial
executable subset of the [storage/index design](../design/database_storage_requirements.md),
not completion of its database, paging or query-planning contracts.

```text
serializer version 1.24.0;

class book managed stable_ids {
  public index(unordered, unique) uint64 key (1);
  public index(ordered, on = local) string title (2);
  public index bool available (3);
}
class library stable_ids {
  public managed array book books (1);
}
```

`index` defaults to `ordered`, allows duplicates, and inherits the standalone
store's local site. Options are `ordered` or `unordered`, optional `unique`, and
optional `on = local` or `on = inherit`, in any order. Repeated options reject.
Each declaring file requires language 1.24.0 or newer; a newer including file
does not upgrade an older declaration. An ordinary type named `index` remains
valid in older schemas.

Supported keys are stored `bool`, signed/unsigned 8/16/32/64-bit integers,
`string`, and schema enums. Fields must belong to unpacked owning managed
classes. Private fields use the existing generated friend access. Direct and
separate-value storage, both native identity widths, and disabled/linear/tree
history use the same declaration contract.

The compiler rejects floating point, optional and record keys, collections,
references, lifecycle/version fields, format omissions, transient/cache fields,
packed/view storage, and non-C++ output. Named/compound/collection-scoped indexes,
explicit index IDs, other methods/options, data placement, `on = database` and
`on = both` remain unsupported and reject. An index never implies data upload or
creates a database binding.

## Typed equality lookup

```cpp
using book_fields = rohit::managed::fields<book>;

auto view = store.registry();
auto selected = view.find_unique(book_fields::field_key(), std::uint64_t{17});
auto matches = view.find_by(book_fields::field_title(), "Dune");
for (const auto& match : matches) {
  use(match.id(), match.value());
}
bool exists = view.contains_by(book_fields::field_title(), "Dune");
```

`find_by` returns `match_range<T>` of immutable `entity_read<T>` handles; a miss
is an empty range. `find_unique` returns `std::optional<entity_read<T>>`; a miss is
`std::nullopt`. Its descriptor must declare `unique`, checked at compile time.
An ordinary field or a currently singular nonunique result cannot supply this
contract. `contains_by` returns a Boolean and creates no match range.

Each registry builds a selected access path on first use and reuses it for later
queries on that same registry. Ordered declarations use an ordered map; unordered
declarations use a hash map. Equal-key results follow ascending persistent IDs.
Integer, Boolean and enum comparison is exact; strings use the native standard
string's byte-sequence comparison without case folding or normalization. Hash
traversal order is unspecified, and no persistent hash/key byte format is defined.
The older explicit `ordered`/`hashed` ID index APIs remain available.

Ranges and entity handles pin their exact source snapshot even when the registry
was temporary, the store publishes a newer version, or history navigates. A range
also pins its derived index and resident charge. It exposes no mutable payloads.
Lazy construction is synchronized for concurrent readers of one immutable view;
store mutations retain the existing owner-thread rule. Moving/destroying a view
while it is concurrently in use remains the caller's responsibility.

## Constraints and stable metadata

`unique` is an automatic managed-store invariant, active during initial
construction and every existing candidate validation, load/recovery and history
selection boundary. It covers all owned occurrences of the exact generated
entity type in the document, including sibling managed collections. Different
exact types have separate key domains. It never replaces the native entity ID.
Duplicate initial values throw `registry_error` with `duplicate_key`; failed
transaction validation preserves accepted values, wire bytes and history.
No query or manual `declare_unique_key` call is needed to activate the constraint.
Discarding query indexes cannot disable it. Save/load validates retained
historical snapshots too; importing a snapshot that violates a newly declared
constraint fails before publication.

Generated indexed field descriptors retain ordinary descriptor access and add
`index()` metadata: type-local index ID, scalar field ID, method, unique flag,
declared site and comparator version `1`. Field shorthand uses the field ID in
the separate index-ID namespace. Prefer explicit stable field IDs when evolving
indexed schemas. Index metadata adds no payload field and does not change
existing managed schema fingerprints or encoded model ordering. Introducing a
constraint can still reject previously valid data; validate or migrate such data
before adopting that declaration.

## Resource and implementation limits

The registry still eagerly catalogs the complete selected root. Index creation
copies the selected scalar keys and IDs. `registry_options::max_entities` bounds
catalog membership; `max_index_bytes` bounds each explicit legacy index, aggregate
cached declared-index map capacity, and each materialized match range separately.
The store's resident admission ledger retains derived index/range charges until
their last owners release them. Construction estimates standard container
capacities; this is admission accounting, not a hard allocator/process peak cap.
Cache bookkeeping/control blocks are not included in those payload-capacity
estimates. An over-budget build is not cached and does not change source state.

Unique validation uses scratch ordered maps independently of query readiness,
subject to decode work/entity/allocation budgets and the resident ledger. Its
cost scales with the whole document and the number of distinct declared unique
fields. Models without declared unique keys skip this extra traversal entirely.

This release does not provide shared cross-view caches, eviction policies,
ordered typed range/prefix/scan APIs, dynamic numeric/name field queries, joins,
planner selection, persistent accelerators, a bounded streaming cursor, or
incremental object validation/storage. Equality ranges materialize their matches.
Database authority/placement, persistent provisioning and larger-than-RAM claims
require their separate implementation and qualification.

The maintained [schema/runtime tests](../../test/schema_indexes_test.cpp) exercise
the real parser and generated bindings under direct/separate storage and both
identity widths. Executed platform/configuration results belong in the release's
verification record; the presence of tests alone does not qualify another host.
