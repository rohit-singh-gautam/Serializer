# Restricted HTML profile qualification: 24 October 2026

Compiler/runtime **2.34.0** completes **XML-B24** for the
[closed 30-tag fragment contract](../serialization/xml_advanced_contracts.md#html-is-a-separate-fragment-adapter).
All eleven outputs add `sub` and `sup` as explicitly balanced phrasing containers.
They use the existing global attributes and byte/event/depth/text bounds. Block
children, nonvoid slash syntax, missing closes and event-handler attributes reject.
There is no schema-language change for this adapter.

## Executed checks

Windows 11 build 26300, x64 MSVC **19.51.36231**, C++23/C11. The replay compiler
SHA-256 is `24a4f75a6dc48d0f2cd945a6a70707a34f0aa4c543d5c7a6dd64df0a99a0de0f`.
This snapshot precedes the subsequent unrelated Python Core tree-routing repair;
the final Python token/Core regression suites also pass after that repair.

| Output | Actual executed result |
| --- | --- |
| C++ | Six native HTML tests and seven shared fixtures pass; independent standard-library HTMLParser and lxml HTML trees agree on the accepted fragments. |
| Python | Shared seven valid and 29 invalid fragments pass token/tree checks against both independent HTML readers, with all four native protocols preserved. The final combined XML token/fallback suite passes 14 tests. |
| JavaScript / TypeScript | Four tests pass with seven shared fixtures, generated JavaScript execution, native byte parity and strict TypeScript API compilation. Locked TypeScript **5.9.3**, Node **22.23.1**. Generated source hashes are retained before temporary module cleanup. |
| Java / C# / Go | Each passes 39 decisions, including accepted/rejected structure and native controls. Java **26**, .NET SDK **10.0.402** targeting net8.0, Go **1.26.4**. |
| Kotlin / Swift | Each passes 36 decisions with the same fragment contract. Kotlin **2.3.10** on JDK **17.0.18+8**, Swift **6.4**. The maintained runner invokes the installed Kotlin compiler JAR when selected. |
| Rust / C | Each passes 36 decisions. Rust/Cargo **1.99.0**, MSVC C11. |

Python is **3.14.3**; independent lxml is **6.1.3** using LibXml2 **2.11.9**.
These are actual SDK runs, not generation-only checks. C++ native fixtures and
the portable/mobile runners retain their own source and protocol controls.

Evidence remains under ignored `out/build`: `html-profile-cpp-oracles.log`,
`html-profile-python-oracles.log`, `html-profile-java-csharp-go.log`,
`html-profile-kotlin-swift.log`, `html-profile-rust-c.log`, and
`html-profile/javascript-typescript/verification.json` with the generated hashes.
`html-profile/sdk-summary.json` records successful SDK group exit codes.
Reproduce with the maintained `test/html_interop_test.py`,
`test/javascript_html_test.py`, `test/native_html_test.py`,
`test/mobile_html_test.py`, `test/portable_html_test.py` and
`test/python_xml_token_test.py` runners and the built native HTML test target;
their help describes SDK/dependency arguments.

## Scope

The vocabulary is finite and explicitly balanced. Arbitrary HTML, browser recovery,
scripts/styles, foreign content and unrestricted mixed-content XHTML remain
unsupported. This adapter performs no URL trust decision and is not a sanitizer.
XHTML uses XML parsing separately. No Linux replay or interactive editor-host
qualification is claimed by this record. The [resolution ledger](../project/xml_package_backlog_resolution.md#xml-b24)
records the closure decision and links the other XML/package requirements.
