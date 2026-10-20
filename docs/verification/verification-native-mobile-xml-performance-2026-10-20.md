# C, Rust, Kotlin and Swift XML measurements — 20 October 2026

[Pending requirements](../pending.md) · [Qualification runner](../../qualification/native_mobile_xml_benchmark.py)

Compiler/runtime **2.16.0**, schema language **1.20.0**. These selected native
Windows owning workloads qualify `C/RS/KT/SW-Q05`, `Q06` and `Q07`: reproducible
wall/CPU measurements, explicit memory-counter limits, XML/native encoded sizes
and generated-source sizes. Other platforms, providers, vocabularies and larger
inputs remain open. No speed threshold or earlier-compiler regression claim is
made.

## Reproduction and capture

Run from the repository root with the actual installed SDKs and the retained
immutable compiler, including its deployed DLLs:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' `
  -Arch amd64 -HostArch amd64 -SkipAutomaticLocation -NoLogo
$env:TEMP = 'C:/src/Serializer/out/build/pending-qname-2026-10-20/temp'
$env:TMP = $env:TEMP
python -B qualification/native_mobile_xml_benchmark.py `
  --compiler out/build/pending-qname-2026-10-20/fifty-compiler-initial/serializer.exe `
  --build out/build/pending-qname-2026-10-20/native-mobile-final-measurements `
  --libxml2-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows `
  --kotlin-home out/tools/kotlin-2.2.21/kotlinc `
  --cargo-home out/build/pending-qname-2026-10-20/native-namespaces/cargo-home
```

Use an empty build directory. The runner requires every selected SDK and does
not install dependencies or skip unavailable cases. Cargo uses the maintained
XML example's lockfile with `--offline --locked`; C resolves the explicit
libxml2 installation. `--language` permits a distinct subset of
`c,rust,kotlin,swift`; the full qualification uses all four.

The local capture is
`out/build/pending-qname-2026-10-20/native-mobile-final-measurements/verification.json`.
The directory retains exact command arrays and command logs, input fixtures,
generated and handwritten sources, binaries/JARs, canonical wire outputs and XML
outputs. The report retains SHA-256 hashes for the immutable compiler, runner,
drivers, generated sources, executable images and relevant dependency artifacts,
plus exact SDK identity and compiler settings. The measured compiler SHA-256 is
`aeadbe59308d2b3b3dadce0aa09f2fe12dee51666132ec2dff40ea133987a87d`;
its hash is checked again when the run finishes.
The retained runner SHA-256 is
`3613e561b060845d642d6f77d4ffb57e98fad3bccf01e6687d335f8ca53a8af0`.

The final integrated compiler snapshot is
`out/build/pending-qname-2026-10-20/fifty-compiler-qualified/serializer.exe`, SHA-256
`78c3210e00ec48d9ee265e323d4498b8e38f3b18712cac5cd5fda5bac7c55f6c`.
An eight-profile regeneration in `native-mobile-qualified-source-check/verification.json`
confirms the six C/Rust/Swift measured sources are byte-identical to its outputs.
Both Kotlin sources changed during package-runtime integration, so their complete
18-operation measurement was repeated against that final compiler:

```powershell
python -B qualification/native_mobile_xml_benchmark.py `
  --compiler out/build/pending-qname-2026-10-20/fifty-compiler-qualified/serializer.exe `
  --build out/build/pending-qname-2026-10-20/kotlin-qualified-measurements `
  --language kotlin --kotlin-home out/tools/kotlin-2.2.21/kotlinc
```

The final Kotlin report and separate integrity audit are retained in
`kotlin-qualified-measurements/{verification,verification-audit}.json`. Both regenerated
source hashes match the final eight-profile comparison. Every native and XML
output is byte-identical to the original multi-SDK capture. Results below use
that final Kotlin replay with the unchanged C/Rust/Swift source measurements.

The subsequent C# template whitespace cleanup uses compiler
`out/build/pending-qname-2026-10-20/fifty-compiler-clean/serializer.exe`, SHA-256
`ef49b32f4a37abb493b877275eaf263960e761de9c91b981ee603cfdfae6b6a1`.
`native-mobile-clean-source-check/verification.json` regenerates all eight
profiles and proves byte-for-byte equality with the selected C/Rust/Swift and
final Kotlin measurement sources. Their source sizes and measured code remain
unchanged.

## Workloads and independent checks

All eight generated profiles use the maintained
[`example/xml/model.serializer`](../../example/xml/model.serializer): one
XML-enabled and one XML-disabled `BINARY_NONE` profile per SDK. The three
independently constructed workloads contain two small tags, 20,000 repeated
escaped Unicode tags, and 20,000 fixed-seed randomized tags. The seed is
`0x584D4C50455246`. Exact values include XML metacharacters, tabs, CR/LF, Greek
text and supplementary Unicode. Independent ElementTree checks compare expanded
names, attributes, child order and exact decoded values.

Each workload runs XML encode/decode and native encode/decode on its XML-enabled
model, followed by native encode/decode on its XML-disabled control. Each
operation runs in a fresh actual executable or JVM. All final model results must
encode to exactly the same native bytes across modes, profiles and SDKs, and each
XML-enabled operation must also produce XML accepted by the independent oracle.
Native controls measure a different wire format and provide ordinary-codec
baselines; they do not perform equivalent XML provider work.

All **72 isolated operations** passed, retaining **360 positive wall samples**.
All **24 native outputs per workload** are byte-identical, and all **48 XML
outputs** pass the independent oracle. A separate audit checked all eight
generated-source hashes, executable/driver/dependency hashes, all 120 output
hashes and sizes, every throughput calculation and positive process CPU/peak
counters. Its summary remains beside the report in `verification-audit.json`.
The original four-SDK capture's minimum median timing batch was **10,550,500 ns**.
The final Kotlin replay independently passed **18 operations**, **90 positive
wall samples** and **12 XML checks**, with all 30 output hashes/sizes verified
against both the original capture and the prior integration replay. Its minimum
median batch was **10,258,400 ns**. The original capture and qualified replay retain
90 executed operations; the earlier 18-operation integration replay remains in
`kotlin-final-measurements` for audit provenance. The current-source selection
contains 72 operations across the four SDKs.

Each child performs three warm batches and retains five measured wall batches.
Calibration launches fresh children and raises repetitions until the median
batch reaches 10 ms. Timed calls use public owning APIs; setup, final result
validation and disk I/O occur outside those wall samples. Replacing the previous
owned result is part of the measured lifecycle. Rust uses `black_box`; Kotlin
stores each result through a volatile sink. C and Swift pass every result to an
opaque C sink compiled separately without LTO, so prior results remain observable
to the optimizer. That call's overhead is included.

## Actual SDKs and memory limits

The selected host is Windows 11 build **26300**, amd64, with Python **3.14.3**
orchestration. C builds with MSVC **19.51.36260.0**,
C11, `/O2 /GL- /W4 /WX`, using CMake **4.4.2** and Ninja. Its XML-enabled profile
links libxml2 **2.15.3**; its native-only profile does not link an XML
provider. Rust uses **1.99.0**, release compilation and `-D warnings`, with pinned
roxmltree **0.21.1**, xmlparser **0.13.6** and memchr **2.8.3**. Kotlin uses
**2.2.21-release-469**, JVM target 17 and warnings as errors on Oracle Java
**26+35-2893**. Swift uses **6.4**, `-O -warnings-as-errors`, Foundation and the
platform XML provider. C and Swift optimization barriers use a separate native C
translation unit with whole-program optimization disabled.

Kotlin's compiler and benchmark run through the real `java.home/bin/java.exe`,
whose path and image hash are retained; Windows `javapath` launchers are bypassed
for measured processes. Kotlin sets finite JAXP general/total entity bounds to
4,194,304/8,388,608 bytes for these escaped-text fixtures. Other generated limits
remain at their defaults.

Windows `GetProcessTimes` records whole-process user plus kernel CPU; the OS
scheduling resolution limits its precision. Absolute peak working-set counters
include startup, input/model setup, warmups, all timed batches, validation and
output. They are process high-water residency, with no before/after subtraction,
and do not measure exact per-call allocations or enforce hard memory caps.

Rust separately counts successful global allocation and reallocation requested
bytes for one untimed codec call. Reallocation contributes its full requested
extent, rather than net heap growth; allocator overhead is excluded. The
counter's disabled branch remains in timed allocation calls. Kotlin separately
records JVM current-thread allocated bytes for one untimed call; native provider
storage can fall outside that counter. C and Swift have no exact allocation
instrumentation in this profile and explicitly report `null` for that value.
Owning results, allocator policy and JVM collection affect residency, so it must
not be inferred from one call's requested-byte counter.

## Observed large-workload results

The randomized fixture has **1,659,129 XML input bytes** and **802,318 native
bytes**. XML encode throughput uses each backend's actual emitted bytes; XML
decode uses the common input size. Native throughput uses native bytes. The
process peak ranges below cover all 18 operations per SDK across all three
workloads, including the runtime and retained inputs.

| SDK | XML encode/decode MiB/s | Native-only encode/decode MiB/s | XML output bytes | Process peak range MiB |
| --- | ---: | ---: | ---: | ---: |
| C | 66.24 / 36.62 | 379.06 / 290.09 | 2,439,207 | 4.35–17.36 |
| Rust | 121.89 / 58.81 | 1,260.96 / 220.79 | 1,499,101 | 4.38–13.45 |
| Kotlin | 50.21 / 16.03 | 135.27 / 102.07 | 2,439,207 | 90.05–344.88 |
| Swift | 39.86 / 6.35 | 396.39 / 85.70 | 2,439,207 | 16.57–33.01 |

For this randomized workload, the separate XML encode/decode allocation samples
are **4,714,481 / 7,137,716 requested bytes** for Rust and
**62,250,968 / 59,860,056 current-thread bytes** for Kotlin. These counters have
the different scopes described above; they do not represent interchangeable
per-call peak heaps. C/Swift allocation samples remain explicitly unavailable.

| SDK | XML-enabled generated bytes | XML-disabled generated bytes |
| --- | ---: | ---: |
| C | 288,203 | 108,292 |
| Rust | 226,517 | 99,192 |
| Kotlin | 258,600 | 141,532 |
| Swift | 224,139 | 82,964 |

Generated sizes count the compiler's output source only. They exclude handwritten
drivers and optimization barriers, SDK/provider libraries, compiled images and
JARs. Both measured Kotlin sources include the final compiler's ZIP helpers,
package accounting repairs and qualified SDK references. The XML operation hook
uses the preexisting reserved runtime name. Both profiles were regenerated and
compiled for the final 18-operation replay.

Concurrent machine work affects timing. These measurements establish neither a
cross-SDK ranking, a performance improvement, streaming behavior, Linux/macOS
qualification nor a complete-operation memory guarantee.
