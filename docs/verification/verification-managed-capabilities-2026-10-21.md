# Native capability activation, history conversion and collaboration faults

Date: 2026-10-21. This checkpoint completes the native C++ contracts in **MG02**
and qualifies the library-owned session/cache boundary in **MG05**. It extends,
rather than relabels, the [earlier authorization checkpoint](verification-managed-authorization-2026-10-21.md).
Host authentication, transport/service operation, policy administration and
presence UI remain [application responsibilities](../project/application_responsibilities.md).

## Implemented and exercised behavior

`model_store::configure_capabilities` activates a version-one native profile,
rejects unavailable mechanisms and prevents attachment of excluded engines.
Identity and atomic publication remain mandatory. Selected authorization requires
a provider and cannot subsequently be cleared or downgraded; provider replacement
has no intermediate unprotected state. Attached journal/session state cannot be
silently excluded. Invalid changes preserve the previous configuration.

The capability test exercises all forty actual combinations: five history/label
profiles times eight journal/collaboration/authorization choices. Selected engines
perform real journal recovery, typed session acceptance, edits, history navigation
and full-save reload. Additional cases cover compiled-out engines, all optional
features disabled, required providers, reconfiguration rejection, imported/adopted
objects, replacement candidates and permissions revoked while an edit is open.

`migration::convert_history_policy` exercises every ordered pair of those five
history/label policies with both direct uint32 and separated uint64 generated
models. It retains payload bytes, document/entity identities, allocation high-water
marks and native leases. Branch selection and history loss require explicit input;
invalid retained states reject even when they would be discarded. Bounds, labels,
redo cursors, branching paths and lease-wrapper round trips are checked.

The new fault transport crosses the public record encode/decode boundary for
uint32, uint64 and string session keys. It injects failure before authority
acceptance, lost acknowledgements, failed polling and process-style checkpoint
reload. Three submissions retain identical request bytes and exactly one authority
acceptance. Epoch changes retain uncertain drafts without resubmitting them.
Presence/lock caches are checked against authority snapshots across 384 cycles
(768 acquire/release transitions), reordered and duplicated events, malformed
snapshots, expiry tombstones, read-only observers and authority epoch handoff.

## Results and reproducibility boundary

| Test translation unit | Windows x64 MSVC | Linux x64 GCC |
| --- | --- | --- |
| `managed_authorization_test.cpp` | 24 passed | 24 passed |
| `managed_migration_test.cpp`, direct uint32 | 11 passed | 11 passed |
| `managed_migration_test.cpp`, separated uint64 | 11 passed | 11 passed |
| `managed_collaboration_fault_matrix_test.cpp` | 2 passed | 2 passed |
| Total per host | **48 passed** | **48 passed** |

These are current-source isolated native checks, including existing tests in the
listed translation units. MSVC 19.51.36260.0 uses C++20, `/W4 /WX`, optimized release
and release GoogleTest/runtime imports; GCC 15.2 uses C++20, `-O1`,
`-Wall -Wextra -Wpedantic -Werror`. Windows launch parents suppress operating-system
error dialogs. Logs and useful binaries are retained under ignored
`out/build/pending-managed-completion-2026-10-21/`.

The isolated builds reuse unchanged generated fixtures and dependency/runtime
libraries: Windows metadata is 2.21.0; the Linux dependency fixture metadata is
2.20.0. They do not establish a newly configured full-release build or a new
non-C++ managed engine. Root's integrated release regression is recorded separately.
The `managed(...)` schema selector remains proposed syntax; versioned native C++
selection is the implemented MG02 interface, without a schema-language change.

Network deployment, authentication, transport encryption, distributed consensus,
cross-language managed engines and performance targets are not established by
these deterministic native tests. Existing resource, lease, crash and thread/reentry
tests remain part of the integrated collaboration and journal suites.
