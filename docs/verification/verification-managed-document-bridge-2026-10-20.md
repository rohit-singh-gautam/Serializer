# External documents and native recovery - 20 October 2026

This record qualifies the selected C++ native/external-JSON document bridge for
**D01-D21** on compiler/runtime **2.14.0**, schema language **1.20.0**. The bridge
keeps the application document separate from authoritative native identities,
allocation marks, retained history and collaboration metadata. Format-specific
XML/OPC adapters and broader provider/platform qualification have separate gates.

## Implemented contract and repaired admission order

The public `make_journal_document_codec<Root>` adapter supplies synchronous owning
encode/decode callbacks and an exact codec identifier. The application explicitly
selects `journal_storage_mode::sidecar`. Ordinary edits retain the original main
file; full Save publishes a new coherent document/manifest/companion generation.
The main-file projection has zero placeholders for generated managed identities.
Those placeholders never supply authoritative IDs during recovery.

Recovery first binds exact main-file length and SHA-256 to a supported manifest
and codec. It then validates the selected companion header, generation and first
baseline frame, including its extent and checksum, before calling the external
decoder. Canonical native reconstruction and complete replay validation precede
store publication. A pending manifest selects the generation matching the actual
document bytes; applications must retain the selected side-channel files.

The new counter regression exposed an admission-order bug: a foreign companion
generation with a valid recomputed header checksum, a corrupted baseline payload,
and a truncated baseline each invoked the external decoder once before rejecting.
The unmodified runtime failed all three zero-call assertions. Moving companion
and baseline-frame validation ahead of decoding fixes these cases without
changing public APIs, journal bytes or successful replay semantics. The rejected
recovery preserves the previous live store. Temporary external bytes are released
after the owning decoder callback returns.

## Requirement evidence

| IDs | Executed evidence |
| --- | --- |
| D01-D02 | Eight independent manifest unit tests pin native-header/length/digest/codec offsets, checksums, unsupported versions, all truncations, forged extents and stale exact-byte bindings. External recovery counts decoder calls for same-sized stale JSON and wrong codecs. |
| D03-D04 | Exact native save/reopen comparisons preserve root/child IDs and native allocation marks. Burned reservations survive Save and later creation cannot reuse them. |
| D05-D06 | A new labeled tree fixture saves at an undone cursor, reopens exact native state, retains labels and alternate redo children, and checks subsequent child identities. |
| D07/D18 | Independent JSON inspection accepts only the root name/entries/persistent_id projection and exact key/value child shape, with zero ID placeholders. Manifest, history and session records remain outside that document. |
| D08-D09 | Existing staged Save fixtures cover encoder failure/loss, live tampering, pending publication with old bytes, replacement interruption and metadata-only generation selection. |
| D10-D13 | Native platform synchronization/replacement plus abrupt process termination exercise old/new generation decisions before and after publication. Repeated recovery selects the same coherent state. |
| D14 | Stale main bytes, wrong codec, foreign companion generation, corrupt baseline and truncated baseline reject before the external decoder; replay validates generation and native checkpoint fences. |
| D15-D16 | Collaboration fixtures retain unsent changes, exact lost-reply retries and metadata-only saved generations. Saving never acknowledges an operation or advances a receive cursor independently. |
| D17 | Legacy version-two sidecars remain writable and explicit Save upgrades them to version three. Native appended mode remains the default; selecting an external codec there rejects. |
| D19-D20 | The maintained crash matrix now includes external JSON with normal and delta logical records, with 124 additional repeated recovery checks. |
| D21 | The new runnable C++ example demonstrates bounded callbacks, Save at an undone cursor, exact native reopen, redo with the original child ID and nonreuse of a burned allocation. |

The crash runner executes **364** recovery checks across native appended/sidecar
normal/delta configurations and the two external-JSON configurations. Each crash
uses a child process with distinctive abrupt exit status **86**. Append, reserve,
undo, staged Save, publication, cleanup and tail-repair boundaries are included.
Every selected recovery is repeated to reject duplicate application. A further
write must continue the selected generation.

The Python inspector independently verifies main JSON keys/zero IDs, manifest
magic/version/length/SHA-256/CRC/codec, companion generation and complete frame
extents, sequence checksums and baseline binding. External differential records
receive independent shape/extent checks; their canonical native dictionary
expansion is checked by native recovery rather than a second Python decoder.
The existing native-container path retains its independent delta expansion.

## Windows execution

The selected build uses the actual root CMake pipeline in
`out/build/opc-implementation`, Release C++20, MSVC **19.51**, `/W4 /WX`, the amd64
Visual Studio developer environment and task-local `TEMP`/`TMP`.

The extended admission test was first compiled against the earlier runtime and
failed the three expected zero-decoder assertions. After the localized runtime
repair, all **3/3** selected CTests passed in **132.10 seconds**:

| CTest | Time |
| --- | --- |
| `managed_direct_test` | 19.74 s |
| `managed_journal_crash_test` | 110.50 s |
| `managed_external_document_example` | 0.72 s |

The direct target includes external-document, collaboration, native sidecar and
manifest tests. An earlier standalone execution also reported all **364** crash
checks passing. The retained before/after logs are
`out/build/backlog-2026-10-20/document-admission-before.log`,
`document-admission-after.log` and `document-crash-windows.log`.

## Linux execution

The initial Linux-local full regression snapshot passed **207/207 CTests** in
**571.42 seconds**. Its source aggregate is
`c0bc174d82004a02f601c59cec6380de5c38f73c1a9b83240271237fee6b8ee6`, compiler
SHA-256 `7bd1887df5a04c9f4dd196e940b6a97fb877c44fcd585068891c6498b940dbaf`.
That snapshot includes the expanded crash matrix and runnable example but
predates the final admission-order repair. A subsequent complete 2,280-file
snapshot includes the final repair and passes all **3/3** selected CTests in
**180.25 seconds** on native Linux GCC **15.2**, CMake **4.2.3** and Ninja **1.13.2**:

| CTest | Time |
| --- | --- |
| `managed_direct_test` | 60.35 s |
| `managed_journal_crash_test` | 116.60 s |
| `managed_external_document_example` | 3.28 s |

Its source aggregate SHA-256 is
`c7fb6d1015d78bf2a7cb4f6e72588b02c165cd545a3df2ac0047232fbaaff736`, compiler
SHA-256 `7282fcb10947bae3c657089f05e5db33522f84317d5839fbc6d16d5ae2178cf1`.
Configuration, selected builds and tests took **481.32 seconds**. The root Linux
wrapper and native CMake pipeline operate in a private Linux-local repository;
input manifests retain the Windows source hashes and required LF normalization
of `setup.sh`. No Windows-mounted build tree or WSL restart is involved.

The retained evidence is
`out/build/backlog-2026-10-20/current-regression/linux-document-focused.log`,
its verification JSON and CTest LastTest log. These focused current-source
results qualify the repaired D01-D21 profile separately from any subsequent
complete regression run.

## Reproduction and limits

Build with managed runtime/tests/examples enabled through the root platform
wrapper, then use:

```sh
cmake --build out/build/opc-implementation --target managed_direct_test managed_journal_crash_helper managed_external_document_example
ctest --test-dir out/build/opc-implementation -R '^(managed_direct_test|managed_journal_crash_test|managed_external_document_example)$' --output-on-failure
```

Windows multi-configuration builds additionally select `--config Release` and
`-C Release`. For the executable lesson and Linux wrapper options, see the
[example instructions](../../example/managed/external_document/README.md).

Application codecs enforce their own external-format parsing/allocation limits,
must preserve durable values exactly and must not reenter the store. Cooperative
callbacks cannot be preempted. Process-termination and injected I/O boundary tests
qualify this selected implementation; they do not certify power-loss durability
on every filesystem/device, automatic XML/OPC projections, hard process RSS caps,
field-level history storage or native managed engines in other languages.
