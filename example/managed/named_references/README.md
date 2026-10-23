# Named managed generic self references

Requires compiler/runtime **2.31.0**, schema language **1.23.0** and C++23.
The named `number_node` record binds its copied `managed_ref<node<T>>` field to
`managed_ref<number_node>`. Its target is the named record, preserving the ordinary
document/class/object identity checks. Two differently named instantiations remain
different target types even when their generic arguments match.

The example creates two managed nodes, links a cycle through generated editors,
and resolves it through one immutable registry. It prints `11 -> 22`.

From the repository root, `./make.ps1 test` on Windows or `make test` on Linux
builds and runs it with the other native examples. For standalone installed use:

```sh
cmake -S example/managed/named_references -B out/named-references \
  -DSERIALIZER_EXAMPLE_USE_INSTALLED=ON -DCMAKE_PREFIX_PATH=/path/to/serializer-2.31
cmake --build out/named-references
ctest --test-dir out/named-references --output-on-failure
```

Select the installed configuration with `--config Release` / `-C Release` for
multi-configuration Windows generators. The installed runtime must include managed
records. See the [binding contract](../../../docs/schema/named_managed_references.md)
for language provenance, supported container wrappers and nested-record limits.
