# Authoring construction and provider lease verification

Verified release: compiler/runtime **1.17.0**, schema language
**1.12.0**, and both editor extensions **1.1.40**, reviewed on **2026-10-11**.
This record distinguishes implemented contracts from optional storage/runtime
proposals and application-owned adapters.

## Implemented contracts and migration

Scalar trailing parameter defaults, including `count = 32`, were already
supported. Language `1.12.0` adds fresh generic owning-record defaults through
the explicit `factory(name)` constraint and `default(name)` parameter syntax.
Open C++ templates accept qualifying application types; concrete owning schema
specializations retain portable fresh factory defaults. Default field values and
absent-field decoding remain independent of authoring parameter defaults.

C++ construction adds distinct overloads, independent input types nested inside
owning record arguments, and scalar/enum readonly call bindings. Explicit
`cpp storage(none)` and `readonly constexpr(cpp)` contracts preserve eligible
constant evaluation without interpreting native tokens. Duplicate/ambiguous
default signatures reject; implicit copy/move and codec default construction
remain available. Old language headers retain their previous meaning, including
ordinary declarations using the names `cpp`, `storage`, and `common_type`.

Ordinary readonly C++ APIs accept constrained independent inputs and explicit
`common_type<T, Input>` scalar representations. Check both operands into that
representation before arithmetic. Boolean or nonnumeric policies reject,
including unused native parameters; the policy cannot define stored fields,
record/container arguments, defaults, or constraints. Strong record wrappers
and operator adapters remain application code. Host C++ callers deduce these
inputs; schema expression calls to independently generic functions remain
unsupported and diagnose explicitly.

Authoring constructors bind fields, perform typed/native validation, then run
their optional host initialization once. Their `create` factories invoke that
constructor once. Default construction, codec decoding and ordinary copies do
not repeat validation or registration. Native hooks require C++ output and
`cpp.naming = preserve`; application code owns registration, session lifetime
and rollback of side effects. Other unsupported backend APIs diagnose.

Borrowed external-provider leases add immutable provider versions, mandatory
owning lifetime/synchronization guards, checked access and one-way invalidation.
Expired/missing/wrong-version results are distinct errors. Invalidation blocks
future checked access; already borrowed references remain protected by retained
guards. Existing owning external registries and managed scalar identities retain
their APIs and protocols. Leases do not make provider edits transactional,
undoable, authorized, or journaled.

No wire format, payload revision or managed durable identity metadata changes.
Existing schemas need no migration. Opt into language `1.12.0` only when adopting
its new declarations; validate each authored native qualification before removing
an application adapter. Package discovery retains same-major release compatibility.

## Native verification

The complete Windows build through `./make.ps1 test` passed **122/122 CTests**.
The final generated authoring/hybrid executable passed **13/13** internal tests
on Windows. Coverage includes native and custom frame defaults, constructor-only
registration, failed validation before side effects, copy/decode behavior, checked
mixed arithmetic, generic input lowering, constant evaluation, version gates,
invalid numeric policy signatures and ordered readonly argument evaluation.
A final compatibility rebuild of the compiler and its core/authoring consumers
and a repeat **122/122 CTest** pass verify the preserved public callable
aggregate initialization.
Compiler `--version` and same-major CMake package compatibility also passed.
Windows logs are `out/build/authoring-qualified-test.log`,
`out/build/authoring-compatibility-build.log` and
`out/build/authoring-compatibility-test.log`, with internal test results in
`out/build/authoring-focused-test.log`.

WSL GCC **15.2.0** compiled final sources with strict C++20 warnings and passed
**7/7 focused CTests**: core, authoring/hybrid, the managed custom-key companion,
and direct/separate registries with 32-bit/64-bit persistent IDs. The registry
tests include guarded lifetime, invalidation/version checks and ownerless-token
rejection. The final compiler/core/authoring rebuild also passed with the
aggregate initialization compatibility regression.
Linux compression SDKs were disabled; this is focused Linux qualification, not
a complete Linux compression or managed-engine build. An earlier full Linux
build attempt was intentionally stopped to focus on these final-source targets.
Final Linux logs are `out/build/authoring-linux-api-build.log`,
`out/build/authoring-linux-api-test.log` and
`out/build/authoring-linux-api-version.log`.

## Portable SDK and editor verification

Existing fixed-array bindings, trailing factory defaults and independent fresh
record defaults compiled and ran in all eleven outputs: C++, Java, JavaScript,
TypeScript, Go, C#, Rust, Python, Swift, Kotlin and C. Java and Python also passed
concrete generic factory-default identity/freshness probes. These checks do not
claim open generic method APIs or native managed engines in other languages.

Both editor packages were rebuilt and validated through `./make.ps1 extension`.
Passed: **124** Node unit tests, **334** Visual Studio grammar checks, **36,309**
fresh generated navigation checks across eleven outputs and new C++ declarations,
and **609** real .NET/Jint navigation checks. Archive inspection confirms equal
`1.1.40` versions, stable IDs, the current `1.12.0` snippet and backward-compatible
navigation for user-defined `common_type` under older language headers.
No extension installation or interactive IDE-host validation is claimed.

Editor logs are `out/build/editor-112-*.log`; packages are retained under
`out/extensions/`. Portable consumer output is retained under
`out/extended-construction-112-final/`.

## Deferred optional projects

Custom managed persistent-ID representations, replacement of the store's internal
identity/location index, paged/lazy model/history storage, and non-C++ native
managed engines remain unimplemented proposals. They require separate identity,
storage, recovery and ownership contracts. Existing application-defined keys,
snapshot-local indexes and owning providers remain the supported composition.
See [state registries](state_registry.md), [paged indexing](paged_indexing.md),
and the [managed runtime](managed/cpp_runtime.md).

## Artifacts

Generated headers, native build caches, SDK consumers, final verification logs and
both VSIX packages remain under ignored `out/`. Task diagnostic probes and
source-directory Python bytecode are removed; generated output is not committed.
Maintained schemas/tests, editor manifests and contract documentation remain source.
