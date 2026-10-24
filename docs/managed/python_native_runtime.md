# Python native managed engine

The native Python engine runtime is implemented in
[`python_managed_engine_runtime.inc`](../../src/python_managed_engine_runtime.inc),
with generated schema descriptors and independent codec/metadata adapter tests.
Windows qualification passes **25 runtime tests** and **281 actual generated SDK
checks across four identity profiles**. `MG01` remains open for the complete
ten-output contract; this qualifies the restricted Python profile below without
establishing parity for the other native engines. It is separate from Python
managed token-only codecs and from C++'s native storage format.

Enable the native engine explicitly; ordinary Python generation retains its
existing acceptance and managed-declaration rejection rules:

```ini
[python]
managed_engine = true
[managed]
id_type = uint32
id_encoding = compact_varint
```

The CLI equivalent is `--python.managed_engine true`. Both boolean options use
normal configuration/CLI precedence. Native engine mode includes the checked
typed-token codecs, so it does not require `managed_token_codecs = true` as well.
The common managed identity policy selects `uint32` or `uint64` and `fixed` or
`compact_varint`. No schema syntax or existing wire revision changes.

The generator emits `managed_schema(root_type, **options)`, which binds actual
generated record types, logical managed field boundaries, class tags and native
codecs. Using the generated names from the test fixture, the entry pattern is:

```python
schema = model.managed_schema(model.EngineGraphModel)
store = model.ManagedStore(schema, model.EngineGraphModel(), document=(17, 18))
pin = store.read()
store.transact(lambda edit: setattr(edit.root, "title", "accepted"))
store.close()
assert pin.root.title == ""
```

The current generated profile supports concrete owning records and enums,
ordinary nested values, direct managed array/map occurrences, optional/fixed
containers and typed references. It rejects inherited classes, transient,
wire-only, version-filtered, lifecycle, omitted and variant fields, behavior,
indexes, caches, generic declarations, bindings, XML, computed helpers and
`managed.separate_values = true`. Reserved native-runtime names also reject.
These explicit guards keep missing metadata from silently weakening ownership or
identity validation. The token-only profile has a separate, broader record scope.

The current runtime exposes these contracts:

- A `ManagedSchema` binds exact generated record/field shapes, class tags, identity
  width and native codecs. Unknown mutable values, ownership cycles and modified
  identities reject. No business record names occur in the runtime.
- A `ManagedStore` owns its graph and publishes immutable `ManagedSnapshot` pins.
  Record fields, sequence/optional tuples, map views and tokens expose no normal
  mutable alias to the accepted graph. Pins retain their values after edits and
  store closure. Mutation and closing belong to the creating `Thread` object;
  immutable read pins can cross threads.
- Synchronous `transact` callbacks edit isolated ordinary drafts. Exceptions and
  caught nested-edit attempts cannot publish partial state. Retained draft aliases
  stay detached after publication. Mutable drafts must remain on their owner
  thread throughout the callback and publication. Store/client callbacks and
  reference initializers must return `None`; async functions and other returned
  values reject before acceptance.
- Logical object IDs are nonzero, document-wide unique and monotonic. Ordinary
  value occurrences and their complete owned subtrees retain ID zero. Undo/redo restore retained identity lifetimes
  through increasing acceptance epochs. A reference-initialization callback runs
  after new IDs are reserved, and can obtain exact typed tokens for live owned
  occurrences. Observable reservations remain burned after a failed callback.
- Bounded linear history holds immutable full snapshots. A new acceptance after
  undo discards the previous redo branch. Snapshot bytes, entity count, retained
  history and authority receipt count have explicit bounds. These are admission
  limits, not Python allocator or process peak memory limits.

The native Python journal uses an independent versioned format. Each frame has a
fixed length header and SHA-256 header checksum, followed by canonical metadata
and its SHA-256 checksum. State and its exact authority receipt share one durable
frame. Writes flush and call `os.fsync` before publication; an uncertain write or
post-flush publication interruption fences mutation until close/reopen.

A stable companion file supplies an OS writer lock (`msvcrt` on Windows, `flock`
on POSIX). Process termination releases the lock. Recovery validates all complete
schema, identity, history and receipt records before repairing an incomplete final
frame. A complete recovered generation is also flushed and `fsync`ed before any
accepted state or exact retry outcome is returned. Failed recovery flushing
exposes no store and requires reopen. Complete corrupt headers/payloads and semantic failures preserve the file
and reject. The journal has a 256 MiB total bound and 65,536-record bound in addition
to its per-frame bound. Capacity exhaustion rejects before another append.
Compaction/rebaseline, native-format migration and generation replacement are not
implemented in this Python profile. The current file creation profile establishes
process-crash recovery; directory-entry persistence across machine/power failure
is not yet qualified.

`ManagedAuthority` serializes exact full-state proposals against document, schema
and acceptance epoch. Accepted and stale-base conflict receipts retain the
original request bytes and outcome across exact retries and reopen. Reusing a
request identity with different bytes/base rejects. A `ManagedClient` retains one
confirmed immutable baseline and at most one pending exact request; retry never
reruns a callback, and conflicts require explicit subsequent editing. No automatic
merge, rebase, network transport, authentication or authorization policy is added.
New references to newly inserted entities currently require the local store's
post-identity initializer; a client wire proposal cannot reserve authoritative IDs
or create those forward edges in the same request.

`python -B test/python_managed_engine_test.py` currently passes **25/25** Windows
runtime tests, including actual writer-process termination before write, within
header/payload/checksum and after flush. Tests also cover immutable ownership,
identity forgery/references, failed reservations, history bounds, thread affinity,
exact retries/conflicts across reopen, length-header corruption, deferred repair,
full-log admission, post-flush allocation failure and the MSVC runtime-literal
size cap. Added checks cover complete-generation recovery flushing, async/returned
callback rejection, exact policy integer types and ordinary ancestor boundaries.
Float metadata accepts exact integer defaults and normalizes them to float;
booleans and overflowing integer conversions reject before publication.
`test/python_managed_engine_generated_test.py` is registered to exercise
four actual generated identity profiles, native codec fields, descriptors,
ownership, history, durable authority retry/conflict recovery and opt-in
configuration. All **281 generated SDK checks pass**, including nested ordinary
ancestor boundaries, default double values, float/bool/int64/enum/digest fields,
and scalar failure rollback. The configured canonical engine codec in these
four profiles is integer-key binary. Each profile runs both inline and included
dependency-file layouts, checking their emitted file inventory. Non-Windows replay and other protocol
selection profiles remain separate qualification gates. See the
[verification record](../verification/verification-python-managed-runtime-2026-10-24.md).
