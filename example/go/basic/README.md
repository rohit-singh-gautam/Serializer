# Go: basic

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
go run . ../../schemas/basic/fixture.json build/result.json
```

The [entry schema](model.serializer) includes the shared contract. Generation
writes the ignored `internal/model/model_gen.go` package imported by `main.go`;
rerun generation after schema changes. No CMake, Python, C++ runtime, or cgo is
needed for this native workflow. `go test ./...` checks independent fixture
results and relevant rejection paths. The program can also be run directly with
the fixture arguments above.
See the [Go project guide](../README.md) for layout and optional build wrappers.

The schema uses `public version uint32 (1) { 1 };` to declare the payload
revision. The consumer preserves revision 1 through all four protocols. Native
tests and the qualification runner also verify that unsupported revision 2 is
rejected.
See [payload versioning](../../../docs/schema/versioning.md).

For the independent repository qualification, run from the repository root:

```sh
python example/run.py --compiler <serializer-executable> --language go --example basic
```

That harness builds a separate validation copy and checks its output against
independent expected data. It does not replace this source module.
