# Typed state registries

Compiler/runtime **1.16.0** provides optional generated C++ managed registries.
The registry extends generated C++ managed identities without replacing their
allocator, history, journal, authorization or collaboration contracts. The
existing eager store remains the default; [paged indexing](paged_indexing.md) is
a separate proposal.
The [custom-ID examples](../example/custom_ids/README.md) include application-owned
registries in every language and a C++ managed snapshot/transaction companion.

## Typed identity and snapshot lookup

Include a generated managed header and use its normal `model_store`. Independently
managed occurrences retain their existing document-local `uint32` or `uint64`
`persistent_id`. The registry records the owning document namespace and exact
generated type, so application keys never replace allocator identity.

```cpp
using store_type = rohit::managed::model_store<model::document>;
using entry_fields = rohit::managed::fields<model::entry>;
store_type store{initial};
auto selected = store.registry();
auto entry = selected.find<model::entry>(id);
auto count = selected.member(id, entry_fields::field_count());
```

`registry_view` and `secondary_index` are movable, noncopyable handles. `find`
returns an immutable `entity_read<T>` which pins the complete source root; its
`storage()`, `value()` and `id()` remain valid across later commits, load, recovery,
undo, redo and tree checkout. A catalog created before navigation keeps its old
values. Request `store.registry()` again to index the newly selected snapshot.
`contains`, `size`, `type`, and `visit` inspect membership without granting
permissions. Missing IDs, wrong types, missing members, wrong member types,
duplicate keys and expired registry editors use `registry_error` with a
machine-readable `registry_error_code`.

Catalog construction traverses the generated ownership graph and builds an
in-memory ordered ID table: O(N log N) construction, O(N) space and O(log N)
lookup. No catalog is allocated until requested. Existing store validation and
collaboration identity tables remain separate and retain their original behavior.
This release does not replace those internal tables with customer indexes.

## Generated members and paths

`fields<T>` describes ordinary members of the managed storage representation;
`value_fields<T>` describes ordinary nested values. Each generated descriptor
contains its schema field/base ID, name, mutability, exact value type and generated
access functions. Functions also reach private schema fields. Descriptors use
neither native field offsets nor RTTI names, and no runtime address is persisted.

`selected.member<T, Value>(id, field_id)` checks the exact expected value type.
For nested ordinary records and nonmanaged bases, pass a
`std::span<const std::uint32_t>` containing stable field/base IDs:

```cpp
constexpr std::array<std::uint32_t, 2> rank_path{6, 1};
auto rank = selected.member<model::entry, std::int32_t>(id, rank_path);
```

Paths intentionally stop at managed entity and collection boundaries; select the
next independently identified entity or use the generated collection editor.
Generated persistent identity metadata is excluded from writable descriptors.
Version/transient fields and complete base replacement are readonly through this
API. Read results copy the selected field; use `entity_read<T>` to borrow payload
storage for the lifetime of its owning handle.

## Transactional bindings, imports and retirement

`transaction::registry()` and `transaction_edit::registry()` share the ordinary
edit channel. They do not own or retain a mutable candidate pointer. Every editor
operation resolves the requested ID/type again, so append, erase and vector
relocation cannot silently redirect a saved editor to another occurrence.

```cpp
store.execute_transaction([&](auto& tx) {
  auto registry = tx.registry();
  registry.edit<model::entry>(id).set_count(12);
  registry.bind(id, entry_fields::field_key(), std::string{"new key"});
  registry.bind<model::entry>(id, rank_path, std::int32_t{8});
}).throw_if_failed();
```

Bindings change ordinary durable model state in the same candidate. Rebinding an
application key preserves the internal persistent ID. A lookup/type/lifetime or
binding failure poisons the transaction through its normal failure channel;
catching it inside the callback does not permit a partial commit.

`reserve()` uses the existing flush-before-exposure allocator. `adopt(value,
attach)` allocates a fresh identity; `adopt_reserved(id, value, attach)` accepts
only an unused `registry.reserve()` reservation made by the active transaction. Imported managed
metadata is replaced with new identities, including independently managed
descendants. The trusted host attachment callback must insert the generated
storage into an owning location in the candidate. Ordinary application references
inside imported data are not automatically remapped. `retire<T>(id, remove)`
uses a trusted owning-container removal callback and checks that the identity is
absent afterward. Retired, canceled and rejected allocation numbers remain
consumed; undo restores historical occurrences rather than allocating new IDs.

These callbacks have the existing trusted `tx.update` contract. They cannot
publish independent bindings or change a live immutable reader.

## Scoped keys and secondary indexes

Application keys are ordinary schema fields. A composite generated record can
contain a scope and local key; the descriptor's entity type supplies the typed
key domain. Opt in to compatible generated comparisons for ordered record keys,
or provide explicit comparison/hash/equality functions. Key semantics and scope
are declared by the application, not inferred from field names.
Ordered keys require a strict weak ordering; hash equality must be an equivalence
relation, and equal keys must have equal hashes. These functions must remain
consistent for the lifetime of each snapshot index or declared uniqueness policy.

```cpp
store.declare_unique_key(entry_fields::field_application_id());
auto selected = store.registry();
auto keys = selected.ordered(entry_fields::field_application_id(), true);
auto groups = selected.hashed(entry_fields::field_group());
auto matching_ids = groups.at(4);
```

`ordered` and `hashed` copy keys and stable IDs into immutable snapshot-local
indexes; they also pin the source root. `true` rejects duplicate keys during
index construction. Ordered indexes support `lower_bound` and iteration; hash
indexes accept caller-supplied hash/equality. `at` retains the standard map
missing-key exception. Construct a new catalog/index after load, recovery,
navigation or commit; indexes are rebuilt on request and have no independently
updated durable state. Never resolve IDs from an old index against a different
snapshot without deliberately selecting that newer state.

Use `field_path<Entity, Value>({field_id, nested_id, ...})` as an index or uniqueness
descriptor for nested ordinary fields/bases. It owns the stable path and checks
the exact key type through generated access. Direct descriptors and path
descriptors share the same ordered/hash and uniqueness operations.

`declare_unique_key` installs a model invariant at existing validation boundaries,
including raw callback edits, load, history navigation and journal replay. It
checks the current root and every retained history snapshot before installation,
so registration cannot leave undo/redo or tree checkout unusable. Declare policies
on a fresh store before loading or recovering a document. Policies are runtime
configuration and are not serialized. A uniqueness failure rejects publication;
it does not append a journal commit or advance accepted state.

`collaboration_authority::registry()` pins accepted state without introducing
another mutation path. `declare_unique_key` also applies to authority acceptance,
conditional collaborative undo and authority document restore. Session rights,
host policy, affected-field comparison, locks, exact retry results and durable
acknowledgments continue through the existing authority. Replica proposal
callbacks can use `tx.registry()`; the authority still validates the proposal.

## Memory and provider boundaries

`store.registry(registry_options{max_entities, max_index_bytes})` bounds catalog
admission. The same per-index byte limit applies to secondary indexes. Owned
catalog/index capacities are conservatively charged to the existing store resident
ledger until their last handle is released. The estimate is not an allocator-wide
or process peak-memory limit: secondary construction and uniqueness validation
use temporary host-library containers, and pinning a small entity keeps its whole
root alive. Ordered construction is O(N log N); hash construction has expected
O(N) cost. Uniqueness checks add a traversal and ordered key table only when
explicitly declared. None of these operations make eager models/history lazy.

`external_registry<Key, Value>` delegates external resolution to a runtime
callback returning `std::shared_ptr<const Value>`. Missing keys produce a typed
failure and successful owning handles outlive provider rebinding. Persist only an
ordinary schema key when the binding itself is durable; callback pointers and
external values are not automatically serialized, undoable or transactional.
The provider owns synchronization, authentication, version selection and external
transactions. Additional borrowed-provider APIs would need an explicit lease
contract; a bare pointer is not accepted as an owning lookup result.

Customer-managed lookup and disk-backed paging are feasible future backends, but
delegation must include the requested document/snapshot, exact type, generation,
failure and handle lifetime. Serializer must retain identity uniqueness, allocator
watermarks, candidate validation and authoritative publication. History needs
the selected revision; authorization and collaboration also need affected field
and ownership/ancestry information. A stale cache miss must not grant access or
silently report absence. See the [paged indexing proposal](paged_indexing.md).

## Capability selection

| Capability | Selection |
| --- | --- |
| Generated scalar persistent identities and typed editors | Used as the only managed allocation and mutation authority. |
| Snapshot pins, transactions, history, recovery and collaboration | Used unchanged; derived catalogs select one immutable snapshot or active transaction. |
| Generated record comparisons | Used for application-defined composite ordered keys. |
| Bounded decoding and unchanged codecs | Used for existing load/recovery and imports. |
| Custom identity policy replacing persistent_id | Not applicable: custom-key bindings retain current uint32/uint64 managed identity. |
| Disk paging and lazy model/history storage | Unavailable; separate design and fault qualification are required. |
| Other-language managed registries | Unavailable: native managed engines currently run in C++. |
| Application algorithms and arbitrary callbacks in schemas | Not applicable: application behavior remains host-owned. |

Qualification is recorded with the [implementation checklist](../SerializerTODO.md).
