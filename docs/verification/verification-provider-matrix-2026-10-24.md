# Native provider combinations — 24 October 2026

[Qualification requirements](../pending.md#qualification-and-distribution) ·
[Maintained runner](../../qualification/provider_matrix/README.md) ·
[Verification index](README.md)

Compiler/runtime **2.33.0**, language **1.23.0**. The new maintained provider
runner compiles the actual runtime separately for all **16 combinations** of
Zstandard, LZ4, zlib and XML. Its strict C++23 consumer executes **576 semantic
checks** on Windows x64, MSVC **19.51.36260.0**, MSVC 14.51 library, CMake
**4.4.2**, Ninja, Release. Every build and profile executes serially with **one
worker**. The complete run takes **1,628.390 seconds** on the shared host.

Native dependencies are Zstandard **1.5.7**, LZ4 **1.10.0**, zlib **1.3.2** and
libxml2 **2.15.3**. The runner preserves actual configure/build/run commands,
complete diagnostics, source/probe hashes, linked consumer and CMake-cache hashes
in separate numbered attempts. Incomplete or duplicate inventories cannot produce
a full `passed` result; `--profiles` subsets are explicitly diagnostic.

## Executed decisions

Each format's compiled availability must agree with its independently configured
switch. Enabled identity/Zstandard/LZ4/GZIP/zlib/raw-DEFLATE formats must round-trip
the independently specified eight-byte input, admit exact limits and reject
one-byte-tight input, output and decode bounds. Disabled providers must reject
validation, compression and decompression with the unavailable category.

XML-enabled profiles must preserve independently specified namespace/local-name
and escaped text values through parse/write/parse. Disabled XML must retain its
exact unavailable diagnostic. Every enabled compression format must produce the
same encoded bytes across all provider combinations on this host; identity is a
byte-for-byte ordinary, non-OPC control.

Mask bits are Zstandard, LZ4, zlib and XML in that order, least significant first.
The required decision counts for masks 0–15 are:

```text
33 34 34 35 36 37 37 38 34 35 35 36 37 38 38 39
```

The initial diagnostic compile exposed an MSVC constant-array capture/span
conversion issue in the new probe. Giving its fixed input static storage resolves
that probe-only issue. Its failed attempt remains in `provider-matrix-smoke/`;
the subsequent diagnostic subset and complete 16-profile run pass. Runtime sources
and public behavior are unchanged.

## Evidence and remaining scope

The complete Windows report is retained under ignored
`out/qualification-2026-10-24/windows-provider-matrix/verification.json`, SHA-256
`c30fb5101d0428502da5a88474de37046a094b32b2d064aaf0224bbe54dcd27a`.
The executed runner hash is
`54e725995446dc34103d363ca1c9ab43fbf9110c9f4dfd4acb569b40ed158e82`;
the native probe hash is
`a235cdbc0d017f35b885840dd94a90979b372e02c2f60e4200d786177df320ce`.

This commit completes the Windows provider-toggle slice of QA01. The native
Linux matrix and other current platform/SDK gates are separate executions;
this slice alone does not close QA01.
It does not claim arbitrary caller adapters, another architecture, equal compressed
bytes across different provider versions, provider-internal memory caps or a
performance threshold. README and the integration skill were reviewed; production
APIs, usage, wire formats and release versions are unchanged.

## Native Linux replay

The native Linux execution also passes **all 16 masks and 576 checks**, using
GCC 15.2, libxml2 2.15.2, zlib 1.3.1, Zstandard 1.5.7, LZ4 1.10.0 and the
explicit extracted xxHash 0.8.3 link dependency. The actual run uses two workers
and takes **1,802.074 seconds**. The complete report is
`linux-provider-matrix/verification.json`, SHA-256
`6376ce5dc41ffe75a007fd7a83c83ae52c62472d5826cf15329145dde329fc80`.
This is a separate native provider execution; generated-C strict compilation
failures elsewhere remain [QA-B02](verification-qualification-bugs-2026-10-24.md#qa-b02-strict-generated-c-scalar-rule-initialization).
