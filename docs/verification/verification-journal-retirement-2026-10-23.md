# Explicit document-journal retirement qualification — 23 October 2026

Compiler/runtime 2.29.0 adds opt-in retirement without changing schema language
1.21.0 or the journal wire format. The
[usage contract](../guides/usage.md#explicit-retirement-of-self-contained-documents)
defines the host's self-contained-document assertion and reattachment obligation.

## Executed checks

Windows x64 Release, Visual Studio 18 2026/MSVC 19.51, generated managed records
and ledger fixtures from the owning compiler. XML and optional compression
backends were disabled in this focused build; GTest 1.18 was reused as a
read-only installed package. Build output and retained logs are outside source.

`managed_direct_test` with
`--gtest_filter=managed_document_journal.*:managed_document_collaboration.*`
passed **39 tests across two fixtures** in 4.464 seconds. Coverage includes:

- Clean opt-in retirement removes the companion directory, preserves the main
  byte for byte and fences the terminal owner. Default opt-out retains storage.
- In-memory retirement retains live history and native identity/allocation;
  reattachment before Undo preserves the saved main and makes Undo dirty.
- Dirty values, unsaved ID reservations and prepared publication retain storage.
- A changed saved generation rejects cleanup without modifying either file;
  non-file companion entries reject before deletion or fencing.
- Saved unsent collaboration survives recovery and publishes once. Retirement
  never sends it, including after an empty outbox has been acknowledged.
- Unsent discard stays local. Acknowledged changes remain shared until a new
  synchronized undo. Lost replies prohibit discard and retain exact retry bytes
  across Save/reopen.
- Existing external-document identity, history, preparation, origin, bounded
  differential storage and extended-length Windows path controls still pass.

The rebuilt executable reports `Serializer compiler 2.29.0` and supported schema
language `1.21.0`. Six package-version checks passed through the owning generated
CMake discovery files: both runtime and host-compiler components accept 2.28.2,
report exact compatibility for 2.29.0, and reject 3.0.0.

## Scope boundaries

The JSON fixtures deliberately opt in to exercise retirement guards; normalized
JSON itself does not independently retain the real IDs stored in its side
channel and must not normally enable retirement. Qualification of a host's
self-contained external format remains that host's responsibility.

POSIX lock-path identity validation after acquisition is source preparation;
Linux runtime, concurrent retirement/open stress, hostile ancestor replacement,
power-loss and injected partial-cleanup failures were not executed here. No
cross-platform durability claim follows from these Windows checks. Cleanup is
nonrecursive and may retain a directory if another writer acquires it.

This feature supplies neither transport synchronization nor a checked shared
session leave handshake. Active/durable collaboration state remains protected;
an empty outbox does not establish permission to delete its checkpoint.
