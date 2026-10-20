# Package reproducibility qualification — 21 October 2026

[Pending requirements](../pending.md) · [Inventory and replay commands](../../qualification/package_reproducibility/README.md)

Compiler/runtime **2.19.1** completes `XML-B23` for the selected Windows and Linux
package writers. The new qualification preserves existing backend ordering,
metadata and compression defaults; it does not change generated runtimes or
normalize different backends to one archive representation. Schema language
**1.21.0** and both editor extensions **2.0.10** are unchanged.

## Executed matrix

The independent manifest fixes literal payloads, names, media types, relationship
IDs, types, targets and modes. Each of eleven actual SDK consumers runs four
profiles (`zip-stored`, `zip-deflate`, `opc-stored`, `opc-deflate`), ordinary and
forced ZIP64 output, six input permutations and two fresh producer processes.
That is **96 archives per backend, 1,056 per host and 2,112 across both hosts**.
Both full inventories pass; missing SDKs are failures, and a language subset can
report only a diagnostic result.

The producer adapters permute caller inputs without sorting them. The ZIP
fixture distinguishes UTF-8, Unicode scalar and UTF-16 ordering using BMP
U+E000 and supplementary U+10000 filenames. The OPC fixture uses ASCII part
paths and source-local IDs, UTF-8 payloads, relative and absolute internal
targets, literal query/fragment suffixes, percent-escape case, inert external
targets and combining marks after ASCII delimiters. Empty payloads are included.

The oracle imports no generated ZIP reader, writer or metadata emitter. It
parses local and central headers, signed descriptors, ZIP64 extras/end records,
CRCs and complete raw DEFLATE streams. It checks payload bytes, physical ordering,
timestamps, flags, attributes, comments, permitted extras, content-type declarations,
raw relationship targets and closure against the independently frozen manifest.
Its authored negative controls reject corrupted headers/CRCs/offsets, trailing
data, comments, unfinished empty DEFLATE streams and mixed classic/ZIP64 end
sentinels. Existing large count/offset tests remain separate.

Every archive for a given backend/profile/ZIP64 choice is byte-identical across
the six permutations and two fresh processes on its host. Cross-host comparisons
require the checked metadata/uncompressed projection to match for all **1,056
pairs**. Complete archive bytes match for **936 pairs**:

| Output | Archives per host | Byte-identical Windows/Linux pairs |
| --- | ---: | ---: |
| C++ | 96 | 96 |
| Python | 96 | 24 |
| C | 96 | 96 |
| Rust | 96 | 96 |
| Go | 96 | 96 |
| Java | 96 | 96 |
| JavaScript | 96 | 96 |
| TypeScript | 96 | 96 |
| C# | 96 | 48 |
| Kotlin | 96 | 96 |
| Swift | 96 | 96 |
| **Total** | **1,056** | **936** |

The other 120 pairs differ in compressed representation and dependent size/offset
fields while retaining the checked projection. Stored ZIP bytes must match across
hosts. Compressed archive equality is required only when complete provider pins
match or the source-bound, self-contained Rust/Swift implementation identities
match. Observed equality with different provider pins is recorded without making
a general compressed-byte equality promise. `opc-stored` can contain compressed
infrastructure metadata under the backend's existing policy.

## SDKs, provider pins and compatibility

| Output | Windows SDK/runtime | Linux SDK/runtime |
| --- | --- | --- |
| C++ | Strict CMake-built C++20 consumer/MSVC | Strict CMake-built C++20 consumer/GCC |
| Python | Python 3.14.3 | Python 3.14.4 |
| C | MSVC 14.51 C11 | GCC 15.2 C11 |
| Rust | Cargo 1.99.0 | Cargo 1.93.1 |
| Go | Go 1.26.4 | Go 1.26.0 |
| Java | JDK 26, Java 17 compilation target | OpenJDK 25.0.4.1, Java 17 compilation target |
| JavaScript | Node 22.23.1 | Node 24.15.0 |
| TypeScript | TypeScript 5.9.3/Node 22.23.1 | TypeScript 5.9.3/Node 24.15.0 |
| C# | .NET SDK 8.0.426/runtime 8.0.32 | .NET SDK 8.0.426/runtime 8.0.32 |
| Kotlin | Kotlin 2.2.21/JDK 26, JVM 17 target | Kotlin 2.2.21/OpenJDK 25.0.4.1, JVM 17 target |
| Swift | Swift 6.4 | Swift 6.1.3 |

Java compiles with all lint warnings treated as errors. Kotlin compiles generated
library and consuming code as separate modules with warnings treated as errors.
TypeScript compiles a strict external consumer without `skipLibCheck`. Native
consumers likewise use their recorded strict compiler options. Actual command
lines, diagnostics, generated source hashes, consumer images and provider/tool
artifacts are retained in each language report.

Provider records retain actual implementation/version/options and binary/source
fingerprints, with the supplemental C++/C# provider checks distinguished below.
C++/C select zlib 1.3.2 on Windows and 1.3.1 on Linux; Python selects
1.3.1.zlib-ng and 1.3.1 respectively. Node reports zlib 1.3.1-e00f703 on both hosts,
with the actual Node images pinned separately. JVM records retain the selected
runtime images/native ZIP providers and linked zlib when present. Java uses level
6; Kotlin preserves its existing default level. C# pins its selected compression
assembly, native shim and actual linked Linux zlib. Rust/Swift bind their
self-contained compressor identities to generated implementation bytes and fixed
options. Matching version strings alone do not establish compressed eligibility.

Public ZIP64 controls retain their different layouts. C++ forces payload entry
options while retaining infrastructure/directory defaults; other selected writers
use their existing operation controls. The oracle checks these policies instead
of requiring one forced-ZIP64 layout across all outputs.

Python's OPC producer uses a public generated schema-bound facade and Save.
Python has no public generic ZIP writer: its plain ZIP producer is explicitly a
diagnostic use of the generated private physical helper. This record adds no
public Python ZIP API. The C# inventory selects .NET 8; a .NET 10 preflight's
unfinished empty raw DEFLATE stream is retained as an excluded provider
checkpoint, not a qualified result.

## Provenance and final evidence guards

Evidence is retained under ignored
`out/build/pending-reproducibility-2026-10-21/`. The immutable Windows compiler is
`compiler-first/serializer.exe`, SHA-256
`25d54043ee5e834e08be6f2524841bb77be15a9aa28326753333a38c62c13c49`.
The immutable Linux compiler is `compiler-linux-first/serializer`, SHA-256
`5da6e8685489efd67da71cbc3a7b7c908e77874bdd54c0905858d50e581832e1`.
Their maintained compiler/runtime source manifest covers 406 files and has
SHA-256 `31c697b9567557fa16b01bc2084598345881a6d222a74229bbe144e7042a2b83`.

The actual full-matrix reports are immutable:

| Report | SHA-256 |
| --- | --- |
| `windows-second/verification.json` | `2cf899d227eacf2354d160450c67574490b26499379419441aaa659e95eb2acb` |
| `linux-second/verification.json` | `015049f4d487aec2d0df852cbf27b68231b830a4a0bcd6a7a4283ff4a5a6bc04` |

Both reports record unchanged source snapshots and compiler images for their
executed matrices. After those formal runs, the driver and managed provider
capture helper were hardened: cross-host comparison now requires complete,
unique inventories and identical normalized source snapshots; source guards also
include the imported Node dependency preparer, its two manifests and the two
Rust Cargo inputs. Linux C# provider capture additionally pins the zlib linked by
its native compression shim. These are evidence-guard changes; codecs, producer
sources, manifest and oracle remain unchanged from the passing formal matrices.

The full two-host matrices were not rerun under these later guards. Instead,
their existing archives are independently reinspected and their retained sources,
consumer images and provider artifacts are checked. A focused Linux C# replay
runs all **96 archives** with the final source/provider guards and produces bytes
identical to the corresponding original formal run. Its report is
`csharp-provider-final/verification.json`, SHA-256
`a185d0a35de1fe18255f19c9bffc107ebf70268394733f29c23ae5bf32789218`.

The formal C++ reports retain the producer executable and actual zlib
version/options, but omit the separately loaded zlib binary. Their differing
cross-host pins already make compressed equality ineligible. Supplemental C++
replays pass on each host with the actual linked library selected through
`--cpp-provider-library`; all 96 archives per host are byte-identical to the
immutable formal output hashes. Their retained reports are:

| Supplemental report | SHA-256 |
| --- | --- |
| `cpp-provider-final-windows/verification.json` | `daea2e40c47ba568ae33291f75e02e6a1ca15d2c194730a07d80ebf034993ba0` |
| `cpp-provider-final-linux/verification.json` | `c88b5af6b44e009e0499f53fc14c296c8e7783a83b2b9a55fa6f9d23c03fae36` |

Together with the C# replay, this supplemental scope is **288 additional
archives**; the formal inventory remains 2,112. The final integrated audit
rechecks all 2,112 formal archives, 288 supplemental archives and their retained
source/image/provider hashes.

Final Windows and Linux CTests each pass **6/6**: core, runtime literals, compiler CLI,
package-version compatibility, the independent oracle and qualification guards.
Separate Windows/Linux wrapper configurations also pass with XML/tests enabled
and zlib disabled: the zlib-dependent C++ probe is absent, while the independent
oracle and evidence-guard CTests still pass **2/2** on each host. This preserves
the optional-provider configuration contract.
The final `integrated-verification.json` passes, SHA-256
`322407ae6691ff0c2c007b4cf5ba43d4563dfe445533c9871e876f3e88045c4c`.

This qualification completes the ordering/metadata/provider gate `XML-B23`.
It makes no lexical XML/C14N/signature, broad OPC/3MF conformance, large-archive
stress, CPU/RSS performance or provider-internal hard-resource claim. No release
publication or editor package change is part of this batch. `.gitignore` already
covers the task output and dependency caches; useful SDKs, immutable reports,
negative checkpoints and replay logs are intentionally retained there.
