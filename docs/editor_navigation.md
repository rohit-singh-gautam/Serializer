# Editor navigation investigation

This records the VS Code 1.1.4 navigation repair and the new Visual Studio 1.0.4
navigation implementation, with verification on Windows. Both use the same
schema/generated-output resolver and custom-type highlighting grammar.
Version 1.1.4 synchronized both editor extension release versions. Subsequent
build-tooling releases keep them equal. Version 1.1.11 additionally registers schema
type-definition navigation and accepts declaration keywords/whole declarations.

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
completion and schema member/function navigation remain outside the implemented
feature set.

## Navigation coverage matrix

Version 1.1.12 adds a real CMake/C++ project regression using the managed ledger
schema and three caller files with repeated `using ledger` aliases. A clean build
does not itself repair missing project configuration: `SERIALIZER_BUILD_MANAGED`
defaults to OFF, and CMake Tools must configure the same build and provide the
source target's include paths to Microsoft C/C++. The two targets in the test
deliberately generate the same qualified type and header filename from different
schema paths, so matching by name alone cannot pass.

Navigation must never generate files, configure a project, save buffers or prompt
to build. Only an explicit generation/build command may create missing output.
The project test verifies this before it explicitly builds the fixture headers.

| Case | Required result | Automated coverage |
| --- | --- | --- |
| Schema class/enum declaration, keyword or qualified reference | Declaration/type definition select the source; definition selects available generated output, otherwise source | Shared/provider tests and native VS Code/Visual Studio commands |
| Every identifier cursor position, selection endpoint and forward/reversed selection | Exact destination identifier; comments/whitespace do not become types | Shared/provider tests; 6,007 fresh-output checks; both editor hosts |
| CRLF, Unicode before a symbol, unsaved schema/include edits | Correct UTF-16 ranges from live buffers; never save | Provider, shared and editor-host tests |
| Direct/transitive, extensionless, diamond/cyclic and missing includes | Resolve the owning schema; deduplicate cycles; no fabricated destination | Shared tests and native schema/include checks |
| Enum defaults, inheritance, containers, namespace collisions | Resolve type operands, preserve scope; ignore field/value names and literals | Shared tests and fresh compiler output |
| Generated class/enum, C typedef, TypeScript alias, managed companions | Reverse to exact schema ownership and naming profile | All 11 fresh compiler output languages, nine C++ and three Java profiles |
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
