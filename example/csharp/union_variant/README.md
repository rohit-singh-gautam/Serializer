# C#: union and owning variant

The [shared schema](../../schemas/union_variant/model.serializer) declares a raw
`union(int32 = code, point_payload = point)` and an owning
`variant(text_payload = text, collection_payload = collection)`.
The fixture exercises both alternatives of each declaration, including strings,
arrays, maps, and embedded control characters. The example reads the generated
classes, retains the schema-defined payload `version`, and verifies every field
through JSON, BINARY_NONE, BINARY_INTEGER, and BINARY_STRING round trips. The
runner also requires the generated JSON reader to reject unsupported version 2.

Run from the Serializer repository root:

```sh
python example/run.py --compiler build/serializer --language csharp --example union_variant
```

Use `.exe` for the compiler on Windows. The local [entry schema](model.serializer)
includes the shared schema. Generated sources belong in the build directory.
C++ raw unions require trivially destructible alternatives; owning variants use
`std::variant` and require no separate manually maintained discriminator.
The wire alternative indices and JSON alternative keys are the same across
languages. See [all examples](../../README.md) for installed SDK requirements.
