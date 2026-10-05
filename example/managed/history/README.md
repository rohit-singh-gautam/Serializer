# 7. Undo and redo one action

Read [main.cpp](main.cpp). A single transaction changes both x and y.
The store template defaults to linear snapshot history. Tree history would use
`model_store<point, history_mode::tree>`; the mode cannot change at runtime.
This example sets a limit of 100 retained states,
including the current state. New commits evict the oldest states when needed;
the byte budget can reduce the number retained further. A successful changed
edit after undo discards redo. Failed, canceled, and no-op edits preserve it.

Expected output:

```text
after edit: (10, 20)
after undo: (1, 2)
after redo: (10, 20)
```

`undo()` restores the previous entry, so both coordinates return together.
`redo()` restores the next entry. Linear history uses deque order and a cursor;
it has no revision numbers or `checkout()` API. The outcome reports status and
failure, without a revision ID.

The point keeps the same `persistent_id` through the edit, undo, and redo. A retained `read()` pointer
pins its particular snapshot; it does not change when the store navigates.
That is why the example obtains a fresh read after each operation.

There is no journal or file saving in this example. Undo history is currently
whole-root snapshots held by the store.

Build target: `managed_point_history`. See [build instructions](../README.md#build-the-point-examples).
Next: [cancel an edit](../revert/README.md).
