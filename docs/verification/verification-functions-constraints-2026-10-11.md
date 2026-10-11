# Readonly calls, generic constraints and C++ qualifier verification

Compiler/runtime **1.15.0**, schema language **1.10.0**, and both editor
extensions **1.1.37** were verified in the worktree based on
`5b2af1ecb65e823b8ac314888cb4c983c36e2f7a`. Existing compatible schemas and
wire layouts remain unchanged. These results apply to this change; earlier
toolchain qualification does not imply that every new feature was rerun on
those toolchains.

| Check | Result |
|---|---|
| Windows native regression | All 110 CTest entries passed through `./make.ps1 test -VcpkgRoot '' -Jobs 4`, including installed-package and package-version compatibility checks |
| Direct constexpr codecs | MSVC C++20 compiled static encode/decode, size, generated stream entrypoint and independent byte assertions; runtime parity and bounded failure checks passed |
| Type constraints | Parser tests passed for Boolean precedence, category/exact types, defaults, nested applications, language gates and legacy stateless constraints; C++ template acceptance/rejection checks passed |
| Constrained generic consumers | All eleven language SDKs passed the generic example across its four protocols, including the new numeric roots and unchanged nested wire fixture |
| Readonly function calls | All eleven SDKs passed namespace/member calls, forward binding, exact uint64 results and checked failures |
| Standalone overloads | C++, Java, C#, Swift and Kotlin passed; unsupported backends and Java erased-signature collisions were rejected |
| Evaluation order | Python/Go native counters confirmed one evaluation for a reached call, false for observed NaN and no call in a skipped lazy branch |
| Exception contracts | C++ throwing generic comparisons propagated through expressions and predicates; allocating string/record getters remained potentially throwing |
| Expression example | Generated C++ example compiled and passed static and runtime checks |
| Editor unit tests | All 109 VS Code grammar/navigation tests passed |
| Generated editor navigation | 34,105 bidirectional checks passed across all eleven outputs and C++ profiles |
| Visual Studio shared resolver | 525 source/output checks passed in the real .NET interpreter across all eleven outputs |
| Editor packages | Both 1.1.37 VSIX packages rebuilt and validated |
| Version metadata | `serializer --version` reported compiler 1.15.0 and supported schema language 1.10.0 |

The native build used the installed Windows MSVC toolchain. Language consumers
used installed SDKs, with Rust through WSL. Missing SDKs were not silently
skipped. The final compiler relink also passed after adding direct standard
header includes. No new full Linux, macOS or ARM native regression is claimed.

Run the portable checks with the built compiler and runtime:

```powershell
python test/readonly_function_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/readonly-function-languages --wsl-languages rust
python example/generics/run.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/generic-languages --language all --wsl-languages rust
```

Use `--language` to select installed SDKs. The readonly runner also supports
`--overloads-only`, `--evaluation-only` and `--native-types-only` for targeted
qualification. Set `SERIALIZER_TSC` and `SERIALIZER_KOTLINC` when those compiler
entrypoints are not on `PATH`. Run SDK generation after the native compiler build
finishes to avoid holding the Windows executable open while it is relinked.

Useful generated consumers, binaries, packages and verification logs remain
under ignored `out/`. The native result is recorded in
`out/constexpr-build-test.log`; editor results are in `out/editor-tests.log`,
`out/generated-navigation-tests.log` and `out/vs-navigation-checks.log`.
The all-language readonly results are in `out/readonly-function-sdk-final.log`;
its later obsolete diagnostic-expectation failure was corrected and the full
overload checks passed in `out/readonly-function-overloads-sdk.log`. Separate
observation and native-type logs record the targeted successful reruns. Generic
SDK results are recorded in the two `out/generic-constraint-*-verification.log`
logs.

See [declared function calls](../schema/behavior.md#declared-function-calls),
[Boolean type constraints](../schema/generics.md#boolean-type-constraints), and
[direct codecs and qualifiers](../serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers)
for implemented behavior and supported limits. Ordinary model APIs remain
`constexpr`, allowing runtime inputs; existing immediate compile-time helpers
retain `consteval`. Codec exceptions, allocation and opaque native behavior
retain their existing failure contracts.
