# Identified managed baseline initialization - 20 October 2026

The selected **D22** Windows and Linux profiles passed on compiler/runtime **2.14.0**, schema
language **1.20.0**, baseline `bd2ee2ef9239b65335367bd8de19be377ce84262` plus the
uncommitted changes. This record covers checked first initialization from actual
generated identified storage, its nonzero document namespace, and the stored
native allocation watermark.

## Public contract

`model_store` has an additive tagged constructor:

```cpp
Store store{rohit::managed::identified_baseline, std::move(storage), document,
            stored_watermark, options, validator};
```

`storage` has the store's exact generated `storage_type`, including in separated
storage profiles. The required watermark preserves burned gaps; it is never
inferred from the largest live ID. The constructor does not allocate or remap
object IDs, fabricate a managed envelope, invoke preview initialization, generate
a document namespace, or attach a journal or collaboration service.

The incoming storage passes bounded native `binary_integer` encoding/decoding.
Runtime-only fields reset to generated defaults, consistently with native load.
Re-encoding must retain identical durable bytes. Native identities must be
nonzero, unique and within the supplied native-width watermark; declared tokens
must have a live target of the exact type in the supplied document namespace.
Application validation, history budgets and conservative resident admission all
precede the scoped root/control-block allocation-resource hook.

Acceptance creates one initial linear entry or tree root, or no history records
for disabled history. Existing constructor signatures, store data members,
generated schema identities, codec layouts and native envelope versions remain
unchanged. The existing private history entry point used by collaboration is
preserved. This API initializes one current durable baseline; restoring saved
history, journals or client lease state continues to use native load/recovery.

## Executed Windows checks

The host used Windows x64, Visual Studio 2026 **18.10.4**, the MSVC **14.51.36231**
toolset, CMake/CTest **4.4.2**, Ninja and GTest **1.18.0**. Targets built in Release
with C++20 and `/W4 /WX`. Configuration ran through the real root `make.ps1`
wrapper in the existing full-provider cache. `TEMP` and `TMP` pointed to the
ignored task build tree.

| Targets/profiles | Cases per target | New D22 cases per target | Result |
| --- | --- | --- | --- |
| `managed_entity_references_direct32_test`, `separate32_test`, `direct64_test`, `separate64_test` | 18 | 10 | All passed |
| `managed_entity_references_fixed32_test`, `fixed64_test`, `fixed_separate32_test`, `fixed_separate64_test` | 18 | 10 | All passed |
| `managed_store_test` | 53 | 1 | All passed |

The abbreviated profile names in the first two rows share the
`managed_entity_references_` target prefix. The eight profiles cover compact and
fixed identity encoding, uint32 and uint64 IDs, and direct and separated storage.
All **9/9 CTests** passed, with **197/197** GTest cases, including **81** new D22
cases and no disabled cases. Selected CTest elapsed time was **8.78 seconds**.

The new cases cover:

- Sparse, nonmonotonic IDs, exact native storage bytes, cyclic/forward/owner
  tokens, ordinary ID-valued fields, the stored namespace and a single baseline.
- Required cyclic references without bootstrap callbacks or remapping; linear,
  tree and disabled history admission.
- Burned watermark gaps, subsequent allocation, undo and an allocation consumed
  by a failed edit; no ID reuse or wrap at the native uint32/uint64 maximum.
- Zero namespace/watermark/root/child IDs, duplicate identities, insufficient
  watermark, uint32 watermark overflow, and missing schema identity.
- Foreign-document, dangling and wrong-target-type references; invalid required
  tokens; application validator rejection.
- Snapshot/input, string, collection, nesting, cumulative decode allocation/work,
  history entry/byte and conservative resident limits; exact snapshot/history
  boundaries and balanced scoped resource ownership.
- Zero root/control-block resource allocations on rejected admission and empty
  journal directories after rejection.
- Real native memory save/load, appended and sidecar journal creation, full Save,
  recovery, undo and redo across all eight profiles. These checks consume actual
  store-produced envelopes and journal files.
- Actual generated transient cache/buffer defaults in the separated-storage
  runtime fixture, retaining the supplied root ID and allocation watermark.

The first compile exposed an MSVC shadow warning in a test alias; the alias was
renamed. A later compile exposed an existing collaboration call to the private
zero-argument history helper; that entry point was restored as a wrapper. The
final complete selected build and test run above includes both corrections.

## Executed Linux checks

The same **9/9 CTests**, **197/197** cases and **81** new D22 cases passed on native
WSL2 Linux x64, with GCC **15.2.0**, CMake/CTest **4.2.3** and Ninja **1.13.2**.
The selected CTest elapsed time was **1.11 seconds**, with no failures, disabled
cases or remaining `identified-*` journal directories. Release targets used the
repository's C++20 and warning-as-error configuration.

The existing isolated Linux source/build cache received a narrow overlay of the
managed header, tests, CMake registration and exact schema/profile inputs. Every
overlaid file was checked against its Windows SHA-256; changed files received
current timestamps before the root `make configure` wrapper and selected CMake
build. Source, generation, compilation and test access used native Linux storage.
The unchanged C++ generator came from the existing 2.14.0 compiler, SHA-256
`258f9c4d011855d82bb90f26b234231cf1cc8a9b44d1f36458ddd832d0b434bf`.
This narrow overlay does not claim a new full snapshot build of concurrent
changes to other generated-language helpers.

## Reproduction and retained evidence

After initializing the normal MSVC x64 environment, run from the repository root:

```powershell
./make.ps1 configure -BuildDirectory out/build/opc-implementation
cmake --build out/build/opc-implementation --config Release --parallel 6 --target `
  managed_entity_references_direct32_test managed_entity_references_separate32_test `
  managed_entity_references_direct64_test managed_entity_references_separate64_test `
  managed_entity_references_fixed32_test managed_entity_references_fixed64_test `
  managed_entity_references_fixed_separate32_test `
  managed_entity_references_fixed_separate64_test managed_store_test
ctest --test-dir out/build/opc-implementation --build-config Release `
  --output-on-failure --verbose --parallel 4 `
  -R '^(managed_entity_references_.*_test|managed_store_test)$'
```

Final logs and per-executable GTest JSON are retained under ignored
`out/build/backlog-2026-10-20/`: `windows-d22-build.log`,
`windows-d22-tests.log`, and `d22-results/`, including `summary.json`. Initial
compile diagnostics are retained separately as `windows-d22-build-initial.log`
and `windows-d22-build-friend.log`. Fresh test journal directories were removed;
no `identified-*` directories remained in the CMake test tree.

The Linux root wrapper uses `make configure BUILD_DIR=<native-build-directory>`,
followed by the same selected CMake targets and CTest expression. Linux evidence
in the ignored task directory consists of `linux-d22.log`,
`linux-d22-verification.json` with exact overlay hashes, `linux-d22-summary.json`,
and `linux-d22-results.tar.gz` containing the nine GTest JSON files and native
`LastTest.log`. The isolated cache and useful qualification outputs were retained.

The actual fixture-generating compiler SHA-256 was
`77352650a6d6295b55c67bebb3be8ca64f94c3ca8133b770c8b87bc762336f59`.
`serializer --version` reported compiler **2.14.0** and schema language **1.20.0**.
Frozen checked source hashes were:

| File | SHA-256 |
| --- | --- |
| `include/rohit/managed.hpp` | `b93731fab933c1c520f02e189551f3b33515aed7b34e85faed9b09a074a5fdf1` |
| `test/managed_identified_baseline_test.cpp` | `e9e685990038ac480d2d44ade7f3b6ae147d8784e726c02bdd5357634685c1ec` |
| `test/runtime_features_test.cpp` | `f89d48ba5572db07722590a9e9e64424ff1e1f8dde3dd934bb31ecf0a02c64fe` |

## Qualification limits

These are selected Windows/Linux C++ admission/recovery results, not a new full native
suite. The resident budget is existing conservative admission
accounting; standard codec/container temporaries and allocator/RSS peak usage are
not a hard heap cap. User callbacks retain responsibility for their own external
effects. The constructor consumes a statically typed native storage layout; it
does not perform arbitrary historical-schema or application ID conversion.
