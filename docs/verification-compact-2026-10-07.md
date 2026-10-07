# Compact scalar verification — 7 October 2026

This record covers compiler/runtime **1.3.0**, schema language **1.2.0**, and
synchronized editor extensions **1.1.25**. Implemented annotations are
`compact_prefix`, `compact_varint`, `strict`, and `lenient` on owning unsigned
scalar fields. Signed and floating encodings remain proposals; see the
[compact encoding guide](compact_integers.md).

## Generated language codecs

The `test/compact_languages.py` harness passes for **all eleven outputs**:
C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C.
Each checks round trips in JSON and the three native binary protocols, exact
bytes against three independently frozen binary fixtures, and **551 additional
acceptance/rejection cases**. These include prefix width transitions, full-width
varints, strict overflow, lenient masking, narrow destination types, truncated
input, excess groups, noncanonical varints, and trailing input.

| Execution environment | Verified generated outputs |
| --- | --- |
| Windows | C++, Java, JavaScript, TypeScript, Go, C#, Python, Swift, Kotlin, C |
| WSL | Rust |

Windows C++ and C consumers use MSVC 19.51. TypeScript declarations compile
against the JavaScript runtime. Kotlin compiles with the installed Android
Studio Kotlin compiler using Java 17. A generated C++ consumer additionally
passes a GCC C++20 compile-only check under WSL with warnings treated as errors;
this does not establish a complete Linux runtime test result for this change.

The consolidated result is retained in
`out/build/verification_logs/compact_all_languages.log`; the same directory
contains final per-SDK logs. Generated consumers, SDK build outputs, and frozen
and malformed wire fixtures are retained under `out/compact-languages`.

## Native build and tests

A fresh Windows x64 MSVC/Ninja build succeeds with the default managed runtime,
SIMD, and optional compression configuration. All **80 CTest checks** pass
across the full run and a targeted retry. Three dimension-history checks in the
full run encountered access denial in the system temporary directory; all three
pass when `TEMP` and `TMP` point into the workspace. No source changes were needed
for that retry, and the other 77 passing checks were not repeated.

The checks include the native compact tests, default Python compact
qualification, parser/language-version/generic validation, native binary schema
compatibility, compiler CLI, installed CMake consumer, and same-major package
version compatibility. `serializer --version` confirms compiler **1.3.0** and
schema language **1.2.0**.

Useful build outputs and `build.log`, `ctest.log`, and `ctest-rerun.log` are
retained under `out/build/compact-msvc`. Scratch launchers, diagnostic helpers,
tool caches created for this task, and the empty retry temporary directory were
removed. Existing user work and earlier verification outputs were preserved.

## Editor checks

The shared TypeScript build and **86** unit/provider tests pass. Fresh generated
navigation checks pass **17,037** assertions across all eleven output languages,
and the Visual Studio .NET resolver passes **154** checks. Coverage includes
compact modifiers, policy tokens, scalar type/field navigation, snippets, and
legacy declarations whose type names coincide with the new modifiers.

Both VSIX packages were rebuilt and validated at **1.1.25**, with final packages
retained under `out/extensions`. Extension installation and interactive IDE
checks were not requested.

No size-throughput benchmark, complete Linux runtime suite, native Android,
macOS, or ARM qualification was performed for this change.
