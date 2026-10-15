# Go generic example

With Go 1.22+ and the installed `serializer` executable on `PATH`, run from this
folder:

```sh
go generate .
go build -o build/ .
go test ./...
```

The [entry schema](model.serializer) generates the ignored
`internal/model/model_gen.go` package imported by [main.go](main.go). No CMake,
Python, C++ runtime, or cgo is needed for generation and native builds.
See the [Go project guide](../README.md) for the maintained module layout.
Native tests construct an independent nested fixture, compare complete results
after the payload edit, and reject malformed messages across all four protocols.

The application accepts `<input.json> <output.json>`. It decodes `example_model`,
increments its revision, and changes `payload.value.key` to `7`. The payload is
`box<pair<uint32, string>>` inside `envelope<T>`; arrays and maps retain their
independent values, including full-width integers, UTF-8, and NUL. Generated
schema applications are concrete types; the consumer uses the named root and
typed nested fields rather than compiler-owned instance names.

For independently constructed fixtures and four-protocol verification, run the
optional qualifier from the repository root:

```sh
python example/generics/run.py --compiler <serializer-executable> --language go
```

It prepares an isolated validation copy, generates its internal model package,
and checks `revision=10`, `payload.value.key=7`, and every other fixture field.
Successful qualification prints `PASS: 1 generic consumers, all four protocols`.
See the [shared generic guide](../../generics/README.md) for matrix options.
