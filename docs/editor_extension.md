# Serializer editor extensions

Both extensions are for the [Serializer schema compiler and serialization library](https://github.com/rohit-singh-gautam/Serializer).
Their package descriptions and extension READMEs identify this repository directly.

Serializer provides separate packages for VS Code and Visual Studio. Both use
`editors/serializer.tmLanguage.json`; build assistance commands currently belong
to the VS Code extension. For the Visual Studio VSIX, see
[Visual Studio](#visual-studio-extension) below.

Both extensions use release version **1.1.19**. Keep their versions equal and
increment them together for future changes, including changes to only one package.

Version 1.1.14 restored VS Code navigation actions in Git index/history tabs.
It uses snapshot text for local symbols and the current workspace for includes
and generated destinations. Both extension packages remain versioned together.

Version 1.1.13 adds generic declarations, nested type arguments, and named
instantiations. Both extensions navigate local type parameters and generated
concrete types back to their schema definitions. VS Code also supplies `generic`
and `instantiate` snippets. Try the [runnable examples](../example/generics/README.md).

Both packages highlight custom type references such as `demo::order`,
`demo::snapshot` and `demo::customer` using theme type/namespace scopes. Field names
keep their ordinary identifier scopes. Both also highlight `include common;` with distinct scopes for
the keyword, unquoted relative path, and semicolon. Paths such as
`../shared/common` are supported; quoted paths are marked invalid. Shorthand resolves
directly to a `.serializer` file, and explicit `.serializer` paths remain accepted.
Both resolve either spelling for navigation; VS Code also supplies a shorthand
`include` snippet. Use an updated compiler to build shorthand includes. See
[schema include syntax](usage.md#share-declarations-with-includes).

## VS Code

The [Rohit Serializer extension](../editors/vscode/README.md), version **1.1.19**, provides `.serializer`
syntax highlighting, snippets, declaration/definition navigation, and CMake generated-header commands. Schemas use
the current `serializer version 1;` header. Legacy `.def` and `.struct` names are
not registered. Generation remains owned by the project's build rules. Navigation
supports C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin,
and C output; build and missing-header assistance target C/C++.

## Build and install locally

On Windows, `./make.ps1 all` builds the enabled CMake targets and then both
extension packages. It restores locked npm dependencies and rejects mismatched
extension versions. Run `./editors/build.ps1` to package just the extensions.
Both commands require Node.js 22+, npm and Visual Studio MSBuild, place the VSIX
files in `out/extensions`, and do not install them. `configure` and `test` remain
CMake-only. See [build requirements](cmake_integration.md#build-this-repository).

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
out/extensions/serializer-vscode-1.1.19.vsix
```

From the repository root, install it with:

```sh
code --install-extension out/extensions/serializer-vscode-1.1.19.vsix
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
**Serializer: Go to Implementation**. This opens existing generated output in any
supported language for the clicked file, including an included schema's entry output. It uses the
clicked file rather than whichever editor is active. Multiple available outputs
produce a picker; missing output does nothing and never prompts for generation.
The command is also available in the Command Palette for the active schema.

Use the editor's built-in **Go to Declaration**. It opens a schema include's file
or the original class/enum declaration for a type reference. Qualified references
such as `demo::order`, array/map/union types, bases and enum-default prefixes work
on either name component and at a selection's endpoint. Schema declarations do
not consult CMake Tools. From C/C++ includes the action opens the entry schema;
from caller type references in any supported output language it maps that
language service's resolved generated type back to its original schema.

**Go to Definition** on a schema include opens the included schema, as does Go to
Declaration: an include can contribute multiple declarations to generated output
and has no single generated target. On a schema type it selects the generated type
definition, falling back to the original
schema declaration when no matching generated definition is available. This also
works on `AccountState` inside a default such as `AccountState::WaitingForReview`;
the enum value itself is not a type reference. Included `account.serializer`
may be emitted in `request.hpp`, so navigation follows include relationships.
Caller definitions continue to use their language service. The extension
also supplies generated-header locations for literal C/C++ includes.

**Go to Type Definition** on schema types selects their source class/enum declaration,
including qualified references and unsaved transitive includes. On an include it
opens the included schema. The provider is restricted to `.serializer`; native type
lookup in generated languages remains unchanged. Field names and primitives have no
schema type-definition destination.

All three commands recognize declaration keywords as well as names, including forward
and reversed selections of `class ledger`. On the declaration itself, declaration and
type definition select the same schema name; definition opens matching generated code.

These actions and **Open Generated Header** never save, configure, build, generate,
activate CMake Tools, or offer generation. Unresolved types and missing header-only
destinations return no result; schema type definitions can still use the source fallback.
An already active CMake configuration supplies output candidates for its registered
languages and C/C++ include paths. Other languages remain discoverable in the
current workspace folder, including ignored build trees.
Sibling `<output>.d` and workspace multi-output `.d` dependency files identify entry schemas. Legacy outputs
without depfiles use the generated-file banner and schema basenames, with ambiguous
matches exposed as choices. Naming profiles and storage-mode specializations are
recognized, as are Java namespace containers, flattened portable/native types,
C typedefs and TypeScript enum aliases. Unsaved schema contents participate in lookup; stale output is not
regenerated or represented as current.

Caller type references require their installed language's definition provider.
VS Code merges providers' declaration results; **Serializer: Go to Schema Declaration**
is available in the Command Palette for only schema destinations. If ordinary
definition stops at an alias or variable, this explicit command asks the native
type-definition provider for the underlying generated type. Native declaration,
definition and type-definition commands retain their language semantics. The normal
context menu uses the native action, and other providers remain enabled.
For precise mapping, retain the compiler's `--depfile` output in the workspace.
For example, `--output generated/schema.py --depfile generated/schema.py.d`
lets Python declarations map back to their entry and included schemas. A shared
`--depfile generated/model.d` supports multiple outputs. Renamed outputs and
native outputs without a banner (Python, Swift, Kotlin and C) need this metadata.
This is a lexical schema index, not a full language
server. Member/function navigation and semantic diagnostics remain unimplemented.
See the [extension README](../editors/vscode/README.md#limits) for discovery limits.
See the [navigation investigation](editor_navigation.md) for the reported failure,
editor API findings, regression coverage and remaining verification boundaries.

## Development and tests

For this repository's managed ledger examples/tests, enable the optional targets:

```powershell
./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_MANAGED=ON'
```

Pass the option again after removing the CMake cache. The default is OFF, so a
successful clean default build can omit `ledger.hpp` and the managed test targets.
Select that same build/configuration in CMake Tools and run **Serializer: Configure
IntelliSense** to supply per-target include paths to Microsoft C/C++. Do not merge
all generated directories into one global include path. Building VSIX files does
not install them or reload the editor. See the
[coverage matrix](editor_navigation.md#navigation-coverage-matrix) for command
semantics, missing-file behavior and verification boundaries.

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

`npm run test:navigation:generated` invokes the current compiler specified by
`SERIALIZER_COMPILER` and checks every complex-model type in all 11 outputs in
both directions. It also covers acronym/digit naming and preserve profiles using
fresh output and a shared depfile. It does not require the target-language SDKs.

`npm run test:navigation:project` additionally uses the installed CMake Tools and
Microsoft C/C++ extensions, specified by `SERIALIZER_CMAKE_TOOLS_PATH` and
`SERIALIZER_CPP_TOOLS_PATH`. It explicitly generates the actual managed ledger
schema into separate target directories, then checks qualified references, aliases,
alias chains, variables, native editor actions and schema-only lookup. It first
verifies that navigation before generation leaves missing output missing.

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

Version **1.1.16** passed all 74 shared tests and 13,376 fresh-compiler navigation
checks across all eleven output languages, including versioned models. The Visual
Studio .NET resolver passed 89 checks. Both packages were rebuilt and validated;
installation and interactive IDE-host checks were not rerun. See the
[versioning verification record](verification-versioning-2026-10-06.md).

Version **1.1.12** passed 66 shared/provider tests, 6,007 fresh-compiler navigation
checks across all 11 output languages, and 117 checks in the new CMake Tools /
Microsoft C/C++ host. That host covers duplicate aliases, alias chains, variables,
target-specific headers, native commands and reversed selections. Navigation before
explicit generation leaves output missing; provider tests repeat that requirement
for every output language. The existing VS Code host also passed its schema,
include, unsaved-buffer and real TypeScript service checks.

The exact `ledger_example::ledger` expression in this checkout's
`test/managed_direct_test.cpp` was separately verified with CMake Tools pointing
at `out/build/make-Release` and `SERIALIZER_BUILD_MANAGED=ON`: declaration included
the ledger schema, and definition/type definition reached
`test/managed_direct/ledger.hpp` in that build. No unrelated test aliases appeared.
Both 1.1.12 packages were rebuilt, validated and installed. The Visual Studio .NET
resolver passed 42 checks, and the installed native Visual Studio 2026 host passed
all declaration/definition/type-definition, selection and reverse-navigation checks.
Native non-C++/TypeScript language services and other platforms remain unverified;
see the [coverage matrix](editor_navigation.md#navigation-coverage-matrix).

Version **1.1.11** adds schema type-definition navigation and declaration-keyword
support. All 60 shared tests, 821 fresh-compiler navigation checks and the isolated
VS Code native-command test passed, including forward/reversed `class ledger`
selections. The Visual Studio .NET resolver passed 42 checks across all 11 languages.
Both VSIX packages were rebuilt and installed; native Visual Studio verification
passed declaration, definition and type-definition commands for `class ledger`
selections in both directions, plus existing complex-model and reverse-navigation
checks. Mixed line endings in the local ledger schema were normalized before the
host run. Both C# projects passed locked restore through `dotnet` and Visual Studio
MSBuild after aligning their Windows runtime identifiers. The repository's editor
solution limits project discovery to the maintained extension/test projects.

For version **1.1.10**, all 57 shared tests and 821 fresh-compiler navigation checks
passed, including default direct managed classes and opt-in separated companions
across every C++ profile. Both packages were rebuilt and the Visual Studio package
validator passed. Installation and interactive IDE checks were not rerun.

For version **1.1.9**, all 57 shared grammar/model/command/navigation tests passed.
Fresh compiler output passed 659 bidirectional navigation checks across all 11
output languages and all C++ profiles, including managed data/storage/editor
companions. Both VSIX packages were rebuilt; the Visual Studio package validator
passed. Installation and interactive editor-host tests were not rerun for this change.

For both extensions at **1.1.8**, the package descriptions and opening README
paragraphs identify the Serializer repository explicitly. All 54 existing VS Code
grammar/model/command/navigation tests passed. Both VSIX packages were rebuilt;
package inspection checked the repository descriptions, versions, and stable IDs,
and the Visual Studio package validator checked bundled assets and dependencies.
The Visual Studio NuGet lockfile now includes the Windows runtime targets required
by the installed toolchain; existing dependency versions are unchanged. These
restore targets do not extend the VSIX beyond its declared Windows x64 support.
Installation and interactive editor-host tests were not rerun for this metadata
update. Managed-state examples remain design documentation, with no parser/runtime
implementation or compiled history examples.


For both extensions at **1.1.5**, `./make.ps1 all` completed the real CMake build
and produced both VSIX packages. Standalone `editors/build.ps1` also passed under
Windows PowerShell 5.1 from outside the repository with an explicit MSBuild path
containing spaces. Package versions and canonical grammar contents matched the
source; Visual Studio's package validation passed. Controlled failure checks
confirmed that failed CMake configuration/compilation stop packaging, unequal
extension versions are rejected, and `configure`/`test` remain CMake-only.
Navigation code was unchanged, so its earlier tests were not repeated for this
build integration. These build checks did not install either extension.

The version synchronization on 2026-09-18 rebuilt and validated Visual Studio
**1.1.4** with unchanged navigation code, installed it, and confirmed both installed
extensions report **1.1.4**. The installed Visual Studio assembly matches the new
build. Source manifests and VS Code lockfile version entries also agree. The
navigation tests below were not rerun for this metadata-only version change.

For VS Code **1.1.4** and Visual Studio **1.0.4** on 2026-09-18:

- All 54 automated grammar/model/provider tests passed, including the reported
  complex model, whole-name selections and custom-type token scopes.
- Fresh compiler output passed 335 bidirectional type checks across all 11
  languages, nine C++ profiles, three Java profiles, preserved names and acronyms.
- The isolated VS Code navigation host passed native declaration commands with
  forward/reversed selections and real Microsoft C/C++ 1.34.4 and TypeScript
  definition results, plus unsaved schemas and ignored output directories.
- Visual Studio's actual .NET interpreter passed 42 source/output checks across
  all 11 languages, selection endpoints, unsaved includes and cancellation.
  Its Release VSIX built without warnings and passed package/asset validation.
- Both packages were installed locally for this run. A new hidden Visual Studio 2026
  instance passed eight native-command checks: forward/reversed selections of
  `demo::order`, `demo::snapshot` and `demo::customer`, schema-to-generated-header
  definition and generated-declaration-to-schema navigation. The reproducible test
  is `editors/visual_studio/test/test_host.ps1`.

Other language services, Linux/macOS, remote hosts and Visual Studio 2022 have not
been exercised interactively for these revisions. Compiler-format coverage does
not establish the behavior of every external language-service extension. Ctrl+click
mouse gestures and multiple-destination dialogs in Visual Studio have not been
automated; their resolver paths and package registrations are covered separately.

For VS Code **1.1.3** and Visual Studio **1.0.3** on 2026-09-18, all 45 automated
editor tests passed, including shorthand paths, mixed include spellings, source
ranges, missing files, and tokenization of the migrated repository schemas. The
isolated VS Code navigation host passed with extensionless includes. Both VSIX
packages were rebuilt and validated for their versions and shared grammar; the
VS Code bundle, snippet, and updated packaged documentation were also checked.
Native Visual Studio installation and interactive highlighting were not rerun.

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
been exercised for navigation. Visual Studio was highlighting-only at that revision.

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
**1.1.19**, targets Visual Studio 2022/2026 on Windows x64. It includes the canonical
grammar, editing configuration and a native MEF navigation component using the
same resolver as VS Code. The grammar's `fileTypes` associates `.serializer` files;
a `.pkgdef` registers its grammar and editing configuration.

Build with Node.js 22+, Windows PowerShell 5.1+ and Visual Studio's MSBuild:

```powershell
npm ci --prefix editors/vscode
./editors/visual_studio/build.ps1
```

The script restores locked NuGet dependencies, rebuilds package intermediates, and writes
`out/extensions/serializer-visual-studio-1.1.19.vsix`. Close Visual Studio,
double-click this VSIX, install into the desired instance, and restart Visual
Studio. The root `install_extension.ps1` remains the VS Code installer.

Use native **Go to Declaration** on schema includes/types or generated type
declarations. **Go to Definition** and **Ctrl+click** in schemas prefer existing
generated output and fall back to the original schema type. **Go to Type Definition**
opens source schema types/includes, leaving other languages to their native services.
All 11 output languages
are supported through dependency metadata and generated-name mapping. From caller
code, first reach the generated declaration with that language's native service;
the adapter does not request external definition locations as VS Code does.

For C# editing of the extension, select `editors/visual_studio/serializer_editors.sln`
with VS Code's workspace `dotnet.defaultSolution` setting. This avoids loading
temporary source copies and third-party projects under `out/`. Both maintained
projects declare Windows runtime targets explicitly to keep editor/Visual Studio
dependency restores consistent.

The resolver runs in-process; Node.js is only required at build time. Navigation
captures unsaved documents and performs bounded, cancellable background lookup.
No navigation action builds or changes files. This package also configures comment
toggling, bracket/quote pairs and indentation. Use existing CMake targets for
generation. Semantic diagnostics, completion, member/function navigation, VS Code
CMake commands and snippets remain outside this package.

Package checks verify identity, architecture, grammar/MEF registration, interpreter
dependencies and notices, and byte-for-byte agreement with canonical assets.
See [verification](#verification-performed) and the package README for native-host
coverage, build steps and resolver tests. Marketplace publication remains pending.

## Managed interface support (1.1.10)

Both packages highlight bare `managed` class/member annotations. Schema navigation
skips this modifier and resolves direct, array, and map value types, including
qualified names. Generated C++ `managed_<type>_data`, `managed_<type>_storage`, and
`<type>_editor` declarations navigate back to the source schema. Forward schema
navigation continues to prefer the ordinary generated type. Selectors and non-C++
managed runtimes are not implemented; see the [managed API](managed/cpp_runtime.md).

Default managed output places `persistent_id` on the schema class. The optional
`[managed] separate_values = true` output retains ordinary classes and managed
storage/data companions. Navigation qualification generates both representations
across every C++ naming profile.

## Native templates and dimensions (1.1.15)

Both packages recognize `uint64 N`, trailing defaults, fixed `array[N * M] T`
expressions, nested generic applications, and optional `instantiate` contracts.
Navigation resolves dimension/type parameter references and defaults to their
local declarations, including unsaved text. C++ definitions include native templates
with no concrete schema uses. Other languages have destinations only for concrete
contracts; fixed-array output is currently unsupported there. The shared grammar
highlights dimension arithmetic and the `matrix` snippet inserts a fixed-array
example. Both packages use the same navigation implementation.

## Payload revisions (1.1.16)

Both extensions share highlighting for revision types, compatibility, lifecycle and reservation syntax. Version metadata is excluded from type-reference navigation; qualified types on annotated fields retain normal navigation. See [versioning](versioning.md).

## Release policies (1.1.17)

Both packages highlight release catalogs, nested allow conditions, date/count leaves, and compatibility inside policy trees. The shared navigation resolver preserves qualified field-type references beside this metadata. See [release policies](versioning.md#release-dates-and-compile-time-policies).

Version 1.1.17 passed all 76 shared tests, 13,376 fresh-compiler navigation checks,
and 89 Visual Studio .NET resolver checks. Both packages were rebuilt and validated;
installation and interactive IDE-host tests were not rerun. See the
[policy verification record](verification-release-policies-2026-10-06.md).

## Magic and format exclusions (1.1.19)

Both packages highlight static magic declarations and generic `omit(format, ...)` annotations.
The shared magic snippet uses the portable `SRLFILE` signature. Qualified type
navigation remains available beside the metadata. See the
[magic and omission contract](magic_and_omission.md) and its
[verification record](verification-magic-2026-10-07.md).
