# Serializer managed runtime: four-toolchain verification

Date: 2026-10-07. Full default C++ Release builds and all configured tests passed on all four toolchains.

## Source and release

Base Git revision: `f535f0230207143a167cf3b5aebcb762623cb36c`, plus the current working-tree portability corrections for compiler/runtime release **1.1.1**. The schema language remains **1.0.0**.

The change contains seven compatibility corrections:

- `include/rohit/stream_io.hpp` checks `__has_cpp_attribute(no_unique_address)` before applying the standard attribute to the synchronization callback. GCC and Clang ABIs that support the attribute retain it. The MSVC ABI retains its existing layout when the standard attribute was previously ignored; the change does not substitute `msvc::no_unique_address`.
- Nine `command_entry` fixtures in `test/command_line_test.cpp` explicitly initialize their existing help string to `{}`. Command names, options, and the default null handler remain unchanged.
- `test/schema_generics_test.cpp` uses `concrete.template serialize_out<Protocol>` for the dependent member-template call inside the templated lambda. This fixes parsing without changing the tested serialization.

- `include/rohit/managed.hpp` compiles the journal recovery predicates in `failure_status()` and `require_idle()` only under `if constexpr (has_journal)`. Disabled-journal stores have a constant null sink; GCC 15 otherwise diagnoses the guarded member call under `-Werror=nonnull`. Enabled-journal checks, status values, diagnostics, ordering, public APIs, member layout, and wire data remain unchanged.

- `test/managed_history_allocation_failure_test.cpp` rebuilds a fresh attempted history by replaying normal growth for each failure position. Successful eviction no longer shifts the baseline deque storage through copy assignment. The test still compares all retained state after failure and checks eviction/redo/labels after success, and now requires exercising failure positions covering both deque map and block allocation.
- `test/managed_collaboration_session_test.cpp` value-initializes the existing proposal and lock-snapshot fixtures and assigns the same context, session, operation, grant, and sequence values explicitly. This avoids GCC 15 Release `-Werror=maybe-uninitialized` diagnostics without changing validation inputs or wire values.

- `src/managed_file_journal.cpp` uses a private nested journal error with a non-template message constructor and a defaulted const copy constructor. Windows Clang generated an exception-copy thunk selecting the MSVC standard library nested wrapper's forwarding constructor, which recaptured the active exception recursively. The private wrapper retains `journal_indeterminate_error` catch behavior and `std::nested_exception` cause inspection while preserving public type layout, fencing, failure status, and wire data. Focused journal regressions check retained exception capture/rethrow and the original cause.

The project patch version changes from 1.1.0 to 1.1.1. No schema syntax, generated public API, serialized format, or payload revision changes are intended.

## Matrix

The results below use the final seven-correction source and journal regression.

All four configurations use x64 Release builds with managed support, tests, installation, SIMD, Zstandard, LZ4, and zlib enabled.

| Platform | Compiler | Full build | Full CTest | Installed consumer |
| --- | --- | --- | --- | --- |
| Windows x64, Visual Studio 2026 | MSVC 19.51.36257.0 | Passed | 78/78 passed, 0 failures, 101.13 s | Passed; MSVC 19.51.36257.0 matches parent, 29 headers |
| Windows x64, Visual Studio 2026 developer environment | Clang 21.1.6, clang-cl / MSVC ABI | Passed | 79/79 passed, 0 failures, 73.22 s | Passed; Clang 21.1.6 / MSVC frontend matches parent, 29 headers |
| Ubuntu 26.04.1 LTS x64 under WSL2 | GCC 15.2.0 | Passed | 80/80 passed, 0 failures, 38.46 s | Passed; GNU 15.2.0 matches parent, 29 headers |
| Ubuntu 26.04.1 LTS x64 under WSL2 | Clang 21.1.8, libstdc++ 15 | Passed | 80/80 passed, 0 failures, 36.73 s | Passed; Clang 21.1.8 matches parent, 29 headers |

Windows uses CMake 4.4.2. Linux uses CMake 4.2.3, Ninja 1.13.2, Python 3.14.4, and clang-format 19.1.7. The Linux host reports WSL2 kernel `6.18.40.1-microsoft-standard-WSL2`. The project baseline is C++20; existing dimension profile/history qualification targets explicitly use C++23.

The native Linux builds use a copy of the repository source to avoid mounted-filesystem build overhead. SHA-256 manifests verify that all 654 tracked build files match the current Windows checkout after the seven corrections, with aggregate SHA-256 `4e94212666ddc08f80b7f7c9f57f6b8974b2bf0fc1d0d0c0982d05839d5a7829`. Both Linux configurations use this same verified final snapshot.

Optional benchmarks, fuzzers, Protobuf interoperability, Java examples, all-language examples, and broader language interoperability remain disabled. The normal C++ test configuration still builds and tests the managed, naming-profile, iostream, compression, include, and generic examples.

## What the full tests cover

The full CTest suite includes managed direct/separated representations; uint32/uint64 identities; transactions and history; collaboration, sessions, ownership, and local synchronization; generated profiles; allocation failures; file streams; appended/sidecar journals; and the process-crash recovery matrix. It also runs the regular compiler, codec, CLI, example, package-version compatibility, and installed-package checks.

The installed-package check installs the actual build, compiles each of the 29 installed `rohit/*.hpp` headers as a standalone translation unit, checks exported managed targets, and runs the linked compression capability consumer. A passing parent build alone is insufficient to identify the consumer compiler: its `CMakeCXXCompiler.cmake` metadata must also match the intended compiler ID, frontend, and version.

The current installed-package test inherits the CTest process environment when configuring its consumer. It does not explicitly forward the parent's compiler path/toolset. Keep `CXX` exported for Ninja/GNU-style configurations throughout both configure and CTest; inspect the consumer compiler metadata afterward. A Visual Studio generator without an alternate toolset selects MSVC.

## Reproduction

Install CMake 3.28+, the selected compiler and standard library, Ninja where used, clang-format 19+, GoogleTest, Zstandard, LZ4, and zlib. Replace the generic dependency prefixes below with existing installations. Use a new build directory for each compiler and generator.

### Windows MSVC

Run from an x64 Visual Studio 2026 developer environment:

```powershell
$env:CXX = "cl"
$env:PATH = "<dependency-prefix>/bin;" + $env:PATH
cmake -S . -B out/build/verify-windows-msvc -G "Visual Studio 18 2026" -A x64 `
  "-DCMAKE_PREFIX_PATH=<dependency-prefix>" `
  "-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=<clang-format-path>" `
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_MANAGED=ON -DSERIALIZER_INSTALL=ON
cmake --build out/build/verify-windows-msvc --config Release --parallel 2
ctest --test-dir out/build/verify-windows-msvc -C Release --output-on-failure --parallel 2
out/build/verify-windows-msvc/Release/serializer.exe --version
```

The verification uses installed dependency prefixes rather than provisioning another vcpkg tree. With this setup, the dependency `bin` directory must be on `PATH` for dynamically linked tests and subprocesses.

### Windows Clang

Run from the same x64 Visual Studio developer environment, which supplies the Windows SDK and MSVC headers/libraries. Select clang-cl explicitly and preserve `CXX` for the installed consumer:

```powershell
$env:CXX = "<clang-cl-path>"
$env:PATH = "<LLVM-bin>;<Ninja-bin>;<dependency-prefix>/bin;" + $env:PATH
cmake -S . -B out/build/verify-windows-clang -G Ninja `
  "-DCMAKE_CXX_COMPILER=$env:CXX" -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_MAKE_PROGRAM=<Ninja-path>" `
  "-DCMAKE_PREFIX_PATH=<dependency-prefix>" `
  "-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=<clang-format-path>" `
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_MANAGED=ON -DSERIALIZER_INSTALL=ON
cmake --build out/build/verify-windows-clang --parallel 2
ctest --test-dir out/build/verify-windows-clang -C Release --output-on-failure --parallel 2
out/build/verify-windows-clang/serializer.exe --version
```

Do not pass `-A x64` to Ninja; the x64 developer environment selects the target architecture. The existing Windows warning policy, including `/W4 /WX` on qualification targets, is retained.

### Linux GCC

```sh
export CC=gcc CXX=g++
cmake -S . -B out/build/verify-linux-gcc -G Ninja \
  -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
  '-DCMAKE_PREFIX_PATH=<gtest-prefix>;<compression-prefix>' \
  -DSERIALIZER_CLANG_FORMAT_EXECUTABLE=clang-format-19 \
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_MANAGED=ON -DSERIALIZER_INSTALL=ON
cmake --build out/build/verify-linux-gcc --parallel 2
ctest --test-dir out/build/verify-linux-gcc -C Release --output-on-failure --parallel 2
out/build/verify-linux-gcc/serializer --version
```

### Linux Clang

```sh
export CC=clang CXX=clang++
cmake -S . -B out/build/verify-linux-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
  '-DCMAKE_PREFIX_PATH=<gtest-prefix>;<compression-prefix>' \
  -DSERIALIZER_CLANG_FORMAT_EXECUTABLE=clang-format-19 \
  -DSERIALIZER_BUILD_TESTS=ON -DSERIALIZER_BUILD_MANAGED=ON -DSERIALIZER_INSTALL=ON
cmake --build out/build/verify-linux-clang --parallel 2
ctest --test-dir out/build/verify-linux-clang -C Release --output-on-failure --parallel 2
out/build/verify-linux-clang/serializer --version
```

All default-on feature options can also be set explicitly: `SERIALIZER_ENABLE_SIMD=ON`, `SERIALIZER_WITH_ZSTD=ON`, `SERIALIZER_WITH_LZ4=ON`, and `SERIALIZER_WITH_ZLIB=ON`.

## Verification evidence

Final Windows MSVC evidence is in `out/build/verification_logs/windows-msvc-seven-retry-build.log`, `windows-msvc-seven-ctest.log`, and `windows-msvc-seven-metadata.log`. The installed consumer passed in 58.34 seconds. A transient executable file-access linker failure in the first pass cleared on the complete build retry.

Final Windows Clang evidence is in `out/build/verify-windows-clang/build.log`, `final-build.log`, `ctest.log`, and `final-metadata.log`. The installed consumer passed in 57.51 seconds. The earlier crashing run is preserved separately as `ctest-before-nested-error-fix.log`.

Final GCC evidence is in `out/build/verify-linux-gcc/native-build.log`, `native-ctest.log`, `native-final-build.log`, `native-version.log`, `native-consumer-compiler.cmake`, `native-consumer-ctest.log`, and `native-installed-headers.txt`. The installed consumer passed in 38.45 seconds. The two `*-final-build-files.sha256` manifests record source equality.

Final Linux Clang evidence is in `out/build/verify-linux-clang/native/build-final.log`, `build-no-work.log`, `ctest-final.log`, `metadata.log`, `last-test.log`, and `consumer-last-test.log`. The installed consumer passed in 36.72 seconds.

Both Linux final rebuilds report no remaining work. Each parent `serializer --version` reports compiler/runtime release 1.1.1 and schema language 1.0.0. All four installed consumers use the matching compiler and independently compile 29 installed headers; package-version compatibility checks also passed.

The Linux suite has two checks beyond the MSVC configuration: `serializer_include_dependencies`, registered when Ninja is discovered, and the Linux-only `stream_allocation_failure_test`. Windows Clang also registers the Ninja-dependent check, giving 79 tests. Optional external zstd/lz4 command-line tools were unavailable on Linux, so those external interoperability tests were not registered; enabled codec backends remained covered by core tests, compression examples, and installed capability checks.

These checks qualify the named x64 configurations and default C++ development targets. They do not qualify every compiler version, platform or architecture, optional SDK, sanitizer configuration, network filesystem, physical power-loss behavior, or device. The managed journal’s documented filesystem and cooperative-locking assumptions remain applicable.
