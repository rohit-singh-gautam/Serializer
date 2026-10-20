# Go owning compression verification — 2026-10-20

This record covers the selected GO-Z01–GO-Z08 whole-message implementation for
compiler/runtime **2.14.0**, schema language **1.20.0**. The selected Windows and
Linux profiles passed actual generated-code execution. Template compilation
alone does not qualify the feature.

## Reproduction

Build the current compiler through the repository wrapper, then run:

    python -B test/go_compression_test.py --compiler out/build/make-Release/serializer \
      --build out/build/go-compression

Use the actual Windows compiler path ending in serializer.exe on Windows.
The runner requires a local Go SDK, disables toolchain/module downloads, and
writes a go 1.22 module. Missing SDKs fail explicitly. CMake registers the same
runner as serializer_compression_go when Python and Go are found.

## Scope and retained evidence

The runner emits actual standalone Go modules, compiles/runs the SDK, consumes
independent Python-produced standard frames, and independently reads Go output
frames. Cases cover native protocols, exact and one-tight byte bounds,
malformed/truncated/trailing/concatenated frames, gzip optional headers/checksums,
provider absence, native decode limits, borrowed stream failure fencing,
short/zero-progress I/O, and cancellation after final application callbacks.
Separate generated modules check collision-selected names, the binary-only
native protocol guard and XML-independent availability.

Before the shared portable emitter changed, the existing
example/schemas/basic/model.serializer was emitted in the other ten languages.
After the Go-only change, those ten files were regenerated at the same paths and
compared byte-for-byte: **10/10 equal**. The retained comparison under
out/build/backlog-2026-10-20/go-nontarget-final/comparison.json pins all hashes,
schema SHA-256 d2749a264b7e204e38dbb82c1d2c8b78639e79d32b1fe86cde8bbd2ea858ac4f,
baseline compiler 975163b4cec10cab5276233ac80d242991a5d007c6ed96c3deca686de83cc1da,
and compared compiler 8c1b18b72486e7227255ad436e8b33a395c32c2b93bf0ef980a53b38f08f2fc2.
This is one representative output per backend, not a universal parity proof.

## Executed profiles

| Profile | Executed result |
| --- | --- |
| Windows amd64, Go 1.26.4, module language go 1.22 | Seven Go test groups passed; 82 independent input decisions and 16 independent output decisions; collision, binary-only and XML-independent modules compiled and ran. |
| Linux amd64, Go 1.26.0, module language go 1.22 | Strict GCC 15.2 compiler rebuild passed; the same seven groups, 82 input decisions, 16 output decisions and three extra generated modules passed. Five focused CTests passed in 9.66 seconds. |

Windows evidence is retained under
out/build/backlog-2026-10-20/go-compression-windows-final.log and
go-compression-windows/verification.json. The tested compiler SHA-256 is
eb43639342f6ecb45ea9c248f3dcf159470e6d355f1e9e16a20d280b940d8a25.
The ordinary generated module was 58,186 bytes; collision, binary-only and
XML-enabled cases were 63,223, 57,352 and 173,828 bytes respectively. These are
specific generated-source measurements, not general memory or throughput claims.

Linux used CMake 4.2.3/Ninja 1.13.2 and the existing verified native
LibXml2 2.15.2/zlib 1.3.1/Zstandard 1.5.7/LZ4 1.10.0 enabled profile.
The CLI was rebuilt through its real CMake target after root make configure;
generated Go itself uses only the standard library with cgo disabled.
The focused tests were serializer_compression_go, serializer_runtime_literals,
serializer_compiler_cli, package_test and python_package_test. This targeted
post-feature replay is separate from the earlier 200/200 native baseline.

Retained Linux evidence under out/build/backlog-2026-10-20 includes
linux-go-focused.log, linux-go-LastTest.log, linux-go-verification.json and
linux-go-source-inventory.json. The frozen 2,155-file source aggregate is
e675b592f680d54fd237f7014302ef5b5c22d26f8119cf171b271af99866df87,
based on the completed P03/P05 package snapshot. The tested Linux compiler
SHA-256 is 5e3d4b1c2695e623518b42aee077e11b423b034837b102c3234599e43e4849ff.
Generated-source sizes matched the listed Windows cases.

Go Zstandard/LZ4 providers (GO-Z09/GO-Z10),
full shared-operation/S35 parity (GO-Z11), broader platform/performance gates,
and an actual Go 1.22 SDK replay remain outside this selected evidence.
