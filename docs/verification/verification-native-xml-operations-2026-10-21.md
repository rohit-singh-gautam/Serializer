# Native XML operations and selected package codecs, 21 October 2026

[Backlog](../pending.md) · [Package APIs](../serialization/xml_packages.md) ·
[Previous qualification batch](verification-managed-reference-faults-2026-10-21.md)

Compiler/runtime **2.18.0**, schema language **1.21.0**, editor packages
**2.0.10**. This batch completes `XML-B20` for the supported generated owning
profiles. Broader graphs, provider interruption, process peaks and platform
qualification remain separate requirements. Existing C/Rust document-companion
package graphs still reject explicitly; this release removes only their supported
XSD identity-graph gate.

## Implemented contract

C adds a separate `srl_xml_operation`, generated model codec descriptors and
collision-selected package `*_with_codecs` functions. Existing native limit,
output and identity aggregate layouts and function-pointer signatures remain
valid. Operations borrow callbacks and policies synchronously; the first arbitrary
nonzero caller integer and its native resource/allocation/semantic classification
remain distinguishable.

Rust adds a collision-selected operation module with `Operation`, `Options` and
qualified `Codec` methods. Existing five-field `XmlLimits` literals,
`Error(pub &'static str)`, inherent methods and old result signatures still compile.
Package `XmlOperation` and typed `*_with_codecs` callbacks borrow the same ledger;
the first arbitrary caller error retains its identical `Arc`.

Primary native XML traversal admits elements, physical attributes including
namespace declarations, and expanded-name/decoded scalar bytes exactly once.
Secondary identity/header passes admit their real work and retained storage
without repeating primary XML counters. Every complete key/unique/keyref tuple
spends a reference before equal keyrefs are deduplicated. Nested identity owners
retain separate indexes. Known generated default storage is admitted before
construction. Opaque authored factory internals and provider-private temporary
storage are not hard allocation or preemption hooks.

Diagnostics use immutable generated schema owner/member projections independently
of XML aliases or output naming profiles. Native local quota branches classify
resource failures explicitly. Header dispatch observes cancellation before its
local quotas; root/type checks precede caller decoders. Failed callbacks, ignored
native failures, middle-role failures and full Save validation cannot publish an
aggregate or replace the previous file.

## Public composition across eleven outputs

| Output | Caller-owned composition boundary |
| --- | --- |
| C++ | `budget.xml_context(part, relationship_id)` and typed writer/loader callbacks |
| Python | `PackageBudget.xml_context` and generated XML codec context arguments |
| Java | Public Composer/Loader callbacks and bridged XML limits/operation |
| C# | Public Composer/Loader callbacks and bridged `Limits.Operation` |
| Swift | Composer/Loader `CodecContext` and `SerializerPackage.xmlLimits` |
| C | Native operation descriptor and selected-codec table/context helpers |
| Rust | Native operation trait and package `XmlOperation.encode/decode` |
| Go | Separate collision-selected per-role callback functions receiving bridged `XmlOptions`, part and relationship ID |
| JavaScript | Synchronous role-keyed `options.codecs` with bridged limits and context |
| TypeScript | The same runtime plus strict generic codec/context and facade option declarations |
| Kotlin | Public facade `xmlLimits(context[, configured])`, including consumers in a separately compiled module |

The existing C++/Python/Java/C#/Swift package and application tests qualify their
public boundaries. The [105-item record](verification-pending-105-2026-10-21.md)
retains their original real SDK evidence. This batch adds external Go imports,
strict TypeScript declarations/runtime calls and separate generated/consumer Kotlin
JARs so private generator hooks cannot satisfy the public composition tests.

## Windows execution

The MSVC 19.51 RelWithDebInfo build completes all enabled native targets under
their configured strict warnings. Final compiler sources are captured before
the build and verified unchanged afterward: **406 maintained compiler/runtime
files**. Final image and report hashes are retained in the integrated manifest.
The Windows compiler SHA-256 is
`efc1aa292e1d8e273eb15ee19eb5b05b22bc720ce6b3fc082183b85231855149`;
the Linux ELF SHA-256 is
`7e7cbe05b59fee7d9d17a58f0f4fb7c8b5df5a3499a99c577d6b40ca28731cf7`.

| Gate | Actual result |
| --- | --- |
| C shared native/package operation | 399 original and 408 nested/inherited/concrete-generic assertions; sanitizer replay passes |
| C independent native policies | 21,367 assertions and 12 independent XML inventories; sanitizer replay passes |
| C ordinary regression runners | All 12 pass, including extension, mixed, token, identity and ordinary native controls |
| Rust shared operation and header failures | 47 original and 54 expanded assertions, including expanded-namespace quotas and exact cancellation error identity |
| Rust independent native policies | 141 controls and 10 independent exact XML comparisons |
| Rust ordinary regression runners | All 12 pass, including extension, mixed/scalar, token, identity and ordinary native controls |
| Existing Rust package / application fixtures | 64 / 16 independent cases |
| External Go / JavaScript / strict TypeScript / Kotlin | 43 / 41 / 42 / 41 assertions |
| Existing Go package / application | 62 independent inputs, 7 outputs and extra profiles / 55 assertions |
| Existing JavaScript/TypeScript package / application | 175 independent inputs, 14 outputs, 56 lifecycle fences, browser isolation and strict declarations / 64 assertions |
| Existing Kotlin package / application | 241 checks, 180 independent inputs, 11 outputs / 30 assertions |
| Native/core/version/package regression | 46/46 checkpoint CTest cases pass, including core, XML, ZIP/OPC, installed/compiler packages and same-major version compatibility; final diagnostic change passes 4/4 focused checks |

Independent Expat counts the shared payload inventory as **12 elements, 15
physical attributes and 543 expanded-name/scalar bytes**. Native identity spends
12 references; the package including relationships spends 15. A caller composing
the selected native decoder twice spends 27, demonstrating one cumulative ledger.
Exact and one-tight reference/work/allocation policies, cancellation, original
error identity, wrong-root-before-callback, swallowed native failures and atomic
Save are exercised. C allocation-failure probes visit generated owned allocation
sites; sanitizer success is separate from a hard provider-memory guarantee.

## Linux execution

The native WSL Ubuntu configuration uses GCC **15.2**, CMake **4.2.3**, LibXml2
**2.15.2** and zlib **1.3.1**; Zstandard/LZ4 development providers are absent.
The final ELF is built from the same verified source manifest as Windows.

| Gate | Actual result |
| --- | --- |
| C shared operation | 398 original / 407 expanded assertions, including ASAN/UBSAN; Windows has one additional thread-join assertion |
| C native policies | 21,367 assertions / 12 independent inventories, including ASAN/UBSAN |
| Rust operation/policies | 47 original / 54 expanded; 141 policy controls / 10 independent XML comparisons |
| Rust ordinary scalar/mixed and package/application | 183 scalar controls plus 6 gates, 21 mixed controls plus 3 gates, 64 package / 16 application fixtures |
| External Go / JS / strict TS / separate-module Kotlin | 43 / 41 / 42 / 41 assertions |
| C ordinary regression replay | Token, scalar, mixed and extension runners pass; identity passes 45 checks / 5 binding fences with the local JDK; map SDK passes all 33 cases against the independent JDK oracle |
| Core/XML/package/CMake regression | 45/45 checkpoint CTest cases pass; final diagnostic change passes 4/4 focused checks |

Actual Linux SDKs include Go **1.26.0**, Node **24.15.0**, TypeScript **5.9.3**,
Kotlin **2.2.21**, OpenJDK **25.0.4.1** and Cargo **1.93.1**. Generated C/Rust
operation fixtures and all five public Go/JS/TS/Kotlin library artifacts match
Windows byte for byte. An independent Python ZIP/XML reader checks all eight
external-consumer output archives and their 24 identity-owner payloads.

The Linux lxml **6.0.2** wheel links LibXml2 **2.14.6**. Its independent XSD
oracle accepts duplicate unsigned-integer map keys `-0` and `0`, disagreeing with
the independent JDK oracle and the selected canonical-value contract. The
original lxml runner failure is retained. All 33 Linux map inputs are separately
validated by JDK SchemaFactory before the actual generated SDK executes; this is
not reported as a successful replay of the unchanged lxml oracle. Windows uses
its existing lxml **6.1.3** provider and passes the original runner.

## Provenance, negative controls and limits

Evidence is retained under
`out/build/pending-native-operation-2026-10-21/`, including compiler images,
source manifests, actual SDK commands, source/executable/report hashes, independent
archives and original failure logs. `integrated-verification.json` inventories the
final records and any byte-identical generation bridges between compiler snapshots.
Its SHA-256 is
`e91dabbbe7b9bb34e19692677669ef5e15ea98b3eef1dadad3e16c096046fdfd`.
Earlier images are not described as executing later fixes.
The broader CTest sets precede the final diagnostic-only synthetic-map repair.
After that repair, compiler/core targets and the four core/CLI/version/literal
checks are rebuilt/replayed on both hosts. Affected native map/policy SDKs execute
the final image; other results retain exact generated-source bridges.

Maintained negative controls exposed and then verified repairs for ignored C
native callback failures, missing known-default admissions, strict C/Rust gated
wrapper warnings, mixed/text/synthetic-map member diagnostics, expanded-namespace header quota
classification and cancellation versus local header quotas. Initial missing Python
or Node XML dependencies are recorded separately from source failures. The old
JavaScript package runner now restores the maintained locked XML dependencies
when a supplied dependency directory lacks them.

No schema grammar, native wire representation, payload revision or editor
navigation/output layout changes. Both editor listing payloads, READMEs and
changelogs were reviewed; their unchanged language support needs no new package.
No editor packages are installed and no release is published by this batch. Logical admission does not
measure process peaks, container spare capacity or provider/factory internal
temporaries. Large-model performance, harder interruption/deadline guarantees,
broader document graphs, signatures and lexical preservation stay in their
separate pending contracts.
