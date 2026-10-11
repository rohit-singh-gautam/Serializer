# Integration enhancement qualification, 11 October 2026

This change starts from clean upstream revision
`1d64d777206087992ed17a90188dc0e816a0e409`. The compiler/runtime release is 1.13.0 and schema
language 1.8.0. Earlier compatible schemas retain their generated public API
and default native wire encoding. This report describes the upstream framework;
consumer dependency pins and product qualification are separate gates.

The selected backlog was frozen at 09:51 IST on 11 October 2026. This run covers
its fourteen upstream contracts; subsequent backlog additions are deferred.

## Capability selection

| Capability | Selection and fixture |
|---|---|
| Native keyed output measurement | Used: bounded counter, exact integer/string key and endian byte parity |
| Scalar output normalization | Used: explicit positive-zero policy, scalar/fixed-batch/nested optional/array/map traversal, unchanged source bits |
| Borrowed native output | Used: spans/reference wrappers share native traversal/counting, key/endian/policy parity and explicit source lifetime |
| Owning generic inheritance | Used: generic/concrete generated bases, named instances, inherited fields and native round trips |
| Reusable generated declarations | Used: two entry graphs sharing ordinary/generic/managed types; matching definitions reuse, incompatible definitions diagnose |
| Guarded ordinary and identified collections | Used: candidate edits and const borrowed callbacks in both managed storage profiles, history, Save/load and journal recovery |
| Checked portable expressions | Used: comparison/bool guards, division, sqrt/sin/cos, checked intermediate/domain handling in generated backends |
| Digest return signatures | Used: built-in bare/fixed/algorithm return types preserve existing storage and wire layouts in all eleven SDKs |
| Typed readonly predicates and C++ qualifiers | Used: all eleven strict SDKs, exact integers/enums, public member/index access, bounded all/any reductions, false-on-invalid and eligible constexpr/noexcept |
| Portable construction/defaults | Used: exact field/parameter types and fresh typed defaults across all eleven outputs |
| Concrete generic construction | Used: actual owner-bound constructor/factory names and self-typed parameters; opaque native hooks remain verbatim |
| C++ constant construction | Used: eligible scalar/record/generic/literal/default factory construction; runtime map specialization retained |
| Class-specific record comparison | Used: all eleven configured backends, exact uint64, partial floating order, source field selection, generic/category restrictions |
| Empty transient layout | Used: stateless record hints; C++ size checks and backend representation checks; no durable field identity |
| Existing optionals, history, journals and collaboration | Used by existing and new regression fixtures; no replacement framework |
| View/packed/emission-only construction | Not applicable to this owning native profile; unsupported combinations diagnose |
| Protobuf construction/default qualification | Not selected for this native profile; not qualified by this run |
| Non-C++ managed engines | Unavailable; ordinary portable outputs do not implement transactions/history/journals |

## Executed checks

`./make.ps1 test -Jobs 4` completed successfully: **107/107 CTest targets**,
including **477 core tests**, passed. The native run used the x64 Visual Studio
developer environment and its installed clang-format through
`SERIALIZER_TEST_CLANG_FORMAT`. It covers both C++ naming profiles, native codecs,
views, managed storage profiles, transactions, history, journals, collaboration,
installed-package consumption, version compatibility and existing examples.

| Additional qualification | Result |
|---|---|
| Checked behavior and native behavior SDK consumers | All eleven languages passed; Java/C# preserved-name and Go profile regressions also passed |
| Typed readonly predicates and bounded reductions | All eleven languages passed strict SDK builds and runtime checks; eligible C++ declarations passed actual constant evaluation and noexcept assertions |
| Portable construction and typed defaults | All eleven languages passed; eligible C++ factories passed constant evaluation |
| Managed C++ construction | Direct/separate storage with uint32/uint64 identities: four strict consumers passed |
| C construction allocation failure | Ownership and transactional failure checks passed |
| Configured record comparisons | All eleven languages passed, including exact integers, unordered floating values and nested records |
| Digest return signatures | All eleven languages passed bare, fixed and algorithm-specific return checks |
| Editor unit tests | 104/104 passed, with no skipped tests |
| Fresh generated-output navigation | VS Code: 22,485 checks; Visual Studio: 544 real .NET checks; all eleven outputs and C++ profiles passed |
| Editor packaging | Both version 1.1.35 VSIX packages rebuilt successfully through `./make.ps1 extension` |

The final typed SDK and navigation checks used the rebuilt 10:26:44 IST compiler.
C, Rust and Swift consumers used installed WSL SDKs; C++, Java, JavaScript,
TypeScript, Go, C#, Python and Kotlin used installed Windows SDKs. Kotlin's JDK
printed its compiler's deprecated `Unsafe` warning; the generated consumer still
passed `-Werror`. Missing SDKs are failures in these runners, rather than skips.

The SDK checks were split into language groups. With the same toolchains
installed, the following commands reproduce the complete selection:

```powershell
$compiler = 'out/build/make-Release/Release/serializer.exe'
$runtime = 'out/build/make-Release/Release/serializer_runtime.lib'
python test/behavior_languages.py --compiler $compiler --cpp-library $runtime --language all --wsl-languages c,rust,swift
python test/native_behavior_languages.py --compiler $compiler --cpp-library $runtime --language all --wsl-languages c,rust,swift
python test/typed_behavior_languages.py --compiler $compiler --cpp-library $runtime --language all --wsl-languages c,rust,swift
python test/construction_languages.py --compiler $compiler --cpp-library $runtime --language all --wsl-languages c,rust,swift
python test/record_comparison_languages.py --compiler $compiler --cpp-library $runtime --language all --wsl-languages c,rust,swift
```

`serializer --version` reports compiler 1.13.0 and supported schema language
1.8.0. CMake package discovery accepted requested versions 1.0.0, 1.12.0 and
1.13.0 and rejected newer 1.14.0 and different-major 2.0.0 requests. The normal
package-version and installed-package CTest checks also passed.

## Qualified limits

Reusable per-type definition guards are a C++ output feature. Typed readonly
bodies support the documented scalar, enum, record, index and bounded-reduction
profile; maps, optionals, variants, arbitrary calls and mutation remain outside
that profile and diagnose. Ordinary `expression(...)` retains checked failure and
direct scalar preflight, while `predicate(...)` returns false on a reached
semantic failure. Native hooks are emitted verbatim; portable owner-bound
construction is the supported alternative for concrete generic names.

Comparison selection is class-specific INI configuration. Portable comparison
supports selected scalar, enum and record fields; unsupported selected categories
diagnose. C++ managed direct storage is supported, while configured comparisons
with separate managed values diagnose. `layout(empty)` requires a resolved,
stateless transient record and does not promise physical field overlap in every
language or ABI. These limits are documented with the implemented features,
rather than counted as qualified future behavior.

Native build output, SDK consumers and final verification logs are under ignored
`out/` directories. Editor packaging also uses its existing ignored staging,
dependency, `bin/` and `obj/` directories. `.gitignore` already covers these paths
and toolchain caches. Required schemas, source templates, manifests and regression
tests remain tracked source. Useful final SDK outputs, the native test log and
both editor packages are retained; superseded diagnostic helpers and task-created
source-tree bytecode caches were removed.
