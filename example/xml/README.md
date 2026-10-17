# XML across languages

All eleven outputs have **generated owning XML codecs**. The examples use the
same [Serializer schema](model.serializer), [mapping configuration](codegen.ini)
and [XML fixture](order.xml). No example-local mapping or JSON intermediary is
required. The separate [C++ package example](../cpp/xml/README.md) also demonstrates
whole-object compression, ZIP and OPC; their other backend paths remain pending.

```text
shared .serializer model
         |
         +-- generated XML codec
         |        |
         |    native XML provider
         |        |
         +------ XML document
```

| Output | Example |
| --- | --- |
| C++ | [Generated XML](cpp/README.md), [packages](../cpp/xml/README.md) |
| Java | [Generated XML](java/README.md) |
| JavaScript | [Generated XML](javascript/README.md) |
| TypeScript | [Generated XML](typescript/README.md) |
| Go | [Generated XML](go/README.md) |
| C# | [Generated XML](csharp/README.md) |
| Rust | [Generated XML](rust/README.md) |
| Python | [Generated XML](python/README.md) |
| Swift | [Generated XML](swift/README.md) |
| Kotlin | [Generated XML](kotlin/README.md) |
| C | [Generated XML](c/README.md) |

Each example accepts `INPUT.xml OUTPUT.xml`, checks the expected values, writes a
complete document and reads it back. The small example contract requires the
expanded root name `{urn:serializer:xml-example}order`, the unqualified `id`
attribute, then `customer`, `count` and repeated `tag` children. It rejects DTDs
and unexpected structure. The default configuration selects this mapping directly;
the fixed-XSD variant below obtains placement and cardinality from the schema.
These examples do not demonstrate unknown preservation, HTML, general document
editing or managed persistence.

See the [implemented API](../../docs/serialization/xml_packages.md) and
[pending work](../../docs/xml_pending.md) before selecting a production backend.

Build the XML-enabled compiler and C++ examples first, then run:

```powershell
python example/xml/run.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --build out/build/xml-examples
```

The runner requires every selected SDK, generates fresh model sources, builds
with strict compiler checks and exchanges all producer/consumer pairs. It also
checks each export with Python ElementTree. Use `--language java,python,go` to
select fewer outputs. SDK executable overrides follow
[the common runner](../README.md); `--wsl-languages` explicitly
selects WSL SDKs on Windows. Kotlin needs `kotlinc` on PATH or `SERIALIZER_KOTLINC`.
C++ needs the root `serializer_xml_cpp` executable or `--cpp-executable`.

JavaScript/TypeScript and Rust install locked parser dependencies into the ignored
build tree. C needs libxml2 development headers/libraries; `--libxml2-prefix` can
select an installation. Windows defaults to the compiler build's vcpkg prefix.
The runner never installs global language packages.

The [fixed schema](order.xsd) defines `id` as an attribute and the ordered
`customer`, `count` and repeated `tag` children. Its [configuration](xsd.ini)
contains no `attributes` setting: XSD is authoritative. This remains compiled
mapping with no runtime XSD loading. Run the same exchange using:

```powershell
$xsd_cpp = 'out/build/make-Release/example/xml/cpp/Release'
python example/xml/run.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --config example/xml/xsd.ini `
  --build out/build/xml-xsd-examples `
  --cpp-executable "$xsd_cpp/serializer_xml_cpp_xsd.exe"
```

Build `serializer_xml_cpp_xsd` first when including C++. This dedicated executable
uses the XSD-generated header; the default C++ example uses `codegen.ini`.
Other selected backends compile the fresh output produced by the runner.
