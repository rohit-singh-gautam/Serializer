# TypeScript: basic

Run from the repository root:

```sh
python example/run.py --compiler build/serializer --language typescript --example basic
```

Use `.exe` for the compiler on Windows. See the [language guide](../README.md)
for the example contract and [all examples](../../README.md) for SDK setup.
The local [entry schema](model.serializer) includes the shared schema; compile
only this entry when generating the example manually. Generated files belong
in the build directory.

The schema uses `public version uint32 (1) { 1 };` to declare the payload
revision. The generated `version` stays at 1 during these round trips; readers
reject unsupported version 2. Ordinary business counters should remain ordinary
fields. See [payload versioning](../../../docs/schema/versioning.md).
