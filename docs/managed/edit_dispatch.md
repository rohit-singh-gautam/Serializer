# Guarded C++ managed edit dispatch

The unreleased compiler/runtime **2.34.0**, schema language **1.24.0**, adds
explicit edit-behavior adapters for native C++ managed editors. Four generated
Windows identity/storage profiles pass the [focused dispatch replay](../verification/verification-managed-dispatch-notifications-2026-10-24.md).
Broader platform and native-engine contracts remain pending. Ordinary readonly
behavior contracts and older schema headers retain their existing contract.

```text
serializer version 1.24.0;
class counter managed stable_ids {
  public int64 value (1);
  public function virtual int64 set(int64 next) edit
    cpp { set_value(next); return get_value(); };
}
```

```cpp
store.execute_transaction([](auto& transaction) {
  auto editor = transaction.root();
  auto behavior = editor.bind_edit_behavior();
  behavior.set(17);
}).throw_if_failed();
```

The default adapter calls the nearest concrete implementation of each schema
contract. A derived `override` must match inherited return type, parameter types,
ownership, effect and fixed extents. Inherited concrete methods forward through
their checked base editor. An explicitly selected base accessor refers to that
base occurrence; it does not reinterpret the stored entity as a runtime subtype.

An abstract declaration creates a separate implementation interface while its
persisted record remains constructible. For a schema type `counter`, generated
`counter_edit_behavior<Editor>` exposes virtual methods whose first parameter is
`Editor&`. Implement that interface and pass a live implementation to
`editor.bind_edit_behavior(implementation)`. Runtime virtual dispatch selects the
supplied implementation; it receives no mutable payload pointer or reference.
Types with abstract edit contracts require an implementation and have no default
binding overload.

Adapters copy the checked editor/channel and borrow the implementation. The
implementation must outlive every adapter call. Binding a temporary implementation
is deleted. An adapter escaping commit/cancel is inert: invocation rejects the
closed channel before entering application implementation code. Exceptions during
dispatch poison the transaction even if the application catches them; accepted
state/history remain unchanged when that candidate fails.

Parameters retain their declared ownership. By-value inputs transfer owned values
through wrappers; `const_ref` borrows the caller's object for the synchronous call
without copying or moving it. Adapters retain no argument references. A native
implementation must not retain a borrowed argument beyond its documented lifetime.
Results use the declared owning C++ value, including `expected<Result, Error>`.
An unexpected value is an ordinary typed return, not an implicit transaction
cancellation: the implementation/caller decides whether to revert or throw.
No borrowed mutable candidate result is exposed by the adapter.

Dispatched edits are C++ native/external contracts. Independently generic methods,
static methods, callable guards and native nonthrowing edit promises remain
rejected. Borrowed/fixed-array dispatched edit signatures require a managed owner.
Each declaring file must select language 1.24.0; a newer including file does not
upgrade an older managed edit declaration.

Dispatch belongs to an exact generated editor type. This API does not provide
polymorphic entity storage, downcasts, cross-type identity lookup, or a non-C++
managed engine. Existing packed/view/recursive ownership and inheritance
[representation boundaries](representation_contracts.md) remain enforced.
Applications own native implementation effects and implementation-object lifetime;
implementations and vtable addresses are never serialized.
