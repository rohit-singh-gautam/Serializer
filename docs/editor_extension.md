# Serializer editor extensions

Serializer provides separate packages for VS Code and Visual Studio. Both use
`editors/serializer.tmLanguage.json`; build assistance commands currently belong
to the VS Code extension. For the Visual Studio VSIX, see
[Visual Studio](#visual-studio-extension) below.

Both packages highlight `include common.serializer;` with distinct scopes for
the keyword, unquoted relative path, and semicolon. Paths such as
`../shared/common.serializer` are supported; quoted paths are marked invalid.
The VS Code package also supplies an `include` snippet. See
[schema include syntax](usage.md#share-declarations-with-includes).

## VS Code

The [Rohit Serializer extension](../editors/vscode/README.md), version **1.1.2**, provides `.serializer`
syntax highlighting, snippets, declaration/definition navigation, and CMake generated-header commands. Schemas use
the current `serializer version 1;` header. Legacy `.def` and `.struct` names are
not registered. C++/Java generation remains owned by the project's build rules;
this initial extension's generated-file commands target C++ headers.

## Build and install locally

Use Node.js 22+, npm, and VS Code's `code` CLI. From PowerShell at the repository
root, build and install with:

```powershell
./install_extension.ps1
```

The script restores locked dependencies with `npm ci`, builds a VSIX using the
version in the extension manifest, and installs it with `--force` so rerunning
also replaces an installed copy of that version. It works from other directories
when invoked by its path and stops on build or installation failures.

Use `-SkipBuild` to install the existing VSIX without Node.js/npm, or
`-CodeCommand code-insiders` (or a full CLI path) for a different editor install.
`-ExtensionsDirectory <path>` selects a separate extension directory for testing;
relative paths are resolved from the caller's directory. It does not install
CMake Tools or Microsoft C/C++.

For the individual development commands, start from the repository root:

```sh
cd editors/vscode
npm ci
npm test
npm run package
```

Packaging compiles and bundles TypeScript, copies the canonical grammar, logo,
and repository license into the extension, and writes:

```text
out/extensions/serializer-vscode-1.1.2.vsix
```

From the repository root, install it with:

```sh
code --install-extension out/extensions/serializer-vscode-1.1.2.vsix
```

Alternatively run **Extensions: Install from VSIX** and select the file. The
package is for VS Code, not Visual Studio. The extension ID is
`rohitjairajsingh.serializer-language`, using Rohit Jairaj Singh's Marketplace
publisher ID. The manifest's `author.url` links to his LinkedIn profile; `homepage`,
`repository`, and `bugs` link to the extension documentation, source, and issue tracker.

## Generate and resolve headers

1. Install Microsoft CMake Tools and configure the consumer's CMake project.
2. Register schemas with `serializer_generate` as described in the
   [CMake guide](cmake_integration.md).
3. Run **Serializer: Generate Headers**. The extension asks CMake Tools to build
   `serializer_generated_headers` with the active preset/configuration/environment.
   Set `serializer.headersTarget` to `<consumer>_serializer_headers` when needed.
4. For Microsoft C/C++, run **Serializer: Use CMake Tools for C/C++ IntelliSense**
   or set its configuration provider yourself. That command explicitly updates
   only the chosen workspace folder's provider setting.
5. If an include still fails, place the cursor on it in a compiled C++ source file
   and run **Serializer: Diagnose Missing Header**. Read the Serializer Output
   report for file existence, target-specific search paths, and suggested changes.

**Serializer: Open Generated Header** opens only existing output containing the
active schema, including declarations merged from included schemas. It asks which
path to use when several outputs apply and returns silently when none is available.

Build/diagnosis/configuration commands require workspace trust and CMake Tools;
file navigation and lexical editing do not.
Generation saves modified schema, INI, and CMake documents in the chosen workspace
folder. It can compile the Serializer generator but not the consuming target.
Nothing configures or builds automatically when opening or saving a schema.
See [IntelliSense troubleshooting](intellisense.md) for the underlying integration.

## Navigate available schemas and headers

Right-click a `.serializer` file in **Explorer** or its **editor tab** and select
**Serializer: Go to Implementation**. This opens existing generated C++ output
for the clicked file, including an included schema's entry header. It uses the
clicked file rather than whichever editor is active. Multiple available outputs
produce a picker; missing output does nothing and never prompts for generation.
The command is also available in the Command Palette for the active schema.

**Go to Declaration** opens a schema include's file or the original class/enum
declaration for a type reference. From C++ includes it opens the entry schema;
from C++ type references it maps the C++ language service's resolved generated
type back to its original schema, including declarations in included files.

**Go to Definition** on a schema include opens its existing output header; on a
schema type it selects the generated type definition, falling back to the original
schema declaration when no matching generated definition is available. This also
works on `AccountState` inside a default such as `AccountState::WaitingForReview`;
the enum value itself is not a type reference. Included `account.serializer`
may be emitted in `request.hpp`, so navigation follows include relationships.
C++ class definitions continue to use the C++ language service. The extension
also supplies generated-header locations for literal C++ includes.

These actions and **Open Generated Header** never save, configure, build, generate,
activate CMake Tools, or offer generation. Unresolved types and missing header-only
destinations return no result; schema type definitions can still use the source fallback.
An already active CMake configuration supplies output candidates and include paths;
otherwise the current workspace folder is searched, including ignored build trees.
Sibling `<header>.d` dependency files identify the entry schema. Legacy outputs
without depfiles use the generated-file banner and schema basenames, with ambiguous
matches exposed as choices. Naming profiles and storage-mode specializations are
recognized. Unsaved schema contents participate in lookup; stale output is not
regenerated or represented as current.

C++ type references require Microsoft C/C++, clangd, or another definition provider.
VS Code merges providers' declaration results; **Serializer: Go to Schema Declaration**
offers only schema destinations. This is a lexical schema index, not a full language
server. Member/function navigation and semantic diagnostics remain unimplemented.
See the [extension README](../editors/vscode/README.md#limits) for discovery limits.

## Development and tests

The canonical TextMate grammar is `editors/serializer.tmLanguage.json`. Do not
edit the ignored copy under `editors/vscode/syntaxes`; packaging refreshes it.

The extension icon comes from `logo/serializer_logo_128x128.png`. Packaging copies
it to the ignored `editors/vscode/dist/serializer_logo.png`, referenced by `icon`
in the extension's `package.json`. Replace the source logo and rebuild the VSIX
to update the icon in VS Code and the Marketplace listing. The existing 128×128
PNG meets the [extension icon requirement](https://code.visualstudio.com/api/references/extension-manifest);
the larger `logo/serializer_logo.png` remains available as the source artwork.

File and editor-tab icons use `logo/serializer_icon_32x32.png`, copied to
`editors/vscode/dist/serializer_icon_32x32.png` and referenced by both light/dark
icons under `contributes.languages`. This keeps the file icon separate from the
128×128 Marketplace logo. VS Code scales the image for display; the language
contribution takes one image per light/dark theme, not a resolution set. The
16×16 and 64×64 variants remain in `logo/` for other uses.

The language icon appears beside `.serializer` files in Explorer and editor tabs
when the selected file icon theme supports language defaults and does not
override this file type. Themes can suppress language icons; the extension cannot
force its icon across all file icon themes. If the logo is not
visible after reinstalling and running **Developer: Reload Window**, select
**Seti (Visual Studio Code)** using **Preferences: File Icon Theme**. See
[language default icons](https://code.visualstudio.com/api/extension-guides/file-icon-theme#language-default-icons)
for the theme precedence rules.

The extension uses the public `vscode-cmake-tools` API. Keep compiler generation
and include directories on CMake targets rather than maintaining editor-only rules.

`npm test` checks the grammar using VS Code's TextMate/Oniguruma engines, tokenizes
the maintained schemas, and verifies configuration isolation, include search
order, and duplicate generated-header ownership.

It also covers navigation through include graphs, namespace/name resolution,
storage-mode classes, dependency paths, naming profiles, missing/changed files,
Restricted Mode and passive CMake integration.

`npm run test:navigation` launches an isolated VS Code host with existing source
and header fixtures, without CMake or a compiler. It checks the standard provider
commands, explicit schema/header commands, ignored build directories and unsaved
documents. Set `VSCODE_EXECUTABLE_PATH` to use an installed editor. Optional
`SERIALIZER_CPP_TOOLS_PATH` adds an installed Microsoft C/C++ extension to exercise
its real definition provider; otherwise the test supplies a controlled C++ provider.

`npm run test:integration` launches an isolated VS Code extension host and a real
CMake consumer under `out/extension-tests`. It requires CMake, a C++20 compiler,
and an installed CMake Tools extension. Set:

- `SERIALIZER_CMAKE_TOOLS_PATH`: directory containing CMake Tools' `package.json`.
- `VSCODE_EXECUTABLE_PATH`: optional path to VS Code's executable; otherwise the
  test runner downloads a test instance.
- `SERIALIZER_TEST_CMAKE_GENERATOR`: installed generator name; defaults to `Ninja`.

The test uses separate user data and extension directories, disables workspace
trust for its fixture only, and does not install into the user's normal editor.
It checks association, command registration, generation, header opening,
consumer compilation, and a malformed schema leaving the prior header intact.
The fixture disables generated-output formatting so it does not need clang-format.

### Verification performed

For VS Code extension version 1.1.2 on 2026-09-17, all 43 automated tests and the
isolated navigation host passed on Windows. Regressions use the maintained AUTOSAR
account schema and cover enum default type prefixes, exact schema destination
ranges, included/qualified enums, missing or stale generated definitions, unsaved
types, Restricted Mode, and CMake profile isolation. Existing generated type
matches retain priority; header-only commands remain silent without output.
The 1.1.2 VSIX was rebuilt and verified for matching manifest versions, the tested
navigation bundle, and updated packaged documentation.
No additional Linux/macOS, remote-host, clangd, or interactive F12 checks were
performed for this revision.

For VS Code version 1.1.1, all 38 existing automated tests and the isolated
navigation host passed on Windows. The file-menu actions share the existing
URI-based header-opening handler. The VSIX was rebuilt and checked for the new
command, Explorer/tab menu contributions, version metadata and bundled code.
Interactive rendering of the new context-menu items has not been checked.

For VS Code version 1.1.0, all 38 automated grammar/model/command/navigation tests
passed on Windows. An isolated VS Code host passed navigation checks without CMake
Tools, and a second run passed with Microsoft C/C++ 1.34.4 as the real definition
provider. These covered schema and C++ includes/types, merged include output,
hidden build directories, unsaved schemas and missing outputs. Provider tests
also covered Restricted Mode and passive use of an already active CMake model.
Existing generated headers from all nine C++ coding profiles passed 63
bidirectional type-location checks. The existing CMake Tools consumer regression
also passed generation, read-only header opening, compilation and failed-generation
checks. Linux/macOS, remote hosts and clangd have not
been exercised for navigation. Visual Studio's package remains highlighting-only.

For version 1.0.2, all four grammar tests passed on Windows, including unquoted
include paths, comments, invalid quoted paths, token scopes, and maintained schemas.
Both VSIX packages were rebuilt and their packaged grammar was checked against the
canonical source. Interactive installation/highlighting was not rerun for this update.

Previously, an isolated
VS Code host with CMake Tools 1.24.42 and the Visual Studio 18 2026 CMake generator
passed the end-to-end consumer checks above, including an expected failed build
for an unsupported schema version. Native Visual Studio, Linux/macOS hosts,
remote workspaces, and live C/C++ IntelliSense reparsing were not exercised.

The root installer was checked for the initial 0.1.0 package from outside the
repository with an isolated extension directory containing spaces: a full build/install passed, and a
Windows PowerShell 5.1 `-SkipBuild` reinstall passed with an explicit CLI path.
VS Code's extension listing confirmed `serializer-language@0.1.0` was installed.

## Publishing

The local VSIX can be distributed before Marketplace publication. To publish,
use the registered `rohitjairajsingh` Marketplace publisher, matching `publisher`
in `package.json`, and keep the extension ID stable thereafter. Increment the extension
version and update the package output name for each release. Review packaged files,
license, README, and changelog, then follow Microsoft's
[VS Code publishing workflow](https://code.visualstudio.com/api/working-with-extensions/publishing-extension).
The package includes the license for the bundled CMake Tools API helper.

A shared language server is not implemented. Package-manager recipes are also
outside this extension's implementation.

## Visual Studio extension

The separate [Visual Studio package](../editors/visual_studio/README.md), version
**1.0.2**, targets Visual Studio 2022/2026 on Windows x64. It includes the canonical
grammar, shared language configuration, repository license, and logo. The grammar's
`fileTypes` associates `.serializer` files; a `.pkgdef` registers the grammar and
its editing configuration. It contains no compiled extension code.

Build and validate with Windows PowerShell 5.1+ and Visual Studio's MSBuild:

```powershell
./editors/visual_studio/build.ps1
```

The script restores locked NuGet dependencies, rebuilds package intermediates, and writes
`out/extensions/serializer-visual-studio-1.0.2.vsix`. Close Visual Studio,
double-click this VSIX, install into the desired instance, and restart Visual
Studio. The root `install_extension.ps1` remains the VS Code installer.

This package configures highlighting, comment toggling, bracket/quote pairs, and
indentation. It does not port VS Code commands or snippets. Generate headers using
the consumer's existing CMake targets and keep include paths on those targets.
Semantic schema diagnostics, schema completion, and go-to-definition are not implemented.

The build's package check verifies identity, target architecture, `.pkgdef`
registration, manifest assets, and byte-for-byte agreement with canonical sources.
The Release build and package checks passed with Visual Studio 2026 MSBuild,
including Windows PowerShell 5.1 invocation from outside the repository. All 13
existing VS Code grammar/model/command tests passed with the shared grammar change.
Native Visual Studio installation, interactive editing, and Marketplace publication
remain pending. See the package README for the manual verification checklist.
