# 1. Create, read, and update a managed point

Read [point.serializer](point.serializer), then [main.cpp](main.cpp).

The schema declares a managed `point` with just two application members,
`x` and `y`. CMake runs Serializer to generate `point.hpp`; the C++ program
includes that header. No point class or adapter is handwritten.

`point{1, 2}` initializes the generated coordinates.
`rohit::managed::model_store<point>` owns the committed point, assigns its
persistent ID, and creates the initial history revision.

`store.read()` returns an immutable shared pointer to the committed point.
`before->x` and `before->y` read its initial coordinates.

`store.execute_transaction` runs a synchronous edit. `transaction.root()` gives
the generated point editor; `set_x(10)` and `set_y(20)` update the candidate.
Successful completion publishes both changes together. `outcome.throw_if_failed()`
reports any recorded failure before the program reads the result.

A fresh `store.read()` obtains the updated point. The earlier `before` pointer
still refers to the original immutable snapshot.

Expected output:

```text
before update: (1, 2)
after update: (10, 20)
```

The generated class also contains `persistent_id` as runtime metadata.
The schema's field numbers (1) and (2) identify x and y, not object instances.

For how the store and transaction implement this update, read the
[source walkthrough](../../../docs/internals/managed_transactions.md), including
vertical diagrams of the transaction lifecycle and setter callback path.

Build target: `managed_point_plain`. See [build instructions](../README.md#build-the-point-examples).
Next: [a real transaction callback](../callback/README.md).
