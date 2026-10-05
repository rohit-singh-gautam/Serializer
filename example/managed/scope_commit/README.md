# 6. Commit at scope exit

Read [main.cpp](main.cpp) alongside example 5. The explicit `commit()` call is
gone. At the closing brace, the editor is destroyed, then the transaction is
destroyed. On healthy normal exit, the transaction destructor attempts to commit.

This uses ordinary C++ lifetime rules, often called RAII. It is not a background
job or a garbage-collection event.

Expected output:

```text
inside scope: (1, 2)
after scope: (10, 20)
```

A destructor cannot report a commit exception by throwing here. It records the
failure in `outcome`; check `outcome.throw_if_failed()` after the closing brace.
If an exception unwinds the transaction scope, the candidate is discarded.
Mutation failures also prevent earlier edits from accidentally committing.

Build target: `managed_point_scoped`. See [build instructions](../README.md#build-the-point-examples).
Next: [undo and redo](../history/README.md).
