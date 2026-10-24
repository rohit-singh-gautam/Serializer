# Managed edit dispatch and dependency notifications

Schema language `1.24.0` adds C++ guarded managed edit behavior contracts. The
persisted record remains constructible; abstract behavior does not turn stored
entities into polymorphic C++ objects. Other native managed engines, polymorphic
owning storage and virtual base layouts remain unsupported.

See the [detailed edit adapter contract](edit_dispatch.md) for owned/borrowed
arguments, typed errors and implementation lifetime examples. Native bodies
require `cpp.naming = preserve`, as for other opaque authored C++ behavior.

```text
serializer version 1.24.0;
class counter managed stable_ids {
  public int64 value (1);
  public function virtual int64 add(int64 delta) edit
    cpp { set_value(get_value() + delta); return get_value(); };
}
```

Inside a transaction, `transaction.root().bind_edit_behavior()` returns an adapter
that selects the nearest concrete schema override. Inherited methods use checked
base accessors. A declaration with an unresolved abstract edit contract requires
an application implementation of the generated `counter_edit_behavior<Editor>`
interface, bound with `editor.bind_edit_behavior(implementation)`. Its methods
receive `Editor&` followed by the exact schema arguments. The adapter owns an
editor/channel copy and borrows the implementation; that implementation must
outlive every call. Temporary implementation binding is deleted.

Every call checks the transaction channel before entering implementation code.
An escaped adapter fails after commit or cancellation. An exception from an
implementation poisons the transaction even when the application catches it.
An `expected<T, E>` error value is an ordinary result: the implementation or caller
chooses whether to cancel. Borrowed `const_ref` parameters retain their existing
call lifetime and must not be retained by the implementation. Native borrowed
and fixed-array signatures are available for this dispatched edit profile;
independently generic, static and callable-guarded dispatched edits are rejected.
Older language versions retain their existing managed edit dispatch restriction.

`model_store::set_publication_callback(callback, notification_options)` optionally
prepares durable field changes before acceptance. Set `.field_paths = true` to
receive `changed_fields`, `created_ids`, `deleted_ids` and
`field_paths_precise`. Each change owns an entity ID, stable field/base path and
an inserted/modified/erased kind. `before` and `after` retain immutable snapshots
from the existing publication event. Entity-local IDs require the event's
namespace binding; `namespace_replaced` invalidates dependencies across import.

```cpp
store.set_publication_callback([](const auto& event) {
  // Consume event.changed_fields while retaining event.before/event.after as needed.
}, rohit::managed::notification_options{.field_paths = true});
```

`projection_dependencies` owns a bounded list of `projection_dependency` values
and filters an event with `affected_by(event)`. Entity zero matches every entity;
an empty field path matches every field. Prefix matching in either direction
handles replacement of a whole ordinary child. Managed child payload edits do
not change their parent's membership field. Entity birth/deletion and namespace
replacement invalidate relevant dependencies; runtime updates are independently
selectable. Legacy traits without generated field visitors explicitly fall back
to entity-level invalidation.

Notification options bound field/path counts, output metadata bytes, entity
counts and standalone catalog estimates. Exceeding a preparation limit fails
the transaction before observer invocation and leaves accepted state/history
unchanged. Observer exceptions retain the existing post-publication error
isolation contract. These limits do not impose a hard allocator peak bound on
application copies or codec temporaries. Notifications still compare whole
candidate roots; they do not implement compact entity history or avoid full-root
decode/validation.

The four representation-profile tests in
[managed_edit_dispatch_test.cpp](../../test/managed_edit_dispatch_test.cpp) and
[managed_notifications_test.cpp](../../test/managed_notifications_test.cpp)
exercise these contracts. Registered tests describe qualification scope;
executed platform results are recorded separately.
