# Rohit Serializer for Visual Studio

Version **1.1.44** supports language **1.15.0** fresh record defaults such as
`area = {}` in constructor and factory parameters, including empty record values
inside positional defaults. Signature and following field types retain navigation
through transitive includes and unsaved edits. New schema snippets select
`1.15.0`. See [owning APIs](../../docs/schema/owning_api.md).

Version **1.1.43** supports language **1.14.0** owning APIs: readonly field exposure,
arithmetic mappings and owning calls, reciprocal/external constructor signatures,
C++ declaration additions and typed nested defaults. It shares the current
grammar and navigation resolver with VS Code, preserving native services for
unrelated symbols. See [owning APIs](../../docs/schema/owning_api.md).

Version **1.1.42** supports schema language 1.13.0 C++ `noexcept(cpp)` function contracts. It highlights bodyless readonly declarations and native C++ methods, including either order with `constexpr(cpp)`. Signature type navigation and generic input scopes remain available; qualifier metadata does not claim unrelated schema symbols. New VS Code schema snippets select language 1.13.0 and include `readonly-noexcept-cpp`.

Version **1.1.40** supports schema language 1.12.0 overloaded constructors, nested constructor input types, factory-constrained owning parameter defaults, generic readonly method signatures and `common_type<T, Input>` result types. It highlights `cpp storage(none)`, `readonly constexpr(cpp)`, constructor `validate expression(...)` and `initialize cpp { ... };` contracts. Shared navigation retains independent input scopes and treats contract names as metadata. New VS Code schema snippets select language 1.12.0.

Version **1.1.38** highlights embedded target source using available TextMate
language grammars. `cpp { ... }`, `java { ... }`, and the other native tags select
their language's syntax inside method bodies, class declarations, and preambles.
Install a grammar provider for targets the Visual Studio instance does not bundle.
See [embedded source highlighting](../../docs/editors/editor_extension.md#embedded-source-highlighting)
and the [custom C++ example](../../example/cpp/custom_code/README.md).
This adds syntax coloring; IntelliSense and semantic diagnostics inside schema
blocks remain unavailable.

Version **1.1.37** adds language 1.10.0 namespace functions, declared readonly calls,
and Boolean generic type constraints. Shared navigation indexes namespace function
signature types and exact type alternatives. Language 1.9.0 features include stateless generic constraints,
computed `auto_size` fields, `wire_only` headers, `size_scope(record|payload)`,
`omit(all_except(...))`, and portable record conversions. Conversion target types
and target/source field projections navigate through included schemas and unsaved
edits, including inherited fields. Existing constructor, predicate, generic and
all-language generated navigation remains supported. New schema snippets select
language 1.13.0.

**Editor support for Serializer — State Framework**

Rohit Serializer helps you edit `.serializer` schemas and navigate between schemas
and existing generated code in Visual Studio. It provides syntax highlighting,
editing assistance, and native navigation for projects using Serializer — State Framework.

Version **1.1.31** provides `.serializer` highlighting and native navigation in
**Visual Studio 2022 and Visual Studio 2026 on Windows x64**. It shares the VS Code
extension's grammar and schema/generated-output resolver. Custom type references
such as `demo::order`, `demo::snapshot` and `demo::customer` use the active theme's
type and namespace colors.

Version **1.1.31** highlights language 1.5.0 field warning suppression, including
`ignore(warning magic, version)`. Warning names are metadata; adjacent types,
unnamed payload revisions, and fields named `ignore` or `warning` retain normal
navigation. The companion VS Code schema snippet selects language 1.5.0. Owning
`variant` and existing `union` declarations remain supported.

Version **1.1.28** shares language `1.3.0` digest highlighting and navigation:
`digest`, `digest[32]`, and `digest(sha256)`. Named algorithms and byte extents
are metadata; adjacent qualified types and legacy declared types named `digest`
retain navigation. VS Code's companion package adds digest snippets and uses
the `1.3.0` schema header. See the [digest contract](../../docs/schema/digest.md).

## Navigation

- **Go to Declaration** on a schema include or type opens its source declaration.
  Extensionless/transitive includes, qualified names, containers, bases, enum-default
  prefixes and whole-name selections are supported, including unsaved schemas.
- **Go to Type Definition** on a schema type opens its source class/enum declaration;
  on an include it opens the included schema. Generated-language type lookup remains
  with its native language service. Schema field names and primitives have no destination.
- All three actions accept declaration keywords and type names, including forward
  and reversed `class ledger` selections. Declaration/type definition on a type's
  own declaration select that same name; definition prefers generated output.
- On a schema include, **Go to Definition** opens the included schema too; an include
  can contribute multiple declarations to generated output, so it has no single generated target.
- **Go to Definition** (F12) and **Ctrl+click** on schema types prefer existing generated
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
See [usage](../../docs/guides/usage.md), [CMake integration](../../docs/guides/cmake_integration.md)
and the [navigation investigation](../../docs/editors/editor_navigation.md).

## About Serializer

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

The extension provides editor support; it does not install the Serializer compiler
or runtime. Application history, journals, and collaborative editing are C++
framework capabilities integrated by the application, not editing features supplied
by this extension. Other generated languages can exchange managed wire records;
native managed engines currently run in C++.

[Project website](https://www.singh.org.in/serializer.html) ·
[Serializer repository](https://github.com/rohit-singh-gautam/Serializer) ·
[Serialization guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/usage.md) ·
[C++ state-management guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/managed/getting_started.md)

## Download and install Serializer

The editor extension is separate from the Serializer compiler and runtime:

- [Download Serializer source (ZIP)](https://github.com/rohit-singh-gautam/Serializer/archive/refs/heads/main.zip)
  or [clone the repository](https://github.com/rohit-singh-gautam/Serializer).
- Follow the [build and installation instructions](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/cmake_integration.md#use-an-installed-package)
  to install the compiler, runtime library, headers, and CMake package.
- Use the [CMake integration guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/cmake_integration.md)
  to add Serializer to your project and generate code from schemas.

Installing this extension does not install the compiler or runtime.
For compiler/runtime 1.7.0 and newer, generated applications link the 0BSD
`Serializer::runtime` target; the generation helper supplies this automatically.
The GPL `Serializer::serializer_lib` target is reserved for compiler API use.
See the [licensing guide](../../docs/project/licensing.md) for proprietary applications
and source-schema notices. The editor extension retains its own repository license.

## Build and install

For C# editing in this repository, open `serializer_editors.sln` or set the VS Code
workspace's `dotnet.defaultSolution` to `editors/visual_studio/serializer_editors.sln`.
This loads the maintained extension and test projects without discovering old source
copies and third-party C# projects under `out/`. Both projects explicitly declare
Windows runtime identifiers so C# tooling and Visual Studio restore the same assets.

Use Node.js 22+, npm, Windows PowerShell 5.1+, and Visual Studio 2022/2026 or its
Build Tools with MSBuild. The pinned SDK/reference packages restore from NuGet;
the separate Visual Studio SDK workload is not required.

From the repository root, `./make.ps1 extension` builds both editor packages
without CMake configuration, native builds, or repository cleanup. `./make.ps1 all`
builds Serializer and both packages. The underlying `./editors/build.ps1` restores
locked npm dependencies and checks that their versions match. These commands do
not install the packages. WSL `make extension` delegates to the same Windows
packaging script through `powershell.exe` and `wslpath`; it requires the same
Windows tools. Native Linux cannot build this VSIX and requires a Windows/MSBuild
build host. The individual Visual Studio build remains available:

```powershell
# From the repository root:
npm ci --prefix editors/vscode
./editors/visual_studio/build.ps1
```

The build script works from any directory and accepts `-MSBuildPath` to choose an
installation. It uses full-framework MSBuild with locked dependencies, bundles
the current shared resolver, rebuilds and validates:

```text
out/extensions/serializer-visual-studio-1.1.44.vsix
```

Close Visual Studio, double-click the VSIX, select the installation and restart
the IDE. **Extensions > Manage Extensions** lists **Rohit Serializer**. The root
`install_extension.ps1` is the separate VS Code installer. A published extension is
available on [Visual Studio Marketplace](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio).
This source checkout may include changes not yet available in the published package;
compare the Marketplace version with the [extension changelog](CHANGELOG.md) before
relying on recently added behavior. Building a local VSIX does not publish it.

The maintained [Marketplace overview](marketplace_overview.md) is a separate payload
for the Visual Studio listing's Overview field. Updating or rebuilding this source
checkout does not apply that portal text; publish it with the corresponding reviewed
extension release. Keep the package description and listing overview aligned.

## Implementation and verification

The shared resolver supports generic declarations (`class box<T>`), nested type
arguments, and named instantiations. Parameters navigate to their local declaration;
generic definitions navigate to the C++ native template or existing concrete generated types.
Generated instances map back to the generic definition across all output languages.
Unused generic definitions have a C++ template destination; other languages require a concrete contract. See
[schema generics](../../docs/schema/generics.md) for the supported compiler profile.

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
Run `./editors/visual_studio/test/test_grammar.ps1` from the repository root to
check embedded source against the installed Visual Studio TextMate grammar assets
and the missing-provider fallback. It uses the locked TextMate tokenizer from
`editors/vscode/node_modules`; restore those dependencies first. This checks grammar
assets and block boundaries, rather than native editor UI colors.
After `npm run test:navigation:generated` produces real compiler output, build
`test/navigation_test.csproj` with MSBuild and run
`out/extension-tests/visual-studio/SerializerNavigationTest.exe <repository> <generated-complex-directory> [generated-generic-directory] [generated-inheritance-directory]`.
This checks the actual .NET interpreter, all 11 output languages, selection
endpoints, unsaved include changes and cancellation. The optional inheritance
directory also checks inherited field declarations with unsaved schema edits.
After installing the package, run the native editor test with Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File editors/visual_studio/test/test_host.ps1
```

It creates a hidden Visual Studio 2026 instance and a disposable solution, checks
native commands and reversed selections, then terminates only its own test host.
Use `-ProgId VisualStudio.DTE.17.0` to target a Visual Studio 2022 installation.
See the [verification record](../../docs/editors/editor_extension.md#verification-performed)
for completed runs and remaining editor-host coverage.

The [installation target](https://learn.microsoft.com/en-us/visualstudio/extensibility/migration/extension-compatibility?view=visualstudio)
covers Community, Professional and Enterprise through the Community target.
ARM64 and earlier Visual Studio releases are not targeted. Grammar registration
uses Microsoft's [language configuration support](https://learn.microsoft.com/en-us/visualstudio/extensibility/language-configuration?view=visualstudio).

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

Version 1.1.25 recognizes payload `version`/`compatibility`, `version2`/`version3`/`version4`, `created`/`obsolete`/`replaced`, and `reserve` syntax. Qualified field-type navigation remains available beside lifecycle metadata. See the [revision contract](../../docs/schema/versioning.md).

## Release policies (1.1.25)

Both extensions highlight `releases`, nested `policy` / `any` / `all`, and the `max_age`, `keep_last`, `released_since`, `expires_on`, and compatibility leaves. The schema compiler folds these into ordinary version bounds; generated readers contain no dates or policy tree. See the [release policy contract](../../docs/schema/versioning.md#release-dates-and-compile-time-policies).

## Magic and format exclusions (1.1.25)

Both editors recognize static `magic` declarations and field `omit(...)` annotations.
The compiler writes exact magic bytes before native binary payloads and verifies the
fixed `magic` JSON field by default. `omit(json)` explicitly removes it from JSON;
ordinary fields can exclude any supported format and retain their defaults on decode.
Magic byte strings remain literals, and adjacent schema type references retain navigation.

## Inferred arrays and typed magic (1.1.25)

Both packages recognize schema language `1.1.0` declarations such as
`public array[] uint32 values {1, 2, 3};` and `private magic uint32 (99) {42};`.
Empty extents and element types receive fixed-array highlighting. Enum type
operands and qualified enum defaults in typed magic retain declaration, definition,
and type-definition navigation, including unsaved and transitive included schemas.
VS Code includes `array_inferred` and `magic_typed` snippets; its schema snippet
selects `serializer version 1.15.0;`. The original `1` header remains exactly
`1.0.0` and does not enable these features. See the
[magic contract](../../docs/schema/magic_and_omission.md) and [fixed arrays](../../docs/schema/generics.md).

## Simplicity warnings (1.1.31)

The compiler suggests `magic` for manually modeled signatures and `version` for
ordinary payload version/revision fields. If an ordinary field is intentional,
append `ignore(warning version)` or `ignore(warning magic, version)` before its
semicolon. Suppression requires schema language `1.5.0`; it affects compiler
warnings and preserves the field's generated API and wire representation. Both
editors highlight suppression rules and preserve navigation beside them. Compiler
warnings appear in build output; this extension does not provide semantic diagnostics.

Omit the redundant name with `public version uint32 (1) { 1 };`. Its implicit
generated member remains `version`, preserving the API and wire identities.
Retain a custom revision name when existing consumers depend on it. See the
[revision contract](../../docs/schema/versioning.md).

## Compact integer fields (1.1.25)

Both packages recognize schema language `1.2.0` modifiers such as
`public compact_prefix strict uint32 value (3) {32};` and
`public compact_varint uint64 count (4);`. Encoding and overflow-policy keywords
receive modifier highlighting; generic type operands retain normal navigation.
The compiler supports compact fields in unpacked owning classes: the prefix encoding
holds at most 30 payload bits, while unsigned LEB128 varints retain the host type's
full range. `strict` is the default; `lenient` prefix output keeps the low 30 bits.
JSON and Protobuf scalar encodings retain their existing representation.
