# Go: custom ID registries

This folder is a native Go 1.22+ module. With the installed `serializer` executable
on `PATH`, run here:

```sh
go generate .
go build -o build/ .
go test ./...
go run .
```

The local [entry schema](model.serializer) includes the shared custom-ID schema.
`generate.go` writes the ignored `internal/model/model_gen.go` package imported by
`main.go`. Rerun generation after editing the schema or its includes. These
commands need no CMake, Python, C++ runtime, or cgo.

The application owns a registry keyed by scope, stable type domain, and local
identity. It checks duplicate rejection, rebinding, retirement, import policy,
and all four codec round trips. A nonzero exit status reports a failed invariant.
Native tests run these checks and reject invalid registry imports, including
reserved or missing identities, duplicate keys, and live/retired overlap.
See [the custom-ID contract](../README.md) and
[Go module layout](../../go/README.md).

The optional repository matrix creates an isolated validation copy:

```sh
python example/custom_ids/run.py --compiler <serializer-executable> --language go
```

Run that command from the repository root. It is an independent qualification
harness, separate from native Go generation and builds in this folder.
