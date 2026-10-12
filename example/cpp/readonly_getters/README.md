# Readonly field getters

This owning C++ example requires compiler/runtime **1.19.0** and schema language
**1.14.0**. Compile `model.serializer` through `serializer_generate`; generated
headers stay in the build directory.

```text
private integer_box contents_ ("contents", 1) expose readonly(public) as contents;
```

`document::contents()` returns `const integer_box&` from a mutable or const lvalue
owner. The explicit getter adds no setter, copied value, extra storage, field ID
or wire name. The example verifies its backing address, a generic owning result,
friend writes and an exact JSON round trip. Native C++ friendship already supplies
the privileged write path and does not need dedicated schema syntax.

Keep the owner alive and its field stable for the complete reference lifetime.
Calls on mutable or const temporary owners are deleted; the returned reference
cannot mutate the field. Container relocation, owner destruction or replacement
can still invalidate references obtained from a surviving lvalue.

The initial profile supports unpacked ordinary owning C++ records, including
stored transient fields. Managed declarations, views, wire-only values, version
metadata, legacy union storage and other output languages reject exposure.
`readonly(protected)` and `readonly(private)` retain normal C++ access rules.

To build this directory independently, install Serializer **1.19.0** first. Its
package supplies the compiler and runtime used by `serializer_generate`. Replace
`<Serializer install prefix>` with that installation directory; include any required
dependency installation prefixes in `CMAKE_PREFIX_PATH` as well. From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<Serializer install prefix>"
cmake --build build --config Release --target serializer_readonly_getters_example
ctest --test-dir build -C Release --output-on-failure
```

These commands select Release for single-configuration and multi-configuration
generators. Generated headers stay in the build directory.

Repository builds include this example through the normal root wrappers. See the
[usage guide](../../../docs/guides/usage.md) and
[construction and behavior guide](../../../docs/schema/behavior.md).
