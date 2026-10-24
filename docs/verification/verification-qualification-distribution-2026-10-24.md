# Current qualification and distribution — 24 October 2026

[Pending requirements](../pending.md#qualification-and-distribution) ·
[Qualification tools](../../qualification/README.md) · [Verification index](README.md)

This record concerns only QA01–QA04 on the qualification/distribution branch.
The frozen broad baseline is compiler/runtime **2.33.0**; the focused Core
repair is **2.33.1**. Schema language remains **1.23.0**, and matching editor
packages are **2.0.19**. Other pending implementation sections are unchanged.
Checks below qualify implemented contracts; they do not implement proposals on
the storage, compiler/backend or XML/package branches.

The original **QA01–QA04 execution and triage are complete** for the documented
contracts and host profiles. QA-B01 and QA-B02 remain pending at the user's
instruction to record unfixed findings separately. This is not an all-green
release certification: full Linux inventories retain their strict-C failure.
A historical pass, interrupted command, missing SDK or diagnostic subset does
not admit a failed full inventory. Raw logs and useful build artifacts remain
beneath ignored `out/qualification-2026-10-24/`.
Initial native builds use one worker per host. After the user authorizes more
capacity, Linux builds use up to four workers and Windows up to three; native
CTest and SDK inventories remain controlled, with lifecycle suites capped at two.
Independent builds use disjoint outputs and preserve their source snapshots.
Other chat windows share the machine, so timings cannot establish isolated speed
regression thresholds.

## QA01: platform, provider and installed-consumer replay

The source checkout was pulled before editing; upstream was already current.
Native builds use the root wrappers, with independent build directories and
explicit provider prefixes. The Windows provider directory is the existing
`out/build/ReleaseWindows/vcpkg_installed/x64-windows` deployment in the retained
source checkout. No dependency versions were changed to make qualification pass.

The frozen source base is Git revision
`ca04f1af33af9927110eb5ca67bf9e7bf82c478c` plus the qualification/editor changes
identified by this branch's focused commits. Broad baseline runs preserve that
source epoch. Linux compiles an independently extracted archive on its native
filesystem, verified against **2,924 exact file hashes** before the build.
`source-snapshot.json` has SHA-256
`a681ffb81529ce35ea3776ab8b9ecd6ed0768c5f1fed67cba630422e6667e2d0`;
the corresponding archive SHA-256 is
`2092c077159f3745c8034087467a5ba5931af05481bfd6316b7f66304c75e1f9`.
Later branch changes and future proposals require their own qualification.

Repair commit `d830c60` backports only the guarded Python Core tree routing
from main `60468b0`, with the required 2.33.1 patch metadata. Main's unrelated
namespace-token feature introduces a projection regression and is excluded.
The new Python and strict generated-C findings are tracked as
[QA-B01 and QA-B02](verification-qualification-bugs-2026-10-24.md), at the user's
instruction to log bugs without implementing unrelated runtime work. Original
failed inventories remain failures; separately selected non-C SDK runs are
explicit diagnostic scopes.

| Host profile | Actual selected native toolchain/dependencies | Disposition |
| --- | --- | --- |
| Windows 11 build 26300, x64 | MSVC 19.51.36260.0, MSVC 14.51 library, CMake 4.4.2, Ninja, Release C++23; libxml2 2.15.3, zlib 1.3.2, Zstandard 1.5.7, LZ4 1.10.0 | All 310 originally configured tests pass across the initial suite and seven targeted replays; all 16 provider profiles pass |
| Windows x64 alternative compiler | clang-cl 21.1.6, MSVC 14.51 library, same providers | Complete 2.33.0 compiler/runtime/records, all public installed headers/examples and all-eleven/NIO generator equality pass; current 2.33.1 compiler and exact generated equality also pass |
| Ubuntu WSL x64, native build/data filesystem | GCC 15.2, libstdc++ 15, CMake 4.2.3, Ninja, Release C++23; libxml2 2.15.2, zlib 1.3.1, extracted Zstandard 1.5.7, LZ4 1.10.0 and xxHash 0.8.3 | 284/288 native commands pass across the suite and setup replays; four strict generated-C failures are QA-B02. All 16 native provider profiles pass |
| Linux x64 alternative compiler | Clang 21.1.8, libstdc++ 15, same native providers | Complete frozen compiler/runtime/records, all eleven/NIO generator equality, public installed headers and standalone examples pass; fresh current 2.33.1 GNU/Clang builds also pass all eleven/NIO exact generated comparisons |

The Linux dependency setup initially lacked pip, then linked Zstandard without
its extracted xxHash search directory. The original logs are retained. The replay
uses an existing pip module to stage the exact pinned independent Python oracles,
and supplies both `LD_LIBRARY_PATH` and `LIBRARY_PATH` for the unchanged extracted
native provider deployment. No global Linux packages were installed or changed.

The first Windows build compiled the current compiler/runtime but could not
launch generated-schema commands (exit `0xc0000135`) because the provider DLL
directory was absent from `PATH`. `windows-initial-dll-failure.log` preserves
that attempt. The replay explicitly supplies the same prefix's `bin` directory.
This was a deployment correction; no compiler/runtime source change was needed.

The full Windows serial suite initially passes **303/310** configured tests in
**2,522.69 seconds**. Two independent crash-oracle path failures now pass with
short private temporary fixtures: [474 checks, 942 fresh processes and exact
images](verification-crash-oracle-paths-2026-10-24.md). Four generated-C consumer
commands need the actual provider prefix recorded in their CMake configuration;
the Swift provider loader also needs libxml2 and zlib staged beside its bridge.
After recording `VCPKG_INSTALLED_DIR` and `VCPKG_TARGET_TRIPLET=x64-windows` for
the actual retained deployment and staging those unchanged DLLs, all **five
deployment replays pass in 135.59 seconds**. Thus all **310 originally configured
tests pass across the initial run and seven corrected replays**; this is not a
claim that the initial continuous run passed. Original failures and corrected
commands remain in their separate logs.

The serial Linux suite initially passes **275/288** in **805.54 seconds**. One
diagnostic harness requires its ignored output below the source `out/` directory;
the unchanged replay passes **80 cases**. Eight crash launchers initially collide
with report-directory names because Unix executables have no `.exe` suffix.
All eight unchanged native helpers/oracles pass with distinct report paths:
**1,896 checks, 696 scenarios and 3,768 fresh processes**. The focused CMake
suffix repair is commit `8913d0b`. Thus **284/288** commands pass across the
retained runs. Four actual GCC strict-C errors remain QA-B02, without suppression.

The final persistent recovery also executes all **eight actually registered
crash CTests**, which pass in **124.80 seconds** using their corrected `_oracle`
report paths. The first standalone helper build omitted the generated managed
header's `rohit` include directory; correcting only that private harness include
path resolves the setup failure. Its original failure remains in the logs.

All **16 independently compiled Windows provider combinations pass 576 semantic
checks** with one worker. The [provider record](verification-provider-matrix-2026-10-24.md)
retains exact counts, source/consumer/report hashes, ordinary-byte controls and
the initial new-probe compile correction. Linux also passes **all 16 profiles and
576 checks**, using two workers. Its report is `linux-provider-matrix/verification.json`,
SHA-256 `6376ce5dc41ffe75a007fd7a83c83ae52c62472d5826cf15329145dde329fc80`.

The alternative Windows installed-example attempt initially mixes C MSVC and
C++ clang-cl, which CMake 4.4 rejects. The corrected fresh replay selects clang-cl
for both languages and passes every installed header, capability, compiler-only
to native discovery transition, C++/C/Python example and named self-reference.
The actual 2.33.1 MSVC/clang-cl compiler images generate identical bytes in all
eleven outputs plus Java NIO; `windows-final-clang-compare.json` has SHA-256
`fcef621bfa8e0102c3fbbae78b18faf019f1d1d895c7205c035861cb08613f67`.

The first Linux current-compiler comparison detects differing Python output.
Its Clang incremental log recompiles the CLI/parser but omits the changed Python
writer, while GNU recompiles that writer. A stale Clang object is the recorded
inference; the WSL shutdown removes the object before direct inspection, so an
object-mtime proof is not claimed. The failed comparison and original build logs
remain intact. Fresh persistent GNU/Clang builds replace this incomplete
incremental attempt for the final current-compiler qualification.

Both fresh native compiler images report **2.33.1 / language 1.23.0** and generate
identical bytes in **all eleven outputs plus Java NIO**, using identical output
paths. The saved GNU compiler SHA-256 is
`0cc15b9f90e6e4fef5f8526eef08b8de45d918f1647bf524ad37571463bf73b0`;
Clang is `6b55527dfa37cfc742fad987bb7abbd06aef91a5671f7e87ed24c879c05e7d19`.
`linux-final-images/` and its manifest preserve both images and the current static
runtime/linked-library copies independently of the WSL deployment.

The actual Linux Java caller-owned array/NIO window regression also passes
**454,826 assertions** in **51.159 seconds**. The saved log SHA-256 is
`a96ee64053bc10185c8d73ef814b5f0bf6e649d95b1ae2b0a41d3adae5131872`.
`linux-finalize.json` records this and the ordinary native measurements,
SHA-256 `58bde0e44a84fbf3ffc37c616157034c0cef64c04570b707547cbaf490b76785`.

## QA02: independent corpus, malformed inputs, faults and exchanges

Windows and native Linux XSD tooling each pass **321/321 semantic decisions** across
all three mandatory modules. Windows uses pinned lxml 6.0.2/libxml2 2.11.9,
xmlschema 4.2.0 and elementpath 5.1.4. Its complete report is
`xsd-tooling-current/verification.json`, SHA-256
`1fb986c14aa5dd8bc9e5dbcd084baf7af37be961a4aad7bc0035a895da27f1c6`.
The initial oracle-directory access failure and later stale union-rejection
expectation remain in their separate reports. The maintained corpus now checks
the already implemented `xs:int | xs:string` export with three valid and three
invalid independent XML fixtures, exact native fields/IDs after rebind, fresh
imports, idempotence and protected publication. The bound prefix-list capability
remains rejected with its current diagnostic. No XML/backend feature was added.

Linux uses Python **3.14.4**, Expat **2.7.4**, lxml **6.0.2** with its bundled
libxml2 **2.14.6**, xmlschema **4.2.0** and elementpath **5.1.4**. This independent
libxml2 is distinct from native runtime libxml2 2.15.2. The complete Linux report
is `linux-xsd-tooling-final/verification.json`, SHA-256
`2168086cefd2c05bab0245ed428b5479fb7771c3fb5553a7bc9942bb8738186e`.
Its exact maintained inputs were copied to a private native directory and verified
before/after. An earlier mounted-source run passed every semantic decision but
correctly failed its source guard when the crash-harness registration changed;
that failed report is retained rather than relabeled.

The current eleven-language configured/fixed-XSD XML exchange inventory passes
**242 producer/consumer pairs, 264 independently checked XML documents and 44
native protocol roundtrips**. XML-disabled consumers execute all four native
protocols and reject future protocol versions. C/C++ dependency inspection confirms
their XML-disabled images do not import an XML provider. Exact SDK commands,
generated/consumer hashes and policies remain in `xml-exchange/verification.json`.

The complete eleven-language v1/v2 schema-evolution inventory also passes:
**22 emissions, 484 native exchanges, 242 XML exchanges and 44 invalid XML
rejections**, with unchanged sources/compiler/artifacts. The independent XML oracle
controls include two positive and eight negative documents plus three native
protocol negatives. The separately exported-XSD rebind inventory also passes its
complete **22/484/242/44** decisions using freshly compiled exported-XSD C++
consumers and all eleven actual SDKs. Its report is `xsd-rebound/verification.json`,
SHA-256 `be194b233ec105387cc07cdf45fb3870a5fa2153e405ebb9de8efc10b1feae29`.

The complete Windows platform runner additionally passes both configured/fixed-XSD
profiles with all nine maintained platform SDKs. Linux's full requested inventory
retains its strict-C compilation failure, QA-B02. A genuinely executed Windows/Linux
**8/9 diagnostic scope** excludes C explicitly: each profile passes **64 same-host
consumer pairs, 88 independent documents and 128 bidirectional cross-host pairs**.
Across both profiles that is **256 cross-host directions**. Linux's scoped report
is `linux-platform-without-c/verification.json`, SHA-256
`dcee6f17766cc1f679ea0964435b8e8dc8db19be79c6fed858b5f0e52f409715`.
It compares the actual matching Windows eight-SDK baseline, not a relabeled
nine-SDK report. The absent C runtime pass remains explicit.

The prepared XML provider groups pass JVM/.NET and native adapters. The JS/TS
attempt fails because the selected cache omitted its exactly pinned libxml2-wasm
0.7.1 dependency. The fresh complete-cache replay passes **all six JS/TS tests**,
including strict declarations, prepared reuse, validation/failure controls and
worker-isolate ownership. Together with the unchanged passing JVM/.NET and native
commands, all nine prepared-provider SDKs pass across the retained runs. The failed
aggregate setup and successful JS replay remain separately identified.

The [complete Windows compression gate](verification-compression-parity-2026-10-24.md)
passes on both the frozen 2.33.0 and repaired 2.33.1 compiler images. Each executes
all eleven SDKs: **15,400 decisions, 2,904 owning exchanges, 264 independent frames,
792 encode bounds, 25 C++ controls and all 17 mandatory lifecycle suites**. The
fresh final run uses two workers and takes **1,061.425 seconds**. Its report is
`compression-parity-final.json`, SHA-256
`d77e2b5e6a51b982e7754d6846a6c91ef25a1e47a79865227b99f70ac9b9e545`.
`windows-final-validation.json` pins all six completed final Windows groups,
SHA-256 `ebd9f103d9f746d7a9ae14471c39667b7986f97a41ef8b632e5d1887f48c9746`.

All eleven SDKs pass **1,056 independently inspected canonical ZIP/OPC archives**
with stored/deflated profiles, ZIP32/forced-ZIP64, six inventory permutations and
two fresh-process repetitions. The complete report is
`package-reproducibility/verification.json`, SHA-256
`dbc3931f01f765ac40c0a9a3db839b22793a3752f7e1f8d6760dc8f86ccd04ba`.
Per-backend provider images/options, canonical bytes and independent projections
remain explicit. Linux executes the complete eleven-SDK inventory; its aggregate
retains the C compilation failure, SHA-256
`bd480f8d569f06f6b73c35b27ef07be53f1f0ceca768b867e9ef118d93e38b68`.
All ten non-C SDKs pass **960 independently inspected archives**. The derived
`linux-packages-without-c/verification.json` diagnostic admission rechecks all
**2,191** retained source/provider/consumer/log/archive hashes and the complete
96-case inventory per SDK, without inventing a fresh run. Its SHA-256 is
`e694a77dfd8637cdf2844e37bbc40117c6e442b1ff37c2fb194782ddad475d43`.
The separately scoped cross-host comparison passes all **960 projections** plus
stored and eligible compressed-byte equality; `linux-package-diagnostic-crosshost.json`
has SHA-256 `3b88a7f814c3c99e9ac1731c70b1c302a1addfd57c2189144526d02fe2ab5a79`.
This ten-SDK diagnostic does not admit the failed complete eleven-SDK Linux gate.

The [repaired Python Core record](verification-python-core-repair-2026-10-24.md)
retains the original public API failure, the focused main repair, **44 direct
regression tests and 24 Python XML/package CTests**, actual package-version
compatibility and all ten unaffected generator-byte comparisons. All **175**
official Core originals are independently classified; both C++ and repaired Python
produce **156 edited roundtrips**, six schema rejections, four cumulative resource
rejections and nine explicit over-16-MiB exclusions. Every **156+156** emitted
model passes the unchanged independent XSD oracle. The corpus is pinned by the
checked-in [manifest](../../test/resources/3mf_core_1_4/official_suite3_manifest.json).
Missing artifacts and unsupported rich-schema backend admissions retain their
separate dispositions. Native geometry/application semantics are not inferred
from XML schema validity.

The fresh native Linux **2.33.1 Python** replay independently repeats all **175**
original classifications using the persistent .NET 8 `XmlSchemaSet` oracle.
It produces the same **156 roundtrips, ten declared rejections and nine input
budget exclusions**; all **156 emitted models** independently validate. Python's
owning phase takes **214.321 seconds**, with **25.201 seconds** for independent
output validation. `linux-official-core-patched/python.json` has SHA-256
`79b0eee6dd43261040a6dbe1953fa11e80f97235dc3ccb85db333062098e8d8c`;
`python-output-oracle.json` has SHA-256
`83b39c549f7b4ae7b8d5ab860a9df564c3abe6497255f6dfa619fa5983b617e0`.
All **14 selected Python XML/package CTests configured in this Linux profile**
pass in **9.51 seconds**, alongside the direct token/Core regressions. This is
distinct from the Windows profile's 24 configured commands and two owning
language corpora; no additional Linux C++ Core run is inferred.

The complete final Linux overlay/recovery validation report is
`linux-patched-validation.json`, SHA-256
`fc8b3b9000521b2f772a11f87ed97abe289b878212cd17bd8da1d2f1a534efdd`.
The preserved compiler/runtime image manifest is `linux-final-images.json`,
SHA-256 `21717641379eae2f3806eeda31b93f02b62b456a9c72af233f29ac29af041c85`.
Initial mismatches, temporary-directory loss and corrected harness setup remain
separate evidence; successful final checks do not erase them.

## QA03: reproducible measurements and native controls

The [completed native measurement record](verification-native-baselines-2026-10-24.md)
retains all **132 native operation profiles, 660 wall samples**, reviewable encoded
and generated sizes, exact ordinary bytes, **80 C++ timing and 80 allocation
profiles**, and the six-format generated-Python/provider-native compression baseline.
The [package process record](verification-package-baselines-2026-10-24.md) adds
**220 timed, independently checked archives** across all eleven SDKs, with actual
generated codec sizes, encoded size, wall/CPU and process residency. Its startup,
Save, validation and I/O costs are explicit. QA03 is complete for these reproducible
Windows workloads; raw native/package CSVs are tracked for review.
The [ordinary baseline driver](../../qualification/native_codec_baseline.py)
requires all eleven SDKs, three fixed workloads and XML-enabled/native-only
controls, with encoded and generated sizes, five timing samples and separately
identified allocation or process-residency measurements. Python traced allocation
and whole-process high-water residency are different categories. Neither is a
provider-internal hard memory ceiling.

After the Linux SDK inventories finish, native C++ separately repeats **80 timing
and 80 allocation profiles**, with 200 iterations and nine samples per case.
Their retained raw log SHA-256 values are respectively
`b8ab44cd3a6db6fbe8f84d7f5a72a5c934ff569d4459aeacd83cae1b5bd9eabe`
and `858e253002dc679e0d7384c0916201b539f8fa050b43bef5b07ac0cf5b004080`.
These are Linux native controls, separately identified from the eleven-SDK
Windows baseline and its repaired-Python supplement.

## QA04: actual editor hosts and matching distribution payloads

Completed within the current Windows host profile. See the separate [editor-host record](verification-editor-hosts-2026-10-24.md) for all executed commands, package/assembly hashes, actual host checks and qualification limits.

The executed VS Code host is **1.143.1**, x64, application commit
`e646868227c3e3a9e3f24277c8c7220be42c0be7`. The installed Visual Studio host is
**18.10.12306.391**, DTE **18.0**. The native adapter uses Jint **4.16.4** and targets
.NET Framework **4.7.2**. These are the measured current application profiles;
package verification does not imply Marketplace publication or another editor OS.

## Evidence retention and repository scope

Useful failed/successful logs, retained compiler images, provider inventories,
generated fixtures and build caches intentionally remain beneath ignored
`out/qualification-2026-10-24/`. After the completed baseline gates, WSL shuts
down while idle and clears its tmpfs `/tmp/` deployment. The saved source archive,
hash inventories, SDK fixtures and reports survive in the workspace; the original
ephemeral native source/build tree and its uncopied images do not. The final
current-compiler replay is reconstructed on persistent native `/var/tmp/` storage,
at `/var/tmp/serializer-qa-final-20261024/`, with its compiler images copied back
to the workspace. Recovery verifies the 2,924-file archive, approved seven-file
overlay, provider/GTest prefixes, all 175 official fixtures and the existing
Linux .NET 8 SDK; its temporary-only independent oracle assembly is rebuilt.
Editor
host fixtures and successful package/build caches are also retained in ignored
outputs. Unused probes, obsolete empty coordinator logs and failed temporary
crash trees are removed with checked workspace/temporary-directory boundaries.
The qualification-owned WSL keeper is stopped after preserving the final images
and independent oracle. `linux-recovery-completion.json` pins the completed
recovery and cleanup, SHA-256
`6c59b98bab6eee54534a3491d6ec2f8bdcc176e783ef6c1210692439ca191fd8`.
The existing recovered-work backup is preserved. `.gitignore` already covers
the generated outputs; raw measurement CSVs and required source inputs remain
tracked. No repository-wide cleanup or reset is used.

Only the original qualification/distribution rows and the two newly recorded
qualification bugs change in the pending requirement inventory. Other branch
sections retain their original rows. Documentation links, inventory counts,
report hashes and whitespace are checked before the final evidence commit.
