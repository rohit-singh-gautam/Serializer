# Published distribution inspection, 20 October 2026

[Distribution guide](../project/distribution.md) · [Verification index](README.md)

This read-only inspection covers published artifacts as fetched at
**2026-10-20 04:14 UTC**. Current source is compiler/runtime **2.14.0**, with
unchanged local extension source **2.0.5**. The observations below qualify a
selected `REL01` profile, including an isolated installed upstream consumer.
Published extensions were inspected without installation; editor-host behavior
remains under QA04. No publication was performed.

## Published editor packages

The official [VS Code listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.serializer-language)
and [Visual Studio listing](https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio)
both identify version **2.0.0**, last updated **13 October 2026**. Their downloaded
VSIX archives pass ZIP CRC validation, and their embedded manifest versions and
identities match the listings. The VS Code package also has matching
`extension/package.json` publisher/name/version and the SHA-256 advertised by
Marketplace metadata. Manifest inspection does not qualify host execution or
navigation behavior, and the older packages were not installed over local builds.

| Published archive | Bytes | SHA-256 |
| --- | --- | --- |
| VS Code 2.0.0 | 83,001 | `a968a91e1dcbc293c7cfe2cd2ce03847d27909a251f31102571354a4984578b6` |
| Visual Studio 2.0.0 | 1,169,739 | `2e43f5328ad31903d5f7f782836d94d6554ae68e06559ab2cda9c7379ab01b7f` |

VS Code retains publisher/name `rohitjairajsingh.serializer-language`. The Visual
Studio VSIX identity remains
`Rohit.Serializer.VisualStudio.40c33349-6c7b-42a6-b843-390d7120b1b9`.
Both local source packages remain **2.0.5**; the inspection changes no extension
package files or Marketplace payloads.

## Upstream vcpkg source and metadata

The official [manifest](https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/vcpkg.json)
identifies **2.0.1**, with `lz4`, `zlib` and `zstd` as default features. The
[portfile](https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/portfile.cmake)
pins `d39fc06efa005bbd88896975e40309c71abe7ec1`, enables managed support and
installation, and applies the host-generator and extent-width patches. It retains
static Windows linkage and excludes UWP. The
[pinned CMake source](https://github.com/rohit-singh-gautam/Serializer/blob/d39fc06efa005bbd88896975e40309c71abe7ec1/CMakeLists.txt)
declares release **2.0.1**, separate runtime/managed targets and `SameMajorVersion`
package discovery. This updates the historical 1.0.0 inspection. The installed
consumer replay below checks that selected port through its actual build pipeline.

| Inspected input | SHA-256 |
| --- | --- |
| Upstream manifest | `8dac981fe19a087caa67e504b195b109887e43a77748a78d061584f1364d16dd` |
| Upstream portfile | `244bbc20cd53217aad5510248f0557a590bf9a94a9726d80aeb7038fb025f3cf` |
| Host-generator patch | `3c3e32907d87beadc4250e0a8f9ff9e2c2947649baba9bcde741e5e2f5757161` |
| Extent-width patch | `c92c125afc519088ee9c6daa438d61cf0946f221d3710a865de398e58d850ede` |
| Pinned CMake source | `fcfc4f5d36ae40c2455a4e9cc25bdf6e362d18ffd5d62e5fb030fc2b4ba8009f` |

## Actual upstream port and installed consumers

The local vcpkg checkout at revision
`062067969e61347807cc7b8c00bdbba29e1b6db6` has byte-identical manifest, portfile
and patches to the independently fetched upstream files above. Its actual vcpkg
pipeline builds and installs **2.0.1** for **x64-windows**, including Debug and
Release libraries and all default compression features, in **3.1 min**. The
downloaded source archive's SHA-512 matches the portfile pin. No overlay port or
current Serializer checkout substitutes for the registry source.

An isolated CMake consumer resolves only that installed prefix and requires
`Serializer::runtime`, `Serializer::managed`, `Serializer::serializer` and
`Serializer::serializer_lib`. The imported compiler works from its relocated
`tools/rohit-singh-gautam-serializer` path and reports compiler **2.0.1**, schema
language **1.17.0**. The installed managed/collaboration record headers exist,
and the package's copyright file contains all three exact source license texts.

**9/9 installed-consumer CTests pass in 1.07 s**: compiler version, an owning
runtime compression probe, the pinned ledger/journal examples and five pinned
collaboration examples (changes, locks, sessions, undo and local synchronization).
The probe requires all six formats to be available and verifies **12 exact
round trips** across empty and 300,000-byte payloads. Every generated model comes
from the installed 2.0.1 compiler; application headers and libraries come from the
installed prefix. Consumer compilation uses MSVC **19.51.36260**, C++20,
`/W4 /WX`; CMake is **4.4.2** and Ninja **1.13.2**. Providers are zlib **1.3.2**,
Zstandard **1.5.7** and LZ4 **1.10.0**.

| Installed artifact | SHA-256 |
| --- | --- |
| Compiler 2.0.1 | `25b5d9a849f5273db2008babafd90e197f8ccdef61a9883360622dd9ba457052` |
| Release runtime | `31a505fea223ce6905d01e89593fc2df05ebd7442622a0f6ce862c5cf69be276` |

The vcpkg executable is
`2026-07-27-98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8`. Tool discovery uses the
existing portable PowerShell executable because the Windows app alias failed on
the first attempt; no port source was changed to resolve that environment issue.
Metrics are disabled and binary-cache access is read-only. Build, downloads,
package staging and installation stay under ignored
`out/build/backlog-2026-10-20/upstream-port/`. Final logs are
`upstream-port-install.log` and `upstream-port-consumer.log`; `provenance.json`
records archive, source, compiler/runtime, generated-record and license hashes.
The useful installed package and reproducible consumer/build output are retained.

This closes the selected REL01 archive/installed-consumer profile. Other
triplets/platforms, Debug consumer execution, editor-host behavior and publishing
current source remain separate gates; 2.0.1 does not acquire current-source APIs.

Downloaded official responses, parsed listing metadata, VSIX archives and the
bounded inspection report remain under ignored
`out/build/backlog-2026-10-20/distribution-inspection/`. The report records exact
hashes and archive members. Other versions/platforms, editor hosts and publication
remain separate gates.
