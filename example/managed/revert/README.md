# 8. Cancel before publication

Read [main.cpp](main.cpp). Both setters run, but `transaction.revert()` closes
the transaction and discards the candidate. Returning from the outer callback
cannot commit that canceled transaction.

Expected output:

```text
after revert: (1, 2)
```

`outcome.status` is `transaction_status::reverted`. Intentional cancellation
is not failure, so `throw_if_failed()` does not throw. The final condition checks
both the status and the unchanged coordinates.

Revert and undo are different: revert cancels an unpublished candidate; undo
navigates away from an already committed revision. No move revision is published
by this example.

Build target: `managed_point_revert`. See [build instructions](../README.md#build-the-point-examples).
