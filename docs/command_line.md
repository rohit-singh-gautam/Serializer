# Serializer compiler and schema versions

The compiler version is **0.1.0**, defined by `project(... VERSION ...)` in the
root CMake file. `serializer --version` (or `-v`) prints that release and the
supported schema language version. CMake generates `<rohit/version.hpp>` with
`rohit::serializer::compiler_version` and `schema_language_version`; installed
packages also provide `SerializerConfigVersion.cmake` for versioned discovery.
Package compatibility is limited to the same major/minor release.

## Schema files

Use the `.serializer` extension and start each file with:

```text
serializer version 1;

class account stable_ids {
  public uint32 id (1);
}
```

The statement must precede every declaration. Whitespace and `//` or `/* ... */`
comments may precede it or separate its tokens. The version is a decimal integer;
only `1` is supported. Missing, malformed, repeated, misplaced, or unsupported
version statements are rejected. The header selects the schema language grammar;
it is not a minimum compiler release requirement or a serialized message header.
It does not change field IDs, wire names, encoding, or runtime compatibility.

The executable rejects `.def`, `.struct`, and other input extensions. Rename old
schemas and add the header, then update build references. Library callers using
`parser::parse(input)` may continue parsing headerless fragments; any supplied
header is validated. Use `parser::parse(input, true)` to require the file header.

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
| `--output` | `-o` | Output file for exactly one selected language |
| `--config` | `-c` | Optional generator INI configuration |
| `--language` | `-l` | `cpp`, `java`, or a comma-separated list; repeatable |
| `--cpp.output` | | C++ `.h`, `.hpp`, or `.hxx` destination |
| `--java.output` | | Java `.java` destination; filename supplies the outer class |
| `--cpp.coding_standard` | | `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, `qt` |
| `--cpp.naming` | | `profile` or `preserve` |
| `--cpp.format` | | `true` or `false` |
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

## Parser provenance

The internal parser adapts the descriptor-driven short/long option approach from
[Chaturanga's commandline.h](https://github.com/rohit-singh-gautam/Chaturanga/blob/f6b2d270d4950ed5d92433e79e7060df15f351cd/include/commandline.h)
and [commandline.cpp](https://github.com/rohit-singh-gautam/Chaturanga/blob/f6b2d270d4950ed5d92433e79e7060df15f351cd/commandline.cpp),
by Rohit Jairaj Singh, under GPL-3.0-or-later. It keeps the option descriptors and
generated help, replacing raw type-erased storage with owned string values and
adding strict validation, `--name=value`, and repeatable language options.
Serializer performs typed backend validation after merging config and CLI values.
It has no network/build dependency on the upstream repository.

The other proposed parser uses MicroMonolithServer-specific type definitions.
Chaturanga's standard-library-only interface provides a smaller adaptation.

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

The full unit suite retains four pre-existing failures:
`serialize_parser.identifier`, `serialize_parser.hierarchical_identifier`,
`serialize_parser.access_type`, and `binary_view.edits_preserve_the_encoded_layout`.
No cross-platform or performance qualification is claimed by these checks.
