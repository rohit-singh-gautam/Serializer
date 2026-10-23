# Managed variant merge qualification — 24 October 2026

This qualifies the existing C++ managed-variant field merge and selective-history
contract with compiler/runtime **2.30.1**, schema language **1.22.0**. Runtime
implementation, generated APIs, schema syntax and serialized formats are unchanged.
The broader `MG09` virtual-edit and portable ownership/error contracts remain open.

## Executed scope

Four additional regression cases exercise the public generated editors and real
managed stores/authorities:

- Divergent edits to one surviving alternative report its child identity and field,
  preserve exact Save bytes on rejection, and accept an explicit source retry
  without changing the identity or allocator high-water mark.
- Replacing an entity with another entity in the same variant tag conflicts with
  an edit to the old entity. Resolution selects the complete replacement rather
  than combining their fields; an immutable old pin retains its original identity.
- Appended/sidecar journals, with full/delta frames, recover both merged fields,
  the surviving identity and two-parent ancestry. Undo/Redo, checkpointing and a
  second reopen preserve the accepted Save bytes.
- A later author's same-field contribution blocks selective Undo by the earlier
  author. The rejected request retains its exact retry decision and accepted
  sequence. Ordered author Undo/Redo subsequently restores each contribution
  while retaining the alternative identity.

These extend the existing independent-field and tag-switch checks. Every profile
runs the complete merge suite, including its existing authorization, quotas,
ancestry, collaboration and journal fault controls.

| Host | Profiles | Result |
| --- | --- | --- |
| Windows x64, MSVC 19.51.36260, C++23 requirement, `/W4 /WX` | Direct/separate values × uint32/uint64 identity | **4/4 commands pass**, 22 cases each, **88 cases**, 6.75 seconds total CTest time |
| Ubuntu WSL x64, GCC 15.2/libstdc++ 15, C++23, strict warnings as errors | Same four profiles | **4/4 commands pass**, 22 cases each, **88 cases** |

The initial Windows seven-case variant-only control also passes. Linux journal
data uses exclusively created native `/tmp` directories and is removed by the
test harness. Windows uses the existing build-tree filesystem profile. This does
not qualify journals on the Windows-mounted WSL filesystem, hard power loss,
other toolchains or concurrent-host workloads.

Logs and reproducible drivers remain under ignored
`out/build/managed-variant-overnight/`: `windows-all-profiles.log`,
`windows-test-detail.log`, `linux-gcc-build.log` and `linux-gcc-tests.log`.
The native implementation and broader retained-branch/collaboration evidence are
documented in [branch merging](../managed/branch_merging.md) and the
[earlier collaboration record](verification-collaboration-merging-2026-10-21.md).
