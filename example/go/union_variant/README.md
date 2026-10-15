# Go: union variant

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
go run . ../../schemas/union_variant/fixture.json build/result.json
```

The [entry schema](model.serializer) includes the shared contract. Generation
writes the ignored `internal/model/model_gen.go` package imported by `main.go`;
rerun generation after schema changes. No CMake, Python, C++ runtime, or cgo is
needed for this native workflow. `go test ./...` checks independent fixture
results and relevant rejection paths. The program can also be run directly with
the fixture arguments above.
See the [Go project guide](../README.md) for layout and optional build wrappers.

The [shared schema](../../schemas/union_variant/model.serializer) contrasts
`union(int32 = code, point_payload = point)` with an owning
`variant(text_payload = text, collection_payload = collection)`. The consumer
checks numeric, record, string, and collection alternatives through all four
protocols. Wire alternative indices and JSON keys are shared across languages.
Native tests and the qualification runner also reject unsupported payload revision 2.

For the independent repository qualification, run from the repository root:

```sh
python example/run.py --compiler <serializer-executable> --language go --example union_variant
```

That harness builds a separate validation copy and checks its output against
independent expected data. It does not replace this source module.
