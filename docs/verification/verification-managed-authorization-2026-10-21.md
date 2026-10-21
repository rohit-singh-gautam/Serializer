# Managed authorization, capability and allocation batch

Date: 2026-10-21. This batch audits all fourteen non-database managed/storage
requirements assigned from `docs/pending.md` together. It implements coherent
C++ slices of `MG02`, `MG03` and `MG07`. It does not mark the broader requirements
complete. `DB01` and `MIG03` remain on hold at the user's request.

Subsequent ownership disposition: the user clarified that application-owned
responsibilities must be documented as will not fix in Serializer. The
[canonical decisions](../project/application_responsibilities.md) mark `MIG02`
and `MG06` accordingly and recognize `MG03`'s completed library enforcement with
principal binding, policy persistence and administration supplied by the host.
The audit table below records the earlier implementation checkpoint; this note
does not retrospectively change its test counts or claim new tests.

## Implemented contract

- `managed_authorization.hpp`: immutable principal-context rules on stable entity
  IDs and field scopes; independent content, structural and policy permissions;
  default ancestor-denial precedence and explicit nearest-rule exceptions;
  generated complete-candidate comparison through actual owning graphs. Moves,
  deletion, rekeying and active variants cannot hide protected descendants.
- `model_store::set_authorization_policy`: owner-thread publication gate before
  durable writes and state/history publication, including current-policy
  Undo/Redo, load/recovery/migration/discard and runtime updates. Namespace
  replacement is explicit. Trusted client join/refresh/epoch reconnect are
  replication operations, allowing read-only observation independently of edits.
- `managed_capabilities.hpp`: explicitly versioned requirement/availability
  validation, fixed version-one `all`, exclusions, unknown-version/bit rejection,
  compiled history/journal/collaboration compatibility and required installed
  authorization. This helper does not attach/detach engines or convert history.
- `managed_allocation.hpp`: a thread-safe hard cap on requested bytes routed
  through one PMR resource, with upstream lifetime ownership, failure rollback,
  current reservations and reservation high-water accounting. Standard generated
  payload containers, allocator overhead and whole-process residency are excluded.

No schema syntax, generated public naming, existing saved format or wire value
changes. Policy persistence, principal selection and policy administration remain
trusted host responsibilities. The explicit `policy` permission is a host check,
not automatic classification of ordinary schema fields.

## Fourteen-requirement audit

| ID | Result of this batch and remaining acceptance boundary |
| --- | --- |
| MR03 | Existing lowering/emitter groundwork remains available, but the compiler still rejects other-language native managed generation. No codec-only token mode or recursive-reference contract is introduced. |
| MIG02 | No additional historical format is guessed. Earlier explicit fixed/compact and whole-history conversions remain implemented; other historical layouts and checkpoint/outbox/epoch migration need selected source contracts and historical fixtures. |
| MG01 | Native managed engines remain C++ only. Ordinary eleven-language codecs do not supply native transactions, history, journals or collaboration. |
| MG02 | Adds versioned C++ requirement/availability validation. Schema selectors, complete automatic activation/exclusions and history-policy conversion remain open. |
| MG03 | Adds inherited rules and complete publication enforcement with separate content/structure permissions, explicit policy permission and replication origin. Principal binding, policy persistence/administration and automatic permission-policy-field classification remain host-owned; broader gate stays open. |
| MG04 | Existing conditional collaborative field merging remains available. No new multi-parent retained-history DAG, merge-base selection or branch publication format is claimed. |
| MG05 | Adds a tested distinction between authorized local edits and trusted accepted replication. Broader distributed transports, presence/cache and fault/reconnect qualification remain open. |
| MG06 | No application effect execution or exactly-once delivery claim. Durable distributed intents/reconciliation require an independently specified service contract. |
| MG07 | Adds bounded routed PMR requests and resource-lifetime/failure tests. Generated payload allocator propagation/decoding and hard total allocation peaks remain open. |
| MG08 | Existing packed/view/recursive/Protobuf and virtual/repeated-base rejection remains. No unsupported editor combination is silently enabled. |
| MG09 | Existing ordinary behavior dispatch and atomic variant collaboration remain unchanged. Field-level managed variant merging and portable owned/borrowed error contracts remain open. |
| MG10 | Existing precise publication notifications and retained snapshot history remain unchanged. No compact nested identity/history representation or measured edit-latency improvement is claimed. |
| PG01 | No page store/cache or dirty-page recovery is introduced; eager model/index behavior remains explicit. |
| PG02 | No lazy payload loading or paged selector is introduced ahead of its page-lifetime/recovery prerequisites. |

## Verification

Windows Release: **135/135 GTests pass**. The complete
`managed_collaboration_test` passes **113** cases, including **19** added
authorization/capability/allocation cases. `managed_document_features_test` passes
**22**, including three new managed-base/variant, ordinary-base-field-scope and
runtime-update policy cases. The tests include authority-side candidate rules
and exact denied retries, immutable pins, denied insertion/nonreuse, deletion of
identity-only entities, structural rekeying, protected cross-parent movement,
current-policy Undo/Redo, foreign namespace replacement, callback reentry,
appended/sidecar journal denial/recovery/discard, and read-only trusted replication.
Existing authority single-encoding and collaboration resource/fault checks also
remain passing with no policy installed.

Build: MSVC **19.51.36260.0**, C++20, `/W4 /WX`, CMake Release/Ninja,
`SERIALIZER_BUILD_TESTS=ON`, `SERIALIZER_BUILD_MANAGED=ON`. The normal
`./make.ps1 configure` workflow used the existing
`out/build/ReleaseWindows/vcpkg_installed/x64-windows` provider prefix. Actual
link inputs use Release GTest/lz4 libraries, not debug imports. XML, ZLIB, ZSTD
and LZ4 are enabled. Initial compiler generation lacked the provider DLL search
path; prepending the existing provider `bin` directory resolved that environment
failure. Native test launchers suppress Windows error dialogs before launching
children and retain exit codes. Development build metadata is **2.20.1**;
the aggregate release is **2.21.0**, and its integrated release check is recorded
separately by the root verification task.

Linux GCC **15.2.0**: **41/41 GTests pass**: all **19** new helper tests and all
**22** document-feature tests. Current source headers/tests were compiled with
`-std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -pthread`. This isolated
replay links the unchanged prebuilt native runtime/GTest/provider libraries and
generated fixture headers from the earlier native Linux build, whose version
metadata reports **2.20.0**, schema **1.21.0**. It validates the new header/runtime
boundary; it is not a fresh full-repository Linux compiler build or a claim to
requalify every existing managed profile.

Two initial test-fixture issues were corrected before the final passing replay:
the Windows sandbox denied canonicalization of an OS-temp journal path, so the
test-owned journal directory now uses its build/test working directory; and a
denied variant insertion intentionally burns its issued ID, so the test checks
unchanged accepted pins plus the advanced watermark rather than demanding an
identical saved allocator envelope. No failure is waived in the final counts.

Final logs and useful binaries remain under the ignored
`out/build/pending-managed-batch-2026-10-21/` directory:
`managed_collaboration_test.log`, `managed_document_features_test.log`,
`managed_authorization_linux.log`, `managed_document_features_linux.log` and build
logs. Scratch launch/compile scripts were removed after use. Test-owned journal
directories were removed; the narrow `serializer-authorization-[0-9]*/` ignore
pattern handles interrupted runs. No database adapters, editor packages,
sanitizers, full-process memory measurements or performance speedup claims form
part of this slice.
