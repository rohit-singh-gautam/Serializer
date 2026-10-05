# 5. Commit explicitly

Read [main.cpp](main.cpp). The point and its setters are unchanged; the caller
now controls when `commit()` runs.

`transaction_outcome outcome;` is declared before the inner scope because the
transaction borrows it. The store and outcome must outlive the transaction.
`begin_transaction` creates a candidate. Until commit, `store.read()` still
returns the original point.

Expected output:

```text
before commit: (1, 2)
after commit: (10, 20)
```

`commit()` validates and publishes the candidate, then closes the transaction.
Its later destruction does not commit again. The editor cannot be used after
completion. This example leaves exceptions uncaught so a failed run is visible.

Omitting `commit()` does not cancel: healthy normal scope exit attempts to commit.
The next example shows that behavior. To cancel, call `revert()` as in example 8.

Build target: `managed_point_manual`. See [build instructions](../README.md#build-the-point-examples).
Next: [scope-exit commit](../scope_commit/README.md).
