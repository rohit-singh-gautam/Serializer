# Output languages and coding profiles

Keep output choices in the **generator configuration**, separate from `.serializer` schemas.
Schemas define names, IDs, types, defaults, and representations. Each application can
select a language backend and its presentation rules without changing that contract.
The **C++, Java, JavaScript, Go, C#, Rust, Python, Swift, Kotlin, and C backends** are implemented. The
`typescript` output emits declarations for JS. Other language names are rejected. Java has independent `serializer`, `google`, and `oracle` profiles;
see [Java configuration and profile scope](java.md#java-coding-styles). The C++
configuration below remains the default.

## Configuration and command line

Save a UTF-8 file without a BOM, such as `serializer_output.ini`:

```ini
[output]
language = cpp
include_mode = separate

[cpp]
coding_standard = serializer
naming = profile
format = true
clang_format = clang-format
```

```sh
serializer --input account.serializer --output account.hpp --config serializer_output.ini
serializer --input account.serializer --output account.hpp --config serializer_output.ini --cpp.coding_standard google
```

Precedence is **built-in defaults, configuration file, command-line overrides**.
The position of `--config` among the command-line arguments does not affect precedence.
Options now require leading dashes. Use `--language cpp,java` with `--cpp.output`
and `--java.output` to generate both backends in one run; `[output] language = cpp,java`
also selects both. See [the full command-line contract](../guides/command_line.md), including
short options, version reporting, and required `.serializer` version headers.

For several variants of the same schema, repeat `--config` and supply matching
output paths for each configuration. Each INI is loaded independently over the defaults;
shared CLI overrides apply to each variant. The compiler parses the schema once
and generates all variants before writing any output. The INI syntax is unchanged
and has no named-configuration sections. See
[batch-generation examples and output ordering](../guides/command_line.md#generate-several-configurations-from-one-parse)
and the [CMake variants helper](../guides/cmake_integration.md#generate-several-configurations-from-one-parse).

| Setting | Default | Meaning |
| --- | --- | --- |
| `[output] language` | `cpp` | Select supported backends listed above, individually or as a comma-separated list. |
| `[output] include_mode` | `separate` | C++ schema include layout: `separate` emits dependency headers; `inline` keeps the combined entry header. |
| `[output] protocols` | `all` | Select `all` native codecs or `binary_none` for every selected backend; XML remains independently opt-in. |
| `[cpp] protocols` | Inherit `[output] protocols` | Override native codec selection for C++ alone. |
| `[cpp] coding_standard` | `serializer` | Select a profile from the table below. |
| `[cpp] naming` | `profile` | Rename target identifiers; `preserve` retains schema spellings. |
| `[cpp] protobuf` | `false` | Generate direct Protobuf binary, ProtoJSON, and TextProto codecs; validate the [compatible schema subset](../serialization/protobuf.md). |
| `[xml] enabled` | `false` | Generate the optional typed XML mapping for every selected output. |
| `[xml] xsd` | Unset | Fixed standalone XSD build input, relative to this INI file; unsupported constructs fail generation. |
| `[xml] runtime_xsd` | `false` | Permit an explicitly supplied prepared runtime validator. |
| `[xml] validation_scope` | `input` | Apply a supplied validator to `input`, `output`, or `both`. |
| `[cpp] format` | `true` | Run `clang-format`; `false` emits intermediate source for a caller-managed formatting pipeline. |
| `[cpp] clang_format` | `clang-format` | Executable name on `PATH`, or a path to the executable. |
| `[cpp] format_file` | Unset | Use a custom clang-format YAML file in place of the profile's layout. |

C++ CLI overrides use `--cpp.` plus the setting name, for example
`--cpp.naming preserve` or `--cpp.clang_format "C:/Tools/LLVM/bin/clang-format.exe"`.
Use `--cpp.format_file=` to clear an inherited custom format file.
Output filenames must end in `.h`, `.hpp`, or `.hxx`; `.hpp` is the repository default convention.
`serializer --help` lists the arguments. Unknown/repeated scalar arguments, unsupported
settings, malformed configurations, and failed generation return a nonzero exit code.

XML CLI overrides use `--xml.` followed by the setting name. Without an XSD,
members become children unless `[xml.class.<schema-qualified-name>] attributes`
selects scalar attributes; `root`, `namespace` and `name.<member>` configure names.
A supported fixed XSD is authoritative for placement and order. See
[XML configuration and APIs](../serialization/xml_packages.md) and the
[all-language examples](../../example/xml/README.md).

Use `--protocols binary_none` or `[output] protocols = binary_none` to restrict
native encode/decode entry points across every output. `--cpp.protocols` remains
a C++ override and takes precedence over the common CLI selection. Configuration
section order does not affect this inheritance. Defaults retain all four native
formats and their existing public APIs and wire bytes.

Restricted C++ output constrains the concrete protocol methods at compile time.
Other generated SDKs keep their established protocol-parameter APIs and reject
excluded formats before serializing or decoding an object; TypeScript declarations
also restrict that parameter to the positional protocol literal. Generated record
decoders omit redundant keyed/name dispatch paths where supported. Shared runtime
helpers remain present, so this selection does not promise complete source-level runtime pruning
or a particular code-size reduction. `xml.enabled = true` still generates XML
entry points with this native selection; native selection never enables XML.
The existing C++ restrictions on managed/view models and Protobuf combinations
continue to apply.

INI keys, section names, profile names, and boolean values are case-sensitive.
Each section/key may appear once. Blank lines and whole-line `#` or `;` comments
are allowed. Values may have surrounding double quotes. Backslashes are literal;
there are no escape sequences, inline comments, environment expansion, or includes.
File paths in the config resolve relative to its directory. A bare executable
name is searched on `PATH`; a relative executable path containing a directory is
resolved relative to the config. CLI paths resolve from the current working directory.

## Generated schema include layout

Compiler/runtime 2.0.0 defaults to `include_mode = separate` for C++ generation.
The entry header contains its own definitions and includes generated dependency
headers. Generate the entry schema once; the compiler generates its transitive
dependencies with the same naming, runtime, codec and formatting settings.
Headers may also retain shared forward declarations needed by supported callable
signatures; the declaration guards below protect these reusable declarations.
For `--output out/d.hpp` and same-directory source dependencies `a.serializer`,
`b.serializer` and `c.serializer`, the additional outputs are
`out/d.hpp.includes/a.hpp`, `out/d.hpp.includes/b.hpp` and
`out/d.hpp.includes/c.hpp`. The child directory tree preserves paths relative to
the common canonical ancestor of all source files; child headers use the entry
header's extension. Generated files remain inside the output bundle even when
source includes use `../`.
Canonical source paths must share a filesystem root in separate mode; inline
mode supports include graphs that span different filesystem roots.

Use the INI setting above or `--include-mode separate|inline`. Command-line
overrides apply after the INI file, including to every variant in a batch.
`inline` preserves the previous combined C++ header layout. This setting changes
generated file organization; schema versions, public model names and wire data
remain unchanged. Other language backends retain their combined entry output.
Separate-file generation for those backends is not implemented.

When upgrading a workflow that copies, packages or installs only the entry
header, retain all generated dependency headers too, or select `inline`.
This distribution change is part of the compiler/runtime 2.0.0 major release;
schema language headers remain 1.x. Versioned CMake consumers must also move
their package request to 2.x. See [2.0 migration](../../migration.md#compilerruntime-200).
See [include usage](../guides/usage.md#share-declarations-with-includes) for
paths, generated includes and the full schema dependency contract.

### Generated declaration guards

C++ output wraps each reusable declaration in a
`SERIALIZER_GENERATED_<hex-identity>` guard. The suffix encodes the declaration's
qualified identity as hexadecimal; the macro value is a decimal fingerprint of
its generated body, followed by `ULL`. The first copy emits the declaration.
An identical copy from another independently generated bundle is skipped, while
a different fingerprint produces `#error` for incompatible definitions.

`#pragma once` protects an individual header, so it cannot deduplicate the same
declaration in distinct entry bundles. These declaration guards cover that case
without adding object storage, runtime work or serialized bytes. Keep naming,
runtime and codec settings consistent when sharing generated declarations.

## Class-specific comparison settings

Each implemented backend accepts `[language.class.source::qualified_name]`
sections selecting `equality = true|false`, `ordering = none|lexicographic`, and
`fields = durable` or a comma-separated list of own source member names. They
select comparison APIs independently of backend naming and codec generation;
they do not change the schema or serialized formats. Unknown selectors and
unsupported selected shapes diagnose before outputs are replaced. See
[record comparisons](../schema/record_comparisons.md) for generated APIs, portable field
restrictions, NaN behavior and C++ generic alias requirements.

## Supported profiles

These are **presentation profiles**, covering layout and generated identifiers.
They do not change algorithms, access control, ownership, exception handling,
the C++23 minimum, or wire encoding. They do not certify compliance with a whole guide.

| Profile | Layout | Types | Class fields | Enum values | Generated accessors |
| --- | --- | --- | --- | --- | --- |
| `serializer` | Repository `.clang-format`: 2 spaces, attached braces, 100 columns | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `core` | Serializer layout | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `google` | clang-format Google | `AccountRecord` | `account_id_` | `kReadyNow` | `GetAccountId()` |
| `llvm` | clang-format LLVM | `AccountRecord` | `AccountId` | `ReadyNow` | `getAccountId()` |
| `gnu` | clang-format GNU | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `cert` | Serializer layout | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `misra` | Serializer layout | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `autosar` | Serializer layout | `account_record` | `account_id` | `ready_now` | `get_account_id()` |
| `qt` | 4 spaces, class/function braces on a new line, attached control braces, 100 columns | `AccountRecord` | `accountId` | `ReadyNow` | `getAccountId()` |

Namespaces use `snake_case` in every renaming profile. Acronyms are split at word
boundaries: `HTTPServer` becomes `http_server` or `HttpServer`; `userID` becomes
`user_id`, `user_id_`, or `UserId`/`userId`, according to its role. Normalization can
collapse distinct schema names. Collisions and C++ keywords are rejected before
output is appended, including collisions with generated helpers.

The `core`, `cert`, `misra`, and `autosar` choices deliberately share the repository
layout and naming rules: those broader guides do not provide one interchangeable
clang-format/naming specification. Use `format_file` for an organization's approved
layout. In particular, selecting `misra` does not change the project to C++17;
selecting `autosar` does not change it to C++14 or remove unsupported constructs.
Google's restrictions on exceptions and public data members are not implemented
by its presentation profile. The Qt profile retains generated control-flow braces
and does not add Qt's framework-specific `Q` prefixes or dependencies.

Official references:

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- [GNU Coding Standards](https://www.gnu.org/prep/standards/standards.html)
- [SEI CERT C++](https://cmu-sei.github.io/secure-coding-standards/sei-cert-cpp-coding-standard/)
- [MISRA C++:2023](https://misra.org.uk/product/misra-cpp2023/)
- [AUTOSAR C++14 Guidelines](https://www.autosar.org/fileadmin/standards/R17-03_R1.1.0/AP/AUTOSAR_RS_CPP14Guidelines.pdf)
- [Qt Coding Style](https://wiki.qt.io/Qt_Coding_Style)
- [clang-format configuration](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)

## Names, compatibility, and generated syntax

The writer translates identifiers from resolved schema declarations; it does not
search and replace words in the completed header. Class/enum/namespace references,
parents, nested types, maps, union members/discriminators, and view accessors use
the same naming policy. The parsed schema is not mutated, so it can be emitted
under multiple profiles in the same process.

Generated C++ declarations retain schema order. Consecutive declarations with the
same access share one section, and the writer emits `public:`, `protected:`, or
`private:` when access changes. Generated helpers retain their required access;
opaque native hooks require access restoration because their contents can change
the current section. Blank-line grouping between ordinary fields follows the
schema source rather than adding a separator after every field. This applies to
the emitted source before formatting. Built-in profiles leave spacing around access
labels intact; custom formatter rules can adjust whitespace and empty-line limits.

**Wire names stay unchanged.** For `public uint32 accountID (2);`, Serializer emits
the C++ member `account_id`, Google emits `account_id_`, and Qt emits `accountId`.
All three still encode the key `accountID`. Explicit display names, field IDs,
enum/union wire spellings, alternative numbers, declaration order, and endianness
also stay unchanged. Renaming a profile changes the C++ API, not the wire format.

Runtime/ADL interfaces and static codec helpers keep their required spellings, including
`serialize`, `deserialize`, `serialize_in`, `serialize_out`,
`serialize_in_member_by_name`, `serialize_in_member_by_identifier`,
`serializer_enum_*`, `serializer_scan`, `map`, and `to_string`. Runtime types and
inherited view APIs such as `serialized_bytes` also retain their public names.
Generated conversion helpers such as `to_account_state` and field accessors follow
the selected profile. `naming = preserve` keeps the earlier schema-derived names
and helper spellings while allowing another layout.

Literal defaults remain unchanged. A default referring to the field's declared
enum, such as `{ AccountState::WaitingForReview }`, is resolved and renamed too.
Opaque C++ expressions involving other identifiers are rejected when renaming is
enabled; use a literal, a qualified declared enum value, or `naming = preserve`.
This prevents silently emitting references to names that no longer exist.

The generator also corrects brace/pointer spacing, normalizes indentation and
line wrapping, places purpose comments before template declarations, and removes
redundant `inline` on `constexpr` functions. Ordinary non-constexpr free functions
defined in the header still need `inline`. Field writes explicitly use `this->`
so a generated parameter cannot hide the field.

## Formatting dependency and custom rules

Formatting requires **clang-format 19 or newer** at generation time. Runtime
serialization and applications using already generated headers have no formatter
dependency. Pin the formatter version in a reproducible build because built-in
style defaults can change between versions.

The generator formats a temporary header using an explicit style. It does not
pick up a `.clang-format` from an unrelated current directory. Formatter arguments
are passed without a shell, including paths with spaces. Launch/configuration/
format failures are reported before the destination header is written.

For organization-specific rules:

```ini
[cpp]
coding_standard = google
format_file = styles/team.clang-format
```

```yaml
# styles/team.clang-format
BasedOnStyle: Google
ColumnLimit: 100
LineEnding: CRLF
```

`format_file` replaces the profile's layout; it does not change naming. A custom
file controls its own line endings. Built-in profiles emit CRLF. `format = false`
skips the formatter and its dependency; the caller must format that intermediate
source before treating it as output matching a layout profile.

For direct C++ use, pass `writer::cpp_options` to `writer::cpp::write`:

```cpp
rohit::serializer::writer::cpp_options options{};
options.standard = rohit::serializer::writer::coding_standard::google;
options.clang_format = "C:/Tools/LLVM/bin/clang-format.exe";
rohit::serializer::writer::cpp::write(output, statements, options);
```

## Examples for every profile

The [coding-style examples](../../example/coding_styles/README.md) contain one
configuration, local schema, and C++ consumer in each profile's own folder.
Their schemas describe the same mixed-case contract with owning classes, both
views, inheritance, enums, unions, arrays, and maps.
Generated headers belong in the build directory and are produced by the real
generator. They are not checked in as manually maintained examples.

**Validation status:** all nine C++ profiles and all three Java profiles were
generated, compiled, and executed on Windows during Java backend verification.
Generator/configuration tests passed. The full C++ unit suite still has four
failures; see the [verification notes](java.md#verification-performed-for-this-implementation).
These checks do not establish whole-standard compliance or benchmark performance.

## JavaScript, Go, and C# settings

`[js]`, `[go]`, and `[csharp]` accept `naming = profile|preserve`. The default is
native profile naming; no external formatter is invoked. TypeScript declarations
share `[js]` settings. `[go] package` defaults to `generated`; `[csharp] namespace`
defaults to `SerializerGenerated`. CLI overrides are `--js.naming`, `--go.naming`,
`--go.package`, `--csharp.naming`, and `--csharp.namespace`. The optional C#
namespace can be cleared with `--csharp.namespace=`.

Use `--js.output`, `--typescript.output`, `--go.output`, and `--csharp.output` for
multi-language destinations. The [portable language guide](portable_languages.md)
covers naming, native type mappings, runtime APIs, and supported schema shapes.

## Rust, Python, Swift, Kotlin, and C

Select `rust`, `python`, `swift`, `kotlin`, and/or `c` in `[output] language`.
Their required destinations are `.rs`, `.py`, `.swift`, `.kt`, and `.h`, using
`--<language>.output` for multiple targets. They use fixed native naming; C++/Java
coding profiles and earlier portable naming options do not configure them.

```ini
[output]
language = rust, python, swift, kotlin, c
[kotlin]
package = example.models
```

`--kotlin.package` overrides the configured package and `--kotlin.package=` clears
it. No package is generated by default. The other four targets require no
backend-specific INI section. See [native target usage](native_languages.md).

## Managed identity configuration

The optional [C++ managed interface](../managed/cpp_runtime.md) accepts a central section:

```ini
[managed]
id_type = uint64
id_encoding = compact_varint
```

The default is `uint32`; no other spellings are accepted. CLI
`--managed.id_type uint32` or `--managed.id_type uint64` overrides the file. All
managed descendants and the root use this document-local type. Managed classes
carry `persistent_id` directly by default. Set `[managed] separate_values = true`
(or `--managed.separate_values true`) to generate ID-free ordinary values and
separate managed storage; the default is `false`. Use the same representation
for every output sharing a model.
`id_encoding` defaults to `compact_varint`; `fixed` selects the configured integer
width on the wire. CLI `--managed.id_encoding` overrides it. The policy applies
to generated `persistent_id` and managed-reference object IDs in `binary_none`,
`binary_integer` and `binary_string`. IDs retain their numeric values, memory
width and JSON representation. Ordinary integer fields and binary field tags
are unaffected. Unknown policy names reject; use `compact_varint`, not
`compact_variant`.

All participants sharing a model agree on width, representation and encoding.
Changing these settings requires an explicit persistence migration; see the
[persistent-ID migration guide](../guides/persistent_id_migration.md).
