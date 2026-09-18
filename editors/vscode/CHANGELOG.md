# Changelog

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
