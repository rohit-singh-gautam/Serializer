# Java heap/direct NIO output - 24 October 2026

Compiler/runtime **2.33.0** adds an independently opt-in Java NIO output overload.
Schema language remains **1.23.0**; both editor packages advance to **2.0.18**.
This extends caller-owned output storage within BE02. Packed output, native
acceleration and direct codec stream/channel APIs remain separate.

Enable `java.nio_output_buffer_reuse = true` to add
`int encode(Protocol, java.nio.ByteBuffer)` on owning codecs and readonly
translator projections. Default false and the independent array flag preserve
existing method sets and native overloads. Writable heap/direct/sliced windows
use the caller's initial position through limit. Success advances its position;
failure preserves position, limit, order and mark while allowing partial inside
contents. Outside bytes, including around size patches, remain unchanged.
Read-only buffers reject before writes, including empty records. Require
exclusive, non-aliased storage and no concurrent model mutation.

The writer retains only a private duplicate during the call. Its constructor
uses a distinct three-argument signature so existing private writer/limits
construction remains unambiguous. The protocol-admission emitter is shared by
all native codec entries without changing its generated spelling. Embedded
runtime literal bounds pass before rebuilding. Primary output storage is reused;
writer/view/UTF-8 and other temporary allocations remain.

This completes **BE02b** within the caller-storage contract above. The broader
BE02 packed/stream/channel/native-acceleration requirement remains open.

| Check | Executed result |
| --- | --- |
| MSVC 19.51 / GCC 15.2 core | **631/631** and **628/628** pass, plus **25/25** constructor cases per host |
| Actual Windows JDK 17 / Linux JDK 21 | **454,826 main matrix assertions per host**, covering three presentation profiles and restricted BINARY_NONE output |
| Independent/default compatibility | Default APIs, NIO-only/array-only flags, strict Boolean values, INI/CLI precedence, existing application-native overloads and older enclosing filenames pass |
| Storage / metadata / lifetime | Exact/short/empty and reused heap/direct/sliced windows, both byte orders, position/limit/mark preservation, read-only admission, outside guards, metadata/magic patches and readonly projections pass |
| Failure / wire controls | Malformed UTF-16, nonfinite JSON, binary NaN bits and depth failures retain categories/messages; four native wire hashes equal 2.32.2 and each other across hosts |
| Installed standalone example | **1/1 pass per host**, compiler-component discovery with LANGUAGES NONE, actual installed prefixes, array and heap/direct nonzero-offset reuse |
| Editor checks | **165/165** shared tests, **50,713** fresh bidirectional checks across all eleven outputs, **1,548** native source/output checks plus **52** named-self checks, unchanged 256 MiB interpreter budget |
| Matching local packages | Both **2.0.18** VSIXs rebuild and validate through `./make.ps1 extension`; interactive-host testing, installation and publication remain separate |
| Full Windows release suite | **316/316 pass** in **1,692.37 seconds** against frozen compiler 2.33.0 source at `1b5bb7d` |

Each normal SDK profile reports 141,113 assertions and the restricted profile
31,487; separate compatibility probes remain additional checks. Compilation uses
`--release 17 -Xlint:all -Werror`. Linux uses the pinned workspace JDK 21 extraction;
system packages and settings remain unchanged.

## Allocation and generated-source observations

The maintained BINARY_NONE workload encodes a **4,096-byte** payload into a
**4,189-byte** message. Each mode runs in a fresh 256 MiB SerialGC JVM after 3,000
warmup operations, with five samples of 2,000 operations. Volatile publication
keeps results observable. Counters measure the encoding thread's requested JVM
allocation, excluding retained caller storage, native/JIT/other-thread allocation
and whole-process peaks.

| Mode | Windows JVM bytes/operation | Linux JVM bytes/operation | Generated UTF-8 source bytes |
| --- | ---: | ---: | ---: |
| 2.32.2 ordinary owning | 22,808 | 22,808 | 266,398 |
| 2.33.0 ordinary owning | 22,808 | 22,808 | 269,414 |
| 2.33.0 array window | 2,128 | 2,128 | 282,739 |
| 2.33.0 heap NIO window | 2,192 | 2,192 | 287,614 |
| 2.33.0 direct NIO window | 2,200 | 2,168 | 287,614 |

NIO uses about **90.4-90.5% less requested JVM allocation** than ordinary owning
encoding in this workload, with **40-72 bytes** more than the array window for
its private view. Caller direct memory is allocated before measurement and is
excluded. Temporary writer/view/UTF-8 work remains. Source sizes include the
maintained fixture's declarations and are distinct from compiled class size.

Initial timing samples ran alongside other qualification and varied; raw reports
remain in `*-benchmark-concurrent/`. The final replay below ran each host
sequentially after other task qualification finished. Medians are observations
with JVM/host variation, not a general speedup or regression guarantee. The Linux
direct-mode allocation sample also changes from 2,200 to 2,168 bytes/operation
between runs; JVM scalar replacement is not a fixed allocation contract.

| Mode | Windows median ns/operation | Linux median ns/operation |
| --- | ---: | ---: |
| 2.32.2 ordinary owning | 69,929.2 | 84,952.9 |
| 2.33.0 ordinary owning | 72,716.4 | 82,963.4 |
| 2.33.0 array window | 73,082.6 | 73,729.0 |
| 2.33.0 heap NIO window | 66,943.2 | 77,555.6 |
| 2.33.0 direct NIO window | 80,726.1 | 73,784.2 |


Drivers, prior 2.32.2 compilers, raw reports and verification logs remain under
ignored `out/build/java-nio-overnight/`. The maintained
[SDK regression](../../test/java_output_buffer_test.py),
[measurement tool](../../qualification/java_output_buffer.py) and
[runnable example](../../example/java/output_buffer/README.md) cover this API.

Package SHA-256 hashes:

- `serializer-vscode-2.0.18.vsix`: `74f2fceb8e12293a6d1d42274d35b67312a1022711ffa4f1f3e3e1c00c6b9929`
- `serializer-visual-studio-2.0.18.vsix`: `f56759f2c4c51968ace7eedc84e26c59598016365c61071dd180b3d20a2ed337`
