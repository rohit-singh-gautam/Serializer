# C++ generic example

[main.cpp](main.cpp) decodes the shared `example_model`, increments its
revision, and sets `payload.value.key` to `7`. The payload is the nested schema
application `box<pair<uint32, string>>` inside `envelope<T>`. Arrays and maps
retain their independent values, including full-width integers, UTF-8, and NUL.

C++ also demonstrates schema-declared template aliases with a compile-time field-type check.

From the repository root, after building Serializer:

```sh
python example/generics/run.py --compiler build/serializer --language cpp
```

On Windows, use the path to `serializer.exe`. See the [shared generic guide](../../generics/README.md)
for SDK selection, Windows developer-shell requirements, and WSL options.
The runner generates [model.serializer](model.serializer), builds this consumer,
and checks all four protocols against an independent fixture. Generated sources
and `result.json` remain under `out/generic-examples/cpp`.

The decoded result has `revision=10` and `payload.value.key=7`; every other field
matches the input fixture. A successful run ends with:

```text
PASS: 1 generic consumers, all four protocols
```
