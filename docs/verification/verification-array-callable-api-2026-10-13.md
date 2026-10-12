# Array and callable API qualification - 2026-10-13

This record covers compiler/runtime **1.22.0**, schema language **1.17.0** and
both editor source packages **1.1.46**. See the
[feature contract](../schema/array_and_callable_api.md) for native/portable
boundaries and migration requirements. Prior dated records remain historical.

## Selected contracts

The source selects explicit ordinary array exposure, writable/readonly access,
const-value indexing, dimension-aware checked logical helpers, native C++ range
borrowing, typed array bindings/generation, callable-local availability,
fresh owner-type field factories, structural constructor delegation and richer
C++ callable signatures. No managed transport, authorization or alternate
runtime is introduced. Existing wire IDs, names, layouts, journal records and
collaboration protocols retain their contracts.

## Verification status

| Check | Status |
| --- | --- |
| Canonical Windows native wrapper, parser/generator and runtime CTests | Passed: 134/134, 79.20 seconds |
| Fresh/current compiler and language version output | Passed: compiler 1.22.0, schema language 1.17.0 |
| Installed-package and compatible package-version checks | Passed in the final native suite |
| Native borrowing, bounds, const policies and copy-only transfers | Passed: final C++20 array fixture |
| Deferred factory side effects, exceptions, identity/missing defaults and wire parity | Passed: focused array fixture, including all four codecs and JSON overrides |
| New portable array helpers in all eleven SDKs | Passed: full eleven-language run, plus a focused post-fix C++ rerun |
| Array bindings/generation and unsupported backend diagnostics | Passed: native regression tests and actual SDK consumers |
| VS Code grammar/navigation source suite | Passed: 133/133 |
| VS Code package | Passed: rebuilt 1.1.46 through the canonical extension wrapper |
| Visual Studio installed grammar/fallback tokenizer | Passed: 424 checks, 11 language tags, 48 installed scopes |
| Visual Studio .NET navigation | Passed: 1,103 source/output checks across all eleven languages |
| Visual Studio package | Passed: rebuilt 1.1.46 and package validation |
| Markdown links, metadata and whitespace review | Passed: 21 changed Markdown files, 907 local links/anchors; no private-name/path matches |

These results belong to the completed runs recorded below. They do not claim
IDE installation, an editor-host UI run or publication. Existing public example
sources are deliberately unchanged.

The final source unit command was `npm test` in `editors/vscode`, using the
locked TypeScript and TextMate dependencies. The installed grammar command was
`./editors/visual_studio/test/test_grammar.ps1`; it loaded Visual Studio 2026's
real grammar assets and checked the missing-provider fallback. These are source
and tokenizer checks, rather than an editor-host UI run. Logs are retained under
`out/build/schema-adoption/array-callable-vscode-units-final.log`,
`array-callable-visual-studio-grammar.log` and `array-callable-doc-audit-final.log`.

The refreshed Visual Studio resolver executable rebuilt with locked NuGet
dependencies and passed **1,103** real .NET/Jint source/output checks against
retained all-language complex, generic and inherited fixtures. The canonical
`./make.ps1 extension` wrapper rebuilt both **1.1.46** packages using PowerShell 7
and the Visual Studio x64 developer environment; Visual Studio package validation
passed. Logs are `array-callable-visual-studio-navigation.log` and
`array-callable-editor-packages-final.log`. No IDE installation or editor-host
UI test was performed for this release.

The final Windows x64/MSVC `./make.ps1 test` wrapper compiled all targets and
passed **134/134** CTest entries in **79.20 seconds**. Installed-package and
package-version compatibility checks passed. The final log is
`out/build/schema-adoption/schema-native-tests-1.22-qualified.log`. The array
fixture passed eleven focused checks, including original-storage aliasing,
coordinate bounds, copy-only transfers, exact native factory exceptions,
constructible non-exact results, fresh/missing/present JSON defaults and
four-codec byte parity. It also covers guarded omission helpers, concrete/open
builtin type identities, generator name collisions, checked pack failures and
nonliteral map/record value APIs under C++20. Earlier stale test negatives and
transient output-link failures were corrected before this successful final run.

The maintained `--array-callable-api` SDK qualifier regenerated, compiled and
executed the fixture in all **eleven** SDKs: C++, Java, JavaScript, TypeScript,
Go, C#, Rust, Python, Swift, Kotlin and C. Checked logical access, readonly/value
policies, fixed-array packs, ownership and supplied overrides passed. C++, C and
Rust used the existing WSL toolchains and Linux runtime; the other targets used
installed Windows SDKs. The successful log is
`out/build/schema-adoption/array-callable-sdk-all.log`. The unchanged Python
constructor/default baseline also regenerated and ran successfully, recorded in
`array-callable-defaults-baseline.log`.

Those eleven SDK passes precede the final C++20 nonliteral facade qualification
fix. Portable sources are unchanged by that fix. The focused C++ SDK rerun also
regenerated, compiled and executed successfully after that change, using the same
maintained fixture and existing WSL runtime. Its separate evidence is
`array-callable-sdk-cpp.log` and `array-callable-sdk-cpp-postfix-runner.log`;
the original eleven-language log is unchanged. `serializer --version` in that run
reported compiler **1.22.0** and schema language **1.17.0**.
The complete native result and the focused post-fix C++ SDK pass qualify the final
source; the other ten backend sources were unchanged by that final C++ fix.
