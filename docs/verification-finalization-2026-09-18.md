# Finalization verification - 2026-09-18

This record covers the final correctness fixes and ordering/documentation review.
It supplements the earlier [native-language](verification-native-2026-09-18.md),
[cross-endian](verification-cross-endian-2026-09-18.md), and
[UTF-8](verification-utf8-2026-09-18.md) records. Results apply to the source changes
shipped with this record, based on commit `415e8fb0d28c3e488e99dcacc99c14e25b991863`.

## Implemented fixes

- **Floating defaults:** Rust emits floating literals for integer-spelled
  `float`/`double` defaults, avoiding inference as an overflowing `i32` before a
  cast. C emits floating literals for integer-spelled `double` defaults, avoiding
  integer-literal range diagnostics. Regression schemas cover large positive and
  negative defaults, exponent notation, integer-spelled negative zero, implicit
  zero, absent keyed fields, and all four native protocols.
- **ProtoJSON replacement:** duplicate ordinary fields, including original and
  JSON-name aliases, replace earlier values. Nested objects replace rather than
  merge, and a later `null` restores Protobuf defaults. Generated union wrappers
  also replace earlier wrappers; conflicting alternatives within one object are
  still rejected. Binary Protobuf message merging is preserved.
- **TextProto allocation checks:** decoded string bytes are charged before
  appending, including Unicode escapes and adjacent quoted fragments. Tests
  distinguish an early resource-limit failure from a payload-sized allocation
  attempt and verify that rejection preserves the caller's destination.
- **Windows Swift UTF-8 preservation:** native Windows testing found that the
  Foundation string initializer removed leading U+FEFF data. The generated runtime
  now uses strict standard-library UTF-8 validation for binary strings, JSON
  strings, and tokens. Added cases cover leading/repeated U+FEFF, raw/escaped JSON,
  and rejection of invalid field-name and number-token prefixes.

Regenerate affected Rust, C, Swift, and C++ Protobuf output after updating the
compiler. See [migration instructions](../migration.md#finalization-correctness-fixes).

## Ordering and resource contracts

Historical `map(char)` ordering is preserved: Java uses signed byte ordering, C++
uses its native `char` ordering, and the other generated backends use unsigned byte
ordering. Different orders preserve decoded values but can produce different
bytes. A dedicated test pins each producer's existing binary bytes and checks
all producer/consumer directions between signed-char C++, unsigned-char C++, Java,
and Python. New portable byte-keyed schemas should prefer `map(uint8)` when they
need unsigned ordering. Changing existing schemas requires compatibility review,
including their different JSON key representation. See
[map ordering](wire_format.md#map-ordering).

The allocation budget describes logical decoded storage, not total parser heap
usage. Protobuf text identifier/numeric scratch buffers, capacity slack, and
diagnostics are outside that budget. Input/work limits bound scanning; TextProto
token lengths also obey `max_string_bytes`. These limits must be configured
together. This scope is now explicit in the usage, wire-format, Protobuf, and
integration-skill documentation.

The optimization review does not establish a new benchmark threshold or make a
throughput claim. The changes preserve existing formats and ordering, avoid
parser redesign, and correct allocation timing and invalid decoding behavior.
Broader optimization work is outside this maintenance finalization.

## Verification environment

The main build uses Windows x64, MSVC 19.51, CMake/Ninja Release, and SIMD enabled.
Rust and sanitized C run through WSL; Swift runs natively on Windows. The full
interoperability runner also executes s390x C/C++ through QEMU with the existing
little-endian wire profile. SDK versions are those in the earlier native-language
record, with Windows Swift **6.4** added. A separate Linux Swift **6.1.3** check
qualifies the updated UTF-8 initializer on that previously tested compiler.

Focused C++ Protobuf sanitizer verification uses Clang **21.1.8** under WSL with
ASan/UBSan and leak detection. It compiles the current tests, headers, and freshly
generated model, linking the existing instrumented runtime archive; it is not a
fresh full Linux build of every target.

## Results

The final full regression run passes **49/49 CTest checks** in 279.79 seconds.

- The C++ core suite passes **220 tests**; the Protobuf executable passes
  **18 tests**, including its stream-adapter checks.
- All **44 maintained examples** pass across 11 language folders. The full
  interoperability matrix passes **2,028 exchanges**: 13 producers, 13 consumers,
  four protocols, and three union alternatives, including the two s390x consumers.
  Frozen positional bytes match. This fixture does not contain high-byte `char`
  map keys; the separate ordering test below covers that exception.
- Python, Rust, Windows Swift, Kotlin, and C each pass **1,029 boundary cases**.
  The C consumers also pass ASan/UBSan and allocation-failure checks.
- The dedicated character-map test passes **64 semantic exchanges**, covering
  four producers, four consumers, and four protocols. Binary keys include
  `0`, `127`, `128`, and `255`; JSON uses ASCII keys and separately rejects
  high-byte character keys.
- Windows Swift 6.4 and Linux Swift 6.1.3 each pass **1,029 boundary cases** with
  optimized builds and warnings treated as errors.
- Focused Protobuf ASan/UBSan checks pass **15 codec tests** and the standalone
  pre-allocation guard, with no reported sanitizer findings.
- Compiled large-floating-default probes pass for Java, Go, C#, Kotlin, and Swift
  in addition to the maintained Rust/C regression consumers.
- Generated-output navigation passes **335 bidirectional checks** across all
  11 output languages. Both editor integrations were reviewed; these changes
  require no editor behavior, syntax, metadata, or package changes.

Optional compression backends, official Protobuf interoperability/conformance,
extension package rebuilding, and installation tests were not rerun for this
change. Earlier records retain their historical results. No new macOS, Apple
device, minimum-SDK, or full Linux C++ regression qualification is claimed.

## Reproduction

Use the [build prerequisites](../README.md#build-and-test) and
[language SDK setup](../example/README.md). In a Visual Studio x64 developer
PowerShell session, ensure Swift's matching toolchain and runtime `bin` directories
are on `PATH`, and set `SDKROOT` to its installed Windows SDK. For the Swift 6.4
layout used here, with `$swift_install` set to the installation root:

```powershell
$env:PATH = "$swift_install\Toolchains\6.4.0+Asserts\usr\bin;$swift_install\Runtimes\6.4.0\usr\bin;" + $env:PATH
$env:SDKROOT = "$swift_install\Platforms\6.4.0\Windows.platform\Developer\SDKs\Windows.sdk"
```

Configure the existing Windows preset with the relevant test options. Rust and C
must be installed in WSL; Swift must be available to the Windows process. The
big-endian option also needs the cross-compilers and QEMU listed in the
[interoperability guide](../example/interoperability/README.md#different-machine-byte-orders).

```powershell
cmake --preset ReleaseWindows -B out/build/finalization `
  -DCMAKE_BUILD_TYPE=Release `
  -DSERIALIZER_BUILD_TESTS=ON `
  -DSERIALIZER_BUILD_JAVA_EXAMPLES=ON `
  -DSERIALIZER_BUILD_INTEROP_EXAMPLES=ON `
  -DSERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON `
  -DSERIALIZER_EXAMPLE_WSL_LANGUAGES=rust,c `
  -DSERIALIZER_EXAMPLE_SANITIZERS=ON `
  -DSERIALIZER_EXAMPLE_BIG_ENDIAN=ON
cmake --build out/build/finalization --parallel 4
ctest --test-dir out/build/finalization --output-on-failure -j 3
```

The focused additions are registered as `protobuf_allocation_test` and
`serializer_char_map_interoperability`; the existing `protobuf_codec_test` and
`serializer_native_codecs` include the new regressions.
