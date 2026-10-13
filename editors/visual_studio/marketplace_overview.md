# Rohit Serializer for Visual Studio

**Editor support for Serializer — State Framework**

Rohit Serializer helps you edit `.serializer` schemas and navigate between schemas
and existing generated code in **Visual Studio 2022 and Visual Studio 2026 on
Windows x64**. It provides syntax highlighting, editing assistance, and native
navigation for projects using Serializer — State Framework.

The current source release is **2.0.0**, supporting schema language **1.17.0**
and navigation for Serializer **2.0.0**'s default separate C++ generated headers.
The compiler/runtime, schema language, and editor extension have independent
versions; existing 1.x schema language headers remain supported.

## Editor features

- Syntax highlighting for schema declarations, custom type references, namespaces,
  includes, and supported schema annotations, using the active editor theme.
- Schema language 1.5.0 warning suppression such as `ignore(warning magic, version)`,
  with type navigation preserved beside field metadata and unnamed payload revisions.
- Schema language 1.9.0 stateless constraints, computed size headers and portable conversions.
- Language 1.17.0 array exposure and explicit `dimensions`, `readonly`/`const_value`,
  trailing `const_ref` inputs, static/expected callable signatures, and
  `when`, `delegates`, and `generate` metadata. Type navigation distinguishes
  schema symbols from metadata names.
- Language 1.16.0 defaults such as `Frame placement = {}` for owning generic
  class parameters in constructors and named factories.
- Language 1.15.0 fresh record defaults in constructor/factory parameters, including nested positional braces.
- Language 1.14.0 readonly field exposure, arithmetic mappings, reciprocal/external constructors, C++ declaration additions and typed nested defaults.
- Language 1.13.0 C++ `noexcept(cpp)` function contracts, external declarations and native methods with either C++ qualifier order.
- Language 1.12.0 overloaded constructors, generic readonly/common-type signatures, factory-constrained defaults, native storage/constexpr contracts and authoring hooks.
- Language 1.11.0 checked numeric conversion, projected readonly calls, exact record selection and extended construction signatures.
- Language 1.10.0 namespace functions, declared readonly calls and Boolean generic type constraints.
- Schema language 1.8.0 constructors/factories, structured defaults, empty transient layout, checked math/boolean guards and generic bases.
- Generated field declarations map back to their declaring schema, including inherited members reached through native language services.
- Schema language 1.7.0 `optional<T>` and nested generic containers, with
  navigation to their element types and generic method signature types.
- Comment toggling, bracket and quote pairs, and indentation.
- **Go to Declaration** from a schema include or type to its source declaration,
  and from a generated type declaration to its originating schema.
- **Go to Definition** and **Ctrl+click** in schemas to existing generated output,
  with a source-declaration fallback when output is unavailable.
- **Go to Definition** and **Ctrl+click** on typed external schema function names
  to available authored C++ out-of-class definitions, matching qualified owners
  and overload signatures in bounded passive lookup with live buffers.
- **Go to Type Definition** in schemas to the source class, enum, or included file.
- Qualified names, declaration keywords, forward and reversed selections,
  transitive and extensionless includes, and unsaved schema text.

Generated-output navigation supports **C++, Java, JavaScript, TypeScript, Go, C#,
Rust, Python, Swift, Kotlin, and C**, including the supported naming profiles.
With separate C++ output, included types and fields open their own generated
headers, including transitive includes and renamed entry outputs. The compiler's
`// Serializer source:` comments identify the exact declaring schema when several
files share a basename.
Combined C++ output remains supported; select `--include-mode inline` or
`[output] include_mode = inline` in the compiler configuration when generating it.
Other output languages retain their existing compilation units and modules.

In caller code, use the language's normal navigation to reach the generated type,
then **Go to Declaration** to open its schema. The Visual Studio adapter does not
query other language services for a one-step caller-to-schema jump.

Navigation reads available files. It never saves, configures, builds, or prompts to
generate missing output. Retain the compiler's dependency files (`--depfile`) in
the solution tree or beside generated output, especially for renamed files and
Python, Swift, Kotlin, and C output without a generated-file banner. Serializer's
CMake generation helpers already produce this metadata.

This extension does not provide semantic diagnostics, completion, general
expression/member-call navigation, snippets, or the separate VS Code extension's
CMake commands. Native blocks remain opaque; deferred function lookup is lexical.
Use
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
[source-build and installation guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/cmake_integration.md#use-an-installed-package).
Open a `.serializer` schema and build your project's generation targets when you
need generated-output destinations.

Published package versions may lag this source checkout. Compare the Marketplace
version with the [extension changelog](https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/visual_studio/CHANGELOG.md)
before relying on recently added behavior. A locally built VSIX is a separate
release state from a published package.

[Project website](https://www.singh.org.in/serializer.html) ·
[Serializer repository](https://github.com/rohit-singh-gautam/Serializer) ·
[Serialization guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/usage.md) ·
[C++ state-management guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/managed/getting_started.md)
