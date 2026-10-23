# Named managed self-reference qualification — 24 October 2026

Compiler/runtime **2.31.0**, schema language **1.23.0**, and matching editor
packages **2.0.15** initially implement the versioned C++ named-self binding contract.
The feature is committed as `911394e`. This record completes **MR03a** within
the scope below. The separate editor **2.0.16** allocation repair completes the
native adapter gate; MR03 remains open for other-output token codecs and broader
recursive generic selection.

## Implemented scope

The instantiate declaration retains its language provenance. Under language
1.23.0, copied direct token fields and their supported container wrappers bind an
exact generic self target to that named record. Family identity and resolved
arguments, including defaults/dimensions, must match. Each named copy remains a
separate target class. Shared nested/base records and callable signatures keep
their existing binding rules; unsupported recursive graphs continue to diagnose.

Earlier declarations retain their old AST binding and C++ generation rejection.
The parser tests cover an older included instantiate under a newer entry header.
The reference envelope, class-ID algorithm, field IDs, owning layout contracts,
public runtime APIs and all four ordinary codecs retain their existing behavior.

## Current checks

| Check | Status |
| --- | --- |
| MSVC parser/codegen controls | Pass, including old-header rejection, new named targets, distinct class tags, included declaration provenance and shared nested-record rejection |
| MSVC generated runtime profiles | **8/8 commands pass**, six cases each: compact/fixed encoding × direct/separate values × uint32/uint64 identity; final parser/example replay passes 10/10 commands |
| GCC native profiles and full core | Pass: all 613 core cases, parser/codegen controls, all eight six-case runtime profiles and the runnable cycle example; final parser counterexamples also pass |
| Clang installed consumers | Windows clang-cl and Linux Clang pass installed headers, capability consumers, compiler-only/native discovery and ordinary examples; the named example passes after a Linux formatter-setup correction |
| Earlier-schema generated artifact comparison | **19 byte-identical files** against the retained 2.30.1 compiler: all eleven ordinary generic outputs and eight managed-reference profiles |
| Editor shared navigation and matching packages | Feature-only **2.0.15** packages pass 162 tests; the **2.0.16** resource follow-up passes **163 tests** and **50,713 fresh bidirectional checks** |
| Native editor allocation follow-up | **52** direct/separate named-reference checks plus **1,548** existing adapter checks pass under the unchanged 256 MiB budget; [repair and matching packages](verification-native-cpp-navigation-2026-10-24.md) |

Runtime cases exercise optional/sequence/map self edges, all four codecs,
Save/load and history, distinct named-type rejection, foreign/deleted targets,
independent import rewriting and full/delta appended/sidecar journal recovery.
Required self/mutual edges also use the existing constructor reference initializer
after native allocation. Each host passes **48 runtime cases** across eight profiles.
Windows passes all **314 configured tests** in **1,849.77 seconds**, followed by
the final ten-command parser/profile/example replay in **2.81 seconds**.
The initial GCC build reports two conservative temporary-handle lifetime warnings
in the new tests. Explicit entity pins remove those warnings without changing
runtime code or assertions; the strict replay passes. The final parser control
passes **12 cases**, including same-family type/dimension counterexamples.
The Linux installed example initially invokes an unavailable bare `clang-format`;
its explicit formatter-independent configuration removes that irrelevant setup
dependency. The first failure and successful replay remain retained. The example
explicitly requires an installed Serializer 2.31 runtime.

The feature-only 2.0.15 package SHA-256 hashes are
`c15a029b05289f0bbfe9ad98181d0a8e66062f819699b3376b182f3e0370c001`
(VS Code) and
`8f6ce2ab13efc7f7647407f5a52d30645cd0f3c3614fc1c749edb75309bd1fd1`
(Visual Studio). The separately qualified editor allocation repair is not included
in those feature-only packages.

Logs and reproducible drivers are under ignored
`out/build/recursive-reference-overnight/`. Linux journal data must use exclusively
owned native temporary directories, preserving the mounted-filesystem limitation
recorded in [C++23 qualification](verification-cpp23-2026-10-23.md).
No other-language managed engine or token codec, hard power-loss profile,
interactive editor installation or Marketplace publication is claimed.

See the [language contract](../schema/named_managed_references.md),
[example](../../example/managed/named_references/README.md), and
[MR03 backlog](../pending.md#managed-references-and-migration).
