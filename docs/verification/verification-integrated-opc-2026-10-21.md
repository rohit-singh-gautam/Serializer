# Integrated OPC and runtime completion batch

Compiler/runtime **2.22.0**, schema language **1.21.0**, 21 October 2026.

## Scope and accounting

The preceding [2.21.0 checkpoint](verification-integrated-pending-2026-10-21.md)
left 793 implemented rows and 54 pending rows. This batch completes eleven:
`MG02`, `MG03`, `MG05`, `OPC-OPT03`, `OPC-R05`, `OPC-R08`, `OPC-R09` and
`XML-B13`, followed by `OPC-R11`, `XML-B05` and `XML-B21` through the complete
large owning operation qualification. The application-ownership review separately marks `MIG02`, `MG06` and
`REL02` **will not fix**. Those three are not implemented features.

The inventory is **804 implemented/verified, three will not fix and 40 pending**
out of 847 rows. The pending count includes held `DB01` and `MIG03`; 38 are
active. Six main OPC/3MF requirements remain open. Assigning or auditing a
requirement is not evidence that its acceptance gate has passed.

| Evidence | Implemented and executed scope |
| --- | --- |
| [Managed capability and fault qualification](verification-managed-capabilities-2026-10-21.md) | Native activation/exclusion checks, all 25 history-policy conversions, typed session faults and cache authority; 48 targeted tests on each Windows/Linux host. |
| [Application ownership](../project/application_responsibilities.md) | Reconciles the implemented C++ authorization boundary with host principal/policy administration and the three explicit will-not-fix decisions. |
| [Pack-IRI addressing](verification-pack-iri-2026-10-21.md) | Public C++ outer-address composition, parsing, resolution and policy-directed equivalence, with independent Windows/Linux checks. |
| [Official Core corpus](verification-core-conformance-2026-10-21.md) | 175 model parts accounted for; 156 edited round trips per supported C++/Python consumer, independent unchanged XSDs and explicit schema/resource dispositions. |
| [OPC metadata](verification-opc-metadata-2026-10-21.md) and [Annex B](verification-opc-annex-b-2026-10-21.md) | All eleven actual SDKs execute mandatory physical-record, URI/IRI, encoding and Relationships compatibility controls. Native metadata replay preserves the existing `zip_entry` layout and regenerates dependent fields. |
| [Namespace/provider boundaries](verification-runtime-boundaries-2026-10-21.md) | 44,892 physical namespace admission decisions and actual prepared-provider checks across eleven outputs. |
| [Large XML](verification-large-xml-2026-10-21.md) | Eleven Windows SDKs, 216 isolated profiles/1,080 timed batches and 132 interruption/recovery controls; Linux C++ adds 54 profiles/270 calls and 12 controls. Earlier timings retain their original compiler provenance. |
| [Operation publication](verification-operation-publication-2026-10-21.md) | C ZIP rollback, prepared-provider completion fences, Python Save pre/post-publication checks and external C#/Kotlin/Swift public cancellation policies. |
| [Large combined operations](verification-large-package-operations-2026-10-21.md) | All eleven actual SDKs qualify three large identity owners, DEFLATE, opaque/scoped preservation, exact/one-tight logical quotas, cancellation/real deadlines, atomic Save and fresh recovery. Backend API boundaries and immutable compiler bridges remain explicit. |

## Integrated Windows regression

The root build wrapper passed with MSVC 19.51, CMake 4.4, XML, zlib, Zstandard,
LZ4 and managed targets enabled. Eight build jobs and four CTest workers were
used; the existing provider installation was retained:

```powershell
./make.ps1 test -BuildDirectory out/build/ReleaseWindows `
  -Configuration RelWithDebInfo -Jobs 8 `
  -CMakeArgs '-DVCPKG_MANIFEST_INSTALL=OFF','-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/DEBUG /INCREMENTAL:NO'
```

**275/275 CTests pass**, exit code zero, **660.51 seconds** CTest wall time.
Compiler SHA-256:
`8d0d0f4af7135fb4db36e185d020bcfeb591895c7cfbfaf6382169eea712fa92`.
This capture includes the public cancellation helpers, Kotlin collision repair,
Swift safe deferred parser failure, new QName-list tests and runtime-literal
guard. It predates the final isolated Swift IPv6 guard and Java prepared-provider
completion fence; those changes have separate subsequent checks.

`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=OFF` in this configuration. The full
eleven-output namespace, corpus and performance matrices are separate actual SDK
executions, not additional targets hidden inside the 275 count. Existing
installed-package and same-major version compatibility tests are included.

The Swift URI checkpoint, compiler SHA-256
`cecc04508b507a447f27517485c75b9f566f2b5ba3aa5dca362bdc45602098ae`,
passes all six affected compiler/CLI/package/version/runtime-literal/public-Swift
CTest targets in **89.63 seconds**. The independent compiled-helper oracle
rejects all eight malformed scalar/IPv6 controls while the old helper accepts
all eight. The positive IPvFuture address `https://[v1.:::]/` remains accepted.
Its generated-source SHA-256 is
`b097bd9c24169c13462c0cffea7b15d6f20fddd92dc72af03b649eae817f6cd4`.

The combined source after the Java completion fence rebuilds successfully;
compiler SHA-256
`dea245fd49925f789d3d4444750d21501bdd726656fcf7372c046dfde4af6499`.
All six affected CTests pass again in **90.75 seconds**. The executable reports
compiler 2.22.0 and schema language 1.21.0. The independent Swift owning package
consumer also passes **183 OPC rows** at the URI checkpoint in 79.14 seconds;
its source is byte-identical to the earlier fully exercised Swift package image.

The final immutable combined image independently passes the actual Java/JAXP
and Go/libxml2 readonly projection controls, all four native protocol controls
and the Java prepared-provider bounds/concurrency/no-fetch suite:

```powershell
python -B test/portable_xml_projection_test.py --compiler out/build/pending-runtime-providers-completion-2026-10-21/compiler-final/serializer.exe --build out/build/pending-runtime-providers-completion-2026-10-21/java-go-projection-main-final --language java,go --bridge out/build/ReleaseWindows/test/xml_provider_bridge.dll
python -B test/portable_xml_provider_test.py --compiler out/build/pending-runtime-providers-completion-2026-10-21/compiler-final/serializer.exe --build out/build/pending-runtime-providers-completion-2026-10-21/java-provider-main-final --language java
```

These commands require the installed `lxml` directory on `PYTHONPATH`, the
existing provider DLL directory on `PATH` and actual JDK/Go toolchains. The
Windows opt-in portable projection CTest now passes the actual provider bridge.
Non-Windows profiles retain their existing platform qualification boundary.
A separate root-wrapper configure with
`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=ON` passes. Its RelWithDebInfo CTest
JSON contains the expected `--bridge` argument and generated target DLL path.
That configuration was inspected rather than built or counted as an additional
SDK test run; the actual SDK runs above use the integrated build's bridge.

All **22 benchmark source profiles** regenerated with the final image are
byte-identical to the dated public-wrapper checkpoint `69c2f1a2`. Ordinary Java
benchmark mappings do not select runtime XSD; the new prepared-provider fence is
qualified by the separate actual-provider tests, not that source comparison.
No timing or peak-memory sample is relabeled as a final-image measurement.

The subsequent large-package work found the Python Save callback defect described
in the [operation-publication record](verification-operation-publication-2026-10-21.md#python-save-publication-callbacks).
The compiler rebuild after its two completion polls passes the six affected
compiler/package/version/runtime-literal/Python CTests in **169.78 seconds**.
Its SHA-256 is
`82019ab1cc5088111bc7c562a5b29ba5899f660a5143b0120abcf9736dc3a061`.
The Python target includes the new 44-test suite and both meaningful old-image
failures. The earlier full 275-target result remains the earlier capture; it is
not relabeled as a full run after this isolated fix.

All 22 benchmark source profiles again compare byte-identical to the preceding
integrated image, recorded separately in
`pending-large-xml-2026-10-21/publication-final-source-sizes/verification.json`.
The five portable large-package runtime modules and two JS/TS declaration files
also compare byte-identical to their actually executed `dea245fd` outputs;
their original SDK receipts remain unchanged. These comparisons are generation
checks, not repeated SDK execution or new timing measurements.

## Failures and limitations

The reused Windows incremental-link state initially produced LNK1163 for an
existing COMDAT. The unchanged object links with `/INCREMENTAL:NO`; the private
build cache uses that setting. No source workaround or tracked build-wrapper
change is claimed. The meaningful Kotlin schema-name collision, oversized C
literal, Swift invalid IRI acceptance and Java completion-fence failures were
fixed and retained as regression controls. Swift's original FoundationXML
callback abort caused an access violation; deferred failure avoids that reentry
and is bounded by admitted input, without promising provider CPU preemption.

Native wire revisions and existing model APIs remain unchanged. The schema
language stays 1.21.0. Both editor extensions remain 2.0.10: this batch changes
neither schema grammar nor extension behavior. Package publication and editor-host
qualification are not claimed by native or SDK tests.

Useful build, counterfactual and verification logs remain under ignored
`out/build/pending-runtime-providers-completion-2026-10-21/`, with the other
immutable evidence directories named in the linked records. The main append-only
build log retains earlier failed runs and later passing runs separately.

## Final audit and hygiene

The completed large-operation aggregate passes all eleven SDK profiles. Its
receipt is `pending-runtime-providers-completion-2026-10-21/large-package-aggregate.json`.
The final backlog audit independently counts **847 unique rows: 804 checked,
three will not fix and 40 pending**. All 37 changed/new Python files parse, all
41 changed/new Markdown documents have existing local link targets, and the
staged diff passes `git diff --check`. The quick ten-entry Python operation
replay passes; opt-in CMake configuration and both affected test registrations
are inspected separately from the original 275-target capture.

One-use root launchers, source-bridge helpers and diagnostic scratch files were
removed after retaining their reports and exact commands. Useful ignored build
outputs, immutable compiler images, SDK consumers, original failures and final
verification logs remain intentionally retained. The earlier C# trial directory
whose deletion was rejected by automatic approval review remains untouched, as
documented in the preceding 2.21.0 record. No deletion retry was attempted.
The verified repository changes are staged; no commit or publication was made.
