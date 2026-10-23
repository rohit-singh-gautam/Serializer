# Durable address references (advanced)

These advanced examples serialize document/object/generation/field-path identity
and explicitly resolve those addresses after loading. This is separate from the
[canonical value-reference examples](../references/README.md), whose pointer-only
bindings write ordinary field values without identity metadata. The examples here
use [one schema](model.serializer), require compiler/runtime **2.5.0** and language
**1.18.0**, and also demonstrate scoped translator output.

The owner contains a `live_value` with `text` and `count`. Its `to_wire` translator
projects those fields as `projected_value.label` and `projected_value.count`.
After binding the reference, each consumer changes the source to `"updated"` and
`2`, then writes the projection immediately. The projected JSON is:

```json
{"label":"updated","count":2}
```

Whitespace can differ between encoders. The consumers verify the projected content.

## Choose ownership

The schema declares three reference types for the same field slot:

```text
reference borrowed_b = borrowed(owner.b);
reference shared_b = shared(owner.b);
reference weak_b = weak(owner.b);

class consumer stable_ids {
  public shared_b d (1);
}
```

| Mode | C++ lifetime | Other outputs |
| --- | --- | --- |
| `borrowed` | Binds an lvalue owner without retaining it. Keep the owner alive at a stable address until the last access. | Java, Kotlin, C#, Python and JavaScript use a weak owner; Rust uses `Weak`; C, Go and Swift use their explicit scope/owner facilities. |
| `shared` | Retains the complete owner through `std::shared_ptr<const owner>`, including the inline `b` field. | Retains the owner using the runtime's strong reference, Rust `Rc`, Swift owner box, Go pointer or C scope control block. |
| `weak` | Uses `std::weak_ptr<const owner>` and pins the owner during each read. | Uses the backend's weak owner or explicit scope-liveness contract. Reclamation timing is runtime-dependent. |

Sharing an inline field keeps its parent alive. Give a child its own storage owner
and identity if it needs a lifetime independent of that parent. Reference bindings
add no synchronization; complete edits before entering a read/translation callback.
See the [full lifetime and language mappings](../../docs/schema/references.md).
In C++, `read_value()` deliberately returns an owning copy. Use `with_value()`
for synchronous const access to the selected storage, as in the composition below.
Other outputs keep their native value/reference parameter rules.

## Translate through the reference

The mapping lives on `live_value` in the shared schema:

```text
public translator projected_value as to_wire {
  label = text;
  count = count;
};
```

The C++ consumer creates a shared owner, binds `d`, then edits the original field:

```cpp
auto storage = std::make_shared<owner>();
consumer links;
links.d.bind(storage, "document-1", 7, 1);
storage->b.text = "updated";
storage->b.count = 2;

rohit::full_stream_auto_alloc output;
links.d.with_value([&](const live_value& source) {
  source.with_to_wire([&](const auto& projected) {
    projected.template serialize_out<rohit::serializer::json>(output);
  });
});
```

The outer callback borrows the current `b` field while its owner is available.
The inner callback reads the mapped fields and writes the target schema directly,
without constructing a complete owning `projected_value`. Only the encoded bytes
escape these scopes. Keep the source unchanged during the operation and do not
retain the scoped projection or its borrowed getter results. Existing `conversion`
declarations remain available when an independent owning result is needed.

## Save addresses and resolve them after loading

The address records contain a document ID, owner ID, generation and stable field
path. They never contain a process pointer or an embedded copy of `b`. A decoded
reference starts unbound. Load the owner, look it up using the application's
identity policy, then call `resolve(owner, document_id, owner_id, generation)`.
Every consumer demonstrates this step; owners and edges remain separate payloads.
The application assigns nonzero IDs and generations and verifies that the selected
owner actually has that identity.

## Run from the repository root

The runner generates fresh sources under the ignored build directory, compiles
the selected handwritten consumers and runs their checks. Python 3.10+ and each
selected language SDK must already be installed. It does not download SDKs.
For the existing Windows source-build layout:

```powershell
python example/address_references/run.py --compiler out/build/ReleaseWindows/serializer.exe --build out/address-reference-examples --language all
```

For the default Windows wrapper layout, use
`out/build/make-Release/Release/serializer.exe` instead. Run from a Visual Studio
developer shell when selecting C/C++. On Linux, after `make all`:

```sh
python3 example/address_references/run.py --compiler out/build/make-Release/serializer --build out/address-reference-examples --language all
```

Select a subset with `--language cpp,python,javascript`, for example. The C++
runtime library defaults to the compiler's directory; use `--cpp-library PATH`
when it lives elsewhere. SDK overrides such as `SERIALIZER_KOTLINC` and explicit
`--wsl-languages rust,swift` selection follow the [common runner](../README.md#run).
JavaScript borrowed/weak bindings require native `WeakRef` support; see the
[backend contract](../../docs/schema/references.md#language-mappings).

The Go folder is also a native module. With the installed `serializer` CLI on
`PATH` and Go 1.22+, run from `example/address_references/go`:

```sh
go generate .
go build -o build/ .
go test ./...
```

Generation writes the ignored `internal/model` output. Regenerate after schema
changes; ordinary Go builds and tests do not run generation automatically.

## Build C++ with CMake

The C++ folder can be configured independently, using this checkout as its
dependency:

```sh
cmake -S example/address_references/cpp -B out/address-reference-cpp -DCMAKE_BUILD_TYPE=Release
cmake --build out/address-reference-cpp --config Release --target serializer_reference_example
ctest --test-dir out/address-reference-cpp -C Release --output-on-failure
```

To consume an installed Serializer **2.5** package instead, configure with:

```sh
cmake -S example/address_references/cpp -B out/address-reference-cpp-installed -DCMAKE_BUILD_TYPE=Release -DSERIALIZER_EXAMPLE_USE_INSTALLED=ON -DCMAKE_PREFIX_PATH=/path/to/serializer/install
cmake --build out/address-reference-cpp-installed --config Release --target serializer_reference_example
ctest --test-dir out/address-reference-cpp-installed -C Release --output-on-failure
```

The C++ program also checks all four native protocols and demonstrates shared
retention followed by weak-reference expiration. The standalone CMake entry point
requires CMake 4.0+, a C++23 compiler and the installed package's dependencies.

## Read the consumers

| Language | Source |
| --- | --- |
| C++ | [main.cpp](cpp/main.cpp) |
| C | [main.c](c/main.c) |
| Java | [Main.java](java/Main.java) |
| JavaScript | [main.mjs](javascript/main.mjs) |
| TypeScript | [main.mts](typescript/main.mts) |
| Go | [main.go](go/main.go) |
| C# | [Program.cs](csharp/Program.cs) |
| Rust | [main.rs](rust/main.rs) |
| Python | [main.py](python/main.py) |
| Swift | [main.swift](swift/main.swift) |
| Kotlin | [Main.kt](kotlin/Main.kt) |

These examples use ordinary owning records and JSON/native binary codecs.
Collection-element references, managed references, arbitrary translator expressions
and optional XML generation are outside this feature's current supported subset.
Read [reference and translator rules](../../docs/schema/references.md) before
adapting the schema, and see the
[release qualification](../../docs/verification/verification-references-translators-2026-10-18.md)
for the recorded verification status.
