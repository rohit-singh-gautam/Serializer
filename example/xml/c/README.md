# Generated C XML example

Generate the shared model with `--language c --config example/xml/codegen.ini`
and output `schema.h` in an ignored build directory. Compile `main.c` as C11 with
that directory on the include path and link libxml2 (for example,
`pkg-config --cflags --libs libxml-2.0`). No C++ runtime is required.

Run `xml-c example/xml/order.xml OUTPUT.xml`. The program checks generated XML
mapping, a complete round trip, independent `binary_none`, malformed roots and
DTD rejection. Output files are complete objects.

Initialize owning models and buffers before passing them to the generated APIs:

```c
xml_example_order value = {0};
srl_buffer bytes = {0};
srl_status status = xml_example_order_init(&value);
/* Fill value, then encode or decode using zero-initialized/valid destinations. */
if (status == srl_ok) {
  status = xml_example_order_encode_xml(&value, &bytes, NULL);
}
if (status == srl_ok) {
  status = xml_example_order_decode_xml(
      &value, bytes.data, bytes.size, NULL);
}
xml_example_order_free(&value);
srl_buffer_free(&bytes);
/* Return or handle status. */
```

Check every returned `srl_status`. Successful encode/decode replaces the owning
destination; failure preserves it. Zero initialization and repeated cleanup are
valid. The XML APIs use `srl_limits`; a null limits pointer selects their defaults.

With generation-time `runtime_xsd = true`, encode/decode also take a final
`const srl_xml_validation*` argument. Pass null for generated checks alone. For
native prepared validation, zero-initialize `srl_xml_schema`, call
`srl_xml_schema_prepare`, then obtain its callback adapter with
`srl_xml_schema_validation`. That adapter borrows the prepared schema: keep the
schema alive through every codec call using the adapter, then release it with
`srl_xml_schema_free`. Do not prepare/free a schema while a call borrows it;
the bundled validator creates a separate validation context per call. Callers own
custom callback contexts and their concurrency policy. The generated
`validation_scope` selects input, output or both.
