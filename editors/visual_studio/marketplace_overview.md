# Rohit Serializer for Visual Studio

Compiler/runtime **2.34.0** requires a C++23 compiler and matching standard
library for generated C++ applications. Both local packages are **2.0.22**.
They support schema language **1.24.0** and earlier compatible headers.
Language 1.24 adds schema-local `xml(ordered("items"), ordered_child("branch", "{uri}name"))`
and package `target_mode`, `requires_type` and `vocabularies` settings.
Compiler/runtime 2.34 also adds `sub`/`sup` to the closed thirty-tag HTML fragment adapter.

Language **1.24.0** adds local managed scalar `index` declarations in compiler/runtime
2.34.0. Ordered/unordered keys, optional uniqueness and `on = local` / `inherit`
retain qualified enum and generated-field navigation. Metadata names are not
type destinations. Database placement, composite indexes and lazy storage remain
outside this initial schema subset. See the [index contract](../../docs/schema/indexes.md).

Compiler/runtime **2.33.0** also provides opt-in Java caller-owned output
windows through `java.output_buffer_reuse = true`. Both packages retain type/field
navigation around the additional generated overloads; method parameters remain
ordinary Java symbols. See the [Java ownership and failure contract](../../docs/languages/java.md#caller-owned-output-windows).

Compiler/runtime **2.33.0** independently adds heap/direct `ByteBuffer` windows
through `java.nio_output_buffer_reuse = true` (default false). Generated type/field
navigation retains ordinary Java parameter and local-variable behavior around
these qualified overloads. Cursor/ownership and failure bounds are documented in
the [Java NIO API](../../docs/languages/java.md#caller-owned-nio-buffers).

Compiler/runtime **2.34.0**, language **1.24.0**, adds unsigned C++ `bits(width)` fields, checked scalar defaults and exact generic calls. Java adds opt-in `java.stream_output` and fixed-scalar packed buffers. Explicit native separate layouts retain dependency source links for navigation. See the [compiler/backend guide](../../docs/guides/compiler_backend_updates.md).

Language **1.23.0** binds direct self-token fields in a named managed generic
instantiation to that named C++ record. The instantiate declaration selects the
contract; older included declarations retain their behavior. Navigation preserves
the named declaration and copied generic field provenance. See the
[binding contract](../../docs/schema/named_managed_references.md) and
[qualification status](../../docs/verification/verification-named-managed-references-2026-10-24.md).

Version **2.0.22** also prepares namespace/tag and brace indexes for generated
C++ and limits field searches to complete candidate names. The native adapter
passes direct and separate-values named-reference navigation within its original
256 MiB budget; authored C++ retains its existing indexing behavior. See the
[allocation qualification](../../docs/verification/verification-native-cpp-navigation-2026-10-24.md).

Version **2.0.22** indexes generated C declarations without tokenizing bundled
runtime implementation bodies. Type, enum, package and field destinations retain
their source offsets; the Visual Studio interpreter keeps its existing resource limits.
See the [native navigation qualification](../../docs/verification/verification-native-c-navigation-2026-10-24.md).

Version **2.0.22** supports schema language **1.22.0** cache declarations for
compiler/runtime **2.30.0**: bare `cache` defaults to priority `0`, and
`cache(priority = 200)` gives a higher retention preference within `0..255`.
Owning cache values and cache persistence services currently require C++; other
outputs diagnose these declarations explicitly. Cache type navigation preserves
qualified names, transitive includes, unsaved edits and ordinary types named `cache`.
The new shared `.<filename>.serializer/` companion directory separates recovery
journals, retained caches and required state. See the
[cache contract](../../docs/design/cache_attributes.md). Building packages does
not install them or publish Marketplace listings.

Both local packages share version **2.0.22**. Building and validating these files
does not install the extensions or update live Marketplace listings.

The existing schema language **1.20.0** authoring support includes XML defaults, field overrides,
namespace `using schema` declarations and declarative package roles. Qualified
role model types navigate to their declarations; fixed XSD paths open the local
file without treating it as a Serializer include or fetching remote resources.
XML policy/placement tokens do not claim type destinations. VS Code also provides
XML/package authoring snippets. Compiler/runtime providers and supported vocabulary
remain prerequisites; editor support does not establish format conformance.
Typed C++ `managed_ref<T>` tokens navigate to the managed target, including forward
and included declarations. `class_id(decimal)` preserves explicit class identity;
its numeric metadata does not become a type reference. VS Code supplies token and
class-identity snippets. Both local packages share version 2.0.22; building them does
not install or publish them.


Generated C#/Java package classes, Kotlin package objects, JavaScript package factories and TypeScript namespace facades
navigate to their schema package declarations and back, preserving enum
navigation, naming profiles, live edits and cursor boundaries.

Version **2.0.4** supports language **1.19.0** inline value references such as
`public borrowed(payload) d (1);`, including payload-type and optional owner-field navigation.
Borrowed views use application-owned storage; shared/weak modes are optional.
Output contains the ordinary field value.
Scoped translators can project payload records containing arrays without copying their
buffers. Language 1.18 named durable address references remain supported separately.
See [value references](../../docs/schema/value_references.md) and
[durable addresses](../../docs/schema/references.md). Generated helper-type navigation
for anonymous inline wrappers is not a separate authored schema declaration.
Both editor packages remain synchronized. Local package verification does not
publish or update the live Marketplace listing.


**Editor support for Serializer — State Framework**

Rohit Serializer helps you edit `.serializer` schemas and navigate between schemas
and existing generated code in **Visual Studio 2022 and Visual Studio 2026 on
Windows x64**. It provides syntax highlighting, editing assistance, and native
navigation for projects using Serializer — State Framework.

The current source release is **2.0.22**, supporting schema language **1.24.0**
and navigation for Serializer **2.34.0**'s C++ headers and native dependency bundles.
The compiler/runtime, schema language, and editor extension have independent
versions; existing 1.x schema language headers remain supported.

Compiler/runtime 2.3.0 offers optional generated XML codecs and fixed XSD
bindings across output languages through generator INI/CLI configuration. Existing generated type
and field navigation applies. XML/XSD validation, ZIP/OPC packaging, and
whole-object compression are runtime/compiler features; the extension provides
no XML, XSD, or HTML language service. See the
[XML and package support report](../../docs/design/xml_support.md) for the
implemented subsets, provider dependencies, verification by backend, and pending capabilities.

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
by this extension. The native C++ engine and explicit Python, Java,
JavaScript/TypeScript owning profiles have separate capability and qualification
contracts; see the [binding guide](../../docs/managed/language_bindings.md).
Token-only output does not activate a state engine.

## Get started

Install **Rohit Serializer** from the existing
[Visual Studio Marketplace listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio)
and restart Visual Studio. Install the compiler and runtime separately using the
[source-build and installation guide](https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/guides/cmake_integration.md#use-an-installed-package).
Serializer source builds and individual example CMake projects require CMake **4.0+**;
each selected output language also requires its own SDK.
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
