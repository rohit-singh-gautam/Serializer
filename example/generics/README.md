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
  public uint32 revision (1) ignore(warning version);
  public T payload (2);
  public map(uint32) T lookup (3);
}

instantiate example_model = shared::envelope<shared::box<shared::pair<uint32, string>>>;
```

The complete schema includes the required version header and puts these three
generic declarations in `namespace shared`. Type arguments are schema types.
All generators share the same concrete expansion and field IDs.

The schema language `1.10.0` example also declares a constrained numeric value:

```text
class number<T: (integral || floating_point) && !bool = int32> stable_ids {
  public T value (1);
}

instantiate integer_number = shared::number<int32>;
instantiate floating_number = shared::number<double>;
```

Both `int32` and `double` satisfy the constraint. `bool` and `string` are rejected
when checking schema applications and native C++ template arguments. Constraints
combine categories or exact schema type names with `&&`, `||`, `!`, and grouping;
an exact-type disjunction such as `int32 || double` expresses an allowed type list.
They add no encoded fields. Defaults and nested generic arguments must satisfy
the same predicate. Dependent type arguments are checked after substitution into
each concrete application; the original `stateless` constraint retains its
declaration-time forwarding proof requirement.

## Native C++ templates

[main.cpp](main.cpp) uses `shared::pair<std::uint32_t, std::string>`,
`shared::box<item_type>`, and `shared::envelope<batch_type>` alongside the named
`example_model` root. It encodes the named root and decodes the bytes into the
equivalent native template specialization, checking the nested payload and map contents.
It also checks constrained `shared::number<std::int32_t>` and
`shared::number<double>` values, including compile-time rejection probes for
Boolean and string specializations.

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

[run.py](run.py) generates each language's schema entry and builds its dedicated
generic consumer. Each consumer reads an independently constructed JSON fixture,
increments `revision`, sets the nested `payload.value.key` to `7`, round-trips all
four native protocols, and verifies its
complete positional representation. The runner also checks every output field
against the fixture, including nested collections, UTF-8, embedded NUL, and a
maximum-width `uint32` value.
Each consumer also uses the accepted constrained `integer_number` and
`floating_number` roots with signed integer and floating-point values.

| Language | Dedicated example |
| --- | --- |
| C++ | [Template aliases and typed fields](../cpp/generics/README.md) |
| Java | [Concrete generic models](../java/generics/README.md) |
| JavaScript | [Concrete generic models](../javascript/generics/README.md) |
| TypeScript | [Typed generic models](../typescript/generics/README.md) |
| Go | [Concrete generic models](../go/generics/README.md) |
| C# | [Concrete generic models](../csharp/generics/README.md) |
| Rust | [Owned generic models](../rust/generics/README.md) |
| Python | [Concrete generic models](../python/generics/README.md) |
| Swift | [Concrete generic models](../swift/generics/README.md) |
| Kotlin | [Concrete generic models](../kotlin/generics/README.md) |
| C | [Owned generic models and cleanup](../c/generics/README.md) |

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
[schema guide](../../docs/schema/generics.md) for syntax and limitations.

Both editor extensions highlight these examples and navigate generic
parameters, nested arguments, named roots, and generated implementations.

C++ also permits application-only specializations with no `instantiate` declaration
or concrete schema field. Keep either optional contract form when generating
other languages or checking concrete schema compatibility. Fixed dimension arrays
currently require the native C++ codecs; see [generics](../../docs/schema/generics.md).
