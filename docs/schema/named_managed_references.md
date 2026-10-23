# Named managed generic self references

Compiler/runtime **2.31.0**, schema language **1.23.0**, adds a C++ binding for
self-reference fields copied into a named managed generic instantiation.

```text
serializer version 1.23.0;
class node<T> stable_ids managed {
  public T value (1);
  public optional<managed_ref<node<T>>> next (2);
}
instantiate number_node = node<uint32>;
instantiate other_node = node<uint32>;
```

In the default uint32 profile, `number_node`'s copied field has C++ type
`std::optional<rohit::managed::managed_ref<number_node>>`. A uint64 profile supplies
`std::uint64_t` as the token's second template argument. In `other_node` the token
targets `other_node`. The records remain
different exact types and keep their independent static class identities.
Matching generic arguments do not make their tokens interchangeable.

## Binding and language provenance

The `instantiate` declaration selects the contract. It must be declared in a file
using language 1.23.0 or later. Its generic definition may come from an older
compatible file. An older included `instantiate` retains its earlier contract
even when the entry file uses 1.23.0; a newer entry header does not upgrade it.
Earlier named self-reference generation remains rejected rather than silently
changing previously selected meaning.

Binding compares the generic family and its complete resolved arguments,
including defaulted types and dimensions. Only a token targeting that exact
specialization becomes a named self edge. A token targeting another concrete
specialization or an explicit other class retains its target.

The copied field's token may be direct, optional, or inside supported array/map
value wrappers. Required tokens retain their usual complete-candidate validity
rules; use optional presence for an initially absent edge. Token fields still
need ordinary unpacked owning storage and no schema value default. Existing
transient/omission, map-key and profile restrictions remain in force.

Shared ordinary nested records and inherited base records are not cloned or
retargeted for each named owner. A recursive edge inside such a shared record
still requires explicit concrete target classes. Callable signatures retain their
existing binding rules; this feature binds copied fields. Mutual named-generic
selection, recursive owning storage and non-C++ token codecs remain separate
contracts. The compiler continues to diagnose unsupported combinations.

## Runtime and wire behavior

Create references through the native registry after identities exist:

```cpp
const auto registry = transaction.registry();
transaction.root().nodes().edit(first_id).set_next(
    registry.template reference<number_node>(second_id));
```

The reference retains the existing document/class/object envelope and native
`uint32`/`uint64` object identity. Compact-varint and fixed-width policies keep
their existing meaning. Binary positional, integer-key, string-key and JSON codecs
are unchanged. Registry lookup and complete publication still reject wrong types,
foreign documents and deleted targets. Self cycles do not own their targets.

History, Save/load, journals and independent imports use the existing native
reference traversal. Imports allocate new destination IDs and rewrite self tokens;
they do not reuse source-document authority. No managed engine for another output
language, database storage, new token envelope or payload revision is introduced.

See the [runnable example](../../example/managed/named_references/README.md),
[managed reference contract](../design/managed_entity_references.md),
[generic types](generics.md#managed-specializations), and
[qualification record](../verification/verification-named-managed-references-2026-10-24.md).
