# Changelog

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
