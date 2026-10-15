# Go: record conversions

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
go run .
```

The [entry schema](model.serializer) includes the shared conversion contract.
`generate.go` writes the ignored `internal/model/model_gen.go` package imported by
`main.go`. The application checks projected inherited fields, nested record
copies, schema defaults, and a generic target; it exits unsuccessfully if any
conversion result is wrong. No fixture arguments are needed.
`go test ./...` runs these conversion assertions through the native test package.

These native commands require no CMake, Python, C++ runtime, or cgo.
See the [Go project guide](../README.md) for the module layout and
[conversion rules](../../../docs/schema/conversions.md) for supported values.
The optional repository qualifier runs the same checks in an isolated copy:

```sh
python example/run.py --compiler <serializer-executable> --language go --example conversions
```
