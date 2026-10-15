# Go examples

Each folder is a native Go 1.22+ module. Put the installed `serializer` executable
on `PATH`, then work in the example directory:

```sh
cd example/go/basic
go generate .
go build -o build/ .
go test ./...
```

Generation uses the local `model.serializer` entry and its shared includes.
The generated codecs use only the Go standard library. An installed Serializer
CLI and Go are sufficient; these commands require no CMake, Python, C++ runtime,
or cgo. Building Serializer itself remains a separate C++ source-build step.
On Windows, `serializer.exe` is found through `PATH` by the same directive.

Every module has this layout:

```text
go.mod                 maintained module identity and Go version
main.go                application code importing its internal/model package
main_test.go           native fixture and behavior checks
generate.go            go:generate command
model.serializer       entry schema
internal/model/
  README.md            generated-package instructions
  model_gen.go         generated, ignored
  model_gen.go.d       generated include dependencies, ignored
```

When adapting this layout to a new project, create `internal/model` first: the
CLI requires the output and depfile parent directory to exist. Maintained
examples retain `README.md` there, so a fresh checkout already has the directory.

`go generate .` must run before the first build or test and again after changing
the entry schema or its includes. `go build` and `go test` do not run generation
automatically. `go test ./...` runs the module's native fixture and behavior
tests, including full-value codec checks and relevant failure cases. The
interoperability module also checks its own emitted positional bytes against the
frozen fixture. The programs can be run separately with the arguments described
in their READMEs.
Use `go build .` for the ordinary module-named executable, or `go build -o build/ .`
to keep the executable under the ignored build directory.

| Folder | What it demonstrates |
| --- | --- |
| [basic](basic/README.md) | Scalars, full-width IDs, defaults, wire names, and payload version validation |
| [collections](collections/README.md) | Arrays, maps, enums, Unicode, embedded NUL, and integer boundaries |
| [complex](complex/README.md) | Shared schemas, includes, composed parents, orders, customers, payments, and audit unions |
| [conversions](conversions/README.md) | Typed record projections, defaults, nested copies, and generic conversions |
| [document_collections](document_collections/README.md) | Fixed arrays and memory-only lookup data |
| [generics](generics/README.md) | Nested applications, typed edits, and four native protocols |
| [interoperability](interoperability/README.md) | Typed producer/consumer fixtures across all four protocols |
| [optional](optional/README.md) | Scalar, record, nested, and collection presence |
| [union_variant](union_variant/README.md) | Raw alternatives and owning string/collection variants |
| [versioning](versioning/README.md) | Historical reads, explicit replacement migration, and revision types |

The [custom-ID Go module](../custom_ids/go/README.md) and
[managed wire-record Go client](../managed/multilanguage/clients/go/README.md)
use the same layout. The latter defaults to the history record schema and
exchanges record codecs; native Go managed engines are not implemented.

CMake is an optional wrapper around this native workflow. It can supply the
Serializer executable and invoke Go generation/build/test commands; see
[individual CMake builds](../README.md#build-an-individual-example-with-cmake).
Use `SERIALIZER_EXAMPLE_COMPILER` for an explicit executable path; otherwise the
wrapper reuses a parent compiler target, a requested installed package, a CLI on
`PATH`, or a source-build fallback. The Go wrapper requires no Python and its
CTest entry runs native Go package tests.

The repository's Python runners are independent qualification tools. From the
repository root, for example:

```sh
python example/run.py --compiler <serializer-executable> --language go --example basic
python example/generics/run.py --compiler <serializer-executable> --language go
```

They copy the maintained Go project into an ignored validation directory,
regenerate its `internal/model` package, and compare complete decoded values
against independent fixtures. The interoperability runner supplies producer
manifests and checks producer/consumer exchanges. These validation copies do
not replace the native source project or overwrite its generated package.
See [SDK and matrix options](../README.md).
