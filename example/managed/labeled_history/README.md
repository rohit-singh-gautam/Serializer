# 9. Optional history labels

Read [main.cpp](main.cpp). This example generates the same simple managed point
as the other lessons, then enables action descriptions at compile time:

```cpp
using point_store = rohit::managed::model_store<
    point, rohit::managed::history_mode::linear,
    rohit::managed::history_labels::enabled>;
```

It supplies `"Move point"` to `execute_transaction` and uses `undo_label()` and
`redo_label()` to display the available action. The strings returned by these
accessors are copies; they remain valid after further edits. An unavailable action
throws `std::out_of_range`; an unnamed available action has an empty label.

Expected output:

```text
Undo Move point
Redo Move point
point: (10, 20)
```

Labels are disabled by default. The other point lessons and ledger example use
unnamed transactions, with no label string in entries or transactions and no
serialized label field. Enabling labels also enables the named manual form,
`begin_transaction("Move point", outcome)`. Unnamed forms remain available.

The same option works with tree history. Use `redo_label(revision)` to name a
particular redo branch. Labels require enabled history; disabled history has no
label option. A saved history must match the receiving store's label policy.

Build target: `managed_point_labeled_history`. See
[build instructions](../README.md#build-the-point-examples).
