# Empty class-valued parameter default qualification - 2026-10-12

This record covers compiler/runtime **1.20.0** and schema language **1.15.0**.
See [construction](../schema/construction.md#empty-class-valued-parameter-defaults-in-language-115)
for syntax, fresh ownership and backend API differences.

## Scope and capability selection

- Used: typed owning record construction and trailing constructor/named-factory
  defaults. Empty literals use schema field defaults; full positional literals
  retain their `1.14.0` behavior. Nested empty records and known schema generic
  applications use the same checked default contract.
- Not applicable: adding fields, payload revisions, managed transactions,
  registries, persistence or collaboration. Parameter omission changes value
  authoring APIs and introduces no durable metadata or protocol changes.
- Unavailable: partial/named record literals, ordinary function defaults,
  unconstrained `Value input = {}`, or extending the owning default contract to
  inherited, managed, packed or native-storage records. Fixed-array literals
  retain exact cardinality. Method-only `storage(none)` blocks remain eligible.

The native fixtures cover omitted and supplied record arguments, factory
defaults, nested empty literals, constexpr creation, fresh mutable ownership,
language gates, malformed shapes and unchanged fresh/missing-field codec
defaults. The portable fixture exercises generated output through
`test/constructor_defaults_languages.py`, including shortened default wrappers.
The prior [owning API qualification](verification-owning-api-2026-10-12.md)
records verification of `1.14.0`, separately from this feature.

## Verification status

The Windows Release build through `./make.ps1 test` completed. Its first test run
passed 129/131 checks; two compiler-rejection controls lacked MSVC's standard
header search environment. After initializing x64 Visual Studio Developer
PowerShell, the final compiler, constructor and core targets rebuilt through
CMake, and the complete configured CTest suite passed **131/131** checks with
four workers in **52.91 seconds**. Package-version compatibility and the installed
package consumer passed. `serializer --version` reports compiler **1.20.0** and
schema language **1.15.0**.

Native coverage also verifies throwing external public default constructors:
omitted calls propagate host failures, wrappers do not infer `noexcept`, and
constant-evaluation eligibility follows explicit host promises. Owning expression
calls and record-valued signatures use the same qualification. Older schema
contracts retain their previous generation.

`test/constructor_defaults_languages.py` regenerated, compiled and executed the
portable fixture successfully in **C++, C, Java, JavaScript, TypeScript, Go, C#,
Rust, Python, Swift and Kotlin**. C++, C and Rust used WSL; other SDKs used Windows.
Final compiler regeneration preserved identical SHA256 hashes for all **12**
tested output files, including the TypeScript JavaScript runtime. The full Linux
native suite was not rerun. Kotlin emitted existing JVM deprecation notices;
consumer compilation and execution passed.

Final native build/test and SDK logs remain under ignored `out/build/` as
`empty-record-defaults-final-build.log`, `empty-record-defaults-final-tests.log`
and `empty-record-defaults-sdk.log`. The requested simple schema and seven-language
generated examples are retained in `out/build/empty-record-defaults-example/`.
Existing ignore patterns cover these artifacts; no new ignore rule was required.

## Editor qualification

Both editor packages use release **1.1.44** and preserve their existing extension
IDs. The schema snippet selects language **1.15.0**. Their existing shared grammar
and navigation implementation already balance empty and nested brace defaults;
new regression coverage verifies the constructor/factory parameter types and the
following parameter/field boundaries, including transitive includes, unsaved
changes and independent generic input scopes.

- `npm.cmd test` in `editors/vscode`: **131/131** editor unit tests passed.
- `./editors/visual_studio/test/test_grammar.ps1`: **390** installed grammar and
  fallback syntax/boundary checks passed across all eleven native language tags.
- The rebuilt `SerializerNavigationTest.exe` passed **1,002** .NET/Jint
  source/output checks against retained generated fixtures in all eleven output
  languages, including fresh/nested default signature boundaries.
- Focused compiler **1.20.0** generation passed **957** bidirectional type/field
  navigation checks for the new language **1.15.0** empty/nested constructor and
  factory defaults across all eleven output languages. A temporary runner reused
  the unchanged `verify(...)` function and fresh-record-defaults fixture from
  `editors/vscode/test/generated_navigation.cjs`.
- `./make.ps1 extension` rebuilt both **1.1.44** VSIX files in `out/extensions`.
  Visual Studio package validation passed. VS Code ZIP checks passed for identity,
  version, canonical grammar/snippets and current packaged README/changelog.

The Windows extension build required network-enabled execution for the locked
NuGet restore after sandbox HTTPS access failed. No extension installation or
interactive editor-host verification was performed. Current VSIX files and useful
editor build outputs were retained; the partial failed packaging log was removed.
The full unrelated generated-navigation matrix was stopped during its existing
conversions batch; the **957** fresh checks cover only the new fixture. Its successful
outputs remain in `out/extension-tests/fresh-defaults-focused-H7mxhW`; the temporary
runner and incomplete full-matrix output were removed.
