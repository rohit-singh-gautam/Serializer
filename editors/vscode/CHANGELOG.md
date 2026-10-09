# 1.1.28

- Highlight schema language 1.3.0 bare, fixed-byte, and named-algorithm digest fields.
- Add digest snippets and use language 1.3.0 for new schema snippets.
- Skip algorithm selectors and byte extents during navigation while preserving adjacent and legacy user types.
- Keep both extension versions synchronized and update local package names.

# 1.1.27

- Direct generated applications to the proprietary-compatible `Serializer::runtime` target.
- Document the 0BSD application runtime and separate GPL compiler API scope.
- Preserve schema notices through compiler generation without changing editor navigation.
- Keep both extension versions synchronized and update local package names.

# 1.1.26

- Clarify Serializer's State Framework positioning, product links, and editor/runtime boundaries.
- Distinguish published Marketplace versions from newer source and local packages.
- Update extension descriptions and discovery terms while preserving identities and editor behavior.
- Keep both extension versions synchronized; no schema or runtime behavior changes.

# 1.1.25

- Highlight schema language 1.2.0 `compact_prefix` and `compact_varint` fields with explicit `strict` or `lenient` overflow policy.
- Preserve type navigation beside compact modifiers and keep generated-output navigation shared by both editors.
- Add compact field snippets and select schema language 1.2.0 in new schemas.
- Keep the Visual Studio and Visual Studio Code package versions synchronized.

# 1.1.24

- Recognize schema language 1.1.0 inferred fixed arrays and scalar/enum magic declarations.
- Highlight empty fixed-array extents and their element types.
- Navigate typed magic enum operands and enum initializers through transitive includes and unsaved edits.
- Preserve generated namespace navigation beside C++ numeric separators in inferred defaults.
- Add VS Code snippets for inferred arrays and typed magic; new schemas select language 1.1.0.
- Synchronize both editor versions and rebuild the local packages.

# 1.1.23

- Highlight three-component schema language versions as complete numeric tokens.
- Recognize the original `serializer version 1;` header as the `1.0.0` baseline.
- Keep schema language versions independent of compiler releases; future majors require dotted headers.
- Verify dotted headers beside qualified types, transitive includes, unsaved edits, and generated-output navigation.
- Synchronize both editor extension versions and rebuild their packages.

# 1.1.22

- Update build and navigation guidance for default-enabled managed runtime support.
- Synchronize both extension releases with the current repository build defaults.

# 1.1.21

- Refresh the packaged extension logo from the updated canonical 128×128 artwork.
- Keep both editor packages at the same release version.
- Refresh the Serializer file and editor-tab icon variants from the updated source artwork.

# 1.1.20

- Refresh the shared TypeScript compiler, bundler, Node.js declarations, and locked dependency graph.
- Update the Visual Studio navigation engine to Jint 4.16.4 and VSSDK build tools to 18.5.40034 while retaining the supported editor baselines.
- Patch the Visual Studio SDK's MessagePack dependency without adding its runtime assemblies to the package.
- Declare compiler ambient types explicitly for TypeScript 7 and synchronize both extension packages.

# 1.1.19

- Use the portable `SRLFILE` magic snippet default in both synchronized editor packages.
- Update package identities, build output paths, and current editor documentation together.

# 1.1.18

- Highlight immutable magic declarations and generic format exclusions.
- Preserve adjacent type navigation and add shared magic/omit snippets and regression coverage.

# 1.1.17

- Highlight release catalogs, generation-time date/count policies, and nested any/all acceptance trees.
- Add a release-policy snippet and verify navigation across nested version metadata.

# 1.1.16

- Recognize message versions, compatibility floors, field lifecycles, replacements, reservations, and dotted version types.
- Keep schema type navigation correct around version declarations and retained obsolete fields.

# 1.1.15

- Support native C++ generic declarations, dimension parameters, defaults, and fixed-array expressions in shared navigation and highlighting.
- Retain optional `instantiate` and concrete-field contracts for cross-language generation; C++ templates no longer require them.

# Changelog

## 1.1.14

- Restore declaration, definition, and type-definition actions in read-only Git index and history tabs.
- Resolve cursor positions from the displayed snapshot and external destinations from the current workspace.

## 1.1.13

- Highlight generic parameters, nested type arguments, and named instantiations.
- Navigate generic parameters and arguments, named roots, and concrete generated types across all output languages.
- Keep both editor packages synchronized with schema generic support.

## 1.1.12

- Follow native type definitions from aliases and variables in the explicit Go to Schema Declaration command.
- Preserve ordinary language-service declaration, definition and type-definition behavior.
- Test real managed ledger generation with CMake Tools, duplicate aliases, target-specific headers and missing output.
- Cover stale ownership metadata, unresolved providers, CRLF/UTF-16 positions and generated identifier boundaries in all 11 languages.
- Document the navigation contract, coverage matrix and managed-target configuration after a clean build.

## 1.1.11

- Add native Go to Type Definition for schema types, resolving their source declarations.
- Navigate from class/struct/enum keywords, including reversed whole-declaration selections.
- Cover the ledger example, live transitive types and native editor commands with regression tests.
- Preserve generated-language type providers and keep both extension versions synchronized.
- Document the C# editor-project solution to avoid loading temporary build-tree projects.

## 1.1.10

- Document default direct managed identities and opt-in separate value/storage output.
- Verify navigation for both representations across all C++ naming profiles.
- Keep both Rohit Serializer extension packages at the same version.

## 1.1.9

- Highlight `managed` class and member declarations for the C++ managed interface.
- Resolve direct, array, and map member types after `managed`, including qualified names.
- Navigate generated managed storage/data/editor declarations back to their schema types.
- Keep both Rohit Serializer extension packages at the same version.

## 1.1.8

- Identify the Serializer GitHub repository directly in the package description
  and at the start of the extension README.
- Keep the release version synchronized with the Visual Studio extension.

## 1.1.7

- Navigate from schema includes to the included schema for both Go to Declaration
  and Go to Definition, independent of generated output languages.
- Keep the release version synchronized with the Visual Studio extension.

## 1.1.6

- Add direct Serializer source-download, compiler/runtime installation, and CMake
  integration links to the extension description.
- Clarify that installing the editor extension does not install the compiler or runtime.
- Keep the release version synchronized with the Visual Studio extension.

## 1.1.5

- Build both editor VSIX packages as part of `make.ps1 all` without installing them.
- Add `editors/build.ps1` for standalone packaging with locked dependencies and matching-version checks.
- Keep both extension releases synchronized at 1.1.5 and document the required build tools.

## 1.1.4

- Highlight custom class/enum references and namespace qualifiers using theme type colors.
- Share the navigation resolver with the new native Visual Studio integration.

- Fix navigation at the end of selected qualified types and include paths,
  with regressions for `demo::order` in the maintained complex model.
- Use the built-in Go to Declaration context-menu action. Retain the schema-only
  command in the Command Palette for choosing only Serializer destinations.
- Register declaration navigation for every supported generated language:
  C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C.
- Resolve actual nested/flattened type names, C typedefs, TypeScript enum aliases,
  naming profiles and compiler multi-output dependency files. Schema definitions
  and file-level Go to Implementation can open all generated output languages.
- Keep schema declaration lookup independent of CMake responsiveness; deduplicate
  language-provider locations and accept declaration ranges containing modifiers.
- Verify the real compiler's output across all languages and exercise the native
  declaration command with forward/reversed selections in an isolated VS Code host.
- Refresh the package, installation guidance and integration skill to include
  the shorthand include support introduced in 1.1.3.

## 1.1.3

- Highlight extensionless schema includes and use shorthand in the include snippet.
- Resolve shorthand to `.serializer` files for declaration, definition, and
  transitive include navigation while retaining explicit paths and source ranges.
- Cover mixed spellings, dotted paths, cycles, missing files, and maintained schemas.

## 1.1.2

- Resolve enum type prefixes in field defaults such as
  `AccountState::WaitingForReview`, including qualified types from included schemas.
- Fall back from Go to Definition to the original schema type declaration when
  no matching generated C++ definition is available. Existing generated matches
  retain priority; header-only commands remain silent when output is unavailable.
- Add regression coverage using the AUTOSAR account schema, including editor
  provider ranges, unsaved types, missing/stale output, and CMake profile isolation.

## 1.1.1

- Add Serializer: Go to Implementation to `.serializer` file context menus in
  Explorer and editor tabs, using the clicked file even when another editor is active.
- Reuse existing generated-header navigation, including its output picker and
  silent handling of missing output, without builds or generation prompts.

## 1.1.0

- Add Go to Declaration for schema includes, class/enum declarations and type
  references, and generated C++ includes/types.
- Add Go to Definition from schemas to existing generated headers and exact type
  definitions, including included schemas and storage-mode specializations.
- Read existing dependency files for output ownership; honor available CMake
  configurations, include search order, naming profiles and ambiguous outputs.
- Add Go to Schema Declaration to select only Serializer destinations when
  another C++ provider also supplies declaration results.
- Make Open Generated Header read-only and available without CMake Tools or
  workspace trust. Navigation never saves, configures, builds, activates CMake
  Tools, or offers generation; unavailable destinations return no result.
- Add resolver/provider tests and an isolated VS Code navigation test harness.

## 1.0.2

- Highlight unquoted schema includes, paths, comments, and semicolons.
- Recognize invalid quoted include paths and add real TextMate tokenization coverage.
- Add an `include common.serializer;` snippet.

## 1.0.1

- Include the shared grammar's explicit `.serializer` file association.
- Link the separate Visual Studio extension from the limitations documentation.
- Update the package version and installation references; VS Code commands and
  highlighting rules retain their existing behavior.

## 1.0.0

- Use the Marketplace display name "Rohit Serializer".
- Prepare the Marketplace release under publisher `rohitjairajsingh`.
- Include Rohit Jairaj Singh's author profile and documentation/support links.
- Include the Serializer logo as the extension and Marketplace icon.
- Register the dedicated 32×32 icon for `.serializer` files in Explorer and editor tabs,
  where the selected file icon theme supports language icons.
- Update the VSIX filename and installation instructions for version 1.0.0.

## 0.1.0

- Highlight versioned `.serializer` schemas and supply schema/field snippets.
- Generate headers using the selected CMake Tools project and build configuration.
- Locate generated headers without merging include paths from different profiles.
- Diagnose missing runtime/generated includes and offer C/C++ quick fixes.
- Keep builds disabled in untrusted workspaces while retaining language assets.
