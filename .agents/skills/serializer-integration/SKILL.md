---
name: serializer-integration
description: "Integrate Serializer into C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python, Swift, Kotlin, or C applications from a provided repository or existing dependency. Use for proprietary-compatible generated output and application runtime, digest byte fields and C++ digest creation, opt-in C++20 constant binary sizing and exact-array emission, positional-only generation and borrowed emission-only models, compact unsigned prefix/varint scalars, .serializer schemas, native C++ generic templates, optional cross-language contracts, dimension parameters, inferred fixed arrays, typed scalar/enum magic, CMake generation, several INI variants from one schema parse, language-specific coding profiles, owning classes or C++ binary views, stable_ids, payload revisions, compiler-only release policies, lifecycle annotations, schema compatibility checks and reservations, stream concepts, durable file streams and iostream adapters, exact fresh-value decoding, managed journal/crash recovery and C++ local collaboration with store-owned sessions, timed synchronization and undo, optional message compression, JSON or binary codecs, database persistence guidance, C++ Protobuf binary/ProtoJSON/TextProto protocols, and schema migration."
---

# Serializer Integration

Serializer — **State Framework** combines multi-language generated codecs and
schema evolution with C++ application-state management. Choose ordinary generated
models for data exchange or the managed runtime for editable C++ models. Native
managed engines outside C++ remain unimplemented; exchanging managed wire records
does not provide those engines. See the
[positioning and scope reference](../../../docs/product_positioning.md).

Implement the user's requested Serializer integration using the version present
in their project. Preserve established wire IDs, names, protocol choices, and
application ownership requirements.

This is the canonical integration entry point linked from the repository's README
and AGENTS.md. Apply it when loaded as a skill or read directly from a supplied
checkout; direct use does not require copying the skill into the consuming project.

## Locate the library and the requested work

Find the application's existing Serializer dependency, schemas, generated-header
build rules, and callers before adding another copy. In this bundled layout,
the Serializer repository root is three directories above this skill. If the
skill has been copied or installed elsewhere, locate the matching checkout from
the application's dependency/build configuration; resolve the following files
there instead of relying on the relative links.

- Start with [README.md](../../../README.md) for the project overview and first-use workflow.
  Follow the [schema reference](../../../docs/schema_reference.md),
  [build and generation guide](../../../docs/build_and_generation.md), and
  [feature status](../../../docs/feature_status.md) for detailed contracts and limitations.
  For a first introduction to history, journals, collaboration, and host authorization,
  read the [managed features overview](../../../docs/managed/getting_started.md) and
  [authorization guide](../../../docs/managed/authorization.md).
  See [agent setup](../../../docs/agent_integration.md) for discovery and use from another project.
- Check the dependency's actual release, build options, installed targets, and
  headers before using source-documentation features. The root `vcpkg.json` is a
  development dependency manifest, separate from the upstream
  `rohit-singh-gautam-serializer` port. Follow
  [distribution guidance](../../../docs/distribution.md) when package availability
  differs; prefer a source build of the needed revision rather than assuming a
  published port contains current managed APIs or generation helpers.
- For compiler/runtime 1.7.0 and newer, follow the
  [licensing contract](../../../docs/licensing.md): generated output permits
  proprietary licensing through `LICENSE-GENERATED`, and application runtime
  headers/compiled helpers and managed support use `LICENSE-RUNTIME` (0BSD).
  Preserve schema copyright/license notices, including those from included files;
  generation does not change their ownership or license. Use `Serializer::runtime`
  for application linkage or `Serializer::managed` when needed. The compiler and
  `Serializer::serializer_lib` parser/generator API remain GPL-3.0-or-later. Check
  third-party dependency licenses separately, and do not apply these permissions
  retroactively to an older installed package.
  Put notices in leading schema comments, before the version header. Compiler
  library integrations should pass the full `parser::parsed_schema` to the
  C++, Java, or portable `generate_schema` function to retain file-level notices even
  when a schema declares no models; the CLI does this automatically.
- Use the [C++ managed interface](../../../docs/managed/cpp_runtime.md) for
  schema-driven identity and local snapshot history. `SERIALIZER_BUILD_MANAGED` defaults ON;
  link `Serializer::managed`, and declare bare `managed` on eligible leaf classes
  and independently identified members. Classes carry `persistent_id` directly by
  default; `[managed] separate_values = true` opts into ID-free values and storage
  wrappers. The compiler generates `model_traits` and typed editors such as
  `transaction.root().entries().edit(id)`.
  It provides manual/scoped/callback transactions, linear/tree snapshots, and
  bounded memory save/load. Select history at compile time with
  `model_store<Root, history_mode::linear|tree|disabled, Labels, Traits>` (choose one enum
  value; linear is the default). Each specialization stores only its selected
  representation. `store_options` has no mode field; `reset_history()` clears
  enabled history without switching modes. Runtime journaling is independent of
  history mode. A fifth template argument independently selects `store_features::none`,
  `journal`, `collaboration`, or `all` (the compatible default). Use `none` with linear/tree
  history for history-only stores; pair disabled history with `journal` or `collaboration`
  for those isolated configurations. Disabled features have no attachment storage/checks
  and their store APIs are unavailable. Identity, validation and transactions remain core.
  The authority's seventh argument and session facade's fifth authority argument accept
  `collaboration` to remove journals. Authentication remains host-owned within collaboration;
  standalone authentication is not a store feature. Built-in authorization policies and
  schema capability selectors remain proposals. See
  [feature selection](../../../docs/managed/cpp_runtime.md#independent-store-features).
  Labels default to `history_labels::disabled`, with no string member or serialized
  label field. Opt in with the third argument `history_labels::enabled`; custom
  traits are the fourth argument. Enabled stores accept named and unnamed
  transactions and provide `undo_label()` plus linear `redo_label()` or tree
  `redo_label(revision)`. Disabled history cannot enable labels.
  Linear history uses `std::deque` entries containing snapshots (optional labels) and a
  cursor, with parameterless `undo()`/`redo()` and no revision IDs or checkout.
  Only tree stores provide `redo(revision)`, `redo_children()`, and `checkout()`.
  Use `Store::outcome_type` for manual generic code; only tree outcomes include
  `revision`. Linear history evicts oldest
  states to meet count/byte limits; `max_revisions = 100` includes the current
  state and allows at most 99 undo steps. A changed commit after undo drops redo;
  failed, canceled, and no-op edits preserve it. Tree mode retains its map and
  rejects over-budget commits. Loads reject excess history without pruning;
  default label-free saves use version 3. Label-enabled linear/tree stores preserve
  versions 2/1. Loads must match history mode and label policy; cross-policy saves
  require explicit migration. Linear saves contain ordered entries and a cursor.
  Use `create_journal(path, journal_storage_mode::appended|sidecar, options)` for a
  new durable baseline, `recover_journal(path, options)` to reopen into an
  unattached store, and `save_journal()` for full Save. Commits, undo/redo/checkout,
  history reset, and ID reservations flush before publication. Both representations,
  ID widths, history modes, and optional labels are supported.
  `journal_dirty()` compares values with the full-Save baseline; memory `save()`
  does not clear it. `journal_sequence()` advances independently of undo position.
  Handle `transaction_status::indeterminate` / `journal_indeterminate_error` by
  destroying the fenced store and recovering into a fresh one before further writes;
  never blindly retry editing callbacks. `load()` cannot bypass an attached journal.
  Each edit writes its already serialized snapshot once with compact length/CRC
  framing (snapshot bytes + 41 bytes for unlabeled linear history, + 33 for
  unlabeled tree/disabled). Navigation/reservations use 33-byte control records.
  Only base creation/full Save encodes the complete envelope and retained history.
  There is no per-edit envelope encoding, byte-diff pass, snapshot output-buffer
  copy, or history deque copy. A retained native handle avoids reopening/seeking
  for each append. Use journal record/file budgets with store decoding limits. See the [journal guide](../../../docs/managed/journal.md)
  for versioned framing, single-writer locking, platform flush assumptions, and
  limitations. Selectors, exclusions, custom allocation, background checkpoints,
  delta journals, built-in authorization policies, and other-language runtimes remain
  future work. Do not present the complete proposals below as shipped behavior.
  Omit the store constructor's document argument for automatic namespace creation;
  explicit namespaces remain supported. Root/managed child IDs are allocated 1, 2,
  3... within each document, not globally. Undo/deletion never renumber or recycle
  consumed IDs; load restores the saved namespace and allocator high-water mark.
  Default clones retain IDs; opt-in separate-values clones remove them. Switching
  representations changes the wire binding and requires migration. Direct managed
  Protobuf output is unsupported. Consult the runtime guide before accessing storage.
- For explanations of the current C++ implementation, use the
  [managed transaction walkthrough](../../../docs/internals/managed_transactions.md).
  It follows the generated point example through store/candidate ownership,
  nested transaction types, the borrowed callback facade, editor forwarding,
  publication, failure, and cleanup. Keep diagrams vertical and distinguish
  implemented behavior from the broader managed proposals.
  The companion [editor walkthrough](../../../docs/internals/managed_editors.md)
  explains channel type erasure, resolver captures, generated getter/setter calls,
  and map/array identity checks. Editors avoid retaining element addresses;
  they resolve the target afresh and reject missing entities or closed transactions.
- For database persistence, read the
  [database integration guide](../../../docs/database_integration.md). Treat its
  backend matrix as candidate storage mappings, not tested Serializer adapters.
  Establish the database product, edition/version, SDK, and JSON/typed-document
  or opaque-byte representation. Preserve integer widths, map entry arrays,
  presence semantics, and metadata boundaries; reject unsupported values or use
  an explicit reversible mapping. Use existing exact-message decoding on reads.
  The proposed sink operations are not public APIs or generator options. Keep
  dependencies optional and report actual provider verification separately from
  codec or mock tests.
- Use the [managed design index](../../../docs/managed/README.md) for the proposals.
  The [capability contract](../../../docs/managed/capabilities.md) now defines
  history, collaboration, authorization, and journal. Every managed
  storage instance has a generated persistent ID, default `uint32`, centrally
  configurable through `[managed] id_type` / `--managed.id_type` (`uint32` or `uint64`).
  Use the same setting for every shared output. Separate ID-free values are opt-in; map keys and
  sidecar indexes cannot replace the managed ID field. Document scope, non-reuse,
  durable allocation reservations, and collision-free replica allocation apply.
  The [journal design](../../../docs/managed/journal.md) separates base/journal
  durability from undo history and defines appended and sidecar storage modes.
  Full Save durably publishes a replacement before retiring covered records;
  preserve newer edits, retained history dependencies, and allocation metadata.
  Changes may remain unsaved in the UI while durably journaled for recovery.
  These storage modes now have the synchronous C++ implementation described above;
  selector syntax and broader delta/checkpoint/distributed contracts remain proposals.
  Active/required authorization cannot
  be bypassed by member selectors. Imported/generated models must agree on ID type.
  The [data structures](../../../docs/managed/data_structures.md) and
  [language bindings](../../../docs/managed/language_bindings.md) describe optional
  store components, `model_store<Root, support>` / `managed` in C++, other-language
  composition, revision graphs, version records, and deleted-entity retention.
  Use the [C++ class walkthrough](../../../docs/managed/data_structures.md#c-class-walkthrough)
  for actual ordinary class excerpts from the [walkthrough schema](../../../docs/managed/walkthrough.serializer),
  with codec functions omitted, and their proposed runtime ownership mapping.
  Preserve generated access sections, qualified field types, initialization, and
  storage metadata; do not present handwritten templates, optional fields, or
  runtime pointers as current schema output. The example uses supported data
  syntax, but its record IDs are illustrative. Opt-in generated C++ companions use
  `managed_<type>_storage` with fixed `persistent_id`/`value` metadata and preserve
  payload field IDs inside `value`. Optimized history formats remain proposals. The snapshot
  runtime uses its own generated versioned envelope, described in the runtime guide.
- Treat [transactional history](../../../docs/managed/history.md) as the broader
  design contract. Bare `managed` and typed C++ setters are implemented;
  `exclude(...)`, `transient`, and selectors remain unimplemented. Generation supports
  public, unpacked owning classes without inheritance, unions, or recursive ownership;
  unsupported shapes and non-C++ managed backends are rejected explicitly.
  Existing `stable_ids` identifies schema fields, not objects.
  Proposed change addresses combine a namespaced entity ID with a relative field-ID
  path; field IDs do not create independent entities or require delta storage.
  Scoped C++ transactions provide auto-commit on successful exit with an explicit
  outcome, rollback on failure/cancellation, and deterministic resource cleanup.
  Prefer the [callback transaction](../../../docs/managed/history.md#callback-based-transaction-execution)
  `execute_transaction(callback)` for a single synchronous action (supply a leading
  label only with `history_labels::enabled`). Pass a
  borrowed edit context, capture external IDs, and return the outcome after scope
  completion. The wrapper owns commit; callback revert/failure prevents it. Reject
  accidental non-void/async C++ callbacks and never retry the callback implicitly.
  Keep `begin_transaction` for caller-controlled lifetimes. Both entry points exist
  in the C++ runtime. Use generated `root()` editors; raw `update(callback)` remains
  a trusted low-level escape hatch and must not leak aliases or rewrite IDs.
  Preserve all three forms: begin with explicit commit, begin with automatic
  completion on healthy normal scope exit, and callback execution. Commit closes
  once; revert cancels. Guard deletion is not a distinct cancellation signal.
  Compact child addressing can omit repeated IDs only while preserving recoverable
  identity bindings; deleted entities can remain in retained history without live
  mutable objects. Storage budgets and measured latency guide optimization.
  The proposed [ownership/allocation contract](../../../docs/managed/history.md#ownership-and-custom-allocation)
  keeps lifetime control in the store while permitting state/history/scratch
  resources. These hooks are unimplemented, do not automatically redirect payload
  container allocations, and require explicit backend support. Internal reference
  counts remain optional and are separate from persistent entity identity.
- Use the [managed examples](../../../docs/managed/managed_examples.md) only for design
  discussion: hollow-cylinder differences, accounting ledgers, wordpad documents,
  and other models illustrate a generic facility, not implemented sample programs.
  Start beginners with the [nine point examples](../../../example/managed/README.md),
  each in its own folder and using a schema-generated managed point: store creation,
  transaction callbacks, generated editors, explicit/scoped commit, undo/redo,
  cancellation, and optional labels. Only `labeled_history` opts into names;
  the first eight and the ledger example use unnamed transactions.
  Use the real runtime throughout; do not replace the generated
  point, editor, or access machinery with handwritten teaching implementations.
  The callback example shows the existing low-level update API; generated editors
  remain the recommended application interface.
  Build all nine with `managed_point_examples`; each README shows expected output.
  The separate [draft ledger example](../../../example/managed/ledger/README.md) is runnable
  using actual `managed` declarations and generated editors without handwritten adapters.
  Use its annotated source and walkthrough to explain document namespaces versus
  object IDs/map keys/field IDs, transaction lifetimes, undo, and memory save/load.
  The [hollow-cylinder](../../../example/managed/design/README.md) and
  [wordpad](../../../example/managed/wordpad/README.md) schema examples have their
  own folders and are compiled in integration tests.
- Treat [managed state](../../../docs/managed/managed_state.md) as a companion proposal.
  Its broader generated `managed` interfaces, scoped authorization, merging, advanced collaboration,
  distributed transactions, and external-effect handling are unimplemented.
  Inferred companion generation is implemented in C++; selectors such as
  `managed(history)` and `managed(all except history)` remain proposed syntax.
  Plain containment does not activate nested managed annotations or infer managed
  support for the containing class; generated capability and occurrence differ.
  Managed targets must qualify through their own class marker or managed members;
  an unmarked leaf target is a compiler error and is never implicitly promoted.
  `exclude(history, collaboration)` is a proposed ordinary-value exclusion that
  preserves serialization and mandatory policy checks; it does not confer identity.
  Authentication and invitation management remain application responsibilities.
  Use the implemented [C++ collaboration runtime](../../../docs/managed/collaboration_runtime.md)
  and [runnable examples](../../../example/managed/collaboration/README.md) for
  actual integration. Include `<rohit/managed_collaboration.hpp>`, link
  `Serializer::managed`, and regenerate model headers for `visit_collaboration`,
  `visit_collaboration_fields`, and `merge_collaboration`.
  `collaboration_authority<Root, Mode, Labels, Traits>` privately owns the store;
  construct it without a command handler for ordinary generated edits.
  For immediate local editing, follow the [store integration guide](../../../docs/managed/local_collaboration.md):
  attach with `store.collaborate(session, options)`, bind a host-authenticated transport,
  and join a fresh placeholder before editing. Keep using normal generated editors
  and all existing `model_store` transaction forms. The store owns its typed session,
  local undo/redo, outbox, receive cursor and identity mappings. Journals flush each
  committed transaction with its client state; synchronization cadence is independent.
  Call `store.synchronize()` explicitly/on reconnect, or drive `synchronize_if_due(now_ms)`
  from the owner-thread event loop. The default interval is 5000 ms; zero is explicit
  only. `send_pending()` sends one transaction; `receive_changes()` only receives.
  Receive accepted changes before sending a successor; retry unresolved delivery
  exactly before pull-only receiving. Bind transports by reference and keep them alive.
  The supplied `collaboration_transport` is in-process; the host supplies network I/O.
  Callbacks are never replayed. Disjoint fields rebase; container membership and order
  remain atomic fields. Conflicts/rejections keep the complete local view, expose
  `acknowledged_read()` separately, and retain drafts in `state()`. After fixing a
  known denial/lease failure use `retry_pending()`; resolve conflicting intentions
  explicitly with retained drafts, `discard_pending()`, and a normal new transaction.
  Lost replies retain exact request bytes; a new epoch marks work uncertain for host
  reconciliation. Never claim durable authority deduplication across epochs.
  `store.undo()` and linear `store.redo()` create local inverse transactions.
  Prefer these store methods and `execute_transaction` for application modifications;
  the [store undo example](../../../example/managed/collaboration/undo.cpp) journals both
  clients through ordinary `create_journal`/`save_journal` calls. `synchronize()` handles
  request submission and accepted changes; applications need no `propose`/`deliver` helper.
  The internal journal adapter pairs model records with session/outbox metadata in
  the existing durable file mechanism, so recovery cannot separate an edit from its
  pending send or acknowledgement. No application-owned store wrapper is needed.
  For host lock requests, call `store.collaboration().reserve_operation_id()` after
  joining. Locks and document submissions share the persisted operation counter;
  never supply an independent counter for the same session. The host must retain
  each lock request for exact retries; reserving an ID does not enqueue that request.
  See the [store locks example](../../../example/managed/collaboration/locks.cpp).
  Tree branch checkout/reset are unavailable while attached; collaborative redo on
  a tree store uses `store.collaboration().undo(true)`. Disabled history retains only
  outstanding synchronization work; it has no undo stacks, inverse archive or history labels.
  Completed client transactions are removed once their accepted sequence is received;
  local transaction numbers remain monotonic. History-free authorities reject inverse
  requests and omit undo stacks, inverse snapshots and contribution-version tables.
  Enabled history labels survive sync/recovery. An attached journal uses the existing
  store journal pointer and file adapter with one model/session wrapper per frame.
  Attach the same session to a fresh store before load/recovery, then synchronize;
  ordinary model readers cannot open client checkpoint format 1. Save preserves
  pending work. One active writer owns each session within an authority epoch.
  Offline persistent IDs are client-local, mapped durably to authority IDs; use
  `remote_id`/`local_id` at server/UI boundaries. Do not remap arbitrary integer
  fields, map keys or application references. Client transactions/state bytes and
  authority command/history budgets are bounded. Enabled histories retain records;
  disabled histories compact completed client work. History-free client checkpoints use
  `serializer.collaboration.pending.v1`; older disabled-history client checkpoints require
  [explicit migration](../../../migration.md#independent-managed-features). History-enabled
  client checkpoints and ordinary model/journal bytes remain unchanged. The store encodes
  each candidate once for authority publication. Non-journal synchronization uses a
  generated-field budget visitor instead of encoding/decoding checkpoints. Unpinned local
  appends without a journal reuse retained buffers; pins and journal callbacks keep
  copy-on-write preparation. Checkpoints borrow their state during encoding. Tree history
  checks incremental retained-byte totals rather than rescanning revisions. See the [local example](../../../example/managed/collaboration/local_sync.cpp).
  The lower-level acknowledged-state API remains available. Use
  `replica.propose(session, operation, callback[, grants])`; the callback runs once
  on an isolated draft using generated transaction editors. No application command
  schema or dispatcher is required. With default uint64 sessions, the authority uses protocol version 6
  with a version-one `model_change` snapshot payload. Custom synchronous host
  command handlers remain optional and use protocol version 5; contexts reject
  mixing the two. Rebuild the managed target for the new coordination record.
  Bind the claimed session to a separate trusted host session.
  Applications own session creation, storage, connection binding, and lifetime.
  Use `collaboration_session<Session, SessionTraits>` and its `authority<Root>`,
  `replica<Root>`, `records`, `result`, and `lock_cache` aliases for custom session
  types. String and unsigned integer policies are supplied; uint64 remains default.
  No registry or numeric mapping is imposed. Custom policies provide a stable
  `wire_name`, `max_encoded_bytes`, `valid`, `less`, and bounded canonical
  `encode`/`decode` functions. IDs must own their data and be default-constructible
  and copyable; equivalent identities must encode identically. Sessions retain their
  type in changes, locks, presence, retries, trusted arguments, and policy hooks.
  Use the collaboration codec helpers for typed records: protocols 7/8 carry a
  generated session envelope around existing command/model records; peers must agree
  on the session policy and codec. Default uint64 endpoints use protocols 5/6.
  Upgrade both peers: protocols 1..4 lack history fields and are rejected.
  Rebuild managed support for the added envelope records. See the
  [session policy contract](../../../docs/managed/collaboration_runtime.md#application-owned-sessions).
  Open an ID once per participation lifetime; after closure use a new ID or a new
  application generation within the ID. Closed IDs cannot be reused in an epoch.
  Ordinary `submit_change` requires the accepted base sequence and a nonzero operation ID;
  exact retries return the retained result, different-payload reuse throws, and
  conflicts require reconciliation plus a new operation ID. Never blindly retry
  callbacks or perform external effects before acceptance.
  `collaboration_replica` receives ordered trusted accepted snapshots; authoring a
  proposal does not change that state or grant edit permission. Draft IDs are
  provisional until acceptance. Resynchronize after a conflict even if the accepted
  sequence is unchanged: rejected creation may consume authority IDs.
  Use `snapshot`/`synchronize` for join and explicit
  recovery, `accepted_since` for delivery/retry, and treat gaps as resync requests.
  `change_lock` manages optional entity/subtree leases; proposals supply current
  `{target, generation}` references. Final generated ownership diffs include
  deletions and moves. Drive `advance_expiry` with host monotonic milliseconds.
  `lock_snapshot`/`locks_since` feed `collaboration_lock_cache`; prune acknowledged
  lock events explicitly. Incomplete caches confer no edit rights. Presence is
  advisory; ended/expired lifetimes require new IDs and cannot be resurrected.
  Host policy hooks gate final changes and lock requests but are not a built-in
  inherited permission engine. Transport, authenticated session binding, policy
  synchronization, and leader/epoch fencing remain host responsibilities.
  Save/load and journal recovery preserve document/history only; recover before
  opening sessions in a fresh fenced epoch. Rights and retry outcomes do not
  survive restart, so uncertain replies across epochs need application reconciliation.
  Configure retry/byte/session/presence/lock budgets. Default proposals carry the
  complete candidate snapshot, bounded by `max_command_bytes` (default 1 MiB);
  `max_created_ids` bounds new-ID reservations (default 4096). The runnable example
  combines the server authority and client replicas in one process. Session IDs
  carry no inherent local/remote/read-only role; the authority enforces write access.
  Retained retries and tombstones
  count for the epoch; no automatic deduplication compaction exists. This version
  sends full snapshots, rejects stale ordinary edits at document scope, and scans active locks for
  overlap/expiry. General merging, deltas, grouped leases, and other-language
  engines in the [broader design](../../../docs/managed/collaboration.md) remain proposals.
  Use `replica.undo(session, operation[, target_operation, grants])` and `redo`
  for conditional per-session history on generated model endpoints. Submit through
  the same trusted authority path and replicate only accepted results. A nonzero
  target must match the stack tip; zero chooses the latest visible tip. The inverse
  comes from retained authority history, not client-supplied values. Every accepted
  batch carries `history`: stable entity/field addresses, before/after presence and
  codec values, active contribution versions, and ownership dependencies. The outer
  session/operation/sequence supplies authorship and transaction grouping. Preserve
  unrelated edits; reject the complete inverse on conflicting field versions or
  ancestry, including ordinary write-away/write-back ABA changes. Structural map
  membership and array order are currently atomic at the containing field level.
  Ordinary nested values remain atomic fields. Recheck current policy and leases;
  failure/no-op preserves history, new local accepted edits clear that session's
  redo, and retries retain the original result. Use `undo_operation`/`redo_operation`
  only as stack inspection, never as permission checks. Version tokens restore
  earlier contributions on undo; accepted sequence always advances.
  Bound retained before snapshots and metadata with `max_retained_bytes`, and field
  projection/version tombstones with `max_tracked_fields` (default 100000). The
  document journal persists undo's result through restoration tag 5, including
  original deleted IDs, but not collaborative stacks or version tombstones across
  authority epochs. Upgrade journal readers for tag 5. Normal transactions cannot
  resurrect retired IDs. Store clients implement immediate local editing/undo and
  durable periodic synchronization as described above; general conflict-free merging
  and peer-to-peer authority ordering remain unimplemented. See the
  [store undo example](../../../example/managed/collaboration/undo.cpp) and
  [history contract](../../../docs/managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).
- Use [docs/usage.md](../../../docs/usage.md) for the schema, CMake, and codec examples.
- Use [docs/portable_languages.md](../../../docs/portable_languages.md) for JS/TypeScript,
  Go, C#, target SDKs, type mappings, limits, and interoperability.
- Use [docs/native_languages.md](../../../docs/native_languages.md) for Rust, Python,
  Swift, Kotlin/JVM, and C owning codecs, SDKs, limits, and ownership.
- Use [docs/java.md](../../../docs/java.md) for pure Java 17+ generation, profiles,
  type mappings, supported schema features, limits, and protocol compatibility.
- Use [docs/cmake_integration.md](../../../docs/cmake_integration.md) for the shipped
  generation helper, source dependencies, installed packages, and shared schemas.
- Use [docs/views.md](../../../docs/views.md) for view generation, mapping, and mutation.
- Use [docs/output_configuration.md](../../../docs/output_configuration.md) for
  language sections, coding profiles, naming, formatter configuration, and examples.
- Use [docs/intellisense.md](../../../docs/intellisense.md) for generated include
  errors, editor configuration, and generating headers without compiling consumers.
- Use [docs/editor_extension.md](../../../docs/editor_extension.md) for the optional
  VS Code extension, local VSIX installation, and CMake header assistance.
- Both editor navigation actions open the included schema when invoked on an
  `include`; a schema include can contribute multiple generated declarations and
  has no single generated definition. Type-reference definitions still target the
  matching generated declaration when available.
- Use [Visual Studio extension](../../../editors/visual_studio/README.md) for the
  separate Visual Studio 2022/2026 x64 navigation package and its build/install steps.
- Read [docs/wire_format.md](../../../docs/wire_format.md) when choosing protocols,
  limits, failure handling, or compatibility behavior.
- Read [migration.md](../../../migration.md) when updating older headers or APIs.

Use local instructions from the repository being edited. When a path or message
contract cannot be determined from the project, ask for that missing information.
Do not turn an integration request into a library redesign or implement a proposed
feature as a prerequisite without the user's request.

## Author or evolve the schema

- Use [schema generics](../../../docs/generics.md) for reusable owning models.
  C++ emits native templates even without concrete schema uses: nested
  `result<T>`, `response<T>`, and `message<T>` can be used as
  `response<std::uint32_t>` with the ordinary generated codecs. Host type arguments
  must satisfy the selected codec and owning operations; do not add handwritten codecs.
  Keep optional `instantiate person_result = result<person>;` or concrete schema
  fields when other languages or compatibility checks need a concrete contract.
  All eleven backends expand those contracts; other languages do not expose open
  native generic APIs. Definitions and argument types must precede use.
- Use positive `uint64 N` parameters, trailing type/value defaults, and
  `array[N] T` or `array[Rows * Cols] T` for fixed owning C++ storage. Defaults may
  reference earlier parameters. Expression arithmetic is checked uint64 with
  decimal literals, parentheses, addition, and multiplication only. Extents are
  1..65,536, with 32 expression levels and 256 parse nodes. Generic parse/expansion
  limits remain 32 levels, 1,024 concrete applications, and 4,096 identity bytes.
  C++ emits std::array with compile-time checks for application-only specializations.
  With language `1.1.0`, use `array[] T field {values};` to infer a fixed extent
  from a nonempty initializer list. Count top-level elements; nested aggregate
  values and quoted commas do not create additional elements. A single char text
  literal uses decoded byte count, including escaped NUL and excluding any
  implicit terminator. Missing/empty defaults or extents above 65,536 reject.
  Preserve the explicit element type. Each declaring dependency needs its own
  `1.1.0` header; the original `1` alias remains exactly `1.0.0`.
  Compiler/runtime 1.6.0 also supports fixed arrays without explicit initializers
  in JavaScript/TypeScript and Python. Their owning arrays/lists start with exactly
  the schema extent and reject short/extra elements during encoding and decoding.
  TypeScript retains ordinary array annotations; the JS codec enforces cardinality.
  Other backends and Protobuf reject fixed arrays explicitly; do not claim mappings.
  Preserve existing runtime decode limits and exact cardinality. Missing keyed
  fields retain defaults; explicitly empty fixed arrays fail. Generic managed/view
  declarations, inheritance, unions, recursive ownership, and user specialization
  are unsupported. Ordinary generic values may occur inside managed roots; use
  generated replacement setters and the existing history/journal APIs.
  Consult [usage](../../../docs/usage.md), [migration](../../../migration.md), and
  [qualification](../../../docs/verification-dimensions-2026-10-04.md).
- Use `.serializer` files beginning with `serializer version 1.3.0;`, before declarations
  (leading comments are allowed). Rename older `.def`/`.struct` inputs and update
  build references. The compiler requires this header; library fragment parsing
  remains available through `parser::parse(input)`. Schema language version 1.3.0 is
  independent of compiler release 1.8.2 and wire protocols. Bug fixes and minor
  updates increment the patch number; new features increment the minor number.
  Minor and patch releases preserve full compatibility. Developers change the
  major number manually, with best-effort compatibility across major releases.
  Installed CMake packages accept earlier minor and patch versions in the same
  major release. Use `serializer --version` to inspect the compiler and schema
  versions; see [CLI and versioning](../../../docs/command_line.md).
  The original `serializer version 1;` is exactly the `1.0.0` language baseline,
  not an alias for the latest language release. Only version 1 has that integer
  shorthand; future versions require all three decimal components. See the
  independent [language versioning policy](../../../docs/command_line.md#schema-language-version-policy).
- Use `class`, `enum`, and `namespace`. Every member needs an explicit access
  modifier and a trailing semicolon. Classes/enums have no trailing semicolon.
- Share schema declarations with `include common;` after the version
  header and before declarations. Paths are unquoted, relative to the including
  file, and use forward slashes; `./` and `../` work, while spaces, backslashes,
  absolute paths, and quoted paths are rejected. A filename without an extension
  resolves directly to `.serializer`; explicit `include common.serializer;` remains
  valid. Use the explicit form for dotted stems such as `order.v2.serializer` or
  when targeting an older compiler. Keep physical files and CLI/CMake entry paths
  suffixed `.serializer`; no extensionless file lookup is performed. Both include
  spellings share the same dependency identity and do not affect wire data.
  Every dependency needs its own version header. Files load once per entry
  compilation; cycles are errors.
- Reopened namespaces reuse their scope during parsing. Duplicate qualified
  classes/enums and namespace/type conflicts are errors; same leaf names in
  different namespaces are valid. Referenced types still precede their use.
- Generate only the entry schema for a combined model. Included declarations are
  emitted into each selected entry output; overlapping C++
  entry graphs can produce duplicate definitions when their headers are included
  together. All generation helpers track transitive inputs using depfiles. Library
  callers use `parser::parse_file(path)` and pass its `statements` to the writers.
  See [include usage](../../../docs/usage.md#share-declarations-with-includes) and
  [paired C++/Java examples](../../../example/includes/README.md).
- In owning representations, map `array T` to `std::vector<T>` and `map(K) T` to `std::map<K, T>`.
- With language `1.3.0` and compiler/runtime `1.8.0`, use `digest` for variable
  application-supplied bytes, `digest[N]` for a decimal fixed byte extent
  (1..65,536), or `digest(algorithm)` for a supported standard output length.
  The bare form implies no algorithm. Preserve existing declared types named
  `digest`, which take precedence for bare references. Require `1.3.0` in each
  declaring included file; keep older compatible headers for unrelated schemas.
  C++ fixed values use `std::array<uint8_t, N>` and start with zero bytes;
  bare storage is an empty `std::vector<uint8_t>`. All eleven native outputs
  support storage/codecs with fixed cardinality checks. Map keys, nonempty digest
  defaults, C++ views, and Protobuf mappings reject explicitly.
  Compute hashes explicitly through `<rohit/digest.hpp>` and
  `rohit::make_digest<rohit::digest_algorithm::sha256>(input)` with
  `Serializer::runtime`; portable applications choose their own provider.
  Native binary keeps the byte-array count, and JSON uses numeric byte arrays.
  Use `digest_to_hex`/`digest_from_hex` for application text conversion only.
  Serializer's creation helpers are 0BSD and introduce no OpenSSL dependency.
  Treat MD5/SHA-1 as legacy algorithms, and preserve the agreed meaning when
  migrating equal-width digest types. Follow [digest usage](../../../docs/digest.md),
  [licensing rationale](../../../docs/licensing.md#why-the-license-was-separated),
  and [migration](../../../migration.md#digest-fields-and-creation-9-october-2026).
- For new portable byte-keyed maps, prefer `map(uint8)`. Existing `map(char)`
  output retains signed Java `byte` ordering and compiler-dependent C++ `char`
  ordering; other backends use unsigned bytes. High-byte keys decode across
  backends but do not promise identical encoded ordering. Preserve existing
  contracts; changing key types is a schema migration, especially for JSON.
  See [map ordering](../../../docs/wire_format.md#map-ordering).
- Keep literal spaces in quoted defaults, such as `public string label { "schema default" };`.
  The parser preserves quoted whitespace, escapes, and braces without rewriting the literal.
  Preserve wire names and defaults. Check the output naming policy before writing
  callers; `naming = preserve` retains existing schema-defined C++ identifiers.
- Put class attributes after the name, before a parent list. `stable_ids` requires
  explicit IDs on every member and parent; it does not assign IDs or compare old
  schemas. For new evolving integer-key contracts, prefer this check.
- When enabling `stable_ids` on an existing class, determine the previously emitted
  IDs first and write those exact numbers. Implicit slots count parents followed
  by members in declaration order; an explicit override does not shift later
  implicit slots. Parent/member IDs share the containing class's ID space.
- Keep IDs unique in `1..0x3fffffff`. Preserve wire names with metadata such as
  `public string name ("fullname", 3);` when renaming a C++ field. Do not reuse old
  IDs/names or renumber enum/union alternatives in an existing contract.
- For revision checks, run `serializer --input current.serializer --check-against
  previous.serializer --compatibility-protocol binary_integer` with the agreed
  native or Protobuf-binary protocol. Select `backward`, `forward`, or `both`
  through `--compatibility-direction` (default both), and retain old include files.
  Maintain `--compatibility-policy policy.json` with retired class field/parent
  IDs and wire names. Exit 2 means a compatibility hazard, 1 invalid input, and 0
  no hazard detected for the selected direction. This is a conservative separate
  checker, not a new `reserved` keyword or proof of application semantics.
  Read [schema evolution](../../../docs/schema_evolution.md) before choosing policy
  scope or interpreting ordinal and unknown-field diagnostics.
- Native keyed readers reject unknown fields by default. For JSON, explicitly select
  `read_policy::flexible` and regenerate classes to skip additive fields.
  Explicit IDs alone do not change input policy. Positional binary still requires
  matching declaration order/types.
  Resolve incompatible changes through the application's versioning contract.
- Check supported types before choosing raw unions: generated alternatives must
  be trivially destructible. Ordinary generated enums use names in JSON/string-key
  fields; binary collection enum elements use numeric values.
- Select only the requested representations: no keywords or `owning` means an
  owning class; `view` means both views; `view readonly` or `view mutable` restricts
  the views. Adding `owning` retains owning storage too. Attributes are unordered;
  `readonly`/`mutable` require `view`. One mode generates a plain class; multiple
  modes require `storage_mode::owning`, `read_only_view`, or `mutable_view`.
- Nested classes/parents must enable each mode their owner needs; map keys require
  read-only nested views. Declare referenced types before use. `packed` cannot be
  combined with `view`. `stable_ids` is independent of mapping, which is positional.
- `inplace` and `simd` are not implemented keywords. Views and little-endian
  defaults are implemented; use the dated
  [verification record](../../../docs/verification-2026-09-17.md) for tested
  revisions and configurations rather than assuming all platforms are qualified.

For a new schema, a minimal explicit-ID example is:

```text
serializer version 1;

class person stable_ids {
  public string name (1);
  public uint32 age (2);
}
```

Adding `public string country;` is rejected; `public string country (3);` assigns
an unused ID. Existing explicit IDs work even without `stable_ids`.

- With language `1.2.0`, use [compact unsigned scalars](../../../docs/compact_integers.md):
  `public compact_prefix strict uint32 value (3) {32};` or `compact_varint uint64`.
  Both keep the host type and ordinary JSON/Protobuf mappings. Prefix has a 30-bit
  maximum; strict checks binary writes, lenient deliberately retains the low 30
  bits without an overflow branch. Varint is full-width unsigned LEB128. Dynamic
  lenient varint values retain the low declared-width bits. All eleven outputs
  support owning unsigned scalars; collections, signed/floating types, enums,
  views, packing, magic and payload versions reject. Defaults must be unsigned
  literals fitting the host type; strict prefix defaults must fit the wire range.
  Each declaring include needs its own `1.2.0` header. Changing the encoding is a
  native binary migration; do not silently compact existing contracts.

## Integrate generated headers

Use the consuming project's existing dependency mechanism. When using CMake,
`add_subdirectory` and installed `find_package(Serializer CONFIG REQUIRED)` expose
`Serializer::runtime`, `Serializer::serializer`, and `serializer_generate`.
Use `serializer_generate(TARGET app SCHEMAS schemas/person.serializer)` after creating the
consumer. It supplies generation dependencies, the generated include directory,
runtime linkage, and the C++20 minimum. CMake 3.28+ is required; keep a newer C++
mode if the application already uses one. Source dependencies build the generator
as needed; installed packages use their installed executable. `SERIALIZER_INSTALL`
controls installation and defaults off when Serializer is embedded.
The generation helpers link the permissively licensed runtime rather than the
GPL compiler API library. Keep `Serializer::serializer_lib` only for deliberate
compiler/parser API use; building or running the GPL generator as a build tool
does not impose its license on application output.

With compiler/runtime 1.5.0 or newer, group several C++ targets needing different
INI configurations of the same schemas with `serializer_generate_variants`:

```cmake
serializer_generate_variants(
  TARGETS models32 models64 models_separate64
  SCHEMAS schemas/model.serializer
  CONFIGS profiles/managed32.ini profiles/managed64.ini profiles/separate64.ini)
```

Create the local targets first. Pair `TARGETS` and `CONFIGS` by order, one INI per
target; optional `OUTPUT_DIRECTORIES` must have that same length. Defaults are
`generated/<target>` in the current build directory. The helper parses each entry
schema once for the whole batch, tracks transitive schema/config inputs, and keeps
the existing generated-header properties and per-target/aggregate generation
targets. Shared helper overrides apply to every config. Keep one consistent model
definition within each consuming binary, especially when changing managed ID
width, separate-values representation, or naming profiles. See the
[variants helper contract](../../../docs/cmake_integration.md#generate-several-configurations-from-one-parse)
and [usage examples](../../../docs/usage.md#generate-several-configurations-from-one-parse).

For this repository's own build, use GNU `make` on Linux and `./make.ps1` on
Windows. Keep their common `configure`, `all`, `test`, `clean`, `rebuild`, and
`extension` commands, defaults, corresponding options, and behavior synchronized, and
verify both when changing either. Both default to Release and four build jobs,
build all enabled CMake targets, and use the `VCPKG_ROOT` toolchain when set.
The test target builds before running CTest; configure stops before building.
Clean verifies the expected repository root and runs `git clean -fdx` from that
root. **Clean and rebuild delete untracked source files and local configuration
along with ignored build caches, dependencies, and editor packages.** They
preserve tracked files, local tracked edits, the index, Git metadata, and nested
Git repositories protected by Git's single-force semantics; never use `git reset`
for cleanup. Build-directory and configuration options do not restrict the
repository-wide cleanup, and external build/toolchain paths are outside its scope.
Clean requires Git, not CMake, and performs no configuration or vcpkg validation.
Verify destructive cleanup only in disposable Git repositories.
Rebuild must complete repository cleanup before configuration, then use CMake's
`--clean-first` before building, including `make -j rebuild`. Reacquire dependencies
removed from inside the repository and reapply nondefault options previously kept
only in the deleted cache.
Normal builds require the same compiler, GoogleTest 1.18.0+, compression libraries,
and clang-format dependencies as direct CMake; Java, benchmarks, and fuzzers remain
opt-in. Use Make's `CMAKE`/`CTEST` or PowerShell's `-CMakeCommand`/`-CTestCommand`
to select executables outside `PATH`. See [wrapper options](../../../docs/cmake_integration.md#build-this-repository)
for configurations, separate build directories, and CMake overrides.
GoogleTest is test-only: the repository manifest requires vcpkg `gtest` 1.18.0 or
newer, and enabled tests use `find_package(GTest 1.18.0 CONFIG REQUIRED)`.
Keep it out of application
runtime dependency requirements; applications link `Serializer::runtime` or
`Serializer::managed` with their enabled compression dependencies.
On Windows, reconfigure and rebuild existing test targets after updating this
checkout. Post-build staging copies their transitive imported shared DLLs,
including shared GoogleTest dependencies, beside the test executable. It derives
paths from CMake targets and selects the current Debug/Release configuration;
static-only dependency lists need no copies. This applies to repository test
executables, with verification status recorded separately in the build guide.
If a cached `GTest_DIR` selects an older package, update the dependency installation
and reconfigure with `cmake -S . -B <build-directory> -U GTest_DIR`, preserving the
build's toolchain and other options, then rebuild the tests. Follow
[test package migration](../../../docs/cmake_integration.md#windows-test-runtime-dlls)
for DLL staging and targeted test commands.
If a cached Visual Studio instance no longer exists, use
`./make.ps1 configure -CMakeArgs '--fresh', '<required -D overrides>'` and reapply
the build's nondefault settings before building again. Inspect/back up the cache
first; do not preserve stale compiler paths or overwrite only the instance entry.
See [cache recovery](../../../docs/cmake_integration.md#recover-a-stale-visual-studio-instance).
On Windows, `./make.ps1 all` and `./make.ps1 rebuild` additionally package both
editor extensions after a successful CMake build. They require Node.js 22+, npm
and Visual Studio MSBuild, restore locked npm dependencies, and reject unequal
extension versions. Packages go to `out/extensions`; the commands do not install
them. Use `./make.ps1 extension` for both packages without CMake configuration,
native builds, or repository cleanup; `./editors/build.ps1` is its packaging script.
WSL `make extension` delegates to that Windows script through `powershell.exe`
after `wslpath` conversion. Require Windows Node.js, npm, and Visual Studio MSBuild;
native Linux cannot build the Visual Studio VSIX and must report that limitation.
Linux `all`, `test`, and `rebuild` build native CMake targets; Windows `configure`
and `test` remain CMake-only. Clean uses Git and removes untracked/ignored extension
packages with the other repository artifacts.

The repository's Windows presets explicitly use Ninja and x64 architecture with
`strategy: external`. Visual Studio supplies the compiler environment; command-line
preset builds require an x64 developer environment and Ninja on `PATH`, plus
`VCPKG_ROOT`. Do not pass `-A x64` to Ninja or change the architecture strategy to
`set`. Clear an older CMake cache before reconfiguring after a generator/platform
change. See [Visual Studio folder builds](../../../docs/cmake_integration.md#visual-studio-folder-builds).

If the default build reports missing clang-format, install a host clang-format
19+ and rerun configuration; vcpkg's GoogleTest dependency does not supply it.
On Ubuntu/Debian with the package available, use `sudo apt install clang-format-19`.
For other installation paths, pass `-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=...`
through `CMAKE_ARGS` or PowerShell's `-CMakeArgs`. Do not disable tests merely to
hide a missing formatter when a full build is requested. See
[formatter setup](../../../docs/cmake_integration.md#formatter-setup).

For dependency or vcpkg CI checks, review the complete
[build requirements](../../../docs/cmake_integration.md#build-this-repository)
and the actual port's options and host dependencies. Package builds should disable
all development targets; they then need no GoogleTest, formatter, JDK, Protobuf
runtime, or sanitizers. Consumer schema generation has separate host-tool needs,
especially when cross-compiling. See
[vcpkg package builds](../../../docs/cmake_integration.md#vcpkg-package-builds)
for configuration, installation checks, and Windows static linkage. Do not infer
other platforms' verification from a successful local package build.

Use a revision with the explicit stream-offset return type, warning-clean pointer
accessors, and native-width identifier hash seed for GCC, Clang, and 32-bit MSVC
builds. Regenerate LLVM-profile headers after the storage-donor naming fix; only
generated local names change, with public APIs and wire data preserved.
Updated runtime headers support `binary_none` encoder `serialize_out(view)`
dispatch for mapped views; keyed/JSON protocols remain unsupported for views.
The [latest four-toolchain verification](../../../docs/verification-toolchains-2026-10-07.md)
records full default C++ builds, CTest suites, and installed consumers on Windows
x64 (MSVC and clang-cl) and Ubuntu WSL2 x64 (GCC and Clang), using Release with a
C++20 baseline and managed records, SIMD, and all compression backends enabled.
All four full CTest suites and installed consumers passed; consult the record for
exact compiler versions and test counts. Earlier Windows x86 package/consumer checks are separate.
Native macOS, Android, and ARM Linux verification remains pending.

Schema scanning and runtime codecs enable SIMD by default through `SERIALIZER_ENABLE_SIMD`.
Supported x64 builds use SSE2 and select isolated AVX2 backends after CPU/OS checks;
short spans and other architectures retain scalar processing. Set this CMake option
to `OFF` before adding the source dependency when explicit SIMD must be disabled.
Installed generators/libraries retain their build-time choice. Runtime helpers
accelerate compact/formatted JSON strings and C++ fixed-width binary numeric arrays
on input and output in every key mode; matching-endian arrays and binary views use bulk copies.
Native JSON and ProtoJSON also scan long whitespace runs with bounded SSE2/AVX2
helpers, retaining scalar short gaps/tails and exactly space, tab, LF, and CR.
Input/work budgets and ProtoJSON message bounds constrain scanning; consumed bytes
are charged once per run with existing failure positions. Rebuild the runtime
library and consumers with matching headers; no schema regeneration is needed.
See [JSON whitespace scanning](../../../docs/runtime_simd.md#simd-json-whitespace-scanning).
Bulk array input validates the complete payload before destination changes and
retains allocation, input, nesting, collection, and work limits. Work exhaustion
uses the scalar path to preserve partial results and failure positions. Boolean
and enum arrays retain per-element validation. Input must not overlap decoded storage.
Keep scalar handling for single values, variable-length values, and view setters.
Link `Serializer::runtime` for pre-generated headers too. Disabling SIMD
retains bulk array reads/writes and direct JSON escaping. See
[runtime SIMD](../../../docs/runtime_simd.md) for scope, aliasing, and limitations.
Wire bytes and schema syntax stay unchanged; do not add a `simd` keyword or promise
a measured speedup. The [verification record](../../../docs/verification-2026-09-17.md)
records boundary tests and bounded SIMD ON/OFF sanitizer/fuzz runs; benchmarks remain outstanding.

Call the helper once per target with all its schemas. For a shared generated API,
use an interface library and link consumers to it so one target owns generation.
Keep outputs in the build tree. Add custom format files selected indirectly by
configs to `DEPENDS`, or use `FORMAT_FILE` to select and track them. Cross builds
need a host-runnable executable passed through `GENERATOR`. Do not hand-edit
generated headers. Consult the CMake guide for options and public-header installation.

For IntelliSense include errors, check both the real generated header's existence
and the source target's include directories. In VS Code, use CMake Tools as the
C/C++ configuration provider; different profile headers share filenames and must
retain per-target include paths. A normal build generates headers automatically.
For generation without compiling consumers, build `<target>_serializer_headers`
or the aggregate `serializer_generated_headers` target. Configuration alone does
not create headers. No custom VS Code task or Serializer editor extension is
required, and `.vscode/*` remains ignored. Editor provider settings may be user-level.
The optional Rohit Serializer extension (`rohitjairajsingh.serializer-language`)
in `editors/vscode` highlights `.serializer`, offers
32Ã—32 language icons for Explorer/editor tabs where the file icon theme permits them,
versioned snippets, and invokes the same targets through CMake Tools. Run the root
`install_extension.ps1` with Node.js 22+, npm, and the VS Code CLI to build and
install it; `-SkipBuild` installs an existing VSIX. This installs the editor
extension only; application dependencies remain managed by the consumer. Extension
version 1.1.29 is shared with the Visual Studio extension and is independent of
compiler and schema versions. Keep both editor extension versions equal.
Both package descriptions and READMEs identify the
[Serializer repository](https://github.com/rohit-singh-gautam/Serializer). Configure
the consumer first, then use `Serializer: Generate Headers` or `Serializer:
Diagnose Missing Header`. Set `serializer.headersTarget` for one consumer; keep
profile include paths separate. Its IntelliSense command explicitly updates the
selected folder's C/C++ provider. Generation saves dirty schema/INI/CMake inputs
in that folder and requires workspace trust. Declaration/definition navigation
and `Serializer: Open Generated Header` only read available files. VS Code Git
index/history tabs use the displayed snapshot for local symbols and offsets;
includes and generated destinations resolve against the current workspace,
not a historical checkout. Never invoke
generation or configuration to satisfy navigation, or offer generation for a
missing destination. Declaration opens the originating schema, while definition
prefers existing generated output in any supported language and falls back to the original schema type
declaration if no generated definition matches. Enum type prefixes in defaults
such as `AccountState::WaitingForReview` participate in both lookups; enum value
navigation is not provided. Follow schema includes to their actual entry
output and use existing `<output>.d` or workspace multi-output `.d` dependencies when present; expose ambiguous
legacy basename/profile matches as choices. An already active CMake model can
narrow lookup but navigation must also work without it and in Restricted Mode.
Use the built-in `Go to Declaration` menu action, including for whole-name selections.
Use `Go to Type Definition` in schemas to reach the source class/enum declaration
or included schema. All three actions accept declaration keywords and forward/reversed
`class ledger` selections. On a declaration itself, declaration/type definition select
that same schema name. Field names and primitives have no schema type destination;
generated-language type lookup remains with the native language provider.
Caller type references in C++, Java, JavaScript/TypeScript, Go, C#, Rust, Python,
Swift, Kotlin, and C use their installed language definition providers.
For reliable ownership of renamed/flattened output, retain the compiler's
`--depfile` output in the workspace; native generators without a banner require
dependency metadata. Use `Serializer: Go to Schema Declaration` from the Command
Palette when other providers add non-schema declaration locations. That explicit
command also follows a native type-definition result for aliases/variables when
ordinary definition stops at their local declaration; native commands retain
their language semantics. For this repository's managed ledger tests/examples,
keep `SERIALIZER_BUILD_MANAGED=ON` in the active CMake configuration. Fresh builds
enable it by default; existing caches retain OFF values until reset or overridden
with `./make.ps1 all -CMakeArgs '-DSERIALIZER_BUILD_MANAGED=ON'`.
Use the same build directory/configuration in CMake Tools as the command-line build;
do not combine include paths from unrelated targets. Do not override
VS Code's global commands or disable other language services.
For file-level navigation, right-click a `.serializer` file in Explorer or its
editor tab and choose `Serializer: Go to Implementation`. It uses the clicked
file's URI and the same available-output lookup without building or prompting.
See [navigation and limits](../../../docs/editor_extension.md#navigate-available-schemas-and-headers).
Semantic schema diagnostics remain unavailable.
For Visual Studio 2022/2026 on Windows x64, use `editors/visual_studio` instead.
Run its `build.ps1` with Visual Studio's MSBuild to package and validate the VSIX,
then install it using Visual Studio's VSIX Installer. It shares the canonical
grammar, custom-type highlighting and navigation resolver. Native Go to Declaration
opens schema includes/types and maps generated declarations in all 11 languages
back to their schemas. Go to Definition/Ctrl+click on schemas prefer existing output.
Go to Type Definition on schemas opens the source type or included schema too.
For repository C# editing, select `editors/visual_studio/serializer_editors.sln`
using the workspace's `dotnet.defaultSolution`; do not load temporary project copies
from `out/`. The extension/test projects declare identical Windows runtime targets
for Visual Studio and C# language-server restores.
From caller code, use its language service to reach the generated declaration first.
The VS package does not supply VS Code's CMake commands or snippets; Node.js is
needed to build the shared bundle, but is not required at runtime.
Continue to use the consumer's CMake targets for generation and include paths.
See the [verification record](../../../docs/editor_extension.md#verification-performed)
for tested editor hosts and remaining verification boundaries.
These targets still build the generator when needed and perform real generation:
honor any instruction to defer configuration, generation, or builds. Install/package
consumption has been smoke-tested on Windows for compiler 0.1.0, including versioned
package discovery and both generation helpers. The extension's Windows VS Code
host smoke test covers header generation, opening, and build failure handling;
live C/C++ IntelliSense reparsing and other editor platforms remain unverified.

Keep output settings in a generator INI config, with `[output] language = cpp`
and a `[cpp]` section. Java uses its own section as described below. C++ `coding_standard` values
are `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, and
`qt`. These are presentation profiles, not whole-guide compliance guarantees.
See the [profile examples](../../../example/coding_styles/README.md).

Default `naming = profile` renames schema-derived C++ types, fields, enum values,
union helpers, and view accessors. Wire names/IDs and runtime-required hooks keep
their original spelling. Resolve generated naming collisions; do not silently
change wire names to fix a C++ naming problem. Enum defaults referring to declared
values are translated; opaque C++ default expressions require `naming = preserve`
or an explicitly chosen literal/default change.

Formatting requires clang-format 19+ at generation time. Repository test/example
builds detect a suitable host tool through CMake's program search and, on Windows,
standard LLVM and all Visual Studio installations reported by `vswhere`. Discovery
runs again for a fresh build cache; installing LLVM or Visual Studio's C++ Clang
tools is still required if no suitable formatter exists. For reproducibility, pin its version and pass
`--cpp.clang_format` or set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` for this repository's
CMake rules; the shipped helper also accepts `CLANG_FORMAT`. `format_file` can
replace layout rules. Config paths are relative to
the config; CLI paths are relative to the working directory. CLI overrides take
precedence over config values. Add configs and custom format files to generation
dependencies. Use `format = false` only when another pipeline formats the emitted
source. The test build includes all profile examples; with tests disabled,
`SERIALIZER_BUILD_STYLE_EXAMPLES=ON` enables them independently.

The CLI uses short/long options (`-i`/`--input`, `-o`/`--output`, `-c`/`--config`,
`-l`/`--language`); bare argument names are no longer supported:

```sh
serializer --input schemas/person.serializer --output person.hpp
```

Repeat `--language cpp --language java` or use `--language cpp,java` to generate
both backends. Supply `--cpp.output person.hpp --java.output PersonSchema.java`
instead of `--output` for multiple languages. Prefix backend overrides with `--`,
such as `--cpp.coding_standard google --java.coding_standard oracle` and
`--java.package app.models`. All backend config settings have CLI overrides.
`--java.package=` and `--cpp.format_file=` clear configured optional values.

For several configurations from one schema parse, keep separate existing INIs
and repeat `--config` with corresponding output paths:

```sh
serializer --input schemas/model.serializer \
  --config profiles/managed32.ini --config profiles/managed64.ini \
  --config profiles/separate64.ini \
  --output out/managed32/model.hpp --output out/managed64/model.hpp \
  --output out/separate64/model.hpp --depfile out/model.d
```

Create destination directories first. Each config starts from defaults; configs
do not overlay one another. Scalar CLI overrides apply to all configurations,
and explicit CLI languages replace every config's selection. Generic outputs
require one selected language per config and exactly one path per config in
config order. For multiple languages per config, repeat language-specific output
options once per config selecting that language, also in config order. Never mix
generic and language-specific outputs. The shared depfile lists all outputs and
configuration/schema inputs. All backends finish generation/formatting before
writes start; parse/validation/backend failures preserve existing outputs, while
filesystem writes across files are not atomic. Batching preserves INI syntax and
each selected schema contract; one INI with named configurations is unsupported. See
[batch CLI details](../../../docs/command_line.md#generate-several-configurations-from-one-parse).

Use the actual built executable path when it is not on `PATH`. Regenerate through
the real build pipeline when execution is part of the task; preserve an explicit
user instruction to defer generation/builds/tests and report what remains unverified.

## Integrate pure Java output

- Select `[output] language = java`, with `[java] coding_standard = serializer`,
  `google`, or `oracle`. All use conventional Java type/field/enum naming; Oracle
  uses four-space indentation and the other two use two spaces. They are limited
  presentation profiles, not full compliance tools. `naming = preserve` keeps
  valid schema identifiers. `package` optionally selects a Java package.
- Run `serializer --input account.serializer --output AccountSchema.java --config java.ini`.
  The output filename supplies the public outer class. Schema namespaces become
  static nested namespace containers. Each generated file includes its own codec
  helpers and needs only Java 17+, with no JNI, reflection, or third-party runtime.
- Compile with `javac --release 17 -encoding UTF-8 -d classes AccountSchema.java`.
  For CMake source/installed dependencies, `serializer_generate_java(TARGET name
  SCHEMA file OUTPUT Schema.java CONFIG java.ini)` creates a generation target;
  `serializer_generated_java` builds all registered Java outputs. Attach Java
  compilation in the consumer. Track custom dependencies with `DEPENDS` and use
  a host `GENERATOR` for cross builds. Java examples are opt-in through
  `SERIALIZER_BUILD_JAVA_EXAMPLES`; every example has its own input folder.
- Generate owning schemas only. Reject view/packed requests rather than promising
  Java buffer views. Parents use `base0`, `base1`, ... composition, not subtyping.
  Arrays use Java arrays; maps use ordered maps and box primitive keys/values.
  Unsigned scalars retain their bits in Java signed primitives. Preserve generated
  map ordering. Object/floating-point keys and opaque C++ defaults are unsupported.
- Call `value.encode(Schema.Protocol.BINARY_INTEGER)` and
  `Schema.Namespace.Type.decode(bytes, protocol[, limits])`. All four protocols
  are implemented; decode validates an exact message before returning a fresh
  object. JSON uses UTF-8, and binary scalars are little-endian. Java strings
  reject invalid UTF-8 byte sequences, matching C++ binary codecs and all other
  backends. Use `array uint8` for arbitrary payload bytes. See the Java guide
  before mapping unusual defaults or union payloads.
- Use the generated `Limits` to bound message bytes, string bytes, cumulative
  collection entries, and nesting. Default JSON readers reject unknown fields;
  opt into `json<serialize_type::in, Stream, read_policy::flexible>` for
  evolving documents. Compatible readers skip additional fields, reject duplicate
  names (including escaped aliases), and preserve decode limits. Regenerate classes
  to enable this protocol hook. Native JSON also supports C++ `std::optional<T>`
  generic arguments as null or typed values; this does not add a schema type or
  binary optional encoding. See `docs/usage.md#json-compatibility`.
  Malformed values fail with
  `IllegalArgumentException`. Do not share mutable input/object storage across
  concurrent codec calls. No explicit SIMD or native acceleration is implemented.
- Keep Java runtime/build verification distinct from C++ source-only status notes.
  With tests and Java examples enabled, CTest exercises compiled Java and two-way
  interoperability, including exact binary bytes. Do not claim benchmark results.

## Integrate JavaScript, Go, or C# output

- Keep the `.serializer` compiler and generators in C++. Target SDKs execute or
  compile generated consumers only. The tracked `.inc` files contain embedded
  runtime source and must ship with compiler source distributions.
- Select `js`, `go`, or `csharp`; add `typescript` for JS declarations. Use
  language-specific output paths for multi-language generation. Pair `.mjs` with
  `.d.mts`, or ES-module `.js` with `.d.ts`, using identical JS naming settings.
- Read [portable language usage](../../../docs/portable_languages.md) before
  choosing native types. JS 64-bit fields require `bigint`, Go constructors apply
  schema defaults, and C# uses an output-file outer class. Namespace flattening
  must remain collision-free. Preserve wire names and IDs across all targets.
- Generate one combined schema per Go package. Use includes for shared types.
  Native maps are sorted during output. Use owning schemas with portable literal
  defaults, valid UTF-8 strings, supported map keys, and ASCII JSON characters.
  Views, packed storage, arbitrary C++ expressions, native acceleration, Protobuf,
  compression APIs, and configurable output-buffer reuse are not implemented.
- Use exact-message decode and explicit limits for untrusted input. Generated
  decoders return a fresh object only after validation; Go returns nil on failure.
  Missing fields retain defaults, nested duplicates merge, and collections replace.
  Do not route JS messages through ordinary JSON.parse/stringify because full-width
  integers and duplicate-field semantics require generated codecs.
- Integrate with `serializer_generate_source(TARGET name SCHEMA file LANGUAGE go
  OUTPUT generated/schema.go OPTIONS --go.package app)` or invoke the C++ compiler
  directly from npm, Go, or MSBuild build steps. The CMake helper tracks includes;
  supply a host `GENERATOR` when cross-compiling.
- Enable `SERIALIZER_BUILD_INTEROP_EXAMPLES=ON` to compile the
  [shared interoperability example](../../../example/interoperability/README.md).
  Test every producer/consumer pair, all four protocols, and all union variants;
  compare complete values and native binary bytes. Run malformed-input tests and
  relevant TypeScript/browser checks when changing target runtimes.
- Measure before making speed or allocation claims. The examples expose local
  throughput checks and Go has an allocation benchmark. Preserve direct field
  codecs, pre-encoded keys, checked capacity reservation, switch dispatch, and
  exact wire bytes while optimizing. Report untested platforms explicitly.

## Implement the C++ codec calls

For compile-time positional sizing or emission, use compiler/runtime 1.4.0 or
newer and read the [constant-evaluation guide](../../../docs/constant_evaluation.md)
and [usage example](../../../docs/usage.md#count-and-emit-constant-positional-bytes).
Include `<rohit/constant_binary.hpp>`. Enable generated owning models with
`[cpp] constant_evaluation = true` or `--cpp.constant_evaluation true`;
the default is false. Regenerate through the normal schema/CMake pipeline and keep
one consistent enabled definition per qualified model across translation units.

Use `binary_none_size(value)` for an explicit validated count without output
allocation and `serialize_binary_none_to(span<uint8_t>, value)` for existing-memory
output. For ordinary owning values the latter uses the original optimized encoder
at runtime, without an implicit sizing pass. Use `make_binary_none_bytes<Factory>()` when the byte-array
extent must depend on variable transient fields; the factory constructs the value
twice and must be deterministic. Do not persist heap-owning constexpr model objects
or return views into destroyed local storage. Source and output memory must be
independent. Decode with the ordinary matching positional reader and exact framing.

The generator adds `serialize_constant_out`, a static marker and exact-type
alias. It leaves regular runtime `serialize_out` unchanged. Do not replace
runtime memcpy/memmove, batching or SIMD paths to achieve constant output. The
independent `[cpp] protocols = binary_none` / `--cpp.protocols binary_none`
selector removes JSON/keyed output; its default is all. It rejects requested Protobuf,
managed and view combinations. Retain all when those runtime capabilities are needed.
Maps, managed operations, views, compression, I/O and nonconstant customizations are
outside the C++20 constant subset. Raw unions require legal initialized active members.

For allocation-free construction, optionally select `emission_only = true`; it
requires constant_evaluation true, protocols binary_none and Protobuf false. Emission
DTOs borrow string_view/span fields under a terminal emission namespace and expose
only non-template `serialize_out(binary_none_output&)`. Construct inline owners
whose output method creates temporary views while buffers stay alive; avoid retaining
views into a copied/returned owner. Use the concrete writer for both count and supplied
memory. Generated emission headers include the lean constant_binary_output.hpp
support without the full runtime serializer/I/O/JSON parser closure; include
constant_binary.hpp when using the owning runtime-memory overload. Generate an
ordinary owning header separately for runtime reading. Maps,
managed/views, nonempty array defaults and multidimensional arrays are rejected.
Native generic type arguments retain their supplied C++ representation. Emission
writes fields logically, so copyability is not a wire requirement; generated headers
do not repeat field-level copyability assertions. Serializer tests verify supported
borrowed representations. Preserve the separate raw-union destruction constraints.

Verify actual generated models, independent expected bytes and runtime reading;
report the tested compiler/library combination and unavailable cases. Do not infer
all-toolchain support, zero compilation cost or exhaustive performance qualification
from one probe. The guide distinguishes implemented behavior from remaining regression
and measurement targets.

- For optional compression, read [the compression contract](../../../docs/compression.md)
  and [usage examples](../../../docs/usage.md#compress-complete-messages). All three
  `SERIALIZER_WITH_ZSTD`, `SERIALIZER_WITH_LZ4`, and `SERIALIZER_WITH_ZLIB` backends
  default ON, and the repository manifest acquires their libraries by default.
  Disable unused backends explicitly for a reduced build. Select typed compression
  options at the whole-message boundary;
  keep inner protocol/endian/schema agreements explicit. Regenerate owning headers
  for member/static overloads, or use free helpers with existing generated headers.
  Input requires an explicit compression format and exactly one frame/member.
  Bound compressed input, expanded output, and backend window independently of
  object decode limits. Calls stage whole messages and finish the inner decoder;
  member parse errors can still partially update fields. Use the fresh exact
  helper for replacement semantics. Views must map stable decompressed storage.
  Java compression wrappers, dictionaries, concatenated streams, and unbundled
  algorithms are not provided. Custom `compression::backend` adapters can extend
  format support without generator changes. Refer to the separate
  [compression verification record](../../../docs/verification-compression-2026-09-17.md).
  Start from the [runnable compression examples](../../../example/compression/README.md)
  for each built-in format, identity output, or the custom-backend interface.
  Set `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON` and `SERIALIZER_BUILD_TESTS=OFF`
  to build them without GoogleTest, enable the matching `SERIALIZER_WITH_*`
  dependencies, and run CTest with `-L serializer_compression_examples`. Regular
  test builds include the enabled examples; disabled backends have no example target.

- For Protobuf binary, ProtoJSON, or TextProto, read
  [the Protobuf guide](../../../docs/protobuf.md). Enable `[cpp] protobuf = true`
  or `--cpp.protobuf true`, regenerate, and select `protobuf_binary`, `protojson`,
  or `textproto` with the existing compile-time protocol-template calls. No
  external Protobuf runtime or `.proto` export is required. These codecs currently
  support C++ owning objects only; Java retains its four established protocols.
  Match the documented schema mapping with the peer, including union wrapper
  messages, field-number limits, map keys, and the absence of a `bytes` schema type.
  Input uses Protobuf defaults and commits a replacement object only after exact
  decoding succeeds. Unknown binary fields are skipped and discarded; unknown
  named fields/enums fail. Do not promise preservation of field/union presence,
  unknown fields, well-known-type mappings, or arbitrary Protobuf schemas.
  ProtoJSON duplicate ordinary fields replace prior values rather than merging;
  a later `null` resets the field to its Protobuf default. Binary messages still
  merge. Regenerate Protobuf-enabled C++ headers for corrected union-wrapper
  handling. TextProto checks decoded string storage before growth; temporary
  token/numeric scratch is outside `max_allocation_bytes`, so also bound input,
  string/token length, and work. See
  [finalization verification](../../../docs/verification-finalization-2026-09-18.md).

- Select `json`, `binary_none`, `binary_integer`, or `binary_string` according to
  the agreed message contract. Do not silently change protocols to improve size
  or speed. `format::compress` means JSON whitespace suppression.
- Binary fixed-width scalars default to little-endian. Inspect `Protocol::wire_endian`;
  an explicit third `binary` template argument selects another byte order when
  required. Compact prefixes keep their own encoding. No endian/protocol marker,
  auto-detection, or compatibility aliases are provided; agree on the format with
  the peer. The schema-language version identifies neither message byte order nor
  wire version. Independent legacy/current fixtures cover explicit selection in
  all three native binary modes. JSON is unaffected by numeric byte order.
- Keep wire byte order independent of host CPU byte order. The shared profile is
  little-endian on both little- and big-endian machines; only C++ currently offers
  explicit big-endian wire selection. Use the interoperability runner's
  `--big-endian` option for QEMU s390x C/C++ participants and shared frozen
  positional bytes. See [qualification commands](../../../example/interoperability/README.md).
- Require valid UTF-8 for binary strings in every language, including C++ owning
  codecs, mapped views, and mutable string setters. Preserve embedded NULs and
  do not normalize Unicode. Use `array uint8` for arbitrary bytes; migrate older
  C++ raw strings using the [migration guide](../../../migration.md#binary-string-validation).
  Preserve leading U+FEFF as string data too. Regenerate Swift output for strict,
  platform-independent UTF-8 decoding and Rust/C output for corrected large
  floating defaults; see [finalization fixes](../../../migration.md#finalization-correctness-fixes).
- Keep C++ binary text validation strict by default. Only for text whose validity
  is guaranteed elsewhere, select `binary_text_validation::unchecked` as the
  third `binary_none`/`binary_integer`/`binary_string` argument (after `Stream`),
  or the fifth `binary` argument. This compile-time policy propagates through
  nested owning values and stream rebinding and removes runtime text scans only.
  Bounds, limits, Boolean/enum checks, and exact-message checks remain active.
  Invalid UTF-8 is still not interoperable; peers may remain strict when text is
  valid. JSON, generated view mapping/setters, and other runtimes have no new
  opt-out. See [usage](../../../docs/usage.md#choose-c-binary-text-validation).
- Rebuild the runtime and consumers for the dedicated binary UTF-8 SIMD backend;
  schema regeneration is not needed. AVX2 validates complete Unicode sequences,
  with bounded SSE2/word ASCII and scalar Unicode fallback. The existing
  `SERIALIZER_ENABLE_SIMD` setting controls explicit vector acceleration, not
  whether validation is enabled. Consult [UTF-8 verification](../../../docs/verification-utf8-2026-09-18.md)
  for measurements and test scope; do not promise zero strict-validation overhead.
- Regenerate C++ headers for structural stream concepts and implicit standard-stream
  adapters. No `rohit::stream` inheritance is required: use `input_buffer` or
  `output_buffer` for the contiguous fast path, or standard-style byte-stream
  concepts for external I/O. Generated calls accept standard streams directly.
  Memory input (`istringstream`/`stringstream`) borrows the unread suffix; known
  file streams use 64 KiB batches and erased/custom byte streams use 8 KiB batches.
  Standard memory output uses generic buffered writes. No measured speedup is claimed.
- For complete stream-specific consumers, use the seven
  [iostream example folders](../../../example/iostream/README.md). They share a
  52-class, 645-field schema and verify large messages through memory, file,
  explicitly buffered, erased, and custom non-seekable streams. Enable
  `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON` to build them independently without
  GoogleTest, then run CTest with `-L serializer_iostream`. The standard test build
  includes them too. Keep their concrete stream types where specialization matters.
- Regenerate owning C++ classes to use matching static entrypoints:
  `Type::serialize<Protocol>(stream, value)` writes a const borrowed value and
  returns void; `Type::deserialize<Protocol>(stream[, limits])` returns a newly
  initialized owning value marked `[[nodiscard]]`. Defaults and stream adaptation
  match the existing member APIs. Failures throw without returning a partial
  object, but may consume input. Keep `serialize_in` for destination storage reuse.
  The static factory retains buffer-input semantics without an implicit `finish()`;
  byte streams still receive exact-message validation. Views retain `map`.
  Both new names are reserved in owning class scope; preserve wire names and IDs
  with metadata when resolving schema member collisions.
- Byte-stream input means one bounded EOF-delimited message and includes `finish()`
  validation; it is not incremental parsing and adds no wire framing. Use a bounded
  source or exact-size buffer for framed traffic. Regenerated owning classes expose
  `destination.serialize_in<Protocol>(input, limits)` for explicit limits; the
  one-argument overload retains default behavior. The free
  `serialize_from<Protocol>(input, destination, limits)` remains an alternative.
  Low-level codec templates accept a concrete buffer type; existing buffer calls
  remain valid. Keep memory-stream
  storage stable while decoding. Never imply that concepts prove lifetime or
  alias safety. See [stream contracts](../../../docs/usage.md#stream-concepts-and-implicit-adapters).
- Use `<rohit/file_stream.hpp>` for a reusable owning file stream supporting both
  generated serialization and journals. `file_stream` satisfies existing byte-stream
  concepts and adds `sync()`, absolute `seek()`, `truncate()`, and `size()`.
  `file_open_mode::create` is exclusive; `update` preserves existing file bytes;
  `read` is read-only; `lock` owns a stable exclusive writer lock. Keep it
  thread-confined. Serialization and destruction never implicitly sync.
  Memory streams in `stream.hpp` remain memory-only implementations.
  `managed_journal_stream.hpp` provides bounded `write_journal_frame` /
  `read_journal_frame` helpers accepting native/custom buffers, iostreams, and
  file streams. Buffer reads borrow payloads; byte-stream reads own bounded
  payloads. Preserve input lifetimes and use the managed path APIs when a full
  document container, locking, sequence allocation, and recovery are required.
  Optional persistence concepts do not change ordinary stream requirements.
  `durable_output_adapter` requires a real host synchronization policy for the
  exact borrowed destination; plain iostream flushing is insufficient. Never
  claim a memory stream is persistent merely because framing succeeds.
  The managed store owns a journal sink; its implemented file adapter uses these
  stream facilities. Database sinks require an implemented and tested Serializer
  database adapter. None currently exists; do not add speculative database support.
  See [file streams](../../../docs/usage.md#file-streams-and-journal-records) and the
  [runnable journal example](../../../example/managed/journal/README.md).
- Open binary files in binary mode. Adapters borrow streams, retain exception masks,
  and leave explicit flushing/closing to the caller. I/O failures can consume input
  or write an output prefix; decode errors may partially update destinations.
  Encoded staging storage is separate from decoder allocation accounting. Custom
  protocols retain their constructors unless they explicitly provide compatible
  `stream_type` and `rebind_stream` hooks; inherited hooks do not replace derived
  protocol behavior. Schema parser/writer entrypoints use the same stream concepts.
- Encode into an appropriate stream. Reuse `full_stream_auto_alloc` capacity when
  useful, resetting only after readers of the previous message have finished.
- Use `rohit::string_stream` from `<rohit/stream.hpp>` for direct string-backed
  output, including `json_out<true, rohit::string_stream>` for formatted JSON.
  `view()` borrows only written bytes; lvalue `str()` copies them, while
  `std::move(output).str()` transfers the trimmed string and empties the stream.
  Growth resizes writable characters and rebases internal aliases; borrowed
  pointers/views must not survive growth, moves, extraction, or buffer reuse.
  Construct from a byte count for an initial writable extent, or move a string
  to continue after its existing prefix. No schema regeneration is needed for
  already concept-enabled headers. See
  [string streams](../../../docs/usage.md#string-backed-output-buffers).
- Construct input views with the actual message length, such as
  `make_constant_full_stream(output.begin(), output.current_offset())`, never the
  spare allocation capacity. Keep input storage alive and independent of output
  destinations or any mutation that could invalidate it.
- For explicit limits and whole-message validation, construct the input protocol
  with `(input, decode_limits)`, call `decoder.serialize_in(destination)`, and then
  `decoder.finish()`. Convenience object input calls omit the final exact-message
  check for contiguous buffers; byte streams always receive that check.
  Share decoder sessions by reference; they are noncopyable.
- Choose limits from the application's message sizes and workload. Storage/work
  accounting is cumulative per decoder and is not an exact process-memory limit.
- Strings/vectors/maps replace contents. Missing keyed fields retain destination
  values; duplicate fields apply in order and maps keep the last complete duplicate
  entry. Decode failure can leave partial updates and consumed input.
- Reuse an owning destination across messages when useful: native C++ JSON and all
  three binary key modes recycle eligible nested buffers and map nodes automatically.
  Regenerate owning headers for typed storage donation inside generated collection
  elements and parents; no schema option or explicit donor argument is needed.
  Incoming elements still start from fresh schema defaults, including missing
  keyed fields. Shorter/empty collections discard removed elements; custom or
  throwing-assignment types retain fresh decoding. Reused storage remains subject
  to logical resource accounting and may increase temporary retained memory.
  Java and Protobuf codecs keep their existing replacement paths. Consult
  [destination reuse](../../../docs/usage.md#reuse-destination-storage) for scope
  and limitations; see the [verification record](../../../docs/verification-2026-09-17.md)
  for generation and focused test results. Performance measurements remain outstanding.
- When the application requires atomic replacement, decode/finish a temporary
  object before committing it. Keep structured parse error handling at the message
  boundary and leave payload excerpts disabled unless the task needs them.
- Use the optional `deserialize_exact<Value, Protocol>(input[, limits])` free
  function for a fresh value returned only after `finish()` succeeds. It supports
  native and optional Protobuf codecs, concrete buffers, and standard byte sources;
  no schema regeneration is needed. Input can be consumed on failure, a later
  assignment can still throw, and fresh decoding does not reuse the previous
  destination. Bound input to one message: Protobuf binary can merge valid
  concatenated encodings. Keep existing buffer convenience APIs when one-value
  decoding or destination reuse is intended. See [exact decoding](../../../docs/usage.md#decode-one-exact-message-into-a-fresh-value).
- Regenerate owning headers for automatic fixed-width field batching: native
  binary output groups 2 through 16 consecutive scalar fields per reservation;
  positional binary input shares range/budget checks and falls back to scalar
  reads on a failed check or invalid Boolean. Keyed input keeps individual
  dispatch. Fields retain their wire order, IDs/names, endian, and work charges.
  Variable-length fields, enums, unions, parents, and objects bound runs; nested
  serializers group their own fields. JSON/custom protocols without batch hooks,
  Java, and Protobuf retain existing paths. No schema option or SIMD setting is
  required. Failed output reservations leave the current batch unwritten, with
  previous output intact. See [field batching](../../../docs/usage.md#batch-generated-fixed-width-fields)
  for custom-stream policies and tests. See the
  [verification record](../../../docs/verification-2026-09-17.md) for results;
  performance measurements remain outstanding.

- Regenerate owning headers for pre-encoded constant field names in native C++
  JSON and string-key binary output, including fixed-width batches. Schema wire
  spellings and bytes stay unchanged; JSON formatting and dynamic-string validation
  remain active. No schema option is needed. Custom protocols without the static
  `encoded_field_name<Name>()` hook receive ordinary `std::string_view` names.
  Constant arrays add compiler work/read-only data, not per-object storage; no
  measured speedup is claimed. Isolated binary names reserve their length and text
  together, leaving both unwritten on rejection; values may fail separately.
  Input, views, Java, and Protobuf keep their existing paths. See
  [constant field names](../../../docs/usage.md#pre-encode-constant-field-names)
  for details and tests, and the [verification record](../../../docs/verification-2026-09-17.md)
  for results. Benchmarks remain outstanding.

- Use the updated runtime headers for JSON string scan reuse; schema regeneration
  and new options are unnecessary. Validated plain strings copy directly, while
  escaped input/output reuse boundaries to skip rescanning plain prefixes/suffixes.
  Full validation, resource charges, destination reuse, and stream alias/failure
  behavior remain unchanged. Input still must not overlap decoded storage. Shared
  ProtoJSON input and ProtoJSON/TextProto quoted output benefit; separate parsers
  and other codecs retain their existing algorithms. See
  [JSON scan reuse](../../../docs/usage.md#reduce-repeated-json-scans) for remaining
  passes and coverage. Consult the [verification record](../../../docs/verification-2026-09-17.md)
  for unit-test and bounded sanitizer/fuzz results; do not claim measured performance gains.

## Map generated views

Use `View::map(span, limits)` with the exact little-endian `binary_none` message.
Mapping validates the full message and builds inline field offsets. Getters return
scalars, borrowed strings, or nested views. Mutable mappings require writable
spans; read-only mappings never expose setters. Buffer owners must outlive every
view and must not relocate or change the layout while views remain live.

Use setters only for existing scalar/string fields or collection elements with
unchanged encoded sizes. Do not promise insertion, resizing, union switching,
or JSON/keyed/big-endian mapping. Map keys remain read-only; map entries retain
wire order and duplicates. Prefer range-based loops or `begin()`/`end()` for sequential
array/map access: iterators cache entry boundaries, while variable-width indexed
access traverses preceding entries each time. Arrays yield values or nested views;
maps yield key/value pairs. Use iterator `set(value)` for mutable scalar/string array
elements and `set_value(value)` for mutable map values; dereference returns a value,
not a writable scalar reference. Nested object values expose their existing setters.
Copies advance independently and retain their own span/limits, so temporary collection
wrappers are safe while the underlying buffer remains alive and unchanged in layout.
Nested field access still constructs a validated view. Refer to the view guide for
parent accessors, union access, and complete examples.

## Verify and document the result

For cross-language managed work, use the
[managed wire matrix](../../../example/managed/multilanguage/README.md) to qualify
the canonical history, journal-baseline, collaboration and session record codecs.
It runs every selected producer/consumer pair and rejects truncated/trailing input.
Do not confuse passing record tests with native managed feature support: this
matrix does not implement non-C++ undo/redo, durable journals, collaboration engines,
or authentication. Non-C++ managed declarations remain explicitly rejected.
Regenerate Kotlin records with this compiler when a schema contains a property
named `field`; that contextual name is supported without renaming its wire key.

Use the task's permitted validation scope. For an integration, exercise a generated
record through the selected protocol with an exact-size input, destination reuse,
and relevant malformed/limited input. For schema migration, include the relevant
old/new schema behavior. Consult [qualification](../../../qualification/README.md)
for library-wide tests, fuzzing, or benchmarks only when that work is in scope.

Use the dated [four-toolchain verification](../../../docs/verification-toolchains-2026-10-07.md)
for current default C++ build and test results, and the
[earlier verification record](../../../docs/verification-2026-09-17.md) for its
sanitizer, fuzzing, and interoperability checks. Keep each record's source revision,
configuration, and outstanding-check boundaries when describing support; historical
failures and earlier source-only notes do not describe the current tested revision.

For fuzz qualification, enable `SERIALIZER_BUILD_FUZZERS=ON` with a Clang GNU-style
driver providing libFuzzer, ASan, and UBSan. It builds an isolated instrumented
runtime, including SIMD translation units, without changing the normal library or
generator. Build the four fuzz executables and `fuzz_corpus_generator`, then run
`ctest --test-dir <build> -L serializer_fuzz --output-on-failure` to generate and
replay deterministic seeds. Follow [the fuzzing workflow](../../../qualification/README.md#fuzzing)
for control-byte encoding, separate mutation/artifact directories, fixed seeds,
and SIMD ON/OFF runs. Seed replay is not a mutation campaign or a security claim.

Report files changed, how to generate/build/use the result, and what was actually
validated. When editing Serializer itself, keep its README and this skill current
together with affected linked guides. When editing a consumer, update that
application's relevant usage docs without modifying an upstream checkout merely
to restate an unchanged library contract. Keep public examples specific to
Serializer and public sources.

## Integrate Rust, Python, Swift, Kotlin, or C output

- Keep schema compilation in the C++ executable. Select `rust`, `python`, `swift`,
  `kotlin`, or `c`; use corresponding `.rs`, `.py`, `.swift`, `.kt`, or `.h` outputs.
  Select all desired targets in one invocation. `.inc` files are tracked runtime
  source templates embedded by C++, never temporary output or separate compilers.
- Apply [native API and SDK guidance](../../../docs/native_languages.md). These
  are owning codecs for the four original wire protocols, with portable literal
  defaults; C++ views, packing, compression, and Protobuf are separate features.
- Use `Default` in Rust and constructors in Python/Swift/Kotlin. Preserve full
  unsigned widths and Swift `WireString` map-key identity. Python underscores
  express nonpublic fields by convention; C structs cannot enforce access control.
- Initialize or zero C destinations and buffers; free owned strings, collections,
  and models using generated lifecycle functions. Never shallow-copy owning C
  models. Decode replaces the destination only after exact-message success.
- Use explicit decode limits at input boundaries. Repeated objects merge,
  collections replace, and the last complete duplicate map value wins. Do not
  claim graceful allocation recovery where the target runtime cannot provide it.
- Every language has `basic`, `collections`, `complex`, and `interoperability`
  folders under `example/<language>`. Use [the example runner](../../../example/README.md)
  to generate from shared schemas and validate all producer/consumer pairs.
  TypeScript is a typed JS consumer, not another wire format or runtime.
- Enable `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` only when all selected SDKs are
  available. Missing SDKs must be reported, not silently skipped. CTest compiles
  those consumers and runs the shared boundary suites; the ordinary compiler
  build requires no target SDK. Consult the [dated verification record](../../../docs/verification-native-2026-09-18.md).

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
[declaration guide](../../../docs/command_line.md#typed-command-line-declarations) for complete examples, getter types, ownership,
error behavior, and positional/tail syntax. This source utility is not an
installed runtime header.


## Payload revisions and schema evolution

For file identification, use [magic and format omissions](../../../docs/magic_and_omission.md).
`private magic (99) { 'SRLFILE' };` defines static immutable header bytes, not
mutable payload storage. Native binary prefixes them before the version; JSON
includes the fixed `magic` string. Every included format verifies and discards
the header and rejects missing, wrong, or duplicate magic. Freeze the numeric
magic ID for durable Protobuf contracts. Default IDs use the first free identity.
C++ byte magic is emitted as readable character literals with a comment showing
its escaped contents. Preserve the exact array extent and bytes; do not replace
the brace list with a string literal that adds a NUL terminator.
Use trailing `omit(json, binary_positional)` on magic or any ordinary member of an
owning class to exclude that field from the selected protocol. View modes reject
omissions explicitly. Version discriminators cannot
omit formats. Omitted fields keep schema defaults on fresh decode; keyed input
must reject explicitly supplied excluded fields. Canonical format names are
`json`, `binary_none`, `binary_integer`, `binary_string`, `protobuf`, `protojson`,
and `textproto`; `binary_positional` and `protobuf_binary` are accepted aliases.
C++ also exposes `binary_positional` as an unchanged alias of `binary_none`.
Magic requires an unmanaged owning class. Legacy byte literals require valid UTF-8.
With schema language `1.1.0`, use `private magic uint32 (99) {42};` or a
declared enum value. Supported types are `char`, `bool`, signed/unsigned
integer widths, finite `float`/`double`, and enums; strings, classes, collections,
type parameters and payload-version component types reject. Typed char values
128..255 require `omit(json)`; ProtoJSON/TextProto retain numeric char mappings.
Each declaring file
needs its own `1.1.0` header. Emit and validate the immutable typed constant
using the selected codec's scalar/enum mapping, with no per-object storage.
Native binary prefixes it without an extra magic key, field ID or collection
count: enum values use compact ordinals in positional/integer-key binary and
length-prefixed names in string-key binary. JSON carries the scalar value or
enum name; Protobuf uses the corresponding scalar/enum field mapping. Retain
legacy byte-magic declarations unchanged unless migration is explicitly requested.
Regenerate all relevant language and editor outputs when changing these contracts.

Consult the [versioning verification record](../../../docs/verification-versioning-2026-10-06.md) and [release-policy verification record](../../../docs/verification-release-policies-2026-10-06.md) for completed language, compiler, codec, and editor checks. Release catalogs and nested allow policies are evaluated entirely during schema compilation; generated readers and writers contain only resolved version bounds.

Use [payload versioning](../../../docs/versioning.md) and the [all-language examples](../../../example/README.md#versioning) for `version`, `compatibility`, `created`, `obsolete`, `replaced`, and class-scoped `reserve` syntax. Distinguish the supported schema language header (`1.3.0`, older `1.2.0`/`1.1.0`/`1.0.0`, or the original `1` alias for `1.0.0`) from a class's payload discriminator. Freeze durable discriminator identities explicitly when the default first-free ID could change.

Keep retained historical definitions and relative positional order intact. Set the object's revision to write an older supported layout. Use `read_policy::compatible` for declared history, `strict` for the current revision, and `flexible` to skip safe JSON extensions within the declared interval. The former `json_read_policy::compatible` spelling is removed; its unknown-field behavior is `read_policy::flexible`. Native generated languages select `ReadPolicy` through `Limits`; C uses `srl_read_policy`.

Choose uint8/16/32/64, finite nonnegative float/double, or version2/3/4 with canonical bounded uint16 components. Dotted revisions are C++ value types and strings in other native APIs. Conversion for `replaced` is explicit application code. Do not infer forward support for unknown revisions or automatic decoding of previously unversioned bytes. Versioned unmanaged owning models support all eleven languages and all four native codecs; managed models, views, and Protobuf mappings currently reject this feature. Verify the consuming application's historical bytes and the relevant language examples.

### Release-date and count policies

Follow [release policies](../../../docs/versioning.md#release-dates-and-compile-time-policies)
for `releases`, `policy`, nested `any` / `all`, `max_age`, `keep_last`, `released_since`,
`expires_on`, and compatibility leaves. These are allow conditions, not deny lists.
Require ordered release identities and dates with a dated current revision. Preserve
retained historical definitions; release entries do not supply missing decoder layouts.

Pin `--version-policy-as-of YYYY-MM-DD` in reproducible generation and compatibility
checks; otherwise the schema compiler captures UTC today once per invocation.
All three CMake helpers accept `VERSION_POLICY_AS_OF` and the flag
`VERSION_POLICY_WARNINGS_AS_ERRORS`. An incremental build does not rerun generation
just because the date changes. Already-built readers retain the compiled floor.

Inside a tree, compatibility is a normal leaf; outside it, compatibility overrides the
final tree. Date/count leaves need a catalog; compatibility-only trees do not. The
runtime accepts the folded version interval rather than a catalog whitelist. The
current version remains readable, with generation warnings for exceeded time leaves;
`--version-policy-warnings-as-errors` promotes these before output publication.

Library callers may pass `parser::parse_options` to `parse_file(path, options)` or
`parse(input, require_header, options)` and supply synchronous diagnostic callbacks.
Use `parser::version_policy_reference_date()` when multiple calls must share a UTC day.
Do not add clocks, dates, release arrays, or policy objects to application codecs.
