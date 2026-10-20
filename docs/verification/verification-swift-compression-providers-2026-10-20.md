# Swift optional compression providers, 20 October 2026

[Swift API](../languages/swift_compression.md) ·
[Core proof](verification-swift-compression-2026-10-20.md) ·
[Reference adapter](../../qualification/swift_compression_providers/README.md)

This record qualifies **SW-Z09/SW-Z10** in compiler/runtime **2.14.0**, schema
language **1.20.0**, on baseline `0928053389fcf237e53f682442510ae19ef5eb5c` plus
working-tree changes. Actual Windows-generated Swift compiles and executes on
Windows and native Linux. It does not assert every provider, platform, nested
operation-context combination or performance gate.

## Generated contract and caller-owned adapter

The default helper remains standalone Swift. Original option initializers keep
their signatures, defaults, trailing closures and function-value types. Separate
initializers with required `provider:` select optional providers. The original
availability overload keeps Zstandard/LZ4 unavailable even when native libraries
are linked. There is no global registry, implicit selection or fallback.

The generated helper's `Provider` creates an exclusive `Context`; `Progress`
reports checked byte counts and completion. Synchronous input/output spans are
bounded to 32 KiB. Generated code validates policy, complete frame extents,
dictionary absence, advertised size/window and exact EOF before decoder creation.
The native adapter validates compressed grammar and checksums. Context cleanup
runs once, after an active step has returned, including when reentry requests
cleanup. Application error objects retain their identity and first-cause fencing.

The separate 0BSD reference shim uses stable C APIs from Zstandard **1.5.7** and
LZ4 **1.10.0**. It queries native level limits; Zstandard defaults to 3 and LZ4 to
0, while the legacy initializer remains level 6. Zstandard emits content checksums
and pledged size. LZ4 emits independent 64 KiB blocks with block/content checksums.
Input also accepts checked linked blocks and legal empty raw blocks. Native LZ4
may buffer/work on an admitted block up to 4 MiB: the 32 KiB context span is not
a total native workspace limit. Logical caps exclude provider workspace and RSS.

Only known native frame errors map to generated `.invalidData`; native history
admission has its resource category. Unexpected native failures retain the
reference adapter's own namespaced error type. No experimental LZ4 API or static
error-code definition is needed. The C module and its deployment remain owned
by the consuming application.

## Actual Windows compiler and SDK

The [maintained runner](../../test/swift_compression_optional_test.py) invokes
immutable compiler SHA-256
`e4ef04ca05811266387a52dd8bac539d1ce33e5c64e3b767853a632e2e989700`,
built through the Windows wrapper and strict MSVC **19.51**. Its 2,324-file
source inventory aggregate is
`cb1ba4e3646db9b09310dcbf3af42c6aacee31e7245d3d2759ef8cd3765eecf7`.
Actual **Swift 6.4**, target `x86_64-unknown-windows-msvc`, compiles with
`-O -warnings-as-errors -swift-version 6`. The C shim builds with
`/MD /std:c11 /W4 /WX`; SDK/runtime paths are supplied explicitly by the host.

| Check | Actual result on each host |
| --- | --- |
| Independent input decisions | **317/317**: 136 Zstandard and 181 LZ4 |
| Independently validated outputs | **24/24**, with exact payload and frame EOF |
| Native interoperability | **6** C++ producers and **48** whole-message/stream C++ consumers |
| Protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING |
| Generated profiles | **5/5**: normal, helper collision, provider/error-name models, binary-only and absent |
| Observed cancellation boundaries | **62/62** |
| Callback/close boundaries | **72/72** |

The common corpus covers standard empty/multichunk frames, every strict prefix,
suffix/concatenation, skippable/legacy formats, dictionary requests, checksum and
size failures, exact/one-tight byte/history bounds, Zstandard descriptor/content
size forms, all LZ4 block maxima, linked history and eleven history boundaries.
The 24 produced frames include eight model/protocol frames and sixteen native
level/empty/multichunk controls. Independent Python **3.14.3** uses Zstandard
**1.5.7** and python-lz4 **4.4.5** with liblz4 **1.9.4**; immutable C++ consumers
use Zstandard **1.5.7** and LZ4 **1.10.0**.

Maintained controls cover initializer function values/trailing closures, native
failure identity, invalid progress, factory failure, context reuse, reentrant
cleanup, seven-byte borrowed reads, bounded partial sink failures, original
transport ownership, independent returned chunks, late-candidate withholding and
inactive first-cause fencing. The absent profile compiles and runs without the
C module, object or native libraries, retaining standalone gzip behavior.

Independent review reproduced premature context cleanup during an active nested
step in the candidate implementation. The repaired helper defers cleanup until
the active step returns; the retained before/after controls report respectively
`cleanupDuringActiveStep=true` and `false`, with exactly one cleanup in both.
The maintained regression checks the repaired behavior. A second before-control
reproduces a global reference-adapter error type colliding with an ordinary model
named `NativeProviderFailure`; the final adapter nests the error in the generated
helper, and the expanded provider-name profile compiles and runs on both hosts.

## Native Linux compilation and execution

The exact Windows-emitted sources, canonical adapter/C shim and common fixtures
are hash-checked and streamed into a private Linux workspace. Actual **Swift
6.1.3** and Ubuntu **GCC 15.2.0** compile and run all five profiles, including the
unlinked absent program. The installed native provider prefix is read-only.
All 317 decisions, 62 cancellation and 72 callback/close boundaries pass in
**85.93 seconds** of native SDK execution. The 24 returned frames pass independent
Python checks and 48 immutable Windows C++ consumers.

The 650-file input aggregate is
`f1e13dc5eec9202146edc7e496fce0703ef6ef17c02e34a133da022ad05da3aa`.
Native static library SHA-256 values are
`f66f71b37a7a0a1f4606af721a8467943aa3c6092b15847995d548bd12922626`
for Zstandard and
`948304b1a75ae6152ffcc317a0ae1746d0d4e7761f8785596827458d44526bf6`
for LZ4. The private workspace is removed after returning evidence. No global
installation, shared build-cache mutation or Linux compiler rebuild is claimed
by this SDK replay.

## Core preservation, provenance and reproduction

The unchanged standalone core runner also passes against the actual new compiler
on Windows Swift 6.4: **974/974** input decisions, **64/64** independent outputs
and **3/3** profiles. It links no optional provider. The complete normal emitted
file is now **81,881 bytes**, SHA-256
`aea778dc4f1db993158d0955da4740034aa3cce900b1af1e902b03e25570990b`;
the larger helper does not change ordinary native APIs or wire identities.
The core runner hash remains
`c05132d428c6c8373101806b80a41c0b0807d3bd73513c433b783e18ffd4845c`.

The optional runner SHA-256 is
`c59447461906de6c4caf0002372906c56132aff190e44a69fc84f77c2216c8fb`.
The generated template hash is
`8835c1c8325a0494537a6bad6c0a18002501df925680cb5c2c5a592b7d5e4ea0`;
the five raw literals total 50,006 bytes and satisfy the runtime literal check.
Canonical adapter hashes are retained in the final report, including Swift
`63e8d553350898355388125b34ef4aabcf4eebb3b6868baba6d3591be5784a6a`
and C shim
`5a0ebb3217d32d4e6633ef37f3a70ff0b5f07acc9168a679d35aaaed11437f13`.

```text
python -B test/swift_compression_optional_test.py --compiler <serializer> --build <ignored-output> --cc <C11-compiler> --provider-include <native-include> --zstd-library <zstd-library> --lz4-library <lz4-library> --provider-runtime-directory <dll-directory-if-needed> --provider-path <python-lz4-directory> --cpp-helper <compression_interop> --cpp-linked-helper <compression_linked_lz4>
```

The optional runner requires explicit SDK/provider prerequisites and is separate
from the default CTest suite. Missing selected prerequisites fail explicitly.
Retained ignored evidence under `out/build/backlog-2026-10-20/` includes
`swift-optional-final.log`, `swift-optional-final/verification.json`,
`swift-core-provider-update/verification.json`, the reviewed cleanup/collision
before-controls, and `swift-optional-linux-final/` with commands, input inventory,
SDK logs, returned frames and `linux_verification.json`. These selected results
do not establish the separate all-language parity or performance gates.
