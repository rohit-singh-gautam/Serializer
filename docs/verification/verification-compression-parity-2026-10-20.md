# Selected compression parity — 20 October 2026

[Qualification instructions](../../qualification/compression_parity/README.md) ·
[Manifest](../../qualification/compression_parity/manifest.json) ·
[Compression guide](../serialization/compression.md)

This record covers the compression acceptance contract in CP/PY/JV/KT/CS/GO/JS/TS/RS/SW/C-Z11:
shared framing, errors, byte/history limits, cancellation and failure fencing.
The compression manifest follows S35's mandatory producer/consumer inventory and
missing-SDK failure rule. It is a separate inventory from the XML S35 fixtures.
Broader XML/OPC operation contexts, performance and platform matrices have their
own qualification scope.

## Source and artifact identity

The selected source is the uncommitted compiler/runtime **2.14.0** worktree over
`0928053389fcf237e53f682442510ae19ef5eb5c`; schema language **1.20.0** is unchanged.
The complete SDK baseline uses the immutable compiler
`compiler-compression-final-99380733/serializer.exe`, SHA-256
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`.
Its by-value source inventory contains 2,349 files; the Windows aggregate is
`303fc92b9f2932d0b4c5d4572ab27b5be81b304a4c8735d48a0979b994e0c223`.
The shared-window fixture overlay is included in the corresponding normalized
native source aggregate
`363b689147918fb584c724987a3686301d29a23e695e8256a37dfa764effc5bf`.

The standalone C++ consumer uses a separate installed-prefix snapshot: existing
2.14.0 package metadata, the frozen public headers and the immutable rebuilt
static runtime library, SHA-256
`6c3f405b53f20346623940dfb8a0323c86bb2c6d545f17974e009efe415874ba`.
The generated version header is retained. Every file and the overlay origins
are inventoried in `compression-parity-runtime-manifest.json`; the final report
also hashes the prefix. No active native build cache is modified by the SDK
qualification.

The isolated C++ follow-up uses `compiler-zstd-final-978a9afe/serializer.exe`,
SHA-256 `978a9afe1d32480063762dd9d93f4800a5bf6c9d7979f2fd3dd8428621e3d260`,
and its captured static runtime,
`efb0bc4690c9d42399d58ce49f800e0da4fcef3521adc798a5a3025917f27b59`.
Its by-value inventory contains 2,359 files, Windows aggregate
`3a09ff1ef5794fcb02d81d5568a6ba7e40db665d801278bdec404600cd6f9fc2`
and normalized native aggregate
`163c229a705fdee1d7a4bd84259c70c2a326c9e9fce1dad69f4aba9073cba2aa`.
Among compiler/runtime production inputs, only `src/compression.cpp` differs
from the SDK baseline. The three appended native tests and new qualification
files are retained in the complete inventory. The follow-up uses a separate
prefix; it does not overwrite the baseline runtime or reports.

## Actual baseline execution

The complete baseline invocation uses the captured `99380733` compiler and
runtime on Windows x64. It exits with failure for the C++ Zstandard window
decision described below; it does not publish a successful aggregate report.
`compression-parity-final/failed-decisions.json` records exactly that one failed
job and all successful jobs. The complete C++ frame-only before-control probe
independently reproduces exactly one failure among 1,400 decisions.

| Required inventory | Actual baseline result |
| --- | --- |
| Strict SDK adapter builds | All 11 selected profiles compile |
| Producer frames / independent exact-frame checks | 264 / 264 pass |
| Exact / one-tight encoding admission | 792 decisions pass |
| Shared framing/error/limit consumers | Each of the 10 non-C++ profiles passes 1,400 decisions |
| Owning native model exchanges | 2,640 pass: all 11 producers into the 10 non-C++ consumers |
| C++ callback/publication controls | 25 pass |
| Maintained core/optional SDK suites | All 17 pass |

## Final selected parity result

The [C++ refresh driver](../../qualification/compression_parity_cpp_refresh.py)
completes successfully against the captured repaired runtime. It reuses the
executed ten-profile SDK baseline only after verifying its sealed fixture,
adapter, maintained runner/report, provider, deployment and generated-source
identities. Fresh emission with the repaired compiler is byte-identical for
all ten SDK profiles and the C++ model headers. CMake dependency files contain
their output directory and are excluded from the model-header comparison.

| Required inventory | Final result |
| --- | --- |
| Shared framing/error/limit decisions | **15,400 pass**: 1,400 for each of 11 profiles |
| Owning native model exchanges | **2,904 pass**: 11 producers × 11 consumers × 4 protocols × 6 formats |
| Independent producer frame checks | **264 pass** |
| Exact / one-tight encoding admission | **792 pass** |
| Maintained core/optional SDK suites | **17 pass** on the sealed baseline |
| Refreshed C++ callback/publication controls | **25 pass** |
| Refreshed C++ producer frames | **24 byte-identical** to the sealed baseline |

The refreshed C++ consumer actually runs all 1,400 shared decisions and all
264 incoming model exchanges. Its linked runtime and public headers are bound
to the native capture; controlled provider-DLL search locations are checked
before and after the standalone build. The baseline's exact one-failure result
is preserved, rather than rewritten as a successful baseline run. This earns
the selected framing/error/limit parity acceptance for all eleven language
rows; separate context, performance and platform requirements retain their
own scope.

The retained successful report is `compression-parity-refresh.json`, SHA-256
`cb0208f6c37ed6cae567e50d5fc35a12ff926684c3fb6ceb4badce33cf501095`.
It binds the baseline seal
`5d16fe5b32b4d3df8b47ba99437dc8791504dae75092ad91dd837a2bafc09681`
and executed refresh driver
`87ab7fa78d2abf96d4777375490fc124361fe689defd123f160b609b7e52a422`.

The selected SDKs are CPython **3.14.3**, Microsoft OpenJDK **17.0.18+8**,
Kotlin **2.2.21** targeting JVM 17, .NET SDK **8.0.426** targeting .NET 8,
Go **1.26.4**, Node **22.23.1**, TypeScript **7.0.2**, Rust/Cargo **1.99.0**,
Swift **6.4** for Windows, and MSVC **19.51** with warnings treated as errors.
The JSON command/version logs retain the exact builds and paths.

Selected optional deployments are Zstandard **1.5.7** and LZ4 **1.10.0** native
libraries; Java/Kotlin `zstd-jni` **1.5.7-16** and `lz4-java` **1.12.0**;
.NET `ZstdSharp.Port` **0.8.8** and `K4os.Compression.LZ4` **1.3.8**;
Go `klauspost/compress` **1.20.3** and `pierrec/lz4/v4` **4.1.33**;
Rust `zstd` **0.14.0** and `lz4_flex` **0.14.0**; and caller-selected
JavaScript `lz4js` **0.2.0** with Node's Zstandard implementation. The independent
oracle uses CPython Zstandard **1.5.7**, python-lz4 **4.4.5** and its liblz4
**1.9.4**. Provider identities are also covered by the dedicated backend records.

## Common contracts

Every selected profile generates the same owning account model with a stable
unsigned ID and a Unicode label. Every producer emits JSON, binary-none,
binary-integer and binary-string native payloads in identity, gzip, zlib, raw
DEFLATE, Zstandard and LZ4 framing. Every consumer checks the generated exact
native decoder's resulting field values. TypeScript checks real emitted
declarations with its strict SDK and executes the JavaScript implementation.

The common error vocabulary is `invalid_data`, `resource_limit` and `ok`.
Adapters translate documented backend errors into those categories; unexpected
exceptions fail. The shared input corpus includes the existing 974 core and
317 optional inputs, exact and one-too-tight byte caps, conservative DEFLATE
history budgets and the additional advertised-window grammar controls.

The independent [window fixture author](../../test/compression_window_fixtures.py)
produces **44 frames / 64 policy decisions**, SHA-256
`0363f3b735a4525d8ed2cb9d35f871d5bd93d2186bf10483fd50b1f568fd3b34`.
It covers all seven zlib windows from 256 through 16,384 bytes at exact and
one-beyond distances, fixed and dynamic trees, stored-to-fixed history,
literal-only and degenerate valid trees, code-length repeats 16/17/18, and
malformed guarded grammar. Raw valid bodies are independently decoded with
Python zlib; malformed bodies must independently fail there before framing.

Each producer also re-encodes with exact input/output limits and rejects each
one-too-tight limit. For C++, these compression-bound controls use the ordinary
generated native bytes; native owning serialization/publication is checked
separately. Independent Python providers decode every returned frame to its exact
ordinary native bytes and require one complete frame with no trailing input.
Byte equality between different encoders is not required.

## Regressions exposed by the shared corpus

The first actual generated replay exposed acceptance of a zlib stream whose
match distance exceeded its advertised RFC1950 window in C++, Python, Java and
Go. The selected providers accepted the raw DEFLATE body, so the applications
needed to enforce the smaller advertised history themselves. Valid headers with
`CINFO < 7` now receive a bounded grammar preflight before provider expansion;
gzip, raw DEFLATE and ordinary 32 KiB zlib keep their existing provider paths.
The preflight counts expansion, checks distances/extent and cooperatively polls
cancellation. It keeps fixed grammar scratch rather than allocating expanded
history. The general C++ private validator remains separately testable.

Qualification controls also reproduced C++ cancellation after a final sink
callback, publication after a native decode callback, swallowed same-wrapper
reentry, further borrowed reads after source cancellation, and cancellation
between cached input consumption and explicit `finish()`. The repaired C++
operation polls around those boundaries and fences failed wrappers. The input
wrapper's borrowed cancellation callback must remain alive through its last
operation. Premature `finish()` remains a recoverable length check.

The original failures and repaired controls are retained separately under the
ignored task output. SDK harness fixes, such as trimming Windows TSV line
endings and comparing C++ `logic_error` category/message instead of nonportable
MSVC `exception_ptr` identity, are distinguished from runtime defects. User-thrown
pointer-exception identity remains checked where the callback contract allows it.

The final baseline also exposed a separate C++ Zstandard admission gap:
`zstd-123` advertises a 65,536-byte single-segment window but succeeds under a
65,535-byte caller cap. The native contiguous-input decoder can bypass its
`windowLogMax` allocation check in that form. The explicit header admission
repair passes all 1,400 common decisions with the refreshed captured Windows
runtime. Before promotion, the same source also passed strict MSVC and native
GCC **15.2** probes. The three maintained native test bodies reproduce one
failure against the original captured runtime and pass with the repair.

The repair derives the advertised single-segment window from content size and
other windows from the [RFC 8878 descriptor](https://www.rfc-editor.org/rfc/rfc8878.html#section-3.1.1.1.2),
then applies the existing conservative power-of-two/provider clamp before
context allocation. Native full-header validation remains first. The controls
cover every content-size width, exact/one-tight/zero caps, non-power-of-two
windows, huge and truncated headers, reserved/unused bits and dictionary IDs.

## Reproduction and limits

Run the [standalone driver](../../qualification/compression_parity_qualification.py)
from a native developer environment with every SDK/provider path explicit:

```sh
python -B qualification/compression_parity_qualification.py \
  --compiler /path/to/immutable/serializer \
  --config out/compression-sdk-config.json \
  --build out/compression-parity-final \
  --output out/compression-parity-final.json
```

The [configuration example](../../qualification/compression_parity/sdk_config.example.json)
lists the required deployments. Use prepopulated Go/Rust caches; Go proxy and
checksum lookups are disabled, Cargo runs offline with its lock, and .NET uses
empty package sources. Swift needs its matching SDK and runtime paths. The
driver installs nothing. Missing tools, providers, jobs or expected decisions
fail; diagnostic subsets cannot publish a successful full report.

The isolated follow-up can be reproduced with the
[refresh driver](../../qualification/compression_parity_cpp_refresh.py), providing
the original compiler, build directory, configuration and pre-existing baseline
seal, plus the repaired compiler and a separate prefix/configuration. It refuses
other production changes, changed SDK/provider inputs or changed producer bytes;
those require a new complete invocation. A retained negative control passes
the original compiler as candidate and rejects the absent production repair.

The final JSON records source, generated model, runtime/provider, fixture and
exchange hashes, actual SDK versions, commands and maintained runner reports.
The logs, explicit machine configuration, immutable artifacts and reports stay
under ignored `out/build/backlog-2026-10-20/`.

These are selected host/provider profiles. Logical byte/history admission and
bounded callback batches do not promise a physical heap/RSS ceiling or preemption
inside native/caller code. The qualification does not cover arbitrary SDK/provider
versions, browser execution, nested XML/OPC context propagation or throughput.
