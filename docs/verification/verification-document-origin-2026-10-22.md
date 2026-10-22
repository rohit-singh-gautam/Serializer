# Native document origin and confirmed Save qualification

Compiler/runtime **2.26.0**, schema language **1.21.0**. Work started 21 October;
qualification continued on 22 October 2026. This record covers the generic C++
external-document bridge in D25–D28 and the grouped auxiliary-directory change.
It does not close unrelated OPC requirements or deferred managed subsystems.

## Contract exercised

Native external manifests preserve bounded opaque application provenance outside
the authored root. The exact saved main size/SHA-256, codec, generation and
checkpoint bind that record to native namespace/identity state. Preparation pins
the accepted checkpoint and old activation, stages one proposed main and a
differential checkpoint, and durably records an unresolved token. Persistent
mutations and paired client transport reject until confirmation.

Host copying/publication and independent evidence collection run outside native
fault hooks. Confirmation validates old activation, token, proposed checkpoint,
exact proposed bytes and supplied size/digest evidence. It uses the existing
pending-manifest publication protocol. Exact retries do not reactivate a base or
mark later edits saved. Pending and lost-reply session/outbox bytes survive native
confirmation and cold recovery without executing host transport during replay.

The maintained [external-document example](../../example/managed/external_document/README.md)
exercises both application choices: an independent owned main and a durable
original. The owned case removes the external original and separate capture
before recovery. This adds no shared immutable-base resolver or cache dependency
inventory; the owned profile does not depend on those mechanisms.

## Executed checks

Final Windows and Linux/ext4 native replays each run 130 tests: **129 pass and one
existing appended-mode case skips** (it checks a sidecar-only dependency).
Both include real model/history/ID recovery, origin replacement/bounds, corrupt
and truncated metadata, unresolved restart, wrong size/digest/token rejection,
foreign/stale bindings, inner confirmation-receipt integrity even with a repaired
outer checksum, empty-origin preparation, preparation and confirmation failures,
exact retries, and subsequent Undo/Save.

The final Windows root `make.ps1 test` build and complete regression pass
**281/281 CTests** in **679.91 seconds** of CTest time. The first full run found
two test fixtures still using flat-layout paths; their manifest/companion lookups
were corrected before the clean final replay. The installed consumer and
same-major CMake package-version checks pass. CLI output reports compiler 2.26.0
and schema language 1.21.0. The final compiler SHA-256 is
`ac317c7d25476f0f85644752b3dd82b6040ffd9bbdbef4dcfd7ae8c30beae545`.

The maintained Python process oracle executes actual native helpers, terminates
without C++ cleanup at I/O boundaries, then independently checks raw main,
manifest and companion bytes and repeats cold recovery. Final Windows and
Linux executions each pass **480 recovery checks**. Its document cases cover abrupt
preparation and confirmation, host publication before native confirmation,
real partially flushed host-copy failure and rejected partial evidence,
unresolved reconciliation, unchanged authored bytes with changed native metadata,
and post-confirmation writes. The oracle independently
checks CRC-64/ECMA, SHA-256, frame/generation binding and differential-record
shape, plus the proposed token's old header/sequence/tip, proposed metadata hash
and document-size/digest bindings. It also verifies native cross-process writer
exclusion in all six
appended/sidecar/external-document × full/delta combinations. Windows additionally
checks the hidden-directory attribute.

The maintained owned-copy/differences-only example executes successfully on both
hosts using the actual runtime and generated ledger model. Its host performs a
separate flushed publication and independently rereads the evidence; native
confirmation, exact retry, cold reopen and retained Undo all pass. D25–D28 are
complete within the external-document sidecar scope documented by the API.

## Actual physical storage

The maintained large-document test measures files after three real native edits:

| Physical quantity | Windows | Linux |
| --- | ---: | ---: |
| Main JSON bytes | 1,048,696 | 1,048,696 |
| Manifest bytes | 181 | 181 |
| Companion bytes after three edits | 758 | 758 |
| Companion growth for the three edits | 408 | 408 |

The main remains byte-for-byte unchanged during editing. This measures the real
companion, not an in-memory logical delta, compressed full SRJ or package per edit.
Full proposed main bytes are staged only during explicit Save preparation.
Distinct historical values and pending session data can still consume space;
these measurements do not promise a constant-size sidecar for arbitrary edits.

## Toolchains and limits

Windows uses NTFS, CMake 4.4.2, the root `make.ps1` wrapper and the existing
RelWithDebInfo MSVC **19.51.36260.0**
configuration, with `/W4 /WX` on maintained native tests/examples. Linux uses
GCC **15.2.0** (Ubuntu 15.2.0-16ubuntu1), kernel
**6.18.54.1-microsoft-standard-WSL2**, the actual C++20 runtime built through root
`make configure` and CMake, then
the maintained native test sources, process helper and example compiled with
`-Wall -Wextra -Wpedantic -Werror`. GoogleTest headers are system includes;
test/runtime code retains warnings-as-errors. Generated headers come from the
current Windows compiler pipeline, not a separately built Linux compiler.
The Linux runtime profile enables XML/zlib, with zstd/lz4 disabled.

The final Linux disk replay uses verified `/dev/sdd` ext4 under `/var/tmp`, with
`TMPDIR` and the working directory pointing into one uniquely created directory.
Every fixture, example and crash oracle writes there. This is separate from
earlier `/tmp` controls, where this host uses tmpfs. The ext4 directory is empty
and removed after the passing run.

Additional WSL Windows-mounted
filesystem runs reproduce seven appended-journal discard cases failing
with `EACCES` on replacement or `ENOENT` after close; an isolated slowed trace
passes while the grouped trace fails. These mounted-filesystem checks are not
reported as passing native filesystem qualification. Origin/external-document
cases pass there. No change weakening durable-publication errors or retrying an
uncertain operation is used to hide those failures. That gap is retained in QA01.

This is selected Windows/native-Linux process-interruption qualification, not a
power-loss certification for every filesystem/device/network share. External
service success, authentication, exactly-once effects and policy remain host
responsibilities. Older binaries do not understand the new directory lock layout
or extended manifests. Default manifests stay version one, existing flat layouts
remain supported, and journal frames/native payloads retain their versions.

No schema/editor behavior changes are required; both editor manifests remain at
2.0.11. No editor package publication is claimed.

Task logs and useful native build outputs are retained under the ignored
`out/build/journal-directory-2026-10-21/` directory, including
`regression-final.log`, `ctest-final-details.log`, `linux-ext4-final.log` and
the mounted-filesystem failure controls. Unused scratch scripts and superseded
test binaries are removed. No new ignore pattern is needed: `out/` already
covers the task output. Source, examples, schemas and user artifacts remain intact.
The final documentation audit checks 2,291 local Markdown destinations with no
missing files; the pending inventory has 809 checked, three will-not-fix and
39 open rows, with no duplicate task IDs. `git diff --check` passes. No Git commit
was created.
