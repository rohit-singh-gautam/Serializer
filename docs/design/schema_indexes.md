# Schema-declared indexes and typed queries

[Documentation index](../README.md) · [Design index](README.md)

Status: **proposal**, 2026-10-23. Reviewed against the current C++
[registry implementation](../../include/rohit/managed_registry.hpp) and
[custom-ID example](../../example/custom_ids/managed_cpp/main.cpp).
The first C++ example below is implemented. Schema index declarations and new
query APIs below are proposed; examples deliberately omit a language-version
header. This document changes no compiler, schema language, payload, or extension
version.

## 1. Current code → proposed code

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

| Concern | Current | Proposed |
| --- | --- | --- |
| Identify searchable field | Generated descriptor passed to manual index construction | Same descriptor passed directly to lookup |
| Unique key | Runtime `declare_unique_key` | Generated managed-store invariant |
| Physical index | New `ordered`/`hashed` handle on request | Automatic snapshot cache, reused within budgets |
| Duplicate matches | Vector of IDs, then manual resolution | Iterable immutable entity results |
| Several predicates | Application logic | Typed query and inspectable plan |
| Non-C++ managed engine | Not implemented | Still not implied by portable schema metadata |

## 2. Declaration rules

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

Initial exact-key implementation: independently managed C++ entities, scalar/enum/string keys,
supported ordinary record keys, optional keys, and tuples of those keys. Reject
`transient`/cache fields, arbitrary native values, floating-point keys, collection
keys, callable results, and paths crossing managed references/collections.
Floating-point support needs a separate NaN/equality/total-order contract.
The specialized stages in section 7 explicitly extend this type set: GIN extracts
collection elements; spatial operator families validate coordinate records.
Their declarations must diagnose until their type/operator/backend gates pass.

Nested ordinary values and nonmanaged bases may supply stable field/base paths.
Concrete generic specializations have separate typed index domains. A base index
does not silently include derived entity types; polymorphic indexes need an
explicit later contract. Declarations on ordinary classes without managed
occurrence identity are rejected in this first scope.

### Named and composite indexes

```text
class book managed stable_ids {
  public index(name = by_key, unordered, unique) book_id key (1);
  public index(name = by_title, ordered) string title (2);
  public index(unordered) uint32 category (3);
  public index(ordered) int64 published_at_unix_ms (4);

  index by_customer_title (ordered) on (key.scope.customer, title);
  index by_category_time (ordered) on (category, published_at_unix_ms);
  index by_title_hash (unordered) on (title);
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

Uniqueness is document-wide for the exact entity type, across all its owned
occurrences. It is not implicitly per array, map, parent, customer, or tenant.
Use a composite key to encode the desired scope:

```text
index by_customer_number (unordered, unique) on (key.scope.customer, key.number);
// (17, 100) and (18, 100) coexist; two books with (17, 100) fail.
```

Per-parent indexes need an explicit owner identity and are deferred. An index
never replaces the generated document-local `persistent_id` or typed references.

## 3. Tell lookup which member to search

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

## 4. Duplicates, absence and iteration

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

Ranges own their source snapshot and index pins, including from temporary views.
Iterators are valid while their range lives; copied entity handles retain their
own snapshot lifetime. No references into mutable hash/vector storage escape.
Use a single-pass range contract initially; multipass iteration and constant-time
counts are not required. Resource/cancellation errors propagate, never become an
empty range. A streaming failure may occur after earlier rows were consumed.

### Optional keys and equality

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

## 5. Several predicates versus a composite index

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

### Which order matters?

```text
index by_customer_title (ordered) on (key.scope.customer, title);

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

### Bounded optimizer, not a database engine in the first release

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

## 6. Automatic lifecycle and transactional correctness

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
| Construction/load/recovery | Activate generated constraints before exposing state; reject duplicate unique keys in current and retained historical states |
| Transaction commit | Validate the complete candidate, including raw callback edits, imports, deletion and key changes, before journal acceptance/publication |
| Swap two unique keys | Validate final candidate; intermediate assignments may temporarily duplicate |
| Undo/redo/tree checkout | Select matching immutable indexes; never reuse latest-state entries for old state |
| Key rebind | Update derived key association; preserve entity identity and allocation watermark |
| Failed transaction | Publish neither candidate nor its index; existing readers remain valid |
| Optional index build fails | Fail the query or use an explicitly permitted scan; do not report absence |
| Index update fails after durable acceptance | Mark accelerator unavailable; preserve successful commit and never rerun the editing callback |

The generated invariant replaces manual `declare_unique_key` only for declared
keys. Existing runtime-only constraints and APIs continue to work. Deduplicate
equivalent schema/runtime policies; reject conflicting equality semantics.

First backend may rebuild immutable structures lazily per snapshot. Incremental
maintenance is an optimization requiring reliable complete change information;
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

Indexes start memory-only. Persistence is an optional rebuildable cache with exact
source/descriptor bindings, not another authoritative model or journal. Follow
the separate [paged-index proposal](paged_indexing.md) for disk pages, corruption,
recovery and budgets; missing or stale persisted entries must not prove absence.
Use the [cache-field design and contract](cache_attributes.md) as the optional
persistence integration boundary; qualify the available provider APIs separately.
Cache support does not establish these index methods, a paged payload engine, or
database adapters. Persist method/operator/extractor versions and exact source
bindings with optional index artifacts.

## 7. Generic names, precise methods and richer data

### 7.1 Naming recommendation

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
index by_time (timeseries) on (recorded_at_unix_ms);
// Profile-only: use B-tree by default. It does not promise result ordering.

index by_time_blocks (block_summary, method = brin, summary = minmax)
    on (recorded_at_unix_ms);
// Summary request: requires storage blocks and exact candidate rechecking.

index by_shape (spatial, method = gist, operators = box2_cartesian_mm_v1)
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

### 7.2 Families and their capabilities

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

### 7.3 Types and operators are separate from tree names

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

### 7.4 Bitmap: stored index versus temporary execution bitmap

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

### 7.5 BRIN and the time-series profile

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

### 7.6 GiST and intervals

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

### 7.7 GIN: collections, text and structured paths

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

### 7.8 Spatial/R-tree: boxes, shapes and distance

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

### 7.9 Analysis and recommended scope

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
bytes and unavailable capabilities. This extends section 5's plan contract;
counts remain estimates until execution.

### 7.10 Date/time conventions and remaining special cases

```text
public index(ordered) int64 published_at_unix_ms (4);
index by_category_time (ordered) on (category, published_at_unix_ms);
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

## 8. Compatibility, delivery and acceptance

| Area | Required decision/gate |
| --- | --- |
| Language | New minor language release when grammar ships; reject the feature under older declared versions, including includes; preserve old schemas |
| Compiler/runtime | New minor release when implemented; preserve existing public lookup/index APIs |
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

Delivery sequence: generated declarations and constraint activation → direct
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

## Design references

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

Verification for this document: source/API review and local Markdown link checks
only. Proposed schemas/APIs are not executable tests; no compiler/runtime build or
performance result is claimed.
