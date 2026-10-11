# Generic optional qualification - 11 October 2026

This record covers compiler/runtime **1.12.0**, schema language **1.7.0**, and
synchronized editor extensions **1.1.33**. The tested working tree starts from
`46468568a5c198511ac890511dbfa87a47112d9d` and includes the optional implementation
and example changes described in [the optional contract](../schema/optional.md).

## Native build and tests

The Windows x64 Release build passes all **102 CTest checks**, including **450
GoogleTest cases** in the core executable. The configuration enables the managed
runtime, SIMD, and Zstandard/LZ4/zlib compression backends. It uses MSVC 19.51,
MSBuild 18.10.1, and Windows SDK 10.0.26100.

Seven optional schema tests cover arity, nested substitution, language gates,
legacy declared names, ownership/default/key restrictions,
compatibility identities, lexical binding, and explicit unsupported combinations.
Seven native codec tests cover all four protocols, nested presence, legacy host
JSON behavior, concrete generic applications and default arguments, frozen enum
field-context bytes, malformed framing, replacement, and decode budgets.
The core include/version suites also exercise older, newer, malformed and
overflowing language headers and per-file include requirements.

The direct and separate-values managed profiles each pass six optional tests.
They exercise IDs and wrapped editors, engagement/replacement, immutable history,
undo/redo, failed transactions and stale editors, transient reset, and authority/
replica collaboration. A separate helper process verifies appended and sidecar
journal recovery. These checks are included in the 102-test result.

The generated C ownership/replacement consumer passes MSVC `/W4 /WX` and a WSL
GCC C11 build with AddressSanitizer, UndefinedBehaviorSanitizer, and strict warnings.
It covers every invalid presence tag from 2 through 255, nested records and
containers, destination preservation, truncation, trailing input and limits.

Generated C++ owning constant-evaluation and emission-only positional profiles
also compile with `/W4 /WX`. Scalar, nested and enum optionals produce frozen
consteval bytes `01 2a 00 00 00 01 00 01 01`, matching owning runtime output.
This check does not extend the existing constant subset to maps or other protocols.

The installed CMake consumer and same-major package-version checks pass.
`serializer --version` reports compiler **1.12.0** and language **1.7.0**.

Run the native suite from Visual Studio Developer PowerShell x64:

```powershell
./make.ps1 test -Configuration Release -BuildDirectory out/build/make-Release -Jobs 4
```

This run additionally supplied `-CMakeArgs '-DVCPKG_MANIFEST_INSTALL=OFF'` to use
already-installed dependencies. A first run from ordinary PowerShell passed
100 checks; two compiler rejection controls lacked MSVC standard include paths.
The complete Developer PowerShell rerun passes all 102 checks. Final native
results are retained in `out/build/make-Release/Testing/Temporary/LastTest.log`.

## Independent generated-language fixtures

Actual generated consumers pass **431 optional cases and 80 owning optional
variant cases per language**, or **5,621 fixture checks** across all eleven
outputs. The harness constructs wire fixtures independently of the generated
encoders and also compares generated output bytes. It checks JSON and all three
native binary protocols, nested absence, present empty values, optional records,
generic substitutions, enum field contexts, optional collections, truncation,
invalid tags/cardinality/null values, trailing input, depth and element limits.

| SDK | Execution and compiler settings |
| --- | --- |
| C++ | Windows MSVC 19.51, C++20, warnings as errors |
| Java | Windows javac 26, `--release 17 -Werror` |
| JavaScript | Windows Node.js 22.23.1 |
| TypeScript | Windows TypeScript 7.0.2 and Node.js 22.23.1 |
| Go | Windows Go 1.26.4 |
| C# | Windows .NET SDK 10.0.401, net8.0, nullable checks and warnings as errors |
| Python | Windows Python 3.14.3 |
| Kotlin/JVM | Windows Kotlin 2.3.20, JRE 26 |
| Rust | WSL Rust 1.93.1, edition 2021, `-O -D warnings` |
| Swift | WSL Swift 6.1.3, optimized, warnings as errors |
| C | WSL GCC 15.2.0, C11, `-Wall -Wextra -Wpedantic -Werror` |

Reproduce the independent matrices with these commands after exposing the SDKs
to the runner. `SERIALIZER_TSC` and `SERIALIZER_KOTLINC` can select their launchers.
The C++ runner needs the Developer PowerShell environment and runtime library.

```powershell
python test/optional_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/optional-languages --language all --wsl-languages c,rust,swift
python test/optional_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/optional-variants --language all --wsl-languages c,rust,swift --variant-only
```

Useful generated consumers, binaries and fixtures are retained under
`out/optional-languages`, `out/optional-variants`, and
`out/optional-cpp-qualification` and `out/optional-cpp-variants` (the final isolated
C++ runs).

## Maintained examples and editors

The basic, collections, complex and optional examples pass in all eleven
languages and all four protocols. The first three now demonstrate actual payload
`version` metadata at value 1 and reject future version 2. The optional examples
demonstrate absent, present, nested and present-empty values using each language's
normal owning representation. Results are retained under
`out/optional-example-qualification`.

Both editor packages were rebuilt and validated at **1.1.33**. All **98** unit
tests, **459** checks against the actual Visual Studio .NET navigation resolver,
and **20,047** fresh generated-output navigation checks pass. Coverage includes
optional/generic syntax, include navigation, qualified names, selection boundaries,
all eleven output languages, C++ naming profiles and legacy declared names.
Final VSIX packages are retained under `out/extensions`; interactive installation
was not requested.

No complete Linux C++ runtime suite, native macOS/Android/ARM run, big-endian
matrix, fuzz campaign, or throughput benchmark was performed for this change.
The C sanitizer checks and WSL Rust/Swift/C consumer checks have the scopes
described above. Older qualification records remain historical results for their
recorded source revisions.
