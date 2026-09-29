# 4. Edit a managed point

Read [point.serializer](point.serializer), then [main.cpp](main.cpp). This is
a minimal edit using the actual Serializer runtime.

The schema's only application fields are `int32 x` and `int32 y`.
`stable_ids` requires the explicit field numbers (1) and (2). `managed`
enables generated editors and adds a `persistent_id` metadata member. That
object ID is separate from the two field numbers. The new store assigns the
point ID 1. Each subsequent example repeats this tiny schema so it can be read
without following a shared model file.

`model_store<point>` owns the committed point and, by default, linear history.
`point{1, 2}` supplies the starting coordinates; the generated ID initially
defaults to zero before the store assigns it.

`execute_transaction` creates an isolated candidate, invokes the lambda once,
and completes the transaction before returning. `auto& transaction` lets the
compiler name the borrowed edit-context type. `transaction.root()` obtains the
point editor; `auto editor` avoids spelling its generated template type.

The two setters change the candidate. Successful completion publishes both
changes as one action. `outcome.throw_if_failed()` reports recorded failures.
`store.read()` returns a shared pointer to the immutable committed point;
`value->x` reads x through that pointer.

Expected output:

```text
point: (10, 20)
```

## Follow one setter

The generated setter contains an assignment lambda equivalent to:

```cpp
access_.update([&](auto& target) { target.x = value; });
```

There are two different callback levels: the outer transaction lambda in main
receives a transaction edit context; the setter's inner lambda receives the
point to change.

| Code | Job |
| --- | --- |
| [Setter generator](../../../src/cpp_managed_writer.hpp) | Emits the lambda assigning x or y. |
| [editor_access::update](../../../include/rohit/managed_editor.hpp) | Resolves the target from the candidate root and calls the assignment lambda. |
| [edit_channel::update](../../../include/rohit/managed_editor.hpp) | Checks the shared editor lifetime and forwards through its mutate function pointer. |
| [transaction::root](../../../include/rohit/managed.hpp) | Installs the mutate function pointer that calls transaction::update. |
| [transaction::update](../../../include/rohit/managed.hpp) | Checks transaction state, invokes the root callback on the candidate, and handles failure. |

The final recipient is the candidate point. The assignment is in the generated
setter; the runtime supplies the point to it. Everything is synchronous.
The channel has no history-specific callback or background receiver.

The generated editor in example 3 uses this same path. The runtime provides
transaction and lifetime checks around the candidate mutation. Beginning and
committing currently decode/encode the whole root, and history retains whole
snapshots.

Build target: `managed_point_edit`. See [build instructions](../README.md#build-the-point-examples).
Next: [explicit commit](../manual_commit/README.md).
