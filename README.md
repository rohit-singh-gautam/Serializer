# Serializer

**State Framework**

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

Define models in `.serializer` schemas and generate language-specific code for
exchanging data. For editable C++ applications, add the managed runtime to group
changes into transactions, retain history, recover committed work, and coordinate
collaborative edits through an authority.

Generated codecs support multiple languages. Native history, journaling, and
collaboration engines currently run in C++; other languages can exchange managed
wire records but do not have equivalent native managed engines.

[Project website](https://www.singh.org.in/serializer.html) ·
[Documentation index](docs/README.md) ·
[Serialization quick start](#get-started) ·
[State-management quick start](docs/managed/getting_started.md) ·
[Feature status](docs/project/feature_status.md)

**Using a coding agent?** Give it the
[Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).

| Need | Component | Entry point |
| --- | --- | --- |
| Define and exchange data | Schema compiler and generated codecs | [Usage guide](docs/guides/usage.md) |
| Evolve serialized payloads | Payload revisions and compatibility checks | [Versioning](docs/schema/versioning.md), [schema evolution](docs/schema/schema_evolution.md) |
| Group editable-model changes | C++ managed transactions and persistent identity | [Managed quick start](docs/managed/getting_started.md) |
| Undo, redo, and retain branches | C++ linear or tree snapshot history | [Managed runtime](docs/managed/cpp_runtime.md) |
| Recover committed work | C++ appended or sidecar journals | [Journal contract](docs/managed/journal.md) |
| Coordinate edits across clients | C++ collaboration and authority policy hooks | [Local collaboration](docs/managed/local_collaboration.md) |
| Resolve typed entities and application keys | Opt-in C++ snapshot registries and field indexes | [State registries](docs/managed/state_registry.md) |
| Explore mostly-disk indexes | Proposed paged backend and database comparison | [Paged indexing](docs/design/paged_indexing.md) |

## Releases and supported outputs

The current compiler and runtime release is **2.1.0**. Bug fixes and minor updates
increase the patch number; new features increase the minor number. Both preserve
full compatibility. Developers change the major number manually, with best-effort
compatibility across major releases. The schema language version is **1.17.0**;
see [release and schema versions](docs/guides/command_line.md).

Release 2.1.0 requires CMake 4.0 or newer and adds standalone CMake entry points
for individual examples in every supported language. Configure a folder such as
`example/cpp/basic` directly, then build and run its checks with CTest. See
[individual example builds](example/README.md#build-an-individual-example-with-cmake)
and [2.1.0 migration](migration.md#compilerruntime-210).

Installed packages expose their enabled compression backends so standalone
examples can detect capabilities in both static and shared runtimes.

Release 2.0.2 fixes private arithmetic helper names in generated Rust output for
strict native Windows builds with `rustc 1.99` and `-D warnings`, and runs the
self-checking conversion examples correctly through the shared example runner.
Regenerate affected Rust output and rebuild its consumers; schemas, public model
and function names, and wire formats are unchanged. See
[2.0.2 compatibility](migration.md#compilerruntime-202) and
[Rust qualification](docs/verification/verification-rust-windows-2026-10-15.md).

Release 2.0.1 fixes strict Clang builds of the digest runtime on 32-bit ARM.
Schemas, public APIs, generated output, digest results and wire formats are unchanged;
see [2.0.1 compatibility](migration.md#compilerruntime-201).

Release 2.0.0 changes the default generated C++ artifact layout and the CMake
package major version. Copy or install each entry header's adjacent `.includes/`
tree, or select `--include-mode inline` for one combined header. Versioned
consumers should request `find_package(Serializer 2.0 CONFIG REQUIRED)` and
regenerate through their build rules. Supported 1.x schema headers, generated
model names, runtime APIs and wire contracts retain their existing behavior;
see [2.0 migration](migration.md#compilerruntime-200).

Use `serializer version 1.17.0;` for new schemas. Older `1.0.0`, `1.1.0`, `1.2.0`, `1.3.0`, `1.4.0`, `1.5.0`, `1.6.0`, `1.7.0`, `1.8.0`, `1.9.0`, `1.10.0`, `1.11.0`, `1.12.0`, `1.13.0`, `1.14.0`, `1.15.0`, and `1.16.0` schemas remain supported. The original
`serializer version 1;` is an exact compatibility alias for `1.0.0`, exclusive to
version 1. Future versions require all three components; see the
[schema language policy](docs/guides/command_line.md#schema-language-version-policy).

**Supported outputs:** C++, Java, JavaScript, TypeScript declarations, Go, C#,
Rust, Python, Swift, Kotlin, and C. TypeScript uses the JavaScript runtime.

Generated C++ retains schema declaration order, combines consecutive declarations
with the same access, and preserves source blank-line grouping between fields. See
[C++ output layout](docs/languages/output_configuration.md#names-compatibility-and-generated-syntax).

Language `1.17.0` adds [explicit array and callable APIs](docs/schema/array_and_callable_api.md):
`expose array(public)` selects mutable/const native C++ indexing and ranges;
`readonly` disables writes, `const_value` preserves value-returning const APIs,
and `dimensions(Rows, Cols)` states row-major axes. Portable output uses checked
logical helpers. Owner-type field `default(identity)` keeps omitted/fresh
factory behavior without class-wide constraints. Callable guards, array
bindings/generation, structural delegation and richer C++ signatures remove
supported host adapters. Backend boundaries and
[qualification status](docs/verification/verification-array-callable-api-2026-10-13.md)
are documented separately. Native **134/134**, all eleven SDKs, editor checks
and both **1.1.46** packages passed the recorded qualification.

Language `1.16.0` adds [generic owner-parameter defaults](docs/schema/construction.md#generic-owner-parameter-defaults-in-language-116):
`constructor(int32 count, Frame placement = {})` preserves open C++ representations
without adding a factory or defaultability constraint. Omission creates a fresh
`Frame{}`; explicitly supplied nondefault/nonassignable frames remain valid.
Independent input deduction and fixed-array cardinality keep their existing
boundaries. Existing `1.15.0` record defaults and `1.14.0` lowering remain unchanged.
See [qualification](docs/verification/verification-generic-owner-defaults-2026-10-12.md)
for actual verification status.

Language `1.15.0` adds [empty class-valued parameter defaults](docs/schema/construction.md#empty-class-valued-parameter-defaults-in-language-115):
`constructor(int32 a, int32 b, area input = {})` creates a fresh owning `area`
with its schema field defaults when omitted. Named factories support the same
syntax. Full positional defaults such as `area input = {10, 20}` remain available
since `1.14.0`; ordinary function parameter defaults are unavailable. No field
identity or wire format changes. See [qualification](docs/verification/verification-empty-record-defaults-2026-10-12.md)
for verification status.

Language `1.14.0` adds [owning record APIs](docs/schema/owning_api.md): readonly
lvalue C++ getters, direct member initialization, typed fixed-array/nested defaults,
reciprocal constructor signatures, declaration-site C++ additions, and explicit
arithmetic operator mappings with owning constructor calls in portable expressions.
Existing fields, wire formats and managed protocols retain their contracts.
Run the [four focused examples](example/README.md#owning-record-apis) and consult
[qualification](docs/verification/verification-owning-api-2026-10-12.md) for tested
backends and limits. Both current editor packages are **2.0.1**, with separate
C++ generated-header navigation. See
[editor verification](docs/editors/editor_extension.md#verification-performed)
for package checks and navigation qualification.

Language `1.13.0` adds the C++ function qualifier `noexcept(cpp)` for readonly
native bodies and external declarations, including dispatched member contracts.
It emits C++ `noexcept` as an author promise; other outputs retain their own
failure behavior. Portable expressions and predicates keep their inferred C++
guarantees. See [the contract and migration example](docs/schema/behavior.md#explicit-c-noexcept-contracts)
and [verification status](docs/verification/verification-noexcept-2026-10-12.md).

Language `1.12.0` adds C++ constructor overloads, nested independent input types,
constrained readonly functions with explicit `common_type<T, Input>` numeric
results, native `storage(none)` and `constexpr(cpp)` qualification, and authoring
validation/initialization hooks. Generic record constructor defaults require an
explicit `factory(name)` constraint. See [construction](docs/schema/construction.md),
[behavior](docs/schema/behavior.md), and [generics](docs/schema/generics.md) for contracts and
backend limits. The C++ registry also supports guarded, versioned borrowed
[external-provider leases](docs/managed/state_registry.md#borrowed-external-provider-leases).
See the [verification record](docs/verification/verification-authoring-2026-10-11.md).

Language `1.11.0` adds lossless `checked<T>(value)` numeric bindings,
representation-preserving integer arithmetic, projected readonly record calls,
lazy owning-record conditionals, generic record map keys, and extended typed
construction. See [behavior](docs/schema/behavior.md), [construction](docs/schema/construction.md),
and [generics](docs/schema/generics.md) for backend limits. C++ managed
[state registries](docs/managed/state_registry.md) add pinned typed lookups, stable field
paths, custom-key uniqueness and optional ordered/hash indexes while retaining
the existing identity, history, journal and collaboration protocols.
Mostly-disk indexing is a [separate design proposal](docs/design/paged_indexing.md).
The [custom-ID examples](example/custom_ids/README.md) demonstrate application-owned
scoped keys in every language and a C++ managed registry companion.
See the [enhancement verification record](docs/verification/verification-registry-numeric-2026-10-11.md)
for executed native, SDK and editor checks and their platform limits.

Language `1.10.0` adds [namespace functions and declared readonly calls](docs/schema/behavior.md#declared-function-calls)
inside portable expressions and [Boolean generic type constraints](docs/schema/generics.md#boolean-type-constraints),
such as `T: (integral || floating_point) && !bool`. See the runnable [expression example](example/cpp/expressions/README.md).
C++ emits eligible functions and codecs as `constexpr`, useful results as
`[[nodiscard]]`, and proven nonthrowing operations as `noexcept`.
See the [verification record](docs/verification/verification-functions-constraints-2026-10-11.md)
for the native, all-language and editor checks completed for these additions.

Language `1.9.0` adds [derived native binary sizes](docs/schema/derived_sizes.md), including
`wire_only` headers and `size_scope(record|payload)`, comma-separated
[`omit(all_except(...))`](docs/schema/magic_and_omission.md), explicit
[record conversions](docs/schema/conversions.md), and generic
[`stateless` constraints](docs/schema/generics.md#stateless-type-constraints).
All language outputs share these schema contracts. Decoder size validation can
be configured per language and class. See [runnable examples](example/README.md)
and the [qualification record](docs/verification/verification-deferred-features-2026-10-11.md).
C++ also derives positional size headers in its
[compile-time and emission-only output](docs/serialization/constant_evaluation.md), producing
the same schema byte image as normal runtime encoding.

Language `1.8.0` adds [portable constructors, factories, structured defaults,
and empty transient layout hints](docs/schema/construction.md), owning generic
[inheritance](docs/schema/generics.md), and checked comparisons, boolean guards,
division and `math.sqrt/sin/cos` [expressions](docs/schema/behavior.md). Typed readonly
predicates add exact integer/enum checks, member/index access and bounded `all`/`any`
reductions, returning false for reached invalid checks.
Schema constructors retain a public C++ default constructor automatically;
default arguments are separate. Generic factory parameter defaults require the
explicit language `1.12.0` contract described above.
See the [construction guide](docs/schema/construction.md).
Compiler/runtime `1.13.0` adds allocation-free bounded keyed binary counting,
borrowed span/reference output, an opt-in signed-zero output policy, reusable C++
generated type guards, [class-specific record comparisons](docs/schema/record_comparisons.md), and
guarded transaction collection callbacks. Existing default codecs and wire
identities remain unchanged. See [usage](docs/guides/usage.md) and
[verification](docs/verification/verification-integration-enhancements-2026-10-11.md).
Algorithm-qualified digest return signatures retain the existing built-in digest
representation; see [digest values](docs/schema/digest.md).

### Borrowed native binary output

Native C++ binary output and bounded counting accept `std::span` and
`std::reference_wrapper` directly, including nested generated records, optionals,
and map payloads. They preserve the owning value's wire bytes and output policies
while borrowing source storage for each synchronous call. Keep the referents alive
and unchanged through traversal; see [borrowed output](docs/guides/usage.md#borrowed-native-binary-values).

**Start here:** [Usage guide](docs/guides/usage.md) · [Runnable examples](example/README.md) ·
[CMake integration](docs/guides/cmake_integration.md)

Payload [versioning and schema evolution](docs/schema/versioning.md) support `version`,
`compatibility`, `created`, `obsolete`, `replaced`, and class-scoped `reserve` declarations.
Release catalogs and nested allow policies (`any`/`all`, age/count/date limits) resolve
to compatibility bounds during generation; generated codecs contain no release-date
processing. All eleven language outputs share current/historical read policies and versioned
positional binary layouts. See the [versioning examples](example/README.md#versioning).

Schema-owned [magic headers and format omissions](docs/schema/magic_and_omission.md)
use `private magic { 'SRLFILE' };`, typed `private magic uint32 (99) {42};`,
and `omit(json, binary_positional)`.
These declarations require owning classes; view modes reject them explicitly.
Magic is static metadata: writers emit it, readers verify and discard it, and
JSON includes the fixed `magic` value by default. Native binary writes legacy
header bytes or the declared scalar/enum encoding before the separate version discriminator. C++ format selection
and omissions resolve at compile time without per-object flags.
C++ byte magic uses readable character literals and a comment showing its escaped
contents, preserving the exact bytes without an implicit terminator.

Language `1.2.0` adds [compact unsigned scalar fields](docs/schema/compact_integers.md):
`public compact_prefix strict uint32 value (3) {32};` uses the existing 30-bit
prefix encoding, while `compact_varint uint64 count;` uses full-width unsigned
LEB128. Explicit `lenient` selects deterministic truncation. All output languages
support these owning scalar annotations; JSON retains ordinary integer values.

Language `1.3.0` adds [digest bytes and creation](docs/schema/digest.md): bare `digest`
stores a variable-length application-supplied value, `digest[32]` fixes its byte
length, and `digest(sha256)` selects a standard algorithm's length. All native
outputs support digest storage/codecs; C++ includes 0BSD creation and hex helpers
without an OpenSSL dependency. Fields do not compute hashes automatically.

Language `1.4.0` adds [owning variants](docs/schema/unions_and_variants.md):
`public variant(string = text, event = event) payload;` generates C++
`std::variant` storage with safe construction, copying, moving, and destruction.
JavaScript/TypeScript expose one `{kind, value}` payload. Existing `union`
declarations retain their generated APIs. Both keywords use the same native wire
indices and names, and [paired examples](example/README.md#union-and-variant)
cover every output language.

Compiler [simplicity warnings](docs/schema/schema_reference.md#simplicity-warnings) suggest
schema-owned `magic` for initialized fields named `magic`, and the `version`
keyword for supported scalar fields named `version` or `revision`. A `version`
declaration can omit its default member name: `public version uint32 (1) { 1 };`.
Language `1.5.0` adds field-local `ignore(warning magic, version)` before the
semicolon for intentional uses. Warnings do not change generated APIs or bytes;
review an existing wire contract before replacing an ordinary field.

Language `1.7.0` adds [generic optional values](docs/schema/optional.md):
`optional<T>` stores absence or an owning value and maps directly to C++
`std::optional<T>`. Nested optionals and optional arrays/maps have distinct
JSON and native binary representations across every output language.

C++20 [constant-evaluation binary output](docs/serialization/constant_evaluation.md) can count a
value's exact positional size, write caller-owned memory, or return an exact-size
byte array from a deterministic factory. Enable generated models with
`--cpp.constant_evaluation true`; its default is false. Ordinary runtime codecs
retain their existing traversal, and ordinary owning runtime memory writes use the
existing encoder.
The independent `--cpp.protocols binary_none` profile restricts C++ output to
positional codecs; the default remains `all`. Add `--cpp.emission_only true` with
those settings for borrowed string/span DTOs exposing only one concrete positional
`serialize_out(binary_none_output&)` method.

Release **1.15.0** also makes ordinary generated `serialize_out`, `serialize_in`,
and their uncompressed stream entrypoints `constexpr`, independently of that option.
Constant evaluation depends on the chosen protocol, stream, and value operations;
the direct native binary scalar subset is described in the
[C++ qualifier guide](docs/serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers).
Eligible checked arithmetic and dotted-version construction retain runtime calls.
Value results use `[[nodiscard]]`; proven nonthrowing operations use `noexcept`.

## Proprietary applications and licensing

Generated output may be used, modified, and distributed under your application's
license, including a proprietary license. Serializer-authored generated code is
covered by [LICENSE-GENERATED](LICENSE-GENERATED); copyright and license notices
from the input schemas remain applicable and are preserved in generated output.
Generation does not transfer ownership of your schema to Serializer or transfer
ownership of Serializer-authored support code to you.

Application runtime headers, compiled helpers, and managed support use the
[Zero-Clause BSD license](LICENSE-RUNTIME). Link `Serializer::runtime` for generated
C++ models or `Serializer::managed` for managed applications. Generation helpers
choose the runtime target automatically. The compiler and its
`Serializer::serializer_lib` parser/generator API remain
[GPL-3.0-or-later](LICENSE). Third-party dependencies retain their own licenses.
See [licensing and schema notices](docs/project/licensing.md) before redistributing a package.
The [licensing rationale](docs/project/licensing.md#why-the-license-was-separated)
explains why application support has separate permissions while the compiler
retains GPL, and how those permissions differ from copyright ownership.

## How it works

1. Describe your types, fields, and defaults in a `.serializer` file.
2. Run the compiler, or let CMake generate code during your build.
3. Use the generated classes to read and write your chosen format.

The compiler and generators are written in C++. Generated non-C++ codecs run in
their target language without a native Serializer runtime dependency.

## Get started

### 1. Write a schema

Save this as `person.serializer`:

```text
serializer version 1.5.0;

namespace demo {
  class person {
    public string name;
    public uint32 age;
  }
}
```

Every schema begins with a supported language header. `array[]` and typed `magic`
require `serializer version 1.1.0;` in the file that declares them; the original
`1` alias remains exactly `1.0.0`.
Each field declares its access
level, type, and name. Start with [schema examples](docs/schema/schema_examples.md) for
arrays, maps, enums, and defaults.

### 2. Generate code

After [building or installing Serializer](docs/guides/cmake_integration.md), run the
compiler to generate a C++ header:

```sh
serializer --input person.serializer --output person.hpp
```

When the entry schema includes other schemas, C++ generation emits separate
dependency headers by default and includes them from the entry header. Use
`--include-mode inline` or `[output] include_mode = inline` to keep one combined
header. See [schema includes](docs/guides/usage.md#share-declarations-with-includes)
for output layout and migration details.

Use [output configuration](docs/languages/output_configuration.md) to choose a language or
naming profile. The [command-line guide](docs/guides/command_line.md) lists all options.

To generate several configurations of the same schema from one parse, repeat
`--config` and supply corresponding output paths. Existing INI files
remain independent; see [batch generation](docs/guides/command_line.md#generate-several-configurations-from-one-parse).

### 3. Add it to your build

For a C++ application with Serializer already added as a CMake dependency:

```cmake
add_executable(my_app main.cpp)
serializer_generate(TARGET my_app SCHEMAS person.serializer)
```

A normal build generates the header and supplies the include directory, runtime
library, and C++20 requirement. See [CMake integration](docs/guides/cmake_integration.md)
for complete source-dependency and installed-package setups.

Use [`serializer_generate_variants`](docs/guides/cmake_integration.md#generate-several-configurations-from-one-parse)
when several C++ targets need different configurations of the same schemas. Each
schema is parsed once and generates every target's variant in that build command.

Next, follow the [C++ usage guide](docs/guides/usage.md) to serialize a value and decode a
complete message with explicit limits, or run the [basic C++ example](example/cpp/basic/README.md).

### Build an editable C++ application

Follow the [managed-state quick start](docs/managed/getting_started.md) and
[runnable managed examples](example/managed/README.md) to generate typed editors,
link `Serializer::managed`, and edit a `model_store` through transactions. Start
with local history, then add a journal or collaboration when the application needs
them. History mode and journal/collaboration support can be selected at compile
time; the application supplies transport, synchronization scheduling, and trusted
sessions for collaboration.

The managed runtime is optional for serialization-only applications. Fresh source
builds expose it by default; ordinary schemas do not acquire managed identity or
history storage merely because it was built.

## Choose a language

Reusable [schema generics](docs/schema/generics.md), such as `class result<T>`, emit
native C++ templates: application code can use `result<std::uint32_t>` without
concrete schema declarations. Nested templates, trailing defaults, positive uint64
dimensions, and fixed `array[N] T` storage are supported in C++.
Language `1.1.0` adds `array[] T` with an extent inferred from nonempty defaults:
`public array[] uint32 values {1, 2, 3};` produces `std::array<std::uint32_t, 3>`.
`public array[] char signature {'SRLFILE'};` uses seven decoded bytes with no NUL terminator.
Optional `instantiate person_result = result<person>;` declarations and concrete
schema fields define contracts for all eleven generators. Other languages keep
concrete APIs. All eleven ordinary outputs support fixed arrays without explicit
initializers, enforcing the extent during encoding and decoding. Direct Protobuf
fixed-array output remains unsupported. See the [generic examples](example/generics/README.md)
and [qualification record](docs/verification/verification-dimensions-2026-10-04.md).

All languages use the same schema compiler. Their generated APIs and runtime
requirements are documented separately.

| Output | Guide | Examples |
| --- | --- | --- |
| C++ | [Usage and decoding](docs/guides/usage.md) | [C++ examples](example/cpp/README.md) |
| Java | [Java 17+ codecs](docs/languages/java.md) | [Java examples](example/java/README.md) |
| JavaScript / TypeScript | [Portable language APIs](docs/languages/portable_languages.md) | [JavaScript](example/javascript/README.md), [TypeScript](example/typescript/README.md) |
| Go / C# | [Portable language APIs](docs/languages/portable_languages.md) | [Go](example/go/README.md), [C#](example/csharp/README.md) |
| Rust / Python / Swift / Kotlin / C | [Native language APIs](docs/languages/native_languages.md) | [All language examples](example/README.md) |

The [build and generation guide](docs/guides/build_and_generation.md) includes compiler
commands for each language group and explains naming profiles.

## Schema behavior and managed state

Language 1.6 and compiler/runtime 1.11 add schema-declared methods, ordered native
blocks, typed portable arithmetic and transient memory fields. C++ managed models
support private data, unrelated public bases, owned variants, concrete generic
specializations and payload lifecycle metadata. Runtime APIs add guarded cache
updates, explicit reset clones, notifications, navigation, complete-history
migration and bounded snapshot patches.

The [custom C++ example](example/cpp/custom_code/README.md) demonstrates native
preambles, class declarations, inline method bodies, and an application-owned
method definition alongside generated codecs.

Read [concrete state framework examples](docs/managed/state_enhancements.md) and
[method generation](docs/schema/behavior.md) before choosing a combination. The
[integration skill](.agents/skills/serializer-integration/SKILL.md#use-the-full-framework)
requires reviewing the full feature set and using every applicable implemented
feature. Runnable document-tool examples cover generated APIs and failure paths;
remaining engine/storage combinations are listed explicitly.

## Choose a format

Serializer supports four native protocols across its generated language codecs.

| Format | How fields are represented |
| --- | --- |
| JSON | Named fields in readable text |
| Positional binary | Values in schema order, without field keys |
| Integer-key binary | A numeric field ID before each value |
| String-key binary | A wire field name before each value |

Native JSON supports `std::optional<T>` as a host type (`null` or a typed value).
For evolving JSON documents, use the opt-in `read_policy::flexible`
reader to skip additional fields while rejecting duplicate keys and enforcing
decode limits. Default JSON input remains strict. Regenerate classes to enable
the unknown-member protocol hook; see [JSON compatibility](docs/guides/usage.md#json-compatibility).

Native binary codecs default to little-endian byte order. Strings require valid
UTF-8; use `array uint8` for arbitrary bytes. Read the
[wire-format contract](docs/serialization/wire_format.md) before exchanging data between systems.

C++ also offers optional [Protobuf binary, ProtoJSON, and TextProto codecs](docs/serialization/protobuf.md).
These are separate from the four native protocols.

For evolving schemas, use [explicit field IDs](docs/schema/schema_reference.md#explicit-field-ids-with-stable_ids)
and the [schema compatibility checker](docs/schema/schema_evolution.md). Stable IDs alone
do not make native readers accept unknown fields. Evolving JSON documents can
explicitly select `read_policy::flexible` after regenerating their classes.

## Build and test

Fresh builds enable managed history, journaling, collaboration, SIMD, and all
compression backends. Zstandard, LZ4, and zlib are required; the repository vcpkg
manifest acquires them through its default features. Existing caches retain their
settings; use `-U SERIALIZER_BUILD_MANAGED -U SERIALIZER_WITH_*` to adopt the new
defaults, or set each option explicitly.

The default development build needs CMake 4.0+, a C++20 compiler and standard
library, clang-format 19+, and GoogleTest 1.18.0 or newer. See the
[requirements and setup guide](docs/guides/cmake_integration.md#build-this-repository)
for dependency installation and optional tools.
GoogleTest is a repository-test dependency supplied by the vcpkg `gtest` package
or an installed GTest CMake package; it is separate from the application runtime.
Windows test builds automatically copy imported shared runtime DLLs beside each
test executable so CTest and direct launches can load them.

**`clean` and `rebuild` delete all untracked and ignored files and directories
through `git clean -fdx`, including untracked source, local configuration, build
caches, dependencies, and editor packages.** Tracked files and local tracked
edits remain intact. Cleanup covers the repository regardless of build-directory
or configuration options.

On Linux with GNU Make:

```sh
make all
make test
make clean
make rebuild
# WSL with a Windows/MSBuild host:
make extension
```

On Windows with Visual Studio Developer PowerShell (x64):

```powershell
./make.ps1 all
./make.ps1 test
./make.ps1 clean
./make.ps1 rebuild
./make.ps1 extension
```

The compiler rejection tests invoke MSVC directly. Developer PowerShell supplies
the compiler's standard-library and Windows SDK include paths for those tests.

Both wrappers provide synchronized `configure`, `all`, `test`, `clean`, `rebuild`,
and `extension` commands. `clean` verifies the repository root and runs Git cleanup;
Git is required and CMake is not. Git metadata, the index, and nested Git
repositories protected by Git's single-force semantics remain intact. External
build/toolchain directories are outside the cleanup scope. `rebuild` cleans the
repository first, then configures and uses CMake's `--clean-first` to finish native
cleanup before rebuilding.

Windows `all` and `rebuild` also package both editor extensions. They require
Node.js 22+, npm, and Visual Studio MSBuild; they do not install the packages.
Linux `all`, `test`, and `rebuild` build native CMake targets. Windows `test` and
`configure` remain CMake-only.

`./make.ps1 extension` builds only the two editor packages, without configuring
or building CMake targets or cleaning the repository. WSL `make extension` invokes
the same Windows packaging script through PowerShell. Both require the Windows
tools above; native Linux cannot build the Visual Studio VSIX and reports that
limitation.

For direct CMake commands, presets, builds without GoogleTest, and troubleshooting,
see [build and generation](docs/guides/build_and_generation.md#build-and-test).

The upstream vcpkg package is `rohit-singh-gautam-serializer`:

```sh
vcpkg install rohit-singh-gautam-serializer
```

Package availability can lag the source repository. Check the port version and
build options for the features you need; use the
[source-build guide](docs/guides/cmake_integration.md#build-this-repository) for the
documented current source capabilities. The root `vcpkg.json` acquires development
dependencies and is separate from the upstream port; see
[distribution guidance](docs/project/distribution.md).

## Editor support

Both **Rohit Serializer** extensions highlight `.serializer` files and provide
navigation between schemas and existing generated output in all 11 output languages.
The extensions are separate from the compiler and runtime.

| Editor | What it adds | Setup |
| --- | --- | --- |
| Visual Studio Code | Highlighting, snippets, schema navigation, CMake generation commands, and missing-include assistance | [VS Code guide](docs/editors/editor_extension.md) |
| Visual Studio 2022 / 2026, Windows x64 | Highlighting, editing configuration, native declaration/type-definition navigation, F12, and Ctrl+click | [Visual Studio guide](editors/visual_studio/README.md) |

Both extensions use source release version **2.0.1**, independent of compiler version
**2.1.0**. Embedded `cpp { ... }`, `java { ... }`, and other target blocks reuse
available language grammars for syntax highlighting. Install the language's grammar
provider where the editor does not bundle it; this does not add embedded IntelliSense.
See [embedded source highlighting](docs/editors/editor_extension.md#embedded-source-highlighting)
and the [navigation coverage matrix](docs/editors/editor_navigation.md#navigation-coverage-matrix)
for supported destinations and language-service prerequisites.
VS Code also supports navigation from read-only Git index/history tabs, using
the displayed snapshot and current workspace destinations.
From a typed external function declaration in a schema, **Go to Definition**
also locates available authored C++ out-of-class definitions. Lookup matches the
qualified owner and overload signature, reads live buffers, and never builds.
See [deferred function definitions](docs/editors/editor_navigation.md#deferred-function-definitions-1147)
for lexical matching limits and verification status.
See the [custom code and highlighting verification](docs/verification/verification-custom-code-2026-10-11.md)
for the completed native, editor, grammar-provider, and package checks.

## Explore advanced features

Start with these only when your application needs them.

For application editing, start with the
[history, journal, collaboration, and authorization guide](docs/managed/getting_started.md).
It explains what each feature does, how they work together, and what your
application must provide.

| Need | Guide |
| --- | --- |
| Share declarations across schemas | [Includes](docs/guides/usage.md#share-declarations-with-includes) |
| Read or update encoded C++ data through borrowed views | [Buffer views](docs/serialization/views.md) |
| Produce exact positional bytes during C++20 constant evaluation | [Constant binary APIs](docs/guides/usage.md#count-and-emit-constant-positional-bytes) |
| Encode directly into an owned `std::string` buffer | [String streams](docs/guides/usage.md#string-backed-output-buffers) |
| Use standard streams or durable files | [Streams and adapters](docs/guides/usage.md#stream-concepts-and-implicit-adapters), [file streams](docs/guides/usage.md#file-streams-and-journal-records) |
| Compress complete C++ messages | [Compression](docs/serialization/compression.md) |
| Group edits and support undo/redo in C++ | [Managed runtime](docs/managed/cpp_runtime.md), [small examples](example/managed/README.md) |
| Recover managed state after a crash | [Journaling](docs/managed/journal.md) |
| Synchronize edits between managed stores | [Local collaboration](docs/managed/local_collaboration.md) |
| Control which sessions may publish edits or acquire locks | [Authorization](docs/managed/authorization.md) |
| Understand runtime and generated-code optimizations | [Performance features](docs/serialization/performance.md) |
| Plan database persistence | [Database integration](docs/guides/database_integration.md) |
| Plan indexes larger than RAM | [Paged indexing research and proposed design](docs/design/paged_indexing.md) |

Native managed runtimes outside C++ and Serializer database adapters are not
implemented. The [feature status guide](docs/project/feature_status.md) separates available
features from proposals and records their limitations.

Disk-backed paged indexes are also proposed. The current managed store keeps its
model, identity table, and retained snapshots in memory; paging an index alone
would not make the model larger than RAM. See the
[paged indexing study](docs/design/paged_indexing.md) for existing database approaches,
cache limits, compatibility requirements, and staged implementation criteria.

## Integrate with a coding agent

Give your agent this request, adapting the dependency path:

```text
Read vendor/Serializer/.agents/skills/serializer-integration/SKILL.md
and use it to integrate Serializer into this application's CMake build.
```

The skill covers schemas, generation, codec selection, and bounded decoding.
Keep the tracked `.agents/skills/serializer-integration/` directory when copying
or packaging the source repository.

See [agent discovery and setup](docs/guides/agent_integration.md) for automatic discovery
and use from a consuming project.

## Typed command-line declarations

Use `src/command_line.hpp` and `cli::commandline_declaration` for common options,
subcommands and typed retrieval. Declare `command_options` and `command_entry`
values, call `decl.parse(argc, argv)`, then read `decl["command"].get_path("input")`
or other typed getters. The library owns variant values, validates required
options and numeric conversions, and generates global/command help. It supports
repeatable string/path lists, explicit positional inputs, command tails, and
native Windows argv. Failed reparsing preserves the previous successful result.

Keep option types, defaults and requirements in declarations; handlers consume
typed values without reparsing. Use `decl.common()` for common options and
`was_provided` to distinguish defaults from explicit input. Check
`help_requested()` and write help before calling `cli::dispatch_command(decl)`.
The original compiler string-map API remains compatible. See the
[declaration guide](docs/guides/command_line.md#typed-command-line-declarations) for complete examples, getter types, ownership,
error behavior, and positional/tail syntax. This source utility is not an
installed runtime header.

## Contributing and community

Start with the [contribution guidelines](CONTRIBUTING.md) to report bugs, propose
features, or submit a pull request. Participants follow the
[code of conduct](CODE_OF_CONDUCT.md). Report vulnerabilities privately using the
[security policy](SECURITY.md); see the [accessibility statement](ACCESSIBILITY.md)
for reporting barriers to using the project.

## Reference and project status

- [Product positioning](docs/project/product_positioning.md): shared descriptions and capability boundaries for documentation and distributions.
- [Schema and protocol reference](docs/schema/schema_reference.md): types, field IDs, views, and protocol examples.
- [Schema examples](docs/schema/schema_examples.md): small declarations you can adapt.
- [Migration guide](migration.md): upgrade requirements and compatibility changes.
- [Feature status and roadmap](docs/project/feature_status.md): implemented behavior and future work.
- [Constant-evaluation guide](docs/serialization/constant_evaluation.md): implemented C++20 positional
  size counting, existing-memory writing and exact-array factories, opt-in generation,
  runtime path preservation, supported values and compiler qualification.
- [Four-toolchain verification](docs/verification/verification-toolchains-2026-10-07.md): x64 Windows/MSVC and clang-cl, and Ubuntu WSL2/GCC and Clang, with default runtime features enabled.
- [Optional qualification](docs/verification/verification-optionals-2026-10-11.md): nested presence, all eleven language SDKs, managed recovery, examples, and editor checks.
- [Default runtime verification](docs/verification/verification-default-features-2026-10-07.md): managed/compression defaults and installed-header checks.
- [Compact scalar qualification](docs/verification/verification-compact-2026-10-07.md): all eleven SDKs, frozen wire fixtures, strict/lenient cases, native tests, and editor checks.
- [Verification record](docs/verification/verification-2026-09-17.md): tested revisions, configurations, and outstanding checks.
- [Versioning verification](docs/verification/verification-versioning-2026-10-06.md): historical layouts, all eleven language examples, and editor checks.
- [Release-policy verification](docs/verification/verification-release-policies-2026-10-06.md): compiler folding, calendar boundaries, nested policies, and generation options.
- [Qualification](qualification/README.md): interoperability, fuzzing, and performance workflows.
- [Coding standard](CodingStandard.md) and [agent instructions](AGENTS.md): repository contribution rules.
- [Licensing](docs/project/licensing.md): proprietary generated output, permissive application runtime, and GPL compiler scope.
- [Digest storage and creation](docs/schema/digest.md): byte contracts, supported algorithms, C++ helpers, and provider/licensing boundaries.
- [Digest verification](docs/verification/verification-digest-2026-10-09.md): recorded native, cross-language, and editor checks and their scope.

When updating an existing application, check the migration guide and regenerate
affected output before rebuilding. Verification records describe specific tested
configurations; they do not establish support for every platform or toolchain.
