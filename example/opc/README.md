# Typed OPC catalog

The shared [schema](model.serializer) declares an XML catalog, its package role,
and binary attachments. Generated C++ and Python facades create content types
and relationships, compress payload parts, stage full-file Save, and discover
and validate typed payloads on load. No handwritten OPC metadata is needed.
This example uses an application vocabulary, without claiming 3MF conformance.

Enable XML and zlib when building Serializer. On Windows:

```powershell
./make.ps1 test -CMakeArgs '-DSERIALIZER_WITH_XML=ON','-DSERIALIZER_WITH_ZLIB=ON'
out/build/make-Release/example/opc/Release/serializer_opc_example.exe out/catalog.opc
```

On Linux:

```sh
make test CMAKE_ARGS='-DSERIALIZER_WITH_XML=ON -DSERIALIZER_WITH_ZLIB=ON'
out/build/make-Release/example/opc/serializer_opc_example out/catalog.opc
```

The CMake test suite runs the example and `serializer_opc_exchange` when Python
3.10+ is available. The latter generates Python codecs, checks all four C++/Python
producer-consumer pairs, and independently inspects both files using Python's
`zipfile` and ElementTree. Run it directly with:

```powershell
python -B example/opc/run.py --compiler out/build/make-Release/Release/serializer.exe `
  --cpp out/build/make-Release/example/opc/Release/serializer_opc_example.exe `
  --build out/build/opc-exchange
```

`serializer_opc_example --read FILE.opc` validates a matching file without saving.
The catalog lives at `/Catalog/catalog.xml`, its attachment at
`/Attachments/sample.bin`. The model and attachment views must remain alive
through synchronous Save; load returns an owning aggregate. Generated loaders
check archive integrity, metadata, relationships and XML before returning.

For a standalone C++ build, configure this folder with CMake and
`SERIALIZER_EXAMPLE_USE_INSTALLED=ON` plus `CMAKE_PREFIX_PATH` pointing to an
XML/zlib-enabled Serializer installation. The example also works as a source
subdirectory. See [package contracts](../../docs/serialization/xml_packages.md)
for limits, extension preservation and application validation callbacks.
