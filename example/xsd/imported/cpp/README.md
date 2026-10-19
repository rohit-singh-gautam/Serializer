# C++ XSD imported client

Build with CMake from the repository root; Python is unnecessary:

```sh
cmake -S example/xsd -B out/build/xsd-imported -DXSD_WORKFLOW=imported -DCMAKE_PREFIX_PATH="/path/to/serializer;/path/to/libxml2"
cmake --build out/build/xsd-imported --config Release --parallel 2
ctest --test-dir out/build/xsd-imported -C Release --output-on-failure
```

The compiler imports the fixed XSD, generates this client's model header and links
`Serializer::runtime`. To build from the checkout, use
`-DSERIALIZER_SOURCE_DIRECTORY=<absolute-checkout>` instead of an installed
Serializer prefix. See [the shared guide](../../README.md) for XML-enabled build
prerequisites, Windows runtime DLL paths and exact verification scope. The Python
runner is an optional cross-language qualification tool.
