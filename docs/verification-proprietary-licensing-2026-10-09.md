# Proprietary output and Windows test verification (9 October 2026)

Compiler/runtime **1.7.0** separates the 0BSD application runtime from the GPL
compiler. Schema language **1.2.0** and existing wire formats remain unchanged.
The editor packages are both **1.1.27**.

Generated output preserves leading copyright/license notice groups from entry
and included schemas across all eleven output formats. Declaration-free files
retain their notices through the CLI and the additive `generate_schema` compiler
APIs. Source notices do not assign ownership of Serializer's support code or
remove third-party terms. See [licensing](licensing.md),
[`LICENSE-GENERATED`](../LICENSE-GENERATED), and
[`LICENSE-RUNTIME`](../LICENSE-RUNTIME).

## Completed checks

| Check | Result |
| --- | --- |
| Sequential MSVC Release build | Passed with warnings treated as errors; runtime, compiler, test consumers, and enabled examples rebuilt |
| Full CTest suite | **88/88 passed**, 40.84 seconds, with vcpkg DLL directories removed from `PATH` |
| GoogleTest consumers | All eight executables passed; **569 individual tests**, including **368 core tests in 48 suites** |
| Schema notice regression cases | **7/7 passed**, covering all eleven outputs, includes/order/deduplication, generic lowering, absent notices, empty schemas, and comment control sequences |
| Generated notice compilation | C11 and C++20 passed MSVC `/W4 /WX`; Java passed `javac --release 17 -Xlint:all -Werror` |
| Embedded runtime literal guard | All nine templates passed the explicit 0BSD prologue and raw-literal size checks |
| Installed package | Public headers, runtime-only linkage, managed support, compression capabilities, and all three licensing files passed |
| Version metadata | CLI reports compiler **1.7.0**, schema **1.2.0**; package-version tests accept earlier compatible 1.x releases and reject future/incompatible versions |
| VS Code extension | **86/86 unit tests** and **17,037 fresh-output navigation checks** passed across all eleven formats |
| Visual Studio shared resolver | **176 checks** passed against fresh output using the .NET/Jint interpreter |
| Editor packaging | Both **1.1.27** packages rebuilt; Visual Studio VSIX validation passed |

## GoogleTest and Windows DLL staging

The previous registry baseline selected GoogleTest **1.17.0#3**. The manifest now
requires **1.18.0 or newer**, and CMake rejects older test packages. The installed
vcpkg port is **1.18.0**, verified through its package metadata. It was the current
[upstream vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/gtest/vcpkg.json)
when checked on this date. Other dependency selections retain their existing
registry baseline.

Windows test-directory executables stage transitive imported shared dependencies
beside their binaries using CMake target metadata. The staged `gtest.dll` and
`gtest_main.dll` hashes match the configured vcpkg **1.18.0** package. Explicitly
empty and undefined DLL lists both pass, supporting static dependency builds.
Release was built and executed; Debug configuration selection was reviewed
against imported target metadata, without a separate Debug rebuild.

GoogleTest is a repository-test dependency. Generated application code and
`Serializer::runtime` do not link it. See
[Windows test runtime DLLs](cmake_integration.md#windows-test-runtime-dlls).

## Logs and limits

Final native configure, build, CTest, dependency-provenance, and generated-notice
logs remain under `out/build/verification_logs/licensing-*.log`. Extension and
vcpkg installation logs remain under `out/build/proprietary-licensing/`.
Useful build output, the installed-package consumer, the isolated GoogleTest
dependency prefix, and both VSIX packages were retained under ignored `out/`.
Disposable probes and the temporary generated-navigation directory were removed.

Native editor-host integration, extension installation, and publication were not
performed. These checks verify the current source build; they do not update or
relicense older published packages.
