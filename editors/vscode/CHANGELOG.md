# Changelog

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
