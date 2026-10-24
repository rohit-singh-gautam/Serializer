# Current complete compression parity — 24 October 2026

[Qualification requirements](../pending.md#qualification-and-distribution) ·
[Maintained SDK/provider setup](../../qualification/compression_parity/README.md) ·
[Verification index](README.md)

The complete compiler/runtime **2.33.0**, language **1.23.0** Windows x64
compression inventory passes with **one worker**. All eleven real SDKs execute
the full fixed corpus, without lifecycle omissions or language/provider subsets:

- **15,400 shared decisions**: 1,400 per SDK, including 974 core inputs,
  136 Zstandard inputs, 181 LZ4 inputs and the declared history/window controls.
- **2,904 owning-model exchanges** across all eleven producers/consumers,
  four native protocols and six compression formats.
- **264 independently decoded frames** and **792 encode-bound decisions**.
- **25 C++ lifecycle controls** and all **17 maintained lifecycle/deployment
  suites**: Python; core/optional Java, Kotlin, C#, Go, JS/TS, Rust, Swift and C.

The maintained runner now accepts `--jobs 1` or `--jobs 2`, defaulting to two.
The same bound controls its native CMake build and independent lifecycle suite
executor. Existing callers retain the optional two-worker default; shared hosts
can select one. The initial worker-limit change reused the name `jobs` for the
case list, causing a `TypeError` before lifecycle execution. Renaming that local
inventory to `cases` fixes the harness; the complete one-worker replay passes.
No production codec, callback, ownership, wire or provider behavior changed.

Actual SDKs are MSVC **19.51.36260.0**, CMake **4.4.2**, Python **3.14.3**,
JDK **17.0.18+8**, Kotlin **2.2.21**, .NET SDK **10.0.402**, Node **22.23.1**,
TypeScript **5.9.3**, Go **1.26.4**, Rust/Cargo **1.99.0** and Swift **6.4**.
Optional JVM providers are zstd-jni **1.5.7-16** and LZ4 Java **1.12.0**;
.NET uses ZstdSharp **0.8.8** and K4os LZ4 **1.3.8**. Node lz4js **0.2.0**,
Rust/Go locked dependencies, Python LZ4 **4.4.5**, Python zlib **1.3.1.zlib-ng**
and stdlib Zstandard **1.5.7** retain their exact versions and artifact hashes in
the report. Native C++ uses the separate pinned all-provider deployment.

Raw logs, configuration, corpus and reports remain under ignored
`out/qualification-2026-10-24/`. The complete report is `compression-parity.json`,
SHA-256 `c4deb1b734355e069c786eedbdad9517ed43db34c092d5809fe93f33e5239429`.
Its executed runner SHA-256 is
`7aefcc6c055fa7796f3926f74248acd35ed9d68d86b3e1a79dde1da538ba0ddb`;
its compiler image SHA-256 is
`701f08e6b45120524c2aaa0fe767e6a56de30053a1993e74d365c59cadc8d08c`.
`initial-compression-parity.log` retains the first harness failure.

This is the selected Windows owning-model/whole-compression gate. It does not
claim nested XML/OPC context coverage, other hosts/providers, a performance
threshold, physical provider memory limits or interruption inside an opaque
provider call. README and the integration skill were reviewed; usage and
compiler/runtime release versions are unchanged by this qualification-tool update.
