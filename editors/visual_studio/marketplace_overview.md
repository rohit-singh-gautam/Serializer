# Rohit Serializer for Visual Studio

**Editor support for Serializer — State Framework**

Rohit Serializer helps you edit `.serializer` schemas and navigate between schemas
and existing generated code in **Visual Studio 2022 and Visual Studio 2026 on
Windows x64**. It provides syntax highlighting, editing assistance, and native
navigation for projects using Serializer — State Framework.

## Editor features

- Syntax highlighting for schema declarations, custom type references, namespaces,
  includes, and supported schema annotations, using the active editor theme.
- Schema language 1.5.0 warning suppression such as `ignore(warning magic, version)`,
  with type navigation preserved beside field metadata and unnamed payload revisions.
- Comment toggling, bracket and quote pairs, and indentation.
- **Go to Declaration** from a schema include or type to its source declaration,
  and from a generated type declaration to its originating schema.
- **Go to Definition** and **Ctrl+click** in schemas to existing generated output,
  with a source-declaration fallback when output is unavailable.
- **Go to Type Definition** in schemas to the source class, enum, or included file.
- Qualified names, declaration keywords, forward and reversed selections,
  transitive and extensionless includes, and unsaved schema text.

Generated-output navigation supports **C++, Java, JavaScript, TypeScript, Go, C#,
Rust, Python, Swift, Kotlin, and C**, including the supported naming profiles.
In caller code, use the language's normal navigation to reach the generated type,
then **Go to Declaration** to open its schema. The Visual Studio adapter does not
query other language services for a one-step caller-to-schema jump.

Navigation reads available files. It never saves, configures, builds, or prompts to
generate missing output. Retain the compiler's dependency files (`--depfile`) in
the solution tree or beside generated output, especially for renamed files and
Python, Swift, Kotlin, and C output without a generated-file banner. Serializer's
CMake generation helpers already produce this metadata.

This extension does not provide semantic diagnostics, completion, member/function
navigation, snippets, or the separate VS Code extension's CMake commands. Use
your project's build targets to generate code. ARM64 and earlier Visual Studio
versions are not targeted. See the
[extension guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/visual_studio/README.md)
for prerequisites, limitations, and actual verification coverage.

## About Serializer

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

The extension provides editor support; it does not install the Serializer compiler
or runtime. Application history, journals, and collaborative editing are C++
framework capabilities integrated by the application, not editing features supplied
by this extension. Other generated languages can exchange managed wire records;
native managed engines currently run in C++.

## Get started

Install **Rohit Serializer** from the existing
[Visual Studio Marketplace listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio)
and restart Visual Studio. Install the compiler and runtime separately using the
[source-build and installation guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/cmake_integration.md#use-an-installed-package).
Open a `.serializer` schema and build your project's generation targets when you
need generated-output destinations.

Published package versions may lag this source checkout. Compare the Marketplace
version with the [extension changelog](https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/visual_studio/CHANGELOG.md)
before relying on recently added behavior. A locally built VSIX is a separate
release state from a published package.

[Project website](https://www.singh.org.in/serializer.html) ·
[Serializer repository](https://github.com/rohit-singh-gautam/Serializer) ·
[Serialization guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/usage.md) ·
[C++ state-management guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/managed/getting_started.md)
