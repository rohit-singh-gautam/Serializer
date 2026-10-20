# Shared compression exchange slice — 20 October 2026

The separate [compression inventory](../../qualification/compression_exchange/manifest.json)
passed **144 actual generated-model exchanges** and **249 shared corpus decisions**
on baseline `bd2ee2ef9239b65335367bd8de19be377ce84262` plus the uncommitted
**2.14.0** changes. The selected compiler reports schema language **1.20.0**.
This qualification adds no schema-language, runtime, generator or public API changes.

## Selected configuration

The host was Windows 11 build 26300 x64, MSVC **19.51.36260.0**, CMake **4.4.2**,
CPython **3.14.3** with zlib **1.3.1.zlib-ng**, and Oracle JDK **26**. Java output
and its adapter were compiled with `javac --release 17 -Xlint:all -Werror`.
The C++ adapter was built in its own Release Ninja directory with `/W4 /WX`,
against the existing installed **2.14.0** static runtime and vcpkg zlib **1.3.2**.
The current compiler generated all three models, including C++ through the
installed `serializer_generate` helper with an explicit `GENERATOR` override.

The installed runtime prefix was
`out/build/opc-implementation/test/installed_package/Release/prefix-ON-ON-ON-ON`;
dependency discovery used
`out/build/make-Release/vcpkg_installed/x64-windows`. Other providers were enabled
in that installation, but only identity, gzip, zlib and raw DEFLATE participated
in this inventory. No optional Python package is required by this core profile.

## Executed checks

Each of C++, Python and Java produced one complete owning model in each of four
native protocols (`JSON`, `BINARY_NONE`, `BINARY_INTEGER`, `BINARY_STRING`) and
four compression formats (`NONE`, `GZIP`, `ZLIB`, `DEFLATE`). Every adapter then
decoded all 48 producer files into fresh generated values and compared every
field, including the UTF-8/Unicode string with Greek text, an emoji, a newline
and XML-significant characters. Thus 3 producers × 3 consumers × 4 protocols ×
4 formats produced **144 successful model exchanges**.

The runner independently decoded all **48 produced frames** with Python's zlib
provider, explicitly requiring one complete member/stream and no trailing input.
It compared the result against the same producer's ordinary native-codec bytes;
compressed byte equality across providers was not required.

All three adapters replayed the same **83 corpus cases**, with no skips:

- Empty, one-byte, compressible and fixed-seed incompressible payloads, including
  exact zero-output and exact compressed/expanded byte limits.
- One-byte-too-small input/output limits, zero-output encoding, and exact encoding
  output admission followed by a one-byte-too-small output bound.
- A 32 KiB DEFLATE history-window allowance and rejection at 32,767 bytes.
- Selected strict prefixes, trailing bytes, concatenated streams, bad gzip/zlib
  checksums, malformed gzip magic/method/reserved flags and incomplete headers.
- Independently assembled gzip extra/name/comment/header-CRC fields, a corrupted
  header CRC and a zlib stream requiring an unavailable preset dictionary.
- Sixteen valid outer frames whose native model bytes contain a suffix: every
  protocol/format combination must reject the inner complete-object violation.

Each adapter matched all **83 decisions**: 249 decisions in total. Four exact-bound
encoded outputs per adapter were independently decoded and compared with their
inputs: **12 additional standard-frame checks**. Valid and invalid frames are
constructed by the independent runner, rather than by another generated helper.
The random payload uses seed `4850461806051021651`; larger corpus payloads contain
65,537 bytes. Corpus decode cases allow 8 MiB compressed/expanded bytes unless
testing an exact or one-byte-too-small bound. They use the explicit 32 KiB
DEFLATE window policy unless testing its one-byte-too-small boundary.

An explicitly selected nonexistent `javac` was also rejected with exit 1 before
building or exchanging models. It produced no qualification-result JSON. Required
SDKs, declared languages, providers, jobs and oracle mismatches are hard failures.

## Reproduction and evidence

Run the [standalone runner](../../qualification/compression_exchange_qualification.py)
from a configured native compiler environment, with CMake, Java and Python on PATH:

```powershell
python -B qualification/compression_exchange_qualification.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --build out/build/backlog-2026-10-20/compression-exchange `
  --prefix out/build/opc-implementation/test/installed_package/Release/prefix-ON-ON-ON-ON `
  --dependency-prefix out/build/make-Release/vcpkg_installed/x64-windows `
  --make-program "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" `
  --output out/build/backlog-2026-10-20/compression-exchange.json
```

The executed Windows command first loaded the Visual Studio x64 developer
environment and set `TEMP`/`TMP` to the existing ignored workspace test directory.
`--make-program` can be omitted when Ninja is already on PATH. Use the selected
installed runtime's prefix and any provider dependency prefixes; C++ requires
zlib support. `--javac`, `--java`, `--cmake` and `--generator` select explicit tools.
Missing selected tools fail rather than falling back to a smaller language set.

Retained local evidence is `out/build/backlog-2026-10-20/compression-exchange.json`
and the sibling `compression-exchange/` directory. The JSON identifies every
tracked inventory source, generated source, current compiler, installed static
runtime and consumer executable by SHA-256, and records SDK/provider versions,
case IDs and counts. Logs preserve configure/build/generation commands, each
producer/consumer result and the missing-SDK rejection. Generated files, common
fixtures and useful build outputs remain beneath ignored `out/`.

## Remaining scope

This is a selected **Z11** cross-language compression slice, independent of the
eleven-language **S35 XML** inventory. Z11 remains open for optional Zstandard/LZ4,
other language backends and SDK/platform/provider profiles, XML payloads, and the
complete shared-operation context contract. These cases exercise per-message
compressed/expanded/window limits; they establish no cumulative budget sharing,
allocation/work accounting, deadline preemption, provider workspace/RSS ceiling,
throughput or fuzzing result. Existing focused helper suites separately cover
cancellation exception identity, failure fences and borrowed-stream lifecycle.
