# Owning API qualification — 2026-10-12

This record covers compiler/runtime **1.19.0**, schema language **1.14.0**, and
the synchronized editor packages **1.1.43**. Earlier schema contracts retain
their language-selected behavior. See the [owning API guide](../schema/owning_api.md)
for syntax, backend support and lifetime requirements.

## Focused verification

- Generated C++ readonly getters pass native tests for const-reference and field
  address identity, generic storage, friend writes, name/profile collisions,
  temporary-owner and mutation rejection, unchanged native codec bytes and object
  size, transient fields, and managed history. Unsupported owning/view/storage
  and output combinations produce diagnostics.
- Direct construction passes compile-time tests with copyable representations
  lacking default construction and assignment, including open vector coordinate
  types. Nested record/fixed-array defaults preserve unit-Z values, custom
  coordinates, fresh ownership, exact decode defaults, and older language lowering.
- Reciprocal signatures pass compile-time conversions in both declaration orders,
  including namespace-qualified open generics and named instantiation resolution.
  Explicit default construction, host-defined constant observers, ordinary
  constant-evaluable native edits, storage cycles,
  language gates and malformed declaration additions are covered.
- Arithmetic tests qualify exact owning/scalar result identities, typed operands,
  constructor overload selection, trailing/default constructor calls, constant
  evaluation, independent exception specifications, checked rejection categories,
  ambiguous/invalid mappings, and collision-free private helper names.
- The four new C++ examples build through `serializer_generate` and run through
  CTest with warnings treated as errors.

The portable construction/default and arithmetic fixtures each compile and run
with real SDKs for **C++, C, Java, JavaScript, TypeScript, Go, C#, Rust, Python,
Swift and Kotlin**. Construction checks include exact binary round trips and
missing-field JSON decode defaults. C++, C and Rust SDK consumers use WSL; the
other consumers use the installed Windows SDKs. C++ native tests also use MSVC.

Reproduce the SDK checks with `test/constructor_defaults_languages.py` and
`test/arithmetic_languages.py`, supplying the generated compiler, runtime library,
build directory and installed SDK overrides as described by each script's
`--help`. Final local logs are retained under the ignored `out/build/` directory.
After the final headerless-default compatibility fix, regeneration preserved
identical SHA256 hashes for all SDK-tested outputs in both suites, including the
TypeScript JavaScript runtimes. Scanner, compatibility and warning-location
regressions pass **39 tests**, preserving unterminated-comment error cursors and
reentrant source mapping.

## Full native and installed-package verification

The final Windows Release run through `./make.ps1 test` passes **131/131 CTest
checks** with MSVC 19.51 and four CTest workers, in **83.97 seconds**. This includes
the existing managed/history/journal, codec, compiler, naming and language
regressions, all new owning tests/examples, `serializer_package_version`, and
`serializer_installed_package`. The Windows build enables the available
compression backends. `serializer --version` reports compiler **1.19.0** and
supported schema language **1.14.0**.

The final Linux Release run through `make test` passes **123/123 CTest checks**
with GCC **15.2.0** and four CTest workers, in **98.78 seconds**. It includes
the full configured native regression suite, all four owning examples, package
version compatibility and an installed-package consumer. The existing Linux
configuration enables managed tests and SIMD; optional compression backends,
all-language example builds and interop matrices are disabled. The two separate
11-language SDK suites above qualify their actual generated consumers.

The final native logs are `out/build/owning-api-windows-final.log` and
`out/build/owning-api-linux-final.log`. Both runs use the canonical root wrappers
and retain their useful build outputs in the ignored `out/build/` directory.

All four example directories also configure independently against a freshly
installed **1.19.0** package, invoke its installed compiler, build with MSVC
`/W4 /WX`, and pass their individual CTests. Their isolated prefix, build outputs
and logs are retained under `out/build/owning-api-standalone*`. The reciprocal
example uses a shortened target name to fit Windows MSBuild tracking paths.

## Editor qualification

The VS Code suite passes **129 tests**. Shared Visual Studio grammar checks pass
**370 checks**, the native Visual Studio navigation harness passes **912 checks**,
and freshly generated output passes **37,010 bidirectional navigation checks**
across supported languages. Coverage includes exposure names, declaration-site
qualifiers, arithmetic operands, constructor expressions, generated getters,
transitive includes, unsaved edits and cursor boundaries.

`./make.ps1 extension` rebuilds both matching **1.1.43** packages. Package
validation checks identity/version, current grammar/snippets/configuration, and
normalized README links to existing repository documentation. Packages and final
logs are retained under `out/`; installation and publication were not requested.

## Boundaries

Readonly exposure initially supports ordinary unpacked owning C++ records. Native
C++ external/defaulted constructor declarations and independently generic
construction inputs retain their documented backend restrictions. Constructor
calls require an exact public typed signature; unsupported deduction and dynamic
C ownership combinations diagnose. Named schema instantiations remain distinct
owning identities rather than implicit C++ aliases or conversions.
Legacy fixed-array parameter modifiers are not accepted by portable constructor
expressions; direct fixed-array authoring and aggregate defaults are qualified
separately.

Constant evaluation depends on the selected C++ representation and visible host
definitions. Runtime-only operations retain explicit qualification boundaries;
`constexpr` does not imply `noexcept`. No optimized-code or benchmark speedup is
claimed. These additions do not change fields, wire IDs, payload revisions,
managed journal or collaboration formats.
