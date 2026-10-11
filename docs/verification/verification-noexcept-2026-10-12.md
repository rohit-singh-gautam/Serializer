# Explicit C++ noexcept function qualification

Qualified release: compiler/runtime **1.18.0**, schema language
**1.13.0**, and both editor extensions **1.1.42**, reviewed on **2026-10-12**.
This record distinguishes the implemented exception-signature contract from
native implementation promises and unavailable language extensions.

## Capability selection

| Capability | Selection and reason |
| --- | --- |
| Typed readonly native/external functions | Used to migrate a C++ `const noexcept` signature without opaque class hooks. |
| `noexcept(cpp)` | Used as an explicit unconditional C++ author promise, including namespace and dispatched member contracts. |
| Existing `constexpr(cpp)` | Used together with `noexcept(cpp)` on eligible ordinary readonly native bodies; either order is accepted. |
| Portable expression/predicate inference | Used unchanged for existing schemas; explicit exception qualifiers are rejected on these bodies. |
| Portable behavior migration preference | Used in the canonical integration skill; supported logic moves into shared schema expressions/predicates before considering per-backend adapters. |
| Behavior adapter forwarding guarantees | Used with conditional `noexcept` over the full interface call, including potentially throwing by-value argument copies. |
| Multi-language outputs | Used with C++ qualifier ignored outside C++; no foreign exception suppression or panic guarantee is inferred. |
| Editor syntax and schema navigation | Used to recognize the qualifier while preserving function declaration and generated-output navigation. |
| Fields, codecs and wire compatibility | Used unchanged; function qualifiers introduce no serialized data or runtime metadata. |
| Constructors, managed transactions, registry leases, history, journals and collaboration | Not applicable: the requested change concerns readonly function signatures and requires no state/runtime change. |
| Conditional `noexcept`, arbitrary C++ postfix tokens, other language qualifiers | Unavailable in this release; each would require its own explicit contract and parser/generator support. |
| Portable expression/predicate override of an explicitly qualified base | Unavailable: inherited exception contracts require an explicit qualifier and portable bodies reject it. |
| Independently generic external `noexcept(cpp)` functions | Unavailable: automatic external-definition checks cannot qualify every specialization; supply a native C++ body. External methods using only class generic parameters remain supported. |
| Open-generic dispatched C++ signatures | Qualification uses parameterless compiled regression signatures. Renamed/dependent inherited parameter signatures retain existing backend contract-resolution limitations; full generated dispatch support is not claimed. |

## Contract and migration

Use `public function bool complete() readonly noexcept(cpp);` instead of an
opaque native declaration `public cpp storage(none) { bool complete() const
noexcept; }`. The application owns the external definition and must match its
generated exception signature. A native C++ body may instead follow the qualifier.
Each declaring file requires language `1.13.0` or newer, including included files.
Matching derived declarations preserve an inherited virtual/abstract explicit
exception contract.

Prefer a portable implementation such as `readonly predicate(completed == expected)`
when it expresses the complete behavior. This generates the shared logic for each
selected language; a C++ native body requires separate bodies or external
implementations for other targets. Explicit qualifiers remain target-specific
API metadata and do not create a portable exception guarantee.

The native author promise does not prove purity or inspect throwing operations.
An exception escaping C++ invokes `std::terminate`. Other targets preserve their
own normal failure behavior. Explicit native/external qualifiers do not expand
the existing proof used to infer caller exception guarantees. No existing schema
requires migration; no encoded layout, wire ID or payload revision changes.
Native and interface signatures emit unconditional `noexcept`; generated behavior
adapters use conditional `noexcept` for the full forwarding call so a throwing
by-value argument copy can propagate before the interface method is entered.
For open generic bases, the parser checks explicit qualifiers after substituting
generic arguments. Generated dispatch qualification remains limited to
parameterless signatures; renamed or dependent inherited parameter signatures
retain existing backend contract-flattening limitations. Preserve native
adapters for affected models and compile their selected generated output.
See [behavior](../schema/behavior.md#explicit-c-noexcept-contracts),
[usage](../guides/usage.md) and [migration](../../migration.md).

## Validation status

Editor qualification passed **126 VS Code unit checks**, **346 Visual Studio
installed-grammar/fallback checks**, and **775 .NET/Jint resolver checks**.
Fresh compiler/runtime **1.18.0** generated fixtures in all **11 output languages**;
their generated-output navigation passed **36,896 bidirectional checks**.

Both synchronized **1.1.42** VSIX packages were rebuilt through
`./make.ps1 extension` and validated from the successful direct console run.
Retained packages are `out/extensions/serializer-vscode-1.1.42.vsix`
(75,178 bytes) and `out/extensions/serializer-visual-studio-1.1.42.vsix`
(1,164,896 bytes). Installation and interactive editor-host checks were not rerun.

The final Windows native build and test run completed successfully through
`./make.ps1 test -Configuration Release -BuildDirectory out/build/make-Release -Jobs 4`
in an x64 Visual Studio developer environment. All **123/123 CTest checks** passed
in **208.88 seconds**, including `serializer_package_version` and
`serializer_installed_package`. The compiled `cpp_noexcept_test` regression passed
all **9/9 internal cases**. `serializer --version` reported
compiler/runtime **1.18.0** and schema language **1.13.0**. The final verification
log is retained at `out/build/noexcept-windows-final.log`.

Linux `serializer --version` also reported compiler/runtime **1.18.0** and schema
language **1.13.0**. The WSL GCC run through
`make test CONFIG=Release BUILD_DIR=out/build/make-ReleaseLinux JOBS=4` passed
**114/115 configured checks** in **289.44 seconds**. The only failure was
`serializer_include_cli`, which reported a `basic_ios::clear` I/O error while
writing the C `schema.h` output; parsing/backend generation succeeded and the
other ten language outputs were written. A targeted root-wrapper retry with the
same options and `CTEST="ctest -R ^serializer_include_cli$"` passed **1/1** with
exit code zero (**5.38 seconds** test time; **5.58 seconds** total), with no source
changes between runs. All **115 configured Linux checks passed across the full
run and targeted retry**; this was not a single uninterrupted 115/115 pass.
`serializer_package_version` and `serializer_installed_package` passed in the
full run. Retained logs are `out/build/noexcept-linux-final.log` and
`out/build/noexcept-linux-include-retry.log`.

The Linux run did not configure optional compression examples; Windows configured
the available compression examples. Native source, release/package compatibility,
editor and package verification is complete within these configured platform
scopes. Package installation and interactive editor-host checks were not requested
and were not rerun.
