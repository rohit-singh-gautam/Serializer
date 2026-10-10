# 1.1.31

- Share schema language 1.5.0 warning suppression highlighting with VS Code.
- Preserve qualified type navigation beside unnamed revisions and suppression rules,
  including ordinary fields named `ignore` or `warning`.
- Verify adjacent generated-output navigation in all eleven supported languages.
- Synchronize both extension versions and current local package names.

# 1.1.30

- Highlight schema language 1.4.0 owning variants and navigate their alternative types.
- Preserve existing union support and synchronize both editor extension versions.

# 1.1.29

- Add the synchronized root `extension` command to package both editors without CMake configuration, native builds, or repository cleanup.
- Document Windows packaging and WSL delegation, including the Windows/MSBuild requirement for the Visual Studio VSIX.
- Keep both extension versions synchronized and update current local package names.

# 1.1.28

- Share schema language 1.3.0 digest highlighting and metadata-aware navigation with VS Code.
- Preserve legacy declared digest types, qualified references, and adjacent generated-output navigation.
- Synchronize with VS Code's digest snippets and current schema header.
- Keep both extension versions synchronized and update local package names.

# 1.1.27

- Document generated output and application runtime support for proprietary applications.
- Distinguish the 0BSD `Serializer::runtime` target from the GPL compiler API library.
- Synchronize with the VS Code runtime-linkage guidance update.
- Keep both extension versions synchronized and update local package names.

# 1.1.26

- Clarify Serializer's State Framework positioning, product links, and editor/runtime boundaries.
- Distinguish published Marketplace versions from newer source and local packages.
- Update extension descriptions and discovery terms while preserving identities and editor behavior.
- Keep both extension versions synchronized; no schema or runtime behavior changes.

# 1.1.25

- Highlight schema language 1.2.0 `compact_prefix` and `compact_varint` fields with explicit `strict` or `lenient` overflow policy.
- Preserve type navigation beside compact modifiers and keep generated-output navigation shared by both editors.
- Add compact field snippets to the companion VS Code package and select schema language 1.2.0 in new schemas.
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

- Synchronize with VS Code Git-view navigation fixes; Visual Studio behavior is unchanged.

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
