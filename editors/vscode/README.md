# Rohit Serializer for Visual Studio Code

Edit `.serializer` schemas with syntax highlighting, bracket matching, comments,
folding, and snippets. Version **1.1.3** includes navigation from includes and type
references to source schemas and existing generated C++ headers. Use the project's
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

## Install and use

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

Right-click a `.serializer` file in **Explorer** or its **editor tab**, then choose
**Serializer: Go to Implementation** to open an existing generated C++ header.
The command uses the clicked file, even when another editor is active. Multiple
available headers produce a picker; missing output produces no result or build prompt.

| Selected item | Go to Declaration | Go to Definition |
| --- | --- | --- |
| Schema `include types/account;` | Included schema | Existing header containing that schema's declarations |
| Class/enum declaration or type reference in a schema | Original schema declaration | Matching generated C++ type definition; schema declaration if unavailable |
| C++ `#include <account.hpp>` | Entry schema | Existing generated header |
| Generated class/enum type reference in C++ | Original schema declaration | Normal C++ language-service definition |

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

An already active CMake Tools configuration narrows output discovery and supplies
target-specific include search paths. Otherwise navigation searches the current
workspace folder, including ignored build directories. Existing `<header>.d`
dependency files identify the entry schema precisely. Older headers without a
depfile use matching schema basenames and the Serializer generated-file banner;
ambiguous matches remain available as separate choices. Naming profiles and
storage-mode class specializations are supported. This is available-file lookup,
not an assertion that the generated output is up to date with unsaved schema edits.

C++ class references require a C++ definition provider such as Microsoft C/C++ or
clangd. VS Code can combine its declaration results with ours. Use **Serializer:
Go to Schema Declaration** for just the schema destination. Multiple existing
headers are shown through normal navigation choices or a command picker.

## Commands

| Command | Behavior |
| --- | --- |
| **Serializer: Generate Headers** | Builds `serializer_generated_headers` through CMake Tools, using the selected project and configuration. Saves modified schema/INI/CMake inputs in that workspace folder first. |
| **Serializer: Open Generated Header** | Opens an existing header containing the active schema, with a path picker for multiple outputs. Missing output is a silent no-op. No build or generation prompt. |
| **Serializer: Go to Implementation** | Opens existing generated C++ output for the `.serializer` file selected in Explorer or an editor tab; uses the active schema from the Command Palette. |
| **Serializer: Go to Schema Declaration** | Opens only schema declaration results for the selected include or type, independently of other C++ declaration providers. |
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

- Build assistance requires an already configured CMake Tools project. It works
  through CMake with Makefile, Ninja, and Visual Studio generators; handwritten
  Makefiles and browser-only VS Code are not supported by these commands.
- Header navigation uses existing dependency files when available. Without one,
  duplicate basenames can be ambiguous; all matching candidates are exposed.
  Renamed outputs need a sibling `<header>.d` dependency file for schema mapping.
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
  It shares the grammar and basic editing configuration; the commands above are VS Code features.

See the repository's [extension guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/editor_extension.md)
for development, validation, and packaging instructions. This source distribution
has not been published to Marketplace.
