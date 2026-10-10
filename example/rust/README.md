# Rust examples

Four standard examples and a generic example live directly under `example/rust`:

| Folder | What it demonstrates |
| --- | --- |
| [generics](generics/) | Nested schema applications, typed payload edits, and all four protocols |
| [basic](basic/) | Scalars, full-width IDs, a wire-name override, defaults, and a typed field edit |
| [collections](collections/) | Typed arrays, maps, enums, Unicode, embedded NUL, and integer boundaries |
| [complex](complex/) | An order archive spanning 13 shared schema files, nested/diamond includes, parent composition, orders, customers, inventory, payments, shipments, audit unions, and aggregate maps |
| [interoperability](interoperability/) | Independent typed fixture construction and any-to-any exchange across all four protocols and three union alternatives |

From the repository root, after building the C++ compiler:

```sh
python example/run.py --compiler build/serializer --language rust
```

Use `build/serializer.exe` on Windows and run from a developer command prompt
for MSVC. `--example complex` selects one example. Generated code and executables
stay under `out/examples`; maintained source is never overwritten. Each folder
contains a `model.serializer` entry that includes the shared contract. The runner
compiles that shared contract once per example for all requested language outputs.
See [SDK setup and runner options](../README.md).

The first three programs accept `<input.json> <output.json>`. They decode the
independent fixture, increment `revision` through the typed model, exercise all
four codecs, and write the result. The runner compares all resulting JSON data
against the independent fixture, including exact 64-bit integers. The fourth
program accepts `<fixtures-directory> emit|verify`; the runner supplies the
producer manifest and runs every producer before any consumer.

Run the generic example separately:

```sh
python example/generics/run.py --compiler build/serializer --language rust
```

[Versioning example](versioning/README.md): compatible historical reads, explicit replacement migration, and all revision types.

The [union and owning variant example](union_variant/) contrasts trivial raw storage
with owning string/collection alternatives and verifies all four protocols.
