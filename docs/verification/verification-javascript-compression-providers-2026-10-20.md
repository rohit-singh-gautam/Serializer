# JavaScript and TypeScript optional compression providers, 20 October 2026

[JS/TS compression API](../languages/javascript_compression.md) ?
[Core SDK proof](verification-javascript-compression-2026-10-20.md) ?
[Compression contract](../serialization/compression.md)

This record qualifies **JS-Z09/JS-Z10** and **TS-Z09/TS-Z10** for the selected
Windows Node and matching TypeScript profiles in compiler/runtime **2.14.0**,
schema language **1.20.0**, on baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes.
Broader JS-Z11/TS-Z11 shared-operation parity, browser SDK execution and general
performance gates remain separate.

## Explicit providers and compatibility

`await zstdProvider()` explicitly resolves Node's public Zstandard transforms and
constants, returning a separate private capability handle. The existing
`nodeProvider()` remains the DEFLATE-only handle. Neither an ordinary import nor
an operation lacking an explicit provider loads Node modules. An older Node-like
module containing only DEFLATE functions remains usable for core formats while
Zstandard admission rejects before callbacks.

The selected runtime is **Node 22.23.1**, with Zstandard **1.5.7** and zlib
**1.3.1-e00f703**. The [versioned public Node API](https://nodejs.org/download/release/v22.23.1/docs/api/zlib.html)
marks Zstandard experimental and identifies its addition in 22.15.0. The adapter
uses public transforms, pledged input sizes, checksums and decompression window
parameters. Zstandard levels are -131072 through 22, default 3.

`lz4Provider(module)` explicitly captures synchronous safe block functions from
the application's selected [lz4js](https://github.com/Benzinga/lz4js) module.
The pinned artifact is **lz4js 0.2.0**; no repository or global npm runtime
dependency is added. Fast level 0 is the only supported compression level.
The adapter constructs and validates standard frames independently of the block
provider. Its private branded handle rejects forged, inherited and mismatched
capabilities before application callbacks. The selected CommonJS module also
passes an actual browser-like VM round trip with no Node imports from the
generated module. This is VM isolation evidence, not a real-browser SDK claim.

The existing TypeScript `Provider` name remains exactly `'node:zlib'` by default.
Generic policy/option types preserve old annotated and inferred properties while
new explicit provider types describe their own literal names. A retained
pre-integration widened declaration fails three legacy consumers with TS2322;
both the original declaration and final compatible declaration pass the exact
same strict TypeScript consumer. The maintained profiles include these controls.
Existing native models, protocols and uncompressed bytes remain unchanged.

## Actual emitted-code execution

The maintained [optional runner](../../test/javascript_compression_optional_test.py)
invokes immutable compiler SHA-256
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`,
executes its emitted ESM with actual **Node 22.23.1**, and checks declarations
with actual **TypeScript 7.0.2**, strict NodeNext/ES2022/noEmit options. This final
run uses the real compiler output without staged runtime replacement and exits zero.

| Check | Actual result |
| --- | --- |
| Independent input decisions | **317/317**: 136 Zstandard and 181 LZ4 |
| Independent output checks | **18/18**, exact bytes and frame EOF; 12 Zstandard and six LZ4 |
| Native interoperability | **6** actual C++ producer fixtures and **36** C++ whole-message/stream consumers pass |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callbacks and unread-suffix rejection |
| Strict SDK profiles | **4/4**: normal, preserved model/helper/builtin collisions, private declaration collision and binary-only |
| Provider cancellation | **168/168** observed boundaries preserve the original exception |
| Callback-driven close | **170/170** boundaries fence the active operation and prevent subsequent borrowed calls |
| Environment admission | Missing/mismatched/forged providers, older DEFLATE-only Node and no-Node VM controls pass |

The unchanged [core runner](../../test/javascript_compression_test.py) also passes
against this same compiler: **974/974** independent inputs, **16/16** independent
outputs, four strict profiles and its 102-boundary callback-close sweep.

Inputs cover checked/unchecked standard frames, empty/multichunk payloads,
truncation, trailing/concatenated input, skippable/legacy frames, dictionaries,
size/checksum failures, all advertised LZ4 block maxima, independent and linked
raw/compressed blocks and eleven linked-history prefix boundaries. Exact and
one-tight input/output/window policies are independently checked. Zstandard
controls include content-size forms, descriptor bits, conservative non-power-of-two
windows and full level admission. Native producers include linked LZ4 frames.

Outputs independently decode with Python **3.14.3** stdlib Zstandard **1.5.7**
and python-lz4 **4.4.5**, liblz4 **1.9.4**. Native consumers use immutable MSVC
Release helpers with Zstandard **1.5.7** and LZ4 **1.10.0**. Their executables and
app-local DLLs are copied privately before replay.

## Native Linux runtime replay

The exact Windows-generated ESM, declarations, fixtures and pinned CommonJS
provider files were streamed into a private Linux-local directory. **Node
24.15.0** executes all **317 decisions**, **170 callback-close boundaries**,
**168 provider-cancellation boundaries** and all four generated runtime profiles.
Its **18 newly produced frames** independently pass Python decoding and **36**
Windows C++ whole-message/stream consumers after return.

The 662-file input inventory aggregate is
`59bfb89e0c688f476666a09eba75cd13425a28024a37a73adb930c587136965f`.
Generated source bytes are unchanged. Only each consumer smoke driver's explicit
Windows provider import URI is replaced with the private Linux provider URI;
per-file before/after hashes document that platform adaptation. This is native
Linux execution of actual Windows-emitted source; no Linux TypeScript execution
is claimed. A separate native compiler equivalence check below qualifies generation. The private Linux directories are removed after successful
return. Logs, command inventory, output frames and `linux_verification.json`
remain in ignored `javascript-optional-linux-final/`, with the adjacent workflow
log. An initial ignored replay driver misread Node's trailing warning as its JSON
summary; retained diagnostics distinguish this harness issue from the passing SDK.

A further read-only check invokes actual native Linux compiler
`b4dddfa3bc20be82c42faba3a2473c698ada780d4512882d2ee054a346790629`
from the combined 2,291-file snapshot. All **eight** ESM/declaration outputs across
four profiles are byte-identical to the Windows-generated SDK inputs. It checks
compiler hashes before/after and removes its private generation directory.
`javascript-native-generation-equivalence.json` retains commands, diagnostics and
per-output hashes; `javascript-native-generation.log` records its successful exit.

## Framing, lifecycle and resource boundaries

Zstandard validation establishes a single standard frame, dictionary absence,
content size, window, block extents and exact EOF before provider expansion.
The provider window cap rounds down to a power of two with a 1 KiB minimum.
LZ4 validates descriptor/block/content checksums, token expansion, history and
exact size/EOF before allocating expanded storage. Linked input retains at most
64 KiB of prior output; advertised block maxima must fit the separate window cap.
Both writers emit a content checksum. External dictionaries and fallback formats
are unsupported.

Private Node transforms use at most 32 KiB input batches and cap output before
copying chunks. Generated checksum updates and borrowed I/O use at most 32 KiB
batches. LZ4 output blocks are 64 KiB; admitted input block calls can reach 4 MiB
plus history. Complete messages are staged. Logical byte/window bounds exclude
provider workspace, allocator spare capacity, native callback allocations and RSS.
Cooperative cancellation cannot preempt a provider or application call.

Original provider, native, transport and cancellation failures retain their
values, including falsy JavaScript throws. Invalid block progress rejects before
a later policy callback can replace it. Promise-returning synchronous block calls
are rejected and their rejections drained without an unhandled rejection.
Borrowed wrapper ownership, explicit finish, close/reentry fencing and fresh
candidate publication are exercised for both optional formats.

A pre-integration diagnostic exposed an unsettled Zstandard write promise after
a corrupt-frame error. Final write completion races the transform's terminal
failure, and a premature close cannot report success. Independent corrupt-checksum
fixtures now complete with rejection, preserving ordinary wire data.

## Provenance and reproduction

| Input | SHA-256 |
| --- | --- |
| `src/js_compression_runtime.inc` | `2ec82ce82ba5c2903040616a016289b5f6205adad22f015cf6a9ff89230a7f53` |
| `src/js_compression_declarations.inc` | `969413849f065c9df2ae566486b69bcbc6d99f1f47d9b111ec1511cc4e8fd14a` |
| Maintained optional runner | `58f3d3241c2b8286cdc4d1267dc75cc10b95a22a716c5168881d9ee909523dff` |
| Pinned `lz4.js` | `16a1df1eb50e74c1cbb0c4f4e947385b4918910a8081cd4bac90318b1052f2bb` |
| Normal emitted `schema.mjs`, 79,163 bytes | `89eb8536397647ee4fab67d9e93a75aa52fd3c94fe9066d143dd467b534708c0` |

The npm tarball is 9,262 bytes, SHA-1
`09f1a397cb2158f675146c3351dde85058cb322f`, integrity
`sha512-gY2Ia9Lm7Ep8qMiuGRhvUq0Q7qUereeldZPP1PMEJxPtEWHJLqw9pgX68oHajBH0nzJK4MaZEA/YNV3jT8u8Bg==`.
The provider remains task-local and caller supplied. Raw C++ literal tokens pass
the MSVC 16,380-byte limit; generation preserves schema/helper collisions.

```powershell
python -B test/javascript_compression_optional_test.py `
  --compiler out/build/backlog-2026-10-20/compiler-current-combined-3533d50e/serializer.exe `
  --build out/build/backlog-2026-10-20/javascript-optional-actual `
  --cpp-helper out/build/backlog-2026-10-20/javascript-zstd/native-helper/compression_interop.exe `
  --cpp-linked-helper out/build/backlog-2026-10-20/go-optional/native-ninja/compression_linked_lz4.exe `
  --provider-path out/python-compression-provider `
  --lz4-module out/build/backlog-2026-10-20/javascript-zstd/lz4js/package/lz4.js
```

Retained final evidence under ignored `out/build/backlog-2026-10-20/` is
`javascript-optional-actual.log`, `javascript-optional-actual/verification.json`,
`javascript-core-provider-update.log`, and the strict compatibility before/after
logs in `javascript-zstd/typescript-compatibility/`. Earlier staged SDK runs are
diagnostics only; the results above use the actual rebuilt compiler.
