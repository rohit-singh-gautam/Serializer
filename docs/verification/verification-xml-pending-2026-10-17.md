# XML pending-work checkpoint — 17 October 2026

Compiler/runtime **2.4.0**, schema language **1.17.0**. Source is the uncommitted
working tree based on `83dd2a4`; no Git commit was created. This record covers the
implemented increment, not completion of [the pending requirements](../xml_pending.md).
The [2.3.0 verification](verification-xml-packages-2026-10-17.md) remains a separate
historical qualification and is not evidence that every new path was exercised.

## Implemented scope

- Configured scalar text with attributes and required collection wrappers across
  all generated XML languages; strict shape, namespace and cardinality checks.
- Bounded local fixed-XSD bundles, identity pins, dependency tracking, supported
  named nested types, primitive aliases, export and directional compatibility tools.
- Native `all`/`binary_none` selection across all outputs, independently of XML.
- C++ ZIP64 and opt-in bounded private spooling, shared profile/reference budgets,
  transactional caller-authored OPC aggregate loads, extension policies and
  canonical package ordering, plus cooperative compression cancellation.

## Executed checks

The focused XSD suite passes **11 tests**, including document/depth/aggregate-byte
limits, path/namespace/import rejection, exact fixed-array export, contract
compatibility and unsupported facet/identity diagnostics. The CLI regression
passes export/reimport/compare, independently recomputed SHA-256 pins, transitive
depfiles, overwrite protection and failure before native-output publication.

The vocabulary probes compile actual generated code with every selected SDK.
They cover simple content with attributes, escaped/empty values, mandatory empty
wrappers, malformed wrapper rejection and explicit prepared-validator directions.
The C++ probe is built separately; the non-C++ runs select `--protocols binary_none`.
The Go review additionally found and corrected pre-allocation depth checks and an
unqualified-child default-namespace reset; final focused results are recorded below.

The package checks include an independent Python ZIP byte writer/reader, forced
ZIP64, Python-produced ZIP64 descriptor fixtures, OPC parsing, aggregate rollback,
opaque preservation, canonical reordered-byte equality and cancellation. These
do not qualify generated package profiles or other language backends.

The native selection matrix passes **22 SDK builds/runs**: eleven outputs for
each of `all` and `binary_none`. Its shared fixture includes immutable magic,
payload versioning, nested/optional/collection/map/variant fields and a derived
payload-size field. Every excluded decoder receives a valid message for that
format. TypeScript checks narrowed declarations, C checks destination preservation,
and CLI overrides are exercised in both directions. Go's restricted derived-size
decoder compiles without the keyed-only `srlKeyStart` local. Shared helpers remain
in some generated runtimes; physical removal of every unused helper is not promised.

`serializer --version` reports compiler **2.4.0** and language **1.17.0**. The
generated CMake package-version check accepts earlier 2.x minor/patch requests and
rejects incompatible major/future requests.

| Check | Final result | Retained evidence under `out/build/` |
| --- | --- | --- |
| Windows XML-enabled native regression | **144/144 CTest tests passed**, including managed recovery, installed-package/header checks, XSD tools, package interoperability and version discovery. | `xml-pending-native-final.log`, `xml-pending-final-build.log` |
| Windows compiler compatibility follow-up | **5/5 affected CTest tests passed** after preserving the old XML-options positional initializer; includes the rebuilt core test executable, CLI, XSD tools and package-version checks. | `xml-pending-compat-tests.log`, `xml-pending-compat-build.log` |
| Windows C++ package/compression focus | **4/4 CTest tests passed**; package unit executable contains 27 cases, including borrowed-hook state/exception checks and compile regressions for legacy `constexpr` policies and temporary-callable rejection. | `xml-pending-package-final-tests.log`, `xml-pending-package-final-build.log` |
| Windows native format profiles | **22/22 language/profile SDK builds and runs passed**. | `protocol-selection-qualified.log`, `protocol-selection-qualified/qualification.json`, `protocol-selection-qualified/source_sizes.csv` |
| Windows generated XML vocabulary | C++, Java and JavaScript; TypeScript, Go, C#, Rust, Python, Swift, Kotlin and C passed their maintained probes. Go was rerun after its final namespace/depth corrections. | `xml-pending-shapes-final.log`, `xml-pending-shapes-refresh.log`, `xml-pending-go-final.log` |
| Linux XML-enabled compiler/runtime and focused regression | Build succeeded; **6/6 selected CTest tests passed**: package unit/interoperability, compression stream/allocation, XSD unit and XSD tools. | `xml-linux-on/pending-final-selected-build.log`, `xml-linux-on/pending-final-selected-tests.log` |
| Linux minimal compiler/runtime and focused regression | XML and all built-in compression providers disabled; build succeeded and **4/4 selected CTest tests passed**. | `xml-linux-minimal/pending-test-build.log`, `xml-linux-minimal/pending-final-selected-build.log`, `xml-linux-minimal/pending-final-selected-tests.log` |
| Linux public API link/run probes | Both profiles passed typed numeric/string reference indexes, scratch reservation and a stored forced-ZIP64 round-trip through `package_profile.hpp`. The legacy XML-options initializer also compiled with strict GCC warnings. | Both profiles' `pending-header-build.log`, `pending-header-test.log`; `xml-linux-on/pending-options-compatibility-build.log` |

Linux checks use the selected subset above; the complete Linux native/managed
CTest suite was not run. Windows uses MSVC 19.51 and libxml2 2.15.3; Linux uses
GCC 15.2, CMake 4.2.3 and system libxml2 2.15.2. Both Linux profiles disable
Zlib, Zstandard and LZ4, so these checks do not qualify Linux DEFLATE or other
built-in compression-provider behavior. Managed Linux builds are disabled for
this checkpoint. Shared-filesystem build durations are not performance measurements.

## Reproduce

Run the native Windows tests from an MSVC developer PowerShell so negative
compile tests can find standard-library headers. Build directories below reuse
the dependencies and SDKs already installed on this host.

```powershell
./make.ps1 test -BuildDirectory out/build/make-Release -CMakeArgs @(
  '-DSERIALIZER_WITH_XML=ON', '-DVCPKG_MANIFEST_INSTALL=OFF')

python test/xml_schema_tools_test.py out/build/make-Release/Release/serializer.exe

python test/xml_shapes_test.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --cpp-executable out/build/make-Release/test/Release/xml_shapes_cpp.exe `
  --build out/build/xml-pending-shapes --protocols binary_none

python test/protocol_selection_languages.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --cpp-library out/build/make-Release/Release/serializer_runtime.lib `
  --build out/build/protocol-selection-qualified
```

`VCPKG_MANIFEST_INSTALL=OFF` reuses an already installed dependency tree; omit it
when dependencies must be installed. The SDK commands require the documented
native SDKs, TypeScript/Kotlin launchers, runtime DLL search paths and pinned XML
dependencies. A clean Swift compiler environment is required on this Windows
host; globally injected manual MSVC `INCLUDE`/`LIB` values crashed its frontend.
Supplying those variables only to C/C++ compiler child processes resolved it.

The Linux runs use the root Makefile and a previously installed native GTest
1.18.0 dependency under `out/build/xml-linux-gtest-install`. Use an available
GTest installation instead when reproducing on another host. These commands
reuse the existing build configurations; final test selections are intentionally
narrower than the complete Linux suite.

```bash
common_flags="-DSERIALIZER_WITH_ZLIB=OFF -DSERIALIZER_WITH_ZSTD=OFF -DSERIALIZER_WITH_LZ4=OFF -DSERIALIZER_BUILD_MANAGED=OFF"
gtest_prefix="$PWD/out/build/xml-linux-gtest-install"

make all BUILD_DIR=out/build/xml-linux-on JOBS=2 VCPKG_ROOT= \
  CMAKE_ARGS="$common_flags -DSERIALIZER_WITH_XML=ON -DSERIALIZER_BUILD_TESTS=OFF"
make all BUILD_DIR=out/build/xml-linux-minimal JOBS=2 VCPKG_ROOT= \
  CMAKE_ARGS="$common_flags -DSERIALIZER_WITH_XML=OFF -DSERIALIZER_BUILD_TESTS=OFF"

make configure BUILD_DIR=out/build/xml-linux-on JOBS=2 VCPKG_ROOT= \
  CMAKE_ARGS="$common_flags -DSERIALIZER_WITH_XML=ON -DSERIALIZER_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=$gtest_prefix"
make configure BUILD_DIR=out/build/xml-linux-minimal JOBS=2 VCPKG_ROOT= \
  CMAKE_ARGS="$common_flags -DSERIALIZER_WITH_XML=OFF -DSERIALIZER_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=$gtest_prefix"

cmake --build out/build/xml-linux-on --parallel 2 --target \
  serializer package_test package_interop compression_stream_test \
  compression_allocation_failure_test xml_schema_test
cmake --build out/build/xml-linux-minimal --parallel 2 --target \
  serializer package_test package_interop compression_stream_test \
  compression_allocation_failure_test

for profile in xml-linux-on xml-linux-minimal; do
  ctest --test-dir "out/build/$profile" --output-on-failure \
    -R '^(package_(test|independent_interop)|compression_(stream_test|allocation_failure_test)|xml_schema_(test|tools_test))$'
done
```

## Boundaries

Tree-free XML decoding, XML CPU/peak-memory/encoded-size benchmarks, complete
XSD facets/content models/identities, broader model mappings, HTML, runtime bundle
provider parity, general compression/package backend parity, generated profiles
and managed document/sidecar recovery remain unfinished. Output namespace
declaration accounting and temporary allocation limits still differ by backend.
Native-profile generated-source sizes are not XML performance measurements.

Useful generated output, dependency caches and final verification logs are
retained beneath ignored `out/build/`. Disposable XSD/probe directories are removed.
