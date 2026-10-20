# Java optional compression providers — 20 October 2026

The selected **JV-Z09/JV-Z10** profile passed against actual compiler **2.14.0**
output on baseline `bd2ee2ef9239b65335367bd8de19be377ce84262` plus the uncommitted
changes. Schema language remains **1.20.0**. The compiler SHA-256 was
`3e488eb98ad18f9bc666392e754c8c0b8f3a756fa20ae26a42f6484fa583552a`.

The host was Windows 11 x64, Oracle JDK **26**, CPython **3.14.3**, independent
stdlib Zstandard **1.5.7**, and python-lz4 **4.4.5** linked to liblz4 **1.9.4**.
The native C++ interoperability helper used the existing MSVC Release build,
Zstandard **1.5.7** and LZ4 **1.10.0**. Java source and tests were compiled with
`javac --release 17 -encoding UTF-8 -Xlint:all -Werror`, without provider jars on
the compiler classpath. Runtime execution was on JDK 26; JDK 17 execution is not
claimed by this record.

## Dependencies and implementation boundary

Only the selected runtime classpath supplies these optional jars:

| Provider | Pinned artifact | SHA-256 |
| --- | --- | --- |
| Zstandard | `com.github.luben:zstd-jni:1.5.7-16` | `d84360d39f57a40dc1e26ea2d4bb66da0cb88057ba117edae67431a719057edc` |
| LZ4 | `at.yawk.lz4:lz4-java:1.12.0` | `31c287041eab41f2459e93659d49162eff015a0eb858877e1c5e9dcce1a10c26` |

Official Maven Central jars and matching source jars are retained under ignored
`out/build/backlog-2026-10-20/java-compression-providers/`; no global installation
was changed. Public provider APIs were checked against the pinned source jars.
The [official zstd-jni tag](https://github.com/luben/zstd-jni/tree/v1.5.7-16)
provides the compression context, pledged source size, checksum, bounded streaming
and decode-window APIs. The
[maintained LZ4 provider](https://github.com/yawkat/lz4-java/tree/v1.12.0)
continues the original Java packages. Its frame reader lacks linked-block support,
so Serializer uses its safe pure-Java block decoder/compressor and XXHash APIs
inside an independently bounded frame parser.

Zstandard encoding pledges the complete known source size and uses 32 KiB
input/output provider buffers; the initial unknown-size stream prototype was
replaced before qualification because high levels advertised unnecessarily large
history for tiny input. Decoding validates a single standard frame/header/block
boundary first, then applies the native power-of-two history cap and bounded
output requests. Native contexts close on every operation outcome.

LZ4 frames use 64 KiB independent blocks on output. Input admits the declared
64 KiB/256 KiB/1 MiB/4 MiB block maximum before expansion, validates token lengths
and back references before allocating a regenerated block, and validates descriptor,
block/content checksums and exact frame/content size. For linked blocks it retains
at most 64 KiB and merges that history into the first literal sequence of a
temporary safe-decoder input; match offsets retain their original meaning. The
prototype independently matched **221 blocks**, including eleven tiny/full-history
and literal-extension boundaries. Observed additions were at most **65,793 input
bytes** and **65,536 output bytes**. Its single untuned transformation/decode pass
processed 17,444,640 regenerated compressed-block bytes in 57,319,400 ns; this is
a cost observation, not an isolated benchmark or performance threshold. The final
tracked suite also includes independently validated full frames for those eleven
history boundaries.

## Executed results

The [standalone optional test](../../test/java_compression_optional_test.py) passed:

- **136 Zstandard** and **179 LZ4** mandatory independent input decisions, including
  four native C++ whole-message/stream producer cases.
- Four real generated-model round trips per provider, covering all existing native
  protocols, unchanged ordinary bytes and rejected inner suffixes.
- Exact/one-byte-too-small compressed/expanded encoding/decoding limits, checksum
  on/off frames, active-provider cancellation identity, staged input/output failure
  fences and cancellation after the native decoder returns its candidate.
- Zstandard's full provider-advertised level edges, negative fast levels, rejected
  out-of-range levels before user effects, native deployment failure, non-power-of-two
  frame windows/policy rounding, single-segment sizes, zero/nonzero dictionary IDs,
  ignored unused descriptor bits and rejected reserved bits.
- LZ4 linked/independent blocks at every standard block maximum, raw/compressed and
  empty raw blocks, descriptor/block/content checksum failures, content-size mismatch,
  invalid length/match forms and all eleven history-prefix boundary frames.
- Both providers absent from the classpath: selection fails before serializers,
  cancellation callbacks or borrowed source/sink effects. A deliberately nonexistent
  `ZstdNativePath` with the jar present is also unavailable before user effects.
- Independent Python decoders accepted Java's checksum-on/off frames. Native C++
  whole-message/stream readers accepted all **eight** corresponding consumer cases.

A separate **LZ4-only classpath** passed all **179 decisions**, four native-model
round trips and six direct native exchanges with no zstd-jni jar, establishing
independent optional loading. The no-jar core Java suite passed **957 decisions**,
ordinary model/lifecycle checks and all twelve targeted outer-output-class collision
names, including every new nested provider type, plus the schema-name collision case.
The separate common compression inventory
was replayed against this compiler: **144 model exchanges**, **249 shared corpus
decisions**, 48 independent producer-frame and 12 exact-bound output checks pass.

## Reproduction and retained evidence

After building the compiler and native interoperability helper, run:

```powershell
python -B test/java_compression_optional_test.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --build out/build/backlog-2026-10-20/java-compression-optional-final `
  --provider-jar out/build/backlog-2026-10-20/java-compression-providers/zstd-jni-1.5.7-16.jar `
  --provider-jar out/build/backlog-2026-10-20/java-compression-providers/lz4-java-1.12.0.jar `
  --provider-path out/python-compression-provider `
  --cpp-helper out/build/opc-implementation/test/compression_interop.exe
```

Repeat with `--format LZ4`, only the LZ4 `--provider-jar`, and a separate build
directory for the independent classpath profile. `--format ZSTD` selects its
separate profile. Explicit SDK/jar/helper absence fails; requested checks never
silently skip. `--provider-path` supplies the independent pinned Python LZ4 oracle.
`--cpp-helper` selects the actual native executable and makes its exchanges mandatory.

Retained local evidence includes `result.json`, provider-frame/oracle fixtures,
actual generated source/classes and command logs in
`out/build/backlog-2026-10-20/java-compression-optional-final/` and
`java-compression-optional-lz4-only/`. Result JSON pins the compiler, generated
source, test source, provider jars and native helper by SHA-256 and records
SDK/oracle versions and counts. The initial provider run's generated Java SHA-256 is
`fb183f9543e5ba5079095a3e0921dac5b5ecda99297014765e228168f7469849`.
Core outputs remain in `java-compression-core-final/`; the shared inventory's
JSON/logs remain in `compression-exchange.json` and `compression-exchange/`.

## Subsequent callback lifecycle regression checks

A later audit reproduced a synchronous callback loophole against actual emitted
Java: `Output.finish()` could succeed after its cancellation callback closed the
wrapper. Input callbacks could similarly close or reenter before the next state
check. The new maintained regression failed before the repair, retained in
`out/build/backlog-2026-10-20/java-reentrancy-before.log`.

The repaired wrappers check state before and after policy callbacks, fence nested
operations even when their immediate exception is caught, and stop further
borrowed I/O or candidate publication after close. Output provider polling uses a
private policy retaining the original options, including optional-provider level
and checksum settings. The active outer operation preserves the first checked
I/O, runtime or `Error` object. Later inactive calls retain their established
`IOException` type and attach that first error as cause. No public constructor,
frame, native codec, provider artifact or generated nested-type name changed.

The subsequent actual compiler was captured with its app-local libraries before
other compiler refreshes, SHA-256
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`.
On the same Windows/JDK 26 provider profile, all of these passed:

- The **957** independent core framing fixtures, native-model checks, four-format
  callback close/reentry regressions, original-error tests and all existing
  helper/schema/output-class collision builds, compiled with Java 17 settings.
- Both optional providers: **136** Zstandard and **179** LZ4 decisions, eight
  native-protocol model round trips, four direct C++ producer checks and eight
  direct C++ consumer checks, plus provider/native-deployment absence checks.
- The independent LZ4-only classpath: **179** decisions, four model round trips,
  two direct C++ producer and four consumer checks, without the Zstandard jar.
- The shared C++/Python/Java qualification again: **144** actual model exchanges,
  **249** shared framing/error/byte-bound decisions, **48** independent producer
  frame checks and **12** independent exact bounded-output checks.

Logs and actual outputs are retained in the ignored task directory as
`java-reentrancy-final.log` / `java-reentrancy-final/`,
`java-optional-lifecycle-final.log` / `java-optional-lifecycle-final/`,
`java-lz4-only-lifecycle-final.log` / `java-lz4-only-lifecycle-final/`, and
`compression-exchange-lifecycle.log`, `compression-exchange-lifecycle.json` and
`compression-exchange-lifecycle/`. The shared inventory records source baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus the working changes.

The new generated Java SHA-256 is
`76797d2b3a6e1cf3b9b04925b1cb793ca00162353a1a578777c2887e2cd2ab62`.
The unchanged optional-provider test remains
`6fd6fb628b99ae01db4964ad27fadbfacb00e4633a3e594810ab902a2503049b`;
the repaired core template is
`a37b1fae4016f32975e0754f82b2ab974071207504286eacbe6bc8e2f6264ffb`,
and the maintained core test is
`f770e1ce84334c611b122c6af6a4ac46dbba91eca1e89f248e80d70f84bc941b`.

## Qualification limits

This is a selected Windows/JDK/provider profile. Other Java runtimes, native
deployments, SDK/provider versions, extreme resource profiles and fuzz/sanitizer
campaigns remain unqualified. The LZ4 encoding profile deliberately supports fast
mode and HC levels one through nine; other provider tuning options are outside it.
Byte/window limits and fixed-size staging/provider calls do not establish a hard
heap/RSS ceiling or preempt a single native/block-provider call. Ordinary encoder
callback allocation, provider internals and spare capacity remain outside logical
byte accounting. Z11 remains open for the broader language/provider matrix,
cumulative shared-operation budgets, work/allocation accounting and deadlines.
