# Numeric, construction and registry verification

Release under qualification: compiler/runtime **1.16.0**, schema language
**1.11.0**, and both editor extensions **1.1.39**, reviewed on **2026-10-11**.
This records executed checks for the [enhancement checklist](../../SerializerTODO.md).
It does not qualify a paged database adapter or arbitrary backend combinations.

## Native contract coverage

The new generated fixtures exercise lossless casts across full signed/unsigned
64-bit boundaries, exact checked arithmetic and predicate failures; fixed arrays,
default overloads, explicit/constrained construction and scalar record projections;
projected readonly calls and lazy exact owning-record selection; and generic record
map-key round trips through existing C++ codecs. Older language headers retain
their arithmetic contract, and unsupported backend shapes diagnose explicitly.

Managed registry qualification covers direct and separate-value storage with both
32-bit and 64-bit persistent IDs. Checks exercise snapshot pins, wrong/missing
IDs/types/members, nested private/base paths, composite scoped keys, hash/ordered
indexes, vector relocation and editor expiry, duplicate-key rollback through both
generated and raw callback edits, memory admission cleanup, external owning-handle
lifetime, reservation/import/retirement and same-transaction no reuse. Appended
and sidecar journals are recovered after Save/undo; authority checks exercise
read-only denial, accepted registry editing, uniqueness rejection, exact retries
and conditional collaborative undo.

Windows Release builds ran through `./make.ps1 test`; the complete final
CTest run passed **121/121 tests** with the Visual Studio toolchain environment.
The core executable passed 520 of 521 internal tests, with one optional
clang-format integration test skipped because its explicit formatter override
was not set. All four registry storage/ID configurations passed nine internal
tests each. Existing journal crash recovery, history, authorization, collaboration,
installed-package/header and package-version checks passed. `serializer --version`
reports compiler/runtime `1.16.0` and schema language `1.11.0`; package discovery
retains same-major compatibility.

After a GCC name-qualification correction in the C++ generator, the Windows
compiler and core target rebuilt successfully and all five focused core/new SDK
CTest entries passed again. The correction does not alter emitted behavior.
Two existing read-only codec-test loops were changed to const references for
GCC's strict copy warning; the Windows core rebuilt and passed again afterward.

The final Windows log is `out/build/make-Release/enhancement-verified-test.log`.
The follow-up log is `out/build/make-Release/enhancement-portability-test.log`.
The final test-only recheck is `out/build/make-Release/enhancement-final-core-test.log`.

WSL GCC 15.2 built the final compiler, all four generated registry configurations,
the record-map fixture and the managed custom-ID companion with strict warning
checks. All **6/6 focused Linux CTest entries passed**, including 36 internal
registry tests. The final Linux core also passed: 520 of 521 internal tests,
one optional formatter skip and no failures, giving **7/7 executed Linux CTest
entries passed**. Linux `serializer --version` reports compiler `1.16.0` and
schema language `1.11.0`. Zstd, LZ4 and zlib were disabled because their Linux CMake SDK
configurations were unavailable; this does not qualify Linux compression builds.
The logs are `out/record_linux_focused_tests.log` and
`out/record_linux_core_tests.log`.

## Concrete SDK execution

- Projected readonly record calls/selection: generated consumers compiled and ran
  in all eleven outputs (C++, C, Java, JavaScript, TypeScript, Go, C#, Python,
  Kotlin, Rust and Swift). Coverage includes exact unsigned 64-bit IDs, inherited
  projections, declared/free calls, bounded reductions and a lazy invalid branch.
  C's record fixture deliberately uses storage without dynamic ownership; owning
  C++ strings are separately tested for copy independence.
  Final native tests also exercise projected generic receivers with inherited
  methods whose type parameters use different names, preserving uint64 endpoints.
- Extended construction: generated consumers compiled and ran in all eleven
  outputs. They check dimensioned arrays, trailing defaults/shortened factory
  overloads and fresh record defaults. C additionally checks that invalid
  cardinality preserves both destination and supplied owned input. Checked/open
  input construction is C++ only and unsupported outputs diagnose.
- Numeric concrete SDK consumers compiled and ran in all eleven outputs. They
  cover full-width endpoints, exact literals, overflow/fraction rejection,
  non-finite inputs and predicate failures. Dynamic float32 arithmetic/bindings
  normalize operands before evaluation. Python, JavaScript and TypeScript also
  passed float32 comparison and guard normalization, including explicit inspection
  of NaN/infinity through the finite predicate.
- Custom-ID application-owned examples compiled and ran in all eleven outputs,
  checking scoped/domain keys, duplicate/missing/wrong-domain failures, rebinding,
  retirement/no reuse, validated restore, and all four native codecs. Sources and
  the runner live in `example/custom_ids/`; the C++ managed companion separately
  passed its snapshot, binding, rollback, import, retirement and undo checks.

Maintained runners are `test/projected_records_languages.py`,
`test/extended_construction_languages.py` and `test/numeric_languages.py`.
Python variants are registered in CTest. SDK verification uses installed native
Windows tools and WSL toolchains for Rust, Swift and strict C11 where needed;
TypeScript declarations use the generated JavaScript runtime. Missing tools are
never reported as passes.

## Editor packages

Both `1.1.39` VSIXs were built through `./make.ps1 extension`. Checks passed:
121 VS Code unit tests, 35,110 fresh-compiler navigation checks across eleven
outputs, 567 Visual Studio .NET/Jint navigation checks, and 308 Visual Studio
embedded grammar/boundary checks. Package inspection verifies stable extension
IDs, equal versions, the canonical grammar and schema language `1.11.0` snippet.
This does not claim package installation or interactive IDE-host validation.

## Artifacts and documentation

Local Markdown target checks passed across the changed documentation. Useful
native build/test logs, generated SDK consumers, qualification reports and the
two `1.1.39` VSIX packages are retained under ignored `out/`. Temporary compiler
copies, diagnostic probes and unused source-directory Python bytecode were removed.
Generated output is not committed;
the maintained schemas, examples, manifests and runners remain source files.

## Paged-index study

The [paged indexing document](../design/paged_indexing.md) reviews primary documentation for
SQLite, LMDB/OpenLDAP and RocksDB and proposes a bounded derived-index boundary.
No database dependency, disk backend, paging performance benchmark, hard process
memory bound or larger-than-RAM managed model is implemented or claimed.
