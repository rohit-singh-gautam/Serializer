# Generated Python managed profile: 24 October 2026

Compiler/runtime 2.34.0, schema language 1.24.0, Windows Python 3.14.3.
This record qualifies the concrete owning profile in the
[Python runtime guide](../managed/python_native_runtime.md), separately from
completion of the all-output MG01 requirement.

The combined compiler/backend worktree passes:

- **25 standalone runtime tests**, including process termination, durable identity
  reservations, immutable ownership, history, exact authority/client outcomes,
  complete-generation recovery flushing and root-lifetime rejection.
- **281 actual generated SDK checks** under uint32/uint64 and fixed/compact IDs,
  each with inline and included dependency-file layouts. The runner checks the
  separate-output manifest/files as well as executing generated metadata, native
  scalar codecs, ownership, history, journals and exact receipts.
- **5,310 token-codec checks** with four independent generated C++ exchange
  profiles. Token-only selection remains distinct from engine selection.
- The final affected full core suite passes **647/647 cases**, with no failures,
  errors, disabled cases or skips and the configured formatter gate enabled.
  The ordinary Python dependency-file control also passes after the fix.

The first included-file check reproduced silent monolithic output: the managed
branch had not propagated the existing native source-file map. Passing that map
through the ordinary bundle planner restores the requested layout. It adds no
new storage format and weakens no graph capability gate. The expanded runner
checks every identity profile in both layouts. Earlier 141-check generated SDK
results remain historical checkpoints, not the final layout coverage.

The standalone root regressions initially reproduced authority, recovery and
receipt replacement of the accepted root. Validation now rejects those changes
before identity reservation, incomplete-tail repair or clearing pending state.
Initial failure evidence is retained alongside passing logs.

Evidence under ignored `out/build`:

- `python-managed-separate-audit-first-run.log`
- `python-managed-separate-sdk-first-run.log`
- `python-managed-separate-core.json` and matching core log
- `python-managed-separate-controls.log`
- `java-managed-engine-merged-sdk-first-run.log` for the combined nine-target run

Compiler/native builds and CTest use one worker. The generated driver uses the
canonical integer-key binary engine codec; non-Windows execution, additional
canonical engine protocols, data-object alias locking, native journal migration,
compaction and machine/power-failure durability remain separate gates. Admission
limits do not establish a hard process-memory ceiling.
