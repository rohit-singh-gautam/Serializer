# Features and implementation status

[Back to the project overview](../README.md)

Use this guide for advanced capabilities and their limits. Implemented behavior
is described separately from broader design proposals.

For an introduction to editing features, begin with
[history, journals, collaboration, and authorization](managed/getting_started.md).

## Serialization and streams

A C++20 schema compiler with C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python,
Swift, Kotlin, and C output supporting JSON and three binary protocols. The compiler and
all generators remain entirely C++; generated portable codecs have no native runtime
dependency. The C++ runtime API and default generated C++ use `snake_case`.

Opt-in C++ codecs also support **Protobuf binary, ProtoJSON, and TextProto** through
compile-time protocol templates, for both encoding and decoding. See [Protobuf
codecs](protobuf.md) for generation, schema mappings, and limitations. ProtoJSON
duplicate ordinary fields replace earlier values, including nested objects and later
`null`; binary Protobuf retains its message-merging rules. TextProto checks decoded
string storage budgets before growing strings. See the [finalization verification
record](verification-finalization-2026-09-18.md).

C++ stream APIs use structural C++20 concepts: custom implementations need no
`rohit::stream` base class. Generated calls accept standard streams directly through
implicit adapters. Memory input streams borrow their unread storage; file streams use
larger I/O batches; custom contiguous buffers retain the direct codec path. Generated
owning classes also provide a static pair: `Type::serialize<Protocol>(stream, value)`
and `Type::deserialize<Protocol>(stream[, limits])`, which returns a new object.

Use `object.serialize_in<Protocol>(stream, limits)` to supply explicit decode limits, or
omit the second argument to retain the defaults. See [stream concepts and
adapters](usage.md#stream-concepts-and-implicit-adapters).

## Message compression

Optional C++ [message compression](compression.md) supports standard Zstandard, LZ4,
gzip, zlib, and raw DEFLATE formats through separately enabled dependencies. Generated
member/static calls accept compression options; free helpers support existing headers.
Calls validate one complete frame with independent input, output, and window limits.
Additional formats can supply a custom backend. See
[examples](usage.md#compress-complete-messages) and the [compression verification
record](verification-compression-2026-09-17.md).

The [compression examples](../example/compression/README.md) provide runnable programs
for all five built-in formats, uncompressed output, and a custom backend. Build enabled
formats with `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON` or the standard test build;
select dependencies separately with `SERIALIZER_WITH_*`. The [iostream
examples](../example/iostream/README.md) provide seven runnable memory, file, buffered,
and custom stream examples with a shared 52-class, 645-field schema. They run in the
standard test build or independently with `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON`.

## Schemas and generated languages

[Schema generics](generics.md) support native C++ templates without concrete schema
uses, nested applications, trailing defaults, and positive uint64 dimensions.
Optional named roots and concrete fields produce contracts for all eleven outputs.
Fixed arrays use std::array and native C++ codecs; other backends and Protobuf
explicitly reject them. Ordinary generic values can be contained by managed C++
roots. Generic managed/view declarations, inheritance, unions, recursive ownership,
user specialization, and non-C++ native generic APIs remain unsupported. See the
[qualification record](verification-dimensions-2026-10-04.md).

Language-specific output profiles select layouts and naming conventions. Schemas use
`.serializer` and begin with `serializer version 1;`. Share declarations with `include
common;` before any declarations. Paths are unquoted and relative to the including file;
`.serializer` is appended when the filename has no extension. Explicit `include
common.serializer;` remains supported. Includes are loaded once per entry schema and
emitted together in its generated output.

Namespace scopes are reused during parsing; duplicate types and namespace/type conflicts
are rejected. See [schema includes](usage.md#share-declarations-with-includes) and the
[paired C++/Java examples](../example/includes/README.md). Quoted defaults preserve
literal spaces, for example `public string label { "schema default" };`; escaping the
space is unnecessary. Run `serializer --version` for compiler version **1.0.0** and
supported schema versions. See [command-line options](command_line.md) for
multi-language generation and overrides.

See [Java output](java.md) for dependency-free Java 17+ codecs and [all
examples](../example/README.md) for self-contained example folders.

## Cross-language managed records

The [managed wire examples](../example/managed/multilanguage/README.md) qualify
history, journal-baseline, collaboration, and opaque-session records across
language codecs. These are record-exchange tests, not ports of the managed runtime;
native managed engines outside C++ remain unimplemented.
Kotlin accepts the contextual property name `field`, used by collaboration records,
while preserving its wire spelling and field ID.

## Database storage

The [database integration guide](database_integration.md)
compares document databases, SQL JSON columns, and opaque binary storage, including
MongoDB, Firebase, PostgreSQL, SQLite, and other targets. It describes required
type mappings and a proposed sink boundary; database adapters are not implemented
or qualified in this repository.

## Implemented C++ managed features

The [C++ managed interface](managed/cpp_runtime.md) is available with
`SERIALIZER_BUILD_MANAGED=ON` and `Serializer::managed`. Bare `managed` declarations
generate direct `persistent_id` fields and typed transaction editors by default. Set
`[managed] separate_values = true` to opt into ID-free values and storage wrappers. New
stores generate document namespaces automatically; object IDs increment from 1 within
each document, and saved identities/counters survive reload.

Start with the [nine small point examples](../example/managed/README.md):
schema-generated managed points, transaction callbacks, generated editors, commit,
undo/redo, and cancellation. Each has its own folder, expected output, and a short
explanation. For the implementation, read [how managed transactions
work](internals/managed_transactions.md): nested types, candidate ownership, callback
forwarding, commit, and cleanup. The companion [managed editor
walkthrough](internals/managed_editors.md) explains channels, target resolution, and
map/array editor lifetimes.

The [ledger example](../example/managed/ledger/README.md) uses
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
examples](managed/cpp_runtime.md#independent-store-features).

### History storage and labels

History currently retains whole-root snapshots. Default linear history uses a deque and
cursor, with no revision IDs or revision lookup. Its `undo()`/`redo()` navigate adjacent
entries, evicting oldest states to meet count/byte limits; tree history retains branches
in a map and rejects commits exceeding those limits. A changed linear commit after undo
discards redo. Labels are disabled by default: use `execute_transaction(callback)` or
`begin_transaction(outcome)` with no name.

Opt in with `model_store<Root, Mode, history_labels::enabled>` to store names and query
undo/redo labels; see the [labeled point
example](../example/managed/labeled_history/README.md). Disabled labels have no string
member or serialized label field. Default saves use format version 3; label-enabled
linear/tree stores retain versions 2/1. Saved histories must match the receiving label
policy; see [migration](../migration.md).

### File streams and journals

`<rohit/file_stream.hpp>` provides an owning file stream for both ordinary generated
serialization and journaling. It uses the existing stream concepts and adds explicit
`sync()`, `seek()`, and `truncate()` capabilities; memory streams in `stream.hpp` keep
their existing contract. Journal frame readers/writers also accept Serializer buffers
and supported iostream adapters. See [file streams and journal
records](usage.md#file-streams-and-journal-records). The [runnable journal
example](../example/managed/journal/README.md) demonstrates both file modes, recovery,
undo/redo, and full Save.

Database sinks require an implemented, tested Serializer database adapter; none is
currently implemented.

Synchronous [journal and crash recovery](managed/journal.md) now supports appended and
sidecar files: `create_journal`, `recover_journal`, and `save_journal` append each
already serialized snapshot once with compact framing, preserve undo/redo and allocated
IDs, and track unsaved values separately from durability. `journal_dirty()` and
`journal_sequence()` expose those independent positions. Uncertain I/O reports
`transaction_status::indeterminate` and blocks writes until recovery.

Schema feature selectors, exclusions, built-in authorization policies, and
other-language managed runtimes remain future work; unsupported syntax/backends fail
explicitly.

The [managed capabilities](managed/capabilities.md) proposal covers history,
collaboration, authorization, and journaling. Managed storage carries a mandatory
persistent ID (`uint32` by default, centrally configurable). ID-free ordinary payloads
are an opt-in representation. [Journal recovery](managed/journal.md) uses a base
snapshot plus an appended or sidecar journal, with durable undo cursors, full Save
replacement, and cleanup that preserves newer unsaved changes.

The full feature set remains a proposal; central persistent-ID configuration and the C++
identity/history runtime, synchronous snapshot journaling, and snapshot collaboration
are implemented.

### Collaboration

The implemented [C++ collaboration runtime](managed/collaboration_runtime.md) provides
`collaboration_authority`, replicas with isolated proposal drafts, atomic snapshot
acceptance, base-sequence conflicts, exact operation retries, advisory presence,
entity/subtree leases, and ordered lock caches. Start with the [collaboration
examples](../example/managed/collaboration/README.md). For immediate local editing,
attach a session with `store.collaborate(session)` to the existing `model_store`, then
use normal generated setters and transactions.

The store journals each commit and retains a durable outbox when a journal is attached.
Call `store.synchronize()` explicitly or `store.synchronize_if_due(now_ms)` from the
owner-thread timer; `send_pending()` and `receive_changes()` are also separate APIs.
Local undo/redo uses the same transaction mechanism and the ordinary `model_store`
journal API. The [store undo example](../example/managed/collaboration/undo.cpp) uses
`execute_transaction`, `undo`, and `redo` with separate journaled client stores;
`synchronize()` handles submission and receiving accepted changes.

Host lock requests reserve IDs with `store.collaboration().reserve_operation_id()` so
they share the store's persisted operation counter without collisions; see the [store
locks example](../example/managed/collaboration/locks.cpp). See the [store integration
guide](managed/local_collaboration.md) and [local synchronization
example](../example/managed/collaboration/local_sync.cpp). Conflicts retain local work
and expose received state separately for resolution.

### Acknowledged-state clients and collaborative undo

The lower-level acknowledged-state API remains available: Use `replica.propose(session,
operation, [](../auto& edit) { ... })` with generated editors; the default authority
accepts these proposals without a handwritten command schema or dispatcher. Custom
server command handlers remain optional. Accepted transactions now include serialized
field-change records with author/operation identity, stable entity/field addresses,
before/after values, contribution versions, and ownership dependencies.

`replica.undo(session, operation)` and `replica.redo(...)` create atomic, conditional
history requests for the same acceptance path. They preserve unrelated edits and reject
same-field or ownership conflicts, including equal-value intervening writes. See the
[history
contract](managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).
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
example](../example/managed/collaboration/sessions.cpp). Generated ownership traversal
checks the actual diff; optional host policy hooks gate changes and locks. Coordination
records originate in `.serializer` schemas. Transport, trusted session binding, and
distributed authority fencing remain host concerns. The [broader
contract](managed/collaboration.md) includes future optimization and distributed
features.

This version retains retry state for its bounded epoch and sends full snapshots. Store
clients merge disjoint fields before submission; same-field/ownership conflicts remain
explicit. Offline IDs use persisted local-to-authority mappings; ordinary application
integer references are not remapped.

## Proposals and future work

The [managed-state design index](managed/README.md) groups the proposals under
`docs/managed/`. The [data-structure design](managed/data_structures.md) defines store
state, revision graphs, snapshot/delta records, checkpoints, and deleted-object
retention, with a [C++ class
walkthrough](managed/data_structures.md#c-class-walkthrough) using actual ordinary
classes generated from a [draft example schema](managed/walkthrough.serializer), with
codec methods omitted. These illustrative records differ from the implemented snapshot
envelope.

The [language-binding design](managed/language_bindings.md) proposes C++
`model_store<Root, support>` (with a `managed` alias) and component-based stores for
other backends. These broader capability sketches differ from the implemented
`model_store<Root, Mode, Labels, Traits>` API, whose history mode is a template
argument. Runtime journaling and collaboration are separate from history policy. Consult
the C++ runtime guide above for the implemented subset; the broader generated APIs
remain proposals.

### Identity and history proposals

The [object identity and transactional history proposal](managed/history.md) describes
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
The broader projection and notification APIs remain proposals; bare `managed` and the
typed C++ editor subset are documented in the runtime guide above. The [domain
examples](managed/managed_examples.md) cover a cylinder with a hole, accounting,
wordpad, and other applications.

Scoped C++ transactions provide automatic commit on successful exit, explicit revert,
and observable completion failures. The implemented C++
[`execute_transaction`](managed/history.md#callback-based-transaction-execution)
convenience API passes a borrowed edit context to one synchronous callback and returns
its completion outcome, using the same RAII transaction engine. All three forms remain
supported: manual commit, automatic completion on normal scope exit, and callback
execution, with explicit revert for cancellation.

Compact nested records can avoid repeating child IDs while preserving persistent
identity mappings. Retention rules distinguish deleted live objects from their
recoverable historical versions; storage budgets and measured editing latency come
before aggressive micro-optimization. The proposed [ownership and allocation
contract](managed/history.md#ownership-and-custom-allocation) keeps managed lifetimes in
the store and permits configurable state/history/scratch resources.

It distinguishes optional internal reference counts from entity IDs and makes payload
allocator propagation and backend limitations explicit.

### Broader managed-state proposal

The companion [managed-state proposal](managed/managed_state.md) explores editable
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
