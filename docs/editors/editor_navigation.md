# Editor navigation investigation

This records the VS Code 1.1.4 navigation repair and the new Visual Studio 1.0.4
navigation implementation, with verification on Windows. Both use the same
schema/generated-output resolver and custom-type highlighting grammar.
Version 1.1.4 synchronized both editor extension release versions. Subsequent
build-tooling releases keep them equal. Version 1.1.11 additionally registers schema
type-definition navigation and accepts declaration keywords/whole declarations.

## Separate C++ generated headers (1.1.48)

Both extensions navigate the compiler's default separate C++ headers and the
optional combined layout. Schema definitions and fields resolve to their owning
generated header, including transitive includes and renamed entry outputs.
Each separate header carries `// Serializer source: <absolute-schema-path>`.
For a file with a Serializer-generated banner, the resolver uses this exact
source identity before depfile or basename lookup. Equal schema basenames in
different directories therefore remain distinct.

The shared resolver follows existing quoted C++ generated-header includes with
bounded depth and file counts. It accepts only banner-marked generated
destinations, leaves unrelated native includes to the language service, and
deduplicates repeated or cyclic links. VS Code also expands headers registered
by an already active CMake configuration through that generated include tree,
even when the CMake model lists only the entry header. Linked dependency headers
remain associated with that entry's selected configuration and naming profile.

Lookup stays read-only and uses available files and live buffers. Missing child
headers do not trigger a build, generation or a prompt. See
[current editor verification](editor_extension.md#verification-performed) for
completed source, resolver and package checks.

## Deferred function definitions (1.1.47)

Both editor packages provide **Go to Definition** from the name of a typed
external function declaration in a `.serializer` file to its authored C++
out-of-class definition. Put the cursor on `valid` in a declaration such as
`public function bool valid() readonly;`, then use the context menu or F12.
The result lands on the function name in the available C++ implementation file.
Declaration and type navigation retain their existing behavior, and type
references in the signature retain schema/generated-output navigation.

Lookup matches the schema's qualified owner, including namespace-qualified
templated owners, and compares overload signatures. It reads available workspace
files and live editor buffers in a bounded passive search; it does not require
generated output or an active CMake configuration. Unsaved implementation changes
participate without saving the document. If signature information is insufficient
to distinguish valid candidates, all matching definitions remain choices. Missing
definitions fall back to the exact schema function name without a generation
prompt. Functions defined by a schema expression or native body stay at their
schema function name.

This is lexical navigation, not a C++ parser or replacement for a native language
service. Arbitrary C++ aliases, macro expansion, dependent type equivalence and
functions hidden inside native schema blocks are outside this resolver's contract.
Native blocks remain opaque. General expression/member-call navigation and
semantic diagnostics remain unavailable. Existing includes, types, fields and
generated-output mappings retain their behavior across all eleven output languages.
See the [qualification record](../verification/verification-deferred-navigation-2026-10-13.md)
for actual checks and remaining editor-host boundaries.

## Simplicity warning metadata (1.1.31)

Both editor packages recognize schema language `1.5.0` field suffixes such as
`ignore(warning version)` and `ignore(warning magic, version)`. Suppression rules
remain metadata and do not become type-navigation destinations. Qualified type
operands retain navigation beside these suffixes and unnamed revision declarations
such as `public version uint32 (1) { 1 };`. Its implicit generated member remains
`version`, preserving existing APIs and wire identities.

Ordinary fields and declared types named `ignore` or `warning` retain their
existing behavior. Shared tests cover qualified cursor boundaries, transitive
includes and unsaved schemas; fresh compiler fixtures exercise both navigation
directions across all eleven output languages. See the
[current verification record](editor_extension.md#verification-performed).

## Git index and history views (1.1.14)

VS Code previously registered navigation only for `file:` documents, so its
read-only Git `(Index)` tabs omitted **Go to Definition**. Version 1.1.14 also
registers `git:` providers and maps Git's source-path metadata to the workspace.
It reads the displayed snapshot for cursor offsets and local symbols, preserving
the snapshot URI for local destinations. Includes and generated output use the
current workspace, including unsaved dependencies, rather than historical files.

Verification passed all 70 editor unit/provider tests and the isolated VS Code
navigation suite. The host test stages the Swift generic schema entry in a real
Git repository, changes the working file, and follows its staged include with
the actual **Go to Definition** action. It also checks that a generic parameter's
type definition stays in the staged document. Both 1.1.14 packages were rebuilt;
the VS Code package was installed locally. Visual Studio was not reinstalled.

## Reproduced causes

- The installed VS Code extension was 1.1.2. The repository was already at 1.1.3,
  which introduced navigation for extensionless includes. The complex model uses
  `include sales/order;`, so an older installed bundle could not load its type graph.
- Even the 1.1.3 resolver rejected a caret exactly at the end of a type or include
  range. A regression using `example/schemas/complex/model.serializer` reproduced
  this: all characters of `demo::order` resolved, but its selection endpoint did not.
- The custom menu command used `selection.active`. VS Code retains selections
  when right-clicking inside them, leaving that active position at either endpoint.
  This explains the apparent intermittency between click/selection gestures.
  See the editor's [context-menu implementation](https://github.com/microsoft/vscode/blob/main/src/vs/editor/contrib/contextmenu/browser/contextmenu.ts).
- Navigation providers were registered only for Serializer and C++. C itself and
  the other generated languages had no schema declaration provider. Their emitted
  names and declaration forms also differ from C++.

## Editor integration

The native **Go to Declaration** action uses a registered declaration provider.
Its built-in command is `editor.action.revealDeclaration`, as documented by the
[VS Code implementation](https://github.com/microsoft/vscode/blob/main/src/vs/editor/contrib/gotoSymbol/browser/goToCommands.ts).
The extension now uses that menu action and retains the explicit schema-only
command in the Command Palette for compatibility and provider filtering.

The [public provider API](https://code.visualstudio.com/api/references/vscode-api#languages.registerDeclarationProvider)
merges results from registered providers. There is no supported exclusive override
that selectively suppresses another language service's declarations. Header-only
C++ generation does not change that editor contract. Definitions in caller source
remain owned by that language's service; schema declarations are additional results.

Cursor lookup accepts a range's end while preferring a token starting at that
position. Includes, comments, member names, enum values and type operands retain
distinct ranges. Schema declaration lookup does not wait for CMake Tools.

Visual Studio uses a native MEF editor command filter for Go to Declaration and
schema Go to Definition, plus an `INavigableSymbolSourceProvider` for Ctrl+click.
The shared resolver is bundled into the assembly and runs in-process through Jint;
it requires no Node.js installation or compiler at runtime. Live document snapshots
are captured before background lookup. Requests are cancelled on view closure and
stale results are discarded if the source snapshot or caret changed.
Native caller definitions remain with the caller language service: first navigate
to the generated type, then use Go to Declaration to open its schema. The Visual
Studio adapter does not query other languages' definition providers as VS Code does.
The command-filter integration follows Microsoft's
[editor extension walkthrough](https://learn.microsoft.com/en-us/visualstudio/extensibility/walkthrough-using-a-shortcut-key-with-an-editor-extension?view=visualstudio).

Both grammars now assign custom type references `entity.name.type.serializer` and
namespace qualifiers `entity.name.namespace.serializer`. This covers simple and
qualified fields, array/map/union operands, bases and enum-default prefixes without
coloring field identifiers as types. Colors come from the active editor theme.

## Generated-language mapping

| Output | Mapping and emitted declarations |
| --- | --- |
| C++ | Namespace-qualified classes/enums, nine naming profiles, storage-mode declarations/specializations |
| Java | Filename-derived outer class, nested namespace containers, three profiles and preserve naming |
| JavaScript | Flattened exported classes and frozen enum objects |
| TypeScript | Companion classes, enum objects and enum type aliases |
| Go | Flattened structs and `int32` enum types |
| C# | Flattened classes/enums inside the configurable namespace and outer class |
| Rust | Flattened structs/enums; lifetimes are not mistaken for character literals |
| Python | Flattened classes and `_IntEnum` types; comments and multiline strings are masked |
| Swift | Flattened structs/enums |
| Kotlin | Flattened classes/enum classes |
| C | Snake-case structs/enums, forward tags and typedef aliases |

Output ownership comes from exact targets in a sibling `<output>.d` or a workspace
multi-output `.d` file. Transitive schema includes identify the original declaration.
For separate C++ headers, the banner-marked `// Serializer source:` comment
identifies the exact source before dependency metadata or basename lookup.
Existing generated include links extend the entry output's tree without changing
its active CMake configuration.
This supports renamed outputs without relying on a name appearing somewhere in a
workspace. Legacy basename lookup still requires a Serializer banner. Current
Python, Swift, Kotlin and C generators do not emit that banner, so they require
dependency metadata. Navigation never creates output or changes build settings.

Use the compiler's existing `--depfile` option and retain the result with the
generated files. CMake's generation helpers already emit dependency metadata.
External caller references require an installed definition provider for their
language; a recognized generated declaration can be mapped directly.

## Regression coverage

- The ledger example covers declaration, definition and type definition on keywords,
  names and forward/reversed selections; type definition uses source declarations
  and leaves generated-language type providers unchanged. Live transitive schema
  types and comments between keywords/names have dedicated regression coverage.
- Unit/provider tests exercise the actual complex model, cursor endpoints,
  extensionless/transitive includes, enum defaults, language registrations,
  C typedefs, TypeScript aliases, duplicate provider results, preserved names,
  comments/literals, live buffers, cancellation, missing files and CMake isolation.
- `test:navigation:generated` checks fresh compiler output in all 11 languages,
  every complex-model type, acronym/digit names, preserve naming, nine C++ profiles
  and three Java profiles with an explicit package.
- The isolated VS Code host invokes the native declaration action on forward and
  reversed complex-model selections, and checks standard provider commands,
  ignored build directories, unsaved schemas and missing destinations.
- Microsoft C/C++ and the built-in TypeScript service are exercised with real
  definition results. Other language services are modeled at the provider boundary;
  their generated formats are checked against real compiler output.
- Visual Studio's .NET interpreter test checks the three reported model references,
  their selection endpoints, all 11 generated languages in both directions, unsaved
  include changes and cancellation. Package checks require the MEF assembly, shared
  assets and interpreter dependencies.
- A hidden Visual Studio 2026 instance exercises native declaration/definition
  commands, both selection directions for the reported three types, and reverse
  generated-header navigation. No source document is saved by the host test.

See the [current verification record](editor_extension.md#verification-performed)
for completed runs. Linux/macOS, remote extension hosts and the remaining external
language services have not been exercised interactively. Semantic diagnostics,
completion and general schema expression/member-call navigation remain outside
the implemented feature set. Typed external function definitions have the
bounded lookup described above.

## Navigation coverage matrix

Version 1.1.12 adds a real CMake/C++ project regression using the managed ledger
schema and three caller files with repeated `using ledger` aliases. A clean build
does not itself repair missing project configuration. Fresh builds enable
`SERIALIZER_BUILD_MANAGED`; existing caches retain their values. CMake Tools must
configure the same build and provide the
source target's include paths to Microsoft C/C++. The two targets in the test
deliberately generate the same qualified type and header filename from different
schema paths, so matching by name alone cannot pass.

Navigation must never generate files, configure a project, save buffers or prompt
to build. Only an explicit generation/build command may create missing output.
The project test verifies this before it explicitly builds the fixture headers.

| Case | Required result | Automated coverage |
| --- | --- | --- |
| Schema class/enum declaration, keyword or qualified reference | Declaration/type definition select the source; definition selects available generated output, otherwise source | Shared/provider tests and native VS Code/Visual Studio commands |
| Typed external function declaration in a schema | Definition lands on available authored C++ out-of-class definitions by qualified owner and overload signature; missing definitions fall back to the schema function name | Shared/provider, .NET resolver and isolated VS Code host coverage; current results in the deferred navigation record |
| Every identifier cursor position, selection endpoint and forward/reversed selection | Exact destination identifier; comments/whitespace do not become types | Shared/provider tests; 6,007 fresh-output checks; both editor hosts |
| CRLF, Unicode before a symbol, unsaved schema/include edits | Correct UTF-16 ranges from live buffers; never save | Provider, shared and editor-host tests |
| Direct/transitive, extensionless, diamond/cyclic and missing includes | Resolve the owning schema; deduplicate cycles; no fabricated destination | Shared tests and native schema/include checks |
| Enum defaults, inheritance, containers, namespace collisions | Resolve type operands, preserve scope; ignore field/value names and literals | Shared tests and fresh compiler output |
| Generated class/enum, C typedef, TypeScript alias, managed companions | Reverse to exact schema ownership and naming profile | All 11 fresh compiler output languages, nine C++ and three Java profiles |
| Separate C++ entry/child headers, transitive includes and equal source basenames | Follow generated includes and exact source comments within the selected entry/configuration | Shared/provider and .NET resolver tests, including an active CMake model registering only the entry header |
| Qualified C++ type on the RHS of `using ledger = ledger_example::ledger` | Definition/type definition reach the generated class; declaration includes its schema | CMake/C++ host with three duplicate caller aliases |
| C++ alias use, alias chain and variable | Native declaration/definition retain local meaning; type definition reaches the generated class | CMake/C++ host with real language service |
| Explicit VS Code schema command on those aliases/variables | Map the native type-definition destination to its schema if ordinary definition stops locally | Provider and CMake/C++ host tests |
| Same basename and qualified type in different CMake targets | Use the compiler's include order and exact output owner | Shared/provider and CMake/C++ host tests |
| Other configuration, quoted local shadow, ordinary header/type | Preserve native results; do not borrow another target/profile's type | Model/shared/provider and native C++ tests |
| Header not generated, deleted output, schema moved or stale `.d` target | No guessed origin or automatic generation; schema-side definition may fall back to source | Provider/shared tests and configure-only C++ host check |
| Missing native provider, empty result, duplicate results, non-file destination | Silent miss or deduplicated valid schema destinations | Provider tests |
| Cancelled request, untrusted workspace, inactive CMake, ignored build tree | Read-only lookup, bounded work, no CMake activation or build | Shared/provider and editor-host tests |
| Visual Studio caller language service | First navigate natively to generated type, then Go to Declaration | Documented adapter boundary; shared output mapping and native reverse check |

`Go to Declaration` can show both the native generated class and its schema in VS
Code because provider results are merged. Use **Serializer: Go to Schema Declaration**
for a schema-only destination. On a generated class itself, Definition and Type
Definition may return the current class; VS Code may then invoke its configured
alternative command (commonly references). Equal destinations for these commands
on a class are valid; three unrelated caller aliases for a qualified type are not.

Run `npm test`, `npm run test:navigation:generated`, `npm run test:navigation` and
`npm run test:navigation:project` in `editors/vscode`. The last command requires
`SERIALIZER_COMPILER`, `SERIALIZER_CMAKE_TOOLS_PATH` and `SERIALIZER_CPP_TOOLS_PATH`;
set `VSCODE_EXECUTABLE_PATH` to reuse the installed editor and
`SERIALIZER_TEST_CMAKE_GENERATOR` if the default Ninja generator is unsuitable.
It creates its own workspace with spaces and does not change the user's project.

This matrix covers implemented behavior, not full semantic language servers.
Native C++ and TypeScript are verified separately from the compiler-format checks.
Native Java, C#, Go, Rust, Python, Swift and Kotlin services, Linux/macOS, remote
hosts, symlink workspaces and every editor/provider version remain separate
integration environments, not claims established by the shared tests.

## Typed magic and inferred arrays (1.1.24)

Typed magic enum operands and qualified enum initializer prefixes navigate to
their schema declaration or available generated enum definitions.
`array[]` element types use the existing collection navigation rules. The shared
resolver preserves qualified-name cursor boundaries, transitive includes and
unsaved source edits. Scalar builtins and literal magic bytes add no schema-type
navigation destination.

## Compact unsigned fields (1.1.25)

The shared resolver skips `compact_prefix`, `compact_varint`, and their optional
`strict`/`lenient` policies before indexing the scalar type operand. Generic type
parameters retain their declaration targets, and adjacent qualified types still
navigate through unsaved transitive includes and existing output files. The
modifiers and scalar builtin types add no navigation destination.

Existing schemas may use these spellings as ordinary type names. A compact
modifier requires a separate scalar type and field name; ordinary fields such as
`public compact_prefix value;` retain their type navigation and compiler meaning.
Shared grammar/resolver tests cover both interpretations, and the Visual Studio
interpreter tests exercise compact generic operands and all 11 adjacent output
destinations.

## Inherited-field request allocation (2.0.8)

Both extensions reuse one offset-preserving lexical mask per generated output
while checking inherited-field candidates. A field can appear in its declaration,
constructors and many codec methods; masking the entire generated file again for
every occurrence needlessly exhausted Visual Studio's existing 256 MiB request
limit when one lookup returned definitions across all eleven languages. The
prepared text belongs only to that immutable output within the current request,
so later edits, cancellation and other requests cannot reuse stale masks.

The actual .NET/Jint regression retains the original 256 MiB and 30-second limits.
Its fresh-process mode checks all eleven output languages, live schema offsets
and both reverse-navigation selection endpoints:

```powershell
SerializerNavigationTest.exe <repository> <complex-output> <generic-output> <inherited-output> --inherited-only
```

The preserved prior resolver fails this single request with
`MemoryLimitExceededException`; the repaired resolver passes all twelve checks.
The regular native test also includes these checks among its complete navigation
suite. Both local package versions are synchronized at **2.0.9**; package builds
do not install extensions or update Marketplace listings.

## C# and Kotlin package facades (2.0.9)

Generated C# package classes and Kotlin package objects navigate to the owning
schema `package` declaration and back. Kotlin objects have class-like package
identity; ordinary generated enum identity remains distinct. Source renames and
native token selection endpoints follow the same live dependency resolution.

Compiler-marked C#/Kotlin runtime sections use the existing offset-preserving
masking contract. Runtime types have no authored schema destination; their
declarations are excluded while all original line and character offsets remain
valid. Legacy output without those markers retains its previous indexing path.
The shared unit regression covers both backends, all package token positions,
runtime exclusion, enum identity, legacy fallback and live renames. Fresh
generated-output and native adapter results belong to the
[50-item qualification record](../verification/verification-pending-50-2026-10-20.md).
