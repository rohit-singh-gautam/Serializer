# Rohit Serializer for Visual Studio

Version **1.1.6** provides `.serializer` highlighting and native navigation in
**Visual Studio 2022 and Visual Studio 2026 on Windows x64**. It shares the VS Code
extension's grammar and schema/generated-output resolver. Custom type references
such as `demo::order`, `demo::snapshot` and `demo::customer` use the active theme's
type and namespace colors.

## Navigation

- **Go to Declaration** on a schema include or type opens its source declaration.
  Extensionless/transitive includes, qualified names, containers, bases, enum-default
  prefixes and whole-name selections are supported, including unsaved schemas.
- **Go to Definition** (F12) and **Ctrl+click** on a schema prefer existing generated
  output; types fall back to their schema declaration when no output matches.
  Several destinations produce a picker.
- **Go to Declaration** on a generated type declaration maps it to its originating
  schema. C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin
  and C output are supported, including naming profiles and flattened names.
- In caller code, use its normal language service to reach the generated type,
  then Go to Declaration to open the schema. The adapter does not query external
  language services for a one-step caller-to-schema jump.

Retain the compiler's `--depfile` beside generated output or in the solution tree,
especially for renamed outputs and Python/Swift/Kotlin/C output without a banner.
CMake generation helpers already produce dependency metadata. Navigation only
reads existing files; it never saves, configures, builds or prompts to generate.

The extension also configures comment toggling, bracket/quote pairs and indentation.
It does not provide semantic diagnostics, completion, member/function navigation,
snippets or VS Code's CMake commands. Use existing CMake targets for generation.
See [usage](../../docs/usage.md), [CMake integration](../../docs/cmake_integration.md)
and the [navigation investigation](../../docs/editor_navigation.md).

## Download and install Serializer

The editor extension is separate from the Serializer compiler and runtime:

- [Download Serializer source (ZIP)](https://github.com/rohit-singh-gautam/Serializer/archive/refs/heads/main.zip)
  or [clone the repository](https://github.com/rohit-singh-gautam/Serializer).
- Follow the [build and installation instructions](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/cmake_integration.md#use-an-installed-package)
  to install the compiler, runtime library, headers, and CMake package.
- Use the [CMake integration guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/cmake_integration.md)
  to add Serializer to your project and generate code from schemas.

Installing this extension does not install the compiler or runtime.

## Build and install

Use Node.js 22+, npm, Windows PowerShell 5.1+, and Visual Studio 2022/2026 or its
Build Tools with MSBuild. The pinned SDK/reference packages restore from NuGet;
the separate Visual Studio SDK workload is not required.

From the repository root, `./make.ps1 all` builds Serializer and both editor
extension packages. `./editors/build.ps1` builds only the two packages, restores
locked npm dependencies, and checks that their versions match. Neither command
installs the packages. The individual Visual Studio build remains available:

```powershell
# From the repository root:
npm ci --prefix editors/vscode
./editors/visual_studio/build.ps1
```

The build script works from any directory and accepts `-MSBuildPath` to choose an
installation. It uses full-framework MSBuild with locked dependencies, bundles
the current shared resolver, rebuilds and validates:

```text
out/extensions/serializer-visual-studio-1.1.6.vsix
```

Close Visual Studio, double-click the VSIX, select the installation and restart
the IDE. **Extensions > Manage Extensions** lists **Rohit Serializer**. The root
`install_extension.ps1` is the separate VS Code installer. Marketplace publication
is pending.

## Implementation and verification

`navigation_editor.cs` exports native MEF command and Ctrl+click providers.
`navigation_bridge.ts` uses the shared resolver through a small host path adapter.
The bundle is embedded into `Rohit.Serializer.VisualStudio.dll` and interpreted
in-process by Jint. Node.js is only a build dependency; no compiler, Node runtime
or executable helper is needed to navigate. Interpreter dependencies and their
licenses are included in the VSIX.

Lookup uses the solution directory or repository/build markers for loose files,
skips dependency caches and directory links, and limits inventory to 100,000
relevant files. Disk reads skip files over 16 MiB. Background requests are bounded,
cancellable, and discarded if the initiating source/caret changes. Keep external
outputs within the discovered tree or retain a sibling `<output>.d` for reverse
navigation. The adapter does not use VS Code's CMake Tools configuration.

Edit the canonical `../serializer.tmLanguage.json` and shared
`../vscode/language-configuration.json`; the project links them at build time.
The stable extension ID is
`Rohit.Serializer.VisualStudio.40c33349-6c7b-42a6-b843-390d7120b1b9`.
Its release version must always equal the Visual Studio Code extension's version;
increment both together for future changes.
Commit both lockfiles when intentionally updating dependencies.

Run `build.ps1` to rebuild/validate the VSIX or `test_package.ps1` to recheck it.
The shared grammar and resolver tests run with `npm test` in `../vscode`.
After `npm run test:navigation:generated` produces real compiler output, build
`test/navigation_test.csproj` with MSBuild and run
`out/extension-tests/visual-studio/SerializerNavigationTest.exe <repository> <generated-complex-directory>`.
This checks the actual .NET interpreter, all 11 output languages, selection
endpoints, unsaved include changes and cancellation.
After installing the package, run the native editor test with Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File editors/visual_studio/test/test_host.ps1
```

It creates a hidden Visual Studio 2026 instance and a disposable solution, checks
native commands and reversed selections, then terminates only its own test host.
Use `-ProgId VisualStudio.DTE.17.0` to target a Visual Studio 2022 installation.
See the [verification record](../../docs/editor_extension.md#verification-performed)
for completed runs and remaining editor-host coverage.

The [installation target](https://learn.microsoft.com/en-us/visualstudio/extensibility/migration/extension-compatibility?view=visualstudio)
covers Community, Professional and Enterprise through the Community target.
ARM64 and earlier Visual Studio releases are not targeted. Grammar registration
uses Microsoft's [language configuration support](https://learn.microsoft.com/en-us/visualstudio/extensibility/language-configuration?view=visualstudio).
