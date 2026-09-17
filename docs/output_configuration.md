# Output languages and coding profiles

Keep output choices in the **generator configuration**, separate from `.serializer` schemas.
Schemas define names, IDs, types, defaults, and representations. Each application can
select a language backend and its presentation rules without changing that contract.
The **C++ and Java backends** are implemented. Other language names and sections
are rejected. Java has independent `serializer`, `google`, and `oracle` profiles;
see [Java configuration and profile scope](java.md#java-coding-styles). The C++
configuration below remains the default.

## Configuration and command line

Save a UTF-8 file without a BOM, such as `serializer_output.ini`:

```ini
[output]
language = cpp

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
also selects both. See [the full command-line contract](command_line.md), including
short options, version reporting, and required `.serializer` version headers.

| Setting | Default | Meaning |
| --- | --- | --- |
| `[output] language` | `cpp` | Select `cpp`, `java`, or `cpp,java`; CLI spelling is `--language`. |
| `[cpp] coding_standard` | `serializer` | Select a profile from the table below. |
| `[cpp] naming` | `profile` | Rename target identifiers; `preserve` retains schema spellings. |
| `[cpp] protobuf` | `false` | Generate direct Protobuf binary, ProtoJSON, and TextProto codecs; validate the [compatible schema subset](protobuf.md). |
| `[cpp] format` | `true` | Run `clang-format`; `false` emits intermediate source for a caller-managed formatting pipeline. |
| `[cpp] clang_format` | `clang-format` | Executable name on `PATH`, or a path to the executable. |
| `[cpp] format_file` | Unset | Use a custom clang-format YAML file in place of the profile's layout. |

C++ CLI overrides use `--cpp.` plus the setting name, for example
`--cpp.naming preserve` or `--cpp.clang_format "C:/Tools/LLVM/bin/clang-format.exe"`.
Use `--cpp.format_file=` to clear an inherited custom format file.
Output filenames must end in `.h`, `.hpp`, or `.hxx`; `.hpp` is the repository default convention.
`serializer --help` lists the arguments. Unknown/repeated scalar arguments, unsupported
settings, malformed configurations, and failed generation return a nonzero exit code.

INI keys, section names, profile names, and boolean values are case-sensitive.
Each section/key may appear once. Blank lines and whole-line `#` or `;` comments
are allowed. Values may have surrounding double quotes. Backslashes are literal;
there are no escape sequences, inline comments, environment expansion, or includes.
File paths in the config resolve relative to its directory. A bare executable
name is searched on `PATH`; a relative executable path containing a directory is
resolved relative to the config. CLI paths resolve from the current working directory.

## Supported profiles

These are **presentation profiles**, covering layout and generated identifiers.
They do not change algorithms, access control, ownership, exception handling,
the C++20 minimum, or wire encoding. They do not certify compliance with a whole guide.

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

The [coding-style examples](../example/coding_styles/README.md) contain one
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
