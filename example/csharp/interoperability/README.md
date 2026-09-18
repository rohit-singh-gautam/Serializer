# C#: interoperability

Run from the repository root:

```sh
python example/run.py --compiler build/serializer --language csharp --example interoperability
```

Use `.exe` for the compiler on Windows. See the [language guide](../README.md)
for the example contract and [all examples](../../README.md) for SDK setup.
The local [entry schema](model.serializer) includes the shared schema; compile
only this entry when generating the example manually. Generated files belong
in the build directory.
