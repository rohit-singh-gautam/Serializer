# Default runtime feature verification — 7 October 2026

## Implemented change

Fresh CMake configurations enable managed runtime records, SIMD, and the Zstandard,
LZ4, and zlib backends. Compression remains explicit in application calls.
The repository manifest acquires all compression dependencies through default features.
Existing CMake caches retain their values. Benchmarks, fuzzers, and sanitizer tools
remain opt-in. Tests retain their existing top-level default; managed examples are
part of managed test builds or selected with `SERIALIZER_BUILD_MANAGED_EXAMPLES`.

Normal runtime-only builds generate and install both managed record headers without
a formatter. Explicit managed-OFF installs omit the five source headers that require
those records, while retaining independent journal and editor APIs.

## Completed checks

The following checks describe the original default-feature change. The subsequent
[four-toolchain verification](verification-toolchains-2026-10-07.md) records full
default builds, CTest suites, and installed consumers on Windows x64 (MSVC and
clang-cl) and Ubuntu WSL2 x64 (GCC and Clang), using Release with a C++20 baseline.

Windows x64, Visual Studio 2026, MSVC 19.51, Release:

- Configured the main build with `-U SERIALIZER_BUILD_MANAGED -U SERIALIZER_WITH_*`
  to adopt the defaults, then rebuilt successfully.
- All 77 CTests passed, including managed history, crash recovery, collaboration,
  compression, Protobuf, generated profiles, and the new installed-package regression.
- A separate fresh runtime-only build with `SERIALIZER_BUILD_TESTS=OFF` succeeded
  with a deliberately unavailable `SERIALIZER_CLANG_FORMAT_EXECUTABLE`. Managed
  examples remained disabled, and both runtime records were generated.
- Installed that build into a temporary prefix. A separate consumer compiled each
  of its 29 public headers independently, resolved `Serializer::managed`, and linked
  and ran compression capability checks for all three backends.
- A separate build with managed support and all compression backends explicitly OFF
  succeeded. Its installed consumer compiled all 22 remaining public headers, verified
  the absence of the managed target and record-dependent headers, and confirmed that
  the compression backends were unavailable.
- Both editor packages rebuilt at synchronized version 1.1.22; Visual Studio package
  validation and all 78 Node extension tests passed.

The compression dependency packages were Zstandard 1.5.7, LZ4 1.10.0, and zlib 1.3.2#1.
vcpkg restored them from the local binary cache.

## Reproduce installed-package checks

Build the selected configuration first, then run its registered CTest:

```powershell
ctest --test-dir out/build/make-Release -C Release -R serializer_installed_package --output-on-failure
```

The reusable `test/install_package.cmake` script accepts `BUILD_DIRECTORY`,
`DIRECTORY`, `CONFIGURATION`, `BUILD_MANAGED`, `WITH_zstd`, `WITH_lz4`, `WITH_zlib`,
and `GENERATOR`. Supply `DEPENDENCY_PREFIXES` for compression dependencies and
`PLATFORM`, `GENERATOR_INSTANCE`, or `MAKE_PROGRAM` when needed by the generator.

## Remaining verification and distribution

The subsequent four-toolchain qualification passed the full Linux GCC/Clang
default suites under Ubuntu WSL2. Native macOS, cross-compilation, sanitizer/fuzzer
configurations, and native IDE-host installation were not rerun. The internal
managed target still needs a native host-runnable schema compiler; its
cross-compilation override remains unavailable.
No runtime performance benchmark was performed for this build-default change.

The separately maintained vcpkg port was inspected and left unchanged. It pins
an older Serializer source commit. Publish the upstream change, then update the port
revision/hash and default compression dependency features before distributing these
defaults through the registry. The editor VSIX packages were built without installation
or Marketplace publication.
