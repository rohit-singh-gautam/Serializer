# Historical persistent-ID migration qualification, 20 October 2026

[Persistent-ID migration](../guides/persistent_id_migration.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

The selected Windows x64 profile passes **92 actual CLI migrations** using
independently produced compiler/runtime **2.6.0** artifacts and the current
compiler/runtime **2.14.0** converter. This qualifies `MIG01` for the graph,
representations and supported containers below. It does not establish arbitrary
historical-format compatibility.

## Independent source and build provenance

The older source is exactly Git revision
`7b166e202668747f3a7e9a3545b3e4d75e853599`, whose CMake project release is
**2.6.0** and supported schema language is **1.19.0**. The qualification script
extracts that revision with `git archive`, verifies all **1,687** archived source
files against the Git bytes before reuse, and builds its actual compiler, runtime
and generated managed records. It does not use the current compiler to generate
the older model, modify the archived source, synthesize old bytes through current
policy overrides, or depend on the unrelated older binaries already in the workspace.

The historical build uses MSVC **19.51.36260.0**, C++20, CMake **4.4.2** and Ninja
on Windows x64. Tests, examples, installation, SIMD and all optional XML/compression
providers are disabled. Configuration uses the archived Windows wrapper; the
native build selects `serializer` and `serializer_managed_records`. No package
download, Git worktree, editor packaging or publication is involved. Windows IDE
design-time `obj` caches for unused C# examples are excluded from the archive's
unexpected-file check; every archived file still receives an exact byte check.

Separately compiled producers include only the old generated model, old runtime
headers/records and old runtime library. Current schema-linked CLI executables
and native verifiers include only their matching current counterparts. All
consumer translation units compile with `/W4 /WX /EHsc`.

| Artifact | SHA-256 |
| --- | --- |
| Rebuilt 2.6.0 compiler | `18e7f6cc076a3bc68b9fe726c62366f2cca37ff04bfab071fb6871f4764cd269` |
| Rebuilt 2.6.0 runtime | `4dfa6900b12de46b49e078d7dd1d21483395992c2ac0116e060852e7632abb4e` |
| Selected 2.14.0 compiler | `4c79e3a359e8335dd849569818aa0dc518b236684ad898eff6c9c8f3c5b21ef6` |
| Selected 2.14.0 runtime | `8ff9712d2f0a77bf028977701897fc1ee06796d8354dfac7f21d0e56e541de7f` |

The current starting Git revision is
`bd2ee2ef9239b65335367bd8de19be377ce84262`; its 2.14 changes are uncommitted.
The retained provenance JSON binds the actual compiler/runtime binaries, fixture
sources and every original/converted/restored artifact to their hashes.

## Executed compatibility profile

The [maintained fixture](../../qualification/managed_legacy_migration/model.serializer)
uses a managed document, an owned array of managed entries and ordinary `uint64`
values. It uses declarations supported by both compilers, with no typed managed
references or explicit class-ID syntax introduced in language 1.20. The same
source is generated under two policies:

| Profile | Old storage | Current storage |
| --- | --- | --- |
| `direct32` | Direct managed classes, fixed `uint32` IDs | Same layout with compact-varint IDs |
| `separate64` | Separate ordinary values/storage, fixed `uint64` IDs | Same layout with compact-varint IDs |

The historical producers create **22 original files**:

- Twelve raw artifacts: each of `binary_none`, `binary_integer` and
  `binary_string`, for ordinary allocated IDs and a separate raw boundary graph,
  under both profiles. Boundary IDs include 127, 128 and the complete selected
  unsigned ID type's maximum value.
- Ten managed saves: disabled history, linear history without/with labels and
  tree history without/with labels, under both profiles. Histories retain redo
  states; trees retain two children with different persistent IDs after undo.

The actual current `serializer_add_migration_tool` CLI converts every raw source
codec to every target codec: **36 forward raw cases**. It also converts the ten
managed saves. All 46 outputs are converted back to fixed IDs and the original
codec: **92 CLI operations total**. Every reverse result is byte-identical to its
independently old-produced input, and each original input remains unchanged.

The current native verifiers also decode the compact raw outputs and load every
compact managed save. They check document halves, root/child IDs, ordinary values,
linear cursors/labels, tree revision numbers/parents/labels, retained branch
snapshots and redo behavior. Allocation watermarks include IDs absent from the
current state after undo. Appending after migration allocates the next unused ID;
it does not reuse an undone branch's identity. These semantic checks supplement
the complete reverse-byte comparisons.

## Reproduce and retained evidence

From a Visual Studio developer shell with the current native build available:

```powershell
python -B qualification/managed_legacy_migration.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --current-build out/build/opc-implementation `
  --build out/build/backlog-2026-10-20/managed-legacy-migration --jobs 4
```

The [qualification runner](../../qualification/managed_legacy_migration.py) and its
fixtures are tracked source. It retains build outputs and fixture artifacts under
the selected ignored build directory; a repeated run verifies the old source
before reusing its build. A non-Windows path through the archived Makefile exists
but was not executed in this checkpoint.

Executed evidence:

- `out/build/backlog-2026-10-20/managed-legacy-migration/legacy-build.log` records
  the separate 70-step historical native build.
- `out/build/backlog-2026-10-20/managed-legacy-migration-final.log` records the
  successful current qualification run.
- `out/build/backlog-2026-10-20/managed-legacy-migration/provenance.json` records
  all selected identities, binary/fixture/artifact hashes and operation counts.

This result covers the matching 2.6 graph and the native save formats that the
converter already supports. It does not qualify other old revisions, schema
layout changes, managed references unavailable in 2.6, historical allocation
lease formats, journals, collaborative checkpoints/outboxes, database sinks or
interrupted baseline replacement. Those remain separate contract/qualification
work; normal Load still performs no automatic detection or encoding conversion.
