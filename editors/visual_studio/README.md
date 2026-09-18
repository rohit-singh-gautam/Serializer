# Rohit Serializer for Visual Studio

Version **1.0.3** provides `.serializer` syntax highlighting and basic editing support
to **Visual Studio 2022 and Visual Studio 2026 on Windows x64**. It registers the
shared TextMate grammar and language configuration for comment toggling, bracket
pairs, quote completion, and indentation. Schemas begin with `serializer version 1;`.
The grammar highlights unquoted includes such as `include common;`
and `include ../shared/common;`, including their keyword and path.
Explicit `.serializer` includes remain supported. Shorthand requires an updated
Serializer compiler; schema filenames on disk keep their `.serializer` extension.

This is an asset-only VSIX: it runs no extension code or background process.
It does not provide semantic diagnostics, schema completion, go-to-definition,
snippets, or the VS Code extension's CMake commands. Use the project's existing
CMake generation targets for C++/Java output. See [usage](../../docs/usage.md) and
[CMake integration](../../docs/cmake_integration.md).

## Build

Use Windows PowerShell 5.1+ and Visual Studio 2022/2026 or its Build Tools with
MSBuild installed. NuGet access is needed on the first build. The pinned build
tools and .NET Framework reference assemblies restore automatically; the Visual
Studio SDK workload and Node.js are not required for packaging.

From the repository root:

```powershell
./editors/visual_studio/build.ps1
```

The script also works when invoked by absolute path from another directory.
Pass `-MSBuildPath '<path to MSBuild.exe>'` to select a particular installation.
It uses Visual Studio's full-framework MSBuild, not `dotnet build`.

Output:

```text
out/extensions/serializer-visual-studio-1.0.3.vsix
```

The script rebuilds package intermediates to refresh version metadata, then
validates the package's identity, installation target, registration,
and exact copies of the shared assets. It does not install the extension.

## Install and use

1. Close Visual Studio and double-click the generated VSIX.
2. Select the Visual Studio installation in the VSIX Installer and install.
3. Restart Visual Studio and open a `.serializer` file, such as
   `editors/vscode/test/fixture/account.serializer`.
4. Check keyword, type, string, number, and comment highlighting. Use **Edit >
   Advanced > Comment Selection**, type matching braces/quotes, and check indentation.

Use **Extensions > Manage Extensions** to find or uninstall **Rohit Serializer**.
The VS Code package and `install_extension.ps1` are for VS Code; use this VSIX for
Visual Studio. Marketplace publication is pending.

## Development

Edit `../serializer.tmLanguage.json` for grammar changes and
`../vscode/language-configuration.json` for shared editing rules. The project links
these files into the VSIX at build time, without a second maintained grammar.
`fileTypes` in the grammar registers `.serializer`; legacy `.def`/`.struct` files
are not associated. `serializer.pkgdef` registers the grammar repository and maps
`source.serializer` to the language configuration.

The manifest's stable extension ID is
`Rohit.Serializer.VisualStudio.40c33349-6c7b-42a6-b843-390d7120b1b9`. Its version is
independent of the compiler and VS Code extension versions. The build derives the
VSIX filename from this manifest. Commit `packages.lock.json` when deliberately
updating build dependencies; normal builds restore in locked mode.

The installation target follows Microsoft's
[Visual Studio compatibility model](https://learn.microsoft.com/en-us/visualstudio/extensibility/migration/extension-compatibility?view=visualstudio).
Grammar registration follows the
[language configuration documentation](https://learn.microsoft.com/en-us/visualstudio/extensibility/language-configuration?view=visualstudio).
Community, Professional, and Enterprise editions use the Community installation
target; ARM64 and earlier Visual Studio releases are not targeted by this package.

## Verification

Run `build.ps1` to build and validate the VSIX. Run `test_package.ps1` to recheck an
existing package. The shared grammar's tokenizer tests are in `../vscode`; with
Node.js 22+ and its npm dependencies installed, run `npm test` there.

Verified on Windows with Visual Studio 2026 MSBuild: the Release VSIX build and
package validation passed, including invocation from outside the repository under
Windows PowerShell 5.1 with an explicit MSBuild path. All 13 existing VS Code
grammar/model/command tests passed after adding the shared file association.

Native Visual Studio installation and interactive editing checks have not yet
been performed. Package validation and TextMate tokenizer tests do not establish
the IDE's actual rendering or editing behavior; use the installation checklist
above before publishing.
