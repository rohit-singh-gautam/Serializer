# Export referenced values

Compiler/runtime **2.6.0**, language **1.19.0**. The consumers generate from
[one schema](model.serializer), borrow existing payload storage and compare output
with an ordinary owning consumer. The application keeps the source alive.
No document/object/path identity is serialized.

## Basic borrowed view

```cpp
owner source;
source.b.values = {4, 2, 3};
source.b.codes = {0, 1, 2};
borrowed_consumer consumer;
consumer.d = source.b;
consumer.serialize_out<rohit::serializer::json>(stream);
```

The owner stays ordinary and unaware of this consumer. The C++ field stores
exactly a `const payload*`; it does not copy the arrays or allocate a payload.
Keep `source.b` alive at a stable address through the last access. Temporary
payload assignment rejects.

The basic type form is `public borrowed(payload) d (1);`. The maintained model
also demonstrates the equivalent field selector `borrowed(owner.b)`, with
`bind(source)` as an alternative to direct payload assignment. See the
[language lifetime rules](../../docs/schema/value_references.md#nonowning-access-across-languages)
for other outputs; native representations require their own qualification.

Reference fields and the ordinary owning consumer write the same value:

```json
{"d":{"values":[4,2,3],"codes":[0,1,2]}}
```

Sample edits may differ; each consumer checks its corresponding ordinary value.

## Optional ownership and translation

The example also demonstrates two ordinary records sharing one independent
payload when shared ownership is wanted:

```cpp
auto m = std::make_shared<payload>();
shared_owner a;
a.b.bind(m);
shared_value_consumer c;
c.d.bind(std::move(m));
```

These fields declare `shared(payload)`. Neither containing object retains the
other; releasing `a.b` leaves `c.d` alive. Shared/weak modes are optional and
unnecessary for the basic borrowed export. `shared(owner.b)` separately demonstrates
an alias whose lifetime belongs to its complete inline-storage owner.

`owner.as_consumer`, called through `with_as_consumer(...)`, demonstrates scoped
translation of an ordinary payload. It borrows its arrays without constructing
an intermediate owning consumer.

The consumers compare all four native protocols against an `owning_consumer`
baseline and check buffer identity where exposed. Copies marked as verification
baselines are outside the export paths. Unbound/expired views fail access.

## Run

From the repository root, using installed SDKs:

```powershell
python example/references/run.py --compiler out/build/ReleaseWindows/serializer.exe --language all
```

Use `--language cpp,python` for a subset. The runner reports missing SDK
prerequisites; see the [example catalog](../README.md). C++ supports the root
CMake build and an independent installed Serializer 2.6 package. Go is a native
module: run `go generate .`, `go build -o build/ .`, then `go test ./...` there.

Reference wrappers are export-only and explicitly reject decoding. Read their
bytes with `owning_consumer`; no runtime binding is reconstructed. See the
[value-reference guide](../../docs/schema/value_references.md) for complete rules.
Output buffers retain their normal allocation costs.

The separate [durable address examples](../address_references/README.md) cover
the older identity-bearing API.
