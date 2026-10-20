# Native package Save fault verification, 20 October 2026

This record closes **CP-P12** for the selected Windows and Linux static-runtime
profiles in compiler/runtime **2.14.0**, schema language **1.20.0**. It tests the
production `save_zip_file` and `save_opc_file` implementation and file adapter.
No production Save code, public API or serialized format changed.

## Executed matrix

Both ZIP and OPC execute each boundary below. Failures are injected into actual
native file calls, with fault state confined to this Save's owned staging file.
Setup, independent destination reads and unrelated files are unaffected.

| Boundary | Observed result |
| --- | --- |
| Exclusive staging creation denied | Structured I/O failure; old destination unchanged |
| Native write delivers a five-byte prefix, then fails | Native error; partial staging removed; old destination unchanged |
| File synchronization fails | Native error; staging closed/removed; old destination unchanged |
| Atomic replacement denied before rename | `publication`; old destination unchanged; no copying fallback |
| Successful initial creation and subsequent replacement | Complete ZIP with exact payload, metadata and CRC |
| POSIX parent-directory synchronization fails after rename | `publication_uncertain`; complete new ZIP remains |

Every branch verifies zero remaining owned native handles/descriptors and no
staging names. Successful and postpublication archives validate every payload,
not only the end record. Windows uses its existing write-through replacement;
it has no separate portable directory-sync operation to inject.

Windows passes **2/2 test cases**, CTest **1/1 in 1.13 s**, using MSVC **19.51**,
GoogleTest **1.18.0** and the full-provider Release configuration. Linux passes
**3/3 cases**, CTest **1/1 in 0.10 s**, using GCC **15.2**, C++20 warnings as
errors and the existing full-provider native Linux configuration. No case skips.
The Linux-only third case covers directory synchronization. Each case contains
the ZIP and OPC decision matrix described above.

## Provenance and reproduction

The maintained test is
[`test/package_save_fault_test.cpp`](../../test/package_save_fault_test.cpp),
SHA-256 `604984343f933e82d419bf05794918be248a0d959017bd1f9e552baefeb82098`.
Windows compiles the unchanged production file adapter with private native-call
substitution; Linux links its production object with GNU linker call wrappers.
The target is registered for static runtimes on Windows and Linux. It does not
claim shared-library call interception or other platforms.

The Windows test executable SHA-256 is
`a358ca8525551f4abe70e1c5a1999993131c80dada8da21c058c5c5efef72c3f`.
Its package implementation SHA-256 is
`36cb97f200da1b423065fcae76a2059fa0daac31385bd76b8c10b8f4118178f5`;
that snapshot additionally contains the separate ZIP64 count-boundary repair.
The Linux package implementation remains
`c75ae5172e4c1b0439e0093496e774e78a578deac8ccfb1989f969a7d92ae502`.
The Linux source-inventory aggregate is
`8ce5cff0ec2e902184771f81d7aa23c6e54fb50bff2d9713f3f02a76f421e488`.
Neither run depends on generated code, so no refreshed compiler is required.

Configure through the root platform wrapper, build `package_save_fault_test`,
then run `ctest --test-dir BUILD --output-on-failure -R '^package_save_fault_test$'`.
Commands, exact source inventories and `LastTest.log` files are retained under
ignored `out/build/backlog-2026-10-20/` with the `windows-package-save-faults`
and `linux-package-save-faults` prefixes. An initial Linux overlay omitted a new
unrelated test-source dependency and failed configuration; that log is retained
separately and the complete retry passed.

The checks establish behavior under bounded native error injection. They do not
simulate power removal, storage-controller caches or every filesystem's atomic
replacement/durability semantics. The broader document/sidecar crash matrix
remains a separate backlog item.
