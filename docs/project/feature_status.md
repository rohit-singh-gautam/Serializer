# Features and implementation status

This is the central catalog for supported capabilities and proposed work. The
overview groups features; linked contracts and backend matrices define exact
coverage and verification. Proposals are not delivery commitments.

## Capability overview

| Capability | Implemented status and reference |
| --- | --- |
| Native serialization | JSON and three native binary protocols; [wire contract](../serialization/wire_format.md) and [language outputs](../languages/README.md). |
| Optional Protobuf formats | C++ Protobuf binary, ProtoJSON and TextProto; [mapping limits](../serialization/protobuf.md). |
| Schema models and behavior | Types, collections, generics, construction/defaults, callables, conversions and evolution; [schema reference](../schema/README.md). Backend limits vary. |
| C++ output optimization | Binary views, constant evaluation, protocol selection and emission models; [constant evaluation](../serialization/constant_evaluation.md) and [performance](../serialization/performance.md). |
| XML and XSD | Optional generated owning XML mappings across all outputs, fixed XSD subset and explicit prepared validation; [usage and backend scope](../serialization/xml_packages.md). |
| Streams and compression | C++ stream adapters, whole-message helpers and composable whole-object compression streams; [compression](../serialization/compression.md). General backend parity remains pending. |
| ZIP and OPC | Codec-independent C++ ZIP32, OPC metadata/relationships, shared budgets and full-Save helpers; [package APIs](../serialization/xml_packages.md#plain-zip-and-opc). Backend parity and typed profiles remain pending. |
| Managed application state | C++ identity, transactions, snapshot history, journals, collaboration and typed registries; [managed index](../managed/README.md). Other outputs exchange records without native managed engines. |
| Builds and examples | CLI/INI generation, CMake integration, standalone examples and native Go projects; [build guide](../guides/build_and_generation.md) and [examples](../../example/README.md). |
| Editors | Visual Studio and VS Code schema editing and navigation; [editor index](../editors/README.md). |

## Proposed capabilities

| Capability | Proposal and boundary |
| --- | --- |
| Broader XML and XSD | [Pending report](../xml_pending.md): tree-free optimized tokens, richer vocabularies, XSD bundles/facets, export and wider model coverage. |
| ZIP/OPC parity and profiles | [Package requirements](../design/xml_support.md#multipart-packages-and-opc): all-language APIs/qualification, generated typed profiles, preservation and reproducibility. |
| Reusable document-file profiles | [Proposed profile](../design/xml_support.md#reusable-document-file-profiles): complete saved files, side-channel-only history/recovery and independent external/internal codecs. Current native C++ formats remain unchanged. |
| Compression parity | Whole-object stream wrappers across outputs with exact framing, limits and optional providers. Incremental records/object serialization are not required. |
| Format parity | C++-only Protobuf APIs and `binary_none`-only generation still need equivalent general backend support. C++ constant evaluation and borrowed native views remain language-specific. |
| Mostly-disk indexes | [Paged indexing](../design/paged_indexing.md): bounded page cache and later lazy storage. Not implemented. |
| Database adapters | [Integration guidance](../guides/database_integration.md): storage mappings and a proposed sink boundary; no repository adapter is implemented. |
| Broader managed capabilities | [Proposals below](#proposals-and-future-work): schema feature selectors, broader authorization/distributed behavior and native managed engines for other outputs. |
| Other language enhancements | [Remaining roadmap](#other-roadmap-items): expanded default validation, input-member positions and bit fields. |

## Detailed status and release context

Compiler/runtime **1.14.0** and language **1.9.0** add
[native binary derived sizes](../schema/derived_sizes.md), stored or `wire_only` headers,
record/payload scope, class-specific decoder validation, complement omission
lists, [explicit portable record conversions](../schema/conversions.md), and
[stateless generic constraints](../schema/generics.md#stateless-type-constraints).
Supported backend combinations and executed checks are recorded in
[qualification](../verification/verification-deferred-features-2026-10-11.md). Raw ABI dumps are
not an output protocol; conversions support the ordinary value shapes documented
in their guide.

Compiler/runtime **2.2.0** adds `model_store::discard_journal_changes()` for ordinary
journaled C++ stores. Under the native writer lock it checks the physical file
budget but reads only the fixed header and bounded last full-Save frame, without
reading or buffering an appended unsaved suffix. It validates and restores the
saved values and complete history, and
atomically replaces the journal generation. Object/revision allocation high-water
marks never decrease, abandoned edits cannot reappear through redo/recovery, and
the durable operation sequence advances once. Creation is the baseline before the
first Save. Shared or unattached stores reject this operation. Preparation failures
preserve the live state; uncertain replacement fences the writer until fresh
recovery. This operation adds no saved-envelope copy or work to ordinary edits and
changes no schema language or existing wire format.

[Back to the project overview](../../README.md)

Current compiler/runtime **2.3.0** supports language **1.17.0** and includes
`constexpr` ordinary C++ codec templates and a native positional/integer-key
scalar decoder subset for constexpr-safe typed streams. Eligible checked
arithmetic and dotted-value construction/conversions also constant-evaluate.
Useful value results are `[[nodiscard]]`; proven nonthrowing value operations use
`noexcept`, with conditional guarantees for generic comparisons/observers.
See [direct codecs and qualifiers](../serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers)
for limitations and executed qualification.

Release **2.1.1** makes the maintained Go consumers native modules with their
own `go.mod`, entry schema, `go:generate` directive, and imported `internal/model`
package. An installed Serializer CLI and Go are sufficient for generation,
builds, and package tests; Python runners remain independent matrix qualifiers.
See [Go projects](../../example/go/README.md).

Release **2.1.0** requires CMake 4.0+ and adds standalone CMake entry points for
individual examples across all supported languages.
See [example builds](../../example/README.md#build-an-individual-example-with-cmake).

Compiler/runtime **2.0.0** adds separate C++ generated headers for schema
includes, enabled by default. Select `--include-mode inline` or
`[output] include_mode = inline` for the previous combined header layout.
Other language backends retain their existing compilation units and modules;
separate-file generation for them is not implemented. See
[include layout and migration](../guides/usage.md#share-declarations-with-includes).

The 2.0.0 package's `SameMajorVersion` policy rejects 1.x package requests;
update versioned consumers to `find_package(Serializer 2.0 CONFIG REQUIRED)`.
Supported schema language headers remain 1.x, with 1.17.0 as the current
language release. See [2.0 migration](../../migration.md#compilerruntime-200).

Separate-header Windows Release verification passed all 134 native CTest entries, including
CLI/INI overrides, Ninja dependency regeneration, generic and managed
roundtrips, and library artifact/preamble tests. Both **1.1.48** editor packages
were built and validated; **39,219** fresh compiler navigation checks passed
across all eleven output languages, including separate C++ headers. Compiler
2.0.0 release verification also passed three CLI/package checks and fresh
installed-package builds and tests for all three updated standalone C++ examples.
Non-Windows rebuilds and live editor-host checks were not run for this change.

Language `1.17.0` adds [array and callable APIs](../schema/array_and_callable_api.md).
Array exposure and explicit axes, deferred owner field factories, callable-local
availability and typed array/delegation bindings preserve existing storage.
Native borrow signatures and richer callable types have documented target
boundaries. [New qualification](../verification/verification-array-callable-api-2026-10-13.md)
passed the native 134-entry suite, all eleven SDKs and editor/package checks.

Language `1.16.0` adds [generic owner-parameter defaults](../schema/construction.md#generic-owner-parameter-defaults-in-language-116)
for constructors and named factories. An omitted `Frame placement = {}` uses
native C++ `Frame{}` without adding a factory or defaultability constraint to the
owning class. Explicit nondefault/nonassignable inputs remain valid. Known owning
generated record substitutions retain the portable `1.15.0` default contract;
open native and concrete primitive/enum/container substitutions require C++.
Independent input deduction, direct scalar defaults and fixed-array cardinality
retain their boundaries. See [qualification](../verification/verification-generic-owner-defaults-2026-10-12.md)
for the checks executed against this feature.

Language `1.15.0` adds [empty class-valued parameter defaults](../schema/construction.md#empty-class-valued-parameter-defaults-in-language-115)
for constructors and named factories, including nested empty record literals.
An omitted `area input = {}` creates fresh owning field defaults, without shared
mutable state or parameterized authoring hooks. Full positional defaults remain
available from
`1.14.0`; partial/named literals and ordinary function defaults are unavailable.
Under that contract, unconstrained generic type parameters are unavailable;
inherited/managed/packed/native-storage records and array cardinality relaxation
remain outside the portable record-default contract. See
[qualification](../verification/verification-empty-record-defaults-2026-10-12.md)
for this feature's executed verification status.

Language `1.14.0` adds [owning record APIs](../schema/owning_api.md), with
[four maintained examples](../../example/README.md#owning-record-apis): borrowed
readonly C++ getters, direct construction, portable nested defaults, reciprocal
signatures, C++ declaration additions, explicit arithmetic mappings and owning
constructor expressions. See [qualification](../verification/verification-owning-api-2026-10-12.md)
for executed tests and backend boundaries.

Language `1.13.0` adds explicit [C++ `noexcept(cpp)` function contracts](../schema/behavior.md#explicit-c-noexcept-contracts)
for readonly native bodies and external declarations, including namespace functions
and dispatched member contracts. It is an author promise checked as C++ by the
selected compiler; other outputs retain their normal exception/error behavior.
Explicit qualifiers on `edit` or portable expression/predicate bodies, conditional
specifications and arbitrary native postfix tokens are unsupported. Existing
inferred C++ guarantees remain unchanged. See
[verification status](../verification/verification-noexcept-2026-10-12.md).

Language `1.12.0` adds opt-in C++ [authoring construction](../schema/construction.md#authoring-construction-in-language-112):
constructor overloads, independent record inputs, fresh generic factory defaults,
validation/initialization hooks, and explicit native constant-evaluation contracts.
Constrained readonly C++ inputs use an explicit scalar `common_type<T, Input>`
result policy. Guarded borrowed external-provider leases supplement owning
providers without changing managed identity or persistence protocols. See
[verification and limitations](../verification/verification-authoring-2026-10-11.md).

Language `1.11.0` adds lossless numeric bindings and exact integer expressions,
projected readonly record calls and lazy owning-record conditionals,
[extended construction](../schema/construction.md), and [generic record map keys](../schema/generics.md).
The C++ [state registry](../managed/state_registry.md) is opt-in derived snapshot data;
it preserves scalar persistent identity and existing managed protocols.
[Paged indexing](../design/paged_indexing.md) remains a proposal.

Language `1.10.0` adds [namespace functions and declared readonly calls](../schema/behavior.md#declared-function-calls)
in expressions, with exact argument/result types and forward signature lookup.
It also adds [Boolean generic constraints](../schema/generics.md#boolean-type-constraints)
checked on concrete schema applications and emitted as C++ `requires` clauses.

Serializer — **State Framework** combines schema-driven data exchange with C++
application-state management. Generated codecs cover multiple languages; native
transactions, history, journals, and collaboration currently run in C++.

See [payload versioning](../schema/versioning.md) for revision types, historical positional
layouts, field lifetimes, replacements, reservations, common read policies, and
compiler-only release-date/count policies. These resolve to ordinary version bounds
and add no dates or policy evaluation to generated codecs.

Use this guide for advanced capabilities and their limits. Implemented behavior
is described separately from broader design proposals.

Language `1.4.0` and compiler/runtime `1.9.0` add
[owning variants](../schema/unions_and_variants.md) beside existing raw unions. C++ uses
`std::variant` with safe value semantics; JavaScript/TypeScript use one
discriminated `{kind, value}` object. The four native protocols retain the
existing union wire contract, with paired examples in every output language.

Compiler/runtime `1.10.0` adds advisory [simplicity warnings](../schema/schema_reference.md#simplicity-warnings)
for ordinary magic/revision candidates and repeated default version member names.
Language `1.5.0` adds field-local `ignore(warning magic, version)` suppression.
The checks use names and supported scalar types rather than inferring application
intent; warnings do not change generation, runtime behavior or wire bytes.

For an introduction to application editing features, begin with
[history, journals, collaboration, and authorization](../managed/getting_started.md).

## Schema behavior and managed enhancements

Language 1.8 also supports typed [construction, factories and structured
defaults](../schema/construction.md), owning [generic inheritance](../schema/generics.md), and
stateless transient layout hints. Compiler/runtime 1.13 adds compatible
C++ per-type import guards, bounded allocation-free keyed binary counting,
opt-in signed-zero output normalization, and generated guarded collection
callbacks. Layout overlap is supported by the C++ ABI attribute and native
zero-sized value representations; other runtimes retain their ordinary empty
record storage. These APIs do not add native managed engines outside C++.

Typed readonly predicates support exact integer/enum comparisons, public
member/index access and bounded sequence reductions. Predicate failures return
false; ordinary checked expressions retain their failure APIs. Eligible C++
readonly methods preserve constant evaluation and nonthrowing contracts. See
[behavior contracts](../schema/behavior.md) for the supported scalar and collection profile.

Language 1.6 adds function contracts, explicit target bodies/preambles and
`transient` fields. Compiler/runtime 1.11 adds C++ managed public multiple bases,
nonpublic durable fields, owning variants, named concrete managed generics,
payload lifecycle metadata, immutable runtime updates, clone reset policies,
precise changed-ID notifications, navigation, migration, verified snapshot patches,
opt-in bounded delta journal frames and conservative resident admission accounting. General
fixed arrays now have exact-length codecs and independent defaults in all eleven
ordinary outputs. See [runnable document examples and remaining work](../managed/state_enhancements.md)
and [behavior generation](../schema/behavior.md). Native managed engines outside C++ remain
unimplemented; codecs and behavior declarations do not supply those engines.

Compiler/runtime 1.13 also adds opt-in [class-specific record equality and ordering](../schema/record_comparisons.md)
across all eleven outputs, with scalar/record portable restrictions and unchanged wire formats.

Language 1.8 adds portable checked division, sqrt/sin/cos, comparisons and boolean
guards/results in all eleven ordinary outputs. Arithmetic domains and finite
intermediate checks preserve short circuit branch evaluation; sine/cosine use
each SDK's math library. See the [portable expression contract](../schema/behavior.md#portable-arithmetic).

## Serialization and streams

A C++20 schema compiler with C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python,
Swift, Kotlin, and C output supporting JSON and three binary protocols. The compiler and
all generators remain entirely C++; generated portable codecs have no native runtime
dependency. The C++ runtime API and default generated C++ use `snake_case`.

Owning schemas support fixed static `magic` and per-field `omit(format, ...)`
across all native languages. C++ also supports these in its opt-in Protobuf
mappings. Generated C++ selects omissions at compile time; view layouts reject
these annotations explicitly. See [magic and omission](../schema/magic_and_omission.md).
Language `1.1.0` adds typed scalar/enum magic while preserving legacy byte literals.

Language `1.2.0` adds owning unsigned scalar `compact_prefix` and `compact_varint`
annotations with strict/lenient policies across all eleven outputs. Prefix uses
the 30-bit ID/length encoding; varint preserves the full unsigned type range.
JSON and optional Protobuf mappings remain unchanged. Signed/floating compact
formats, collections, magic, payload versions, packing and views are unsupported.
See [compact encodings](../schema/compact_integers.md) for the wire contract and proposals.

Language `1.3.0` adds bare variable `digest`, fixed `digest[N]`, and named
`digest(algorithm)` byte values in all eleven native outputs. Four native codecs
retain the existing byte-sequence representation and validate fixed sizes. C++
provides original 0BSD creation and hex helpers for twelve algorithms, without
OpenSSL. Other languages use application-chosen providers for computation.
Fields are application-supplied; map keys, nonempty defaults, C++ views, and
Protobuf digest mappings are unsupported. See [digest storage and creation](../schema/digest.md).

Opt-in C++ codecs also support **Protobuf binary, ProtoJSON, and TextProto** through
compile-time protocol templates, for both encoding and decoding. See [Protobuf
codecs](../serialization/protobuf.md) for generation, schema mappings, and limitations. ProtoJSON
duplicate ordinary fields replace earlier values, including nested objects and later
`null`; binary Protobuf retains its message-merging rules. TextProto checks decoded
string storage budgets before growing strings. See the [finalization verification
record](../verification/verification-finalization-2026-09-18.md).

C++ stream APIs use structural C++20 concepts: custom implementations need no
`rohit::stream` base class. Generated calls accept standard streams directly through
implicit adapters. Memory input streams borrow their unread storage; file streams use
larger I/O batches; custom contiguous buffers retain the direct codec path. Generated
owning classes also provide a static pair: `Type::serialize<Protocol>(stream, value)`
and `Type::deserialize<Protocol>(stream[, limits])`, which returns a new object.

Use `object.serialize_in<Protocol>(stream, limits)` to supply explicit decode limits, or
omit the second argument to retain the defaults. See [stream concepts and
adapters](../guides/usage.md#stream-concepts-and-implicit-adapters).

## C++ constant-evaluation binary output

Compiler/runtime 1.4.0 implements `binary_none_size`,
`serialize_binary_none_to`, and `make_binary_none_bytes<Factory>()` in
`<rohit/constant_binary.hpp>`. Enable generated owning models with
`--cpp.constant_evaluation true` (default false). The generator adds a separate
constexpr `serialize_constant_out` traversal and static capability metadata;
ordinary runtime traversal remains unchanged. Runtime existing-memory
writes for ordinary owning models delegate to the established optimized binary encoder.

The independent `--cpp.protocols binary_none` profile removes JSON/keyed output;
its default is `all`. Positional-only generation rejects Protobuf, managed,
and view combinations. Scalars, eligible strings/vectors/arrays, nested objects,
legal initialized raw unions, magic, compact fields and payload revisions support
constant output; maps, views, managed operations, I/O and compression remain
outside the C++20 constant profile. Runtime decoding consumes the resulting bytes.

The optional `--cpp.emission_only true` profile requires enabled constant
evaluation and positional-only protocols. It emits namespace-isolated borrowed
`string_view`/`span<const T>` fields and a concrete, non-template
`serialize_out(binary_none_output&)` method, with no reader. Maps, managed/views,
nonempty array defaults and multidimensional arrays are rejected. Inline fixed-storage
owners can create these views during traversal without string/vector allocation.
See [emission-only output](../serialization/constant_evaluation.md#94-emission-only-borrowed-models).

Actual generated C++20 owning fixtures passed on MSVC 19.51, GCC 15.2/libstdc++, Clang 21.1.8/libstdc++,
and clang-cl 21.1.8 with MSVC headers. The tested cases and remaining performance
qualification are recorded in the [constant-evaluation guide](../serialization/constant_evaluation.md#17-implemented-feature-qualification-2026-10-07).
No universal zero-build-cost or exhaustive runtime benchmark claim is made.

Language **1.7.0** and compiler/runtime **1.12.0** add
[owning generic optionals](../schema/optional.md), with standard C++ `std::optional<T>`
storage, nested presence, collection payloads, and native binary/JSON codecs.
See the contract for defaults, managed behavior, and mappings, and the
[optional qualification record](../verification/verification-optionals-2026-10-11.md) for
executed native, all-language, managed, example, and editor checks.

## Message compression

Optional C++ [message compression](../serialization/compression.md) supports standard Zstandard, LZ4,
gzip, zlib, and raw DEFLATE formats. All three backend dependencies are enabled
by default; compression calls remain explicit. Generated
member/static calls accept compression options; free helpers support existing headers.
Calls validate one complete frame with independent input, output, and window limits.
Additional formats can supply a custom backend. See
[examples](../guides/usage.md#compress-complete-messages) and the [compression verification
record](../verification/verification-compression-2026-09-17.md).

The [compression examples](../../example/compression/README.md) provide runnable programs
for all five built-in formats, uncompressed output, and a custom backend. Build enabled
formats with `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON` or the standard test build;
reduce the enabled dependencies with `SERIALIZER_WITH_*`. The [iostream
examples](../../example/iostream/README.md) provide seven runnable memory, file, buffered,
and custom stream examples with a shared 52-class, 645-field schema. They run in the
standard test build or independently with `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON`.

## Schemas and generated languages

[Schema generics](../schema/generics.md) support native C++ templates without concrete schema
uses, nested applications, trailing defaults, and positive uint64 dimensions.
Optional named roots and concrete fields produce contracts for all eleven outputs.
Fixed arrays use std::array and native C++ codecs; JavaScript/TypeScript and
Python also support explicit extents without initializers in compiler 1.6.0.
All eleven ordinary backends now validate fixed extents; direct Protobuf still
rejects them. Named concrete managed generic specializations and public unrelated
managed bases are supported. Generic views, raw managed unions, recursive managed
ownership, user specialization and non-C++ native generic APIs remain unsupported. See the
[qualification record](../verification/verification-dimensions-2026-10-04.md).
Language `1.1.0` adds `array[] T` extent inference from nonempty defaults, including
decoded byte counts for a single char text literal. Fixed extent limits and wire
behavior remain unchanged.

Language-specific output profiles select layouts and naming conventions. Schemas use
`.serializer` and begin with a supported language header; new schemas use
`serializer version 1.17.0;`. Older `1.16.0`, `1.15.0`, `1.14.0`, `1.13.0`, `1.12.0`, `1.11.0`, `1.10.0`, `1.9.0`, `1.8.0`, `1.7.0`, `1.6.0`, `1.5.0`, `1.4.0`, `1.3.0`, `1.2.0`, `1.1.0`, `1.0.0` and the original `1` alias remain supported.
Schema language and compiler versions are independent.
Future versions require all three components. Share declarations with `include
common;` before any declarations. Paths are unquoted and relative to the including file;
`.serializer` is appended when the filename has no extension. Explicit `include
common.serializer;` remains supported. Includes are loaded once per entry schema and
emitted as separate C++ headers by default; inline C++ mode and other backends
combine them in the entry output.

Namespace scopes are reused during parsing; duplicate types and namespace/type conflicts
are rejected. See [schema includes](../guides/usage.md#share-declarations-with-includes) and the
[paired C++/Java examples](../../example/includes/README.md). Quoted defaults preserve
literal spaces, for example `public string label { "schema default" };`; escaping the
space is unnecessary. Run `serializer --version` for compiler version **2.1.1** and
supported schema versions. See [command-line options](../guides/command_line.md) for
multi-language generation and overrides.

Compiler **1.5.0** also generates independent INI configurations from one schema
parse by repeating `--config` and the corresponding outputs. The C++
`serializer_generate_variants` helper registers one generation command per schema
for several configured targets. INI syntax, schema language **1.2.0**, and existing
single-configuration calls remain compatible. See
[batch generation](../guides/command_line.md#generate-several-configurations-from-one-parse)
for output ordering and failure behavior.

Compiler **1.6.0** adds fixed-array generation without explicit initializers to
JavaScript/TypeScript and Python. Native sequence counts remain unchanged;
generated codecs enforce exact extent in all four protocols. Compiler 1.11
extends the same contract to Java, Go, C#, Rust, Swift, Kotlin and C. Initialized
collection defaults outside previously supported C++ cases remain restricted. See the
[backend matrix](../schema/generics.md#backend-support).

Compiler/runtime **1.7.0** separates the 0BSD application runtime target
`Serializer::runtime` from the GPL compiler API target
`Serializer::serializer_lib`. Serializer-authored generated code can be
distributed under proprietary terms, with input-schema copyright/license
notices preserved. See [licensing](licensing.md) and the
[migration guide](../../migration.md).

See [Java output](../languages/java.md) for dependency-free Java 17+ codecs and [all
examples](../../example/README.md) for self-contained example folders.

## Cross-language managed records

The [managed wire examples](../../example/managed/multilanguage/README.md) qualify
history, journal-baseline, collaboration, and opaque-session records across
language codecs. These are record-exchange tests, not ports of the managed runtime;
native managed engines outside C++ remain unimplemented.
Kotlin accepts the contextual property name `field`, used by collaboration records,
while preserving its wire spelling and field ID.

## Database storage

The [database integration guide](../guides/database_integration.md)
compares document databases, SQL JSON columns, and opaque binary storage, including
MongoDB, Firebase, PostgreSQL, SQLite, and other targets. It describes required
type mappings and a proposed sink boundary; database adapters are not implemented
or qualified in this repository.

[Paged indexing](../design/paged_indexing.md) researches disk indexes and proposes a bounded
page-cache sidecar followed by optional lazy model/history storage. Neither is
implemented. Current identity tables and whole-root history remain in memory;
resident admission accounting does not provide a hard process-memory ceiling.

## Implemented C++ managed features

The [C++ managed interface](../managed/cpp_runtime.md) is available with
`SERIALIZER_BUILD_MANAGED=ON` (the default) and `Serializer::managed`. Bare `managed` declarations
generate direct `persistent_id` fields and typed transaction editors by default. Set
`[managed] separate_values = true` to opt into ID-free values and storage wrappers. New
stores generate document namespaces automatically; object IDs increment from 1 within
each document, and saved identities/counters survive reload.

Start with the [nine small point examples](../../example/managed/README.md):
schema-generated managed points, transaction callbacks, generated editors, commit,
undo/redo, and cancellation. Each has its own folder, expected output, and a short
explanation. For the implementation, read [how managed transactions
work](../internals/managed_transactions.md): nested types, candidate ownership, callback
forwarding, commit, and cleanup. The companion [managed editor
walkthrough](../internals/managed_editors.md) explains channels, target resolution, and
map/array editor lifetimes.

The [ledger example](../../example/managed/ledger/README.md) uses
`transaction.root().entries().edit(id).set_memo(...)` without handwritten adapters. Its
annotated source and walkthrough explain document namespaces, object IDs, each
transaction form, and the state preserved by undo and save/load. Manual/scoped/callback
transactions, linear/tree history, and bounded memory save/load are implemented. IDs
default to uint32; configure `[managed] id_type` or `--managed.id_type` for uint64.
Schemas without managed declarations remain unchanged. Managed representation changes
require saved-state migration.

### History and feature selection

History mode is selected at compile time: `model_store<Root>` defaults to linear;
`model_store<Root, history_mode::tree>` selects tree, and `history_mode::disabled` omits
history. Each specialization contains only its selected storage. Journal and
collaboration can also be compiled out with the fifth template argument:
`model_store<Root, Mode, Labels, Traits,
store_features::none|journal|collaboration|all>` (select one value). `all` preserves
existing defaults; `none` plus linear/tree history selects history only.

Journal-only and collaboration-only stores use disabled history with the matching
feature value. Disabled features have no attachment storage or runtime checks, and their
store APIs are unavailable. Authentication remains host-owned and belongs to
collaboration, not a standalone store feature. See the [feature selection
examples](../managed/cpp_runtime.md#independent-store-features).

### History storage and labels

History currently retains whole-root snapshots. Default linear history uses a deque and
cursor, with no revision IDs or revision lookup. Its `undo()`/`redo()` navigate adjacent
entries, evicting oldest states to meet count/byte limits; tree history retains branches
in a map and rejects commits exceeding those limits. A changed linear commit after undo
discards redo. Labels are disabled by default: use `execute_transaction(callback)` or
`begin_transaction(outcome)` with no name.

Opt in with `model_store<Root, Mode, history_labels::enabled>` to store names and query
undo/redo labels; see the [labeled point
example](../../example/managed/labeled_history/README.md). Disabled labels have no string
member or serialized label field. Default saves use format version 3; label-enabled
linear/tree stores retain versions 2/1. Saved histories must match the receiving label
policy; see [migration](../../migration.md).

### File streams and journals

`<rohit/file_stream.hpp>` provides an owning file stream for both ordinary generated
serialization and journaling. It uses the existing stream concepts and adds explicit
`sync()`, `seek()`, and `truncate()` capabilities; memory streams in `stream.hpp` keep
their existing contract. Journal frame readers/writers also accept Serializer buffers
and supported iostream adapters. See [file streams and journal
records](../guides/usage.md#file-streams-and-journal-records). The [runnable journal
example](../../example/managed/journal/README.md) demonstrates both file modes, recovery,
undo/redo, and full Save.

Database sinks require an implemented, tested Serializer database adapter; none is
currently implemented.

Synchronous [journal and crash recovery](../managed/journal.md) now supports appended and
sidecar files: `create_journal`, `recover_journal`, and `save_journal` append each
already serialized snapshot once with compact framing, preserve undo/redo and allocated
IDs, and track unsaved values separately from durability. `journal_dirty()` and
`journal_sequence()` expose those independent positions. Uncertain I/O reports
`transaction_status::indeterminate` and blocks writes until recovery.

Schema feature selectors, exclusions, built-in authorization policies, and
other-language managed runtimes remain future work; unsupported syntax/backends fail
explicitly.

The [managed capabilities](../managed/capabilities.md) proposal covers history,
collaboration, authorization, and journaling. Managed storage carries a mandatory
persistent ID (`uint32` by default, centrally configurable). ID-free ordinary payloads
are an opt-in representation. [Journal recovery](../managed/journal.md) uses a base
snapshot plus an appended or sidecar journal, with durable undo cursors, full Save
replacement, and cleanup that preserves newer unsaved changes.

The full feature set remains a proposal; central persistent-ID configuration and the C++
identity/history runtime, synchronous snapshot journaling, and snapshot collaboration
are implemented.

### Collaboration

The implemented [C++ collaboration runtime](../managed/collaboration_runtime.md) provides
`collaboration_authority`, replicas with isolated proposal drafts, atomic snapshot
acceptance, base-sequence conflicts, exact operation retries, advisory presence,
entity/subtree leases, and ordered lock caches. Start with the [collaboration
examples](../../example/managed/collaboration/README.md). For immediate local editing,
attach a session with `store.collaborate(session)` to the existing `model_store`, then
use normal generated setters and transactions.

The store journals each commit and retains a durable outbox when a journal is attached.
Call `store.synchronize()` explicitly or `store.synchronize_if_due(now_ms)` from the
owner-thread timer; `send_pending()` and `receive_changes()` are also separate APIs.
Local undo/redo uses the same transaction mechanism and the ordinary `model_store`
journal API. The [store undo example](../../example/managed/collaboration/undo.cpp) uses
`execute_transaction`, `undo`, and `redo` with separate journaled client stores;
`synchronize()` handles submission and receiving accepted changes.

Host lock requests reserve IDs with `store.collaboration().reserve_operation_id()` so
they share the store's persisted operation counter without collisions; see the [store
locks example](../../example/managed/collaboration/locks.cpp). See the [store integration
guide](../managed/local_collaboration.md) and [local synchronization
example](../../example/managed/collaboration/local_sync.cpp). Conflicts retain local work
and expose received state separately for resolution.

### Acknowledged-state clients and collaborative undo

The lower-level acknowledged-state API remains available: Use `replica.propose(session,
operation, [](auto& edit) { ... })` with generated editors; the default authority
accepts these proposals without a handwritten command schema or dispatcher. Custom
server command handlers remain optional. Accepted transactions now include serialized
field-change records with author/operation identity, stable entity/field addresses,
before/after values, contribution versions, and ownership dependencies.

`replica.undo(session, operation)` and `replica.redo(...)` create atomic, conditional
history requests for the same acceptance path. They preserve unrelated edits and reject
same-field or ownership conflicts, including equal-value intervening writes. See the
[history
contract](../managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).
Upgrade both peers and regenerate managed headers: history-bearing collaboration uses
protocols 5/6 (uint64 sessions) or 7/8 (custom sessions). Model save formats are
unchanged. Authority contribution history lasts only for its epoch.

Store clients persist their local history, pending work and exact retries with the same
journal pointer; a new authority epoch retains uncertain work for explicit
reconciliation. Plain replicas continue to expose acknowledged state.

### Sessions and host responsibilities

The examples combine a server authority and separate client replicas in one process;
session IDs do not encode whether a client is local, remote, or read-only. Applications
own session creation, storage, connection binding, and lifetime. `std::uint64_t` is the
default; use `collaboration_session<std::string>` or
`collaboration_session<std::uint32_t>` for other IDs, or supply a session policy for a
custom value type.

Proposals, results, locks, presence, and policy hooks keep that type without an imposed
numeric registry. See the [session
example](../../example/managed/collaboration/sessions.cpp). Generated ownership traversal
checks the actual diff; optional host policy hooks gate changes and locks. Coordination
records originate in `.serializer` schemas. Transport, trusted session binding, and
distributed authority fencing remain host concerns. The [broader
contract](../managed/collaboration.md) includes future optimization and distributed
features.

This version retains retry state for its bounded epoch and sends full snapshots. Store
clients merge disjoint fields before submission; same-field/ownership conflicts remain
explicit. Offline IDs use persisted local-to-authority mappings; ordinary application
integer references are not remapped.

## Proposals and future work

### XML and multipart packages

Release **2.3.0** implements generated owning XML mappings across all outputs,
a deliberately bounded fixed-XSD subset and explicit runtime validation support.
See the [actual APIs](../serialization/xml_packages.md) and
[generated examples](../../example/xml/README.md). XML remains optional, default
members are children, and a supported supplied XSD takes authority over placement
and constraints. Changes to a fixed schema require regeneration/recompilation.

C++ implements whole-object compression streams, ZIP32 and an OPC metadata subset.
Plain ZIP stores arbitrary codec bytes without XML metadata. OPC adds content types
and scoped relationships; this does not establish Office document semantics.
Full-Save helpers stage complete files and add no Serializer history/session parts.
The managed dual-codec document bridge is not implemented.

The [full design](../design/xml_support.md) retains PKG01–PKG09 and completion
gates. General features must support every output; missing compression/package
backends, advanced XML/XSD, typed profiles and preservation remain
[explicit pending work](../xml_pending.md). Compression handles complete objects;
incremental record callbacks are not required.

The [managed-state design index](../managed/README.md) groups the proposals under
`docs/managed/`. The [data-structure design](../managed/data_structures.md) defines store
state, revision graphs, snapshot/delta records, checkpoints, and deleted-object
retention, with a [C++ class
walkthrough](../managed/data_structures.md#c-class-walkthrough) using actual ordinary
classes generated from a [draft example schema](../managed/walkthrough.serializer), with
codec methods omitted. These illustrative records differ from the implemented snapshot
envelope.

The [language-binding design](../managed/language_bindings.md) proposes C++
`model_store<Root, support>` (with a `managed` alias) and component-based stores for
other backends. These broader capability sketches differ from the implemented
`model_store<Root, Mode, Labels, Traits>` API, whose history mode is a template
argument. Runtime journaling and collaboration are separate from history policy. Consult
the C++ runtime guide above for the implemented subset; the broader generated APIs
remain proposals.

### Identity and history proposals

The [object identity and transactional history proposal](../managed/history.md) describes
an optional root collection with stable object IDs, grouped edits, and runtime linear or
branching undo/redo. It proposes `managed` on entity members, with ordinary values as
the default. Classes qualify through their own managed members or a class-level
`managed` marker; a managed member targeting an unmarked leaf type is an error.

Explicit `model_store` construction selects an eligible root. Separate plain and tracked
companion classes apply member annotations only in a tracked parent. Plain containment
neither activates nested managed annotations nor implies companion generation for the
containing class. Project/task examples cover deep value copying, managed factories, and
committed-change notifications. Change addresses combine a persistent entity ID with a
relative field-ID path, independently of snapshot/delta storage.

The language-independent contract distinguishes saved `exclude(history)` fields from
runtime-only `transient` caches and covers application restoration, including Android.
Broader projection and dependency policies remain proposals. Guarded transient
updates and publication callbacks with precise changed IDs are implemented in C++.
Bare `managed` and the typed editor subset are documented in the runtime guide
above. The [domain
examples](../managed/managed_examples.md) cover a cylinder with a hole, accounting,
wordpad, and other applications.

Scoped C++ transactions provide automatic commit on successful exit, explicit revert,
and observable completion failures. The implemented C++
[`execute_transaction`](../managed/history.md#callback-based-transaction-execution)
convenience API passes a borrowed edit context to one synchronous callback and returns
its completion outcome, using the same RAII transaction engine. All three forms remain
supported: manual commit, automatic completion on normal scope exit, and callback
execution, with explicit revert for cancellation.

Compact nested records can avoid repeating child IDs while preserving persistent
identity mappings. Retention rules distinguish deleted live objects from their
recoverable historical versions; storage budgets and measured editing latency come
before aggressive micro-optimization. The proposed [ownership and allocation
contract](../managed/history.md#ownership-and-custom-allocation) keeps managed lifetimes in
the store and permits configurable state/history/scratch resources.

It distinguishes optional internal reference counts from entity IDs and makes payload
allocator propagation and backend limitations explicit.

### Broader managed-state proposal

The companion [managed-state proposal](../managed/managed_state.md) explores editable
subtrees, application-supplied authorization, branch merging, collaboration, distributed
transactions, and external effects. It recommends `managed` and `model_store` for the
broader subsystem while retaining history as an optional component. Feature selectors
such as `managed(history)` and `managed(all except history)` use a pinned feature
profile and preserve mandatory authorization. `exclude(history, collaboration)` excludes
named features for ordinary values while preserving serialization.

These names and capabilities are proposals, not implemented features.

### Other roadmap items

- Expand default-value validation.
- Track member positions in the input stream.
- Add bit-field support.
