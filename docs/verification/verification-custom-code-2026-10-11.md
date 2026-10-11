# Custom C++ example and embedded source highlighting

Compiler/runtime **1.15.1**, schema language **1.10.0**, and both editor extensions
**1.1.38** are the release metadata for this change, based on worktree commit
`83b86aa`. The new example uses the compatible language **1.6.0** native behavior
contract. Compiler grammar, generated public APIs, and wire formats are unchanged.

## Capability selection

Used: ordinary schema-owned values, stable field IDs, literal defaults, C++
preambles and access-qualified declarations, inline readonly/edit bodies,
application-owned external definitions, preserved C++ names, CMake generation,
all four native codecs, and exact-message decoding with the default resource
limits. The example validates before mutation so overflow preserves its value.

Not applicable: managed identity, transactions, history, journals, collaboration,
payload migration, compression, and Protobuf. The requested example demonstrates
custom behavior on an ordinary value rather than an application state store.

Unavailable: IntelliSense, semantic diagnostics, formatting, and native symbol
navigation inside schema source blocks. The editor change reuses TextMate grammar
providers and assigns embedded VS Code language identities. It does not connect
native language services. C/Go class hooks remain rejected, and TypeScript output
remains declaration-only.

## Executed native checks

`./make.ps1 configure` and the real CMake target build generated and compiled
`serializer_custom_code_example` with MSVC `/W4 /WX`. Its output matches the
[example README](../../example/cpp/custom_code/README.md). It verifies durable fields
and custom methods after exact roundtrips through JSON and all three binary
protocols, rejects truncated/trailing messages, and preserves quantity on overflow.

`./make.ps1 test -Jobs 4` built the enabled targets and passed 109 of 111 tests.
The two compile-failure tests initially lacked MSVC standard-header include paths.
Loading `VsDevCmd.bat -arch=amd64` and running CTest's `--rerun-failed` passed both,
so all 111 tests passed across those runs. The package-version test checks the
previous patch and minor releases in the same major, exact current discovery, and
rejection of future/different-major releases. `serializer --version` reports
compiler **1.15.1** and schema language **1.10.0**.

## Executed editor checks

| Check | Result |
| --- | --- |
| VS Code unit suite | 119 tests passed |
| Fresh generated-output navigation | 34,105 checks passed across all eleven outputs |
| Installed VS Code grammar assets | 308 checks passed using 85 registered grammar scopes |
| Installed Visual Studio 2026 grammar assets | 308 checks passed using 48 registered grammar scopes |
| Editor packaging | Both 1.1.38 VSIX packages rebuilt through `./make.ps1 extension` |
| Package inspection | Stable IDs, equal versions, exact canonical grammar bytes, and all eleven VS Code language mappings verified |

Embedded checks include actual C++/Java provider scopes, method/class/preamble
contexts, split and commented headers, language-named field defaults, nested
braces, raw/multiline literals, C# interpolation, and missing-provider fallback.
Ordinary Serializer syntax has one shared grammar rule collection. The Rust
lifetime fixture is tokenizer resilience coverage; it does not claim that the
compiler's native scanner accepts lifetime syntax.

## Verification scope and artifacts

Native validation used the installed Windows MSVC toolchain. No new full Linux,
macOS, or ARM build is claimed. Grammar asset checks use the locked
TextMate/Oniguruma tokenizer; native editor UI colors and embedded language service
behavior were not visually verified. See the
[embedded highlighting contract](../editors/editor_extension.md#embedded-source-highlighting)
for grammar-provider prerequisites and fallback behavior.

Useful build outputs, test fixtures, final verification logs, and editor packages
remain under ignored `out/`. The native logs are `out/custom-code-native-tests.log`
and `out/custom-code-native-retest.log`. Temporary compiler probes and the MSVC
retest helper were removed. Existing `.gitignore` patterns cover all new generated
output. New source schemas, example files, and tests are outside ignored directories.
Editor logs are `out/embedded-extension-unit-tests.log`,
`out/embedded-generated-navigation.log`, `out/embedded-vscode-assets.log`,
`out/embedded-visual-studio-assets.log`, and `out/custom-code-extension-build.log`.
Both VSIX files are under `out/extensions/`; they were built and inspected without
installing or publishing them.
