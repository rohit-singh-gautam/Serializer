# Ordinary native codec baselines — 21 October 2026

[Pending requirements](../pending.md) · [Aggregate runner](../../qualification/native_codec_baseline.py)

The **2.20.1 compiler/runtime checkpoint**, schema language **1.21.0**, completes
`XML-B06` for the selected Windows owning positional-binary profile across all
eleven generated outputs. The aggregate passes **132 native operation profiles,
660 wall samples and exact cross-language byte checks**. XML-enabled models and
XML-disabled native controls retain identical native values and bytes.

## Workloads, baselines and measurements

The maintained C++, Python, portable and native/mobile benchmark drivers use the
same `example/xml` model and seed `0x584D4C50455246`. Three workloads contain two
small strings, 20,000 repeated escaped Unicode strings, or 20,000 fixed-seed
randomized strings. Their positional-binary sizes are respectively **24**,
**520,022** and **802,318 bytes** in every SDK and control.

Each SDK/workload has XML-enabled encode/decode plus XML-disabled encode/decode
controls. The aggregate independently requires all eleven languages, all three
workloads, each four-operation inventory, five raw timing samples, positive
resolved medians, a measured peak and exact shared wire hashes/sizes. A missing
tool/report, changed compiler, failed child or incomplete inventory fails the run.
The underlying drivers also check their generated XML with an independent parser
and retain CPU, allocation, generated-code-size and additional protocol data
where supported. Those extra results do not expand this selected baseline gate.

Observed XML-disabled repeated-workload measurements:

| SDK | Encode MiB/s | Decode MiB/s | Encode/decode peak MiB |
| --- | ---: | ---: | ---: |
| C++ | 628.32 | 218.41 | 8.38 / 8.41 |
| Python | 32.74 | 17.62 | 1.05 / 2.99 |
| Java | 113.71 | 292.80 | 185.99 / 372.65 |
| C# | 319.39 | 171.93 | 41.65 / 65.24 |
| Go | 451.25 | 432.29 | 20.27 / 13.97 |
| JavaScript | 35.89 | 109.52 | 51.02 / 85.47 |
| TypeScript | 38.44 | 111.28 | 51.00 / 82.69 |
| C | 205.95 | 79.29 | 9.15 / 9.41 |
| Rust | 187.15 | 30.24 | 8.60 / 9.69 |
| Kotlin | 83.91 | 78.64 | 103.99 / 142.84 |
| Swift | 96.00 | 42.54 | 22.04 / 22.62 |

Python peaks are **tracemalloc requested/traced Python allocation**, excluding
native workspace and process residency. Other rows use **absolute per-process
residency high-water marks**, including runtime startup, setup and providers.
These are different categories; they are not interchangeable heap measurements.
Other qualification/build jobs were active on the machine. These observations
have no speed pass threshold or claim about earlier-release regression, hardware
isolation, hard memory caps, streaming or larger-than-RAM behavior.

## Executed profile and provenance

Windows 11 x64; MSVC 19.51, Python 3.14.3, Java/Javac 26 with release-17 source
targets, .NET SDK 10.0.402 with the maintained net8.0 target, Go 1.26.4, Node
22.23.1, TypeScript 5.9.3, Rust/Cargo 1.99.0, Kotlin 2.2.21 and Swift 6.4.
Detailed runtime/process images, dependency locks and provider fingerprints remain
in the individual reports. The ordinary control does not require an XML provider;
the XML-enabled comparison retains each driver's actual deployed provider.

```powershell
python -B qualification/native_codec_baseline.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --runtime-library out/build/pending-namespace-budget-2026-10-21/compiler/serializer_runtime.lib `
  --provider-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows `
  --node-dependencies out/build/pending-qname-2026-10-20/javascript/node_modules `
  --kotlin-home out/tools/kotlin-2.2.21/kotlinc `
  --cargo-home out/build/pending-qname-2026-10-20/native-namespaces/cargo-home `
  --build out/build/pending-namespace-budget-2026-10-21/baseline
```

The run used the existing repository `lxml` directory through `PYTHONPATH`, a
Windows developer shell and an immutable compiler/runtime copy. All measured
drivers ran sequentially. `--audit-only` successfully rechecks the aggregate
against the retained reports without repeating measurements.

The compiler SHA-256 is
`9edd3eb526afb1bd8f8efa126fdce9c8bb6da3c85d845605e478ece920a06938`.
The retained aggregate and four original reports are under the named ignored
build directory. Their SHA-256 values are:

| Report | SHA-256 |
| --- | --- |
| C++ | `197013261699dffb8177a31ee846565f2492a40c9755c2b68a939ddd7305c367` |
| Python | `028dbe9e4d56c1bc8c181420140dde2b42711a21d78f6b37a5cea846a4e99c70` |
| Portable five SDKs | `57f850d31c5a666e608142acaa2be0dfbf974536c668d01259b097abb7e620d4` |
| Native/mobile four SDKs | `7fdc7cfca5113069a8d4f7480fc0b6fafccb0ffa9753ed1dd6a24787423c8025` |

Other native protocols, workload shapes, Linux/platform measurements and providers
remain covered by their separate pending qualification requirements. The baseline
records this executed checkpoint; later aggregate source changes require their
own relevant regression and performance review.
