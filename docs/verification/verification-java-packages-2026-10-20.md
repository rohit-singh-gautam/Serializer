# Java package qualification, 20 October 2026

[Java package API](../languages/java_packages.md) ·
[Package contract](../serialization/xml_packages.md) ·
[Verification index](README.md)

This record qualifies **JV-P01–JV-P15 and JV-P17–JV-P19** for the selected Java 17
standard-library, ASCII OPC part-name and Windows filesystem profile in
compiler/runtime **2.14.0**, schema language **1.20.0**, on baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes.
**JV-P16, JV-P20 and JV-P21 remain open.** ZIP64 input checks do not imply a
larger-than-Java-array output path, complete Office/OpenXML conformance or
automatic application reference checks.

## Actual compiler and SDK execution

The maintained [runner](../../test/java_package_test.py) invokes the actual rebuilt
compiler for ordinary, MIME-parameterized, native-example and collision schemas.
It compiles with **Microsoft OpenJDK 17.0.18+8-LTS**,
`--release 17 -encoding UTF-8 -Xlint:all -Werror`, and executes those classfiles on
the same actual Java 17 runtime. The final run uses no reused or manually
assembled generated module and exits zero in **47.413 seconds**.

| Executed check | Result |
| --- | --- |
| Java input/API decisions | **224/224** pass: 178 independently authored fixture decisions and 46 API, lifecycle, resource and publication checks |
| Independent output oracle | **9/9** pass through Python `zipfile` and XML inspection, including ZIP32/forced-ZIP64 with/without descriptors and retained external IRIs |
| Native interoperability | **2/2** pass: actual C++ typed OPC output is loaded/re-encoded by Java, and that Java output is consumed by the existing C++ example |
| Generated collisions | Existing model/helper families retain their names, selecting `SerializerPackage_3`; actual `Code.java` and `Role.java` outer-container profiles also compile/run |
| Save/publication | Every named pre-replacement fault preserves original bytes and removes staging; post-replacement faults leave a complete readable new generation and report uncertain publication with the original cause |
| Canonical output | Reordered physical entries and unordered explicit stable-ID extension part/link sets produce identical bytes; declared role occurrence order remains semantic |

The final immutable compiler SHA-256 is
`0e494607dd15d6fd12cdabe96271ee51c31647ae5a9a70192e5628cd821d7efa`.
It is retained with deployed DLLs and `source-manifest.json` in ignored
`out/build/backlog-2026-10-20/compiler-current-final-0e494607/`.
Logs and the machine-readable result are retained in
`out/build/backlog-2026-10-20/java-package-final/`, including `verification.json`,
`java.log`, strict SDK build logs and C++ producer/consumer logs.

The ordinary actual generated `Schema.java` is **352,970 bytes**, SHA-256
`9dff1894a913d4ace0286a4a1c5760e94a913f84f35ab31dab6cfb2339192deb`.
The maintained runner SHA-256 is
`b4c1300297b397c4ec0f77436a7ff80133ee31c5268f5e60f2c8097c52a5aa00`.
Package runtime support is emitted only for schemas containing package
declarations; ordinary native model codec APIs and wire formats are unchanged.

## Isolated Linux runtime replay

The exact Java 17 classfiles compiled above were streamed with their fixtures to
an isolated native Linux directory and executed using **OpenJDK 25.0.4.1 on Ubuntu
26.04** (`/usr/bin/java`). The replay passes **224/224** decisions, **9/9**
independent output checks, helper-family and `Code`/`Role` outer-container
profiles, and consumption/re-encoding of the Windows C++ producer's typed OPC
output. This includes the Linux filesystem Save fault cases and POSIX directory
synchronization path. It is a Linux runtime/filesystem replay of actual generated
Java 17 classfiles; no Linux `javac` is installed, so no separate Linux compiler
toolchain build is claimed.

`java-package-linux-final/linux_verification.json` retains the SDK/output summary.
Its 902-file input inventory has SHA-256 aggregate
`b83e91a8192d70811991fe1598d614dedc39cf50ee86a5181b45f01d5ce9e59a`.
All Linux source/class/fixture I/O stays in native Linux storage, and the replay
does not modify the persistent native CMake build cache.

## Independent framing, metadata and policy corpus

Inputs come from Python `zipfile` and separate explicit wire producers, including
the shared [descriptor fixtures](../../test/resources/package_descriptor_fixtures.py),
[malformed fixtures](../../test/resources/package_malformed_fixtures.py) and
[ZIP64 boundary producer](../../test/package_zip64_test.py). They exercise local
and central records, signed/unsigned ZIP32/ZIP64 descriptors, CRC equal to the
optional descriptor signature, exact sizes/DEFLATE extent/CRC, truncation,
overlapping or outside extents, optional locator consistency, real 65K entry-count
boundaries and sparse large logical offsets without allocating huge files.

OPC inputs cover discovered alternate main paths, repeated typed roles with a
bad middle occurrence, source-local link IDs, missing/wrong targets, duplicate
and aliased part identities, excluded directories/symlinks/non-part records,
unsupported Unicode part candidates and integrity checks on discarded entries.
MIME controls distinguish quoted parameter values and significant whitespace.
URI/IRI controls retain external spelling, validate IPv6/IPvFuture/ports and
resolve internal dot/query/fragment references without opening external targets.
Metadata checks include UTF8/BOM-UTF16, rejected mismatched/unsupported encodings,
DTD/entity rejection, and Relationships MCE scoping, `MustUnderstand`,
`ProcessContent`, selection/fallback order and discarded-branch behavior.

API checks include exact/one-tight cumulative and simultaneous-buffer limits,
immutable policy snapshots, cancellation, short reads, explicit private spool
admission and cleanup, borrowed ownership, source/sink original exceptions,
close/reentry failure fencing, complete extension state before domain validation,
unknown reject/skip/preserve and required-capability behavior, and Save fault
boundaries before and after atomic replacement.

## Reproduced repairs

The first actual emitted run exposed a SAX portability issue: the selected parser
does not retain its XML-version locator at `endDocument`. Explicit strict encoding
and XML1.0 declaration admission now accepts ordinary declaration-free metadata
without relying on that locator. Additional independent controls found and
verified significant MIME whitespace preservation, complete extension binding
before validation, fail-closed Unicode part candidates and case-insensitive
relationship infrastructure identity.

A legal short-read source callback could reenter the enclosing loader, swallow
its first exception, and allow the nested ZIP reader to continue reading. Private
parent activity guards now stop the inner loop after that one callback and retain
the original failure as the active exception and later inactive-state cause.

The earlier actual compiler also consumed **17 bytes with an I/O limit of zero**
and **1,163 bytes with a capacity of 1,162** during sequential spooling. The final
runtime admits every request against remaining I/O/scratch capacity before source
or private-file I/O. All three maintained zero/one-tight/pre-file tripwires pass.
Short reads charge actual bytes; true EOF needs one byte of request capacity and
charges no bytes when the source returns EOF. Before/after logs remain in
`java-package-repaired/` and `java-package-staged-repair/`; those staged runs are
diagnostic evidence, separate from the final actual-compiler result above.

A clean before-control compiles the earlier actual `356a942e` ordinary and
parameterized outputs with the **identical final test adapter**, SHA-256
`d34dd8264362267c84f38a20447997175f0e8f524c589dc6fc44e1d8621f155d`.
Its four filtered runs reproduce **eight failures**: two silent Unicode
acceptances, two infrastructure casefold rejections, one nested swallowed-reentry
additional-I/O failure and three spool pre-admission failures. Logs and metadata
are retained in `java-package-boundary-baseline/`. Those same checks pass in the
final actual Windows and Linux runtime replays.

## Scope limits

This result covers the selected Windows/JDK17 and isolated Linux/JRE25
filesystem/provider profiles.
Atomic replacement and directory synchronization depend on the native filesystem;
the runtime synchronizes POSIX directory channels where exposed and rejects
unavailable atomic moves before replacement. This record does not certify every
filesystem or a separately rebuilt Linux Java compiler toolchain.

Package ledgers bound logical byte extents and conservatively charged package
work/allocation estimates, rather than JVM heap or RSS. Existing nested native XML
APIs do not accept the full package operation context, so automatic cumulative
nested-codec accounting, exact field diagnostics and in-callback cancellation
remain **P16**. Declarative runtime envelope dispatch **P20** and cross-part
payload key/reference resolution **P21** remain open. Application validators do
not close those framework requirements.

Fixed singleton writers use authored names; opaque links retain their original
context and must be explicitly rebound if those referenced names change.
Finalization rejects stale membership. Whole-part preservation does not imply
understanding of an unknown required vocabulary. Large ZIP64 input offsets/counts
are qualified, but archive output and individual decoded parts still require
Java arrays. No CPU-performance, JVM hard-heap or broad cross-backend shared-context
qualification is claimed.
