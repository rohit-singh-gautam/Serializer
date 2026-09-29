# 7. Undo and redo one action

Read [main.cpp](main.cpp). A single transaction changes both x and y.
Default store options retain linear snapshot history.

Expected output:

```text
after edit: (10, 20)
after undo: (1, 2)
after redo: (10, 20)
```

`undo()` restores the previous revision, so both coordinates return together.
`outcome.revision` identifies the committed move. `redo(outcome.revision)`
selects that direct child revision.

These are revision IDs, not the point's `persistent_id`. The point keeps the
same object ID through the edit, undo, and redo. A retained `read()` pointer
pins its particular snapshot; it does not change when the store navigates.
That is why the example obtains a fresh read after each operation.

There is no journal or file saving in this example. Undo history is currently
whole-root snapshots held by the store.

Build target: `managed_point_history`. See [build instructions](../README.md#build-the-point-examples).
Next: [cancel an edit](../revert/README.md).
