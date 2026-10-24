# Native eager database verification, 2026-10-24

[Documentation index](../README.md) · [Native database](../managed/native_database.md)

Scope: the first DB02 bounded eager embedded C++23 engine in
`native_database.hpp` and `native_database.cpp`. This is not completion of the
consolidated database-storage design or qualification of a paged/cloud backend.

The native unit matrix in `native_database_test.cpp` covers complete-candidate
validation and cancellation, unchanged accepted state before acceptance, immutable
pins, durable state/outbox-style records and managed IDs, exact retry before/after
reopen, request/base/epoch mismatch, stale/foreign/moved/consumed preparations,
owner-thread confinement and edit/I/O callback reentrancy, exclusive writers,
complete-frame corruption rejection, incomplete-tail repair, and retained-outcome
admission. Fault cases inject failures before writes, after a complete write,
after flush and before publication. Issued-write uncertainty must fence mutation
until reopen resolves exactly one original outcome.

`native_database_crash_helper.cpp` and `native_database_crash_test.py` additionally
terminate a separate process without destructors at all four boundaries and verify
recovery and exact retry in two fresh processes. The before-write case recovers
the old base and executes one new command. Complete-write/flush/prepublication
cases recover the original outcome without reexecuting its edit callback.

The initial Windows x64 MSVC 19.51 C++23 Release checkpoint passed all nine
native database unit tests, the four-boundary subprocess recovery matrix,
and the isolated allocation campaign (38 rejected preparation allocation sites
before success, with allocations forbidden during publication). Python token
exchange also passed 5,310 checks including four actual C++ identity profiles.
Logs: `out/build/db02-initial-native-tests.log`.

The complete initial `managed_direct_test` rerun passed 147 tests with one
existing unsupported-profile skip; it required filesystem permission outside
the sandbox for temporary-path canonicalization. Log:
`out/build/db02-native-permissions-rerun.log`.

The subsequent current Windows checkpoint passed all **15/15 native database
unit tests**. The complete managed-direct executable passed 153 tests with the
same one existing skip. The isolated allocation campaign rejected **73** ordinary
preparation allocations before success, then proved allocation-free ordinary
publication and allocation-denied writer transfer after native migration
replacement. Actual resource-domain bounds and process RSS remain separate.

The subprocess matrix passed four acceptance boundaries and three native
migration boundaries. Retained old-schema/old-epoch logs, identity-preserving
conversion, source writer retirement, invalid conversions, pre-replacement faults
and post-replacement uncertainty all passed. Prepared field details and budget
failure tests passed before durability. All four standalone index profiles also
passed actual database uniqueness, snapshot querying and persistent derived-index
integration.

The focused run passed **22/22 CTest targets** in 26.27 seconds with one worker.
Full log: `out/build/managed-contracts-windows-passing.log`; counts:
`out/build/managed-contracts-windows-summary.json`. Initial replacement failures
are retained in the earlier first/second-pass logs; the final run passed after
the native Windows generation-replacement and explicit path-termination fixes.

No real device power failure, filesystem corruption campaign, concurrent reader
profile, cloud provider, persistent database index, lazy/paged storage,
performance/RPO/RTO, compaction, operational backup or Linux
qualification is established by this record. The publication function is
explicitly `noexcept` and consists only of owning pointer/vector swaps and scalar
assignment. The executable-local ordinary-new interceptor verifies that boundary;
it does not measure process RSS or bound arbitrary application allocations.

The subsequent allocation-floor checkpoint passes **16/16 native database unit
tests**. Restoring with a watermark of 100 produces ID 101 on the next allocation;
an invalid floor rejects before invoking the converter. At that checkpoint, the migration
test changes schema metadata on the same generated root type. It does not qualify
conversion between separately generated old/new field layouts or renamed type
keys; that checkpoint's converter preserves every exact live type key.

GCC 15.2 compiles the physical log implementation in the later Linux kernel build;
this is compilation evidence, not Linux native-engine runtime qualification.
After compiler/backend merge `dacd81e`, the combined native build and all 22
selected CTest targets pass in 39.77 seconds, including the 16 native database
cases, allocation-denied publication and all seven native acceptance/migration
process-crash boundaries. Evidence: `out/build/compiler-merge-native`.

The subsequent [native schema conversion replay](verification-native-schema-migration-2026-10-24.md)
qualifies independently generated renamed layouts using preserved stable class
tags. Six new cases pass in direct uint32/separate uint64 compact profiles; the
focused replay passes five targets, including the existing allocation-denial and
seven-boundary native crash campaigns. Exact type keys remain the fallback for
handwritten traits without class tags. Broader native journal/history migration
and additional host/profile qualification remain open.
