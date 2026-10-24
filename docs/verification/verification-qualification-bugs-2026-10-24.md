# Additional qualification findings — 24 October 2026

[Pending bugs](../pending.md#qualification-and-distribution) ·
[Verification index](README.md)

These are new tracked findings, retained at the user's instruction rather than
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
