# C++ XML and packages

This runnable example generates XML mappings from [model.serializer](model.serializer)
and [codegen.ini](codegen.ini). It checks a complete object's round trip through
XML, JSON with gzip, mixed XML/binary ZIP, and a standard OPC package.

```text
order
  +-- XML -------> XML part
  +-- JSON ------> gzip stream
  +-- binary_none -> ZIP part
```

The XML shape is:

```xml
<order xmlns="urn:serializer:xml-example" id="7">
  <customer>R&amp;D</customer>
  <count>2</count>
  <count>4</count>
</order>
```

`id` is explicitly an attribute. Other members are children. The project needs
LibXml2 and ZLIB-enabled Serializer. Compression completes one entire object;
`finish()` emits the gzip trailer or ZIP directory. No incremental record API
is involved. The plain ZIP includes XML and positional binary parts solely to
demonstrate codec independence; document profiles keep history and journals in
separate side channels.

Pass an optional output directory to write independently inspectable `order.xml`,
`order.json.gz`, `order.zip` and `order.opc` artifacts. ZIP/OPC files use full-Save
staging and replacement; no Serializer history is embedded.

The OPC writer generates its manifest and relationships. The reader finds the
main document by relationship type, so its filename is not hardcoded by readers.
This example implements no Office document vocabulary or rendering.

Use the root XML-example build option documented in the
[repository README](../../../README.md), or configure this directory as a
standalone CMake project using the common
[example dependency helper](../../cmake/serializer_example_dependency.cmake).
