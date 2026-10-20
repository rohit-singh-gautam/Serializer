# C++ XML measurements, 20 October 2026

[Runner](../../qualification/cpp_xml_benchmark.py) ·
[Measurement driver](../../qualification/cpp_xml_benchmark.cpp) ·
[XML codecs](../serialization/xml_tokens.md) · [Backlog](../pending.md)

This record qualifies **CP-Q05/CP-Q06/CP-Q07** for selected owning C++ workloads
on Windows, using compiler/runtime **2.14.0**, schema language **1.20.0**.
Actual MSVC **19.51.36260** compiles both profiles as C++20, `/O2 /MD /W4 /WX`.
The host is Windows 11 build **26300**; the XML provider is LibXml2 **2.15.3**.
These measurements establish no performance threshold or hard allocation bound.

The actual compiler SHA-256 is
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`;
its frozen runtime library SHA-256 is
`6c3f405b53f20346623940dfb8a0323c86bb2c6d545f17974e009efe415874ba`.
The public XML order schema/configuration is generated twice, with XML enabled
and disabled. The latter compiles without XML headers or provider linkage;
actual `dumpbin /DEPENDENTS` confirms that it imports no XML provider DLL.

## Workloads, counters and independent checks

The three models contain two small tags, 20,000 repeated escaped/Unicode tags,
or 20,000 fixed-seed 32-character tags. Seed `0x584D4C50455246` and the retained
input hashes reproduce the values. Independent ElementTree produces the inputs
and checks expanded names, order, attributes and exact escaped Unicode values in
every generated XML output. Carriage returns use numeric references to preserve
their values under XML line-ending normalization.

Every operation runs in a fresh process, warms once, and retains five separate
wall-clock and process-CPU samples: **54 isolated operations / 270 samples**.
Codec timing excludes generation, compilation, file I/O, model preparation,
value checking and result destruction. All four native protocols decode to the
same owning values; XML-enabled and native-only builds produce identical native
bytes. The XML-enabled native baseline prepares its model from XML, whereas
the native-only baseline prepares the same values from ordinary BINARY_NONE.
That setup is outside timing and remains part of process residency.

CPU uses actual kernel/user process counters from
[GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes).
Small operations can return zero at the host's effective CPU-counter resolution.
The driver avoids MSVC's elapsed-time `clock()` semantics. Peak memory is the
operating system's absolute
[PeakWorkingSetSize](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters),
in bytes. It includes libraries, fixture/model setup, checking and warmup.
The report retains both prepared and final high-water values; their difference
does not measure exact operation allocation, and an unchanged peak does not
mean that an operation allocates nothing.

| Workload | Input XML bytes | Output XML bytes | XML encode median ms | XML decode median ms | Encode process peak bytes | Decode process peak bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small, two tags | 180 | 191 | 0.009 | 0.060 | 6,422,528 | 5,890,048 |
| Repeated, 20,000 tags | 1,080,144 | 1,120,151 | 80.617 | 206.574 | 31,133,696 | 31,346,688 |
| Fixed seed, 20,000 tags | 1,659,129 | 1,699,136 | 106.002 | 168.057 | 34,217,984 | 34,267,136 |

Repeated-data median XML process CPU is **78.125/156.250 ms** for encode/decode;
fixed-seed data records **109.375/156.250 ms**. Native-only BINARY_NONE medians
are **1.628/8.800 ms** and **6.739/9.622 ms**, respectively. The report retains
all four protocol baselines, raw wall/CPU samples and throughput calculations.
Other builds ran concurrently, and individual wall samples vary substantially.
These observations do not establish a speedup or an isolated regression limit.

## Encoded and generated sizes

| Workload | JSON bytes | BINARY_NONE bytes | BINARY_INTEGER bytes | BINARY_STRING bytes |
| --- | ---: | ---: | ---: | ---: |
| Small | 74 | 24 | 29 | 48 |
| Repeated | 560,070 | 520,022 | 520,027 | 520,046 |
| Fixed seed | 1,373,326 | 802,318 | 802,323 | 802,342 |

The XML-enabled generated header is **25,539 bytes**, SHA-256
`72401b26e6edb58c2478c257e42129932305cb349e42eb64721b7aeb0d35f6da`.
The native-only header is **19,088 bytes**, SHA-256
`8df2455ed7652881e98fa16dd61005a4cee9c58e92027533ff0064e65c223dc9`.
Their selected source difference is **6,451 bytes**. Shared runtime and external
provider code are separate from these generated-header sizes. The retained
report also identifies executable sizes/hashes, public headers and provider
library hashes. Different valid XML/JSON lexical choices explain why these
output sizes need not equal another language's output sizes.

These are complete owning messages. The record claims no constant-memory
streaming, operation-specific heap ceiling, provider-workspace isolation or
support for values larger than available memory. Linux/platform replay remains
separate from these selected Windows measurements.

## Reproduction and retained evidence

Run from an MSVC x64 developer environment:

```powershell
python -B qualification/cpp_xml_benchmark.py `
  --compiler out/build/backlog-2026-10-20/compiler-compression-final-99380733/serializer.exe `
  --runtime-library out/build/backlog-2026-10-20/compiler-compression-final-99380733/serializer_runtime.lib `
  --provider-prefix out/build/make-Release/vcpkg_installed/x64-windows `
  --build out/build/backlog-2026-10-20/cpp-xml-benchmark-final
```

Runner SHA-256:
`310068b26d626034290019e0be2f18588266faa7e24480ad256d2ee3b29d0ae2`.
Driver SHA-256:
`2cf22baf405b1d3da8f2a38a6ea099381f69109094925c80ca1cef2f9ed1689c`.
Final report SHA-256:
`2e7b8fa6059016d88ed5aaf7deb694d05258594d8709fb97abe45fc55269c49d`.
Ignored evidence under `cpp-xml-benchmark-final/` retains exact commands, SDK
versions, all 54 operation logs, independent XML fixtures, encoded outputs,
dependency checks and `verification.json`. Generated build scratch is removed
automatically. An earlier successful measurement run retained the help heading
instead of the SDK banner in one metadata field; the final run corrects that
field and replays all measurements.

## Native Linux replay and cross-host reproducibility

The maintained runner now supports both the existing MSVC command and native
Linux C++20. Its new `--headers`, `--cxx` and `--xml-library` options can select
the exact frozen headers, SDK and provider without a global installation.
The Linux replay uses GCC **15.2.0**, `-std=c++20 -O2 -Wall -Wextra -Wpedantic
-Werror`, compiler
`3b39d98189350b0feebdf2ce59cae51b1aa05a8c6c037d7ff86ce12eb2a1ee78`,
and that compiler's copied runtime archive/public headers. LibXml2 **2.15.2**
is the existing native package under `/usr`, not the separate optional
Zstandard/LZ4 prefix. Its selected shared library SHA-256 is
`4e8990287f9c900905e24d94fa1945cf6fa94efda100ab437c0e0520ba3663a0`.
Both generated profiles compile and run; `ldd` confirms the native-only
executable has no XML shared-library dependency.

All **54 fresh-process operations / 270 measured calls** pass. All **54 output
hashes**, all three independent-input hashes and both generated-header hashes
match Windows exactly. The same updated runner/driver also replays successfully
with the original Windows MSVC environment and frozen `99380733` compiler.
This qualifies **CP-Q08** for these selected Windows and native Linux profiles;
it does not qualify another platform, SDK, schema graph or provider.

| Linux workload | XML encode median ms | XML decode median ms | Encode process peak bytes | Decode process peak bytes |
| --- | ---: | ---: | ---: | ---: |
| Small, two tags | 0.008 | 0.032 | 5,500,928 | 5,533,696 |
| Repeated, 20,000 tags | 22.962 | 64.340 | 25,579,520 | 25,571,328 |
| Fixed seed, 20,000 tags | 42.238 | 176.114 | 28,667,904 | 28,700,672 |

Linux process CPU uses the native C process clock. Residency reads the current
executable image's checked `VmHWM` value from `/proc/self/status`. An independent
probe showed that `getrusage` could retain a **143,944 KiB** parent peak across
`exec`, while the new child image's `VmHWM` was **12,008 KiB**. That inherited
counter is therefore unsuitable for these fresh-process measurements. `VmHWM`
remains an approximate resident-set high-water value, including setup, libraries
and warmup, rather than an operation-specific heap measurement.
[Linux kernel process counters](https://docs.kernel.org/filesystems/proc.html)
describe this high-water counter and the limits of resident-set accounting.
The Windows branch retains its original `PeakWorkingSetSize` counter. Concurrent
builds ran during both replays; cross-host timing differences establish no speedup.

For native Linux, use the same command with a native compiler/runtime archive,
`--headers` pointing to its matching include directory, `--provider-prefix /usr`,
`--xml-library /usr/lib/x86_64-linux-gnu/libxml2.so` and `--cxx /usr/bin/g++`.
All inputs and outputs were copied to a private Linux-local tree; no main native
build cache or provider installation was modified.

Retained evidence is under
`out/build/backlog-2026-10-20/xml-measurements-linux-final/`, including all output
bytes, operation logs, exact argv, source inventory and the cross-host comparison.
The native C++ report SHA-256 is
`2d1b0388b0e61decb371f1f913641ad8bbee3028d64683d6cf6338d6914694dc`;
the updated Windows report is
`6486dd43c335d7c3c485793f08e71062a66c41dfef8779133ec23ac1a3c75e91`.
Updated runner/driver SHA-256 values are respectively
`2e6da5cccb4a9841b43aca7afa46bb6b6d2d71fa4574cbc0d74de108e0e82422`
and `cf17e4fc7409cebedaacba4646784382f37272c0757afcc4eb11f15ba0dd3afb`.
The earlier Windows report and hashes above remain separate historical evidence.
