# Five-SDK XML measurements — 20 October 2026

[Pending requirements](../pending.md) · [Qualification runner](../../qualification/portable_xml_benchmark.py)

Compiler/runtime **2.16.0**, schema language **1.20.0**. Selected native Windows
owning XML workloads qualify `JV/CS/GO/JS/TS-Q05`, `Q06` and `Q07`: reproducible
timing/CPU, memory counters, encoded sizes and generated-source sizes. Other
platforms, vocabularies and larger-input qualification remain open. These are
measurements, with no speed threshold or earlier-compiler regression claim.

## Reproduction and independent checks

```powershell
python -B qualification/portable_xml_benchmark.py `
  --compiler out/build/pending-qname-2026-10-20/performance-compiler/serializer.exe `
  --build out/build/portable-xml-measurements `
  --node-dependencies out/build/pending-qname-2026-10-20/javascript/node_modules
```

The build directory must be empty; retain an immutable compiler and its deployed
DLLs. Node dependencies must match the maintained XML TypeScript lockfile.
The executed report is retained locally at
`out/build/pending-qname-2026-10-20/portable-xml-qualified/verification.json`,
alongside exact command logs, generated sources and encoded outputs.
`performance-current-compiler-check.json` in the enclosing task directory
separately confirms all 12 measured generated sources matched that pre-package
integrated compiler byte-for-byte. Both compiler hashes are retained in that
check. Later 2.16 C# package integration changes its generated support; the final
C# refresh below measures that implementation separately.

All five actual SDKs build XML-enabled and XML-disabled models from the maintained
`example/xml/model.serializer`. The three workloads are two small tags,
20,000 repeated escaped Unicode tags, and 20,000 fixed-seed randomized tags.
The seed is `0x584D4C50455246`; values include XML metacharacters, tabs, CR/LF,
Greek text and supplementary Unicode characters. An independent XML parser
checks expanded names, exact attributes, child order and decoded text.

Each workload executes XML encode/decode and ordinary `BINARY_NONE` encode/decode
on the XML-enabled model, plus native encode/decode on the XML-disabled control.
All **90 isolated operations** passed, with **450 positive timing samples**.
All **30 native outputs per workload** are byte-identical across SDKs, modes and
profiles; all **60 XML outputs** pass independent checks. A separate audit matched
all 12 generated-source hashes, all 150 output hashes/sizes and every recorded
throughput calculation.

Each child warms three batches and retains five measured batches. Calibration
uses fresh children, increasing repetitions until the median batch reaches
10 ms; the final minimum observed median was 10,027,000 ns. Timings exclude model
preparation, verification and disk I/O. Process CPU and absolute peak residency
include runtime startup, setup, warmups, all samples, verification and output.
Native controls use a different wire format and establish an ordinary-codec
baseline, rather than equivalent XML provider work.

## Actual SDKs and provider settings

The host is Windows 11 build 26300 amd64, with Python 3.14.3 orchestration.
Java uses Oracle JDK **26+35-2893**, compiling with `--release 17 -Xlint:all -Werror`.
C# uses .NET SDK **10.0.402**, Release `net8.0` on runtime **8.0.32** with warnings
as errors. Go uses **1.26.4**. JavaScript/TypeScript use Node **22.23.1**,
TypeScript **5.9.3**, saxes **6.0.0**, xmlchars **2.2.0** and
`@xmldom/xmldom` **0.9.12**. TypeScript compiles strictly against generated
declarations and executes the same generated JavaScript runtime.

The runner resolves `java.home` and launches its real `bin/java.exe` directly.
Windows Oracle `javapath` launches a child JVM, so counters on that shim do not
describe JVM memory. An initial run exposed this discrepancy and was excluded.
The retained report records the real executable path/hash; an independent
process-image probe confirmed it. Java keeps two explicit finite provider bounds:
`jdk.xml.maxGeneralEntitySizeLimit=4194304` and
`jdk.xml.totalEntitySizeLimit=8388608`. Default JDK 26 provider limits rejected
the randomized escaped-text fixture before typed decoding; those failed probes
were not counted as successful measurements. Generated resource limits keep
their defaults.

## Observed large-workload results

The randomized fixture has **1,659,129 XML input bytes** and **802,318 native
bytes**. XML encode throughput uses each backend's actual emitted bytes; XML
decode uses the common input size. Native throughput uses native bytes.

| SDK | XML encode/decode MiB/s | Native-only encode/decode MiB/s | XML output bytes | Process peak range MiB |
| --- | ---: | ---: | ---: | ---: |
| Java | 18.29 / 17.10 | 96.88 / 169.34 | 1,659,129 | 79.45–743.59 |
| C# | 29.84 / 25.07 | 160.75 / 159.14 | 1,659,129 | 38.91–83.09 |
| Go | 107.93 / 32.36 | 215.18 / 231.91 | 2,340,939 | 12.58–32.77 |
| JavaScript | 16.81 / 8.41 | 48.57 / 102.55 | 1,499,101 | 38.96–138.50 |
| TypeScript | 16.74 / 6.92 | 43.76 / 76.31 | 1,499,101 | 38.61–138.50 |

Peak ranges cover all 18 operations per SDK across the three workloads. They are
absolute OS process high-water working sets, not per-call heap allocations or
hard memory bounds. Java/C#/Go separately retain one untimed allocation sample:
current-thread `ThreadMXBean`, current-thread managed allocation, and process-wide
`runtime.MemStats.TotalAlloc` deltas, respectively. Native provider allocations
may fall outside those counters. JavaScript/TypeScript have no exact allocation
counter in this profile. Heap residency includes retained inputs and runtime
policy, so it must not be inferred from one call's allocation delta.

| SDK | XML-enabled generated bytes | XML-disabled generated bytes |
| --- | ---: | ---: |
| Java | 231,123 | 102,698 |
| C# | 231,315 | 104,755 |
| Go | 275,050 | 152,545 |
| JavaScript | 200,509 | 79,309 |
| TypeScript | 208,755 | 84,637 |

Generated sizes exclude handwritten drivers, provider packages, compiled output
and SDK libraries. TypeScript includes both JavaScript and declaration files;
its JavaScript source hashes match the JavaScript profile. Timing differences
between those two runs measure separate executions of the same implementation.
Concurrent native builds/tests affected host load; these numbers provide no
cross-SDK ranking, performance improvement or complete-operation memory guarantee.

## Final C# package-integration refresh

The final frozen compiler
`out/build/pending-qname-2026-10-20/fifty-compiler-clean/serializer.exe`, SHA-256
`ef49b32f4a37abb493b877275eaf263960e761de9c91b981ee603cfdfae6b6a1`,
was replayed with the same runner and independent workloads:

```powershell
python -B qualification/portable_xml_benchmark.py `
  --compiler out/build/pending-qname-2026-10-20/fifty-compiler-clean/serializer.exe `
  --build out/build/pending-qname-2026-10-20/csharp-clean-measurements `
  --language csharp `
  --node-dependencies out/build/pending-qname-2026-10-20/javascript/node_modules
```

All **18 operations**, **90 positive samples**, **12 independent XML checks**
and **30 output hash/size checks** passed. All outputs exactly match the earlier
five-SDK capture. `verification.json` and `verification-audit.json` in that
directory retain commands, hashes and raw counters. The unchanged runner hash is
`6d5eec91525289d38283937d85736da8a1a3cd832cd37a50991e834306a004e8`;
the minimum median batch was **10,051,000 ns**. This refresh uses SDK **10.0.402**
to compile `net8.0`, executing on runtime **8.0.32**, as above.

The final C# sources contain **279,474 XML-enabled bytes** and **148,316
XML-disabled bytes**. Randomized XML encode/decode measured **25.59/22.12 MiB/s**;
native-only encode/decode measured **177.82/152.25 MiB/s**. XML output remains
**1,659,129 bytes**. Across all 18 operations, absolute process peaks ranged
**38.89-81.28 MiB**. Separate randomized XML encode/decode current-thread managed
allocation samples recorded **29,200,544/19,333,464 bytes**. The counter and host-load
limitations above apply; differences between captures do not establish a speedup
or regression.

This replay includes package integration and the final template whitespace
cleanup. The earlier five-SDK tables describe the original capture; these C#
figures describe the final generated sources and actual refreshed execution.
