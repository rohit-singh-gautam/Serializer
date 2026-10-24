# Native schema conversion verification, 24 October 2026

Compiler/runtime **2.34.0**, schema language **1.24.0**; Windows x64,
MSVC 19.51, C++23 Release, `/W4 /WX`, one compiler/test worker.

The native database migration now compares stable schema class tags when both
models provide them. Handwritten traits without tags retain exact type-key
matching. This permits an explicit converter between independently generated
old and new types while preserving every live entity ID and class identity.
Ordinary registry lookup still checks the exact generated C++ type.

The two actual generated profiles are direct uint32 compact IDs and separate
uint64 compact IDs. Each compiles three independent schemas:
`native_schema_old.serializer`, `native_schema_new.serializer` and
`native_schema_retyped.serializer`. Their namespaces, class names and payload
fields differ; the positive conversion retains explicit class tags 9101/9102,
maps renamed fields and introduces currency/schema-revision values. Reference
construction uses checked destination registry membership, not a public token
constructor or an unchecked pointer cast.

**Six new cases pass**, three per generated profile:

- Convert the actual old layout, preserve namespace/IDs/watermark, resolve the
  new typed reference, retire the source writer, reopen the new binding and
  reject opening it with the old binding. The archive retains exact original
  file bytes, and an old-epoch exact retry returns its original outcome without
  invoking the callback. Source pins remain valid after both owners close.
- Reuse an existing instance ID with changed class tag 9103: reject before
  archival or durable writes, retaining the source root and file bytes.
- Remove an existing entity or reject the new application invariant: preserve
  the source generation and keep the original writer usable.

The complete focused replay passes **5/5 CTest targets in 26.14 seconds**:
the two new targets, managed-direct, native allocation denial and the native
process-crash campaign. Managed-direct reports **154 passing cases and one
existing inapplicable sidecar case**. Together with the six new cases this is
**160 completed GTest cases and one skipped case**, with zero failures/errors.
The allocation campaign passes; all seven existing native acceptance/rebaseline
process-crash boundaries pass. Those crash cases use the original ledger model;
this record does not claim a separate process-crash campaign for each new schema.

Evidence is retained in `out/build/native-schema-migration`: final `build.log`,
`test.log`, and three GTest JSON files. Initial compiler/fixture failures remain
in `build-first-run.log` and `build-second-run.log`. The final run passes after
correcting a new JavaScript generator string concatenation and using checked
registry lookup in the reference-conversion fixture.

This extends the [earlier native engine evidence](verification-native-database-2026-10-24.md).
It does not close MIG03: broader native journal/history upgrades remain open.
No new Linux, installed-consumer, physical power-loss or cross-language migration
qualification is established here. Log framing, existing application codecs and
the schema-language contract are unchanged. See the
[migration usage and recovery boundary](../managed/native_database.md#native-migration-and-rebaseline).
