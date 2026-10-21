# OPC fifth-edition Annex B physical-record qualification

21 October 2026. Development source following compiler/runtime **2.21.0**,
targeted for aggregate **2.22.0**; schema language **1.21.0** is unchanged.
This record qualifies physical ZIP work under `OPC-R08`; it does not by itself
close the complete URI/content-type/Relationships/encoding requirement.

The normative source is the official ECMA-376 Part 2 fifth-edition bundle
(December 2021), SHA-256
`1d489dc491168ea1f9e9a59063acc8dd5f02b4ad1d21aa7ec19ba9a58d020c70`,
downloaded from the [ECMA-376 publication page](https://ecma-international.org/publications-and-standards/standards/ecma-376/).
The local retained source is under
`out/build/pending-core-properties-2026-10-21/`. Section 7.3.6 and Annex B are the
independent requirements; acceptance by Serializer is not the oracle.

## Implemented contract

The [physical-record usage contract](../serialization/zip_metadata.md) explicitly
selects Annex B's permitted complete in-memory rewrite on Save. It distinguishes
logical opaque part preservation from in-place ZIP record editing. Mandatory
consumption still applies. New guards consume/ignore the central-directory
digital-signature record, enforce the complete 65,535-byte central header bound,
and reject inconsistent local/central DOS timestamps. Supported extraction
versions are checked independently because ZIP64 central offsets can require
version 4.5 while local records remain at version 2.0.

C/Rust now recognize ZIP64 locator/end records even when classic fields are not
sentinels. Swift does the same and accepts bounded ZIP64 extensible data instead
of requiring an exactly 56-byte end record. C/Rust admit the DEFLATE option bits
already supported by the other package readers.

The native additive API retains physical metadata outside `zip_entry`, preserving
its established 88-byte object layout on the tested Windows ABI. Null metadata
avoids a side table for canonical input. Explicit replay copies entry options
and shares immutable archive metadata after admission to the same cumulative
budget; it regenerates dependent sizes, offsets, CRCs and ZIP64 fields. The
public `entry_options` remains constexpr-constructible and trivially copyable.

## Native and independent checks

Windows MSVC 19.51.36260, CMake 4.4, libxml2 2.15.3 and zlib 1.3.2:

- **69/69** package GTests passed, including ownership/copy, malformed metadata,
  descriptor policy, metadata admission and the existing public-layout assertion.
- `qualification/zip_annex_b_oracle.py` passed **71 native invocations**, including
  **24** metadata replay variants, stored/DEFLATE, both EFS states,
  ZIP32/ZIP64, signed/unsigned descriptors, file/archive comments, attributes,
  versions, ZIP64 extensible data and **38** opaque extra-field IDs.
- Independent Python `zipfile` plus zlib verified output payloads and asserted
  `testzip() is None`; edited payloads required regenerated CRCs/sizes. Physical
  comparisons deliberately exclude dependent ZIP64 sizes because recompression
  can differ between provider versions while preserving the actual payload.
- Mandatory directory-signature records, malformed signature lengths,
  complete-header **65,535 accepted / 65,536 rejected**, unsupported flags,
  malformed/duplicate ZIP64 TLVs and conflicting duplicate fields were checked.

Linux WSL GNU 15.2, CMake 4.2.3, libxml2 2.15.2 and zlib 1.3.1 built the actual
runtime through the root Makefile configure workflow. A strict-warning C++20
probe linked that runtime and passed the same **71** invocations / **24** replay
variants. Unselected zstd/lz4 profiles are not claimed by this record.

The common generated-SDK corpus in `test/package_annex_b_corpus.py` contributes
**31** independently framed required-record controls per language profile to the
existing owning package suites. The actual Windows C, C++, Rust, Swift, Java,
C#, Kotlin, Go, Python, JavaScript and TypeScript SDK/runtime gates passed these
controls. Existing suite cases additionally exercise URI,
MIME, relative references, physical/logical mapping, scoped identity, opaque
extension policies and staged publication.

## Failures retained during development

The first native replay design appended a shared metadata owner to `zip_entry`.
The existing layout test rejected its 88-to-104-byte change; private metadata
storage corrected it and the entire 69-test suite passed. This was not waived.

Initial Java/C#/Kotlin/Go replay rejected five existing valid ZIP64 offset
boundary cases because extraction versions were compared for equality. The
independent fixtures establish why those fields can differ. The implementation
now validates supported versions separately and retains equality checks for
appropriate duplicate fields.

Initial C consumption exposed nonsentinel optional ZIP64 footer recognition.
The source was fixed rather than weakening the fixture. Python's standard
`zipfile` rejects the optional directory-signature record, so the Swift fixture
author strips only that independently bounded record when asking this secondary
consumer to check payload CRCs. The actual generated runtime receives the full
original archive and must consume the signature record.

A first standalone Windows probe launch lacked provider DLL search paths and
returned loader status `0xc0000135`. Correct provider `bin` paths were supplied
before replay. All native test launches inherit Windows error-dialog suppression.
Initial Linux probe link command quoting was corrected without changing source.

## Retained evidence and remaining scope

Ignored useful output, logs, generated SDK inventories, exact commands and JSON
results are retained under
`out/build/pending-package-completion-2026-10-21/`: `windows`, `linux`,
`annex-windows`, `annex-linux`, and `sdk-*` directories/logs. Temporary SDK
projects owned by the existing harnesses are cleaned by those harnesses. No
release or Marketplace publication is part of this work.

Relationships MCE is separately covered by **43** independently authored
fifth-edition cases in every language. The common URI and encoding replay is
recorded in the [metadata qualification](verification-opc-metadata-2026-10-21.md).
Optional non-ASCII
logical naming, interleaving and growth-hint production remain deliberately
excluded profile policies documented separately.
