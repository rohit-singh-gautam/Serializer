# Go: versioning

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
go run . ../../schemas/versioning/fixture.json build/result.json
```

The [entry schema](model.serializer) includes the shared contract. Generation
writes the ignored `internal/model/model_gen.go` package imported by `main.go`;
rerun generation after schema changes. No CMake, Python, C++ runtime, or cgo is
needed for this native workflow. `go test ./...` checks independent fixture
results and relevant rejection paths. The program can also be run directly with
the fixture arguments above.
See the [Go project guide](../README.md) for layout and optional build wrappers.

The [shared schema](../../schemas/versioning/model.serializer) declares all eight
revision types, custom names, visibility, compatibility, lifecycle annotations,
replacement metadata, and reservations. The consumer reads revision 8 with the
compatible policy, round-trips its historical positional layout, explicitly
copies `old_name` into `name`, and sets revision 10 before strict four-protocol
checks. Replacement conversion remains application code.

It also constructs every revision type and checks independent positional bytes,
including binary32 `0.1` precision and the maximum uint64 value. Release catalogs
and nested age/count/compatibility policies resolve during generation. The
qualification runner accepts `--version-policy-as-of 2026-10-06` to pin evaluation;
generated models contain no clocks or policy trees.

For the independent repository qualification, run from the repository root:

```sh
python example/run.py --compiler <serializer-executable> --language go --example versioning
```

That harness builds a separate validation copy and checks its output against
independent expected data. It does not replace this source module.
