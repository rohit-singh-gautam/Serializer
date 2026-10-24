# Java caller-owned output storage

Requires Serializer compiler **2.32+**, CMake 4.0+ and Java 17+. The schema keeps
the original language-1 header; this is a backend generation option rather than
a language or payload revision.

The [configuration](java.ini) enables `output_buffer_reuse = true`. The
[consumer](Main.java) reuses one array with a nonzero-offset window for all four
protocols, checks prefix/suffix preservation and independently decodes the exact
returned byte count. Its copy is used for validation; an application can send
`storage, offset, written` directly to its own `OutputStream`.

Configure with an installed compiler package:

```sh
cmake -S example/java/output_buffer -B out/java-output-buffer-example \
  -DSERIALIZER_EXAMPLE_USE_INSTALLED=ON -DCMAKE_PREFIX_PATH=/path/to/serializer
cmake --build out/java-output-buffer-example
ctest --test-dir out/java-output-buffer-example --output-on-failure
```

For a source-checkout build, omit the installed-package options and provide the
normal [source prerequisites](../../../README.md#build-and-test). See the
[Java API](../../../docs/languages/java.md#caller-owned-output-windows) for
exclusive/non-aliased storage, partial failures and remaining allocations.
