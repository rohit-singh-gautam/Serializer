# Rohit Serializer for Visual Studio Code

Compiler/runtime **2.33.0** requires a C++23 compiler and matching standard
library for generated C++ applications. Both local packages are **2.0.18**.
They support schema language **1.23.0** and earlier compatible headers.

Compiler/runtime **2.33.0** also provides opt-in Java caller-owned output
windows through `java.output_buffer_reuse = true`. Both packages retain type/field
navigation around the additional generated overloads; method parameters remain
ordinary Java symbols. See the [Java ownership and failure contract](../../docs/languages/java.md#caller-owned-output-windows).

Compiler/runtime **2.33.0** independently adds heap/direct `ByteBuffer` windows
through `java.nio_output_buffer_reuse = true` (default false). Generated type/field
navigation retains ordinary Java parameter and local-variable behavior around
these qualified overloads. Cursor/ownership and failure bounds are documented in
the [Java NIO API](../../docs/languages/java.md#caller-owned-nio-buffers).

Language **1.23.0** binds direct self-token fields in a named managed generic
instantiation to that named C++ record. The instantiate declaration selects the
contract; older included declarations retain their behavior. Navigation preserves
the named declaration and copied generic field provenance. See the
[binding contract](../../docs/schema/named_managed_references.md) and
[qualification status](../../docs/verification/verification-named-managed-references-2026-10-24.md).

Version **2.0.18** also prepares namespace/tag and brace indexes for generated
C++ and limits field searches to complete candidate names. The native adapter
passes direct and separate-values named-reference navigation within its original
256 MiB budget; authored C++ retains its existing indexing behavior. See the
[allocation qualification](../../docs/verification/verification-native-cpp-navigation-2026-10-24.md).

Version **2.0.18** indexes generated C declarations without tokenizing bundled
runtime implementation bodies. Type, enum, package and field destinations retain
their source offsets; the Visual Studio interpreter keeps its existing resource limits.
See the [native navigation qualification](../../docs/verification/verification-native-c-navigation-2026-10-24.md).

Version **2.0.18** supports schema language **1.22.0** cache declarations for
compiler/runtime **2.30.0**: bare `cache` defaults to priority `0`, and
`cache(priority = 200)` gives a higher retention preference within `0..255`.
Owning cache values and native persistence services currently require C++; other
outputs diagnose these declarations explicitly. Cache type navigation preserves
qualified names, transitive includes, unsaved edits and ordinary types named `cache`.
The new shared `.<filename>.serializer/` companion directory separates recovery
journals, retained caches and required state. See the
[cache contract](../../docs/design/cache_attributes.md). Building packages does
not install them or publish Marketplace listings.

Both local packages share version **2.0.18**. Building and validating these files
does not install the extensions or update live Marketplace listings.

The existing schema language **1.20.0** authoring support includes XML defaults, field overrides,
namespace `using schema` declarations and declarative package roles. Qualified
role model types navigate to their declarations; fixed XSD paths open the local
file without treating it as a Serializer include or fetching remote resources.
XML policy/placement tokens do not claim type destinations. VS Code also provides
XML/package authoring snippets. Compiler/runtime providers and supported vocabulary
remain prerequisites; editor support does not establish format conformance.
Typed C++ `managed_ref<T>` tokens navigate to the managed target, including forward
and included declarations. `class_id(decimal)` preserves explicit class identity;
its numeric metadata does not become a type reference. VS Code supplies token and
class-identity snippets. Both local packages share version 2.0.18; building them does
not install or publish them.


Generated C#/Java package classes, Kotlin package objects, JavaScript package factories and TypeScript namespace facades
navigate to their schema package declarations and back, preserving enum
navigation, naming profiles, live edits and cursor boundaries.

Version **2.0.4** supports language **1.19.0** inline value references such as
`public borrowed(payload) d (1);`, including payload-type and optional owner-field navigation.
Borrowed views use application-owned storage; shared/weak modes are optional.
Output contains the ordinary field value.
Scoped translators can project payload records containing arrays without copying their
buffers. Language 1.18 named durable address references remain supported separately.
See [value references](../../docs/schema/value_references.md) and
[durable addresses](../../docs/schema/references.md). Generated helper-type navigation
for anonymous inline wrappers is not a separate authored schema declaration.
Both editor packages remain synchronized. Local package verification does not
publish or update the live Marketplace listing.


Version **2.0.2** documents compiler/runtime **2.3.0**'s optional generated XML
codecs and fixed XSD bindings across output languages. Select XML in the compiler's INI/CLI
configuration; schema language **1.17.0** and existing generated-output navigation
remain unchanged. XML/XSD validation, ZIP/OPC packaging, and compression run in
the compiler/runtime; this extension adds no XML, XSD, or HTML language service.
See the [XML and package support report](../../docs/design/xml_support.md) for
implemented subsets, provider dependencies, and verification by backend. Both editor package versions match.

Version **2.0.1** updates CMake project fixtures and build prerequisites for
compiler/runtime **2.1.0**, which requires CMake **4.0+** and provides standalone
CMake entry points for individual examples in every output language. Both editor
packages remain synchronized; schema language **1.17.0** remained current for that release.

Version **2.0.0** refreshes Marketplace documentation for schema language
**1.16.0/1.17.0** and the default separate C++ generated headers in compiler/runtime
**2.0.0**. Editor, schema language, and compiler release numbers are independent;
both editor packages remain synchronized. Existing navigation behavior is unchanged.

Version **1.1.48** supports the compiler's default separate C++ include files.
Included types and fields open their own generated headers, including transitive
includes and renamed entry outputs. Compiler source comments identify the exact
schema even when several schemas share a basename. An active CMake configuration
retains the generated headers linked by its entry output. Use `--include-mode inline`
or `[output] include_mode = inline` for combined C++ output; navigation supports both.

Version **1.1.47** adds **Go to Definition** from typed external schema function
names to available authored C++ out-of-class definitions. The shared resolver
matches qualified templated owners and overload signatures, reads live unsaved
buffers, and exposes ambiguous matches as choices. Lookup is bounded and never
builds or generates. Native blocks remain opaque; this is lexical navigation,
not a C++ language service. See
[supported forms and limits](../../docs/editors/editor_navigation.md#deferred-function-definitions-1147)
and [qualification status](../../docs/verification/verification-deferred-navigation-2026-10-13.md).

Version **1.1.46** supports schema language **1.17.0** array exposure,
`readonly`/`const_value`, explicit `dimensions`, trailing `const_ref` inputs,
static/expected callable signatures and `when`, `delegates`, `generate` metadata.
Qualified type and dimension navigation keeps metadata names separate from
schema symbols. New-schema snippets select `1.17.0`. Source unit tests, native
grammar/navigation and current local packages are recorded with
[qualification](../../docs/verification/verification-array-callable-api-2026-10-13.md).

Version **1.1.45** supports language **1.16.0** defaults such as
`Frame placement = {}` for owning class type parameters in constructors and
named factories. Existing generic signature navigation and brace-default
highlighting apply without new grammar tokens. New schema snippets select
`1.16.0`. Native C++ omission uses `Frame{}` without a class-wide defaultability
constraint; backend and deduction limits are described in
[generic owner defaults](../../docs/schema/construction.md#generic-owner-parameter-defaults-in-language-116).
See [qualification](../../docs/verification/verification-generic-owner-defaults-2026-10-12.md)
for the executed verification status.

Version **1.1.44** supports language **1.15.0** fresh record defaults such as
`area = {}` in constructor and factory parameters, including empty record values
inside positional defaults. Signature and following field types retain navigation
through transitive includes and unsaved edits. New schema snippets select
`1.15.0`. See [owning APIs](../../docs/schema/owning_api.md).

Version **1.1.43** supports language **1.14.0** owning APIs: readonly field exposure,
arithmetic mappings and owning calls, reciprocal/external constructor signatures,
C++ declaration additions and typed nested defaults. Grammar and type/member
navigation recognize these contracts without claiming unrelated symbols. New
schema snippets select `1.14.0` and include readonly getters, arithmetic mapping
and constexpr constructor declarations. See [owning APIs](../../docs/schema/owning_api.md).

Version **1.1.42** supports schema language 1.13.0 C++ `noexcept(cpp)` function contracts. It highlights bodyless readonly declarations and native C++ methods, including either order with `constexpr(cpp)`. Signature type navigation and generic input scopes remain available; qualifier metadata does not claim unrelated schema symbols. New VS Code schema snippets select language 1.13.0 and include `readonly-noexcept-cpp`.

Version **1.1.40** supports schema language 1.12.0 overloaded constructors, nested constructor input types, factory-constrained owning parameter defaults, generic readonly method signatures and `common_type<T, Input>` result types. It highlights `cpp storage(none)`, `readonly constexpr(cpp)`, constructor `validate expression(...)` and `initialize cpp { ... };` contracts. Shared navigation retains independent input scopes and treats contract names as metadata. New VS Code schema snippets select language 1.12.0.

Version **1.1.38** highlights embedded target source using available language
grammars. `cpp { ... }`, `java { ... }`, and the other native tags select their
language's syntax inside method bodies, class declarations, and preambles. VS Code
also receives the embedded language identity for basic editing operations.
Install a grammar provider for languages the editor does not bundle. See
[embedded source highlighting](../../docs/editors/editor_extension.md#embedded-source-highlighting)
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
and existing generated code in Visual Studio Code. It provides syntax highlighting,
snippets, and CMake assistance for projects using Serializer — State Framework.

Edit `.serializer` schemas with syntax highlighting, bracket matching, comments,
folding, and snippets. Version **1.1.31** includes navigation from includes and type
references to source schemas and existing generated code in all 11 output languages. Use the project's
CMake configuration for the separate build and missing-header assistance commands.

Version **1.1.31** highlights language 1.5.0 field warning suppression, including
`ignore(warning magic, version)`. Warning names are metadata; adjacent types,
unnamed payload revisions, and fields named `ignore` or `warning` retain normal
navigation. New schema snippets select language 1.5.0. Owning `variant` and existing
`union` declarations remain supported.

Version **1.1.28** recognizes language `1.3.0` digest fields: `digest`,
`digest[32]`, and `digest(sha256)`, including all twelve supported algorithm
names. Algorithm selectors and byte extents are metadata rather than navigation
destinations; adjacent types and older declared types named `digest` retain their
normal navigation. Use `digest`, `digest-opaque`, or `digest-fixed` snippets and
see the [digest contract](../../docs/schema/digest.md). New schema snippets use `1.5.0`.

```text
serializer version 1.0.0;

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
quoted paths and angle brackets are invalid. See [schema includes](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/usage.md#share-declarations-with-includes).

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

## Install and use

From the repository root on Windows, `./make.ps1 extension` builds both editor
VSIX packages without CMake configuration, native builds, or repository cleanup.
`./make.ps1 all` builds Serializer and both packages; `./editors/build.ps1` remains
the underlying packaging script. These commands
require Node.js 22+, npm and Visual Studio MSBuild, restore locked dependencies,
and write to `out/extensions` without installing either extension.
WSL `make extension` invokes that Windows script through `powershell.exe` and
`wslpath`; it needs the same Windows tools. Native Linux cannot build both packages
because the Visual Studio VSIX requires Windows/MSBuild. Individual VS Code-only
development remains available through `npm` commands.

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

The project still needs Serializer, a C++23 compiler, CMake 4.0+, and clang-format
19+ when output formatting is enabled. This extension does not bundle those tools.

## Declaration and definition navigation

Git index and history tabs (including read-only `(Index)` views) support the
same declaration, definition, and type-definition actions. Cursor offsets and
local symbols come from the displayed snapshot. Includes, other schema files,
and generated output resolve against the current workspace; this does not
reconstruct a historical checkout. With an older extension, open the working
file from Explorer to use navigation, or install the current extension package and reload VS Code.

Caller navigation depends on the language service's active project configuration.
For this repository's managed ledger examples/tests, use
`./make.ps1 all`, select that same build in CMake Tools, and run
**Serializer: Configure IntelliSense**. Fresh builds enable managed support;
existing caches with an OFF value need `-DSERIALIZER_BUILD_MANAGED=ON`.
Building packages does not install them or reload the editor.

For `using ledger = ledger_example::ledger;`, selecting the right-hand type should
resolve the generated class. Selecting a later use of the left-hand alias normally
opens the `using` declaration; **Go to Type Definition** follows its underlying class.
**Serializer: Go to Schema Declaration** follows either to the original schema,
including through alias chains and variables when a native type provider is available.
The [coverage matrix](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/editors/editor_navigation.md#navigation-coverage-matrix)
records required cases and remaining native-host verification limits.

Right-click a `.serializer` file in **Explorer** or its **editor tab**, then choose
**Serializer: Go to Implementation** to open existing generated output.
The command uses the clicked file, even when another editor is active. Multiple
available outputs produce a picker; missing output produces no result or build prompt.

| Selected item | Go to Declaration | Go to Definition |
| --- | --- | --- |
| Schema `include types/account;` | Included schema | Included schema |
| Class/enum declaration or type reference in a schema | Original schema declaration | Matching generated type definition; schema declaration if unavailable |
| Typed external schema function name | Existing declaration behavior | Available authored C++ out-of-class definitions matching the owner and overload signature; schema function name if unavailable |
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
produce no result. By default, `account.serializer` included by `request.serializer`
is implemented in a separate header under `request.hpp.includes/`. With inline
generation it maps to `request.hpp`. Unsaved schema edits are used for declaration lookup.

An already active CMake Tools configuration narrows discovery for its generated
languages and supplies target-specific C/C++ include paths. Other output languages
remain discoverable. Navigation searches the current workspace folder, including
ignored build directories. Existing `<output>.d` or multi-output `.d` dependency
files identify entry schemas precisely. Current C++ output also records its exact
origin in a `// Serializer source:` comment, including each separate header.
Older banner-marked outputs without a
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
definition. See [schema generics](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/generics.md) for compiler limits.

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
- Syntax highlighting and schema indexing are lexical. Typed external function
  names support authored C++ definition lookup; general expression/member-call
  navigation, semantic diagnostics, completion and a language server are unavailable.
  Navigation skips unopened files larger than 16 MiB and bounds include traversal.
- For Visual Studio 2022/2026, use the separate
  [Visual Studio extension](https://github.com/rohit-singh-gautam/Serializer/tree/main/editors/visual_studio).
  It shares the grammar, custom-type highlighting and navigation resolver, and
  supplies native schema declaration/definition commands. The CMake commands and
  snippets above belong to VS Code. Visual Studio caller references first use
  their language service to reach the generated declaration.

See the repository's [extension guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/editors/editor_extension.md)
for development, validation, and packaging instructions. A published extension is
available on [Visual Studio Marketplace](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.serializer-language).
This source checkout may include changes not yet available in the published package;
compare the Marketplace version with the [extension changelog](https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/vscode/CHANGELOG.md) before
relying on recently added behavior. Building a local VSIX does not publish it.

The C++ `managed` class/member keyword is highlighted and skipped when locating a
member's type. Direct, array, and map managed references retain declaration and
definition navigation. Generated managed data, storage, and editor class declarations
map back to the original schema type; schema-to-output navigation still selects the
ordinary class. Capability selectors and other-language managed runtimes remain
unimplemented; see the [managed runtime guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/managed/cpp_runtime.md).

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
contracts; ordinary owning fixed-array codecs are available in all eleven outputs.
Generated C++ Protobuf uses repeated fields with exact reader cardinality.
The shared grammar
highlights dimension arithmetic and the `matrix` snippet inserts a fixed-array
example. Both packages use the same navigation implementation.

Version 1.1.25 recognizes payload `version`/`compatibility`, `version2`/`version3`/`version4`, `created`/`obsolete`/`replaced`, and `reserve` syntax. Qualified field-type navigation remains available beside lifecycle metadata. See the [revision contract](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/versioning.md).

## Release policies (1.1.25)

Both extensions highlight `releases`, nested `policy` / `any` / `all`, and the `max_age`, `keep_last`, `released_since`, `expires_on`, and compatibility leaves. The schema compiler folds these into ordinary version bounds; generated readers contain no dates or policy tree. See the [release policy contract](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/versioning.md#release-dates-and-compile-time-policies).

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
selects `serializer version 1.17.0;`. The original `1` header remains exactly
`1.0.0` and does not enable these features. See the
[magic contract](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/magic_and_omission.md) and [fixed arrays](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/generics.md).

## Simplicity warnings (1.1.31)

The compiler suggests `magic` for manually modeled signatures and `version` for
ordinary payload version/revision fields. If an ordinary field is intentional,
append `ignore(warning version)` or `ignore(warning magic, version)` before its
semicolon. Suppression requires schema language `1.5.0`; it affects compiler
warnings and preserves the field's generated API and wire representation. Use
the `ignore-warning` snippet to add a suffix. Both editors highlight suppression
rules and preserve navigation beside them; compiler warnings appear in build
output, as these extensions do not provide semantic diagnostics.

Omit the redundant name in a metadata declaration:

```serializer
public version uint32 (1) { 1 };
```

Its implicit generated member remains `version`, preserving the API and wire
identities. Retain a custom revision name when existing consumers depend on it. See the
[revision contract](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/schema/versioning.md).

## Compact integer fields (1.1.25)

Both packages recognize schema language `1.2.0` modifiers such as
`public compact_prefix strict uint32 value (3) {32};` and
`public compact_varint uint64 count (4);`. Encoding and overflow-policy keywords
receive modifier highlighting; generic type operands retain normal navigation.
The `compact-prefix` and `compact-varint` VS Code snippets insert unsigned scalar fields.
The compiler supports compact fields in unpacked owning classes: the prefix encoding
holds at most 30 payload bits, while unsigned LEB128 varints retain the host type's
full range. `strict` is the default; `lenient` prefix output keeps the low 30 bits.
JSON and Protobuf scalar encodings retain their existing representation.
