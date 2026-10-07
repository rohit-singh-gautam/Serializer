# Serializer compiler and schema versions

The compiler and runtime release is **1.3.0**, defined by `project(... VERSION ...)` in the
root CMake file. `serializer --version` (or `-v`) prints that release and the
supported schema language version. CMake generates `<rohit/version.hpp>` with
`rohit::serializer::compiler_version`, `schema_language_version_text`, and
`schema_language_version_major`, `schema_language_version_minor`, and
`schema_language_version_patch` constants. The original unsigned
`schema_language_version` API remains the language major for compatibility; installed
packages also provide `SerializerConfigVersion.cmake` for versioned discovery.
Package discovery accepts earlier minor and patch releases within the same major
version; requests for newer releases or a different major version are rejected.
Use `EXACT` to require a specific release.

Releases use `major.minor.patch`. Bug fixes and minor updates increment the third
(patch) number. New features increment the second (minor) number and reset patch
to zero. Both preserve full compatibility for existing schemas, public APIs,
generated code, and serialized formats. Only developers change the first (major)
number manually, resetting minor and patch to zero; compatibility across major
releases is best effort and breaking changes require documented migration steps.
The schema language version and payload revisions remain separate version domains.

## Schema files

Use the `.serializer` extension and start each file with:

```text
serializer version 1.1.0;

class account stable_ids {
  public uint32 id (1);
}
```

The statement must precede every declaration. Whitespace and `//` or `/* ... */`
comments may precede it or separate its keywords. The version has exactly three
unsigned decimal components separated by dots, without whitespace or comments
inside the version. The supported language is **1.2.0**, accepting older contracts
in the same major version. The original
`serializer version 1;` is an exact alias for `serializer version 1.0.0;`, exclusive
to version 1; future integer majors such as `2;` are not aliases for `2.0.0;`.
Prerelease and build labels are not supported. Missing, malformed, overflowing,
repeated, misplaced, or unsupported
version statements are rejected. The header selects the schema language grammar;
it is not a minimum compiler release requirement or a serialized message header.
It does not change field IDs, wire names, encoding, or runtime compatibility.

The executable rejects `.def`, `.struct`, and other input extensions. Rename old
schemas and add the header, then update build references. Library callers using
`parser::parse(input)` may continue parsing headerless fragments; any supplied
header is validated. Use `parser::parse(input, true)` to require the file header.

Unquoted directives such as `include common;` are allowed after the
header and before declarations. Paths are relative to the including file; a filename
without an extension resolves to `.serializer`. Explicit `include common.serializer;`
also works. CLI input filenames still require `.serializer`. Each
dependency has its own version header; repeated files are loaded once and their
declarations join the entry schema's output. See [schema includes](usage.md#share-declarations-with-includes)
for path restrictions, namespace handling, duplicate detection, and output limitations.
Library callers use `parser::parse_file(path)` for includes; its result owns the
combined `statements` and records canonical `dependencies` (including the entry).
Existing stream-only parsing does not read files.

### Schema language version policy

The repository's [schema language versioning policy](../AGENTS.md#schema-language-versioning)
uses `major.minor.patch` for language releases, independently of the compiler
release. `serializer_schema_language_version` in the root CMake file defines the
language release separately from `project(... VERSION ...)`. Compiler **1.3.0**
currently supports language **1.2.0**; `serializer --version` reports both.

Language `1.2.0` adds `compact_prefix` and `compact_varint` unsigned scalar
annotations with strict/lenient policies. Each declaring file requires `1.2.0`.
See [compact encodings](compact_integers.md).

Language `1.1.0` adds `array[] T` extent inference from a nonempty initializer and
typed scalar/enum `magic`. Both require a `1.1.0` or newer compatible header in
their declaring file, including included files. A newer entry file does not enable
new syntax in a dependency that declares `1.0.0` or the original `1` alias.
See [fixed arrays](generics.md) and [typed magic](magic_and_omission.md).

Compatible language bug fixes and minor updates increase patch; new language
features increase minor and reset patch to zero. Only developers change major
manually, with best-effort compatibility and migration guidance across major
versions. Minor and patch language releases require full backward compatibility
for existing schemas, their meaning, generated public APIs, and serialized formats.
Compiler-only changes do not automatically increase the language version.

The header selects the required language contract. Newer compilers accept older
same-major contracts; older compilers may
reject newer headers. Versions compare numerically by component, and unsupported
future versions receive a clear diagnostic. Features must be available in the
declared language version, including in included files. The `1` alias always means
the `1.0.0` contract, even after later minor/patch language releases. The language
header is independent of application payload versioning and does not alter wire bytes.

## Options

Run `serializer --help` or `-h` for all accepted names. Options use `--name value`,
`--name=value`, or the documented short form `-x value`. Long names are case
sensitive. Short option clusters, positional arguments, and the previous bare
`input file output file` syntax are rejected. Quote paths containing spaces.
Values starting with `-` require the `--name=value` form. Unknown options, missing
values, and repeated non-repeatable options are errors.

| Option | Short form | Meaning |
| --- | --- | --- |
| `--input` | `-i` | Required `.serializer` schema |
| `--version-policy-as-of` | | Optional `YYYY-MM-DD` reference date; defaults to UTC today captured once per invocation |
| `--version-policy-warnings-as-errors` | | Fail before writing output when current exceeds a release time policy |
| `--verbose` | | Report resolved release-policy dates and minimum versions |
| `--check-against` | | Previous schema for read-only compatibility checking; no code generation |
| `--compatibility-protocol` | | Required for checking: `binary_none`, `binary_integer`, `binary_string`, `json`, or `protobuf_binary` |
| `--compatibility-direction` | | `backward` (new reader), `forward` (old reader), or `both` (default) |
| `--compatibility-policy` | | Optional version-1 JSON reservations file |
| `--output` | `-o` | Output file for exactly one selected language |
| `--depfile` | | Optional Make-style dependency file for all selected outputs and their transitive schema/configuration inputs |
| `--config` | `-c` | Optional generator INI configuration |
| `--language` | `-l` | `cpp`, `java`, `js`, `typescript`, `go`, `csharp`, `rust`, `python`, `swift`, `kotlin`, `c`, or a comma-separated list; repeatable |
| `--cpp.output` | | C++ `.h`, `.hpp`, or `.hxx` destination |
| `--java.output` | | Java `.java` destination; filename supplies the outer class |
| `--js.output` | | JavaScript ES module `.js` or `.mjs` destination |
| `--typescript.output` | | Companion `.d.ts` or `.d.mts` declarations, using JS naming |
| `--go.output` | | Standalone Go `.go` destination |
| `--csharp.output` | | C# `.cs` destination; filename supplies outer class |
| `--rust.output` | | Standalone Rust `.rs` destination |
| `--python.output` | | Standalone Python `.py` destination |
| `--swift.output` | | Standalone Swift `.swift` destination |
| `--kotlin.output` | | Standalone Kotlin/JVM `.kt` destination |
| `--c.output` | | Standalone C11 `.h` destination |
| `--kotlin.package` | | Optional JVM package; empty clears it |
| `--js.naming`, `--go.naming`, `--csharp.naming` | | `profile` or `preserve` |
| `--go.package` | | Go package name, default `generated` |
| `--csharp.namespace` | | C# namespace, default `SerializerGenerated`; empty clears it |
| `--cpp.coding_standard` | | `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, `qt` |
| `--cpp.naming` | | `profile` or `preserve` |
| `--cpp.format` | | `true` or `false` |
| `--cpp.constant_evaluation` | | `true` or `false` (default); add the separate constexpr positional traversal |
| `--cpp.protocols` | | `all` (default) or `binary_none`; restrict generated C++ codecs independently of constant evaluation |
| `--cpp.emission_only` | | `true` or `false` (default); borrowed models with only concrete positional output; requires enabled constant evaluation and binary_none protocols |
| `--managed.id_type` | | Document-local persistent ID width: `uint32` (default) or `uint64`; overrides `[managed] id_type` |
| `--managed.separate_values` | | `true` generates ID-free values plus managed wrappers; `false` (default) puts IDs on the schema classes |
| `--cpp.protobuf` | | `true` or `false`; enable compile-time [Protobuf codecs](protobuf.md) |
| `--cpp.clang_format` | | clang-format 19+ executable |
| `--cpp.format_file` | | Custom layout file; `--cpp.format_file=` clears a configured file |
| `--java.coding_standard` | | `serializer`, `google`, `oracle` |
| `--java.naming` | | `profile` or `preserve` |
| `--java.package` | | Package name; `--java.package=` clears a configured package |

Defaults are C++ output, Serializer presentation, and profile naming. Config values
override defaults; CLI values override config regardless of argument order. An
explicit CLI language selection replaces the config selection. Config paths are
relative to the configuration file; command-line paths are relative to the working
directory. `[output] language = cpp, java` also selects both languages. Duplicate
or unknown languages and empty list entries are rejected.

Compatibility options require `--check-against` and cannot be combined with
generation/configuration/output options. Both schema revisions resolve their own
relative includes. The checker compares wire identities, shapes, positional order,
enum/union ordinals, and persistent reservations; it writes diagnostics only.
Exit 0 means no incompatibility detected in the selected direction, 2 means a
compatibility hazard, and 1 means invalid input or I/O failure. See
[schema compatibility checking](schema_evolution.md) for policy syntax, conservative
limits, and the distinction between native unknown-field rejection and Protobuf
binary skipping.

Depfiles use absolute paths with Make escaping, including paths containing spaces
and Windows drive letters. Their parent directory must exist. They cannot overwrite
schemas, configurations, or generated outputs. Parse, validation, and backend failures
preserve existing outputs and depfiles; filesystem write failures may leave partial output.

Single-language invocation:

```sh
serializer -i example/java/round_trip/account.serializer -l java -o AccountSchema.java --java.coding_standard oracle --java.package example.models
```

Generate both backends from one parse, independently selecting their styles:

```sh
serializer --input example/java/round_trip/account.serializer --language cpp --language java --cpp.output account.hpp --java.output AccountSchema.java --cpp.coding_standard google --java.coding_standard oracle --java.package example.models
```

`--language cpp,java` is equivalent to the two language options. Multiple languages
require one explicit output per language. For a single language, use either
`--output` or that language's output option, never both. Output options for
unselected languages are rejected. Backend settings may remain in a shared config
for languages not selected in a particular run.

Destination directories must already exist. All paths are validated and all
selected sources are generated/formatted before any output is written. Schema,
option, or backend failures leave existing destinations unchanged. Filesystem
write failures are reported but writes across multiple files are not atomic.
Outputs cannot alias each other, the input schema, the INI file, or custom format
file. See [output configuration](output_configuration.md) for profile scope.

## Constant-evaluation C++ generation

Release 1.4.0 adds independent constant-evaluation/protocol settings and an optional
borrowed emission-only profile:

```sh
serializer --input payload.serializer --output payload.hpp --cpp.constant_evaluation true --cpp.protocols binary_none
```

The INI keys are `[cpp] constant_evaluation = true` and
`protocols = binary_none`. Booleans accept exactly `true`/`false`;
protocols accept exactly `all`/`binary_none`. Defaults remain false/all.
CLI overrides INI regardless of argument order, including explicit CLI false.
Repeated scalar CLI options or invalid values are errors.

Constant evaluation adds `serialize_constant_out` and static metadata to owning
models without changing their regular runtime traversal. `protocols = all`
retains other runtime formats. Positional-only output rejects Protobuf enablement,
managed models and view models. It retains owning positional runtime readers/writers,
field identities and wire bytes. See the [constant-evaluation guide](constant_evaluation.md)
for the support matrix, APIs, and actual compiler qualification. Neither option
changes the schema language or payload revision.

Add `--cpp.emission_only true` / `[cpp] emission_only = true` for
namespace-isolated borrowed DTOs with only
`constexpr void serialize_out(binary_none_output&) const`. The option defaults
to false and requires constant evaluation true, protocols binary_none and Protobuf
false. It additionally rejects maps, nonempty array defaults and multidimensional
arrays. String fields become string_view, arrays become span<const T>, and active
fixed-array fields check their declared extent. The concrete writer counts by default
or writes a caller span; no input methods are generated. See
[emission-only output](constant_evaluation.md#94-emission-only-borrowed-models).

## Parser provenance

The internal parser adapts the descriptor-driven short/long option approach from
[Chaturanga's commandline.h](https://github.com/rohit-singh-gautam/Chaturanga/blob/f6b2d270d4950ed5d92433e79e7060df15f351cd/include/commandline.h)
and [commandline.cpp](https://github.com/rohit-singh-gautam/Chaturanga/blob/f6b2d270d4950ed5d92433e79e7060df15f351cd/commandline.cpp),
by Rohit Jairaj Singh, under GPL-3.0-or-later. It keeps the option descriptors and
generated help, replacing raw type-erased storage with owned string values and
adding strict validation, `--name=value`, and repeatable language options.
Serializer performs typed backend validation after merging config and CLI values.
It has no network/build dependency on the upstream repository.

The typed declaration API also draws on the declaration style in
[MicroMonolithServer's configparser.h](https://github.com/rohit-singh-gautam/MicroMonolithServer/blob/main/core/include/mms/cmd/configparser.h).
It owns variant values instead of retaining untyped pointers to caller variables.
It has no dependency on that project's type definitions.

## Verification

Verified on Windows with MSVC and Java 17:

- Compiler/library build, migrated schemas, all nine C++ profile examples, and all
  Java examples compile successfully.
- All 16 CLI, example, and interoperability CTest entries pass. The compiler CLI
  check covers aliases, equals syntax, repeated/comma-separated languages, config
  precedence, per-language overrides, path validation, header failures, and
  preserving existing files when either backend fails.
- All 12 targeted schema-version, output-options, and Java-writer unit tests pass,
  including every truncated prefix of the version header.
- Local installation, exact-version CMake package discovery, generated version
  constants, and both installed generation helpers pass a separate consumer smoke
  build using a shared multi-language configuration.

That historical implementation run retained four failures:
`serialize_parser.identifier`, `serialize_parser.hierarchical_identifier`,
`serialize_parser.access_type`, and `binary_view.edits_preserve_the_encoded_layout`.
The later [verification record](verification-2026-09-17.md) records a passing full
suite for its identified snapshot. No performance qualification is claimed here.

## Portable native output

`--language cpp,java,js,typescript,go,csharp` generates all five runtime languages
and JS declarations from one parsed schema. Supply a distinct language-specific
output path for each. JS/Go/C# generation is implemented in the C++ compiler and
does not launch a target SDK. Their SDKs are only needed by applications and
optional tests. See [portable languages](portable_languages.md) for native APIs,
configuration, default construction, and common-schema limitations.

Add `rust,python,swift,kotlin,c` and their output paths to generate the additional
native languages from the same parse. All backends are C++ implementations.
See [new native targets](native_languages.md) for APIs and limitations.
Internally, scalar CLI reads use `cli::first(parsed, name)`, which borrows the
first value with checked lookup; repeated values remain available in `parsed`.
No second, flattened argument map is constructed.

## Managed C++ generation

Bare `managed` class/member annotations generate classes with direct persistent
IDs and transaction editors by default. Opt into ordinary classes plus managed
companions with `--managed.separate_values true`. See the [managed interface](managed/cpp_runtime.md)
for eligibility and representation rules. Configuration accepts `[managed]` with
`id_type = uint32|uint64` and `separate_values = true|false`. Command-line settings
take precedence. Every output sharing a model must use the same ID width and
representation. Non-C++ managed generation
and capability selectors are rejected; they never silently produce ordinary output.

## Typed command-line declarations

The source header `src/command_line.hpp` provides an owning declaration API.
It is a source-level tool utility, not an installed runtime header. This example
uses only public library types; no application-specific parameter class is needed:

```cpp
namespace cli = rohit::serializer::cli;
cli::commandline_declaration decl{
    cli::command_options{
        {'l', "log_file", "path", "Log output", cli::command_type::path,
         std::filesystem::path("application.log")},
        {'w', "wait", "Wait for more input", cli::command_type::boolean}},
    {{"build", cli::command_options{
        {'i', "input", "path", "Input file", cli::command_type::path, {}, true},
        {'n', "count", "number", "Iteration count", cli::command_type::unsigned_integer,
         std::uint64_t{1}},
        {0, "include", "path", "Include directory", cli::command_type::paths}},
      "Build an input"}}};

decl.parse(argc, argv);
if (decl.help_requested()) {
  decl.write_help(std::cout);
  return 0;
}
const auto& options = decl["build"];
const auto& input = options.get_path("input");
const auto count = options.get_uint64("count");
const auto& includes = options.get_paths("include");
const bool wait = decl.common().get_bool("wait");
```

`option_declaration` takes short name (`0` for none), long name, value placeholder,
help text, `command_type`, optional typed default, `required`, and `allow_empty`.
The last two flags default to false. Boolean declarations also accept the shorter
`{short_name, name, help, command_type::boolean}` form. Defaults must match the
exact declared type; use `std::int64_t`/`std::uint64_t` for integer defaults.

| Type | Getter | Stored value |
| --- | --- | --- |
| `boolean` | `get_bool` | `bool` |
| `integer` | `get_int` | `std::int64_t` |
| `unsigned_integer` | `get_uint64` | `std::uint64_t` |
| `real` | `get_double` | Finite `double` |
| `string` | `get_string` | Owned `std::string` |
| `path` | `get_path` | Native `std::filesystem::path` |
| `strings` | `get_strings` | `std::vector<std::string>` |
| `paths` | `get_paths` | `std::vector<std::filesystem::path>` |

Values and defaults use `std::variant`; missing optional scalar values use
`std::monostate`. String/path/list getters borrow const references, while numeric
and boolean getters return values. No getter reparses text or coerces another
type. Unknown names throw `std::out_of_range`; wrong getter types and absent
optional values throw `std::logic_error`. Lookups never insert declarations.
`has_value(name)` tests availability; `was_provided(name)` tests explicit CLI
presence independently of defaults. Omitted flags default to false, and lists to
empty. Explicit list occurrences replace the default list and retain CLI order.

Parsing consumes `main` or native Windows `wmain` argc/argv directly. No temporary
argument vector is required. Narrow path input is UTF-8; wide Windows paths remain
native, while wide string/tail values are converted to UTF-8. Numeric tokens must
be fully consumed, representable decimal values; non-finite real values fail.
Each successful parse resets defaults and replaces the previous result. Failure
preserves the previous result, command selection, and borrowed references.
Successful parsing, assignment or destruction invalidates borrowed references.
Concurrent const reads are safe; parsing requires exclusive access.

Common options may appear before or after the command and are accessed through
`decl.common()`. Command-specific options follow the command name.
`decl.selected_command()` returns that name (empty for global help/options-only
programs). Only common and selected-command requirements are checked. A required
option must be explicitly supplied even if it has a default. Unknown options,
duplicate scalar occurrences, missing/empty disallowed values, and missing
required inputs throw `std::invalid_argument` before dispatch.

Both `--name value` and `--name=value` are supported, as are `-n value` and
`-n=value`. Short-option clusters are not supported. Flags take no value and
presence sets true. For dash-leading string/path values use equals syntax.
Signed numeric values may use either form. Duplicate declaration names/aliases,
reserved `help`/`h`, wrong default types, and common/command option collisions
are rejected when constructing the declaration.

A `command_options` collection also accepts a `positional_declaration` and a
`tail_declaration` after its option list. Empty names disable them:

```cpp
cli::command_options objects{{}, {"objects", cli::command_type::paths, 1}};
cli::command_options run{{}, {}, {"compiler and arguments", 1}};
```

Positionals support `paths` or `strings` and a minimum count; retrieve them with
`positional_paths()` or `positional_strings()`. An enabled tail retains every
token after `--` unchanged in meaning, including empty values and option-like
strings; retrieve it with `command_tail()`. Without a tail, `--` ends option
parsing and sends remaining tokens to declared positionals. Undeclared extra
inputs fail. Common positionals/tails are supported for options-only programs;
they cannot be combined with subcommands.

Global `--help`/`-h` and `command --help` set `help_requested()` without
requiring missing values. `write_help` renders common options, commands, or the
selected command's options, including defaults, required markers and repeated
inputs. Help does not execute handlers; malformed supplied options still fail.

`command_entry` takes name, options, optional help text and an optional
`int (*handler)(const command_options&)`. After checking help, call
`cli::dispatch_command(decl)` to invoke the selected handler with typed values.
Dispatch never receives argv and fails if parsing has not succeeded or the selected
command has no handler. Alternatively, access `decl[command_name]` directly.
An options-only application constructs `commandline_declaration{options}` and
reads `decl.common()` after parsing.

The original `commandline_option`, `cli::parse(argc, argv, descriptors)`,
`cli::first`, `cli::usage`, and callback-based `cli::parse_into` APIs remain
available for the compiler and existing callers. Their string-map behavior is
unchanged; new applications should use typed declarations.

Release-date and nested acceptance policies are folded entirely by the schema compiler. See [release policies](versioning.md#release-dates-and-compile-time-policies) for composition, calendar arithmetic, warnings, and reproducible generation. The same options apply to `--check-against`.
