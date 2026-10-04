# Generic and dimension template qualification — 4 October 2026

This report records the implementation of [dimension templates](generic_dimension_templates.md)
with the accepted native C++ template scope update. C++ applications can use nested
`result<T>`, `response<T>`, and `message<T>` without schema applications. `instantiate`
and concrete field applications remain optional contracts for other generators and
schema compatibility checks. See [generics](generics.md) for the implemented API.

## Build identity and environment

- Source baseline: `d9c3508934a4add65efdec90f402eacd773a7d38`, with uncommitted
  implementation changes. This is qualification of the working tree, not a released
  immutable revision. The first supporting release/revision has not been assigned.
- Compiler version remains 1.0.0 and schema version remains 1. Older compiler builds
  do not support the new syntax; use a generator and runtime from this change together.
- Native Windows 10.0.26300.0, x64; Visual Studio 2026, MSVC 19.51.36257.0,
  toolset 14.51.36231, MSVC STL version 145/update 202604. CMake 4.4.2,
  Release configuration, SIMD enabled.
- Core regression target: C++20. Nine naming-profile consumers and three dimension
  managed-history consumers: C++23.
- Managed profiles: direct values with uint32 IDs, direct values with uint64 IDs,
  and separate values with uint64 IDs. Configurations are checked in under
  `test/resources/dimensions_*.ini`.
- Journal fixtures used native Windows temporary directories on the local C: NTFS
  volume. Fault injection terminates the helper after journal payload writing with
  exit code 86; recovery runs in a fresh process. This does not establish behavior
  under every storage device failure or power-loss boundary.

The generator was rebuilt after the final lowering cleanup. Clean generation into
`out/dimensions-clean/` exactly matched the tested default-profile headers below.
Generated headers were compiled against this checkout's runtime headers.

| Artifact | SHA-256 |
| --- | --- |
| `out/build/make-Release/Release/serializer.exe` | `337594DA795DA16CF1388EC61D16E120BC1164C0E70B718A165C7678E60358B3` |
| `test/resources/dimensions.serializer` | `1FA87BA6A636F881CA808A11BA15E66B91CAAD79E474B63FFB9E5D52B8990E7B` |
| `test/resources/generics.serializer` | `B4F313506D1C50C853E067380428160CE2F0EFCA521BAF036E48846DB1240CC8` |
| `test/resources/dimensions_managed.serializer` | `6F0C524092FA66E82588173D92292CB5F8D12875C5DB4A0ACAEC7FA438C501B5` |
| Generated `test/dimensions.hpp` | `E3E19240BCFB24F9CE1BDEA7A9068F3A8E55E918820190066B5AF68964EEC161` |
| Generated `test/generics.hpp` | `F0E1EF2613C0DF82A2286986B7A7D346E5911CD1B312E83E09F5FAFC7006F5C0` |

## Executed checks

Commands below run from the repository root unless a working directory is stated.
All completed with exit code zero. Logs and generated/package artifacts are local
build outputs; maintained fixtures and runners are checked-in source.

| Check | Command or entry point | Result / local log |
| --- | --- | --- |
| Release build | `cmake --build out/build/make-Release --config Release --parallel 3` | Passed; `out/dimensions-final-build.log`, `out/dimensions-profile-final-build.log` |
| Full suite | `ctest --test-dir out/build/make-Release -C Release --output-on-failure` | 68/68 passed, 53.05 seconds; `out/dimensions-full-ctest.log` |
| Final core rebuild/run after adding maximum malformed count | Build target `core_serializer_test`, then `out/build/make-Release/test/Release/core_serializer_test.exe` | 257/257 tests passed; `out/dimensions-final-core-tests.log` |
| Host-only C++ specializations | In an x64 MSVC developer shell: `python test/dimension_compile_test.py --compiler cl --generated out/build/make-Release/test` | `matrix<4>` compiled; zero, negative, excessive extent and dependent overflow rejected; `out/dimensions-compile-checks.log` |
| Concrete generic language consumers | `python example/generics/run.py --compiler out/build/make-Release/Release/serializer.exe --build out/generic-interop-native --language all --wsl-languages rust,swift,c` | 11 languages, four protocols passed; `out/dimensions-all-languages.log` |
| Editor unit tests | In `editors/vscode`: `npm test` | 72/72 passed; `out/dimensions-editors-tests.log` |
| Generated navigation | In `editors/vscode`: `npm run test:navigation:generated` | 11,961 bidirectional checks, 11 output languages and nine C++ profiles; `out/dimensions-navigation.log` |
| Visual Studio resolver | Build `editors/visual_studio/test/navigation_test.csproj`, run its executable with repository and fresh complex/generic output directories | 89 checks through the real .NET shared resolver; `out/dimensions-vs-navigation.log` |
| VS Code package | In `editors/vscode`: `npm run package` | `out/extensions/serializer-vscode-1.1.15.vsix`; `out/dimensions-vscode-package.log` |
| Visual Studio package | `editors/visual_studio/build.ps1` (includes package validation) | `out/extensions/serializer-visual-studio-1.1.15.vsix`; `out/dimensions-vs-package.log` |

The language consumer run used native Windows C++, Java, JavaScript, TypeScript,
Go, C#, Python and Kotlin, with Rust, Swift and C under WSL. This qualifies existing
concrete type-generic contracts across languages, not fixed arrays outside C++.
The runner uses the installed SDKs; TypeScript and Kotlin executable overrides
were supplied through `SERIALIZER_TSC` and `SERIALIZER_KOTLINC`.

The final core-only rerun adds the maximum malformed wire-count assertion to the
already-passing full suite; no other runtime change followed that full run.
Clean regeneration used `--input`, `--language cpp`, and `--output` for each of
the two schemas in the digest table, with normal formatting enabled.

## Acceptance evidence

| Gate | Executable evidence |
| --- | --- |
| D01 | `dimension_profile_test.cpp.in`, compiled and run for serializer, core, google, llvm, gnu, cert, misra, autosar and qt; 2D/3D points/frames, defaulted squares, rectangular float/double matrices and nested native generic messages. Core remains C++20. |
| D02 | `dimension_templates.generated_types` and `schema_generics.dimension_defaults_share_lowered_nodes`: default/explicit applications share types and lowered nodes; named roots remain distinct. |
| D03 | `schema_generics` and `schema_include` tests plus `dimension_templates.invalid_dimensions_and_defaults`: nested substitutions, lexical/qualified lookup, defaults, kinds, arity and invalid forward/self references. |
| D04 | Dimension parser tests and `dimension_compile_test.py`: zero, negative, uint64 overflow, operator restrictions, excessive extent and dependent C++ arithmetic failures. The maximum uint64 argument also generates and compiles. |
| D05 | `canonical_resource_boundaries` and `expression_and_storage_limits`: exact/adjacent expression, canonical identity, specialization, nesting and extent limits, and nested inline-storage overflow. |
| D06 | `generated_types`, `wire_and_cardinality`, and profile consumers: distinct nonzero values through all four protocols; frozen binary bytes and JSON match ordinary variable arrays. |
| D07 | `wire_and_cardinality` and `exact_decode_budgets_and_missing_fields`: short/long/empty/maximum wire counts, truncation, invalid elements, trailing input, input/allocation/collection/work/depth budgets and unchanged publication on failure. |
| D08 | `compatibility_directions_and_defaults`, existing `schema_generics` compatibility and stable-ID reservation tests: extent and element changes, renamed parameters, omitted versus explicit defaults, and fixed/variable reader directions. |
| D09 | Three `dimension_history_*_test` subprocess tests: generated whole-value setters, no-change/revert, tree branches, memory save/load, full sidecar Save and fresh-process recovery compare nested values, envelopes and revision identities. |
| D10 | Same history runners: over-budget loads, malformed snapshot cardinality, corrupt file headers, and termination after journal payload writing; failed loads preserve published state and recovery discards the incomplete tail. Existing journal/crash tests also pass. |
| D11 | `dimension_templates.backend_diagnostics` checks all ten non-C++ generators and direct Protobuf rejection. Eleven-language concrete generic consumers pass all native protocols. |
| D12 | All nine C++ profiles; editor grammar/navigation unit tests, fresh generated-output checks and .NET resolver checks cover dimensions/defaults, qualified/transitive includes, unsaved edits and cursor/selection boundaries. Both packages are 1.1.15. |
| D13 | Full 68-test CTest suite and concrete generic consumers retain existing codec, compatibility, ordinary-array, managed-history and example behavior. Existing type-only generic names and wire fixtures remain covered. |

The resource policy is 32 expression levels, 256 expression nodes including
parenthesized groups, 32 generic nesting levels, 1,024 concrete schema applications,
4,096 canonical identity bytes and fixed extents 1..65,536. Checked arithmetic uses
uint64. Nested minimum inline storage must fit host ptrdiff_t; native application
specializations also obey the target C++ compiler's object-size constraints.

## Supported boundaries and remaining qualification

C++ native templates and fixed arrays support binary_none, binary_integer,
binary_string and JSON. Application-selected template arguments must have suitable
runtime codecs. Concrete cross-language contracts still determine matching fields,
types and protocol behavior; an open template alone is not a wire contract.

The other ten generators retain concrete records/codecs for supported generic
applications. They explicitly reject fixed-array fields, including those in unused
generic definitions. Direct Protobuf generation also rejects fixed arrays. There
is no silent conversion to a variable array. Generic definitions alone emit no
models in these non-C++ generators.

Both editor packages were rebuilt and validated, but were not installed. Automated
resolver checks do not claim interactive Visual Studio or VS Code host validation.
No Linux-native dimension/managed qualification, big-endian target, sanitizer run,
or performance benchmark was performed. WSL results above apply only to the listed
cross-language consumers. An immutable supporting revision remains to be assigned
when this working-tree change is committed/released.
