# C++23 minimum qualification — 23–24 October 2026

Compiler/runtime **2.30.1**, schema language **1.22.0**, and both editor packages
**2.0.13** implement the C++23 build migration. Platform qualification is in
progress; this record does not yet close `CPP01`.
The source is based on Git revision
`67feaaccef2b1e4a0575a4a612b4b747bfcea05c` plus this migration's working-tree changes.

## Implemented contract

The root CMake build defaults to C++23, rejects explicit older standard requests
before compiler/dependency discovery, requires the selected standard and disables
compiler extensions. `Serializer::runtime` exports `cxx_std_23`; the compiler
library, managed records and generated consumers inherit that usage requirement.
Separately compiled provider/qualification targets use the same minimum. The
Windows and Linux root wrappers retain common CMake defaults and checks.

Configuration compiles public standard checks and exercises `std::byteswap` and
`std::span`, including `if consteval` used in generated checked arithmetic.
Public stream/scalar headers reject C++20 and missing language/library feature
macros. The integer byte-swap path uses `std::byteswap` directly; supported scalar
APIs, generated names, schemas, payload identities and wire formats are unchanged.
Examples, direct SDK qualification and negative constexpr probes select C++23 or
later. Historical migration consumers retain the old release's actual standard.

The Clang/libstdc++ build exposed eager construction of a vector of an incomplete
XML wrapper type. Using default initialization instead of a default member
initializer preserves the aggregate, empty vector, layout and ownership behavior.
The initializer workaround is specific to Clang with libstdc++; GCC retains its
explicit initializer under `-Weffc++`. Focused controls compile both public XML
aggregates, empty/default construction and wrapper insertion under strict GCC and
Clang warnings. A separate explicit pointer comparison avoids a clang-cl/MSVC STL
vectorized-find instantiation that otherwise assumes scalar element widths for
the compiler's two-pointer XSD declaration identity. Equality meaning is unchanged.
Generated arithmetic uses `if consteval` to avoid Clang's constant-evaluation
warning on the former `const bool` trial initializer. Existing readonly arithmetic
static assertions and runtime NaN/infinity/overflow tests retain their contract.
Clang's advisory about exceeding C++'s minimum guaranteed string-literal length is
waived only on eight compiler sources embedding the maintained backend runtimes;
other strict warnings and runtime/consumer translation units retain their policy.
GCC's disabled-journal collaboration specialization now discards the prepared-save
virtual-call check at compile time. Enabled journals keep the original runtime
guard and error. Installed SDK-only examples select the compiler component before
any native language is enabled, and upgrade to runtime discovery when needed;
the package check exercises both phases against the installed release.
The QName-list fixture keeps its expected-name array in a named local so its
span comparison is warning-clean under GCC 15 without weakening the assertions.

## Configurations and current results

Builds and logs remain under ignored `out/build/cpp23-overnight/`. Windows MSVC
reuses `out/build/ReleaseWindows/` and its already installed provider dependencies.

| Platform and toolchain | Configuration | Status |
| --- | --- | --- |
| Windows x64 MSVC 19.51.36260, MSVC 14.51 library, CMake 4.4.2 | Release, C++23 requirement mapped by CMake to `/std:c++latest`, `/W4 /WX`, SIMD, XML and all compression providers | Full build and all 305 configured tests pass across the main run and provider replay; final managed-guard follow-up described below |
| Windows x64 clang-cl 21.1.6, MSVC 14.51 library | Release, CMake `-clang:-std=c++23`, `/EHsc /W4 /WX`, same providers | Compiler/runtime/managed records, all 76 installed headers, linked capability consumer, C++/C/Python examples, compiler-only/native discovery transition and standard admission pass |
| Ubuntu WSL x64 GCC 15.2, libstdc++ 15, CMake 4.2.3 | Release, `-std=c++23`, warnings as errors, SIMD, XML 2.15.2, zlib 1.3.1, Zstandard 1.5.7 and LZ4 1.10.0 | Final all-provider build running |
| Ubuntu WSL x64 Clang 21.1.8, libstdc++ 15 | Same native providers and C++23 mode | All-provider compiler/runtime/managed records, installed headers/examples, compiler-only/native discovery transition, standard admission and package-version compatibility pass |

The Linux builds use the Windows-mounted source/build directory. Journal
qualification must use native Linux temporary data, preserving the separately
documented mounted-filesystem journal replacement limitation. Zstandard/LZ4 and
their xxHash dependency are official Ubuntu package archives extracted under
`linux-provider-deps/`; archive hashes and actual linked-library paths are retained.
Their package revisions are `1.5.7+dfsg-3`, `1.10.0-8` and `0.8.3-2build1`.
Native macOS, ARM, Windows x86 and newer explicitly selected standard modes need
separate qualification.

The MSVC full suite initially passes 294/305 tests in 766 seconds; the remaining
eleven need the pinned `lxml==6.0.2` test provider. Their replay passes 11/11 in
6.71 seconds after staging dependencies under `python-deps/`. Captured independent
provider versions are lxml 6.0.2, libxml2 2.11.9, xmlschema 4.2.0 and elementpath 5.1.4.
After the final disabled-journal template correction, seven of eight focused
Windows commands pass in a parallel replay. One appended discard fault case
reports an unexpected indeterminate full Save. That exact case passes in isolation,
and the complete 139-case direct/journal target passes serially in 13.33 seconds
(one sidecar-only case is inapplicable). The parallel failure remains retained;
its host-level cause is not established by these controls.

## Editor packages

Both local `2.0.13` packages rebuild and validate. VS Code passes **160/160 tests**;
the native Visual Studio cache-navigation harness passes **128 checks**. Installed
Visual Studio grammar assets pass **546 checks** across all eleven language tags.
Fresh real-compiler outputs pass **45,292 bidirectional navigation checks** across
all eleven ordinary outputs and the C++ naming/storage layouts. A broader native
Visual Studio complex-fixture request exceeds its 256 MiB interpreter allocation
budget (272,497,240 bytes charged); this remains a separate native-navigation gap.
These checks preserve the actual harness scope; they do not establish interactive
host qualification. An attempted hidden Visual Studio COM host did not start
(`CO_E_SERVER_EXEC_FAILURE`). No editor installation or Marketplace publication
was performed.

Package artifacts are `out/extensions/serializer-vscode-2.0.13.vsix` and
`out/extensions/serializer-visual-studio-2.0.13.vsix`. Packaging required locked
npm/NuGet restore outside the network-restricted sandbox. Initial sandbox failures,
the host-start failure and final successful verification logs are retained.

Their SHA-256 hashes are respectively
`b897eadddba5a583285ae1862d72fdbb07b47dbf8a295eae0e7af766efa6e7fd`
and `596ea0247efa4e2eec5d4ffe29ad0f9aff097c9934fd2ab71cfb04bb40d1612b`.

See [migration steps](../../migration.md#c23-minimum-in-2301),
[build guidance](../guides/build_and_generation.md) and the
[storage/index requirement](../design/database_storage_requirements.md#c23-minimum-and-one-qualified-dependency-source).
