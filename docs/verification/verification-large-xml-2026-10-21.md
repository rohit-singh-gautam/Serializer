# Large owning XML qualification, 2026-10-21

The maintained C++, Python, portable and native/mobile benchmark runners passed
their selected **75,000-tag owning XML profiles on all eleven actual Windows
SDKs**. This is bounded large-message qualification for OPC-R09. It complements
the [official Core corpus qualification](verification-core-conformance-2026-10-21.md)
and the smaller provider, projection and namespace matrices; it does not assert
that every SDK implements the rich imported Core graph or every XML vocabulary.

## Workload and immutable inputs

The input is `example/xml/model.serializer`, with its maintained XML mapping.
There are three workloads: two tags, 75,000 repeated escaped Unicode values, and
75,000 fixed-seed random values. The large workloads have **75,003 logical XML
elements**. Seed `24854788110111302` and the generated fixtures are retained in
the raw reports. Escapes, Unicode, carriage returns and tabs remain present.
Owning codecs construct the complete typed model and stage complete messages.

Windows code generation used the immutable compiler SHA-256
`34e67ca28294018920cc54cd26b4ae6f67ed1f42ef67c4ed7c9aac8ef846cf7b`,
whose development version output is **2.21.0**, schema language **1.21.0**.
These timings must not be relabeled as final-release 2.22.0 measurements.
Reports pin runner/driver, generated source, input, output and provider identities.
The C++ runtime archive SHA-256 is
`5ca58dd924d239e2c3a8507446d3a0c21828e230d29ffffb122cfc7e0e9c1636`;
the copied public header inventory is retained separately in its report.

The Windows host was Windows 11 build 26300, x64. Actual SDKs were MSVC
19.51.36260; Python 3.14.3 with Expat 2.7.3; Java/Javac 26+35; .NET SDK 10.0.402
with the consumers targeting .NET 8.0.32; Go 1.26.4; Node 22.23.1; TypeScript 5.9.3; Rust/Cargo 1.99.0;
Kotlin 2.2.21; and Swift 6.4. XML providers include libxml2 2.15.3, saxes 6.0.0,
xmlchars 2.2.0 and xmldom 0.9.12. Cargo ran offline from the maintained cache.
Every selected SDK executed its generated consumer; there are no source-only
substitutes in the Windows result.

## Bounded policies and controls

Each backend retains its declared finite XML input and output policy. Python uses
an explicit **2,000,000 element/fragment shared-entry budget** with 16 MiB encoded
and text-byte bounds. Its default one-million fragment budget correctly rejected
the random workload; the rejection log is retained. This fragment accounting is
different from the 75,003 logical elements.

Kotlin and Swift use an explicit **200,000 shared-entry budget**, 16 MiB input and
text-byte bounds. Their namespace entries share the resource category with tree
entries. The default 100,000-entry policy correctly rejected the repeated Kotlin
output before this bounded profile was selected. C and Rust retain their default
policies. Java explicitly sets `jdk.xml.maxGeneralEntitySizeLimit=4194304` and
`jdk.xml.totalEntitySizeLimit=8388608` for these escaped-text fixtures; its ordinary
JDK 26 entity defaults reject the larger input before typed decoding. These
settings do not claim identical default policies or hard process-memory caps.

Every emitted XML message is read independently and compared with the expected
model values. XML-enabled and XML-disabled builds produce identical native
bytes. All eleven BINARY_NONE profiles agree on **3,008,262 bytes** for the random
workload and **1,950,022 bytes** for the repeated workload. C++ and Python also
exercise JSON, BINARY_INTEGER and BINARY_STRING controls. Native bytes are another
format; they do not represent equivalent XML provider work.

C++ completed **54 isolated operation profiles / 270 timed calls**; the five
portable SDKs completed **90 / 450**; and C, Rust, Kotlin and Swift completed
**72 / 360**. Each portable/mobile operation retains five calibrated wall samples
after three warmup batches. C++ uses one warmup and five measured calls per
isolated process. Python retains five wall/CPU samples and a separate tracemalloc
call for each operation; its ElementTree baselines retain a generic tree rather
than constructing the same typed model.

## Representative measurements

The following row per SDK describes **random-workload XML decoding**; the size
column records that SDK's emitted XML, which may differ from the independently
constructed decoding fixture. Time is the
median wall duration per call in milliseconds. Residency is an absolute OS process
high-water working set in MiB, not a delta or exact codec allocation. Python's
residency covers its whole benchmark process, including traced-allocation calls;
the other rows use isolated operation processes, including runtime startup,
warmup, setup and validation. The raw reports preserve all workloads, operations,
CPU and allocation-counter categories.

| SDK | XML bytes | XML/native generated source bytes | Decode ms | Process high-water MiB |
| --- | ---: | ---: | ---: | ---: |
| C++ | 6,373,445 | 26,838 / 19,088 | 573.79 | 100.6 |
| Java | 6,223,438 | 235,732 / 103,319 | 1,168.60 | 1,442.6 |
| C# | 6,223,438 | 282,838 / 148,696 | 300.02 | 136.5 |
| Go | 8,782,240 | 280,768 / 152,820 | 1,305.79 | 94.7 |
| JavaScript | 5,623,410 | 207,446 / 79,309 | 1,460.19 | 165.7 |
| TypeScript | 5,623,410 | 215,761 / 84,637 | 1,431.01 | 166.7 |
| C | 9,148,516 | 377,974 / 108,292 | 175.61 | 46.0 |
| Rust | 5,623,410 | 262,393 / 99,192 | 159.81 | 34.2 |
| Kotlin | 9,148,516 | 262,886 / 141,891 | 216.57 | 785.2 |
| Swift | 9,148,516 | 273,560 / 125,262 | 1,220.72 | 71.1 |
| Python | 6,073,431 | 250,980 / 62,789 | 7,404.86 | 619.2 |

Generated source counts describe different deployment units: C++ excludes its
shared runtime; TypeScript includes JavaScript plus declarations; C++/Python
include four selected native protocols while the other SDKs select BINARY_NONE.
They are useful within a backend, not an equivalent deployed-size comparison.
Java and Kotlin include JVM residency. Java/Kotlin thread-allocation counters,
C# current-thread counters, Go process TotalAlloc, Rust requested-allocation
counters and Python tracemalloc have distinct scopes. C, Swift and JS/TS do not
claim an exact allocation counter. Python's random XML decode tracemalloc peak
was 15,980,460 bytes; its whole-benchmark peak commit was 644,866,048 bytes.

Concurrent compiler and qualification jobs ran on this machine. There is no
performance threshold, speed ranking or previous-release regression baseline.
The owning profile does not establish streaming, larger-than-RAM processing,
constant space or allocator-enforced total-process limits. No XML compression
throughput was measured. The Windows C++ runtime has XML/ZLIB/ZSTD/LZ4 available;
those optional compression providers were not selected by these operations.

## Large interruption and recovery checks

For each of the two large inputs, C++ and Python each exercised XML decode and
encode cancellation at poll 1 and poll 1,000, and an already expired deadline:
**six controls per input, twelve per backend**. Each check verifies the expected
interruption. After each input's six-control sequence, unchanged source-model
native bytes and successful fresh decode and encode with matching values verify
recovery. C++ uses the native operation
context; Python uses its host context protocol and a monotonic deadline.

All 24 Windows C++/Python controls passed. The Python controls were replayed
separately with `--interruptions-only`, explicitly retaining zero timing samples.
A further public-callback supplement passed **72 controls** through actual Java,
Go, JavaScript, strict TypeScript, C and Rust consumers: twelve per SDK on the
same two 75,000-tag fixtures. Each of these consumers checks fresh encode/decode
recovery after **every** failure and compares recovered complete native bytes
across all six SDKs. Java/JS/TS exceptions, Go errors, C caller status integers
and Rust `Arc` failures retain their exact caller identity; C and Rust do not
invent a native deadline API. These six adapters use the supported callback to
check an already expired host clock. C uses `clock()` process CPU time; the
others use their host monotonic clock. A callback-observed expired host clock is
distinct from a provider's own deadline facility.

The remaining **C#, Kotlin and Swift public standalone callbacks are now
implemented and qualified**, with twelve large controls each. C# exposes
`Limits.WithCancellation(Action)`; Kotlin and Swift expose
`XmlLimits.withCancellation`, accepting a throwing synchronous host check. The
returned policy copies all existing bounds, retains callback state, preserves its
first original failure across nested traversal, and forwards an existing parent
ledger's admission once. Use a fresh wrapper after failure, keep captured external
resources alive through calls, and do not share one wrapped policy concurrently.
Host-clock expiry uses the same callback rather than inventing another deadline
API. **All eleven Windows SDKs therefore passed 132 unique large interruption
controls**, plus the twelve Linux C++ controls below.

The [separate-library qualification](verification-operation-publication-2026-10-21.md)
also passed actual C# external-assembly, Kotlin separate-JAR and Swift DLL/module
consumers, including copied bounds, stacked callbacks, real prepared providers,
late owning/projection failures, exact sticky error identity, native wire controls
and fresh-wrapper recovery. Same-assembly internal hooks are not used as proof
of these public APIs. Package ZIP
DEFLATE, projection, collaboration, authorization and service behavior are outside
this plain owning XML benchmark.

## Actual native Linux replay

The same maintained C++ runner also passed **54 isolated profiles / 270 timed
calls and twelve large interruption/recovery controls** on GNU C++ 15.2.0,
Linux 6.18.54.1 WSL2 x64 with glibc 2.43, libxml2 **2.15.2** and zlib **1.3.1**.
The native Linux compiler, matching freshly rebuilt runtime archive and public
headers were copied together before running. The runtime configuration is
Release, **XML/ZLIB enabled, ZSTD/LZ4 disabled**. Zlib is an actual additional
linked provider, not a mocked symbol; no compression operation is timed here.

The native compiler SHA-256 is
`5088cee418cb029f8cfd114dba814c3b56ea4cf842ea9f6dcba47c9327244e69`;
the runtime archive SHA-256 is
`355bc93735e87ad6b9bd850b5b9a88c7f33df82e6100fb42119bff2fff79c121`.
Version output is development **2.21.0**, schema **1.21.0**. Generated C++ source
hashes and byte sizes match the Windows frozen image for both profiles.
The report pins the actual libxml2/zlib shared libraries, header inventory and
executables. XML-disabled executables do not link libxml2 or zlib.

Random XML output was **6,373,445 bytes**, and BINARY_NONE remained **3,008,262
bytes** with identical enabled/disabled native bytes. Random XML decode median
wall/CPU times were **782.62 / 782.56 ms**, with **99,299,328 bytes** Linux
`/proc/self/status` VmHWM. This kernel high-water field describes approximate
absolute residency for the executable image, not exact allocation. The first two
wall samples were about 253–270 ms and the final three about 783–860 ms while
other work ran; retain this variance rather than treating one median as a speed
guarantee. Independent XML/model-value checks and fresh-call recovery all passed.

The first WSL startup attempt timed out before launching the script; a subsequent
ordinary startup recovered without shutting down shared WSL. The successful
native run is retained in `linux-cpp/verification.json`. Other SDKs were not
replayed on Linux in this large-workload record.

## Reproduction and retained evidence

The maintained entry points are
[`cpp_xml_benchmark.py`](../../qualification/cpp_xml_benchmark.py),
[`python_xml_benchmark.py`](../../qualification/python_xml_benchmark.py),
[`portable_xml_benchmark.py`](../../qualification/portable_xml_benchmark.py) and
[`native_mobile_xml_benchmark.py`](../../qualification/native_mobile_xml_benchmark.py).
Use an immutable compiler, actual installed SDKs, `--tags 75000`, and the explicit
Python/Kotlin/Swift policies above. Each report records exact generation, compile
and execution commands. The C++ runner accepts `--link-library` for actual
additional Linux provider libraries and pins their hashes.

Successful raw reports and useful binaries/fixtures are retained under the
ignored `out/build/pending-large-xml-2026-10-21/` directory:

- `cpp-complete/verification.json`
- `portable/verification.json`
- `mobile-bounded/verification.json`
- `python-bounded/verification.json`
- `python-interruptions-final/verification.json`
- `linux-cpp/verification.json`
- `public-interruptions/verification.json`
- `kotlin-final-interruptions/verification.json`
- `swift-safe-interruptions/verification.json`

The successful C# large profile and strengthened C no-partial-output replay are
in `wrapper-interruptions/verification.json`. That intermediate report is
explicitly **failed overall** because its first Swift attempt exposed the
provider defect described below; its completed C/C#/Kotlin rows are retained as
individual evidence. The corrected Swift replay passed separately. Kotlin was
replayed after qualifying new `kotlin.Unit`/`kotlin.Throwable` references to avoid
collisions with schema declarations. C# and Swift generated sources were
byte-bridged to the `69c2...` checkpoint, rather than relabeling their earlier images.

`python-default-fragment-limit.log` and
`mobile-default-kotlin-namespace-limit.log` preserve the expected default-policy
rejections. Earlier incomplete runs are not counted as successful profiles.
Missing platform/provider combinations remain explicit qualification work;
successful Windows execution is not proof of execution on Linux or macOS.

[`xml_large_interruptions.py`](../../qualification/xml_large_interruptions.py)
builds six public-callback consumers from the existing benchmark's pinned
generated sources, generates the three additive wrapper profiles from an explicit
`--compiler`, and validates the exact retained large fixture hashes. Its
maintained consumers are in `qualification/xml_interruptions/`; it does not
replace or retime the four measurement harnesses. Use `--benchmark` for the
completed evidence directory, a fresh `--build`, actual `--node-dependencies`,
`--provider-prefix`, offline `--cargo-home` and actual `--kotlin-home`. The supplement retains exact SDK
commands, original logs, generated-source hashes and compiled-consumer hashes.
An initial template-versus-copied-artifact consumer hash selection was corrected
from the retained compiled source files; the original metadata is preserved
separately and no control result or log was relabeled.

## Final-release generated-source bridge

A separate source-only replay with compiler **2.22.0**, SHA-256
`2154768e2a80d6bdfb75ce54e241dd2ece660082f6bfe871c3772bdbac90059d`,
regenerated all **22 XML-enabled/native profiles** with the benchmark's exact
schema, mapping, protocol and naming options. Nineteen generated profiles are
byte-for-byte identical to the measured frozen compiler outputs. Go XML changed
from **280,768 to 281,503 bytes**. Swift XML changed from **273,560 to 275,659
bytes**, and Swift native from **125,262 to 126,878 bytes**. Their measured timings
above still belong to the older frozen outputs. This bridge is not a new timing
run or proof that changed emitted runtime code has identical performance.

This first 2.22.0 bridge predates the additive public cancellation wrappers. Its
copied compiler, generated modules, exact argv and hashes remain under
`compiler-final-source/` and `final-source-sizes/verification.json` in the same
ignored evidence directory.

After the public helpers, Kotlin collision correction and Swift safe-parser fix,
the **dated public-wrapper source checkpoint** is compiler **2.22.0**, SHA-256
`69c2f1a263be717de5338653c8026ddc6ea69c2e35e941092e418dce62be4421`.
All 22 source generations passed again in
`public-final-source-sizes/verification.json`. **Sixteen profiles** remain
byte-identical to the measured frozen image. The six changed profile sizes are:

| Profile | Measured source bytes | Public-wrapper checkpoint bytes |
| --- | ---: | ---: |
| C# XML | 282,838 | 285,574 |
| C# native | 148,696 | 151,432 |
| Go XML | 280,768 | 281,503 |
| Kotlin XML | 262,886 | 264,183 |
| Swift XML | 273,560 | 277,172 |
| Swift native | 125,262 | 126,878 |

C# large controls used the wrapper image SHA-256
`d8d2b683d4490dca4609f9aa70fe39f347dd6c7150df08d3e9f68a07ad00a610`;
Swift corrected controls used
`6d3537f66e40460956b70d45c1cbe47ad11a22af7e8a055485fef30124c7d82b`;
and Kotlin final controls used the `69c2...` image. The actual generated
C#/Swift large consumers are byte-identical to that checkpoint's corresponding outputs.
Their recovered native hashes agree with the Kotlin final controls and the six
other public consumers. Its C++ XML/native source hashes remain identical to
the measured image; these helpers introduce no C++ ABI or payload format change.

### Integrated compiler source replay

The subsequent integrated compiler **2.22.0**, schema **1.21.0**, SHA-256
`dea245fd49925f789d3d4444750d21501bdd726656fcf7372c046dfde4af6499`,
passed another **22-profile source-only replay** after the Java prepared-provider
completion fences were integrated. **All 22 generated profiles are byte-for-byte
identical to the dated `69c2...` public-wrapper checkpoint**, so its six changed
source sizes above also describe this integrated image. Exact argv, generated
hashes, per-profile comparison results and version output are retained in
`integrated-final-source-sizes/verification.json` under the same ignored evidence
directory. The immutable compiler is retained under
`out/build/pending-runtime-providers-completion-2026-10-21/compiler-final/`.

This ordinary owning benchmark mapping does not select runtime XSD validation;
the identical Java output does not establish execution of the newly fenced
prepared-provider branch. Its actual owning/readonly provider checks are recorded
in the [operation-publication qualification](verification-operation-publication-2026-10-21.md).
No timing consumer was rerun and no earlier measurement or control image was
relabeled. The original measured-image comparison remains sixteen unchanged
profiles and six changed profiles, with their measured timings preserved above.

The subsequent Python Save publication-fence fix produces integrated compiler
SHA-256 `82019ab1cc5088111bc7c562a5b29ba5899f660a5143b0120abcf9736dc3a061`.
All **22** exact source-generation commands pass again, and every generated
profile is byte-identical to the preceding `dea245fd` image. These ordinary
benchmark schemas declare no package, so this comparison does not exercise the
fixed package Save branch. Its actual 44-test Python package regression is
recorded in the [publication checks](verification-operation-publication-2026-10-21.md#python-save-publication-callbacks).
The new source-only report is retained in
`publication-final-source-sizes/verification.json`; no measurement was rerun.

## Swift provider failure discovered and corrected

The initial large mid-decode cancellation crashed **FoundationXML 6.4 on
Windows** with an access violation when `abortParsing()` reentered its escaped
text cursor from a delegate callback. The failure log remains in the intermediate
wrapper evidence. Both owning token and retained-tree delegates now preserve the
first error and ignore all subsequent events, including namespace/end/text/CDATA
and completion callbacks, without retaining additional frames or tree state.
The original error is thrown after the synchronous provider returns; no candidate
is published. The corrected twelve large Swift controls and separate-module
provider checks passed.

Provider input is admitted under a finite byte limit before parsing. This safe
cooperative rejection **does not forcibly preempt opaque provider CPU work**:
FoundationXML may finish scanning the already bounded input before returning the
original error. It avoids unsafe callback reentry while preserving transactional
failure, resource ownership and fresh-call recovery. The successful measurement
table remains a dated capture of its original runtime; changed emitted Swift
code has not been relabeled as having those exact timings.
