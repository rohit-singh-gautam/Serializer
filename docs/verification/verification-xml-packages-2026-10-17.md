# XML and package verification — 17 October 2026

Compiler/runtime **2.3.0**, schema language **1.17.0**. These results qualify the
[implemented subset](../serialization/xml_packages.md), not the entire
[design](../design/xml_support.md). General backend gaps remain in the
[pending report](../pending.md).

## Completed checks

| Check | Result |
| --- | --- |
| Generated XML examples | All eleven native SDK builds passed. |
| Configured mapping exchanges | **121/121** producer/consumer pairs passed; exports also checked with Python ElementTree. |
| Fixed-XSD outputs | All eleven SDK builds and **121/121** exchanges passed; independent `lxml` validated all 132 output/re-export documents against `order.xsd`. C++ uses its dedicated XSD-generated executable. |
| Typed XML probes | All eleven backends passed nested/repeated records, optional values, fixed enum arrays, numeric edges, Unicode/whitespace and selected validator hooks. C++/C/Java/Kotlin/C#/Python exercise actual prepared XSD providers. |
| Generated API names | 30 authored-callable/import-shadow collision cases passed with XML-disabled compatibility controls. All 16 native XML tests passed, including direct configuration checks across eleven backends and separate C++ includes. |
| Kotlin private fields | Generated XML codecs passed an actual Kotlin SDK probe covering private scalar, optional attribute and collection members. |
| Swift imported types | Focused Swift SDK build and exchange passed with warnings treated as errors. |
| C numeric locale | Maintained C probe passed fractional XML conversion under an installed non-C numeric locale. Generated codecs do not change the process locale. |
| C floating-point boundaries | Fresh Windows and Linux outputs passed strict C11 subnormal round trips (`FLT_TRUE_MIN`/`DBL_TRUE_MIN`), smallest-normal, overflow and underflow-to-zero checks. Windows's non-C locale subcase passed; Linux's was explicitly skipped because the selected locales were not installed. |
| Compression/ZIP/OPC | Native stream tests passed, including nested wrappers and failure bounds. All 16 refreshed package tests passed, including OPC part-name identity and derived-part conflicts. Independent Python gzip, zipfile and ElementTree checks passed for the C++ package example. |
| Runtime templates | Required license prologues and bounded raw literal checks passed. |
| Editor packages | Both version **2.0.2** VSIX packages built and validated; 141 editor tests passed. Packages were not installed or published. |
| Version output | CLI reports compiler **2.3.0**, language **1.17.0**. |
| Windows native regression | Final full run: **140/141** passed; the managed Save failure passed immediate CTest rerun and five repetitions of its specific case. All XML/compression/package tests passed in the full run. |
| Linux XML build | Complete GCC compiler/runtime build passed with XML enabled and compression/managed/tests disabled; all eleven fixed-XSD outputs generated, C++ consumer compiled and ran. |
| Minimal Linux runtime | XML and every bundled compression provider disabled; public-header/link smoke passed identity/custom wrappers, stored ZIP/CRC and explicit XML/gzip/OPC unavailability. Dynamic dependencies contain only standard runtimes. |
| Minimal Linux compiler | Full compiler/runtime build passed with XML/ZLIB/Zstandard/LZ4 disabled. Maintained basic C++ consumer passed all four native protocols; explicit XSD generation failed clearly without publishing output. |

Windows uses MSVC 19.51 and libxml2 2.15.3. The optional Linux build uses WSL,
GCC 15.2 and system libxml2 2.15.2. Language dependencies are pinned by the
maintained example lockfiles; Python's prepared XSD probe uses `lxml` 6.0.2.
Go, JavaScript/TypeScript, Rust and Swift qualify an explicit provider hook;
their tests do not establish conformance of an external XSD engine.

## Reproduce

Run Windows native checks from Visual Studio Developer PowerShell:

```powershell
./make.ps1 test -CMakeArgs @(
  '-DSERIALIZER_WITH_XML=ON',
  '-DVCPKG_MANIFEST_FEATURES=xml')

python example/xml/run.py `
  --compiler out/build/make-Release/Release/serializer.exe `
  --build out/build/xml-examples
```

See the [fixed-XSD command](../../example/xml/README.md) and
[typed-probe prerequisites](../../test/resources/xml_shapes/README.md).
Missing selected SDKs/providers fail rather than silently skipping coverage.

The first full native run exposed stale compiler-client objects, missing MSVC
include environment, template prologues and an outdated XML runtime object.
Compiler-client objects were rebuilt, the Developer environment was loaded, and
the template checks were corrected. All nine previously failing core generator
tests passed after recompilation. The final compiler/XML/core refresh incorporated
late configuration-validation fixes, and all 16 native XML tests passed.

The final full Windows run passed 140 of 141 CTest entries. The single failure,
`managed_direct_test`, reported an indeterminate managed full Save in
`discard_restores_saved_tree_and_drops_unsaved_branches/0`. Immediate CTest rerun
passed 67 cases in that executable, with one intentional storage-mode skip;
five further repetitions of the specific failing case passed. Its intermittent
cause is not established. This is not reported as a clean single-pass full suite.

Performance measurements, full Linux CTest, full XML-disabled CTest,
advanced XSD/vocabulary coverage, general compression/package backend parity and
managed document/sidecar recovery remain unqualified or unimplemented as listed
in the pending report. Useful final build/test output is retained under ignored
`out/`; scratch source probes are removed once replaced by maintained tests.
