# Rohit Serializer for Visual Studio Code

Edit `.serializer` schemas with syntax highlighting, bracket matching, comments,
folding, and snippets. Use the project's CMake configuration to generate and locate
C++ headers and investigate missing includes.

```text
serializer version 1;

class account stable_ids {
  public uint32 id (1);
  public string name (2);
}
```

This extension recognizes `.serializer`; it does not associate `.def` or `.struct`.
Type `schema`, `class`, `field`, `enum`, or `namespace` to insert a snippet. Field
snippets require you to choose an unused ID; they do not manage wire compatibility.

## Install and use

Install the packaged VSIX through **Extensions: Install from VSIX**. The extension
supports VS Code 1.96+ on desktop and remote extension hosts. Highlighting and
snippets work without CMake Tools and in Restricted Mode.

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

## Commands

| Command | Behavior |
| --- | --- |
| **Serializer: Generate Headers** | Builds `serializer_generated_headers` through CMake Tools, using the selected project and configuration. Saves modified schema/INI/CMake inputs in that workspace folder first. |
| **Serializer: Open Generated Header** | Opens the active schema's matching C++ header from the CMake model. Offers generation when absent and a target/path picker when multiple profiles share the filename. |
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
- Header lookup uses the active CMake model. Multiple same-name outputs require
  explicit selection; no global recursive include path is added. Discovery is
  by header basename, so the picker may also show another registered header with
  that name. It does not establish a schema-to-output provenance map.
- Diagnose from a compiled `.cpp` file for source-specific settings. A standalone
  header may have no compile group. Compiler implicit/system include paths and
  macro-expanded include directives are not inspected.
- clangd users should point clangd at their build's `compile_commands.json`; the
  IntelliSense settings command is specific to Microsoft C/C++.
- Syntax highlighting is lexical. Semantic schema diagnostics, completion,
  go-to-definition, and a language server are future work.
- For Visual Studio 2022/2026, use the separate
  [Visual Studio extension](https://github.com/rohit-singh-gautam/Serializer/tree/main/editors/visual_studio).
  It shares the grammar and basic editing configuration; the commands above are VS Code features.

See the repository's [extension guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/editor_extension.md)
for development, validation, and packaging instructions. This source distribution
has not been published to Marketplace.
