# Rohit Serializer for Visual Studio Code

This extension supports the [Serializer schema compiler and serialization library](https://github.com/rohit-singh-gautam/Serializer)
maintained in that repository.

Edit `.serializer` schemas with syntax highlighting, bracket matching, comments,
folding, and snippets. Version **1.1.19** includes navigation from includes and type
references to source schemas and existing generated code in all 11 output languages. Use the project's
CMake configuration for the separate build and missing-header assistance commands.

```text
serializer version 1;

class account stable_ids {
  public uint32 id (1);
  public string name (2);
}
```

This extension recognizes `.serializer`; it does not associate `.def` or `.struct`.
Type `schema`, `include`, `class`, `field`, `enum`, or `namespace` to insert a snippet. Field
snippets require you to choose an unused ID; they do not manage wire compatibility.

The extension highlights `include common;` with separate keyword,
unquoted path, and semicolon scopes. Relative paths such as `../shared/common`
resolve to `.serializer` files during navigation; explicit `.serializer` includes
remain supported. The compiler must support shorthand to build these schemas.
Place includes after the version header and before declarations;
quoted paths and angle brackets are invalid. See [schema includes](../../docs/usage.md#share-declarations-with-includes).

## Download and install Serializer

The editor extension is separate from the Serializer compiler and runtime:

- [Download Serializer source (ZIP)](https://github.com/rohit-singh-gautam/Serializer/archive/refs/heads/main.zip)
  or [clone the repository](https://github.com/rohit-singh-gautam/Serializer).
- Follow the [build and installation instructions](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/cmake_integration.md#use-an-installed-package)
  to install the compiler, runtime library, headers, and CMake package.
- Use the [CMake integration guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/cmake_integration.md)
  to add Serializer to your project and generate code from schemas.

Installing this extension does not install the compiler or runtime.

## Install and use

From the repository root on Windows, `./make.ps1 all` builds Serializer and both
editor VSIX packages; `./editors/build.ps1` builds only the packages. These commands
require Node.js 22+, npm and Visual Studio MSBuild, restore locked dependencies,
and write to `out/extensions` without installing either extension.

Install the packaged VSIX through **Extensions: Install from VSIX**. The extension
supports VS Code 1.96+ on desktop and remote extension hosts. Highlighting,
snippets and file navigation work without CMake Tools and in Restricted Mode.

The dedicated 32×32 Serializer icon is registered for `.serializer` files in
Explorer and editor tabs, for both light and dark themes. The selected file icon theme can
override or hide language icons; use **Preferences: File Icon Theme** to select
**Seti (Visual Studio Code)** if your current theme does not display it.
The extension cannot force this icon over every file icon theme.

For build commands, install Microsoft **CMake Tools**, trust the workspace, select
its configure/build presets or kit, and run **CMake: Configure**. Use Serializer's
existing helper in the consuming project:

```cmake
# After add_subdirectory(...) or find_package(Serializer CONFIG REQUIRED):
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS schemas/account.serializer)
```

The project still needs Serializer, a C++20 compiler, CMake 3.28+, and clang-format
19+ when output formatting is enabled. This extension does not bundle those tools.

## Declaration and definition navigation

Git index and history tabs (including read-only `(Index)` views) support the
same declaration, definition, and type-definition actions. Cursor offsets and
local symbols come from the displayed snapshot. Includes, other schema files,
and generated output resolve against the current workspace; this does not
reconstruct a historical checkout. With an older extension, open the working
file from Explorer to use navigation, or install 1.1.19 and reload VS Code.

Caller navigation depends on the language service's active project configuration.
For this repository's managed ledger examples/tests, use
`./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_MANAGED=ON'`, select that same build
in CMake Tools, and run **Serializer: Configure IntelliSense**. Removing the build
cache resets the optional managed targets to OFF unless the option is passed again.
Building packages does not install them or reload the editor.

For `using ledger = ledger_example::ledger;`, selecting the right-hand type should
resolve the generated class. Selecting a later use of the left-hand alias normally
opens the `using` declaration; **Go to Type Definition** follows its underlying class.
**Serializer: Go to Schema Declaration** follows either to the original schema,
including through alias chains and variables when a native type provider is available.
The [coverage matrix](../../docs/editor_navigation.md#navigation-coverage-matrix)
records required cases and remaining native-host verification limits.

Right-click a `.serializer` file in **Explorer** or its **editor tab**, then choose
**Serializer: Go to Implementation** to open existing generated output.
The command uses the clicked file, even when another editor is active. Multiple
available outputs produce a picker; missing output produces no result or build prompt.

| Selected item | Go to Declaration | Go to Definition |
| --- | --- | --- |
| Schema `include types/account;` | Included schema | Included schema |
| Class/enum declaration or type reference in a schema | Original schema declaration | Matching generated type definition; schema declaration if unavailable |
| C/C++ `#include <account.hpp>` | Entry schema | Existing generated header |
| Generated class/enum type reference in any supported language | Original schema declaration | Normal language-service definition |

**Go to Type Definition** on a schema type opens its source class/enum declaration,
including qualified references and unsaved included schemas. On includes it opens
the included schema. Generated-language type definition remains with that language's
provider. Schema field names and primitive types have no type-definition destination.
All three actions accept the `class`/`struct`/`enum` keyword or the type name, including
forward and reversed `class ledger` selections. Declaration and type definition on
`ledger` in its own declaration select the same name; definition prefers generated output.

Use the editor's built-in **Go to Declaration** context-menu action. Qualified
names such as `demo::order` work on either component and at the end of a selection.
Schema include navigation resolves to the included schema for both actions, since
an include can contribute multiple declarations to generated output. Schema declarations
resolve independently of CMake Tools or generated files.

Navigation only reads available files. It never configures, builds, generates,
saves a document, activates CMake Tools, or offers to generate a missing header.
Type references include the enum prefix in defaults such as
`AccountState::WaitingForReview`. Put the cursor on `AccountState`; the enum value
itself is not a type reference. **Go to Definition** falls back to the source
declaration when the generated header or matching type is unavailable, including
new types in unsaved schemas. Unresolved names and missing header-only destinations
produce no result. Included schemas can map to an entry
schema's header: `account.serializer` included by `request.serializer` may be
implemented in `request.hpp`. Unsaved schema edits are used for declaration lookup.

An already active CMake Tools configuration narrows discovery for its generated
languages and supplies target-specific C/C++ include paths. Other output languages
remain discoverable. Navigation searches the current workspace folder, including
ignored build directories. Existing `<output>.d` or multi-output `.d` dependency
files identify entry schemas precisely. Older banner-marked outputs without a
depfile use matching schema basenames and the Serializer generated-file banner;
ambiguous matches remain available as separate choices. Naming profiles and
storage-mode class specializations are supported. This is available-file lookup,
not an assertion that the generated output is up to date with unsaved schema edits.

Caller type references require a definition provider for their language, such as
Microsoft C/C++, clangd, Java, TypeScript, Go, C#, Rust, Python, Swift, or Kotlin
language tooling. Direct generated type declarations do not require semantic
resolution once the editor recognizes the file's language. Java namespace
containers, flattened portable/native names, C typedefs, TypeScript enum aliases,
and preserved naming are supported. VS Code merges declaration providers' results;
**Serializer: Go to Schema Declaration** remains in the Command Palette for only
schema destinations. Other language-service commands retain their normal behavior.

For renamed outputs or native generators without a generated-file banner, retain
the compiler's dependency metadata in the workspace, for example:

```sh
serializer --input schemas/model.serializer --language python --output generated/schema.py --depfile generated/schema.py.d
```

A shared `--depfile generated/model.d` also works for multiple language outputs.
Navigation only consumes this metadata; it never runs the compiler.

## Commands

| Command | Behavior |
| --- | --- |
| **Serializer: Generate Headers** | Builds `serializer_generated_headers` through CMake Tools, using the selected project and configuration. Saves modified schema/INI/CMake inputs in that workspace folder first. |
| **Serializer: Open Generated Header** | Opens an existing header containing the active schema, with a path picker for multiple outputs. Missing output is a silent no-op. No build or generation prompt. |
| **Serializer: Go to Implementation** | Opens existing output in any generated language for the `.serializer` file selected in Explorer or an editor tab; uses the active schema from the Command Palette. |
| **Serializer: Go to Schema Declaration** | Command Palette alternative that opens only schema destinations for the selected include or type, independently of other declaration providers. |
| **Serializer: Diagnose Missing Header** | With the cursor on a literal C/C++ `#include`, reports source-specific include paths, file existence, and registered generated headers in the Serializer Output channel. Otherwise asks for the header name. |
| **Serializer: Use CMake Tools for C/C++ IntelliSense** | Explicitly sets `C_Cpp.default.configurationProvider` to `ms-vscode.cmake-tools` in the selected workspace folder. Requires Microsoft C/C++. |

Missing-include diagnostics on Serializer runtime/generated headers also offer
quick fixes for diagnosis and, for matching schemas, generation. These are
available when the C/C++ language service reports an error. The extension does
not create its own C++ diagnostics or replace your C++ language service.

To limit generation to a single consumer, set the resource-scoped setting:

```json
{
  "serializer.headersTarget": "my_app_serializer_headers"
}
```

Generation builds the Serializer compiler when required, but does not compile
the consuming application. Compiler errors remain available in the CMake and
Serializer output channels. Cancellation and unsuccessful builds do not report
success. There is no automatic build on file open/save.

## Limits

Generic declarations (`class box<T>`), nested type arguments, and named roots
(`instantiate root = box<uint32>;`) are highlighted and indexed. Parameters
navigate to their local declaration. Generic definitions navigate to their C++
native template or concrete generated models in the other languages. Without a
concrete schema contract, only C++ has a generated destination. Generated concrete classes navigate back to the generic
definition. See [schema generics](../../docs/generics.md) for compiler limits.

- Build assistance requires an already configured CMake Tools project. It works
  through CMake with Makefile, Ninja, and Visual Studio generators; handwritten
  Makefiles and browser-only VS Code are not supported by these commands.
- Generated-code navigation uses existing dependency files when available. Without one,
  duplicate basenames can be ambiguous; all matching candidates are exposed.
  Renamed outputs need an `<output>.d` or workspace multi-output `.d` dependency file.
  Python, Swift, Kotlin and C outputs require dependency metadata because their
  current generators do not emit a Serializer banner. A matching type name alone
  never establishes output ownership.
  Outputs outside the workspace need the active CMake model or a resolved C++
  language-service location. Navigation does not create metadata or add include paths.
- Diagnose from a compiled `.cpp` file for source-specific settings. A standalone
  header may have no compile group. Compiler implicit/system include paths and
  macro-expanded include directives are not inspected.
- clangd users should point clangd at their build's `compile_commands.json`; the
  IntelliSense settings command is specific to Microsoft C/C++.
- Syntax highlighting and schema indexing are lexical. Semantic diagnostics,
  completion, member/function navigation and a language server are not implemented.
  Navigation skips unopened files larger than 16 MiB and bounds include traversal.
- For Visual Studio 2022/2026, use the separate
  [Visual Studio extension](https://github.com/rohit-singh-gautam/Serializer/tree/main/editors/visual_studio).
  It shares the grammar, custom-type highlighting and navigation resolver, and
  supplies native schema declaration/definition commands. The CMake commands and
  snippets above belong to VS Code. Visual Studio caller references first use
  their language service to reach the generated declaration.

See the repository's [extension guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/editor_extension.md)
for development, validation, and packaging instructions. This source distribution
has not been published to Marketplace.

The C++ `managed` class/member keyword is highlighted and skipped when locating a
member's type. Direct, array, and map managed references retain declaration and
definition navigation. Generated managed data, storage, and editor class declarations
map back to the original schema type; schema-to-output navigation still selects the
ordinary class. Capability selectors and other-language managed runtimes remain
unimplemented; see the [managed runtime guide](../../docs/managed/cpp_runtime.md).

Managed classes now expose persistent IDs directly by default. Set
`[managed] separate_values = true` (or `--managed.separate_values true`) when
ID-free ordinary classes and managed storage companions are required. Navigation
supports the direct schema class, its editor, and opt-in companion declarations.

## Native templates and dimensions (1.1.15)

Both packages recognize `uint64 N`, trailing defaults, fixed `array[N * M] T`
expressions, nested generic applications, and optional `instantiate` contracts.
Navigation resolves dimension/type parameter references and defaults to their
local declarations, including unsaved text. C++ definitions include native templates
with no concrete schema uses. Other languages have destinations only for concrete
contracts; fixed-array output is currently unsupported there. The shared grammar
highlights dimension arithmetic and the `matrix` snippet inserts a fixed-array
example. Both packages use the same navigation implementation.

Version 1.1.19 recognizes payload `version`/`compatibility`, `version2`/`version3`/`version4`, `created`/`obsolete`/`replaced`, and `reserve` syntax. Qualified field-type navigation remains available beside lifecycle metadata. See the [revision contract](../../docs/versioning.md).

## Release policies (1.1.19)

Both extensions highlight `releases`, nested `policy` / `any` / `all`, and the `max_age`, `keep_last`, `released_since`, `expires_on`, and compatibility leaves. The schema compiler folds these into ordinary version bounds; generated readers contain no dates or policy tree. See the [release policy contract](../../docs/versioning.md#release-dates-and-compile-time-policies).

## Magic and format exclusions (1.1.19)

Both editors recognize static `magic` declarations and field `omit(...)` annotations.
The compiler writes exact magic bytes before native binary payloads and verifies the
fixed `magic` JSON field by default. `omit(json)` explicitly removes it from JSON;
ordinary fields can exclude any supported format and retain their defaults on decode.
Magic byte strings remain literals, and adjacent schema type references retain navigation.
