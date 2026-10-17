# Typed XML regression fixture

This fixture covers nested and repeated records, optional Boolean attributes,
fixed enum arrays, unsigned 64-bit limits and negative zero, XML whitespace and
Unicode preservation, floating values, and explicit input/output runtime XSD
validation boundaries. Maintained probes cover all eleven generated languages;
runtime XSD uses each backend's declared provider or explicit validator adapter.
Java, Python, C#, JavaScript, TypeScript and Rust also reject authored functions,
factories and conversions that collide with XML APIs, with XML-disabled
generation as the compatibility control.

The same fixture checks configured simple text with scalar attributes, collection
wrappers with escaped/empty values, empty collections, namespace identity and
malformed wrapper rejection. Pass `--protocols binary_none` to verify XML remains
usable alongside restricted native format selection; the C++ executable is built
separately with its configured native selection.

```text
python -m pip install --target out/xml-deps \
  -r test/resources/xml_shapes/requirements.txt

python test/xml_shapes_test.py \
  --compiler PATH_TO_SERIALIZER \
  --cpp-executable PATH_TO_CPP_SHAPES_TEST \
  --build out/xml-shapes \
  --python-dependencies out/xml-deps
```

Require the selected SDKs and [XML parser dependencies](../../../example/xml/README.md),
including pinned `lxml` for Python's prepared runtime validator. C++ requires its
separately built typed probe. `--language` selects an explicit subset; missing
providers are failures. Kotlin accepts `SERIALIZER_KOTLINC`; `--wsl-languages`
selects WSL SDKs explicitly. All output stays in the chosen ignored build directory.
