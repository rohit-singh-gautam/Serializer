# Python XML measurements, 20 October 2026

[Benchmark runner](../../qualification/python_xml_benchmark.py) ·
[XML codecs](../serialization/xml_tokens.md) · [Backlog](../pending.md)

This record qualifies the selected **PY-Q05/PY-Q06/PY-Q07** measurement tasks for
compiler/runtime **2.14.0**, schema language **1.20.0**. It records actual owning
XML CPU/wall time, traced allocation, encoded size and generated-source size.
It establishes no performance threshold, hard process-memory bound or result
for another language, platform, provider or schema graph.

Actual compiler SHA-256 is
`309eb574e5cc10ff6a0d2b65e6f069da5b2add267fb831a657ce60139bc4e14f`.
Python **3.14.3**, 64-bit AMD64, runs on Windows 11 build **26300** with Expat
**2.7.3** and ElementTree **1.3.0**. The maintained benchmark uses the public
XML order schema/configuration and a separate native-only generation of that
same schema. No dependency installation is required.

## Reproducible workloads and independent checks

Each operation warms once and retains five separate wall-clock and process-CPU
samples. A separate call after collection records `tracemalloc` current and peak
bytes. Timing excludes generation, import, input construction and file I/O.
Three workloads contain two small tags, 20,000 repeated escaped/Unicode tags, or
20,000 independently generated 32-character tags. The last uses fixed seed
`0x584D4C50455246`; the retained XML hashes fix its actual input values.

ElementTree independently checks expanded names, order, the unqualified attribute
and exact Unicode/escaped values. Its independent output also passes generated
owning decoding. The provider serializer emits a numeric reference for carriage
return so XML line-ending normalization cannot change that value. Both generated
profiles produce identical bytes for JSON, BINARY_NONE, BINARY_INTEGER and
BINARY_STRING; each protocol decodes back to the same canonical XML.

| Workload | XML bytes | XML encode median ms | XML decode median ms | Encode traced peak bytes | Decode traced peak bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Small, two tags | 169 | 0.475 | 0.296 | 9,730 | 18,820 |
| Repeated, 20,000 tags | 1,040,137 | 1,312.876 | 1,668.801 | 24,964,892 | 4,211,137 |
| Fixed seed, 20,000 tags | 1,619,122 | 1,843.360 | 2,885.473 | 44,446,919 | 4,772,109 |

The repeated workload's median XML process CPU is **1,140.625 ms** encoding and
**1,500 ms** decoding; the fixed-seed workload records **1,765.625 ms** and
**2,500 ms**. Small operations can report zero process CPU at this host's timer
resolution. All raw samples and throughput calculations remain in the report.
Other verification jobs ran concurrently; these observations are a reproducible
workload checkpoint rather than an isolated machine regression threshold.

ElementTree decode/encode baselines are **32.734/107.220 ms** for repeated data
and **106.961/167.456 ms** for fixed-seed data. These operations retain a generic
tree and perform different work from typed generated codecs; their timings are
not interchangeable. Native-only BINARY_NONE encode/decode baselines are
**37.392/79.384 ms** and **37.140/73.315 ms** respectively. With XML enabled, the
same ordinary native paths record **33.302/63.027 ms** and **34.481/58.662 ms**,
with exact byte equality. Noise prevents interpreting these differences as an
optimization or universal absence of overhead.

## Sizes and allocation limits

| Workload | JSON bytes | BINARY_NONE bytes | BINARY_INTEGER bytes | BINARY_STRING bytes |
| --- | ---: | ---: | ---: | ---: |
| Small | 62 | 24 | 29 | 48 |
| Repeated | 560,058 | 520,022 | 520,027 | 520,046 |
| Fixed seed | 948,546 | 802,318 | 802,323 | 802,342 |

The generated XML-enabled Python module is **220,616 bytes**, SHA-256
`2dca0d943c7f42d80e634cc4226e74e5c43c5064bcd56c50dc1e6d1b58fea053`.
The native-only module is **55,275 bytes**, SHA-256
`9bd51ca51940e2c3e96ee45460b7aceeafd13616bbed0a7a1567440c313f7333`.
This selected XML support difference is **165,341 source bytes**. Both profiles
include their ordinary compression helper; it is not charged solely to XML.

`tracemalloc` reports Python allocations traced during one operation. It excludes
untraced native parser workspace, allocator metadata, interpreter/import setup
and total process RSS. The measured peak includes still-live return storage and
temporary buffers. These owning codecs stage complete messages; this record
does not claim constant-memory streaming, a hard allocation ceiling or support
for models larger than available RAM. The sizeable encoding peak remains visible
in the results rather than being presented as bounded by encoded output size.

## Reproduction and retained evidence

```powershell
python -B qualification/python_xml_benchmark.py `
  --compiler out/build/backlog-2026-10-20/compiler-go-opc-quota-309eb574/serializer.exe `
  --build out/build/backlog-2026-10-20/python-xml-benchmark
```

The runner SHA-256 is
`97ace94e47ccbafa24b6339bab05009564265e7b9e12288dca9828ce4dae0e2f`;
the final report SHA-256 is
`d11ab9e6283ea400af1b1bed597eb4eca1965ff999c9a4ac9f96671b4cb97ae2`.
Ignored retained evidence includes `verification.json`, three exact XML outputs,
generation logs and the top-level `python-xml-benchmark.log`. Generated import
scratch is removed automatically. The report includes exact commands, SDK/provider
versions, source/input/output hashes, all samples and measurement limits.
Broader QA03/XML-B08 performance and other backend measurements remain open.

## Final combined compiler replay

The unchanged maintained runner also passes against actual final combined compiler
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`.
Its complete report is retained under
`out/build/backlog-2026-10-20/python-xml-benchmark-final/verification.json`, SHA-256
`107bd5cd1aff4a0efb008296b6efe40261c6408a2230b51aaec35c57c186ee6d`.
All three XML hashes and all four native protocol sizes/hashes per workload are
identical to the earlier measurement. Traced peaks also remain identical.

The generated XML-enabled module is now **228,130 bytes**, SHA-256
`fa5b3a62be428eea396551b8821b5abb98fc034ad46d72cda393a2bbd36cf041`;
the native-only module is **62,789 bytes**, SHA-256
`73a0c02c89a950757ccfb6eee4cb0343a4e4120ba9aa0e7de273c935e356ffbe`.
The new bounded small-zlib-window grammar helper adds 7,514 bytes to each profile;
the selected XML source difference remains **165,341 bytes**.

| Workload | XML encode wall median ms | XML decode wall median ms | XML encode CPU median ms | XML decode CPU median ms |
| --- | ---: | ---: | ---: | ---: |
| Small | 0.448 | 0.562 | 0 | 0 |
| Repeated | 2,187.482 | 2,874.919 | 1,312.500 | 1,546.875 |
| Fixed seed | 6,241.707 | 4,678.783 | 1,593.750 | 2,250.000 |

The larger wall/CPU separation records concurrent native and SDK compilation on
the same machine. It reinforces the stated limit: these workload checkpoints
are not isolated performance regression thresholds. Reproduce with the final
compiler directory and `--build out/build/backlog-2026-10-20/python-xml-benchmark-final`.

## Native Linux reproducibility replay

The **unchanged** maintained Python runner also passes all three workloads and
four native protocol baselines with native Linux compiler
`3b39d98189350b0feebdf2ce59cae51b1aa05a8c6c037d7ff86ce12eb2a1ee78`,
Python **3.14.4**, GCC **15.2.0**, Expat **2.7.4** and ElementTree **1.3.0**.
The complete measurement runs in a private Linux-local tree without installing
dependencies or changing the main native cache. All **three XML output hashes**,
all **12 native output hashes** and both generated-module hashes exactly match
the final Windows `99380733` replay above. This qualifies **PY-Q08** for these
selected Windows and native Linux profiles, with the same workload and provider
limits as the earlier measurements.

| Linux workload | XML encode median ms | XML decode median ms | Encode traced peak bytes | Decode traced peak bytes |
| --- | ---: | ---: | ---: | ---: |
| Small | 0.269 | 0.440 | 9,730 | 19,020 |
| Repeated | 1,565.505 | 1,641.411 | 24,964,892 | 4,211,377 |
| Fixed seed | 1,793.412 | 2,444.105 | 44,446,919 | 4,772,349 |

These are Python traced allocations, not process residency or native-provider
workspace limits. The small decode-peak differences between Python/Expat builds
are recorded rather than treated as a regression threshold. Concurrent builds
affect wall times, so this replay establishes reproducibility of values, bytes,
generation and measurement procedure without claiming a cross-host speedup.

The native report SHA-256 is
`98d09f5c3c2d749855726b3b458a154a49252d11d2f1a6f425ee0fd80907c8e1`.
Ignored evidence under
`out/build/backlog-2026-10-20/xml-measurements-linux-final/` retains the complete
report, three XML outputs, generation logs, exact input inventory and the
cross-host comparison. The maintained runner retains SHA-256
`97ace94e47ccbafa24b6339bab05009564265e7b9e12288dca9828ce4dae0e2f`.
