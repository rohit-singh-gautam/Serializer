# Digest verification — 9 October 2026

This change introduces compiler/runtime **1.8.0**, schema language **1.3.0**,
and synchronized editor source versions **1.1.28**. Existing schema contracts
remain supported. See [digest usage](digest.md) and
[the application licensing rationale](licensing.md#why-the-license-was-separated).

## Recorded checks

| Check | Result |
| --- | --- |
| Native C++ digest implementation | Eight tests passed, including 96 known-answer computations across the twelve supported algorithms; MSVC `/W4 /WX` build passed |
| Release metadata | CLI independently reports compiler **1.8.0** and schema language **1.3.0** |
| Editor unit/grammar/provider tests | **89/89** passed, including every digest algorithm, manual byte extents, adjacent included/generic references, unsaved includes, and legacy user types named `digest` |
| Fresh-compiler generated navigation | **17,182** bidirectional checks passed across all eleven outputs, including the new digest schema, naming profiles, generic/managed models, and older schema headers |
| Visual Studio shared .NET/Jint resolver | **276** source/output checks passed, including digest generic defaults/unions, legacy names, all eleven output languages, unsaved includes, selection endpoints, and cancellation |
| Editor packages | Both **1.1.28** VSIX packages rebuilt through `editors/build.ps1`; Visual Studio package validation passed |
| Documentation links | 667 local Markdown link targets resolved across twenty-three maintained documentation/skill files |
| Cross-language digest models/codecs | All eleven outputs compiled and passed the SDK matrix: **197** C++ cases and **201** cases for each of the other ten outputs, **2,207** total |
| Full native MSVC Release CTest suite | **89/89** passed, exit 0, in 341.37 seconds |
| Installed package | Passed, including standalone digest-header compilation and runtime-only linkage computing the known SHA-256 digest of `abc` |

The editor-generated navigation test uses the newly built compiler and real
`test/resources/digest_language.serializer` output. It tests navigation, rather
than treating successful generation as an SDK compilation or codec roundtrip.
The shared Visual Studio console test uses the actual .NET interpreter and path
adapter, independently of the Visual Studio native host.

The SDK matrix covers JSON and all three native binary protocols, frozen wire
fixtures, exact decode, malformed fixed lengths, and nested array/map/union
digests. Portable writer cases also reject incorrectly sized fixed digests;
C++ uses fixed `std::array` storage. Real SDKs were used for C++, Java,
JavaScript/TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C, with warnings
treated as errors where configured. Kotlin used a local complete CLI launcher
for the installed 2.3.10 compiler with Java 17. Runtime computation remains C++
only; successful portable codec checks do not claim portable hash providers.

Useful editor logs are retained under `out/build/digest-types/`, including
`editor-unit.log`, `generated-navigation.log`, the package build/validation logs,
and `visual-studio-resolver/`. These are ignored build artifacts. Extension
installation, native extension-host tests, publication, and external registry
updates were not requested or performed for this change.

The authoritative native suite log is
`out/build/verification_logs/digest-ctest-msvc.log`; the consolidated all-language
PASS matrix is `out/build/verification_logs/digest-sdk-summary.log`.
The successful SDK fixtures, final generated navigation fixtures, installed
consumer results, and resolver executable are retained as useful ignored output.
Unused probes and recovered failed-build scratch were removed.

## Scope and limitations

All native output languages receive digest storage and four native codecs. Only
the C++ runtime supplies digest computation in this release; other applications
choose their own provider. Fixed digest cardinality is part of the schema
contract. Digest keys, nonempty initializers, C++ views, and Protobuf mappings
reject explicitly. SHAKE/BLAKE computation and selectors remain deferred.

Known-answer testing is not FIPS module validation, an independent cryptographic
audit, or a guarantee that every implementation or application is patent-free.
Proprietary permissions cover Serializer-authored runtime/generated support;
schema and third-party rights retain their own terms.
