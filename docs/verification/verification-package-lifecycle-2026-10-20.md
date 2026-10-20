# ZIP descriptor and Python package lifecycle verification, 20 October 2026

This record covers compiler/runtime **2.14.0**, schema language **1.20.0**, on
baseline `bd2ee2ef9239b65335367bd8de19be377ce84262` plus uncommitted changes.
The selected checks close **CP-P10**, **CP-P11**, **PY-P01**, **PY-P02**, **PY-P11**,
**PY-P12** and **PY-P19**. Broader OPC conformance, other backends, actual
power-loss testing and platform release gates remain separate.

## Executed checks

Windows CPython **3.14.3** passes **25/25 tests** against actual emitted code,
with no skips. The compiler SHA-256 is
`77352650a6d6295b55c67bebb3be8ca64f94c3ca8133b770c8b87bc762336f59`.
The final direct runner exits successfully; its plain stdout/stderr log is
`out/build/backlog-2026-10-20/python-package-lifecycle.log`.
The subsequent Windows native rebuild passes **5/5 focused CTests** in **3.52 s**,
including the same descriptor corpus in `package_test`; its refreshed compiler
SHA-256 is `1c7d8663872085f66e1c1d7c0c4bf35427015a57dc073001502b586fddc45c80`.
Logs and native `LastTest.log` are retained as `windows-package-lifecycle*`.

The persistent native Linux snapshot passes **6/6 focused CTests** in **26.57 s**:
`package_test`, `package_independent_interop`, `opc_profile_test`,
`python_package_test`, `serializer_opc_exchange` and
`serializer_runtime_literals`. Native package tests pass **42/42 cases**;
generated Python passes **25/25**, including the POSIX directory-sync failure
case. This uses the existing GCC **15.2**, C++20 warnings-as-errors, CPython
**3.14.4**, zlib **1.3.1**, LibXml2 **2.15.2** full-provider configuration.
Linux compiler SHA-256 is
`258f9c4d011855d82bb90f26b234231cf1cc8a9b44d1f36458ddd832d0b434bf`.

Linux source changes were streamed into the native filesystem and checked by
SHA-256. The final build explicitly recompiles the changed Python emitter and
native package test. An earlier timestamp-only setup error left the old compiler
unchanged and exposed the expected red regressions; that failed attempt is
retained separately and is not counted as passing verification. Useful logs,
the source delta manifest, exact fixture hashes and `LastTest.log` are under
ignored `out/build/backlog-2026-10-20/linux-package-lifecycle*`.

| Source | SHA-256 |
| --- | --- |
| `src/python_package_runtime.inc` | `d410feab473a4a847c326d96bd93921c901b9bd3d3f54e4917212f5e50defa46` |
| `test/python_package_test.py` | `2e23bd35e1f5df6f0e6eece64721cd1636eb7912c514e61e3b5382c82ad7d210` |
| `test/package_test.cpp` | `c1b93c3f17499bb99fc10d90bd233f3c011d79ab55922306ab28ee6f13a05964` |

## Descriptor matrix

The [independent generator](../../test/resources/package_descriptor_fixtures.py)
authors literal ZIP fields and checks valid output through standard-library
`zipfile`. C++ and generated Python consume the same **12 valid controls**:
signed/unsigned 32/64-bit descriptors, each with empty/model payloads and a CRC
equal to the optional signature `0x08074b50`. Four further archives corrupt
only the descriptor CRC and must report integrity failures. No production
descriptor implementation changed. These small ZIP64 files qualify descriptor
width; large-offset/count input remains under P09.

## Python sources, sinks and resource accounting

Actual generated facades accept short nonseekable reads while borrowing the
source, reject invalid/oversized read results and short/zero/invalid sink writes,
and keep borrowed streams open and unflushed. Random-access ranges reject
negative, overflowing and out-of-extent requests before charging I/O. Exact and
one-tight cumulative I/O, metadata, expansion, entry and relationship bounds are
checked, including reuse of one exhausted budget.

Two regressions first failed against the previous emitted implementation:
typed/multidimensional memoryviews were measured in elements, and cancellation
from the final sink callback could be missed. Input now uses byte extents and
byte addressing; noncontiguous input is materialized only after buffer/byte
admission. Sink callbacks are followed by a cancellation/deadline check.

## Native nonseekable spool follow-up

The final native spool matrix passes on Windows and Linux. A stored 4-KiB entry
forces bounded 1-KiB reads into an explicitly enabled private spool. Cancellation
after its first read observes the live owned regular file and then verifies its
removal; a source exception after the same prefix also removes it. Linux checks
that the live file has no group/other permission bits. Existing successful,
exception-mask, exact-cap and one-byte-overflow controls continue to pass.
The reader borrows its source, and failed construction publishes no archive.
No production C++ spool change was required.

Windows passes **6/6 focused CTests in 6.28 s**, including **44/44 native package
cases** and **6/6 generated C++ profile cases**, using compiler SHA-256
`4a4f8604bb76361f2e5f33e20e1b7e5176d6916a1b3ed56d98f7b79675d8d813`.
Linux passes **6/6 in 2.81 s**, additionally replaying **28/28 generated Python
tests**, with the Linux compiler and provider profile recorded above. The updated
native test SHA-256 is
`03e415205569fa725c2546cec4723158164935f9337a4e5e3d1edb97f2605938`;
the Linux source-inventory aggregate is
`bcf9ae6591b5938f6316690a7e28323fb8fec31aceaee1f8ee2fb3967a5a17df`.
Logs, exact inventories and native `LastTest.log` files are retained under
`out/build/backlog-2026-10-20/` with the `windows-package-spool` and
`linux-package-spool` prefixes. An initial Linux formatter version-probe timeout
is retained separately; the unchanged retry passed after the formatter responded.
These runs qualify spool cleanup, rather than power-loss behavior or a universal
filesystem permission guarantee.

The stored writer is independently inspected for empty, Unicode-name and
multichunk entries, exact CRC/payload/size, timestamps and both ZIP32/forced-ZIP64
forms. An actual emitted stored package role also passes independent inspection.

## Private spooling, Save and deterministic metadata

A greater-than-2-MiB stored opaque payload forces actual private disk rollover.
Success, late source failure, mid-spool cancellation and a one-tight archive cap
all close and remove the owned spool without closing the borrowed source.
Linux additionally checks that group/other permission bits remain clear.

Save injects temporary-file creation, descriptor ownership transfer, partial
write, file sync, prepublication and replacement failures. Every prepublication
failure retains the old bytes, closes owned descriptors and removes staging.
Postpublication failure retains an independently readable complete new ZIP and
reports `publication_uncertain` with the original cause. Linux also injects the
directory-sync failure after replacement. The descriptor-transfer regression
first failed on Windows: failed `fdopen` left the raw descriptor open and cleanup
masked the original error. Cleanup now retains that descriptor until ownership
transfer succeeds and closes it before unlinking on failure.

All **36 permutations** of the same unordered opaque part and explicitly
identified relationship sets produce identical bytes. Independent ZIP/XML
inspection checks canonical ordering and fixed timestamps/attributes. Semantic
typed-role occurrence order is preserved; arbitrary reordering of ordered model
data is not an equality requirement. Determinism is qualified for each selected
provider version, not across every possible DEFLATE implementation.

Reproduce using the root platform wrapper to build the compiler, then run
`python -B test/python_package_test.py --compiler PATH_TO_SERIALIZER --build out/build/package-lifecycle`.
Regenerate the maintained descriptor fixtures with their script and run the
configured native `package_test`. Temporary generated modules and owned spool/
staging files are removed; useful build outputs and final evidence are retained.
