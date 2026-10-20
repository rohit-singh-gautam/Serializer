# Fixed singleton package relocation — 21 October 2026

[Pending requirements](../pending.md) · [Preservation contract](../serialization/xml_packages.md#fixed-singleton-path-canonicalization)

Compiler/runtime **2.19.0** broadens fixed singleton role canonicalization and
preservation/collision qualification across all eleven generated outputs. Schema
language **1.21.0**, existing native model/wire identities and payload revisions
retain their contracts. Both editor extensions remain **2.0.10**: grammar, output
layout and navigation destinations are unchanged; their READMEs, changelogs and
Marketplace payloads were reviewed without changing extension content.

This completes `XML-B22` within the explicitly supported XML/package graphs. It
combines the previously executed [XML extension policy/collision suites](verification-xml-tokens-2026-10-19.md),
[QName scope qualification](verification-qname-scopes-2026-10-20.md), all-eleven
package policy/required-capability checks in the
[105-item batch](verification-pending-105-2026-10-21.md), and the new relocation
matrix below. Those records retain their dated source snapshots and capability
gates. This does not close arbitrary XML extension graphs (`OPC-R02`), dynamic
role renames/pipelines/target-mode profiles (`OPC-R06`), hard resource guarantees
(`XML-B21`), lexical preservation or signatures.

## Implemented behavior and compatibility

Java, JavaScript/TypeScript, C#, Kotlin and Swift loaded extensions retain private
origin-to-role provenance for fixed singleton roles whose discovered path spelling
differs from their declared canonical output path. C++/Python retain their public
relocation representation but now capture only actually moved roles, preserving
the ordinary no-relocation path. C adds canonical encode/write/Save functions,
Rust adds qualified `CanonicalPackage` methods, and Go adds canonical Load
functions. Existing C/Rust discovered-path writers, Go Load behavior and their
public aggregate/option layouts remain valid. Helper identifiers retain collision
selection, and caller-selected native codecs share the operation budget.

Each preserved internal target resolves in the original source scope before
simultaneously mapping either endpoint. Raw suffixes beginning with the first
literal `?` or `#`, inert external URI references, unmoved target spelling and
opaque part bytes retain their values. C/Rust accept relative external URI
references without weakening absolute relationship-type URI validation. Generated
relationship ID algorithms retain their previous backend-specific behavior.

Removing an optional moved role retains its origin ownership: a preserved source
or target cannot silently bind to another role occupying that URI after a swap.
Occupied destinations, ambiguous origins, dangling links and source-local ID
collisions reject before aggregate/final-file publication. Repeated encode/Save
does not mutate caller extensions, typed values or opaque bytes. Private rebinding
uses shared work/allocation admission; exact observed limits succeed and one-unit
reductions reject. Admission estimates are not process-memory peak guarantees.

Swift previously rewrote preserved public relationships eagerly during load. It
now keeps the original context until composition. Consumers must keep the loaded
extensions attached to edited inputs; rebuilding them from public arrays does not
transfer private provenance. See [migration](../../migration.md#compilerruntime-2190).

## Independent corpus and actual SDKs

`test/package_relocation_test.py` authors ZIP/OPC/XML inputs independently, invokes
each actual generated consumer, and inspects completed archives with Python's
standard ZIP/XML providers and a separate URI resolver. The shared schema has two
fixed singleton XML roles, a dynamic child sourced from the first role, and two
opaque preserved payloads. Typed IDs and source occurrence indexes are inspected
after canonicalization. Every backend consumes nine cases on each host:

- Successful alternate paths, a simultaneous path swap, unchanged paths, and
  removal without retained references.
- Rejected occupied destinations, a source-local relationship ID collision,
  removal with retained links, and removal inside a swap.
- Rejected duplicate role origins before publishing a loaded aggregate.

The ID collision is metamorphic: a successful no-move output reveals that
backend's existing generated child ID. Only an unknown edge's ID in the original
scoped source is changed to collide after rebinding; the revised input is checked
independently as valid before execution. No backend ID algorithm is assumed or
changed to make the test pass.

Each successful case emits encoded, repeated and saved forms. C/Rust additionally
verify legacy discovered-path output and canonical reload idempotence. The C
consumer invokes every new canonical entry-point variant, including selected
native descriptor callbacks and their userdata/model identities. Rust exercises
its qualified writer; Go uses selected codecs from a separate consuming package.
TypeScript compiles a separate strict consuming API, and Kotlin compiles the
library and consumer as separate JARs. Failures preserve a pre-existing Save
destination and original caller cancellation/fault objects where applicable.

| Output | SDK assertions per host | Independently inspected outputs per host |
| --- | ---: | ---: |
| C++ | 34 | 12 |
| Python | 41 | 12 |
| C | 249 | 17 |
| Rust | 60 | 16 |
| Go | 70 | 12 |
| Java | 91 | 12 |
| JavaScript | 107 | 12 |
| TypeScript | 111 | 16 |
| C# | 84 | 12 |
| Kotlin | 91 | 12 |
| Swift | 94 | 14 |
| **Total** | **1,032** | **147** |

The matrix comprises 198 input decisions, 2,064 SDK assertions and 294 independently
inspected archives across Windows and Linux. Linux C repeats the complete extended
matrix with AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer enabled;
Windows C repeats it with AddressSanitizer (498 additional sanitizer assertions
across the two hosts).
Existing owning-package, application-reference, shared-codec and selected-codec
regressions are retained with exact source/provenance bridges for unaffected
generators. A strict GCC fixture initializer warning was corrected using `{0}`;
it did not require a generated runtime change.

## Provenance, regression checks and limits

Evidence is retained under ignored `out/build/pending-relocation-2026-10-21/`.
Compiler/runtime manifests cover 406 maintained files captured before building
and checked unchanged afterward. Immutable compiler images retain version output,
deployment dependencies, hashes, actual commands, generated source hashes,
consumer hashes, input manifests and independently inspected output hashes.
Final Windows compiler SHA-256 is
`45e098593fe9f95e8f223720908796a9702d4ab76ebd6ce6b04d66db47fe3978`.
Final Linux compiler SHA-256 is
`192e904428eead968ede26bf702cd4f143c934d191b1094f51101990c9b0ade0`.

Earlier actual SDK runs use the recorded compiler images in their reports.
After the final C#/Swift index-admission correction, those two SDKs are replayed;
the other nine generators are bridged by exact re-emission comparisons. The JVM/
JS bridge includes 23 retained outputs from new and existing SDK scopes. The
native bridge covers six C/Rust/Go/Python/C++/C# outputs. A separate final-ELF
bridge re-emits all eleven representative outputs. Cross-host generated
sources and final archives are compared where the provider/options contract
permits byte equality; general compressed-provider equality remains separate.
All 147 paired archives have identical uncompressed entry bytes and checked
metadata projections; 126 pairs are byte-identical as complete archives. The
remaining Python/C# pairs differ in compressed representation and dependent
offset/size fields. This is retained as an explicit compressed-byte qualification
boundary for the next reproducibility batch.

Both full native trees build successfully. The selected native XML/package/core
regression set passes 46 CTests per host. Final compiler/runtime-literal/version/
registered relocation replays pass five Windows checks plus four Windows CLI/core
checks, and six Linux checks. Package-version compatibility and actual
`serializer --version` report 2.19.0 with schema language 1.21.0.
The final integrated evidence is `integrated-verification.json`, SHA-256
`3a45451e4141f9f5f6919291ab513bd78b8f7e88703c9183159f7bfdd68eea69`.

CMake registers eleven guarded relocation SDK tests, with strict native consumer
builds. Native XML/package/core/CLI checks and version/package compatibility are
recorded separately from actual external SDK drivers. No official 3MF/Annex B
conformance, arbitrary dynamic rename, provider-internal hard allocation/preemption,
lexical XML, signature or external release-publication claim follows from this
matrix. Required unsupported XML document-companion package graphs still reject.

Swift additionally tests combining Unicode marks immediately after query/fragment
delimiters. A strict actual SDK probe exposed character-cluster iteration hiding
the ASCII delimiter; the fix finds Unicode scalar boundaries instead. The final
maintained consumer adds three direct resolution checks and two actual encoded/
saved archives, independently inspected for exact UTF-8 suffixes, typed values,
opaque bytes and closure. The earlier failing probe is retained.

The C# compatibility suite passes all 241 checks and 180 fixture decisions on
Windows and on Linux's native temporary filesystem. Its initial `/mnt/c` DrvFS
run fails the existing POSIX spool-mode assertion (240 pass, one fail); this is
retained separately. No permission check is disabled. The POSIX run copies useful
evidence into the task directory and removes its verified owned temporary path.

The final Linux CTest replay also exposed a test-only path collision: the C++ SDK
scratch directory had the same extensionless name as its Linux executable.
Its directory now uses `package_relocation_cpp`, and both hosts regenerate and
replay the registered check. The original failing CTest log is retained.

`.gitignore` already covers the task build directory and SDK caches. Unused owned
preflight/splitting helpers are removed after verifying their absolute paths;
useful SDK artifacts, reproducible adapters, negative checkpoints and final
verification logs are retained there. Existing unrelated ignored files are
preserved. Nothing is published or pushed by this batch.
