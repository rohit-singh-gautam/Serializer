# Native language verification — 2026-09-18

This record covers Rust, Python, Swift, Kotlin/JVM, and C generation, the expanded
examples, and the CLI argument-access cleanup. The schema compiler and every
backend are implemented in C++.

## Environment and results

Windows x64, Intel Core Ultra 9 185H; MSVC 19.51, CMake/Ninja Release build.
Rust, Swift, and the sanitized C builds ran under WSL Ubuntu with GCC 15.2.

| Tool | Version exercised |
| --- | --- |
| Python | 3.14.3 |
| Rust | 1.93.1, edition 2021, `-O -D warnings` |
| Swift | 6.1.3, Linux x86_64, `-O -warnings-as-errors` |
| Kotlin | 2.4.20; compiler on JDK 17, consumer on JRE 26 |
| C | GCC 15.2 C11 with ASan/UBSan; MSVC 19.51 C11 separately |
| Java | JDK 17 for the existing CMake subset; JDK 26 with `--release 17` for the expanded runner |
| Node.js / TypeScript | 22.23.1 / 7.0.2 |
| Go | 1.26.4 |
| .NET SDK | 10.0.401, targeting net8.0 |

All **46 registered CTest checks passed** across the full run and the final
targeted rerun. The first run found two obsolete tests that still expected Rust
to be unsupported; they now test an unknown language name and both passed on
rerun. The core executable contains **207 passing tests**.

- All **44 maintained examples** passed: four in each of 11 language folders.
  The first three examples compare complete decoded data with independent JSON
  fixtures, including exact 64-bit values. The complex contract spans 13 schema
  files and includes nested and diamond dependencies.
- The full matrix passed **1,452 exchanges**: ten runtimes plus a strict typed
  TypeScript consumer, every producer/consumer direction, four native protocols,
  and three union alternatives. Binary bytes were identical. TypeScript uses
  the JavaScript runtime; it is not an eleventh runtime.
- Each new runtime passed **1,019 boundary cases**: complete and truncated
  messages, trailing input, byte/string/element/depth limits, malformed UTF-8,
  integer extremes, float range failures, Unicode escapes/NUL/BOM, unknown keys,
  invalid enum/union names, missing map entry members, repeated fields, and
  duplicate-map replacement. Swift preserves distinct canonical-equivalent
  Unicode map keys through `WireString`.
- C passed address/undefined sanitizers and injected allocation failures.
  Allocation failures leave existing models/buffers intact and release temporary
  storage. All four C examples also passed with MSVC warnings treated as errors.
- The existing five-runtime matrix, browser test, Java/JS/Go/C# codec tests,
  strict TypeScript declaration checks, schema include tests, and C++ profile,
  iostream, and enabled compression examples passed.
- The C++ compiler built with both MSVC and GCC. All 107 maintained `.serializer`
  files were reviewed for formatting; 51 required changes. A token comparison
  verified that formatting preserved identifiers, IDs, literals, and comments.

The example suites are opt-in; use `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON`
and see [SDK setup](../example/README.md). No Android/iOS device build, macOS
Swift build, Kotlin/Native or Kotlin/JS build, Rust `no_std` build, or minimum-SDK
qualification is claimed. Linux qualification here includes the compiler and
the named SDK consumers, not a second full Linux C++ regression run.

## Local throughput samples

The maintained interoperability programs performed 1,000 warmup iterations and
10,000 measured iterations on variant 0. These are single-run smoke measurements,
not performance thresholds or statistically controlled language comparisons.
SDKs and operating systems differ, and C uses `clock()` with relatively coarse
resolution. Managed runtimes include their usual allocation/GC behavior.

Rates below are messages/second, rounded from the recorded samples. Encoded
sizes are 319 bytes positional, 365 integer-key, and 564 string-key. JSON differs
only in legal numeric/escape presentation: Python 964, Rust/C 970, Swift/Kotlin
972 bytes; decoded values agree.

| Runtime | Protocol | Encode/s | Decode/s |
| --- | --- | ---: | ---: |
| Python | JSON | 18,810 | 2,544 |
| Python | Positional | 28,951 | 18,292 |
| Python | Integer-key | 24,252 | 13,132 |
| Python | String-key | 24,780 | 9,422 |
| Rust | JSON | 434,544 | 198,410 |
| Rust | Positional | 1,738,708 | 777,502 |
| Rust | Integer-key | 1,087,688 | 1,069,440 |
| Rust | String-key | 1,300,962 | 558,879 |
| Swift | JSON | 176,244 | 41,501 |
| Swift | Positional | 490,682 | 295,780 |
| Swift | Integer-key | 472,784 | 301,001 |
| Swift | String-key | 408,859 | 96,688 |
| Kotlin | JSON | 97,944 | 67,565 |
| Kotlin | Positional | 173,183 | 297,285 |
| Kotlin | Integer-key | 213,390 | 366,227 |
| Kotlin | String-key | 107,184 | 196,076 |
| C | JSON | 384,615 | 93,458 |
| C | Positional | 909,091 | 714,286 |
| C | Integer-key | 909,091 | 526,316 |
| C | String-key | 666,667 | 227,273 |

The implementation caches field keys and positional metadata, avoids reflection
and generic JSON object trees, validates capacity before reservation, uses native
numeric arrays in Kotlin, borrows Rust/Python input during decode, and keeps
ordinary C number tokens in stack scratch storage. These samples provide a
baseline for further profiling; they do not establish a regression budget.
