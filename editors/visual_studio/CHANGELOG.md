# Changelog

## 1.1.13

- Highlight generic parameters, nested type arguments, and named instantiations.
- Navigate generic parameters and arguments, named roots, and concrete generated types across all output languages.
- Keep both editor packages synchronized with schema generic support.

## 1.1.12

- Synchronize with the VS Code navigation regression release; Visual Studio command behavior is unchanged.
- Expand shared generated-output boundary checks across all 11 languages and document the coverage matrix.
- Clarify native alias navigation, generated metadata ownership and managed-target build prerequisites.

## 1.1.11

- Add native Go to Type Definition for schema types, resolving their source declarations.
- Navigate from class/struct/enum keywords, including reversed whole-declaration selections.
- Cover the ledger example, live transitive types and native editor commands with regression tests.
- Preserve generated-language type providers and keep both extension versions synchronized.
- Align C# and Visual Studio restore runtime targets and provide an editor-project solution.

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
- Record the Windows runtime targets required by locked NuGet restore while
  preserving dependency versions and the extension's Windows x64 target.
- Keep the release version synchronized with the Visual Studio Code extension.

## 1.1.7

- Navigate from schema includes to the included schema for both Go to Declaration
  and Go to Definition, independent of generated output languages.
- Keep the release version synchronized with the Visual Studio Code extension.

## 1.1.6

- Add direct Serializer source-download, compiler/runtime installation, and CMake
  integration links to the extension description.
- Clarify that installing the editor extension does not install the compiler or runtime.
- Keep the release version synchronized with the Visual Studio Code extension.

## 1.1.5

- Build and validate this VSIX alongside the VS Code package from `make.ps1 all`.
- Add shared `editors/build.ps1` packaging with locked npm restoration and matching-version checks.
- Keep both extension releases synchronized at 1.1.5; building does not install either package.

## 1.1.4

- Synchronize the release version with the Visual Studio Code extension at 1.1.4.
- Retain the navigation and highlighting implementation from 1.0.4.
- Keep both extension versions equal for subsequent releases.

## 1.0.4

- Add native Go to Declaration, schema Go to Definition and Ctrl+click navigation.
- Resolve extensionless/transitive includes, qualified names, full selections and unsaved schemas.
- Map generated declarations in all 11 languages back to schemas using dependency metadata.
- Run the shared resolver in-process with bounded, cancellable background requests.
- Highlight custom type names and namespace qualifiers with the canonical grammar.
- Bundle locked interpreter dependencies and notices; validate the MEF package and real .NET resolver.

## 1.0.3

- Package the shared grammar with extensionless schema include highlighting.
- Retain explicit `.serializer` paths and reject unrelated extensions and directory-only paths.

## 1.0.2

- Highlight unquoted schema includes, paths, comments, and semicolons.
- Recognize invalid quoted include paths and add real TextMate tokenization coverage.
- Package the updated canonical grammar for Visual Studio.

## 1.0.1

- Update the extension version and package references to 1.0.1.
- Rebuild package intermediates to prevent stale version metadata in the VSIX.
- Retain the shared grammar and basic editing configuration from 1.0.0.

## 1.0.0

- Add a Visual Studio 2022/2026 x64 VSIX for `.serializer` schemas.
- Bundle the canonical grammar, shared language configuration, license, and logo.
- Register syntax highlighting and basic editing configuration without extension code.
- Add locked dependency restoration, package building, and asset validation.
