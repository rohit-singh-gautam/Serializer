# Generic schema examples

[model.serializer](model.serializer) demonstrates multiple parameters, arrays,
maps, nested applications, and a named application root:

```text
class pair<Key, Value> stable_ids {
  public Key key (1);
  public Value value (2);
}

class box<T> stable_ids {
  public T value (1);
  public array T items (2);
}

class envelope<T> stable_ids {
  public uint32 revision (1);
  public T payload (2);
  public map(uint32) T lookup (3);
}

instantiate example_model = shared::envelope<shared::box<shared::pair<uint32, string>>>;
```

The complete schema includes the required version header and puts these three
generic declarations in `namespace shared`. Type arguments are schema types.
All generators share the same concrete expansion and field IDs.

## C++ template aliases

[main.cpp](main.cpp) uses `shared::pair<std::uint32_t, std::string>`,
`shared::box<item_type>`, and `shared::envelope<batch_type>` alongside the named
`example_model` root. It encodes the named root and decodes the bytes into the
equivalent template alias, checking the nested payload and map contents.

The target is included when building this repository with tests enabled:

```powershell
cmake --build out/build/make-Release --config Release --target generic_example
ctest --test-dir out/build/make-Release -C Release -R serializer_generic_example --output-on-failure
```

Expected output:

```text
revision=1, key=7, value=portable generics
```

Only applications declared in the schema receive C++ aliases. The explicit named
root is a distinct concrete class with the same fields and wire representation.

## Every output language

[run.py](run.py) generates this schema and reuses the maintained basic consumers
in each language. Each consumer reads an independently constructed JSON fixture,
increments `revision`, round-trips all four native protocols, and verifies its
complete positional representation. The runner also checks every output field
against the fixture, including nested collections, UTF-8, embedded NUL, and a
maximum-width `uint32` value.

Run the Python example alone, or select SDKs explicitly:

```powershell
python example/generics/run.py --compiler out/build/make-Release/Release/serializer.exe
python example/generics/run.py --compiler out/build/make-Release/Release/serializer.exe --language java,javascript,go,csharp
python example/generics/run.py --compiler out/build/make-Release/Release/serializer.exe --language all
```

`--language all` requires all eleven language SDKs. Run from a Visual Studio
developer shell for native Windows C/C++. The runner accepts `--cpp-library`,
`--build`, and explicit `--wsl-languages rust,swift,c` selection. SDK executable
overrides use the same `SERIALIZER_<EXECUTABLE>` environment variables as the
[general example runner](../README.md). There are no automatic tool installations.

Non-C++ languages expose concrete models through their normal APIs:

| Output | Named root |
| --- | --- |
| C++ | `example_model` |
| Java | `Schema.ExampleModel` |
| JavaScript / TypeScript / Go / C# / Rust / Python / Swift / Kotlin | `ExampleModel` |
| C | `example_model` |

The smaller [result.serializer](result.serializer) shows `result<person>` and
`result<uint32>`, named `person_result`/`count_result` roots, and a nested
`batch<result<uint32>>`. The C++ regression suite compiles this exact example and
checks wire equivalence in all four protocols. See the
[schema guide](../../docs/generics.md) for syntax and limitations.

Both editor extensions 1.1.13 highlight these examples and navigate generic
parameters, nested arguments, named roots, and generated implementations.
