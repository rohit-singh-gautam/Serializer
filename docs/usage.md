# Using Serializer in a C++ application

See [payload versioning](versioning.md) for revision types, historical positional
layouts, field lifetimes, replacements, reservations, common read policies, and
compiler-only release-date/count policies. These resolve to ordinary version bounds
and add no dates or policy evaluation to generated codecs.

Use [magic and format omissions](magic_and_omission.md) for schema-owned fixed
headers, verified JSON identities, exact binary prefixes, and compile-time C++
output exclusions. Language `1.1.0` also supports scalar/enum magic values.

Language `1.2.0` supports [compact unsigned scalar fields](compact_integers.md)
in every output language. Keep existing generation and codec calls:

```text
serializer version 1.2.0;
class counter {
  public compact_prefix strict uint32 value (3) {32};
  public compact_varint uint64 total (4) {128};
}
```

Native positional binary writes `20 80 01` for these defaults; JSON keeps the
ordinary integer values. Prefix allows 30 value bits; explicit `lenient` writes
the low 30 bits without an overflow check. Varint preserves the full unsigned
type range. See the compact guide for all policies, limits and migration rules.

New to the project? Start with the [project overview](../README.md#get-started).
For editing and persistence concepts, read the
[history, journal, collaboration, and authorization introduction](managed/getting_started.md).

For pure Java generation and runtime usage, see [Java output](java.md) and the
[Java examples](../example/java/README.md). For JavaScript/TypeScript, Go, and C#,
see [portable language usage](portable_languages.md) and the
[any-to-any interoperability example](../example/interoperability/README.md).
For Rust, Python, Swift, Kotlin, and C, see [native language APIs](native_languages.md)
and [four examples per language](../example/README.md).
All outputs share the C++ schema compiler. The workflow below is for C++.

For reusable parameterized models, follow [schema generics](generics.md).
`class result<T>` emits a native C++ template without concrete schema uses:
`result<std::uint32_t> value{};` has the ordinary generated codec methods.
Nested `response<T>` and `message<T>` compose normally. Optional concrete fields
and `instantiate person_result = result<person>;` supply contracts for other
language outputs and compatibility checks; native C++ uses do not automatically
create those contracts.

Use `class matrix<uint64 Rows, uint64 Cols = Rows, T = double>` with
`public array[Rows * Cols] T elements;` for C++ fixed owning storage.
`matrix<3>` and `matrix<3,3,double>` are the same native specialization.
Fill `value.elements` as a `std::array` and serialize with the existing APIs;
exact input enforces cardinality and ordinary decode budgets. Fixed arrays outside
C++ native codecs are explicitly rejected. For a managed root containing ordinary
point/frame/matrix values, use generated whole-value setters within transactions;
the fixed arrays have no resize/insert/erase editor operations.

To infer a fixed extent from defaults, select schema language `1.1.0`:

```text
serializer version 1.1.0;
enum artifact_kind {
  document,
  image
}
class artifact stable_ids {
  private magic artifact_kind (99) {artifact_kind::document};
  public array[] uint32 samples (1) {1, 2, 3};
  public array[] char signature (2) {'SRLFILE'};
}
```

The C++ fields use `std::array<std::uint32_t, 3>` and `std::array<char, 7>`.
The char literal contributes decoded bytes with no implicit terminator. A missing
or empty initializer cannot infer an extent; every fixed extent remains
1..65,536. A fixed array retains its usual wire sequence count. Typed magic is
an immutable static scalar/enum constant, verified and discarded on input, with
no per-object storage. It uses the selected codec's scalar/enum encoding before
other native binary payload fields. Choose `private magic uint32 (99) {42};`
for an integer signature. See [fixed arrays](generics.md#infer-an-extent-from-defaults)
and [typed magic](magic_and_omission.md) for the complete contracts.

For a small introduction, work through the [point examples](../example/managed/README.md)
in order: create a schema-generated managed point, inspect a transaction callback,
then use the generated editor and complete managed transactions.
Each example keeps the application model to x and y and explains its expected output.
To follow the implementation, read the
[store and transaction walkthrough](internals/managed_transactions.md).

For grouped edits and undo/redo, see the optional
[C++ managed interface](managed/cpp_runtime.md). Bare `managed` class/member
annotations generate direct `persistent_id` fields and typed editors for manual,
automatic, and callback transactions. Central ID settings support uint32 and uint64;
`[managed] separate_values = true` opts into ID-free values plus storage wrappers.
New stores create document namespaces automatically; object IDs start at 1 and
increment within each document. Save/load preserves identities and allocation state.
Synchronous [journal and crash recovery](managed/journal.md) is available through
`create_journal`, `recover_journal`, and `save_journal`, with appended or sidecar
files. Commits and history navigation flush before publication; `journal_dirty()`
tracks unsaved values independently. Selectors and other-language
managed runtimes remain proposals.

For shared editing, use the [local store integration](managed/local_collaboration.md)
and [runnable examples](../example/managed/collaboration/README.md).
Attach a typed session with `store.collaborate(session)`, bind a transport and join
before editing. Normal `model_store` transactions immediately update local values
and queue changes; an attached journal flushes each commit together with the outbox.
`store.undo()` and linear `store.redo()` create local inverse transactions. The
[working undo example](../example/managed/collaboration/undo.cpp) uses these store
methods for every edit and history operation, with ordinary journals on both clients.
`store.synchronize()` exchanges work, or drive `synchronize_if_due(now_ms)` from the
owner thread at a configurable interval. `send_pending()` and `receive_changes()`
separate those directions. The store preserves drafts on conflicts and rejections;
received authoritative state is available through `store.collaboration().acknowledged_read()`.
Client save/recovery preserves the session, local history and exact outstanding request.
Transport reconnect and conflict-resolution choices belong to the application.
Host lock requests use `store.collaboration().reserve_operation_id()` after joining
to share the store's journaled operation counter; the host retains those requests
for exact retries. The [locks example](../example/managed/collaboration/locks.cpp)
passes acquired grants to the store session and edits through normal transactions.

The lower-level [C++ collaboration runtime](managed/collaboration_runtime.md) remains
available for explicitly acknowledged-state clients. A
default `collaboration_authority` accepts proposals created with
`replica.propose(session, operation, [](auto& edit) { ... })`. The callback uses
generated editors on a private draft; no application command schema or dispatcher
is required. Replicas apply ordered accepted snapshots, and custom server command
handlers remain optional. It includes retry deduplication,
base conflicts, presence, entity/subtree lease locks, and optional host policy hooks.
Accepted records also expose `history.fields` and `history.dependencies`, grouped by
the outer author/operation/sequence. Use `replica.undo(session, next_operation)` or
`replica.redo(session, next_operation)`, then submit and replicate the result exactly
as an edit. Undo/redo passes current authorization and lock checks, preserves unrelated
fields, and fails atomically on intervening conflicting changes. See the
[detailed history contract](managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).
These are acknowledged-state requests, not optimistic local writes. Both peers need
the history-bearing protocol and regenerated managed headers; consult [migration](../migration.md).
Include `<rohit/managed_collaboration.hpp>` and rebuild the managed target and model
headers. Default proposals carry a full candidate snapshot; draft IDs are provisional
until acceptance. After a conflict, resynchronize and use a new operation ID.
Session management belongs to the application. Existing APIs default to `uint64_t`;
select `collaboration_session<Session, SessionTraits>` for string, smaller unsigned,
or custom IDs and use its `authority<Root>`, `replica<Root>`, `records`, and `lock_cache`
aliases. The [session policy guide](managed/collaboration_runtime.md#application-owned-sessions)
describes validation, codecs, lifecycle, and wire compatibility.
Transport and trusted session binding belong to the host. Inherited authorization
rules, general conflict-free merging and peer-to-peer ordering remain unimplemented.

For cross-language preparation, the
[managed wire matrix](../example/managed/multilanguage/README.md) exchanges history,
journal-baseline, collaboration and session records between selected codecs.
It validates record preservation and malformed-input rejection; it does not provide
non-C++ managed engines or prove native undo, durable recovery or authentication.

To bound linear history for the generated point in the
[history example](../example/managed/history/main.cpp):

```cpp
rohit::managed::store_options options;
options.max_revisions = 100; // Includes the current state; at most 99 undo steps.
rohit::managed::model_store<point> store{
    point{1, 2}, rohit::managed::make_document_id(), options};
```

Successful changed commits discard redo after undo, then evict oldest states to
meet revision and byte limits. Failed, canceled, and no-op edits preserve history.
Select tree history with `model_store<point, history_mode::tree>` or disable it
with `model_store<point, history_mode::disabled>` (both names in `rohit::managed`).
The choice is compile-time; `store_options` has no mode selector.
The fifth argument independently selects `store_features::none`, `journal`,
`collaboration`, or `all` (the compatible default), after the fourth traits argument.
For example, `model_store<point, history_mode::linear, history_labels::disabled,
model_traits<point>, store_features::none>` is history-only. Pair disabled history
with `journal` or `collaboration` for those isolated configurations. Omitted store
APIs are unavailable at compile time; see [feature selection](managed/cpp_runtime.md#independent-store-features). Linear stores
use `undo()` and `redo()`, with no revision IDs or `checkout()`. Only tree stores
provide `redo(revision)`, `redo_children()`, and `checkout(revision)`. Tree mode
preserves branches and rejects over-budget commits instead. Loading a
save rejects excess history rather than trimming it; see the
[retention contract](managed/cpp_runtime.md#history-persistence-and-limitations).
Labels are disabled by default. Use `store.execute_transaction(callback)` or
`store.begin_transaction(outcome)` without a name. To enable names:

```cpp
using named_store = rohit::managed::model_store<
    point, rohit::managed::history_mode::linear,
    rohit::managed::history_labels::enabled>;
```

This also enables `execute_transaction("Move point", callback)`,
`begin_transaction("Move point", outcome)`, `undo_label()`, and `redo_label()`.
Tree stores use `redo_label(revision)`. See the
[labeled point example](../example/managed/labeled_history/README.md).
Default saves use format version 3 with label fields omitted. Label-enabled
linear/tree stores retain versions 2/1; loads require the same label policy.
See [migration notes](../migration.md).

For persistence, see [database storage and document sinks](database_integration.md).
It covers candidate JSON, typed-document, and opaque-binary stores, plus lossless
mapping requirements. Database clients and the proposed sink API are not included
in Serializer; connect the existing codecs through an application-owned adapter.

For Protobuf binary, ProtoJSON, or TextProto, enable `[cpp] protobuf = true` in
the generator config and use `protobuf_binary`, `protojson`, or `textproto` as the
`serialize_out`/`serialize_in` protocol template. These codecs require no external
Protobuf runtime. See [Protobuf usage and mapping rules](protobuf.md), including
field-number restrictions and Protobuf defaults on input.
ProtoJSON applies the last occurrence of an ordinary field: a nested object
replaces its earlier contents and `null` restores the Protobuf default. This is
different from native JSON and Protobuf binary nested-message merging. TextProto
checks decoded string budgets before growth. Temporary text token/numeric scratch
is outside the allocation budget; also set input, string/token, and work limits.

Serializer compiles a schema into a C++ header, then reads or writes generated
objects through JSON or binary protocols. Use a C++20-or-newer compiler and
standard library; the CMake project requires CMake 3.28 or newer. Header generation
uses clang-format 19+ by default. Applications using existing headers do not need it.
When building this repository's tests/examples with `make all` or `./make.ps1 all`,
install that host tool first; see [formatter setup](cmake_integration.md#formatter-setup).
The Windows `./make.ps1 all` command also builds both editor extension packages,
requiring Node.js 22+, npm and Visual Studio MSBuild. It does not install them;
see [build wrapper options](cmake_integration.md#build-this-repository).
To open this repository in Visual Studio, select a Windows CMake preset and follow
[Visual Studio folder builds](cmake_integration.md#visual-studio-folder-builds)
for compiler environment setup and clearing an older Ninja/platform cache.
If the cached Visual Studio installation was removed or moved, see
[stale instance recovery](cmake_integration.md#recover-a-stale-visual-studio-instance)
for refreshing CMake while reapplying your chosen build options.

The schema compiler enables bounded SIMD scanning on supported x64 builds, with
scalar fallbacks. This is automatic and needs no additional schema keyword; see
[schema-scanner configuration](cmake_integration.md#schema-scanner-configuration)
for the `SERIALIZER_ENABLE_SIMD` source-build option.
The same option also controls [runtime SIMD](runtime_simd.md): JSON string and
whitespace scanning, and endian conversion of C++ binary numeric arrays on input
and output across all key modes. Bulk decoding preserves resource limits and
partial-failure behavior.
Link `Serializer::serializer_lib` even when using pre-generated headers.

The examples below describe the current source API. The
[latest four-toolchain verification](verification-toolchains-2026-10-07.md) records
full default C++ builds and tests on Windows x64 (MSVC and clang-cl) and Ubuntu
WSL2 x64 (GCC and Clang), using Release with a C++20 baseline and managed records,
SIMD, and all compression backends enabled. See the
[earlier verification record](verification-2026-09-17.md) for its generated fixtures,
examples, and configurations. Documentation snippets are not all separate executable
tests. [Qualification](../qualification/README.md) describes how to reproduce and
extend validation.

## 1. Define a schema

The optional [VS Code extension](editor_extension.md) supplies `.serializer`
highlighting and a `schema` snippet with the required version header.
It also navigates includes and type references: both **Go to Declaration** and
**Go to Definition** on an include open the included schema, since an include can
contribute multiple declarations to generated output. On a type reference,
**Go to Definition** opens matching generated output or falls back to the schema
type declaration when no generated definition is available.
**Go to Type Definition** opens the source schema type (or included schema for an
include). All three actions accept declaration keywords and type names, including
forward/reversed `class ledger` selections. Already being at that declaration can
leave the editor on the same line. Field names and primitives have no schema type
destination; generated-language type lookup remains with its language service.
Enum type references inside field defaults, such as `AccountState` in
`AccountState::WaitingForReview`, support both actions.
For an entire schema, right-click its file in Explorer or its editor tab and
choose **Serializer: Go to Implementation** to open existing output in any of the
11 generated languages. Use the built-in declaration menu for qualified names
and whole-name selections. Caller type references require their language's
definition provider; retain the compiler's `--depfile` for precise output ownership.
Navigation never builds or offers generation; see
the [navigation coverage matrix](editor_navigation.md#navigation-coverage-matrix).
In VS Code, the explicit **Serializer: Go to Schema Declaration** command also
follows native type definitions through aliases and variables. Managed ledger
examples/tests require `SERIALIZER_BUILD_MANAGED=ON` in the active CMake build.
Fresh configurations enable it by default; existing caches retain their values. See
[available-file navigation](editor_extension.md#navigate-available-schemas-and-headers).
The separate [Visual Studio extension](../editors/visual_studio/README.md) packages
the shared grammar, custom-type highlighting, and native schema navigation for
Visual Studio 2022/2026 on Windows x64. Go to Declaration opens schemas, and
Go to Definition/Ctrl+click on schema types prefer existing generated output.
Go to Type Definition resolves schema types/includes to their source too.
Generated type declarations in all 11 languages map back to their schemas; caller
references first use the native language service to reach that generated type.
Neither extension is required for compilation or codecs.

Save this as `schemas/person.serializer` in your application:

```text
serializer version 1;

namespace demo {
class person stable_ids {
  public string name (1) { "Unknown" };
  public uint32 age (2) { 0 };
  public array uint32 scores (3);
}
}
```

- Use `class`, `enum`, and `namespace`; schema declarations are not C++ source.
- Start every new `.serializer` file with `serializer version 1.2.0;` before declarations.
  Older `1.0.0` files remain supported.
  The original `serializer version 1;` is an exact alias for `1.0.0`, exclusive to
  version 1. Future versions require three components. The language version is
  independent of the compiler release and application payload revisions.
  See [compiler options and versioning](command_line.md) for the file contract,
  compiler version, and generating C++ and Java together.
- Write an access modifier on every member, followed by its schema type and name.
- `array T` generates `std::vector<T>`; `map(K) T` generates `std::map<K, T>`.
- For new portable byte-keyed maps, prefer `map(uint8)`. Existing `map(char)`
  preserves backend/compiler ordering for high-byte keys, so equal maps can have
  different binary bytes. See [map ordering](wire_format.md#map-ordering).
- Members end with `;`. Classes and enums end with `}` without a trailing `;`.
- Parenthesized metadata assigns a wire name and/or numeric ID. For example,
  `public string name ("fullname", 1);` uses `fullname` in JSON and string-key
  binary. The default C++ profile emits `name`; other profiles can rename the
  C++ member without changing the wire key.
- Braced initializers supply defaults for newly constructed objects. Missing
  keyed fields retain whatever values the destination currently has.
  Quoted defaults may contain literal spaces: `public string label { "schema default" };`.
  Spaces, escaped quotes, and braces within quotes are preserved exactly; a quoted
  `}` does not end the initializer. Unclosed quotes or braces are schema errors.

### Share declarations with includes

Save reusable types in `schemas/common.serializer`:

```text
serializer version 1;
namespace demo {
  class account stable_ids {
    public uint32 id (7);
  }
}
```

Then compile an entry schema such as `schemas/request.serializer`:

```text
serializer version 1;
include common;

namespace demo {
  class request stable_ids {
    public account owner (1);
  }
}
```

- Prefer `include path/to/common;` without quotes or angle brackets. A filename
  with no extension resolves directly to `path/to/common.serializer`; no extensionless
  file or alternative extension is tried. Explicit `include path/to/common.serializer;`
  remains supported. Dotted filenames require the full name, such as
  `include path/to/order.v2.serializer;`. Physical files and CLI/CMake entry paths
  retain `.serializer`. Older compilers require the explicit spelling.
  Use ASCII letters, digits, `_`, `-`, `.`, and forward slashes in a relative path;
  spaces, backslashes, absolute paths, and other extensions are rejected.
  `./` and `../` are supported. Paths resolve from the including file, independently
  of the compiler's working directory. Comments may separate directive tokens.
- Every file requires its own supported version header. `1.0.0`, `1.1.0`, `1.2.0`, and the
  original `1` alias may coexist in an include graph. Typed magic and inferred
  arrays require `1.1.0` in the file that declares them. Includes follow that
  header and precede all declarations, at file scope only.
- Nested dependencies load before their includers. Repeated paths, normalized path
  aliases, mixed shorthand/explicit spellings, and diamond dependencies contribute
  declarations only once per entry compilation. Include cycles and chains deeper
  than 32 files (including the entry file) are errors. Diagnostics identify the
  resolved filename and include chain; depfiles record the actual `.serializer` files.
- Namespace creation reuses the existing logical scope, including equivalent
  `namespace a::b` and nested namespace blocks. Source blocks retain their order.
  A second class or enum with the same qualified name, or a namespace/type name
  conflict, fails during parsing. Equal leaf names in different namespaces are valid.
- Referenced types must still precede their use. Including files cannot provide
  missing types to their dependencies. Including a file does not inject its
  declarations into the caller's namespace or change field IDs or wire names.
- One entry schema produces one combined C++ header or Java compilation unit.
  Java emits one static container per logical namespace. Compile only the entry
  schema for a combined model; generated C++ headers from overlapping entry graphs
  can contain duplicate definitions if included together. Separate generated-file
  imports are not implemented.

Use `serializer_generate(TARGET app SCHEMAS schemas/request.serializer)` or
`serializer_generate_java` as usual. Both helpers track transitive schema dependencies
through compiler depfiles and regenerate when an included file changes. Direct CLI
callers can request `--depfile output.d`. Library callers use
`parser::parse_file(path)`, which returns owning `statements` and canonical
`dependencies`; pass `statements` to either existing writer. The stream-only
`parser::parse` APIs reject includes because they have no source directory.

The [include examples](../example/includes/README.md) contain three runnable pairs
of C++ and Java consumers: shared types, reopened namespaces, and diamond includes.

### When to use `stable_ids`

Use it to make forgetting an explicit ID a generator error. The keyword does not
assign or remember IDs: the numbers in parentheses do that. When adopting it on
an existing schema, first determine the current IDs and write those exact numbers.
Implicit IDs count parents first, then members in declaration order; an explicit
override does not renumber the subsequent declaration-order slots.

For the schema above, `public string country;` would be rejected. Add
`public string country (4);` instead. Keep IDs 1, 2, and 3 unchanged. Each containing
class requires unique parent/member IDs in `1..0x3fffffff`; the generator also
rejects duplicate wire names. Apply the keyword independently to nested/base
classes whose IDs should be explicit.

Reordering declarations with explicit IDs preserves integer-key identity, but
changes positional binary order and can change emitted field order. Older keyed
readers still reject newly added fields. Removing/reusing IDs, changing field
types, and renumbering enum or union alternatives need an application versioning
decision. See the [complete evolution rules](wire_format.md#schema-evolution) and
the [schema reference](schema_reference.md#explicit-field-ids-with-stable_ids).

`view`, `owning`, `readonly`, and `mutable` select generated representations;
see [the view guide](views.md). `inplace` and `simd` are not accepted keywords.
The following build and codec example uses an owning C++ object.

## 2. Generate headers as part of the build

Assume a Serializer checkout is already available at `vendor/Serializer`. Add the
following to your application's `CMakeLists.txt`, adapting the target and paths:

```cmake
cmake_minimum_required(VERSION 3.28)
project(serializer_example LANGUAGES CXX)

set(SERIALIZER_BUILD_TESTS OFF CACHE BOOL "Build Serializer's own tests")
add_subdirectory(vendor/Serializer)

add_executable(serializer_example main.cpp)
serializer_generate(TARGET serializer_example SCHEMAS schemas/person.serializer)
```

The helper ships with Serializer and is also available through an installed
`find_package(Serializer CONFIG REQUIRED)` package. It generates
`build/generated/serializer_example/person.hpp` before compiling the application,
adds its include directory, and links `Serializer::serializer_lib`, which supplies
the C++20 minimum. A newer language mode is preserved. Schema, config, and
generator dependencies drive regeneration; do not edit generated headers manually.
The default formatter must be available as `clang-format` on `PATH`; pass
`CLANG_FORMAT "/path/to/clang-format"` when using another location.

For multiple consumers of one schema, generate it on a shared interface library
and link that target from each consumer. See the [CMake integration guide](cmake_integration.md)
for this pattern, package installation, and the complete helper API.

Building `serializer_generated_headers` generates registered headers and builds
the generator if necessary, without building the consuming application. In VS Code,
use CMake Tools as the C/C++ configuration provider. No custom task is required;
the [IntelliSense guide](intellisense.md) explains the initial generation step.

For another C++ presentation style, add `CONFIG output.ini` to `serializer_generate`.
The helper tracks that file as a dependency. If it selects a custom `format_file`,
pass that YAML file through `DEPENDS` too, or use the helper's `FORMAT_FILE` option
to select and track it directly. See the
[language-specific output configuration](output_configuration.md) and
[examples for all nine profiles](../example/coding_styles/README.md).
The default profile converts names such as `personID` to `person_id` while
retaining their wire spellings. Use `--cpp.naming preserve` when retaining an
existing schema-derived C++ API is required.

Build commands, when validation is being performed:

```sh
cmake -S . -B build
cmake --build build --config Debug
```

You can also invoke an already-built generator directly:

```sh
serializer --input schemas/person.serializer --output person.hpp
```

Use the executable's actual path if it is not on `PATH`. In a cross-compilation
build, generation requires a compiler executable built for the host machine;
pass its path through the helper's `GENERATOR` option.

## 3. Encode and decode an exact message

Save this as `main.cpp` for the CMake example:

```cpp
#include <rohit/serializer.hpp>
#include <person.hpp>

#include <cstddef>
#include <iostream>

// Encode one record, then decode only its written bytes under explicit limits.
int main() {
  demo::person original{};
  original.name = "Ada";
  original.age = 37;
  original.scores = {8, 9, 10};

  constexpr std::size_t initial_capacity_bytes = 256;
  rohit::full_stream_auto_alloc output{initial_capacity_bytes};
  original.serialize_out<rohit::serializer::binary_integer>(output);

  const auto input = rohit::make_constant_full_stream(
      output.begin(), output.current_offset());
  rohit::serializer::decode_limits limits{};
  limits.max_input_bytes = 1024 * 1024;
  limits.max_string_bytes = 4096;
  limits.max_collection_elements = 1024;
  limits.max_nesting_depth = 16;

  demo::person decoded{};
  rohit::serializer::binary_integer<rohit::serializer::serialize_type::in>
      decoder{input, limits};
  decoder.serialize_in(decoded);
  decoder.finish();

  std::cout << decoded.name << ", age " << decoded.age << '\n';
}
```

Use `current_offset()` for the message length, not buffer capacity. Retain the
output storage while the input view is alive, and do not reset, overwrite, or grow
it during decoding. Destination storage must be independent of the input bytes.
For a reused output buffer, call `reset()` before encoding the next message after
all readers of the previous message have finished.

### String-backed output buffers

`rohit::string_stream` in `<rohit/stream.hpp>` owns a `std::string` and satisfies the
contiguous buffer concepts. Generated serialization and concrete low-level codecs
write directly into its resized character storage:

```cpp
#include <rohit/serializer.hpp>
#include <utility>

// Encode formatted JSON and transfer the written string out of the buffer.
template <typename Record>
std::string write_json(const Record& record) {
  rohit::string_stream output;
  rohit::serializer::json_out<true, rohit::string_stream> writer{
      output, rohit::serializer::format::beautify};
  writer.serialize_out(record);
  output.write('\n');
  return std::move(output).str();
}
```

The default constructor starts empty; a byte-count constructor preallocates a
writable extent. Constructing from a `std::string` takes it by value and starts
the cursor after that prefix; move the argument to transfer heap storage.
`reserve(bytes)` ensures that many bytes after the cursor without advancing it.
Growth resizes the string before exposing memory: `std::string::reserve()` alone
does not permit writing past `size()`. The stream's `capacity()` reports the resized
writable extent, which can differ from the string's physical allocation capacity.

`view()` borrows exactly the written prefix, preserving embedded NUL bytes.
`str()` on an lvalue copies that prefix; `std::move(output).str()` trims spare
characters and transfers the owned string, leaving the stream empty and reusable.
String moves may copy small inline buffers. `reset()` retains writable storage for
reuse. Growth, moves, and string extraction invalidate borrowed pointers/views;
internal sources supplied to alias-aware append, text-batch, and transform operations
are rebased automatically. External/unchecked writes retain their existing
non-overlap and reservation preconditions.

For decoding, construct an exact-size input from `view()` and retain the stream
unchanged while that input is alive. Copying the stream and raw malloc/free ownership
transfer are unavailable. Allocation failures propagate from `std::string`; oversized
reservations throw `stream_overflow_exception` before mutation. No additional standard
stream adapter or encoded-message buffer is used, and no measured speedup is claimed.

### Decode one exact message into a fresh value

Use the optional runtime helper when a complete message must validate before its
decoded value reaches the caller:

```cpp
auto decoded = rohit::serializer::deserialize_exact<demo::person,
    rohit::serializer::binary_integer>(input, limits);
```

`deserialize_exact<Value, Protocol>(input[, limits])` value-initializes a fresh
owning `Value`, decodes it, calls `finish()`, and returns it only on success. It
supports generated owning classes, runtime scalar/collection codecs, concrete
input buffers, and standard byte streams. JSON allows trailing whitespace; native
binary rejects any trailing byte. The optional Protobuf codecs work with their
existing message rules. Include `<rohit/serializer.hpp>`; existing generated
headers do not need regeneration for this free function.

Failure destroys the candidate and may consume input. It does not rewind the
source or make a later user-defined assignment atomic. For replacement, keep the
old value until the helper succeeds, then commit using the application's required
assignment/swap policy. Fresh decoding uses additional storage and does not reuse
the old destination's capacity. Limits default to `decode_limits{}`; custom
protocols must accept `(input, limits)` and provide `serialize_in(value)` and
`finish()`.

Bound input to one transport message. Protobuf binary may accept concatenated
valid encodings as a merged message; `finish()` cannot recover missing framing.
Existing `serialize_in`, `serialize_from`, and generated `Type::deserialize`
retain their behavior, including one-value buffer decoding and destination reuse.

### Stream concepts and implicit adapters

Regenerate C++ headers to use any implementation satisfying the public concepts in
`<rohit/stream_concepts.hpp>`. Inheritance from `rohit::stream` is unnecessary.
Generated static `Type::serialize<Protocol>(stream, value)` and
`Type::deserialize<Protocol>(stream[, limits])` calls select their implementation
at compile time, preserving the chosen protocol. The member
`serialize_out<Protocol>(stream)` and `serialize_in<Protocol>(stream[, limits])`
APIs remain available for existing callers and destination reuse.

```cpp
#include <person.hpp>
#include <sstream>
#include <utility>

// Encode into a standard memory stream; its unread contents become one input message.
void round_trip(demo::person& value) {
  std::stringstream message;
  demo::person::serialize<rohit::serializer::json>(message, value);
  auto decoded = demo::person::deserialize<rohit::serializer::json>(message);
  value = std::move(decoded);
}
```

Static `serialize` takes the stream first and a const reference to the value
second; it returns `void`. Static `deserialize` value-initializes an owning
object, decodes it, and returns it by value. Its return is marked `[[nodiscard]]`.
Schema defaults and protocol-specific default behavior are preserved. An exception
prevents a partially decoded object from being returned, but input may already
be consumed. The helper adds no heap-allocated wrapper or extra encoding step.

Both APIs use the existing stream adapters. Omitting `limits` calls the existing
default-input path; passing `decode_limits` selects the explicit-limit path.
They are generated for owning classes, including the owning specialization of
classes that also expose views. Views retain their `map` API and borrowed lifetime.
Regenerate C++ headers to obtain the static helpers.

The same calls accept `std::ifstream`, `std::ofstream`, `std::fstream`, base-class
`std::istream`/`std::ostream` references, and structural custom byte sources/sinks.
For complete programs, see the [iostream example suite](../example/iostream/README.md):
each stream category has its own folder, and all seven examples share a large
52-class schema with multi-batch payloads. It includes separate and bidirectional
memory/file streams, an explicitly buffered file, base references, and a custom
non-seekable stream buffer. Enable `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON` to
build and run the suite without GoogleTest, or use the standard test build.

Standard-stream adaptation supports narrow `char` streams; wide streams do not
provide the required byte interface.
Open binary files with `std::ios::binary`. Stream formatting flags and locale do
not control Serializer's wire encoding. No automatic protocol detection occurs.

| Recognized capability/type | Selected behavior |
| --- | --- |
| `input_buffer` / `output_buffer` | Bind the concrete buffer directly; preserve contiguous scans, scalar batching, alias handling, and allocation policy. |
| `std::istringstream` / `std::stringstream` input | Borrow `view()` from the current read position; avoid an encoded-message copy. |
| `std::ifstream` / `std::fstream` / `rohit::file_stream` input | Read into the final owned message buffer in up to 64 KiB batches. No seeking or regular-file assumption is required. |
| `std::ofstream` / `std::fstream` / `rohit::file_stream` output | Implicit scratch-buffer adapter with an initial 64 KiB capacity; drain completed batches as needed. |
| Other byte sources/sinks, including erased standard-stream references | Generic adapter with 8 KiB read batches or initial output capacity. |

Type recognition uses the static type. Passing a string or file stream as a base
reference selects the generic path and remains supported. Standard memory output
streams use generic batched output: C++20 does not expose their writable storage
through a portable reservation API. These policies do not imply a measured speedup.

Input byte streams contain **one message extending through EOF**, starting at their
current read position. Input is bounded by `decode_limits::max_input_bytes` and
receives `decoder.finish()` validation. Memory streams retain their own bytes;
other streams buffer the whole encoded message before decoding. This is not
incremental parsing. For consecutive framed messages, supply an exact-size input
buffer or a byte source bounded to one frame; no length prefix is inserted or read.

Use the generated overload `destination.serialize_in<Protocol>(input, limits)`
for explicit limits on an existing object, or
`Type::deserialize<Protocol>(input, limits)` to create a new one.
Omitting `limits` preserves the existing default-limit
overload and custom-protocol constructor behavior. Both overloads select the same
implicit adapter from the stream's static type:

```cpp
rohit::serializer::decode_limits limits{};
limits.max_input_bytes = 16 * rohit::serializer::decode_limits::mebibyte;
auto decoded = demo::person::deserialize<rohit::serializer::binary_integer>(stream, limits);
```

`serialize_from<Protocol>(input, destination, limits)` remains available as a
free-function alternative and is the shared implementation behind the generated
member overloads and static factory. Output likewise has
`serialize_to<Protocol>(output, value)`, shared by both output APIs.
Regenerate existing C++ headers to obtain the new two-argument member overload.
All buffer-input convenience calls, including the static factory, retain their
existing behavior: no implicit `finish()` check.
For exact buffer validation, construct the concrete decoder and call `finish()`:

```cpp
// Decode a custom contiguous cursor with explicit resource limits.
template <rohit::type_check::input_buffer Input>
void decode_person(const Input& input, demo::person& value,
                   rohit::serializer::decode_limits limits) {
  rohit::serializer::json<rohit::serializer::serialize_type::in, Input>
      decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}
```

`json`, `binary_integer`, `binary_string`, `binary_none`, `protobuf_binary`,
`protojson`, and `textproto` accept the concrete buffer type as their optional
second template argument. `binary` takes it after `WireEndian`; `json_out` takes
it after the `beautify` argument. Existing protocol names and default buffer types
remain valid. Low-level codecs require buffer concepts; generated calls and the
free functions perform implicit standard-stream adaptation. For formatted JSON,
construct `buffered_output_stream<Output>` explicitly, use it with
`json_out<true, decltype(adapter)>`, and call `adapter.finish()` after encoding.

The concept contracts are:

- `buffer_view`: `curr()` and `remaining_buffer()` expose one valid byte range.
- `input_buffer`: additionally supplies `full()` and a `noexcept`
  `get_curr_and_increase_unchecked(size)` through const access. Checked decoder
  reads advance its cursor without mutating the bytes. Pointers must remain stable
  throughout decoding. Source bytes must be independent of decoded object storage.
- `output_buffer`: supports reservation and `noexcept` unchecked writable-range
  acquisition, raw and external appends, transformed appends, integer formatting,
  and text/byte batch writes. See the concept declaration for the exact expressions.
  A failed reservation leaves its batch unwritten. A successful reservation provides
  a contiguous range valid until the next reservation. Appends preserve the native
  alias/rebasing contract; transform callbacks must be invoked synchronously and
  write exactly their requested output size.
- `byte_input_stream`: standard-style `read`, `gcount`, `peek`, `eof`, `fail`, and
  `bad` operations. `read` fills the requested range or reports EOF/failure;
  `gcount` reports actual bytes read. `peek` must report EOF without consuming data.
- `byte_output_stream`: standard-style `write` and `fail`; write the complete range
  or set failure state/throw. Short writes cannot be silently reported as success.
- `input_stream` and `output_stream` accept either their buffer or byte-stream
  capability. Compatibility `type_check::stream` now means an input/output buffer;
  `write_stream` identifies readable buffer views, including `fixed_buffer`.

Optional capabilities add storage behavior without changing memory-stream requirements:
`durable_output_stream` adds explicit `sync()`, `seekable_stream` adds absolute
`seek(offset_bytes)`, `truncatable_stream` adds `truncate(size_bytes)`, and
`sized_stream` exposes the physical size. Capability checks do not prove durability;
implementations must honor the documented synchronization and failure contract.

Concepts check expressions and types; implementations must also uphold these
lifetime, bounds, aliasing, and failure contracts. Input cursor advancement and
output range acquisition must not throw after the caller has checked/reserved them.

Adapters borrow their streams, leave exception masks unchanged, and do not close
or explicitly flush the underlying stream. Output drains only completed reservations;
a single large string, scalar array, or other batch may grow scratch storage beyond
its initial capacity. Sources aliasing scratch storage postpone draining until the
reservation completes. Output failure may leave an external prefix written; a
failed output adapter cannot be retried. Destructors do not perform fallible I/O.

An initially failed or EOF-marked input is rejected. Successful byte-stream reads
leave the source at EOF; normal EOF is accepted even when EOF/fail exceptions are
enabled. I/O errors throw `std::ios_base::failure`; an oversized encoded input
throws `std::length_error`. Decoder errors keep their existing categories and may
partially modify the destination. A memory-stream size rejection occurs before
consumption; other failures may consume input. Encoded-message staging storage is
separate from decoded-object allocation accounting. Decode a temporary object if
replacement must be transactional.

`parser::parse` also accepts these input capabilities (byte sources use the default
message byte limit), and both `writer::cpp::write` and `writer::java::write` accept
output capabilities. Their `generate` functions return the validated generated
source as a string. Schema includes still require `parser::parse_file`.
Binary views retain stable-span mapping and can copy their positional bytes to
custom buffers or standard output streams; streams do not extend a view's lifetime.

Earlier verification: the regenerated MSVC Debug build passed all 24 CTest
targets, including the core and Protobuf tests and seven iostream examples. Clang 21 on Linux built
the compiler and generated style/include examples and passed a separate smoke test
covering all seven C++ protocols with independent buffers and standard streams.
All seven large-schema iostream examples also passed in a standalone Clang 21
build with GoogleTest disabled.
The subsequent [verification record](verification-2026-09-17.md) includes the full
Linux GoogleTest suite. Performance benchmarks have not been run.

### File streams and journal records

`<rohit/stream.hpp>` continues to provide memory streams. Include
`<rohit/file_stream.hpp>` for `rohit::file_stream`, an owning Windows/POSIX file
stream conforming to the same byte input/output concepts. It works with existing
generated serialization APIs and uses the same implicit 64 KiB batching policy as
concrete standard file streams:

```cpp
#include <rohit/file_stream.hpp>

{
  rohit::file_stream output{"person.bin", rohit::file_open_mode::create};
  demo::person::serialize<rohit::serializer::binary_integer>(output, original);
  output.sync(); // Explicit durable synchronization; serialization itself does not sync.
  rohit::sync_parent_directory("person.bin"); // Persist the new directory entry on POSIX.
}
{
  rohit::file_stream input{"person.bin", rohit::file_open_mode::read};
  auto value = demo::person::deserialize<rohit::serializer::binary_integer>(input);
}
```

`create` is exclusive and rejects an existing file. `update` opens an existing file
without truncation; writes start at the current position. `read` opens read-only.
`lock` acquires an exclusive nonblocking lock on a stable lock file. Keep streams
thread-confined. Destruction closes the handle and releases a lock; it never
implicitly syncs. Use `seek(offset_bytes)` to reposition and reset EOF state,
`size()` to inspect physical length, and `truncate(size_bytes)` followed by `sync()`
to durably resize and position at the new end. `sync_parent_directory(path)` and
`publish_file(source, target, replace)` provide the file lifecycle operations used
by the journal adapter; publish only a synced same-directory replacement, then
sync its parent. Platform/filesystem durability assumptions are described in the
[journal guide](managed/journal.md#durable-decisions-and-files).

`write_stream_bytes` now accepts either an output buffer or byte sink.
`read_stream_some` and `read_stream_exact` read a bounded range without consuming
later records or requiring an EOF-delimited message. The existing whole-message
codec adapter behavior is unchanged.

`<rohit/managed_journal_stream.hpp>` supplies `write_journal_frame(output, parts,
sequence, previous_crc, options)` and `read_journal_frame(input, sequence,
previous_crc, options)`. These use the existing stream concepts, including custom
buffers, `stringstream`, `fstream`, base iostream references, and `file_stream`.
Frame writing returns its checksum and does not flush; all payload parts must stay
valid and unchanged through the call, independently of destination storage. Frame
reading consumes exactly one record, verifies it, and returns an optional
`journal_frame`. `payload()` borrows contiguous input (keep it alive and unchanged)
or owns a bounded payload read from a byte stream. An incomplete final record
returns no value; byte streams consume the partial prefix, while input buffers
remain unadvanced. Corruption, invalid sequencing, excessive lengths, and I/O
failures throw. The existing version-two wire bytes and per-edit sizes are unchanged.
These low-level helpers do not create a managed document container or manage locks,
sequence allocation, or recovery publication; use the managed journal APIs for that.

`durable_output_adapter(output, synchronize)` borrows a supported output stream
and an explicit host synchronization policy. Calling `sync()` first drains
Serializer scratch buffers and flushes an iostream, then invokes the policy.
The policy must make that exact destination durable and throw on failure; it must
not merely reopen a pathname or silently do nothing. The adapter latches failures
and blocks subsequent writes/syncs. Ordinary `std::ofstream::flush()` alone is not
a portable durable-storage guarantee. Memory streams and plain iostreams do not
automatically satisfy `durable_output_stream`, and destructors do not synchronize.
For built-in durable files, prefer `file_stream` directly.

There is no database sink in this release. Add one only with an implemented and
tested Serializer adapter for that database; the database integration guide is
proposal/guidance only.

Earlier verification for the file-stream/journal refactor: all 47 configured MSVC CTest
targets passed, including eight new stream tests, the runnable journal example,
and the journal interruption matrix. GCC compiled the new stream tests with
warnings as errors, and all 120 process-crash recovery checks passed under WSL.
Current full default-suite results are recorded in the
[four-toolchain verification](verification-toolchains-2026-10-07.md).
No throughput benchmark or physical power-cut qualification was performed.

### Choose the protocol

| Protocol | Field identity | Application requirement |
| --- | --- | --- |
| `json` | Wire names | UTF-8 JSON and agreed field names/types |
| `binary_none` | Declaration order | Identical field order and types |
| `binary_integer` | Numeric IDs | Agreed IDs and types; `stable_ids` helps enforce explicit assignments |
| `binary_string` | Wire names | Agreed names and types |

Use the same protocol at both ends. To use JSON in the example, replace both
`binary_integer` occurrences with `json`. For formatted output, construct
`json_out<true>` with a `write_format` and pass it to the object's `serialize_out`.
`format::compress` only removes optional JSON whitespace.

Binary strings require valid UTF-8 in every language, including C++ and its
mapped views. Embedded NULs are preserved. Use `array uint8` for binary payloads;
older C++ raw-byte strings may need the [migration](../migration.md#binary-string-validation).

Binary fixed-width values default to **little-endian**, independent of the host.
Inspect `Protocol::wire_endian`; select the third `binary` template argument only
when your application requires another byte order. Compact prefixes retain their
defined encoding. Messages carry no automatic protocol or endian marker, so both
endpoints must agree on those choices. See [byte order](wire_format.md#byte-order).

### Choose C++ binary text validation

Native C++ binary codecs default to `binary_text_validation::strict`. Validation
adds a bounded, allocation-free UTF-8 scan; long strings use the CPU-selected
[SIMD backend](runtime_simd.md#binary-utf-8-validation). For text already validated
at an application boundary, an explicit policy can remove that scan:

```cpp
namespace codec = rohit::serializer;
template <codec::serialize_type Type>
using trusted_binary = codec::binary_none<
    Type, rohit::stream, codec::binary_text_validation::unchecked>;

value.serialize_out<trusted_binary>(output);
auto decoded = codec::deserialize_exact<my_message, trusted_binary>(input);
```

Replace `my_message` with the generated owning type. The policy is the third
argument of `binary_none`, `binary_integer`, or `binary_string`, after the stream
type; it is the fifth argument of `binary<Type, Keys, Endian, Stream, Policy>`.
`Protocol::text_validation` exposes it. Existing protocol arguments keep their
meaning. Custom buffer and iostream adapters preserve the policy automatically.

`unchecked` removes runtime UTF-8 checks for values, string map keys, and dynamic
field names throughout nested owning objects and collections. Constant generated
field names still validate at compile time, with no per-message cost. Bounds,
length/overflow checks, resource budgets, Boolean/enum/union checks, unknown-key
handling, and `finish()` are unchanged. No runtime policy branch or policy byte
is added. Input/output buffer lifetime and non-aliasing requirements still apply.

This is not a second wire format: valid strings produce identical bytes, and a
strict peer can decode them. Invalid UTF-8 may pass through unchecked C++ codecs
but remains outside the interoperable string contract and can be rejected by
other languages. Use `array uint8` for arbitrary bytes. JSON, generated view
mapping/setters, and other language runtimes remain strict. Rebuild the runtime
library and consumers with matching headers; no schema change or regeneration
is needed. The SIMD build option and text-validation policy are independent.

### Use generated views

Add `view` to generate read-only and mutable views, then optionally restrict them
with `readonly` or `mutable`. Add `owning` to retain an owning representation too.
Keyword order does not matter. A single representation is a plain class; multiple
representations use a `storage_mode` template parameter. Existing owning fields
retain their original API.

Views map little-endian `binary_none` buffers only. A read-only mapping borrows a
`std::span<const std::uint8_t>`; a mutable mapping needs `std::span<std::uint8_t>`.
Use generated `get_field()` and `set_field()` accessors. Setters preserve encoded
sizes, and every borrowed value depends on the buffer's lifetime and stable layout.
Use range-based loops over array/map views for sequential access. Arrays yield
values or nested views; maps yield key/value pairs in wire order. Mutable iterators
provide `set(value)` for array elements and `set_value(value)` for map values,
subject to the same encoded-size restrictions. Iterator copies advance independently.
See [views.md](views.md) for a complete example and collection/nested-type rules.

## 4. Handle limits and failed input

- Each decoder owns cumulative byte, storage, work, and nesting accounting.
  Share it by reference for nested reads and create a fresh decoder per message
  budget. All configured defaults still apply when you override only some limits.
- `finish()` rejects trailing binary data; JSON permits trailing whitespace.
  Generated `object.serialize_in<Protocol>(input[, limits])` calls perform this
  final exact-message check for byte streams; contiguous buffer calls preserve
  their existing behavior without an implicit `finish()`. Omitting `limits`
  uses the defaults.
- Strings, vectors, and maps replace old contents. C++ native codecs can reuse
  nested buffers and map nodes as described below; resource accounting still applies.
- Decoding can leave partial changes on failure. Catch
  `rohit::exception::base_parser` for codec parse errors and inspect `code()`.
  Allocation failures can also propagate as standard C++ exceptions.
- If the existing destination must remain intact on failure, decode and finish
  into a separate temporary object before committing it to application state,
  or use [the exact fresh-value helper](#decode-one-exact-message-into-a-fresh-value).
  Include the temporary's memory cost in application limits.
- Error messages omit automatic payload excerpts by default. Opt-in excerpts
  belong in controlled diagnostics, not ordinary public error responses.

See [the wire-format contract](wire_format.md) for exact binary representations,
Unicode/numeric rules, limit defaults, duplicates, and union restrictions. Use
[migration.md](../migration.md) when adapting older generated headers or callers.

### Reuse destination storage

Keep an owning destination between messages to reuse its allocations. Construct a
fresh decoder over each exact message so budgets restart:

```cpp
std::vector<std::string> names; // Keep this outside the application's receive loop.

// For each message, while its independently owned input bytes remain alive:
const auto input = rohit::make_constant_full_stream(message.data(), message.size());
rohit::serializer::binary_none<rohit::serializer::serialize_type::in> decoder{input};
decoder.serialize_in(names);
decoder.finish();
```

This works automatically for C++ JSON and positional, integer-key, and string-key
binary, including either configured byte order. Existing vector slots can donate
string and nested container storage. Maps can recycle old nodes, replacing their
keys and values only after each complete incoming entry. A donor node need not have
the same old key; map key parsing still uses a fresh key temporary. Completed
incoming map entries never become donors for later entries, so an incomplete
duplicate leaves the last complete value intact.

Regenerate owning headers to reuse fields inside collection elements, including
parents and nested generated classes. Each element is freshly constructed; only
fields actually present in the message take storage from the old element. Missing
keyed fields therefore retain schema defaults inside collections. An ordinary
top-level object update still retains its existing values for missing fields.
Donor selection uses typed generated code with `if constexpr`, without runtime
reflection or field metadata lookup. Keep using the existing one-argument input
methods; the additional generated donor parameter is internal runtime support.
Regenerate LLVM-profile output after the donor naming fix to avoid a local
parameter colliding with its template type. Public hooks and wire bytes are unchanged.

Reuse is limited to supported owning types with nonthrowing move assignment; map
node reuse also requires nonthrowing key assignment. Other/custom types retain
fresh-element decoding. Scalar vectors keep their existing scalar or bulk paths.
Shorter collections destroy unused old entries; decoding an empty collection does
not cache its removed elements. Retaining capacity does not guarantee zero
allocations: growth, default initializers, map bookkeeping, and fresh keys can
allocate. Old donor storage stays alive until consumed or discarded, which can
increase temporary memory usage. Storage and work limits still apply to logical
decoded data, including reused storage; allocation accounting is not a heap limit.

Wire bytes and replacement/failure contracts are unchanged. Input must remain
independent of destination storage. Java and Protobuf/ProtoJSON/TextProto keep their
existing decode paths. Focused reuse/defaults/duplicate/failure/limit tests are
included in the normal C++ test target and passed in the recorded
[verification configurations](verification-2026-09-17.md). Allocation profiling
and performance measurements remain outstanding.

### Batch generated fixed-width fields

Regenerate C++ owning headers to batch consecutive scalar fields automatically.
For example, `uint8 kind`, `uint32 sequence`, and `double reading` occupy 13 bytes
in positional binary. They share one output reservation, and each field is encoded
at its own wire offset. No C++ object layout or padding is copied. The same fields
share an output reservation in integer-key/string-key binary, including their
existing interleaved IDs or names.

Eligible schema fields are `bool`, `char`, fixed-width signed/unsigned integers,
`float`, and `double`. A batch contains 2 through 16 consecutive eligible fields.
Longer runs split into bounded groups; isolated fields use scalar calls. Strings,
arrays, maps, enums, unions, and nested objects end a run. Parent and nested-object
serializers batch their own runs. This bounds generated unrolling and the scalar
snapshots taken before an output reservation can relocate storage.

Positional binary input checks the group's input range and remaining byte/work
budgets, validates Boolean bytes, then loads fields without unaligned typed access
and commits one cursor/accounting update. If the checks fail, the original scalar
reads determine which earlier fields are completed, where the cursor stops, and
which exception is raised. Each value and consumed byte retains its usual work
charge; allocation and nesting behavior are unchanged. Keyed input retains
individual dispatch to accept the existing ordering, missing-field, and duplicate
semantics.

The generated calls detect protocol hooks at compile time using `if constexpr` and
`requires`. JSON and custom protocols without those hooks keep their existing
field calls and framing. Protobuf and Java retain their existing codec paths.
No schema keyword, output option, or SIMD setting is needed. Binary byte order,
wire bytes, and public object input/output calls are unchanged.

Output validates IDs/names and performs one virtual stream reservation before
writing a batch. Custom reservation policies remain authoritative. Rejection
writes none of that batch, while earlier output remains; an output failure may
therefore leave a shorter prefix than individual field writes. A later object
terminator can still fail separately. Source objects and borrowed wire names must
remain valid and independent of the output buffer; input bytes must likewise stay
independent of decoded storage.

`test/fixed_field_batch_test.cpp` covers scalar byte equivalence, golden bytes,
reservation counts, both endians, unaligned input, run boundaries, custom-protocol
fallbacks, resource budgets, Boolean failures, and output rejection. The normal
C++ target generates its fixture; output-profile tests also exercise grouped
fields. See the [verification record](verification-2026-09-17.md) for generation
and test results. Performance measurements remain outstanding; no measured speedup is claimed.

### Pre-encode constant field names

Regenerate C++ owning headers to prepare the encoded form of schema wire names
at compile time. For example:

```text
class account {
  public uint32 user_id ("userID");
}
```

The spelling `userID` never changes between objects. Previously each JSON write
sent it through the ordinary string validator and escape-size scan. String-key
binary wrote its compact length and then its text through separate operations.
The generated output now selects an immutable name token at compile time:

| Protocol | Prepared name bytes | Work performed for each object |
| --- | --- | --- |
| JSON | `"userID"`, including both quote characters | Copy the quoted name; let the formatter write its colon and spacing; encode the current value |
| String-key binary | `06 75 73 65 72 49 44` in hexadecimal | Copy the length and name together; encode the current value |
| Positional/integer-key binary | No name token | Keep the existing positional or numeric-ID output |

The JSON fast path accepts plain ASCII names that need no escaping, covering the
schema's supported identifiers, explicit wire-name aliases, qualified parent
names, and `field:Alternative` union keys. Other constant spellings passed through
the internal helper fall back to ordinary validated JSON string output. Dynamic
strings still receive full UTF-8 validation and escaping. JSON's fixed `key` and
`value` map wrapper names also use constant tokens; map keys themselves remain
ordinary values. Colons, commas, indentation, and object framing keep their
existing formatting rules.

String-key binary tokens contain the canonical one-to-four-byte compact length
and the original name. They work with either scalar endianness: compact lengths
retain their own wire encoding. Within a fixed-width field batch, the token sizes
and scalar widths determine the reservation size at compile time, removing the
per-call name validation, length-prefix encoding, and size accumulation. The
batch still reserves once and writes every field in its original position.

Constant arrays have static lifetime and are shared by matching name token
specializations; serialized objects gain no data members or allocations. The
tradeoff is additional template/constant-evaluation work and read-only program
data, especially when using both named codecs with many unique names. Messages
have exactly the same size and bytes. Expected gains are from avoiding repeated
name processing, particularly for many small records; they have not been measured.

Generated code uses `detail::constant_field_name<Protocol, "name">()`. It selects
the protocol's optional static `encoded_field_name<Name>()` hook with `requires`
and `if constexpr`. Custom protocols without the hook receive the original
`std::string_view`, including in their existing batch calls. A protocol subclass
that needs ordinary names can hide the inherited hook with a deleted template.
Existing generated headers continue using their original name paths; regeneration
enables the generated optimization. No schema keyword, output option, or SIMD
setting is required. Input, views, Java, and Protobuf codecs are unchanged.

Stream reservation policies remain authoritative. An isolated binary name is now
one write: if its reservation fails, neither its length nor its text is written;
previous output survives. Its value and the object terminator can still fail
separately. This can leave a shorter output prefix than the previous split name
writes. Fixed-field batches retain their whole-batch reservation behavior.

Prepared coverage in `test/constant_field_name_test.cpp` includes exact name bytes,
formatted JSON, dynamic escaping, both binary endians, generated parents/unions,
wire aliases, compact-length boundaries, batch and isolated reservation counts,
custom-protocol fallback, and fixed/custom stream failures. Generation and test
results are recorded in [verification](verification-2026-09-17.md); benchmarks remain outstanding.

### Reduce repeated JSON scans

C++ JSON string input/output reuses information from the full validation pass.
The scanner records the first escape and the end of the last escape in addition
to the decoded or encoded byte count. For a string containing a long ordinary
prefix, one newline escape, and a long ordinary suffix:

```text
Validation:  [check prefix][check escape][check suffix]
Copy:        [bulk copy]  [convert]     [bulk copy]
```

Previously, input scanned ordinary strings once to validate them and again while
copying. It now copies validated unescaped input directly into the destination,
reusing its capacity. Plain object keys continue borrowing the validated input;
escaped keys use the same optimized copying into caller-owned scratch storage.

For escaped input and output, known plain prefixes and suffixes are copied without
another character-classification scan. The remaining transformation covers only
the interval between the first and last escapes. Multiple separated escapes
still require scanning ordinary spans inside that interval, and input escapes
are parsed again during conversion. This does not make every string operation
single-pass: validation and copying remain separate, and bytes still have to be
copied to their destination. Unescaped direct JSON output already used a bulk
copy after validation; that path is retained.

The full string is always validated, including malformed UTF-8 or escapes in its
suffix. Input string validation and limit checks finish before the destination
changes. Existing byte, value-work, allocation, and string-length charges remain
unchanged. Output still validates before its single reservation and writes only
after the reservation succeeds. `append_transformed` keeps its source rebasing
and overlap snapshot behavior; saved byte offsets remain valid after rebasing.
Input bytes must remain independent of decoded destination storage as before.

Metadata has fixed size and adds no per-string heap allocation. There is no
message cache, trusted-string mode, schema keyword, or new output configuration.
Recompile consumers with the updated runtime headers when building; generated
headers do not need regeneration for this optimization. Both scalar and SIMD
builds use the same reuse path, with bounded SIMD scanning still available for
the regions that require inspection.

Shared helpers also serve ProtoJSON string input and ProtoJSON/TextProto quoted
output, so those calls inherit the optimization without changing Protobuf
mapping or formatting. TextProto's separate input parser, JSON numbers, native
binary codecs, views, and Java retain their existing algorithms. Pre-encoded
constant field names continue using their separate compile-time path.

Prepared coverage in `test/json_scan_reuse_test.cpp` includes empty/plain strings,
Unicode, surrogate pairs, escape positions around scan-block boundaries, exact
and unaligned buffers, truncated/malformed suffixes, resource limits and cumulative
charges, borrowed keys, destination reuse, overlapping/growing output, and custom
reservation rejection. Existing runtime SIMD tests cover the shared helpers
across JSON formatting modes. See the [verification record](verification-2026-09-17.md)
for unit tests and bounded sanitizer/fuzz coverage. Performance measurements remain
outstanding; no measured speedup is claimed.

### SIMD JSON whitespace scanning

Native JSON and ProtoJSON readers automatically scan long whitespace runs with
the existing SSE2/AVX2 runtime backends. Short gaps and remaining tails stay scalar.
For example, decoding a deeply indented object can skip its long indentation in
blocks before parsing the next field. Spaces inside `"keep  these spaces"` remain
string data and are handled by the string reader.

Only JSON's space, tab, line feed, and carriage return count as whitespace.
Scanning stays within the input and remaining decoder budgets, then charges and
advances once per run. ProtoJSON also bounds scans by the current message. Rebuild
the runtime library and consumers with matching headers; generated schema headers
do not need regeneration. Use the existing `SERIALIZER_ENABLE_SIMD` build option
to choose explicit SIMD or scalar scanning. No schema or application API change
is needed. See [implementation and prepared coverage](runtime_simd.md#simd-json-whitespace-scanning).
See the [verification record](verification-2026-09-17.md) for builds, tests, bounded
sanitizer/fuzz runs, and outstanding platform checks. Benchmarks remain outstanding.

## Agent-assisted integration

Use the [Serializer integration skill](../.agents/skills/serializer-integration/SKILL.md)
to apply this workflow to an existing application. When supplying a Serializer
checkout to an agent, give it that file's path directly; skill installation is
not required for direct use. The [README](../README.md#integrate-with-a-coding-agent)
provides a copyable request, and its [discovery guide](agent_integration.md)
explains automatic selection and use from a consuming project's root.

## Compress complete messages

`SERIALIZER_WITH_ZSTD`, `SERIALIZER_WITH_LZ4`, and `SERIALIZER_WITH_ZLIB` default
to ON when building the runtime, with matching dependency packages required. Set
individual options to OFF for a reduced build. Compression use remains explicit. See [compression contracts and dependencies](compression.md).
For complete programs covering every built-in format, identity output, and a
custom backend, see [compression examples](../example/compression/README.md).
They include generated member/static calls, standard streams, standalone byte
compression, and fresh exact decoding with explicit resource limits.
Regenerate owning C++ headers for the added member/static overloads:

```cpp
namespace codec = rohit::serializer;
namespace compression = codec::compression;

rohit::full_stream_auto_alloc output;
value.serialize_out<codec::binary_integer>(
    output, compression::zstd_options{.level = 3});

const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
const compression::decode_options options{
    .format = compression::format::zstd,
    .max_compressed_bytes = 8 * compression::mebibyte,
    .max_decompressed_bytes = 64 * compression::mebibyte,
    .max_window_bytes = 8 * compression::mebibyte
};
codec::decode_limits limits{};
destination.serialize_in<codec::binary_integer>(input, limits, options);
```

Compression applies once to the entire message and works with all seven C++
owning codecs. Replace `zstd_options` with `lz4_options`, `gzip_options`,
`zlib_options`, `deflate_options`, or `none_options`, and select the matching
input `compression::format`. Native byte order and schema selection remain
separate agreements. `format::compress` still selects compact JSON whitespace.

The matching static and fresh-value alternatives, using independent input streams:

```cpp
record::serialize<codec::binary_integer>(other_output, value, compression::gzip_options{.level = 6});
auto fresh = codec::deserialize_exact<record, codec::binary_integer>(fresh_input, limits, options);
auto another = record::deserialize<codec::binary_integer>(another_input, limits, options);
```

For existing generated headers, use `serialize_to<Protocol>(output, value, options)`
and `serialize_from<Protocol>(input, value, limits, options)` without regeneration.
The free exact helper also needs no regeneration. Output additionally accepts
`compression::encode_limits{.max_input_bytes = ..., .max_output_bytes = ...}`.
Standard streams and custom stream concepts use the existing adapters.

Calls stage bounded whole messages and require one standard frame/member in the
input extent. They reject trailing compressed data, validate checksums when
present, and finish the inner decoder. Frame failures precede field updates;
member decoding can still partially update fields on inner parse errors. The exact
helper returns a fresh value only on success. Input may be consumed on failure.
Views require separately owned decompressed bytes before mapping. See the
[format, memory, and custom-backend contract](compression.md) and
[verification](verification-compression-2026-09-17.md).

## Application command-line declarations

For native tools, use the [typed command-line declaration API](command_line.md#typed-command-line-declarations)
to declare common options, subcommands, defaults and required typed values, then
parse process argc/argv directly. The guide includes full initialization examples
and the ownership, getter, help, and dispatch contracts.

## JSON compatibility

Native JSON remains strict by default. To read a document that may gain fields,
regenerate its classes and select the flexible reader explicitly:

```cpp
auto stream = rohit::make_constant_stream(bytes.data(), bytes.size());
rohit::serializer::json<rohit::serializer::serialize_type::in, rohit::stream,
                       rohit::serializer::read_policy::flexible> reader(stream);
document value{};
reader.serialize_in(value);
reader.finish();
```

Flexible input skips unknown values, including nested arrays and objects. It
rejects duplicate names in both known and unknown objects, including escaped
aliases, and applies the normal input, string, allocation, collection, work, and
nesting limits to skipped values. Missing fields retain their generated defaults;
the application must validate required identities and relationships. Existing
generated classes without the unknown-member hook need regeneration. Other
protocols and default JSON readers retain their existing behavior.

C++ `std::optional<T>` host arguments are supported by native JSON readers and
writers: an empty optional is `null`, and a present optional uses `T`'s codec.
This is useful for generic schema fields without changing existing JSON names.
It does not introduce a schema nullable keyword or native binary optional codec.
