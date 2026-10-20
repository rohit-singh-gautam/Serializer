# Generated Go optional compression providers — 2026-10-20

[Go compression](../languages/go_compression.md) ·
[Default Go verification](verification-go-compression-2026-10-20.md) ·
[Verification index](README.md)

Compiler/runtime **2.14.0**, schema language **1.20.0**. This record qualifies
**GO-Z09/GO-Z10** for the selected Windows and Linux module/provider profiles. Broader
shared-operation, allocation/performance, deadline and platform gates remain
separate; **GO-Z11** is open.

## Selected profile

| Component | Executed selection |
| --- | --- |
| Host | Windows x64 |
| Compiler | Actual refreshed CLI; SHA256 `0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94` |
| Go SDK | Go **1.26.4** windows/amd64 |
| Generated core | Standard library only; module language baseline **Go 1.22** |
| Optional adapter module | **Go 1.25**; pure Go, `CGO_ENABLED=0`, single-worker providers |
| Zstandard | `github.com/klauspost/compress` **1.20.3**, module sum `h1:zWNoFqq/lSuYQRim3dUXlXeA0IqwuZGicdG5ZTfY5j4=` |
| LZ4 | `github.com/pierrec/lz4/v4` **4.1.33**, module sum `h1:GjG1TJ1V4IzKP8L96muuuDNpTwd7D+l2ccXrjAbe014=` |
| Independent Python providers | Python stdlib Zstandard **1.5.7**; python-lz4 **4.4.5** with liblz4 **1.9.4** |
| Native consumers | Actual C++ `compression_interop` whole-message and stream paths |
| Native linked producer | Actual native LZ4 **1.10.0**, separate C++ frame streaming helper; MSVC **19.51.36260.0**, `/W4 /WX /permissive-` |

The SDK ran offline against an explicit module cache. Generated source has no
optional imports. The adapter implementation is caller-owned and selected through
each operation's options; no mutable global provider registry is installed.

## Executed decisions

| Check | Result |
| --- | --- |
| Independent Zstandard corpus, including native C++ producers | **136/136** decisions |
| Independent LZ4 corpus, including native linked-block producers | **181/181** decisions |
| Actual generated owning models, four native protocols × two checksum policies × two providers | **16/16** round trips |
| Independent Python consumption of those Go frames/native bytes | **16/16** decisions |
| Native C++ producers | **6/6**, including checksum-on/off linked LZ4 frames |
| Native C++ consumers, whole-message and stream paths | **32/32** |
| Linked C++ fixtures independently proven to require prior-block history | **2/2** |
| Optional Go test groups | **9/9**, latest retained Windows run **10.489 s** |
| Synchronous callback reentry | **32/32** default-format and **16/16** optional-provider boundaries pass |
| Existing-name collision | New provider-family names select `SerializerCompression_3`; actual SDK compilation/execution passes |
| Default Go core profile | Existing **82** independent input and **16** independent output decisions pass; binary-only, XML independence and existing helper-family collisions compile/run |
| Embedded runtime literals | All literals remain within the **16,380-byte** MSVC token limit |

The shared independent corpus covers empty/nonempty frames, known and unknown
content size, checksums on/off, linked/independent LZ4 blocks, all standard LZ4
block maxima, optional block checksums, dictionary admission, reserved/unused
fields, malformed block syntax, truncation, exact/tight input/output/history
limits, concatenation, trailing data and skippable/legacy rejection. The same
fixture generator is used by the selected Java provider qualification.

Additional actual-provider tests cover valid/invalid levels, tiny expansion
budgets, unavailable admission before native callbacks or borrowed I/O, active
and post-native-callback cancellation, final sink cancellation, later input
read/finish cancellation, original external errors, short sink writes and sticky
failures. Deliberately faulty injected providers cannot publish trailing frames,
changed checksum policy, ignored output-limit failures or corrupt plaintext.

A subsequent callback audit reproduced same-wrapper reentry against the immutable
pre-fix compiler SHA256
`1c7d8663872085f66e1c1d7c0c4bf35427015a57dc073001502b586fddc45c80`:
all **32** shared default-format callback cases failed.
The tracked [shared Go reentry corpus](../../test/resources/compression_reentry_go_test.go)
covers cooperative write/read/finish hooks, borrowed sink/source callbacks and
native encode/decode callbacks. The repair adds active-operation admission and
sticky-state checks after callbacks, including when a callback ignores its nested
failure. The refreshed compiler in the table above passes all **32** default and
**16** optional-provider boundary cases. The complete default-profile suite also
passes against that exact immutable compiler snapshot.

The native linked producer uses 64 KiB linked blocks. Independent Python checks
both complete frames, then proves that at least one compressed block in each
cannot decode without its previous block history. The generated Go provider must
decode those same frames exactly.

## Isolated Linux replay

The same generated Go source, adapter/module pins and independent corpus passed
on **Go 1.26.0 linux/amd64** under native WSL2 Linux storage. Generation used the
immutable **Windows** compiler in the profile table; this run does not claim a
new Linux compiler-source build. All admitted generated/module source hashes
remain unchanged after execution.

| Linux check | Result |
| --- | --- |
| Default core SDK suite | **8/8** groups, **82** independent inputs, **16** outputs; **0.229 s** |
| Optional SDK suite | **9/9** groups, **136** Zstandard and **181** LZ4 decisions; **2.294 s** |
| Actual generated optional native models | **16/16** checksum-on/off round trips |
| Callback reentry | **32/32** default-format and **16/16** optional-provider boundaries |
| Fresh native Linux C++ producers | **6/6** frames, including both linked-LZ4 checksum policies |
| Native Linux C++ consumers | **32/32**, using whole-message and stream APIs |
| Actual prior-block history proofs | **2/2**, native LZ4 **1.10.0** |
| Independent Windows Python consumers of Linux Go output | **16/16** default and **16/16** optional frames/native payloads |

The separate native linked helper compiled with **GCC 15.2.0**, C++20 and
`-Wall -Wextra -Wpedantic -Werror`; CMake was **4.2.3**. It linked the existing
native LZ4 **1.10.0** installation. Existing C++ Zstandard/LZ4 interoperability
executables were used read-only. Source, offline module cache, scratch, helper
build and SDK execution remained in a newly verified Linux temporary directory;
the main native build/cache was not modified.

The **720-file** admitted payload has source manifest SHA256
`92aa1b8f4a26b201a83a002802d17a0b57eeb492b5eaab0f2acd33fdf72c1d8c`.
The collected evidence archive has SHA256
`b7fdc4f572b64a4eba75940732b3fee8aaa005ffde38d5ac2fdeca8492a0251d`.

## Scope and resource limits

Zstandard levels **1–22** map to the maintained Go provider's four speed tiers;
this does not claim identical native-libzstd level selection or compressed bytes.
Zstandard history is conservatively rounded down to a power of two, with a
**1 KiB** minimum. The generated parser admits advertised history before the
single-worker, low-memory decoder is opened; eager whole-buffer decoding is
disabled. LZ4 admits the advertised maximum block before its single-worker
provider allocates the block and linked-history buffers, with a **64 KiB** minimum.

The generated core independently validates complete standard framing and frame
checksums around the trusted provider. Logical byte/history bounds, bounded
provider calls and cancellation polling do not establish a total process-memory
ceiling or preemption inside an uninterrupted provider/application call. Selected
module versions and external provider implementations remain deployment choices.

## Reproduction and retained evidence

The maintained driver is [go_compression_optional_test.py](../../test/go_compression_optional_test.py).
Its [adapter module](../../qualification/go_compression_providers/go.mod),
[lockfile](../../qualification/go_compression_providers/go.sum),
[implementations](../../qualification/go_compression_providers/providers.go),
[SDK tests](../../qualification/go_compression_providers/providers_test.go) and
[native linked producer](../../qualification/go_compression_providers/linked_lz4.cpp)
are tracked. An actual Go SDK, explicit offline module cache, native interoperability
helper and linked producer are mandatory; absence is a failure.

Selected logs, corpus, generated source, module metadata, compiler/DLL snapshot
manifest and exact test/source hashes are retained under ignored
`out/build/backlog-2026-10-20/go-optional/`. The final Windows report is
`windows/verification.json`; `windows/go-test.log`, `windows/go-collision.log`,
the independent/native consumer logs and `native-build.log` retain the executed
decisions. The separate default-profile replay is retained under
`out/build/backlog-2026-10-20/go-core-provider-update/`.
The isolated Linux report, source manifest, actual generated modules, SDK/native
logs and cross-host output checks are retained as `linux-verification.json`,
`linux-replay-manifest.json`, `linux-evidence.tar.gz` and `linux-evidence/` within
the same ignored provider directory. The immutable pre-fix reentry regression is
retained as `go-reentry-baseline.log`.
