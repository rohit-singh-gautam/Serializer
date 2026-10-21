# XSD contract tooling — 21 October 2026

[Pending requirements](../pending.md) · [Inventory and replay commands](../../qualification/xsd_tooling/README.md)

Compiler/runtime **2.20.0** completes `XML-B11` and `OPC-R12` qualification for the
supported XSD tooling profiles, including native-enum string-space binding/export.
Windows and Linux each pass **300 tooling decisions**, **13/13 selected native
CTests** and the complete eleven-SDK exported-schema matrix. The integrated
cross-host/provenance audit passes with zero exceptions. Schema language
**1.21.0** and both editors **2.0.10** retain their versions.

## Contract and implementation scope

The compiler tooling inventory checks actual CLI outcomes, protected publication,
native model syntax and independent XSD decisions. The ordinary exported/rebound
model profile has a separate eleven-SDK runtime follow-up. Passing rich compiler
fixtures does not imply all eleven runtimes implement every rich XML graph.

Native-enum export retains exact authored enum wire names and values, native
field IDs/order, constructor defaults and readonly behavior. The binding space is
`xs:string` with empty or `preserve` whitespace behavior. Existing compiled string
facets may narrow admitted labels; XML defaults/fixed values must name admitted
enumerators, and a restriction must admit at least one native enumerator. Invalid
bindings reject during generation, before existing output or dependency files
can change. Text enum defaults/fixed values belong on the global XSD element.
Ordinary, optional and repeated enum element fields are distinct from XML lexical
lists. Enum text requires one nonoptional scalar; optional/repeated text remains
rejected. Token/collapse/normalized whitespace, enum lexical lists and
numeric/QName/list spaces reject explicitly.

Authored export/rebind preserves native enum types, defaults and methods. Fresh
import of exported enum XSDs instead emits constrained strings with independently
imported names/IDs; it does not reconstruct authored enums, methods or constructor
defaults. Fresh imports establish usable XML declarations rather than authored
native API equality.

Directional comparison treats sequence-to-matching-`all` as widening regardless
of `all` declaration order. A reproduced false positive was corrected without
changing native member order. Reverse `all`-to-sequence and reordered-sequence
controls still reject. Both contracts must bind to the same native model;
unavailable associations and backend particle limitations are distinct from
actual compatibility differences. This is the implemented reader contract,
without general XSD language-inclusion or automatic rename inference.

Schema locations use explicit UTF-8 filesystem conversion. The MSVC compiler
embeds a UTF-8 process manifest, inspected with `mt.exe`. Process-scoped UTF-8
activation is documented for Windows 10 version 1903 and later; the compiler
requires no global system-locale change and does not configure consuming
programs. Older operating-system behavior is not newly qualified here.
[Microsoft's process-code-page documentation](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page)
defines that platform boundary. Local containment, allowlisting and dependency
tracking remain enforced.

## Final third-image compiler inventory

The driver requires all three modules and exact unique decision inventories.
Partial selections are diagnostic. Its compiler-only report explicitly requires
the separate runtime follow-up and refuses an eleven-backend completion claim.

| Module | Frozen scope | Final Windows / Linux result |
| --- | --- | --- |
| Local bundles | 45 CLI decisions: URI/path and namespace allowlist, cycles/chameleon contexts, exact/one-over bounds, dependency/identity framing and protected publication | 45 passed / 45 passed |
| Directional comparison | 45 decisions: ten compatible, 21 incompatible, six unavailable associations and eight backend capability rejections | 45 passed / 45 passed |
| Rich exports/bindings | 210 decisions across eight fixture families, independent XSD validation, native AST preservation, reviewed imports, publication and generation preflights | 210 passed / 210 passed |

The frozen full inventory contains **300 decisions per host**. The export module
contains **142 CLI decisions**, **64 independent XSD/XML decisions** and **four
actual generated Python enum-codec checkpoints**. The expansion adds 24 protected
C++/Python generation rejections, six independently valid-XSD controls and two
positive generation controls to the earlier 178-decision corpus.

The eight rich families cover text/facets, wrappers, cross-namespace records,
singleton `all`, chameleon contexts, cyclic includes, native enums and native-enum
default/fixed/text constraints. They retain 13 authored classes and 12 reviewed
fresh imports. Independent lxml validation imports no generated codec. Generated
Python native methods/enum codec controls and dedicated C++ enum regressions
qualify their selected rich models separately from the ordinary SDK matrix.

The final dedicated native-enum regression contains **six GTests**, including
six invalid binding shapes and six valid counterparts. The common evidence guard
module has **40 controls**, and the full tooling snapshot watches **122 maintained
qualification inputs**. The selected third-image native replay passes 13/13 CTests
on both hosts, including the six enum tests and 40 guard controls. It also covers
core/compiler and package-version checks, XSD/import/generated-contract tests,
runtime literals, evolution oracle/guards and schema tests.

The immutable Windows image is `compiler-third/serializer.exe`, SHA-256
`0bfee77c8b07cf3bf294e34897dcc5598189cd1112f0dc0ad52666fb775b0e7b`.
The Linux image is `compiler-linux-third/serializer`, SHA-256
`e6260355cb70a04a68ba962b192b852606e6073b9711c3a556e8884d4b7f0ecb`.
Both compiler/runtime builds use the same verified **407-file** source manifest,
SHA-256 `e878cf85d7617197585f3cf15b9b78c83b960de3dbc05f58f160e8b81c1bdebd`.
Both complete tooling reports record 300 passing decisions and 122 unchanged
qualification inputs. Windows captures 315 observed artifacts and Linux 319;
neither has compiler/source/artifact changes or conflicting captures.

| Final tooling report | SHA-256 |
| --- | --- |
| `tooling-windows-fourth/verification.json` | `a3a46adc8a67f1ba9707908fc03e6891d7aace146a2a9dd61319ee7cfdbb14a6` |
| `tooling-linux-fourth/verification.json` | `125ee47dbd3fba6d1c6c084511d5837a15072da4fbc2819eb409da61582b46d0` |

| Independent tooling provider | Windows | Linux |
| --- | --- | --- |
| Python | 3.14.3 | 3.14.4 |
| Expat | 2.7.3 | 2.7.4 |
| lxml | 6.1.3 | 6.0.2 |
| libxml2 used by lxml | 2.11.9 | 2.14.6 |

| Third-image native log | Actual result | SHA-256 |
| --- | --- | --- |
| `ctest-windows-third.log` | 13/13 passed, 19.10 s | `245ce229eca33bbd7d3f543607aa8cbfc3ada78cc8679a59c4e5d41eb743091e` |
| `ctest-linux-third.log` | 13/13 passed, 112.82 s | `79d7aa99f7c314bbaf9395918e23b003746b7d0623bdb26eac7386851717024f` |

## Third-image export/rebind follow-up

The tooling driver exports both ordinary evolution contracts into `sdk_schemas`
and copies their authored schemas byte-for-byte. It compares original/exported
contracts in both directions and checks Python native AST/defaults/readonly
behavior, three literal native protocols and independent XML validation.

Every actual SDK must use those exported XSDs, including separately rebuilt C++
consumers `xsd_tooling_cpp_v1` and `xsd_tooling_cpp_v2`. The model retains native
`id` 37, `customer` 11 and optional `note` 83 in the same declaration order.
Fresh defaults remain 7, `"native default"` and absent note; `has_id(7)` is true and
`has_id(8)` false. Version 1 XML is `customer, note`; version 2 is `note, client`.
Both projections retain authored customer `R&D` and note `memo`.

The complete actual inventory per host passes **22 emissions**, **132 emitted
files**, **484 native exchanges**, **242 same-version XML exchanges** and **44
rejected XML inputs** without publication. Across two hosts that is **44 emissions**,
**264 emitted files**, **968 native exchanges**, **484 XML exchanges** and **88
rejected inputs**.
The independent native oracle checks all three protocols; exchange readers use
`binary_none`. Java compiles separate strict Java-17 library/consumer modules,
Kotlin separate JVM-17 JAR modules, and TypeScript a strict external consumer
without `skipLibCheck`.

Both fresh third-image follow-ups use their fourth tooling runs' exported
schemas, separately rebuilt C++ consumers and corrected final compilers. Neither
has failures or source/compiler/artifact guard changes across its 33 maintained
runtime-qualification inputs.

| Actual final SDK report | SHA-256 |
| --- | --- |
| `windows-third/verification.json` | `1ab40e9be10b2b581ec3e59a1360098ecb25d9537e36f4522b3a5ea83d74a5b7` |
| `linux-third/verification.json` | `15451e4a4cd0c04a7bd1cef2b213ebd5ff92bfa4fbb6b011efb61991f99f5aa1` |

| Output | Actual final Windows SDK/runtime | Actual final Linux SDK/runtime |
| --- | --- | --- |
| C++ / C | MSVC toolset 14.51; strict C++20 / C11 | GCC 15.2; strict C++20 / C11 |
| Python | Python 3.14.3 | Python 3.14.4 |
| Rust | Cargo 1.99.0 | Cargo 1.93.1 |
| Go | Go 1.26.4 | Go 1.26.0 |
| Java | JDK 26, Java 17 target | OpenJDK 25.0.4.1, Java 17 target |
| JavaScript | Node 22.23.1 | Node 24.15.0 |
| TypeScript | TypeScript 5.9.3 / Node 22.23.1 | TypeScript 5.9.3 / Node 24.15.0 |
| C# | .NET SDK 8.0.426 / runtime 8.0.32 | .NET SDK 8.0.426 / runtime 8.0.32 |
| Kotlin | Kotlin 2.2.21 / JDK 26, JVM 17 target | Kotlin 2.2.21 / OpenJDK 25.0.4.1, JVM 17 target |
| Swift | Swift 6.4 | Swift 6.1.3 |

Fresh import ID/declaration-order checks remain separate. They do not establish
positional native compatibility for automatic imports or infer a rename of an
authored application field.

## Earlier checkpoints and the final preflight correction

Evidence is retained under ignored `out/build/pending-xsd-tooling-2026-10-21/`.
The second-image Windows/Linux tooling runs passed the earlier 268-decision
inventory, and both corresponding eleven-SDK matrices passed their full ordinary
inventories. Those reports retain immutable **pre-fix observations**, not final
qualification of the third-image enum-binding preflight.

| Pre-fix checkpoint | SHA-256 |
| --- | --- |
| `tooling-windows-accepted/verification.json` | `cc699ef351d7f23e5f4087a6c4461a446ab9433848021f19709425739b2d9069` |
| `tooling-linux-final/verification.json` | `b80e4abd1a2de1d9c82b413b1a886e48c0e7093770e3ede1bbb189071435b752` |
| `windows-final/verification.json` | `94cf742942491b20cf39f7d0eccc529f0ab4a5bb31b61cb2e1449c9261c3fe99` |
| `linux-final/verification.json` | `5036cdf979027cd98b76ddde0b3cbff56a6eb2e6f876adc973d124eda47e5513` |

The late probe found that XSD-valid unknown enum defaults/fixed values, or a
restriction admitting no declared native enum labels, could pass binding
preflight. The current fix validates the compiled enum value space before
publication. New controls distinguish independent XSD validity from native-model
compatibility and protect existing generated output/dependency files on every
rejected C++/Python generation path. These controls are mandatory in the final
300-decision inventory and six-test native enum regression.

The first expanded Windows tooling attempt then encountered a selected external
formatter permission failure in a new positive C++ generation control. The
control now disables optional formatting, matching the existing corpus. The
failed Windows and stopped partial Linux attempts remain diagnostic; complete
replacement tooling runs use `tooling-windows-fourth` and `tooling-linux-fourth`
with the unchanged third compiler images. Their exported schemas feed new actual
SDK runs, rather than a pre-fix input bridge.

Earlier fixture formatting changed nine standalone `.serializer` files without
changing token streams/quoted literals; its proof is
`fixture-formatting/verification.json`, SHA-256
`c5184ee373efbe1ae09a6011e0d132d0135bb67b8f52ff77e275130f9e7f80e6`.
The second-image Windows SDK input bridge was exact, but applies only to that
pre-fix checkpoint. Final third-image evidence requires new actual compiler and
SDK runs rather than an old-phase bridge.

The historical 2.19.3 preview passed 45 bundle decisions on Windows/Linux and
45 comparison decisions on Windows. `comparison-preview-first/verification.json`
has SHA-256 `7534ac149239901c724c28de106097d8fe333cf24425f93a9a04c5f5be931a54`.
It predates the native-enum feature and does not substitute for final evidence.

## Final audit, retention and remaining limits

The final integrated audit passes after independently rechecking both complete
300-decision inventories, all emitted files/exchange outputs, rejected publication
decisions, actual exported-schema C++ dependencies and 56 copied consumer/dependency
inputs. It verifies 407 compiler/runtime files and 122 qualification inputs, with
zero source exceptions. The 40 admission guards pass again in **61.935 seconds**.

All **22 paired Windows/Linux emissions** have identical native bytes, XML
projections and complete XML bytes. This is the executed fixture result, without
a general XML lexical-canonicalization claim. The audit pins **5,844 path aliases**
covering **5,534 physical files**, hashing each physical file before and after
verification and rejecting conflicting captures. The integrated report is
`integrated-third-verification.json`, SHA-256
`41f53ddb6e47f86ef67bc5036e822e6e93c3a9de07df9c7e9026540b30973015`.

Full final reports, generated SDK proofs, provider/tool caches and the pre-fix,
failed and partial diagnostic checkpoints are intentionally retained under
ignored task/output directories. Historical shared C++ build files were rebuilt;
earlier report hashes preserve their original observations without claiming that
every old runtime file remains immutable or available. Only the final selected
native inputs are copied into `retained-native-inputs`: **21 files**, **25 original
path aliases**, **8,055,095 bytes**. These are four C++ binaries, four headers, four
dependency files, two version headers and seven Windows DLLs. Copies are checked
against the final audit pins; its manifest has SHA-256
`7dc341fe02da40d76329e93eece5bf36a8a389adb463b0a27540ff99a5dd72da`.

This qualification does not introduce automatic merge/rename migration,
arbitrary XML layout inference, network schema fetching or support for rejected
document/particle graphs.
