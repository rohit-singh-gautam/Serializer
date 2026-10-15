# Go: managed wire-record codecs

This is a native Go 1.22+ module. With the installed `serializer` executable on
`PATH`, run from this folder:

```sh
go generate .
go build -o build/ .
go test ./...
```

The local [entry schema](model.serializer) selects the history record example by
default. `generate.go` writes the ignored `internal/model/model_gen.go` package,
which `main.go` imports. These native commands need no CMake, Python, C++ runtime,
or cgo; rerun generation after changing schema inputs.
Native tests compare independently loaded history metadata after each revision
increment and reject malformed records without publishing an output file.

The program accepts `<input.bin> <output.bin>` encoded with `BINARY_INTEGER`. It
checks all four record codecs, increments the record revision, and writes the
result. These are wire records only: a native Go managed engine, undo/redo,
journal recovery, collaboration authority, and authentication are not provided.

The optional matrix creates separate validation modules for the history,
journal, collaboration, and authentication schemas. It regenerates each copy's
internal model package without changing this source module's history default:

```sh
python example/managed/multilanguage/run.py --compiler <serializer-executable> --language go --feature all
```

Run the qualifier from the repository root. It supplies binary fixtures and
checks field values, producer/consumer exchanges, truncated input, and trailing
bytes. See [matrix contracts and limits](../../README.md) and
[the Go project guide](../../../../go/README.md).

The optional CMake wrapper in this client folder runs the native Go workflow
with the history schema and accepts `SERIALIZER_EXAMPLE_COMPILER` for an explicit
CLI path. The separate per-feature CMake directories under `multilanguage/`
remain Python-based cross-language qualifiers; they are not required to build
this native Go project.
