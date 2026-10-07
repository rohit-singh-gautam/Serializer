# Serializer

Native JSON supports `std::optional<T>` as a host type (`null` or a typed value).
For evolving JSON documents, use the opt-in `read_policy::flexible`
reader to skip additional fields while rejecting duplicate keys and enforcing
decode limits. Default JSON input remains strict. Regenerate classes to enable
the unknown-member protocol hook; see [JSON compatibility](docs/usage.md#json-compatibility).

Define your data once in a `.serializer` schema, then generate classes and codecs
for JSON and binary serialization. Serializer includes a C++20 schema compiler,
a C++ runtime, and generators for multiple languages.

**Supported outputs:** C++, Java, JavaScript, TypeScript declarations, Go, C#,
Rust, Python, Swift, Kotlin, and C. TypeScript uses the JavaScript runtime.

**Start here:** [Usage guide](docs/usage.md) · [Runnable examples](example/README.md) ·
[CMake integration](docs/cmake_integration.md)

**Using a coding agent?** Give it the
[Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).

Payload [versioning and schema evolution](docs/versioning.md) support `version`,
`compatibility`, `created`, `obsolete`, `replaced`, and class-scoped `reserve` declarations.
Release catalogs and nested allow policies (`any`/`all`, age/count/date limits) resolve
to compatibility bounds during generation; generated codecs contain no release-date
processing. All eleven language outputs share current/historical read policies and versioned
positional binary layouts. See the [versioning examples](example/README.md#versioning).

Schema-owned [magic headers and format omissions](docs/magic_and_omission.md)
use `private magic { 'SRLFILE' };` and `omit(json, binary_positional)`.
These declarations require owning classes; view modes reject them explicitly.
Magic is static metadata: writers emit it, readers verify and discard it, and
JSON includes the fixed `magic` string by default. Native binary writes exact
header bytes before the separate version discriminator. C++ format selection
and omissions resolve at compile time without per-object flags.

## How it works

1. Describe your types, fields, and defaults in a `.serializer` file.
2. Run the compiler, or let CMake generate code during your build.
3. Use the generated classes to read and write your chosen format.

The compiler and generators are written in C++. Generated non-C++ codecs run in
their target language without a native Serializer runtime dependency.

## Get started

### 1. Write a schema

Save this as `person.serializer`:

```text
serializer version 1;

namespace demo {
  class person {
    public string name;
    public uint32 age;
  }
}
```

Every schema begins with `serializer version 1;`. Each field declares its access
level, type, and name. Start with [schema examples](docs/schema_examples.md) for
arrays, maps, enums, and defaults.

### 2. Generate code

After [building or installing Serializer](docs/cmake_integration.md), run the
compiler to generate a C++ header:

```sh
serializer --input person.serializer --output person.hpp
```

Use [output configuration](docs/output_configuration.md) to choose a language or
naming profile. The [command-line guide](docs/command_line.md) lists all options.

### 3. Add it to your build

For a C++ application with Serializer already added as a CMake dependency:

```cmake
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS person.serializer)
```

A normal build generates the header and supplies the include directory, runtime
library, and C++20 requirement. See [CMake integration](docs/cmake_integration.md)
for complete source-dependency and installed-package setups.

Next, follow the [C++ usage guide](docs/usage.md) to serialize a value and decode a
complete message with explicit limits, or run the [basic C++ example](example/cpp/basic/README.md).

## Choose a language

Reusable [schema generics](docs/generics.md), such as `class result<T>`, emit
native C++ templates: application code can use `result<std::uint32_t>` without
concrete schema declarations. Nested templates, trailing defaults, positive uint64
dimensions, and fixed `array[N] T` storage are supported in C++.
Optional `instantiate person_result = result<person>;` declarations and concrete
schema fields define contracts for all eleven generators. Other languages keep
concrete APIs; fixed arrays currently produce explicit unsupported diagnostics
outside native C++ codecs. See the [generic examples](example/generics/README.md)
and [qualification record](docs/verification-dimensions-2026-10-04.md).

All languages use the same schema compiler. Their generated APIs and runtime
requirements are documented separately.

| Output | Guide | Examples |
| --- | --- | --- |
| C++ | [Usage and decoding](docs/usage.md) | [C++ examples](example/cpp/README.md) |
| Java | [Java 17+ codecs](docs/java.md) | [Java examples](example/java/README.md) |
| JavaScript / TypeScript | [Portable language APIs](docs/portable_languages.md) | [JavaScript](example/javascript/README.md), [TypeScript](example/typescript/README.md) |
| Go / C# | [Portable language APIs](docs/portable_languages.md) | [Go](example/go/README.md), [C#](example/csharp/README.md) |
| Rust / Python / Swift / Kotlin / C | [Native language APIs](docs/native_languages.md) | [All language examples](example/README.md) |

The [build and generation guide](docs/build_and_generation.md) includes compiler
commands for each language group and explains naming profiles.

## Choose a format

Serializer supports four native protocols across its generated language codecs.

| Format | How fields are represented |
| --- | --- |
| JSON | Named fields in readable text |
| Positional binary | Values in schema order, without field keys |
| Integer-key binary | A numeric field ID before each value |
| String-key binary | A wire field name before each value |

Native binary codecs default to little-endian byte order. Strings require valid
UTF-8; use `array uint8` for arbitrary bytes. Read the
[wire-format contract](docs/wire_format.md) before exchanging data between systems.

C++ also offers optional [Protobuf binary, ProtoJSON, and TextProto codecs](docs/protobuf.md).
These are separate from the four native protocols.

For evolving schemas, use [explicit field IDs](docs/schema_reference.md#explicit-field-ids-with-stable_ids)
and the [schema compatibility checker](docs/schema_evolution.md). Stable IDs alone
do not make native readers accept unknown fields. Evolving JSON documents can
explicitly select `read_policy::flexible` after regenerating their classes.

## Build and test

Fresh builds enable managed history, journaling, collaboration, SIMD, and all
compression backends. Zstandard, LZ4, and zlib are required; the repository vcpkg
manifest acquires them through its default features. Existing caches retain their
settings; use `-U SERIALIZER_BUILD_MANAGED -U SERIALIZER_WITH_*` to adopt the new
defaults, or set each option explicitly.

The default development build needs CMake 3.28+, a C++20 compiler and standard
library, clang-format 19+, and GoogleTest. See the
[requirements and setup guide](docs/cmake_integration.md#build-this-repository)
for dependency installation and optional tools.

On Linux with GNU Make:

```sh
make all
make test
```

On Windows with PowerShell:

```powershell
./make.ps1 all
./make.ps1 test
```

Windows `all` also packages both editor extensions. It requires Node.js 22+, npm,
and Visual Studio MSBuild; it does not install the packages. The `test` and
`configure` targets remain CMake-only.

For direct CMake commands, presets, builds without GoogleTest, and troubleshooting,
see [build and generation](docs/build_and_generation.md#build-and-test).

## Editor support

Both **Rohit Serializer** extensions highlight `.serializer` files and provide
navigation between schemas and existing generated output in all 11 output languages.
The extensions are separate from the compiler and runtime.

| Editor | What it adds | Setup |
| --- | --- | --- |
| Visual Studio Code | Highlighting, snippets, schema navigation, CMake generation commands, and missing-include assistance | [VS Code guide](docs/editor_extension.md) |
| Visual Studio 2022 / 2026, Windows x64 | Highlighting, editing configuration, native declaration/type-definition navigation, F12, and Ctrl+click | [Visual Studio guide](editors/visual_studio/README.md) |

Both extensions use release version **1.1.22**, independent of compiler version
**1.0.0**. See the [navigation coverage matrix](docs/editor_navigation.md#navigation-coverage-matrix)
for supported destinations and language-service prerequisites.
VS Code also supports navigation from read-only Git index/history tabs, using
the displayed snapshot and current workspace destinations.

## Explore advanced features

Start with these only when your application needs them.

For application editing, start with the
[history, journal, collaboration, and authorization guide](docs/managed/getting_started.md).
It explains what each feature does, how they work together, and what your
application must provide.

| Need | Guide |
| --- | --- |
| Share declarations across schemas | [Includes](docs/usage.md#share-declarations-with-includes) |
| Read or update encoded C++ data through borrowed views | [Buffer views](docs/views.md) |
| Encode directly into an owned `std::string` buffer | [String streams](docs/usage.md#string-backed-output-buffers) |
| Use standard streams or durable files | [Streams and adapters](docs/usage.md#stream-concepts-and-implicit-adapters), [file streams](docs/usage.md#file-streams-and-journal-records) |
| Compress complete C++ messages | [Compression](docs/compression.md) |
| Group edits and support undo/redo in C++ | [Managed runtime](docs/managed/cpp_runtime.md), [small examples](example/managed/README.md) |
| Recover managed state after a crash | [Journaling](docs/managed/journal.md) |
| Synchronize edits between managed stores | [Local collaboration](docs/managed/local_collaboration.md) |
| Control which sessions may publish edits or acquire locks | [Authorization](docs/managed/authorization.md) |
| Understand runtime and generated-code optimizations | [Performance features](docs/performance.md) |
| Plan database persistence | [Database integration](docs/database_integration.md) |

Native managed runtimes outside C++ and Serializer database adapters are not
implemented. The [feature status guide](docs/feature_status.md) separates available
features from proposals and records their limitations.

## Integrate with a coding agent

Give your agent this request, adapting the dependency path:

```text
Read vendor/Serializer/.agents/skills/serializer-integration/SKILL.md
and use it to integrate Serializer into this application's CMake build.
```

The skill covers schemas, generation, codec selection, and bounded decoding.
Keep the tracked `.agents/skills/serializer-integration/` directory when copying
or packaging the source repository.

See [agent discovery and setup](docs/agent_integration.md) for automatic discovery
and use from a consuming project.

## Typed command-line declarations

Use `src/command_line.hpp` and `cli::commandline_declaration` for common options,
subcommands and typed retrieval. Declare `command_options` and `command_entry`
values, call `decl.parse(argc, argv)`, then read `decl["command"].get_path("input")`
or other typed getters. The library owns variant values, validates required
options and numeric conversions, and generates global/command help. It supports
repeatable string/path lists, explicit positional inputs, command tails, and
native Windows argv. Failed reparsing preserves the previous successful result.

Keep option types, defaults and requirements in declarations; handlers consume
typed values without reparsing. Use `decl.common()` for common options and
`was_provided` to distinguish defaults from explicit input. Check
`help_requested()` and write help before calling `cli::dispatch_command(decl)`.
The original compiler string-map API remains compatible. See the
[declaration guide](docs/command_line.md#typed-command-line-declarations) for complete examples, getter types, ownership,
error behavior, and positional/tail syntax. This source utility is not an
installed runtime header.

## Contributing and community

Start with the [contribution guidelines](CONTRIBUTING.md) to report bugs, propose
features, or submit a pull request. Participants follow the
[code of conduct](CODE_OF_CONDUCT.md). Report vulnerabilities privately using the
[security policy](SECURITY.md); see the [accessibility statement](ACCESSIBILITY.md)
for reporting barriers to using the project.

## Reference and project status

- [Schema and protocol reference](docs/schema_reference.md): types, field IDs, views, and protocol examples.
- [Schema examples](docs/schema_examples.md): small declarations you can adapt.
- [Migration guide](migration.md): upgrade requirements and compatibility changes.
- [Feature status and roadmap](docs/feature_status.md): implemented behavior and future work.
- [Default runtime verification](docs/verification-default-features-2026-10-07.md): managed/compression defaults and installed-header checks.
- [Verification record](docs/verification-2026-09-17.md): tested revisions, configurations, and outstanding checks.
- [Versioning verification](docs/verification-versioning-2026-10-06.md): historical layouts, all eleven language examples, and editor checks.
- [Release-policy verification](docs/verification-release-policies-2026-10-06.md): compiler folding, calendar boundaries, nested policies, and generation options.
- [Qualification](qualification/README.md): interoperability, fuzzing, and performance workflows.
- [Coding standard](CodingStandard.md) and [agent instructions](AGENTS.md): repository contribution rules.
- [License](LICENSE).

When updating an existing application, check the migration guide and regenerate
affected output before rebuilding. Verification records describe specific tested
configurations; they do not establish support for every platform or toolchain.
