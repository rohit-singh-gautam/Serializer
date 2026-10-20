# Swift compression qualification, 20 October 2026

[Swift API](../languages/swift_compression.md) ·
[Compression contract](../serialization/compression.md) ·
[Verification index](README.md)

This record qualifies **SW-Z01–SW-Z08** for the selected standalone Swift profiles
in compiler/runtime **2.14.0**, schema language **1.20.0**, on baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes. The original
Linux SDK proof and subsequent Windows SDK replay are distinguished below. Identity,
gzip, zlib and raw DEFLATE are implemented without additional provider dependencies.
Zstandard/LZ4 remain explicitly unavailable; SW-Z09/SW-Z10, broader SW-Z11
operation-context parity and performance gates remain separate.

## Actual emitted SDK execution

The maintained [runner](../../test/swift_compression_test.py) invokes the real
rebuilt Windows compiler, streams its emitted Swift and independent fixtures into
an isolated native WSL temporary directory, and compiles/executes them using
**Swift 6.1.3**, target **x86_64-pc-linux-gnu**, with
`-O -warnings-as-errors -swift-version 6`. Source/SDK execution does not use the
Windows mount or alter the persistent native build cache. This is actual native
Linux SDK execution of Windows-generated source, not a Linux C++ compiler rebuild
or Windows Swift runtime qualification.

| Check | Actual result |
| --- | --- |
| Independent input corpus | **974/974** expected valid/invalid decisions pass |
| Independent output oracle | **64/64** standard zlib/identity decisions preserve exact native/input bytes and EOF |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callbacks, exact inner consumption and late cancellation |
| Generated SDK profiles | **3/3** compile/run: normal, preserved model/helper collisions and binary-only |
| Borrowed lifecycle | Short reads, true EOF, byte bounds, cancellation, source/sink failures, explicit finish, close/reentry and first-failure checks pass |
| Shared-emitter compatibility | One representative Kotlin module remains byte-identical before/after the Swift integration |

The 957-case independent Java/Python-provider corpus is extended with seventeen
separately authored wire-bit fixtures. These cover stored, fixed and dynamic
blocks; short-message truncations; trailing/concatenated frames; gzip optional
headers/FHCRC/trailers; zlib header/dictionary/checksum behavior; stored length
complements; reserved symbols; over/incomplete trees; length-repeat/EOB failures;
empty-distance valid controls; unavailable backward history; and distance 257
under valid/insufficient advertised zlib windows. Python zlib first checks the
handcrafted valid controls. Nonempty valid frames receive exact and one-tight
input/output admission checks.

The independent output oracle checks sixteen owning model frames: four outer
formats for each of the four ordinary native protocols. Sixteen further frames
exercise random 70,001-byte payloads at levels 0/1/6/9. Thirty-two frames exercise
262,145 repeated bytes and exact 32-KiB-history repetitions at those same levels
and formats. Python's zlib independently decodes every compressed output, checks
EOF with no unused/unconsumed bytes and compares the complete original input.
The runner requires all 64 expected output files; omitted outputs cannot reduce
qualification scope. Ordinary JSON field identity is checked separately.

Additional tests cover unavailable admission before any application callback,
negative bounds/invalid levels, conservative 32-KiB window admission, empty output,
exact/one-tight caps, cancellation at observed work boundaries, seven-byte short
sources, bounded 32-KiB transport requests, a one-byte overflow probe, zero-progress
rejection, explicit finish and abandoned close. Native callbacks and failing
source/sink/cancellation errors retain their original class-error identity.

All four formats exercise policy-driven close during output write/flush/finish,
input read and cached empty-input finish. Source/sink-driven close prevents another
borrowed call or successful publication. Same-wrapper reentry remains fenced when
the callback swallows its immediate exception or subsequently throws another error.
The active outer call preserves the exact first error; later inactive errors retain
it as `cause`. Wrappers are synchronous and add no concurrency synchronization.

## Codec and resource scope

The self-contained encoder emits stored blocks at level zero and bounded LZ77
fixed-Huffman blocks at levels one through nine. Levels select search effort;
they do not promise zlib's compressed bytes, ratio or tuning semantics. The decoder
first validates complete RFC1951 grammar, history, exact extent and actual expanded
count without expanded storage, then expands into the admitted extent. gzip/zlib
framing and checksums are explicit. There are no dictionary or concatenation options.

Logical input/output/history caps do not bound array capacity, fixed codec workspace,
caller codec allocation, process RSS or total CPU time. Whole messages are staged.
Cooperative polling cannot preempt an application callback. This selected proof
does not qualify extreme resources, every Swift SDK/platform, external providers,
fuzz/sanitizer campaigns or a performance pass threshold.

## Provenance and reproduction

The immutable compiler/app-local-library snapshot is retained in
`out/build/backlog-2026-10-20/compiler-c-swift-7c213bc6/`. The executable SHA-256 is
`7c213bc6a141ba936d6ce0edbc849d4bd5f4b258b1aa3557cb55c425574c92cf`;
its CLI reports compiler **2.14.0**, schema language **1.20.0**. The snapshot's
`source-manifest.json` retains the compiler input hashes.

| Input | SHA-256 |
| --- | --- |
| `src/swift_compression_runtime.inc` | `8d32d4dde31c10b0ab012a680a1814268e44e3666ee7235361deaab909d485ab` |
| `src/mobile_writer.cpp` | `a0646379511800d4b3a17309d73408cc9572c62b520b71d4bd4f03c65172371c` |
| `test/swift_compression_test.py` | `c05132d428c6c8373101806b80a41c0b0807d3bd73513c433b783e18ffd4845c` |

The normal emitted Swift fixture is **66,461 bytes**, SHA-256
`cf60046c56a7ae805027aa515e95726ae2584ee2d3ef2dbbdd04f9cd93b098b8`.
Collision and binary-only fixtures are 70,275 and 66,446 bytes respectively; the
report retains their hashes. The three new raw literals are **15,383**, **10,742**
and **8,461 bytes**, within MSVC's 16,380-byte token limit. The global runtime
literal check and scoped whitespace check pass.

The representative Kotlin comparison uses the earlier immutable compiler
`4a4f8604…d8d813` and the new compiler at the same schema/output paths. Both produce
61,369 bytes with SHA-256
`773b6595b47bb039f0980450f556cd8f4a27fd31d264987cc4951462dd701140`.
This is one existing owning profile, not exhaustive backend equality evidence.

```powershell
python -B test/swift_compression_test.py `
  --compiler out/build/backlog-2026-10-20/compiler-c-swift-7c213bc6/serializer.exe `
  --build out/build/backlog-2026-10-20/swift-compression-final `
  --wsl --swiftc /usr/bin/swiftc
```

On a native Swift host, omit `--wsl` and provide its local `--swiftc` or PATH.
Missing SDKs fail explicitly. CTest conditionally registers
`serializer_compression_swift` when `swiftc` is discoverable; this record reports
the direct actual-SDK execution, separately from later aggregate CTest replays.

Retained evidence is `swift-compression-final.log`,
`swift-compression-final/verification.json`,
`swift-compression-final/kotlin-comparison.json` and the immutable snapshot under
ignored `out/build/backlog-2026-10-20/`. Generated test projects/native temporary
directories are automatically removed. Useful compiler/SDK outputs and final
logs remain; obsolete staging probes are removed.

## Subsequent native Windows SDK replay

The same maintained runner also passed on native Windows with **Swift 6.4**
(`swift-6.4-RELEASE`), target **x86_64-unknown-windows-msvc**, assertions enabled,
using `-O -warnings-as-errors -swift-version 6`. The run uses the matching Windows
Swift SDK and runtime environment. It reports **974/974** independent input
decisions, **64/64** independent output checks and **3/3** actual compiled/executed
profiles, including the native codec, bounds, cancellation and borrowed lifecycle
checks described above.

The host imported Visual Studio's amd64 developer shell, set `SDKROOT` to
`%LOCALAPPDATA%/Programs/Swift/Platforms/6.4.0/Windows.platform/Developer/SDKs/Windows.sdk`,
and prepended the matching `Toolchains/6.4.0+Asserts/usr/bin` and
`Runtimes/6.4.0/usr/bin` directories under that Swift installation to `PATH`.
`TMP` and `TEMP` selected a private ignored workspace directory. The runner used
that toolchain's `swiftc.exe`, the immutable compiler below, and build directory
`out/build/backlog-2026-10-20/current-regression/swift-windows`. Execution required
the host's approved SDK-helper launch permission after the restricted environment
denied `swift-frontend`; no installation or network change was made.

This later replay invokes immutable compiler SHA-256
`0e494607dd15d6fd12cdabe96271ee51c31647ae5a9a70192e5628cd821d7efa`.
The runner hash remains
`c05132d428c6c8373101806b80a41c0b0807d3bd73513c433b783e18ffd4845c`.
All three emitted-source sizes and hashes match the original Linux SDK proof:
66,461 bytes for normal, 70,275 bytes for collision and 66,446 bytes for binary-only.
No Swift runtime or maintained-test repair was required for the Windows replay.

The retained evidence is
`out/build/backlog-2026-10-20/current-regression/swift-windows.log` and
`current-regression/swift-windows/verification.json`. This is a direct Swift SDK
qualification of that compiler snapshot; it does not assert a complete current
Windows native CTest result or qualify other Swift installations.

## Optional-provider integration follow-up

The unchanged standalone runner also passes against actual compiler SHA-256
`e4ef04ca05811266387a52dd8bac539d1ce33e5c64e3b767853a632e2e989700`
on Windows Swift 6.4: 974 inputs, 64 independently checked outputs and all three
profiles, without linking optional providers. The normal generated file is now
81,881 bytes, SHA-256
`aea778dc4f1db993158d0955da4740034aa3cce900b1af1e902b03e25570990b`.
The preceding sizes describe the earlier core-only helper. The
[provider proof](verification-swift-compression-providers-2026-10-20.md) records
the new APIs and actual Windows/native Linux optional SDK runs. The core runner
hash remains unchanged; retained evidence is `swift-core-provider-update/verification.json`
under the ignored task build directory.
