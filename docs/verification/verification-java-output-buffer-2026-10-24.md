# Java caller-owned output windows — 24 October 2026

Compiler/runtime **2.32.0** adds the opt-in Java `output_buffer_reuse` generation
option. Schema language remains **1.23.0** and both editor packages advance to
**2.0.17**. This is a bounded native-codec slice of `BE02`; Java packed output,
direct native codec streams/channels and native acceleration remain separate.

## Implemented contract

With `[java] output_buffer_reuse = true` or the equivalent CLI override, owning
codecs and readonly translator projections gain
`int encode(Protocol, byte[], int offset, int length)`. Default false retains
existing generated method sets, including application-native overloads. All four
enabled native protocols retain their wire bytes and restricted-protocol gate.

The caller grants exclusive access to a non-aliased destination window. A success
returns the written byte count. Capacity/model failures may modify bytes inside
that window, while outside bytes remain unchanged, including during auto-size
backpatches. Null/range/protocol admission precedes writes. The codec retains no
destination reference after return. Primary message storage is reused; writer,
UTF-8 and other temporary allocations remain.

The private buffer helper does not reserve another public schema type name.
Previously valid `FixedBuffer.java` outer filenames select a different private
helper spelling. Existing owning encoding and package-owned operation accounting
retain their paths. See the [Java API](../languages/java.md#caller-owned-output-windows).

## Verification

This completes **BE02a** within the output-storage contract above. The broader
BE02 packed/stream/channel/native-acceleration requirement remains open.

| Check | Executed result |
| --- | --- |
| MSVC core / constructors | **626/626** core and **25/25** constructor cases pass |
| GCC core / constructors | **623/623** core and **25/25** constructor cases pass |
| Actual JDK 17 / JDK 21 matrices | **99,111 assertions per host** across serializer/google/oracle and restricted BINARY_NONE profiles; offset/end windows, exact/short capacities, reuse, malformed UTF-16, JSON nonfinite values, binary NaN bits and depth failures |
| Compatibility controls | Default method sets, existing application-native overload, strict Boolean/duplicate-option validation, CLI precedence, rejected-option output preservation and an older enclosing filename pass |
| Metadata / projections | Nested total/payload/wire-only size backpatches, magic, live and generic readonly projections preserve source values and outside-window bytes |
| Prior and cross-host wire | Four native protocol hashes match compiler **2.31.2** and each other on Windows/Linux |
| Installed standalone example | **1/1 pass per host**, LANGUAGES NONE and compiler-component discovery, actual installed prefixes, four-protocol nonzero-offset reuse |
| CMake release compatibility | Both generated 2.32.0 package-version files accept earlier same-major releases and reject future or different-major requests |
| Shared / fresh editor checks | **164 tests** and **50,713** fresh bidirectional checks pass, all eleven outputs with Java output-window overloads enabled |
| Native Visual Studio adapter | **1,600** checks pass under the unchanged 256 MiB interpreter limit; interactive-host qualification remains separate |
| Matching 2.0.17 packages | Both VSIX packages rebuild and validate through `./make.ps1 extension`; no installation or publication |
| Initial full Windows release suite | **314/315 pass** in **1,645.70 s** against 2.32.0 (`dfa8b46`); the runtime-literal inventory fails because its first Java token is 69 bytes over the older-MSVC bound. [The 2.32.1 source repair](verification-java-runtime-literals-2026-10-24.md) preserves emitted bytes; the original run remains a failed gate. |
| Repaired full Windows release suite | **316/316 pass** in **1,448.30 s** against compiler 2.32.2 source `5f63e01`, including the Java SDK matrix, runtime-literal repair and subsequent diagnostic regression |

The native-body compatibility fixture initially used profile naming, which is
unsupported for opaque Java code. Its corrected `naming = preserve` case passes;
that prerequisite was not relaxed. An initial example driver also lacked Ninja
on PATH and assumed a separate install component. The corrected driver installs
the actual package layout, selects the available build tool and requests only
compiler discovery. Retained setup failures remain distinct from product results.

Linux uses a pinned, workspace-only extraction of Ubuntu OpenJDK
**21.0.12.1+1-1~26.04.4**; system packages and settings are unchanged. Windows uses
Microsoft OpenJDK **17.0.18+8**. Compilation uses `--release 17 -Xlint:all -Werror`.
The C++ compiler profiles are MSVC 19.51 and GCC 15.2/libstdc++ 15, with the
existing all-provider C++23 configuration.

## Scoped allocation and timing observations

The maintained measurement compares BINARY_NONE encoding of a **4,096-byte**
byte-array payload and a **4,189-byte** message. Each mode uses a fresh JVM,
256 MiB fixed heap and SerialGC, with 3,000 warmup operations and five samples of
2,000 measured operations. Volatile publication keeps encoded arrays observable.
Ordinary and caller-window modes are isolated so their receiver profiles do not
contaminate each other's warmup.

| Host / mode | Median ns per operation | Median requested JVM bytes per operation |
| --- | ---: | ---: |
| Windows JDK 17: baseline-owning | 69,152.4 | 22,808 |
| Windows JDK 17: current-owning | 69,301.1 | 22,808 |
| Windows JDK 17: current-window | 66,960.8 | 2,128 |
| Linux JDK 21: baseline-owning | 75,188.4 | 22,808 |
| Linux JDK 21: current-owning | 72,587.4 | 22,808 |
| Linux JDK 21: current-window | 72,535.5 | 2,128 |

Both hosts retain **22,808** requested JVM bytes per ordinary operation and use
**2,128** for the caller window, a **90.7%** reduction in this workload. Timings are
observations with normal JVM/host variation, not a general speed guarantee.
The allocation counter measures the encoding thread's requested JVM allocation;
it excludes retained caller storage, native/JIT/other-thread allocation and
whole-process peaks. Temporary UTF-8/writer/iteration allocations remain.

The generated source for the maintained fixture is **260,659 bytes** with the
2.31.2 baseline, **263,560 bytes** with 2.32.0 ordinary APIs and **276,352 bytes**
with output windows enabled. Both hosts produce the same sizes under the oracle
profile. These are generated UTF-8 source bytes, including the fixture's included
declarations; they do not measure compiled class size or retained JVM memory.

Reproducible drivers, baseline compilers, logs, wire hashes, SDK output and raw
measurement reports remain under ignored `out/build/java-buffer-overnight/`.
The [SDK regression driver](../../test/java_output_buffer_test.py) and
[measurement tool](../../qualification/java_output_buffer.py) are maintained
source. Local package verification, interactive host tests, installation and
Marketplace publication are distinct states.

Package SHA-256 hashes:

- `serializer-vscode-2.0.17.vsix`: `42807864027a91d6465cbbf943219e82b4819cbac23c660b86839276267df554`
- `serializer-visual-studio-2.0.17.vsix`: `892b09956ed7be1bb678de38aeea9f6a6d1cb48dba8ee0a8e9909e8f3e578a9a`
