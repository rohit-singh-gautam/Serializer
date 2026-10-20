# Selected pending XML/package implementation — 20 October 2026

[Pending requirements](../pending.md) · [Migration](../../migration.md#compilerruntime-2150) ·
[Verification index](README.md)

Compiler/runtime **2.15.0**, schema language **1.20.0**. This record describes
the scoped `OPC-R03a`, `OPC-R06a` and `SC02a` implementations. Their broader parent
requirements remain pending. No package, extension or registry was published.

## Contracts implemented

- C++ child unions preserve the first successful member and its QName/QName-list
  expanded identities in inherited or equivalent child/owning-record scopes.
  Optional/repeated fields, nested unions and earlier string alternatives use
  the same rule. Child-local/shadowed bindings that would change identity or
  selected member still reject. Namespace-dependent outer union enumeration,
  fixed/default values and QName-list enumeration/fixed restrictions remain gated.
- Python namespace-independent outer union enumeration/fixed restrictions compare
  selected primitive value identities and ordered list items. Decimal-derived
  numeric members share their primitive space; Boolean, float, double and string
  spaces remain distinct. Selected-member whitespace and retained source spelling
  prevent normalization from silently choosing another branch on re-encoding.
  Provider/resource/callback failures propagate rather than select a fallback.
- Generated C++/Python fixed singleton typed package roles retain discovered names
  in the additive extension `relocations` field. Preserved relationship sources
  and internal targets rebind simultaneously to declared canonical paths. Targets
  resolve in the original source scope before either endpoint moves, retaining
  query/fragment spelling. External target text and opaque bytes stay unchanged.
  Caller-owned records remain reusable; conflicting origins, dangling references
  and duplicate source-local IDs reject. Other package backends and dynamic role
  renames still need explicit application rebinding.
- Explicit language 1.14+ integer defaults diagnose known decimal/hex literal
  narrowing at the original declaring member. Includes and generic binding retain
  the member path/span/line/column. Existing aggregate/generic failures also report
  their declaration. Native scalar expressions, suffixed/octal literals, numeric
  Boolean defaults and legacy/headerless handling remain compatible. Broader
  default validation and bit fields remain pending.

Existing native wire IDs, generated names, managed side channels and schema
language syntax are unchanged. Rebuild C++ consumers with matching runtime headers
and regenerate affected output. Budgets provide logical admission/work/scratch
limits; these checks do not measure hard process peak allocation or qualify
unexecuted platforms/providers.

## Executed checks

Windows uses MSVC 19.51, CMake 4.4.2, Python 3.14.3, Expat 2.7.3, lxml 6.1.3
(compiled/runtime LibXml2 2.11.9) and the installed
optional native providers. The native configuration is `RelWithDebInfo`, with XML,
zlib, Zstandard, LZ4 and managed support enabled; all-language examples remain off.

| Check | Result |
| --- | --- |
| Fresh generated Python scalar SDK | 7 tests passed; 89 union lexical decisions independently checked with lxml, plus child/default/fixed and callback/resource probes. |
| Fresh generated Python rich SDK | 11 tests passed, no skips. |
| Fresh generated Python token SDK | 12 tests passed, no skips. |
| Fresh generated Python package SDK | 32 tests passed, no skips; independent ZIP/XML inputs and Save inspection include new rebinding/conflict cases. |
| Compiler CLI default probes | 12 cases passed, covering bounds, preserved native/legacy spelling, includes and generic narrowing diagnostics. |
| Focused native default regressions | All 6 tests passed. |
| C++ generated scalar SDK | Native checks passed; the independent Python/lxml runner passed all 206 scalar oracle probes. |
| C++ scalar contract runtime | All 15 tests passed, including equivalent QName prefixes and selected-member whitespace. |
| C++ simultaneous relocation regression | The focused path-swap/source-scope/selector/external-target test passed. |
| CLI release metadata | `Serializer compiler 2.15.0`; supported schema language `1.20.0`. |
| Complete native CTest inventory | 212 distinct registered cases verified: initial wrapper 203 passed/9 environment failures in 609.72 s; all 8 named workspace-temp retries and the isolated Swift retry passed. |
| Final C++ rebuild and regressions | Root wrapper rebuilt the final test-only edits; 2/2 selected CTests passed. The final refreshed core/XML/package subset passed 37/37 in 26.90 s; native scalar and all 206 oracle probes passed again. |
| Installed package/version discovery | Version, installed consumer and compiler-only component cases passed in the complete inventory. |
| Opt-in formatting unit | The full run's pre-existing formatting skip was rerun with the configured clang-format 22.1.3; the unit passed. |

The maintained regressions are in `test/xml_scalar_test.cpp`,
`test/resources/xml_scalar/qname_union.xsd`, `test/python_xml_scalar_test.py`,
`test/opc_profile_test.cpp`, `test/package_test.cpp`, `test/python_package_test.py`,
`test/serializer_parser_test.cpp`, `test/schema_include_test.cpp` and
`test/xsd_import_test.cpp` and `test/xml_scalar_contract_test.cpp`. The C++ XSD
fixture is a declared generation dependency.

The QName prefix repair has a retained before-control: the public validator
rejected an equivalent document prefix with `Unbound QName prefix` before the
repair and accepted it afterward. Expanded schema enumeration identities are now
authoritative without suppressing ordinary string enumerations. Union whitespace
follows the successful member's policy, consistent with
[XML Schema 1.0 Datatypes §4.3.6](https://www.w3.org/TR/xmlschema-2/#rf-whiteSpace).

The independent LibXml2/lxml provider rejects an unbound-QName-first alternative
even when a later string alternative can accept its text. That selected boundary
is explicitly retained as a generated-only round trip; the oracle-backed fallback
uses free-form text. `cpp-qname-provider-boundaries.log` retains both decisions and
the padded/unpadded member-selection controls. The initial native scalar failure
is retained in `cpp-scalar-native-initial.log`; this exclusion does not qualify
general union/provider conformance. Broader `OPC-R03` and `QA02` remain pending.

## Reproduction and retained evidence

The root native verification uses `./make.ps1 test` with the existing
`out/build/ReleaseWindows` cache, `RelWithDebInfo` and four build jobs. Cached native
dependencies are reused with `VCPKG_MANIFEST_INSTALL=OFF`; the usable Visual Studio
clang-format 22.1.3 is selected explicitly. The initial default configure could not
write a vcpkg tool download, and its cached Swift formatter was not executable in
the sandbox. These tool selections resolved configuration without source changes.
Preparatory builds were stopped before CTest to include the final Python/C++ union
repairs. Their logs are retained separately from the verification run.

The full run's nine failures were environmental: Swift's installed frontend could
not execute inside the sandbox; storage canonicalization and the nested C compiler
probe could not use the system temporary directory there. Swift's isolated
outside-sandbox retry passed in 52.67 s. All eight other cases passed by explicit
test name with `TEMP`/`TMP` beneath the task's ignored build directory in 16.61 s.
No runtime repair or weakened assertion was used for these failures. An earlier
ID-based retry selected different cases after exclusion and is retained but is
not credited toward the eight failure recoveries.

The final test-only C++ additions were rebuilt through the root wrapper with a
temporary CTest adapter selecting the two scalar targets. The adapter was removed
afterward. The final 37-case core/XML/package replay and independent oracle run
used those refreshed binaries. The executable/object freshness proof confirms the
generated-only unbound-prefix fallback is present and exercised. The other native
runtime/compiler sources are the same as the complete-inventory run. No CTest
case was silently excluded from the 212-case accounting; optional SDK tests that
were not configured and underlying opt-in unit cases are not implied by that total.

Python SDK commands run the maintained scripts with `--compiler` pointing to the
fresh native executable and `--build` beneath ignored `out/`. The scalar runner
also selects `--python-dependencies out/python-dependencies`; the rich runner uses
the same option and the token runner needs no extra provider option. Final logs are retained under
`out/build/pending-xml-2026-10-20/`, with lxml retained in
`out/python-dependencies/` for replay. Generated temporary Python fixtures are
removed by their runners. `.gitignore` already covers these build/dependency paths.

Unused probe sources/binaries, the temporary adapter/skill copy, task bytecode
caches and the workspace temporary directory were removed. The failed storage
tests cleaned their own system-temp directories; the task-log cleanup audit found
none remaining. Useful native/installed build outputs, oracle fixtures, lxml and
final verification logs are retained under ignored `out/`. Pre-existing editor
and C# build artifacts were preserved.

Starting Git revision: `992289c44bf6672df6309ab8e4f0e99803c54008`.
The final compiler used by all 62 Python SDK tests has SHA-256
`895919fe4e99242f2498d5e3302c246dc38f1d542786749fc6c980e5c12d8f32`;
it was checked unchanged before and after those tests. The matching native runtime
library has SHA-256 `52d4d4c2c5ebcf73733f0ec6fb03f09d00e6d7b63ee3f3755f7f69a3c9cbb8e3`.
These hashes identify local artifacts, not published releases.

Both editor 2.0.8 READMEs and the separate Visual Studio Marketplace overview were
reviewed. No schema tokens, supported output layout or navigation contract changes
in this batch require editor source changes. No editor package installation or
publication, broad OPC/3MF conformance, performance, Linux replay or universal
backend/provider qualification is claimed by this record.
