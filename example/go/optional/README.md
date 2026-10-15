# Go: optional

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
go run . ../../schemas/optional/fixture.json build/result.json
```

The [entry schema](model.serializer) includes the shared contract. Generation
writes the ignored `internal/model/model_gen.go` package imported by `main.go`;
rerun generation after schema changes. No CMake, Python, C++ runtime, or cgo is
needed for this native workflow. `go test ./...` checks independent fixture
results and relevant rejection paths. The program can also be run directly with
the fixture arguments above.
See the [Go project guide](../README.md) for layout and optional build wrappers.

The [shared schema](../../schemas/optional/model.serializer) demonstrates
absent and present scalars, records, nested optionals, optional arrays/maps,
and collections of optionals. The consumer verifies all four native protocols.
Native tests and the qualification runner also reject future payload revision 2 and malformed
optional frames. See the [optional contract](../../../docs/schema/optional.md).

For the independent repository qualification, run from the repository root:

```sh
python example/run.py --compiler <serializer-executable> --language go --example optional
```

That harness builds a separate validation copy and checks its output against
independent expected data. It does not replace this source module.
