# Native Windows Rust verification - 2026-10-15

This record covers compiler/runtime **2.0.2**, schema language **1.17.0**, and
native Windows x64 Rust. Earlier WSL qualification remains historical.

## Toolchain and corrections

The installed SDK is `rustc 1.99.0 (b940084d7 2026-09-28)`, targeting
`x86_64-pc-windows-msvc`. Consumers compile as Rust 2021 with `-O -D warnings`
and use only the standard library. Python **3.14.3** runs the qualification
drivers; Visual Studio 2026 supplies the native C++ build environment.

The initial strict Rust behavior build exposed private collision helpers such
as `portable_arithmetic_checked__predicate`, which trigger `non_snake_case`.
Rust now appends `_internal` when avoiding authored names. Repeated collisions
and derived predicate/numeric helper names remain checked. Other backends retain
their previous collision suffixes. Public schema names, generated model APIs,
arithmetic semantics and serialized formats are unchanged.

The shared example runner also attempted to read a nonexistent conversion
fixture. Conversion consumers now execute their own assertions without fixture
arguments, matching their dedicated qualification driver. See
[2.0.2 migration](../../migration.md#compilerruntime-202).

## Results

| Check | Result |
| --- | --- |
| Canonical Windows build and native CTests | Passed: all 134 entries across the full run and focused core rerun |
| Strict Rust behavior | Passed with the final 2.0.2 compiler and warnings as errors |
| Repeated private-helper collisions | Passed: generated Rust execution and the C++ generator regression |
| Basic, collections, complex, versioning, unions/variants, fixed collections and optionals | Passed in Rust and Python, all four protocols |
| Rust/Python interoperability | Passed: 48 producer/consumer/protocol/variant exchanges and frozen positional bytes |
| Shared conversion example runner | Passed: Rust and Python self-checking consumers |
| Native decoder corpus | Passed: 1,029 boundary cases |
| Compact encoding | Passed: 551 cases, strict/lenient boundaries and frozen bytes |
| Digest storage | Passed: 201 cases, nested values and frozen bytes |
| Nested/generic optional values | Passed: 431 cases |
| Owning optional variants | Passed: 80 cases |
| Construction and behavior features | Passed: all 12 driver runs with the final 2.0.2 compiler |
| Derived sizes, conversions, stateless layout, magic, typed magic and arithmetic | Passed |
| Generic models and scoped custom IDs | Passed, all four protocols |
| Managed history, journal, collaboration and authentication records | Passed: each feature has 4 Rust/Python producer/consumer exchanges and 4 malformed-input checks |

The construction/behavior feature matrix includes native implementations and
missing-definition rejection, typed predicates, extended construction, ordinary
and generic owner defaults, array/callable APIs, record comparisons, readonly
calls and overload rejection, projected records, and checked numeric bindings.
Caught panics in rejection tests are expected; the drivers require successful
exit and assert that the invalid operation was rejected.

The final native build used `./make.ps1 test` in the x64 developer environment.
The full CTest run passed 133 entries. An overly specific assertion in the new
generator regression was corrected to check the helper call independently of
its argument wrappers; the rebuilt core test passed with `ctest --rerun-failed`.
Compiler version output, compatible package discovery and the installed-package
consumer checks passed. The implementation did not change during that test-only
correction.

The ten codec feature driver runs passed before the helper correction. The final
2.0.2 compiler then regenerated all eleven corresponding Rust outputs (including
both derived-size policies) byte-for-byte identically to those tested files.
Temporary comparison outputs were removed after verification. The affected
behavior suite was regenerated, compiled and executed again after the fix.

## Reproduction and scope

From an x64 Visual Studio Developer PowerShell, add the installed Rust SDK to
the process path if needed, then use the repository wrapper and maintained
drivers:

```powershell
$env:PATH = (Join-Path $env:USERPROFILE '.cargo/bin') + ';' + $env:PATH
./make.ps1 test -BuildDirectory out/build/make-Release -Configuration Release -Jobs 4
python -B example/run.py --compiler out/build/make-Release/Release/serializer.exe --build out/rust-verification/examples --language rust,python
python -B test/native/run.py --compiler out/build/make-Release/Release/serializer.exe --build out/rust-verification/native --language rust
python -B test/behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --build out/rust-verification/behavior --language rust
```

Additional feature drivers use the same `--compiler`, `--build` and
`--language rust` arguments. Logs and useful generated consumers remain under
ignored `out/rust-verification/`; these are local verification output, not
distributed source. `.gitignore` already covers this directory and the native
build output.

These checks qualify the installed native Windows standard-library Rust
toolchain. They do not establish a minimum Rust version, `no_std`, other target
architectures or a new Linux qualification. Managed wire records do not provide
native Rust history, journal or collaboration engines. Editor documentation was
reviewed; no editor behavior changed, and packages were not rebuilt or installed.
