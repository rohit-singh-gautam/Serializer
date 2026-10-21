# XSD authoring and evolution — 21 October 2026

[Pending requirements](../pending.md) · [Inventory and replay commands](../../qualification/xsd_evolution/README.md)

Compiler/runtime **2.19.2** completes `OPC-R13` qualification for existing
XSD-derived mappings, inline exceptions and reviewed declaration imports. Generated
runtime behavior and native wire contracts are unchanged. Schema language
**1.21.0** and both editor extensions **2.0.10** retain their versions.

The full Windows and Linux runs pass their complete inventories, final native
CTest replays and the integrated cross-host/provenance audit.

## Two distinct evolution workflows

The authored native model preserves `evolution::order` and the same declaration
order in both versions: `id` (native ID **37**), `customer` (**11**) and optional
`note` (**83**). Fresh defaults remain id 7, customer `"native default"` and absent
note. The readonly `has_id(expected)` behavior returns true for 7 and false for 8.
Actual consumers compile against these same native field names, types and APIs.

Both authored sources use `using schema "order.xsd";`; no parallel INI inventory
is involved. Version 1 derives XML sequence `customer, note`. Version 2 derives
`note, client`, with `xml(name("client"))` as the inline exception on the existing
native `customer` member. Both authored values contain customer `R&D` and note
`memo`. The test preserves native names, explicit IDs, declaration order,
defaults and callable behavior while changing XML naming/order.

New-field addition is qualified separately through fresh automatic declaration
imports. The staged CLI test first imports `customer`, then adds optional `note`
before it, then changes XML `customer` to `client`. It independently computes
hash-derived IDs from expanded XML identities and checks emitted declaration
order. Adding a field retains IDs for unchanged expanded identities; a renamed
identity gets a new ID. Imported declaration order follows the XSD, so this test
does not claim positional native-byte preservation for automatic reimports.

Changed imports reject an existing output and its dependency file with an exact
review diagnostic. Unchanged imports are byte-idempotent. Authored edits remain
unchanged on rejection, and every fresh import is accepted by the compiler.
Shared-type global-root ambiguity requires an explicit root; ambiguous records
in a local imported bundle require namespace/root disambiguation. These failures
are checked before creating or changing source, generated output or sidecars.
The workflow does not infer renames or merge application edits automatically.

## Actual SDK matrix and independent checks

Each host's complete inventory covers C++, Python, C, Rust, Go, Java,
JavaScript, TypeScript, C#, Kotlin and Swift, with two separately generated model
versions per output. The configured scope is:

| Check | Per host |
| --- | ---: |
| Actual generated library/consumer emissions | 22 |
| Emitted files (one XML and five native per consumer) | 132 |
| `binary_none` native exchanges across all languages/versions | 484 |
| XML exchanges across all languages within matching versions | 242 |
| Wrong-version XML and wrong-namespace failures with no output | 44 |

Across both hosts, this is **44 emissions**, **264 emitted files**, **968 native
exchanges**, **484 XML exchanges** and **88 rejected XML inputs**. Each host also
executes the staged importer/ambiguity CLI suite before building its SDKs.

Every emission checks fresh defaults and readonly behavior before authoring all
three fields. Literal, generator-independent native bytes cover `binary_none`,
`binary_integer` and `binary_string`, as well as default and XML-projection
outputs. The tagged protocols therefore check native IDs **37/11/83** and native
names `id/customer/note` without treating XML `client` as a native rename.
Exchange readers consume `binary_none`; tagged-protocol decoder interoperability
is outside this matrix.

The independent XML oracle checks expanded root/member names, the ID attribute,
child order, values and absence of unexpected content, then validates each
document against its unchanged local XSD through network-disabled lxml/libxml2.
It imports no generated codec. Its maintained controls cover two positive XML
documents, eight invalid XML variants and a native corruption in each of the
three protocols. Wrong-version inputs are independently valid emissions for the
other schema; readers must reject them before writing an output.

Java compiles the generated library and consuming source in separate strict
`javac` invocations targeting Java 17. Kotlin compiles separate generated and
consumer JAR modules targeting JVM 17. TypeScript uses a strict external
consumer without `skipLibCheck`. Actual strict native/managed builds and the
selected SDK/XML-provider dependencies are retained in each consumer record.

| Output | Windows SDK/runtime | Linux SDK/runtime |
| --- | --- | --- |
| C++ | MSVC toolset 14.51, C++20 | GCC 15.2, C++20 |
| Python | Python 3.14.3 | Python 3.14.4 |
| C | MSVC toolset 14.51, C11 | GCC 15.2, C11 |
| Rust | Cargo 1.99.0 | Cargo 1.93.1 |
| Go | Go 1.26.4 | Go 1.26.0 |
| Java | JDK 26, Java 17 target | OpenJDK 25.0.4.1, Java 17 target |
| JavaScript | Node 22.23.1 | Node 24.15.0 |
| TypeScript | TypeScript 5.9.3/Node 22.23.1 | TypeScript 5.9.3/Node 24.15.0 |
| C# | .NET SDK 8.0.426/runtime 8.0.32 | .NET SDK 8.0.426/runtime 8.0.32 |
| Kotlin | Kotlin 2.2.21/JDK 26, JVM 17 target | Kotlin 2.2.21/OpenJDK 25.0.4.1, JVM 17 target |
| Swift | Swift 6.4 | Swift 6.1.3 |

The independent XSD oracle uses lxml **6.1.3**/libxml2 **2.11.9** on Windows and
lxml **6.0.2**/libxml2 **2.14.6** on Linux. Their actual extension libraries and
resolved provider dependencies are retained in the reports.

Cross-host qualification requires complete clean inventories with identical
maintained-source snapshots. All five native outputs for each of the **22 paired
emissions** are byte-identical across hosts. All 22 XML projections and complete
XML byte strings also match. This is the executed fixture result, without a
general lexical XML normalization claim or additional opposite-host consumer
exchanges beyond the executed per-host matrices.

## Provenance, regression and limits

Evidence is retained under ignored `out/build/pending-xsd-evolution-2026-10-21/`.
The immutable Windows compiler is `compiler-first/serializer.exe`, SHA-256
`f453aec90c92dfcaba98f3dc8d41aed7dae383204312ad812ffab8a279f541ca`.
The immutable Linux compiler is `compiler-linux-first/serializer`, SHA-256
`64678ad06a8c8aa6a309937020118ecdd5ee3d6c1948d1f1ec1a7f36d5850831`.
The 406-file compiler/runtime source manifest has SHA-256
`cd2020d1217911fa32fc7dfda84c3f5de1f0b7416c34e8a96a94e350502c972d`.

| Full actual report | SHA-256 |
| --- | --- |
| `windows-first/verification.json` | `65c158bdfad73025ef39b34ef81ff6e9716714b573fc4a8bb710fa9b4f3d7173` |
| `linux-first/verification.json` | `883fd56348e8fb6e2ee03d4aa6ef724f2bfe11322902aa968341852e7636c9c7` |

Both reports record zero failures, unchanged source/compiler inputs and no
conflicting or changed artifacts.

After both formal matrices completed, the driver and its guard-test file received
evidence-admission hardening: comparisons must include all five exact literal
native hashes and the manifest's exact expected XML projection. The 13 guard
controls pass. The producer sources, independent oracle, manifest, SDK builders
and executed generated consumers remain unchanged. The formal reports retain
their original source snapshots; they are not relabeled as SDK runs under these
later guards. The final audit independently reinspects all **264 emitted
files**, **1,452 exchange outputs** and **88 negative command decisions**, plus
their source/image/provider pins, without rebuilding unchanged SDKs.

Both hosts pass **11/11 final CTests** covering core, generated XSD import,
authored contracts, declaration import, XML schema behavior, runtime literals,
compiler CLI, package-version compatibility, the independent oracle, its
13-control evidence guard suite and staged CLI publication checks.

| Final CTest log | SHA-256 |
| --- | --- |
| `ctest-windows.log` | `7da90fd30101d98a37beb3936e9fe726cad6dd434254dc4ca884250cd5633c89` |
| `ctest-linux.log` | `bc5aa40a8e767cf957b26da9748a3e324e342de119b9792113b87ddca256e371` |

The final `integrated-verification.json` passes, SHA-256
`0b28339588e8e33ca799ba2701af819e786e3424da3fa267ffcebba7c4513d6b`.

The qualification snapshots all maintained adapters, schemas, the expanded CLI
test, guard controls and imported Node/Rust dependency inputs. Compiler images,
generated libraries, copied/compiled consumers, actual SDK/XML providers and the
independent lxml provider/dependencies are pinned. Repeated captures with
conflicting hashes reject; final source, compiler and artifact changes prevent a
passing verdict. Missing SDKs and setup failures are retained as failures rather
than silently reducing the inventory.

The pre-formal four-SDK JVM/JS probe uses the separately identified immutable
2.19.1 compiler and passes both authored variants. Its checks cover strict public
consumer compilation, all emitted files, same-version XML/native operations,
exact native files across versions, cross-version native reads, malformed-XML
nonpublication and artifact stability. This checkpoint is not a substitute for
the completed final 2.19.2 full matrices.

Broader fixed-XSD association/import/export and local-bundle profiles remain in
`OPC-R12`. Rejected content/document graphs retain their existing diagnostics.
This record does not claim automatic migration, lexical XML/C14N/signatures,
package conformance, hard provider allocation/preemption or performance results.
No runtime changes, editor package changes or external publication are part of
this qualification. `.gitignore` already covers task outputs and SDK caches;
useful generated consumers, immutable reports and actual replay logs are retained.
