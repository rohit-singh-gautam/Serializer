# Qualification bug repairs — 24 October 2026

[Task status](../pending.md#qualification-and-distribution) ·
[Verification index](README.md)

Compiler/runtime **2.34.1** repairs both findings below. Schema language remains
**1.24.0** and editor packages remain **2.0.23**. Regenerate affected Python and C
outputs; existing APIs, schemas, validation defaults and native bytes retain their
contracts. The original failures below describe the earlier qualification capture.

## Repair scope

QA-B01 now selects the generated target class when inspecting a scoped projection's
namespace graph, while record output reads forwarded instance metadata for root
QName scopes. Metadata traversal does not invoke translator getters, construct an
owning target or copy borrowed source values. The projection regression also covers
a token-only nil field combined with a root QName attribute, byte/string limits,
fresh output after failure and native protocol controls.

QA-B02 explicitly initializes all namespace/identity, nested-rule and schema-default
members of `srl_xml_scalar_rule` in both ordinary and operation-aware identity
helpers. The added values are the same false/zero/null defaults previously supplied
implicitly by C11. Scalar kind, whitespace, unspecified length bounds and identity
canonicalization remain unchanged. Strict compiler warnings remain enabled.
The current companion-graph package replay also exposed an unused private decoder
forward declaration for a graph whose companion codec supplies decoding instead.
The package emitter no longer emits that unnecessary declaration; implemented
decoder definitions and document-companion behavior remain unchanged.

The current-source native GCC build also exposed `-Weffc++` failures in Java and
portable generator emitters holding borrowed managed-lowering pointers. Those
internal generation sessions now explicitly delete copying and copy assignment.
This build prerequisite changes no generated APIs or output, and no native or
generated-C warning is disabled to complete qualification.

## Executed acceptance

The final compiler/runtime **2.34.1**, language **1.24.0**, builds with native
GCC **15.2.0** in WSL using the root `make configure` wrapper and CMake's compiler
target. Python **3.14.4** uses the existing lxml provider; the scalar suite also
uses the existing independent JAXP oracle. The final replay passes:

- **8/8 projection tests**, including the added token-only QName/nil combination,
  25,000 borrowed array values, namespace/byte/depth limits, callback failures,
  fresh recovery and native-protocol byte controls.
- **55/55 Python XML tests** across projection, rich, token, identity and scalar
  suites. These five scripts run directly because the broad all-language CMake
  option is disabled in this focused build.
- **4/4 strict generated-C CTests**: `serializer_package_c`,
  `serializer_package_shared_codec_budget_c`, `serializer_c_xml_operation_policy`
  and `serializer_package_relocation_c`. Actual generated consumers compile with
  `-std=c11 -Wall -Wextra -Wpedantic -Werror`; package, identity, resource,
  default/validation and independent XML/archive oracles pass.
- **6/6 selected CTests** overall, including the three-test Python Core regression
  and aggregate package-version check. CLI version output and separate aggregate,
  runtime and compiler CMake package-version compatibility checks also pass.

Both QA-B01 and QA-B02 are closed. Exact commands and successful results remain in
ignored `out/qa-bugs-2026-10-24/verification.json`; per-suite logs and initial
failure/setup records remain beside it. The useful native build remains under
`out/build/qa-bugs-linux/`. The final six-CTest replay takes 52.37 seconds.
This is focused repair acceptance, not a repeat of all platform/provider/SDK
inventories. Original failed inventories below retain their historical results.

## Original findings

These were tracked findings, retained at the user's instruction rather than
repaired as unrelated XML implementation work. Neither has a committed repair in
the inspected main HEAD `650b5ea`; relevant main working-tree files are unchanged.
The existing Core repair from main `60468b0` is integrated independently of its
namespace-token feature.

## QA-B01: main namespace-token projection metadata

The namespace-token addition in main `60468b0` calls
`_xml_token_has_namespace(type(model))` from `_xml_token_encode`. A generated
readonly/scoped projection is an `_XmlProjectionAccess` instance. Its metadata
forwarding works on the instance; its wrapper class lacks `_xml_token_fields`.
The helper therefore raises `AttributeError` before projection output completes.

Actual `test/python_xml_projection_test.py` replay passes **8/8** with the immutable
2.33.0 compiler and produces **four errors** after importing the complete main
commit. Direct Core and token tests still pass, so they do not cover this
regression. No subsequent committed main change fixes the XML runtime call.

This branch's focused 2.33.1 backport retains only main's complete-graph Core
owning-tree admission. It excludes the unrelated namespace-token additions;
the projection regression remains a bug to fix in that main feature, rather than
a weaker workaround applied here. Retain public projection, namespace retention,
logical limits and native bytes when repairing it.

## QA-B02: strict generated C scalar-rule initialization

Generated C identity helpers in `src/xml_c_identity_runtime.inc` and
`src/xml_c_operation_identity_runtime.inc` still use short positional
`srl_xml_scalar_rule` initializers after the scalar contract acquired namespace,
identity and nested-rule fields. GCC **15.2**, strict C11,
`-Wall -Wextra -Wpedantic -Werror` rejects the generated header with
`missing initializer for field 'namespace_dependent'`.

C11 zero-initializes omitted tail members; the observed failure is a supported
strict-build regression. It blocks Linux C package, shared-codec budget, XML
operation-policy and relocation CTests, and the C member of the full SDK matrices.
Native C++ provider toggles and other language SDKs have separate outcomes.
The Windows MSVC C profile passes; that does not establish strict GCC admission.

The qualification does not suppress warnings, remove C from a full-inventory
success claim, or change initializer semantics. Full requested runs retain their
failed C outcome. Separately selected non-C runtime inventories are labeled
diagnostic subsets. A repair should initialize every scalar-rule field from its
intended source/default and retain namespace/identity behavior and wire bytes.

## Retained evidence

Raw logs and reports remain under ignored `out/qualification-2026-10-24/`:
`baseline-projection-recheck.log`, `patched-python-xml-ctest.log`,
`baseline-python-rechecks.json`, `linux-build-test-link-path.log`, and subsequent
explicit full/subset SDK reports. The prototype compiler and emitted artifacts
are retained separately from the final Core-only 2.33.1 validation. The original
Python Core regression is already repaired in main; no duplicate bug is added
for that known repair.
