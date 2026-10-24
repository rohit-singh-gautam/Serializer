# Managed dependency notifications

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

The four `managed_notifications_*_test` profiles pass twenty Windows checks for these contracts.
This evidence does not establish compact entity history or another native managed engine.
