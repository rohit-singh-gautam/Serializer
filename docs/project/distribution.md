# Source and package availability

[Back to the project overview](../../README.md) ·
[Product positioning](product_positioning.md)

Serializer — State Framework has separate source, compiler/runtime, schema-language,
editor-extension, and vcpkg releases. Descriptions must match the components a
particular distribution actually exposes.

## Choose the dependency for your application

The current source release **2.15.0** provides proprietary-compatible generated
output and a 0BSD application runtime. Packages of this release should include
`LICENSE`, `LICENSE-RUNTIME`, and `LICENSE-GENERATED`, and export the separate
`Serializer::runtime` application target. The compiler and
`Serializer::serializer_lib` compiler API remain GPL-3.0-or-later. Confirm the
license scope and targets of the package you actually install; these permissions
must not be inferred for an older port. See [licensing](licensing.md).
Both current editor source packages are version **2.0.5**; their schema support
and documentation links match the current checkout. Source releases and local builds do
not update published Marketplace listings or the upstream vcpkg registry.

The current 2.15.0 CMake package uses `SameMajorVersion` discovery: versioned
requests for Serializer 1.x do not resolve to it. Select
`find_package(Serializer 2.0 CONFIG REQUIRED)` and regenerate through the
consumer's build rules. Copy/install generated C++ dependency-header trees with
their entry headers, or choose inline mode. Schema language 1.19.0 adds
[transparent value references](../schema/value_references.md) for exporting existing
payload storage without an intermediate owning copy. These references are
export-only; matching ordinary owning readers decode their output. Older supported
1.x contracts remain compatible. See [2.6 migration](../../migration.md#compilerruntime-260)
and [2.0 migration](../../migration.md#compilerruntime-200).

The [source-build guide](../guides/cmake_integration.md#build-this-repository) documents the
current checkout. Fresh source builds enable managed support, SIMD, and the three
compression backends. Using managed state is an application choice; ordinary
generated schemas do not gain managed identity or history from that build option.

The upstream port is installed with:

```sh
vcpkg install rohit-singh-gautam-serializer
```

Package availability can lag the source repository. Check the resolved port
version, source revision, build options, installed headers, exported CMake targets,
and host compiler before relying on features from current source documentation.
The generation helpers and managed APIs described here require a package that
ships them. Do not copy omitted record headers into an older installation; use a
source build or a validated package update.

The root [`vcpkg.json`](../../vcpkg.json) acquires development dependencies and enables
`compression-zstd`, `compression-lz4`, and `compression-zlib` by default. It is not
the port published by `microsoft/vcpkg`. Updating that file does not update the
registry package. [`vcpkg-configuration.json`](../../vcpkg-configuration.json) selects
the development registry baseline; it does not maintain a local Serializer port.

## Inspected distribution baseline: 20 October 2026

The official Marketplace pages and their downloaded VSIX packages both report
**2.0.0**. ZIP CRC and manifest identities/versions pass, and the VS Code archive's
SHA-256 matches the Marketplace metadata. Current local extension source remains
**2.0.5**. The older published packages were inspected without installation.
See the [VS Code listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.serializer-language)
and [Visual Studio listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio).

The current [upstream port manifest](https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/vcpkg.json)
reports **2.0.1**. Its [portfile](https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/portfile.cmake)
pins `d39fc06efa005bbd88896975e40309c71abe7ec1`, enables managed support and
installation, and applies host-generator and extent-width patches. The pinned
source declares compiler/runtime **2.0.1**, separate runtime/managed targets and
`SameMajorVersion` discovery. Default upstream features are `lz4`, `zlib` and
`zstd`; Windows linkage is static and UWP is excluded. Its actual x64-windows
vcpkg build/install passes, followed by **9/9 installed-consumer tests** covering
code generation, runtime/managed targets, compression and the pinned managed
examples. The installed compiler reports language **1.17.0**; it does not provide
current-source language/features simply because same-major discovery succeeds.

The [inspection record](../verification/verification-distribution-2026-10-20.md)
retains downloaded artifact hashes and exact limits. These observations supersede
the older distribution state below; they do not publish current source changes.

## Historical distribution baseline: 8 October 2026

| Surface | Inspected state | Interpretation |
| --- | --- | --- |
| Source inspected 8 October 2026 | Compiler/runtime `1.5.0`, schema language `1.2.0` | Historical source inspection; current release information above and `CMakeLists.txt` are authoritative. |
| Editor source inspected 8 October 2026 | Both extensions `1.1.26` | Historical documentation/metadata update; local packages and published listings are separate. |
| Published Marketplace listings | Both report `1.1.22` | Source changes are not automatically published. |
| Upstream vcpkg port | `1.0.0`, source pin `f1966b6a25c7b43f0257b60b7451ce99c7162f18` | Source-version and package-option inspection is required. |

The then-current upstream manifest and portfile were inspected together with
that pinned source. The historical pin already contained managed history,
journal, and collaboration code; its
[pinned CMake configuration](https://github.com/rohit-singh-gautam/Serializer/blob/f1966b6a25c7b43f0257b60b7451ce99c7162f18/CMakeLists.txt)
defaulted the managed option OFF,
and the old port did not enable it or provide a managed feature. Consequently its
configuration omitted `Serializer::managed` and the generated runtime record headers.
The mere presence of managed source or some installed headers does not establish
a usable managed package. This is a source/packaging inspection, not a completed
installed-consumer build.

That port's features were `zstd`, `lz4`, and `zlib`, distinct from this checkout's
development feature names. Its platform policy excluded UWP and required static
linkage on Windows. The historical pinned CMake package used `SameMinorVersion` discovery compatibility;
current source uses `SameMajorVersion`. Apply the installed package's own version
contract rather than assuming the current-source policy retroactively changes it.

Listing versions were read from the official
[VS Code listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.serializer-language)
and [Visual Studio listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio).
These are dated observations; recheck the actual installed/resolved distribution.

## Prepared upstream work

A package exposing the current source features needs a separately qualified
update from the inspected **2.0.1** port:

1. Select an immutable source revision for the intended compiler/runtime release
   and verify its archive hash. A printed version does not establish a release tag.
2. Map port features/dependencies to that revision's CMake options. Enable managed
   record generation and installation when advertising managed APIs. Do not
   silently rename existing compression features.
3. Build through the port pipeline and test an installed consumer using
   `find_package(Serializer CONFIG REQUIRED)`, code generation, exported targets,
   and the documented formatter/host-generator prerequisites.
   For release 1.7.0 or newer, verify that the generated application links
   `Serializer::runtime`, that `Serializer::managed` uses that runtime, and that
   all three licensing files are installed. Keep compiler API licensing distinct
   from the application runtime in the port's license metadata.
4. Exercise installed managed transaction, undo/redo, journal recovery, and
   collaboration examples before including those capabilities in the port copy.
   Record the actual triplets/linkage modes and untested configurations.
5. Generate registry version/baseline records using the current vcpkg workflow,
   then prepare a separate upstream patch and validation report.

No upstream registry change, Marketplace publication, GitHub settings update, or
website deployment is implied by edits in this repository.
