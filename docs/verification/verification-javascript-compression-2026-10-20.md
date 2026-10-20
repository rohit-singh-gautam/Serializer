# JavaScript and TypeScript compression qualification, 20 October 2026

[JS/TS compression API](../languages/javascript_compression.md) ·
[Compression contract](../serialization/compression.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

This record closes **JS-Z01–JS-Z08** and **TS-Z01–TS-Z08** for the selected
optional Node provider and shared ESM/declaration profile in compiler/runtime
**2.14.0**. Identity, gzip, zlib and raw DEFLATE are implemented. Zstandard and
LZ4 selectors fail before application callbacks or I/O in this historical core
snapshot. The [later optional-provider proof](verification-javascript-compression-providers-2026-10-20.md)
qualifies additive providers and repeats the complete core suite. The broader
**JS-Z11/TS-Z11** qualification remains open.

## Actual SDK and independent evidence

The maintained [runner](../../test/javascript_compression_test.py) invokes the
rebuilt real C++ compiler, executes its emitted `.mjs` with **Node 22.23.1**
(zlib **1.3.1-e00f703**) and checks the matching `.d.mts` with actual
**TypeScript 7.0.2**, `--strict --noEmit --target ES2022 --module NodeNext`.
The complete final run exits zero.

| Qualification | Actual result |
| --- | --- |
| Independently produced input frames | **974/974 decisions** agree with expected valid/invalid outcomes |
| Independent output oracle | **16/16** Python zlib/identity checks preserve exact native bytes and exact EOF |
| Callback-driven close | **102/102** policy boundaries reject the active operation and prevent later borrowed calls |
| Strict TypeScript consumers | **4/4** profiles pass: normal, model/helper/builtin collisions, private declaration collision, binary-only |
| Executed profile checks | Main SDK suite plus three separately generated collision/binary smokes pass |
| Browser-like isolation | Ordinary import, native codec and identity succeed with zero Node imports; explicit Node provider request rejects when module loading is forbidden |

The independent input corpus combines Python gzip/zlib/raw producers with
explicit wire-bit fixtures. It covers empty/text/binary/fixed-seed random and
highly compressible payloads, truncation, trailing/concatenated streams,
gzip optional headers/FHCRC/trailers, zlib dictionaries/header/checksum, stored
length complements, reserved block/length/distance symbols, invalid/incomplete
dynamic trees/repeats/missing EOB, empty-distance valid controls, available history
and a distance-257 control under valid/insufficient advertised zlib windows.

Every valid input is exercised at exact bounds and, when nonempty, one-tight
input/output bounds. All four native protocols run through owning encoder/decoder
callbacks, including asynchronous callbacks, native unread suffix rejection and
late candidate cancellation. Produced compressed messages independently decode
to the ordinary native bytes; native JSON field identity is checked separately.

Policy/lifecycle checks cover default and explicit providers, malformed options,
levels 0/1/6/9, empty output, mandatory 32 KiB admission before callbacks, short
borrowed reads, a one-byte overflow probe, zero-progress rejection, exact source
EOF, unread expanded input, explicit finish, abandoned close, bounded writes,
no borrowed close/flush, mutable-input ownership before asynchronous yields,
overlap/close while awaiting transport, first-failure fencing and late source,
sink or model cancellation. Original cancellation values survive, including
falsy JavaScript throws and errors carrying a zlib-like error code.

## Confirmed lifecycle repair

The added maintained regression first failed against the previous actual compiler
with **Missing expected rejection**, retained in
`javascript-compression-reentrancy-baseline.log`. A successful cancellation hook
could close/reenter a wrapper between its activity check and later publication.
The final helper checks activity on both sides of every wrapper-owned policy
callback and retains the first nested failure. The 102-boundary sweep includes
identity and gzip provider paths, initial/cached/EOF reads, writes, flush and
finish; callback-driven close cannot report successful completion or trigger a
later borrowed operation. Reentrant failures and close-plus-throw controls also
preserve the first original failure.

## Provider and environment boundary

Only explicit `nodeProvider()` dynamically imports `node:zlib`; ordinary generated
browser modules contain no unconditional Node import. Each compression operation
uses an owned private transform, bounded input batches and capped provider output.
The maintained [Node API documentation](https://raw.githubusercontent.com/nodejs/node/v22.23.1/doc/api/zlib.md)
and [provider source](https://raw.githubusercontent.com/nodejs/node/v22.23.1/lib/zlib.js)
document its streaming/buffered interfaces and concatenated gzip support.
An independent in-memory extent/history validator therefore defines the selected
single-frame boundary; provider read-ahead or `bytesWritten` alone does not.

The isolated Node VM check is a browser-like no-Node module environment, not a
claim of execution in every browser SDK. No Web Compression Streams adapter,
custom provider injection, dictionary support, hard CPU/RSS bound or performance
gate is claimed. Whole messages are staged, and logical limits exclude provider
workspace, caller object allocation and process RSS. TypeScript describes the
same JavaScript runtime rather than implementing a second codec.

## Provenance and reproduction

The final actual compiler SHA-256 is
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`.
Frozen input hashes are:

| Input | SHA-256 |
| --- | --- |
| `src/js_compression_runtime.inc` | `ab01eeff1eaf96d9412ab308b563463de258b9a939a4d9a6e50fd8f44154ec65` |
| `src/js_compression_declarations.inc` | `d704af67c6500a74f29dd08c427f965cea6a0816650498cec1161ea126eff558` |
| `src/portable_writer.cpp` | `b358abb91e73cba258e1239e009ac2c80c03f21474f6f6e43e868efc66767f97` |
| `test/javascript_compression_test.py` | `5567a64c339743111bc95778a43bfdcaa2ed72452a5d634865b2ee86a373e55f` |

The normal emitted JS fixture is **60,890 bytes**. Template raw literals pass the
MSVC 16,380-byte token check. From a workspace with the selected SDKs:

```powershell
python -B test/javascript_compression_test.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --build out/build/backlog-2026-10-20/javascript-compression/final `
  --tsc editors/vscode/node_modules/typescript/bin/tsc
```

CTest registers `serializer_compression_javascript` when Node and the local
TypeScript CLI are available. This record reports the complete direct actual-SDK
execution; later aggregate CTest replays are separate release evidence.

Retained evidence is `windows-lifecycle-compiler.log`,
`javascript-compression-final.log`,
`javascript-compression-reentrancy-baseline.log` and
`javascript-compression/final/verification.json` under ignored
`out/build/backlog-2026-10-20/`. Generated projects/fixtures are automatically
removed by the runner; final logs/report and useful build outputs remain.

## Current core replay after optional-provider integration

The unchanged core runner passes against immutable combined compiler
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`:
**974/974 inputs**, **16/16 independent outputs**, **four strict TypeScript
profiles** and **102 callback-close boundaries**. The normal generated module
is 79,163 bytes. This replay retains original provider defaults and native model
behavior. See `javascript-core-provider-update.log` and its `verification.json`
under ignored `out/build/backlog-2026-10-20/`; the earlier hashes above describe
the historical core snapshot rather than the later optional-provider source.
