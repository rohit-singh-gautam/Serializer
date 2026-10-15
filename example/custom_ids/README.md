# Custom IDs in every language

This separate example folder contains maintained C++, C, Java, JavaScript,
TypeScript, Go, C#, Rust, Python, Swift, and Kotlin consumers of one ordinary
[schema](model.serializer). Every consumer implements an application-owned
registry policy and exercises all four generated native codecs. The
[C++ managed companion](managed_cpp/README.md) uses Serializer's `model_store`
registry, immutable snapshots, and transactional undo/redo.

`custom_id` is a generated durable record with three components:

| Component | Meaning |
| --- | --- |
| `scope: uint32` | An application namespace, such as an independently identified document or tenant. Zero is reserved here. |
| `domain: uint32` | A stable application type tag: `1` means widget and `2` means gadget in this example. |
| `local: uint64` | The local identity within that scope/domain. Zero is reserved here. |

The complete tuple forms the key. `(7, widget, 42)`, `(8, widget, 42)`, and
`(7, gadget, 42)` can coexist. A widget lookup rejects a gadget key even when its
scope and local value match. A missing scope/local key produces no payload.
Tag values belong to the application contract; they are independent of runtime
addresses, RTTI names, and generated managed `persistent_id` values.

The registry stores an array of `entity_binding` records and a durable array of
retired IDs. Binding copies the key, rejects duplicates, and rejects retired-key
reuse. Rebinding replaces the payload while preserving the complete logical ID.
Retirement removes the live binding and persists a tombstone. Decoding restores
the ordinary records; the host then validates known tags, nonzero IDs, uniqueness,
and disjoint live/retired sets before allowing lookup. Each consumer verifies
these operations and compares complete canonical positional bytes after JSON,
binary-none, binary-integer, and binary-string round trips.

This is a small single-threaded host policy demonstration. The arrays make lookup
linear, and retirement retains tombstones; it does not implement paged indexing.
The C consumer has an explicit 64-entry host admission limit for each array.
Other consumers use their normal owning containers. JavaScript/TypeScript use
`bigint` for local uint64 values. Java preserves uint64 bits in `long` through the
generated codec. Generated models remain mutable host values; callers must route
changes through their registry policy, revalidate imports, and configure bounded
decoding for untrusted input. The ordinary examples do not provide managed
transactions, history, authorization, or collaboration in other languages.

An application can replace the linear scan with a host map whose key includes all
three components, rebuild that derived map from durable bindings, or delegate
lookup to a database/provider with an explicit ownership and snapshot contract.
Record map keys currently require C++ output, so this portable schema uses a
binding-record array. See [state registries](../../docs/managed/state_registry.md) for the
generated C++ runtime and [paged indexing](../../docs/design/paged_indexing.md) for the
separate disk-backed proposal.

The [Go consumer](go/README.md) is a native module: with `serializer` on `PATH`,
run `go generate .`, `go build -o build/ .`, `go test ./...`, and `go run .` in
its folder. It imports the generated `internal/model` package and needs no
CMake, Python, C++ runtime, or cgo.

For the independent repository matrix, build or install Serializer, then
generate, compile, and execute all selected SDKs in isolated validation copies:

```powershell
python example/custom_ids/run.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --language all --wsl-languages rust,swift
```

Windows C/C++ require an MSVC developer shell. Set `SERIALIZER_TSC` or
`SERIALIZER_KOTLINC` if those SDK executables are outside `PATH`; the runner uses
the same explicit SDK overrides as [the main example runner](../README.md).
Choose `--wsl-languages c,rust,swift` when C should also run in WSL. On Linux use
the matching compiler/library paths and omit the WSL option. To run one consumer,
use `--language python` or another supported language name. No SDK is downloaded
or silently skipped. Generated files stay under ignored `out/custom-id-examples`.

The managed companion builds through `./make.ps1` on Windows or root `make` on
Linux when `SERIALIZER_BUILD_MANAGED` is enabled. It preserves generated managed
allocation identity while binding application composite keys; see its own
walkthrough for snapshot pins, duplicate rollback, retirement, and undo/redo.

On 2026-10-11 all eleven ordinary consumers regenerated, compiled, and executed
successfully with the runner's strict SDK checks. Each exercised the binding
policy and all four protocols. The C++ managed companion has separate CTest
qualification; this statement does not extend managed runtime support to the
other language codecs.
