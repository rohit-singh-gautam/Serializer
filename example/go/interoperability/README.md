# Go: interoperability

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
```

The [entry schema](model.serializer) generates the ignored
`internal/model/model_gen.go` package imported by `main.go`. This native Go
workflow needs no CMake, Python, C++ runtime, or cgo. See the
[Go project guide](../README.md) for layout and optional wrappers.
Native tests prepare a local producer manifest, emit and verify all protocols
and union alternatives, compare frozen positional bytes, and reject malformed
messages. They require no Python runner.

The program accepts `<fixtures-directory> emit|verify|benchmark`. Exchange mode
uses the producer manifest in that directory, builds independent typed values,
and checks all four protocols and three union alternatives. For prepared
manifests and a complete producer/consumer check, run the optional repository
qualifier from the repository root:

```sh
python example/run.py --compiler <serializer-executable> --language go --example interoperability
```

It builds a separate copy of this module and compares complete values and frozen
positional bytes. Select additional languages for cross-language exchanges;
[all examples](../../README.md) describes the SDK requirements.
