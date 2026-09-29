# 3. Use the generated point editor

Read [point.serializer](point.serializer), then [main.cpp](main.cpp).
Both the point class and its editor come from Serializer generation.

`transaction.root()` returns the generated editor for the candidate point.
`editor.get_x()` returns a copy of its current x value. Adding one and passing
the result to `editor.set_x(...)` updates the candidate. The y operation is
the same.

There is no handwritten access class, editor, or callback dispatcher here.
The generated getters and setters use the real `editor_access` and
`edit_channel` implementation in
[managed_editor.hpp](../../../include/rohit/managed_editor.hpp).

The transaction starts from (1, 2), so the published point becomes (2, 3).
The editor is only usable while its transaction remains active.

Expected output:

```text
point: (2, 3)
```

Build target: `managed_point_access`. See [build instructions](../README.md#build-the-point-examples).
Next: [follow a generated setter through the runtime](../managed_edit/README.md).
