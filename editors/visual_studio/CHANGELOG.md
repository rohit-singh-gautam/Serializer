# Changelog

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
