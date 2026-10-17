# Generated Python XML

Generate `schema.py` from the [shared schema](../model.serializer) with
[XML configuration](../codegen.ini), then place it beside `main.py` in an
ignored build directory. Requires Python 3.10+.

```text
python main.py INPUT.xml OUTPUT.xml
```

The example uses generated `XmlExampleOrder.decode_xml()` / `encode_xml()` and
checks the same model through `binary_none`, malformed input, DTD rejection,
and byte limits. The optional generated `XmlSchema` runtime validator requires
`lxml` when constructed; ordinary XML codecs use the standard library.

See the [shared example](../README.md) for the contract and build workflow.
