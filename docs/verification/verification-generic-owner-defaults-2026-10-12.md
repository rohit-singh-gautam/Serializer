# Generic owner-parameter default qualification - 2026-10-12

This record covers compiler/runtime **1.21.0** and schema language **1.16.0**.
See [construction](../schema/construction.md#generic-owner-parameter-defaults-in-language-116)
for syntax, omission behavior and backend boundaries. Earlier
[1.15.0 record-default qualification](verification-empty-record-defaults-2026-10-12.md)
remains a separate historical record.

## Scope and capability selection

- Used: trailing empty defaults for a bare type parameter owned by an ordinary
  generated class, in constructors and named factories. Native C++ emits exactly
  `Frame{}` and only omitted arguments require its default construction. Explicit
  nondefault/nonassignable inputs retain direct bindings without a class-wide
  factory/defaultability constraint. Ordinary codec/default-construction
  requirements remain independent.
- Portable substitution: known supported owning generated records reuse existing
  `1.15.0` fresh-record defaults. Open native and concrete primitive/enum/container
  substitutions require C++ output; other outputs diagnose the unsupported default.
- Not applicable: adding fields, codec field defaults, payload revisions, managed
  IDs, transactions, registries, history, journals or collaboration protocols.
  This change affects parameter omission only.
- Unavailable: independently generic input deduction from omission, ordinary
  function defaults, direct scalar `{}` defaults, partial/named record literals
  and empty fixed-array defaults. Existing record storage/mode checks and exact
  array cardinality remain in effect.

Selected C++ types retain their host default/value-initialization behavior,
including exceptions and constant-evaluation eligibility. The feature adds no
implicit `noexcept` guarantee and no schema factory semantics to `Frame{}`.
`default(factory)` remains available when deliberately different semantics are
required. Each declaring file, including dependencies, must select `1.16.0`;
older headers keep their meaning and rejection behavior.

## Reproduce portable SDK checks

The maintained [SDK runner](../../test/constructor_defaults_languages.py) has an
opt-in `--generic-owner-defaults` mode selecting
[the 1.16.0 fixture](../../test/resources/constructor_defaults_owner_portable.serializer).
It includes the existing `1.15.0` concrete-record fixture and adds a generic owner
specialized to the generated frame type. The consumers check fresh omitted
constructor/factory defaults and explicit supplied overrides. Existing nested
positional-default checks remain on the included concrete owner, because a
concrete record cannot bind to an unconstrained owner parameter before
specialization. C supplies separate owned inputs for its constructor and factory
and checks that successful calls move and zero those inputs.

On Windows, with a current Release compiler and a compatible Linux runtime
library already built, the all-language command is:

```powershell
python test/constructor_defaults_languages.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --cpp-library out/build/make-ReleaseLinux/libserializer_runtime.a `
  --generic-owner-defaults --language all --wsl-languages cpp,c,rust `
  --build out/build/schema-adoption/generic-owner-sdk
```

C++, C and Rust use installed WSL SDKs; Java, JavaScript, TypeScript, Go, C#,
Python, Swift and Kotlin use installed Windows SDKs. The runner requires the
selected SDKs on `PATH`, or explicit `SERIALIZER_JAVA`, `SERIALIZER_JAVAC`,
`SERIALIZER_TSC` and `SERIALIZER_KOTLINC` executable overrides where needed.
Use Java 17 and initialize Visual Studio's x64 developer environment for Swift's
Windows linker. This command generates and builds consumers; it does not build
the Serializer compiler/runtime or install editor packages. Select
`--language python` for a focused check without other SDK prerequisites. Omitting
`--generic-owner-defaults` retains the existing `1.15.0` run unchanged.

## Verification status

The Windows x64/MSVC CMake build and root `./make.ps1 test -Jobs 4` workflow
passed all **131/131** CTests. `serializer --version` reports compiler **1.21.0**
and supported schema language **1.16.0**. Installed-package and package-version
compatibility tests passed. The x64 developer environment and PowerShell 7 were
used; earlier fixture errors and transient output-link failures were corrected
before this final run. The successful log is
`out/build/schema-adoption/generic-defaults-native-results.log`.

The finalized compiler regenerated, compiled and executed the opt-in fixture in
all **11** SDKs in one successful run: C++, Java, JavaScript, TypeScript, Go, C#,
Rust, Python, Swift, Kotlin and C. Fresh omitted constructor/factory defaults,
explicit overrides, existing nested defaults and C ownership checks passed.
The unchanged `1.15.0` baseline also regenerated and executed in Python. Logs:
`out/build/schema-adoption/generic-owner-sdk-final.log` and
`generic-owner-sdk-legacy-python.log`. These checks qualify concrete supported
record substitutions; opaque native types remain C++ only.

Static documentation review checked **21** changed Markdown files, **978** local
link targets and **264** local anchors without unresolved targets. Both package
versions, the VS Code lockfile, package filename and new-schema snippet agree
with **1.1.45** / **1.16.0**. `git diff --check` passed. These checks do not replace
compiler, SDK or editor package execution.

The native checks cover constructor/factory omission and supplied arguments,
fresh custom host defaults, nondefault/nonassignable representations, throwing
or non-constant host defaults, known portable record substitution, and unchanged
wire/default-decode behavior. Language gating retains `1.15.0` owner-default
rejection, accepts `1.16.0` in entries and includes, rejects future headers, and
preserves `1.14.0` direct bindings and `1.15.0` concrete record defaults.
Unsupported scalar/array/function/independent-input cases and non-C++
substitutions pass explicit rejection checks. Comment punctuation no longer
changes default capture; regression tests retain the **4,096-byte** and
**32-level** limits, reject over-limit/unterminated inputs, and preserve quoted
aggregate literals.

## Editor qualification

Both editor source packages use release **1.1.45** and preserve their extension
IDs. The VS Code new-schema snippet selects **1.16.0**. The feature uses existing generic
types and brace-default syntax; no new grammar tokens or resolver implementation
are required. Shared navigation must continue to handle generic parameter types,
following parameter/field boundaries, transitive includes and unsaved schemas.

VS Code `npm test` passed **131/131** checks. Installed Visual Studio TextMate
assets passed **390** embedded syntax and boundary checks across all **11** tags
and **48** installed scopes. The rebuilt native .NET/Jint navigation runner passed
**1,002** source/output checks across all **11** languages, including generic
inheritance, selection endpoints, unsaved includes and cancellation, against
existing real compiler-generated fixtures.

The Windows root `./make.ps1 extension` rebuilt both **1.1.45** VSIX packages;
Visual Studio package validation passed. Installation and interactive editor-host
checks are not claimed. Historical package results remain in their dated records.
Logs are retained under `out/build/schema-adoption/`:
`generic-owner-vscode-unit.log`, `visual-studio-grammar-1.1.45.log`,
`visual-studio-navigation-restore-build-1.1.45.log`,
`visual-studio-navigation-1.1.45.log` and `generic-owner-editor-packages.log`.

## Artifacts and repository hygiene

Verification output belongs under ignored `out/build/` and editor packages under
ignored `out/extensions/`. Existing ignore rules cover these locations; no new
ignore rule was required for this feature. Source, schema fixtures, package
manifests and lockfiles remain tracked. The documentation review report is retained
at `out/build/generic-owner-docs-link-audit.json`.
