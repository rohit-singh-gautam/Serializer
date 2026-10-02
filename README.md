# Serializer

Define your data once in a `.serializer` schema, then generate classes and codecs
for JSON and binary serialization. Serializer includes a C++20 schema compiler,
a C++ runtime, and generators for multiple languages.

**Supported outputs:** C++, Java, JavaScript, TypeScript declarations, Go, C#,
Rust, Python, Swift, Kotlin, and C. TypeScript uses the JavaScript runtime.

**Start here:** [Usage guide](docs/usage.md) · [Runnable examples](example/README.md) ·
[CMake integration](docs/cmake_integration.md)

**Using a coding agent?** Give it the
[Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).

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
do not make native readers accept unknown fields.

## Build and test

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

Both extensions use release version **1.1.12**, independent of compiler version
**1.0.0**. See the [navigation coverage matrix](docs/editor_navigation.md#navigation-coverage-matrix)
for supported destinations and language-service prerequisites.

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

## Reference and project status

- [Schema and protocol reference](docs/schema_reference.md): types, field IDs, views, and protocol examples.
- [Schema examples](docs/schema_examples.md): small declarations you can adapt.
- [Migration guide](migration.md): upgrade requirements and compatibility changes.
- [Feature status and roadmap](docs/feature_status.md): implemented behavior and future work.
- [Verification record](docs/verification-2026-09-17.md): tested revisions, configurations, and outstanding checks.
- [Qualification](qualification/README.md): interoperability, fuzzing, and performance workflows.
- [Coding standard](CodingStandard.md) and [agent instructions](AGENTS.md): repository contribution rules.
- [License](LICENSE).

When updating an existing application, check the migration guide and regenerate
affected output before rebuilding. Verification records describe specific tested
configurations; they do not establish support for every platform or toolchain.
