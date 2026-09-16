# VS Code extension

The [Serializer extension](../editors/vscode/README.md) provides `.serializer`
syntax highlighting, snippets, and CMake generated-header commands. Schemas use
the current `serializer version 1;` header. Legacy `.def` and `.struct` names are
not registered. C++/Java generation remains owned by the project's build rules;
this initial extension's generated-file commands target C++ headers.

## Build and install locally

Use Node.js 22+ and npm. From the repository root:

```sh
cd editors/vscode
npm ci
npm test
npm run package
```

Packaging compiles and bundles TypeScript, copies the canonical grammar and
repository license into the extension, and writes:

```text
out/extensions/serializer-vscode-0.1.0.vsix
```

From the repository root, install it with:

```sh
code --install-extension out/extensions/serializer-vscode-0.1.0.vsix
```

Alternatively run **Extensions: Install from VSIX** and select the file. The
package is for VS Code, not Visual Studio. The publisher identifier in the local
manifest is not evidence of Marketplace registration or ownership.

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

**Serializer: Open Generated Header** uses the active schema's basename to locate
headers registered in CMake and asks which target/profile to use when ambiguous.
It offers to generate a missing file. Generated paths stay in the build tree.

Commands require workspace trust and CMake Tools; lexical editing does not.
Generation saves modified schema, INI, and CMake documents in the chosen workspace
folder. It can compile the Serializer generator but not the consuming target.
Nothing configures or builds automatically when opening or saving a schema.
See [IntelliSense troubleshooting](intellisense.md) for the underlying integration.

## Development and tests

The canonical TextMate grammar is `editors/serializer.tmLanguage.json`. Do not
edit the ignored copy under `editors/vscode/syntaxes`; packaging refreshes it.
The extension uses the public `vscode-cmake-tools` API. Keep compiler generation
and include directories on CMake targets rather than maintaining editor-only rules.

`npm test` checks the grammar using VS Code's TextMate/Oniguruma engines, tokenizes
the maintained schemas, and verifies configuration isolation, include search
order, and duplicate generated-header ownership.

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

On Windows, all 13 automated grammar/model/command tests passed. An isolated
VS Code host with CMake Tools 1.24.42 and the Visual Studio 18 2026 CMake generator
passed the end-to-end consumer checks above, including an expected failed build
for an unsupported schema version. Native Visual Studio, Linux/macOS hosts,
remote workspaces, and live C/C++ IntelliSense reparsing were not exercised.

## Publishing

The local VSIX can be distributed before Marketplace publication. To publish,
register a Marketplace publisher you control, update `publisher` in `package.json`
if needed, and keep the extension ID stable thereafter. Increment the extension
version and update the package output name for each release. Review packaged files,
license, README, and changelog, then follow Microsoft's
[VS Code publishing workflow](https://code.visualstudio.com/api/working-with-extensions/publishing-extension).
The package includes the license for the bundled CMake Tools API helper.

Visual Studio support and a shared language server are not implemented in this
release. Keep any future Visual Studio adapter separate and reuse the canonical
grammar. Package-manager recipes are also outside this extension's implementation.
