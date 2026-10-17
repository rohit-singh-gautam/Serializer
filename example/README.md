# Serializer examples

## XML and packages

The [generated XML examples](xml/README.md) use one schema/fixture across all eleven
outputs and exchange documents between producers and consumers. The
[C++ package example](cpp/xml/README.md) also exercises whole-object compression
streams, mixed XML/binary ZIP parts and OPC discovery. See the
[pending report](../docs/xml_pending.md) for format/backend completion gates.

## Owning record APIs

Compiler/runtime **1.19.0** and language **1.14.0** add four focused consumers:

| Example | Demonstrates |
| --- | --- |
| [Readonly getters](cpp/readonly_getters/README.md) | Private storage, borrowed const-reference getters, owner lifetime, friendship and unchanged native bytes. |
| [Construction defaults](cpp/construction_defaults/README.md) | Direct initialization of nonassignable inputs, fresh fixed-array/nested unit-Z defaults and custom-coordinate decode parity. |
| [Reciprocal quantities](cpp/reciprocal_quantities/README.md) | Two-pass constructor signatures, matching visible constexpr host definitions and codec/default construction. |
| [Arithmetic operators](cpp/arithmetic_operators/README.md) | Explicit typed mappings, owning results, constant evaluation and shared checked rejection behavior. |

Repository test builds generate, compile and execute these examples. Each folder
also has a CMake entry point for installed-package consumers. See
[owning API contracts](../docs/schema/owning_api.md) for syntax and backend limits.

## Custom ID registries

The dedicated [custom ID example](custom_ids/README.md) contains one scoped,
type-tagged record-ID schema and maintained application-owned registry consumers
in all eleven output languages. It demonstrates binding, duplicate rejection,
rebind preserving logical identity, retirement without reuse, import validation,
and four codec round trips. Its [C++ managed companion](custom_ids/managed_cpp/README.md)
uses generated `model_store` registries, immutable snapshot pins and undo/redo.

```powershell
python example/custom_ids/run.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --language all --wsl-languages rust,swift
```

## Custom C++ code

The [custom C++ example](cpp/custom_code/README.md) demonstrates a `cpp preamble`,
access-qualified native class declarations, inline `readonly` and `edit` bodies,
and a method defined in an application `.cpp` file. It builds through CMake and
exercises all four generated native protocols, exact-message decoding, and
overflow rejection. See [schema behavior](../docs/schema/behavior.md) for target tags
and external-definition contracts.

## Record conversions

The [conversion schema](schemas/conversions/model.serializer) declares exact
projections, inherited fields, nested owned record copies and a generic target.
Every language folder contains a maintained `conversions` executable; C++ also
checks explicit casts and C exercises failure cleanup. Run all eleven SDKs with:

```powershell
python example/run.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --example conversions --language all --wsl-languages c,rust,swift
```

On Linux use the corresponding compiler/runtime paths and omit the WSL option.
The shared runner executes conversion consumers without fixture arguments; each
consumer checks its conversion behavior directly. `--example all` includes these
checks alongside the fixture-based codec examples.
See [conversion rules](../docs/schema/conversions.md) for supported value shapes.

## Derived sizes and stateless layout

The [derived-size schema](schemas/derived_sizes/model.serializer) demonstrates
stored and wire-only headers, total and payload scope, nested record spans,
different headers per protocol, complement omission and magic framing.
The [stateless schema](schemas/stateless_layout/model.serializer) demonstrates
constrained generic defaults, forwarding, concrete empty types and ordinary
active transient state. Both require schema language 1.9.0. Their runners generate,
compile and execute fresh examples in every selected language:

```powershell
python test/auto_size_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/auto-size-examples --wsl-languages c,rust,swift
python test/stateless_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/stateless-examples --wsl-languages c,rust,swift
```

The size runner executes both class-specific INI policies: `verify` and `retain`.
It also checks source preservation, structural failures and bounded decoding.
See [size controls](../docs/schema/derived_sizes.md),
[stateless constraints](../docs/schema/generics.md#stateless-type-constraints), and
[qualification](../docs/verification/verification-deferred-features-2026-10-11.md).

The [generic schema examples](generics/README.md) demonstrate `result<T>`, multiple
parameters, nested applications, arrays/maps, named roots, and C++ template aliases.
They include a CMake C++ executable, dedicated `generics` consumers in every
language folder, and a runner for all eleven language backends.

## Union and variant

The [union and owning variant schema](schemas/union_variant/model.serializer)
contrasts raw numeric/class alternatives with owning string, array, and map
alternatives. Every language has a `union_variant` example that exercises all
alternatives and all four protocols. Its root declares
`public version uint32 (1) { 1 };`, so generated codecs validate the
payload revision and reject future version 2. The fixture's version stays unchanged.
All eleven JSON readers verify rejection; C++ and JavaScript/TypeScript additionally
check every native encoder rejects an unsupported revision. The C++ example also
constructs, copies, and replaces generated `std::variant` alternatives directly.
Run them with:

```sh
python example/run.py --compiler build/serializer --example union_variant --language all
```

The schema requires Serializer 1.9.0 and language version 1.4.0. C++ `union`
retains its trivial-lifetime requirement; `variant` owns its active alternative
and derives the generated discriminator from its active storage. Both use the
same positional alternative indices and generated JSON alternative keys.

Start with the [eight C++ point examples](managed/README.md) for schema-generated
managed points, transaction callbacks, generated editors, and history.
Each has a separate folder and a short walkthrough with expected output.

The optional [C++ managed draft ledger](managed/ledger/README.md) demonstrates schema-driven
`managed` declarations, generated editors, three transaction forms, undo, and
snapshot save/load. Enable `SERIALIZER_BUILD_MANAGED` to build it. Hollow-cylinder
and wordpad schemas in their own subfolders are exercised by the integration tests.

Each language has runnable examples in its own folder:

- [C++](cpp/README.md)
- [Java](java/README.md)
- [JavaScript](javascript/README.md)
- [TypeScript](typescript/README.md)
- [Go](go/README.md)
- [C#](csharp/README.md)
- [Rust](rust/README.md)
- [Python](python/README.md)
- [Swift](swift/README.md)
- [Kotlin](kotlin/README.md)
- [C](c/README.md)

`basic`, `collections`, `complex`, `interoperability`, `versioning`, and `union_variant` exercise all four native
protocols. The [complex contract](schemas/complex/model.serializer) spans 13
schema files with nested and diamond includes, reopened namespaces, inheritance,
arrays, maps, enums, and audit unions. The [shared fixtures](schemas/) are the
same for every language; the C++ compiler produces all selected outputs from
one entry schema. Qualification outputs stay in the build tree. Native Go source
projects keep their ignored generated package under `internal/model`.

## Build an individual example with CMake

Each individual example has a short `CMakeLists.txt`. With CMake 4.0+, configure
its directory directly from the repository root:

```sh
cmake -S example/cpp/basic -B out/build/cpp-basic
cmake --build out/build/cpp-basic --config Release
ctest --test-dir out/build/cpp-basic -C Release --output-on-failure
```

Replace `cpp/basic` with another language and example, such as `java/collections`,
`python/complex`, `rust/conversions`, or `c/interoperability`. C and C++ consumers
are native CMake executable targets. Go delegates to its maintained native
project as described below. Other languages use their existing SDK
commands through a CMake build target; the build also executes their fixture
checks. CTest runs the selected example's checks. Generated code, SDK projects,
executables and verification output for those wrappers stay in the chosen build
directory. The Go wrapper keeps generated model code in the source module's
ignored `internal/model` directory and writes its executable in the chosen build
directory.

Go examples keep a complete native project: `go.mod`, `main.go`, `generate.go`,
the entry schema, and an imported `internal/model` package. With the installed
`serializer` CLI on `PATH`, work directly in a Go folder:

```sh
cd example/go/basic
go generate .
go build -o build/ .
go test ./...
```

No CMake, Python, C++ runtime, or cgo is needed for this workflow. Go does not
run generation automatically during a build or test. The optional CMake wrapper
invokes the same native commands. The Python runners below remain independent
matrix qualifiers: they copy the maintained project and regenerate the internal
model package in an isolated validation directory. See
[native Go builds](go/README.md) for the layout and each program's arguments.

The Go wrapper accepts an explicit CLI path:

```sh
cmake -S example/go/basic -B out/build/go-basic -DSERIALIZER_EXAMPLE_COMPILER=/path/to/serializer
cmake --build out/build/go-basic --config Release
ctest --test-dir out/build/go-basic -C Release --output-on-failure
```

Otherwise it reuses a parent compiler target, a requested installed package, or
`serializer` on `PATH`, with a source-build fallback. Its native Go package tests
do not require Python. The managed wire `clients/go` wrapper builds the history
client by default; the managed per-feature CMake directories remain explicit
cross-language qualification harnesses and require Python.

Source dependency builds, the default for non-Go wrappers, build the schema
compiler from this checkout and disable unrelated tests, managed runtime and
compression dependencies unless explicitly enabled. Source bootstrap requires a
C++20 compiler; the shared
language checks and SDK runners also require Python 3.10+ and the selected SDK
listed below. C++ generation for the shared language examples uses
an explicit configuration with formatting disabled; specialized style and API
examples retain their documented formatter requirements. On Windows, use Visual
Studio Developer PowerShell for SDK runners that invoke MSVC directly.

To reuse an installed Serializer compiler and runtime:

```sh
cmake -S example/cpp/basic -B out/build/cpp-basic-installed \
  -DSERIALIZER_EXAMPLE_USE_INSTALLED=ON -DCMAKE_PREFIX_PATH=/path/to/serializer
cmake --build out/build/cpp-basic-installed --config Release
ctest --test-dir out/build/cpp-basic-installed -C Release --output-on-failure
```

On PowerShell, put the configure arguments on one line or use its backtick line
continuation. Managed examples require a package with `SERIALIZER_BUILD_MANAGED=ON`;
compression examples require their selected backend. Pass
`-DSERIALIZER_EXAMPLE_WSL_LANGUAGES=rust,swift` for explicitly selected WSL SDK
runners on Windows. Missing SDKs are errors; they are never silently skipped.

Schema-only folders such as `config`, `managed/design`, and `managed/wordpad`
generate C++ headers without an executable. Managed wire feature folders accept
`SERIALIZER_EXAMPLE_LANGUAGES` as a CMake list and run the Python qualification
harness. Their non-Go `clients/<language>` folders select that language across
all four managed wire fixtures. The native `clients/go` project and its optional
CMake wrapper select history; use the per-feature qualifier for other records.

## Run

Build `serializer` in Release first, then select any installed target SDKs:

```sh
python example/run.py --compiler build/serializer --language go
python example/run.py --compiler build/serializer --language rust,python,c --example complex
python example/run.py --compiler build/serializer --language all
```

On Windows use `build/serializer.exe` and an MSVC developer environment for C/C++.
Use `--cpp-library` when the matching Serializer library is not beside the
compiler. The runner uses Release builds; pass the matching Release library.

SDKs: C++20 and C11 compilers, JDK 17+, Node.js with ES2020 BigInt, TypeScript,
Go 1.22+, .NET 8+, Rust 2021, Python 3.10+, Swift 6, Kotlin/JVM 2.0+.
Use `SERIALIZER_KOTLINC`, `SERIALIZER_TSC`, `SERIALIZER_JAVA`, etc. to override
SDK executable paths. The runner never downloads SDKs or silently skips one.
Only the selected SDKs are required; the C++ schema compiler needs none of them.
The TypeScript samples include narrow declarations for the Node APIs they use;
applications may use `@types/node` instead.

On Windows, `--wsl-languages c,rust,swift` explicitly builds and runs those targets
with installed WSL SDKs. `--sanitize` enables address/undefined sanitizers with
the GCC/Clang C toolchain. SDK and language baseline details are in the
[native language guide](../docs/languages/native_languages.md).

Add `--big-endian` to `--example interoperability --language all` to include
QEMU s390x C/C++ producers and consumers using the same little-endian wire
profile. On Windows the cross compilers and QEMU run in WSL. See
[cross-endian requirements](interoperability/README.md#different-machine-byte-orders).
The positional fixtures are pinned to shared expected bytes in both matrices.

## CTest

```sh
cmake -S . -B build -DSERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release -L serializer_native --output-on-failure
```

These opt-in tests compile SDK consumers during CTest and run all per-language examples,
the 1,452-exchange matrix, and the five new runtime boundary suites. All SDKs
must be available. Set `SERIALIZER_EXAMPLE_WSL_LANGUAGES` and
`SERIALIZER_EXAMPLE_SANITIZERS` for the corresponding runner options.
Enable `SERIALIZER_EXAMPLE_BIG_ENDIAN=ON` to include the s390x participants.
The existing `SERIALIZER_BUILD_INTEROP_EXAMPLES` option retains the smaller
C++/Java/JS/Go/C# matrix and optional browser test.

## Additional C++ and Java examples


Each example lives in its own folder with its inputs and consumer sources.
Generated output stays in the build tree.

- [Configuration schema](config/README.md): a small C++ schema with inheritance.
- [C++ expressions](cpp/expressions/README.md): schema-declared checked arithmetic
  and predicates called at compile time and runtime, declared namespace/member
  calls, exact integer equality,
  bounded array reductions and indexing.
- [C++ coding styles](coding_styles/README.md): nine independently selectable profiles.
- [Iostream examples](iostream/README.md): seven memory, file, buffered, and custom
  stream examples sharing a large 52-class telemetry schema.
- [Compression examples](compression/README.md): Zstandard, LZ4, gzip,
  zlib, raw DEFLATE, uncompressed output, and a custom backend with explicit limits.
- [Java examples](java/README.md): a runnable pure Java round trip and three style profiles.
- [Schema includes](includes/README.md): three paired C++/Java examples for shared
  types, reopened namespaces, and repeated/diamond includes.
- [Five-runtime interoperability subset](interoperability/README.md): one shared schema,
  independent C++/Java/JS/Go/C# producers and consumers, all 25 language pairs,
  four protocols, a browser example, and optional throughput measurements.


## Versioning

The [shared versioning schema](schemas/versioning/model.serializer) demonstrates all
supported revision types, historical layouts, replacements, and reservations. Each
language has a runnable consumer that reads revision 8 using compatible policy,
checks old positional binary, explicitly migrates to revision 10, and verifies all
four protocols with strict reads:

| Language | Example |
| --- | --- |
| cpp | [Versioning](cpp/versioning/README.md) |
| java | [Versioning](java/versioning/README.md) |
| javascript | [Versioning](javascript/versioning/README.md) |
| typescript | [Versioning](typescript/versioning/README.md) |
| go | [Versioning](go/versioning/README.md) |
| csharp | [Versioning](csharp/versioning/README.md) |
| rust | [Versioning](rust/versioning/README.md) |
| python | [Versioning](python/versioning/README.md) |
| swift | [Versioning](swift/versioning/README.md) |
| kotlin | [Versioning](kotlin/versioning/README.md) |
| c | [Versioning](c/versioning/README.md) |

```sh
python example/run.py --compiler build/serializer --example versioning --language all
```

See the [revision contract](../docs/schema/versioning.md) for policy semantics and support limits.

## Document collections

Every language has a `document_collections` example. Run
`python example/run.py --compiler <serializer> --language all --example document_collections`.
The shared document schema exercises exact arrays of nested records, strings and
integers, transient wire exclusion and rejection of short/oversized arrays.

## Optional values

Every language has a runnable `optional` example using the
[shared generic schema](schemas/optional/model.serializer). It covers absent
and present scalars, generated records, nested optionals, optional arrays/maps,
empty present collections, collections of optionals, and generic `optional<T>`
fields. C++ storage is the actual `std::optional<T>`. The independent fixture
is preserved through all four protocols; invalid JSON presence frames and
unsupported version 2 reject.

```sh
python example/run.py --compiler build/serializer --example optional --language all
```

Use language 1.7.0 and compiler/runtime 1.12.0. See the
[optional contract](../docs/schema/optional.md) for target APIs and wire semantics.
The basic, collections and complex examples use payload `version` metadata,
keep version 1, and verify rejection of future version 2. These supported
payload revisions are metadata; changing them requires a deliberate schema migration.
