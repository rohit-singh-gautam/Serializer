# Java generic example

[Main.java](Main.java) decodes the shared `example_model`, increments its
revision, and sets `payload.value.key` to `7`. The payload is the nested schema
application `box<pair<uint32, string>>` inside `envelope<T>`. Arrays and maps
retain their independent values, including full-width integers, UTF-8, and NUL.

Schema generics expand into concrete generated types; this example uses the named root and typed nested fields without spelling compiler-owned instance names.

From the repository root, after building Serializer:

```sh
python example/generics/run.py --compiler build/serializer --language java
```

On Windows, use the path to `serializer.exe`. See the [shared generic guide](../../generics/README.md)
for SDK selection, Windows developer-shell requirements, and WSL options.
The runner generates [model.serializer](model.serializer), builds this consumer,
and checks all four protocols against an independent fixture. Generated sources
and `result.json` remain under `out/generic-examples/java`.

The decoded result has `revision=10` and `payload.value.key=7`; every other field
matches the input fixture. A successful run ends with:

```text
PASS: 1 generic consumers, all four protocols
```
