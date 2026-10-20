# Optional C compression — 20 October 2026

Compiler/runtime **2.14.0** qualifies **C-Z09/C-Z10** through regenerated C11
headers with explicit, independent Zstandard/LZ4 compile definitions and native
linkage. Disabled providers need no headers or native symbols and reject before
native model encoding, cancellation callbacks or borrowed I/O. Existing native
model functions and wire bytes retain their behavior. See the
[API and deployment contract](../languages/c_compression.md#optional-zstandard-and-lz4).

## Actual execution

| Host | SDK and providers | Final results |
| --- | --- | --- |
| Windows x64 | MSVC 19.51.36260 C11, `/W4 /WX /O2`; Zstandard 1.5.7, LZ4 1.10.0 | 317 input decisions, 24 independent outputs, 48 C++ consumer checks, six native producer exchanges and six strict compile/run profiles pass. |
| Native Linux | GCC 15.2 C11, `-Wall -Wextra -Wpedantic -Werror -O2`; Zstandard 1.5.7, LZ4 1.10.0 | The same complete inventory passes in private Linux-local temporary SDK directories. |

Both runs use actual immutable Windows compiler SHA-256
`9714898052e8be4c158e5de2a256672f173014ae980f35052d8a4ec517ef21be`.
Linux qualification streams its actual generated output and independent fixture
bytes into private native storage, executes the native C SDK there, and returns
output frames for independent checks. This qualification does not claim a Linux
C++ compiler rebuild. Each input bundle hash is retained in its JSON report;
temporary Linux SDK directories remove themselves on completion.

The maintained [runner](../../test/c_compression_optional_test.py) uses the
independent shared [optional frame authoring](../../test/java_compression_optional_test.py)
and [native producer/linked-history checks](../../test/go_compression_optional_test.py).
Input decisions comprise **136 Zstandard** and **181 LZ4** cases, including
four actual C++ one-shot/streaming frames and two native LZ4 linked-history frames.
Independent Python providers are stdlib Zstandard **1.5.7** and python-lz4
**4.4.5**, linked to independent liblz4 **1.9.4**.

The six profiles are both optional providers, neither provider, Zstandard only,
LZ4 only, a generated-helper collision and binary-only native output. The normal
profile executes all framing, native-model and lifecycle controls; collision and
binary-only profiles also execute their actual native model helpers. All profiles
execute provider/level/history admission checks. The normal generated header is
105,108 bytes, SHA-256
`9508978b2d82f1785408433049e6e0c87b399d9d38a7a6d596aa65721f352b02`.

## Independent contracts

Input covers standard magic, malformed/truncated headers and blocks, exact extent,
concatenation/trailing bytes, skippable/legacy frames, zero/nonzero dictionary IDs,
advertised size, exact/reduced compressed/expanded/history limits, all LZ4 block
sizes, independent/linked history and checksum corruption. Zstandard's unused
descriptor bit is accepted; its reserved bit rejects. Non-power-of-two history
limits round down before native decoder admission. Native linked fixtures are
independently shown to require prior-block history.

The **24 outputs** comprise eight typed model frames (four unchanged native
protocols × two optional formats) and 16 empty/multichunk frames at levels
0/1/6/9. Python independently requires exact outer EOF and original plaintext;
both one-shot and streaming C++ consumers check every output. Additional native
provider minimum/maximum levels round-trip, invalid levels reject before user
work, and exact versus one-tight output caps retain the prior result on failure.

The existing C callback controls run with both optional formats: explicit finish,
abandoned close, seven-byte reads, source/sink failures, zero writes, cancellation,
same-wrapper reentry/close, sticky first status, no retried borrowed operations,
unread input and fresh typed-candidate rejection of inner trailing bytes. Output
always enables standard content checksums; LZ4 also enables block checksums.

The [core C suite](verification-c-compression-2026-10-20.md) was replayed after
optional-provider integration against the same immutable compiler on Windows and
native Linux: each passes **974 inputs**, **48 independent outputs** and all five
core/provider-free/collision profiles. Its prior results remain separately dated.

## Sources and retained evidence

| Source | SHA-256 |
| --- | --- |
| C emitter | `d3aacebc9eafc250f21c43e2a1d47a546b5c130d1250ac413a8426fd10a15665` |
| Core runtime | `3998f36080a7c63b3815edfd94c26e5d63e32f05c4d54cdaeb20048f1d9cf2a0` |
| Stream runtime | `39e1678c20f7956933344787e9d7ae7a6b61456567966f6f3bd4ac2b7a17b459` |
| Zstandard runtime | `e80c0484b4f9efa54a456fbd2c8fe366bd51aa8e9cd9a423d10fd4ce24c8d019` |
| LZ4 runtime | `099d591824628fd39c6352fd3af46cb62c5f0bb9a8ae831751289290b44a1977` |
| Maintained optional runner | `a88f43c0c18dfe95d5ae43b39504b17a7cee686fff53fd94ebf7d0cf40339c42` |

Ignored evidence under `out/build/backlog-2026-10-20/` includes
`c-optional-windows-final.log`, `c-optional-linux-final.log`, their retained SDK
directories/JSON reports, `c-compression-{windows,linux}-provider-update.log` and
the immutable compiler/source manifest. Initial Linux setup without the explicit
native provider prefix failed for missing `zstd.h`; final runs use the existing
isolated provider installation and pass. No WSL restart was needed.

Logical byte/history caps exclude provider workspace, allocator overhead, native
callback allocations and process RSS. Complete messages are staged. Cancellation
is cooperative between bounded provider/transport operations. This record does
not close cumulative cross-layer contexts, C-Z11 or performance/platform gates.
The normative framing references are the upstream
[Zstandard format](https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md)
and [LZ4 frame format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md).
