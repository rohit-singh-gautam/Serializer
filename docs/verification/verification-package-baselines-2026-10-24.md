# ZIP/OPC process measurements — 24 October 2026

[Qualification requirements](../pending.md#qualification-and-distribution) ·
[Replay commands](../../qualification/package_reproducibility/PERFORMANCE.md) ·
[Native measurements](verification-native-baselines-2026-10-24.md)

Compiler/runtime **2.33.0**, language **1.23.0**, Windows 11 x64 build 26300.
All eleven actual SDKs pass **1,056 independently inspected fresh-process
archives**: stored/deflated ZIP/OPC, forced ZIP64, six inventory permutations and
two repetitions. The complete run takes **513.964 seconds** with one SDK worker
and unchanged sources/compiler/artifacts. Independent ZIP/XML inspection checks
payloads, ordering, names, relationships, metadata and each backend's wire policy.

The subsequent maintained measurement runner passes **220 timed archives**:
five fresh processes for all four ZIP32 stored/deflated ZIP/OPC profiles per SDK.
Every output must match its independently checked canonical baseline and pass the
independent archive oracle again outside the timing. Complete inventories are
mandatory; missing SDKs or changed generated/consumer/provider artifacts fail.

## Reviewable measurements

The [raw five-sample CSV](data/package_process_baseline_2026_10_24.csv) contains
wall time, process CPU, peak working set, encoded/generated size, median wall time,
packages/second and exact archive hashes. Its SHA-256 is
`09cc6241db9bad68df98890f4e2ed92a5620f11558bc8b03b5941b7525c87aeb`.
Qualification CSVs retain LF endings across Windows/Linux checkouts.

| Output | Generated codec bytes | ZIP stored/deflated bytes | OPC stored/deflated bytes |
| --- | ---: | ---: | ---: |
| C++ | 36,791 | 863 / 865 | 3,354 / 3,349 |
| Python | 362,609 | 991 / 995 | 2,210 / 2,207 |
| C | 822,316 | 863 / 865 | 3,364 / 3,359 |
| Rust | 450,717 | 863 / 862 | 2,259 / 2,251 |
| Go | 461,272 | 991 / 1,025 | 2,322 / 2,341 |
| Java | 421,462 | 991 / 993 | 2,299 / 2,294 |
| JavaScript | 338,056 | 991 / 993 | 2,300 / 2,295 |
| TypeScript with shared JavaScript runtime | 357,617 | 991 / 993 | 2,300 / 2,295 |
| C# | 409,362 | 863 / 865 | 2,036 / 2,031 |
| Kotlin | 369,803 | 991 / 993 | 2,299 / 2,294 |
| Swift | 389,070 | 863 / 862 | 2,956 / 2,948 |

Generated sizes exclude consumer templates, manifests and dependencies. Package
sizes reflect explicit legacy backend header/metadata/compression policies;
independent projections agree, while raw byte equality across different policies
is not required. Tiny fixtures can grow under compression because framing and
metadata exceed their payload savings. The selected SDK versions match the
[native baseline](verification-native-baselines-2026-10-24.md); the functional
report additionally pins each SDK's actual provider image, runtime and options.
The initial orchestration selected a nonexistent `*zlib*.dll` name; selecting the
deployment's actual `z.dll` fixes setup without changing its provider.

## Interpretation and evidence

The process baseline takes **65.786 seconds** with one worker. Timings include
runtime startup, fixture parsing, owning Save, producer validation and file I/O.
They measure this complete fresh-process workload, rather than isolated in-memory
Save throughput. CPU is whole-process kernel/user time; short processes can fall
below its resolution. Peak working set is absolute residency, not an allocation
count or physical provider limit. Plain ZIP controls accompany OPC in every SDK.
Forced ZIP64/permutations have the complete functional gate; timed cases use ZIP32.

Raw evidence remains beneath ignored `out/qualification-2026-10-24/`:

- `package-reproducibility/verification.json`: SHA-256
  `dbc3931f01f765ac40c0a9a3db839b22793a3752f7e1f8d6760dc8f86ccd04ba`.
- `package-process-baseline/verification.json`: SHA-256
  `dc78c0503edcaec24df80b295d313645e9c8fbba15de90f0d30f587fdcb73625`.
- Executed measurement runner: SHA-256
  `75f3207ba06fe5b54468a3b59ab094f9e61e41725c28d003244a643c2482c589`.
- Frozen fixture/policy manifest: SHA-256
  `ce462d4022a11d3dbdca2fe0b8efc1e38b04879a36b7d172940095fbfebaac2e`.

Together with native/XML controls, other native protocols, requested C++ allocation
and Python/provider baselines, this completes QA03 for the documented reproducible
Windows workloads. Other chats share the host; no isolated speed threshold, hard
physical memory cap, arbitrary large-archive workload or broader XML/package
parent completion is claimed. README and the integration skill were reviewed;
runtime usage and release versions are unchanged.
