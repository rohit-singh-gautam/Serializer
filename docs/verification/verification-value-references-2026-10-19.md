# Inline value reference verification — 19 October 2026

This record covers the compiler/runtime **2.6.0**, schema language **1.19.0**
working tree and editor packages **2.0.4**. It follows the separate
[durable address and translator verification](verification-references-translators-2026-10-18.md).
The [value reference contract](../schema/value_references.md) and
[maintained examples](../../example/references/README.md) describe the supported API.

## Completed SDK checks

The maintained examples generated and ran on Windows x64 using the installed SDKs
for all eleven outputs: C++, C, Rust, Java, Kotlin, C#, Go, Swift, Python,
JavaScript and TypeScript. Each consumer compared referenced and scoped translator
output with an ordinary owning consumer under JSON, binary-none, binary-integer
and binary-string protocols. These comparisons establish identical bytes within
each backend; they do not assert identical floating-point JSON spelling across
languages.

The examples check original payload and collection-buffer identity where the
language exposes it, independent shared payload ownership, unbound access,
applicable weak/scope invalidation and rejection of reference-field input. Ordinary
owning consumers decode the resulting values independently.

C++ checks its borrowed wrapper against `sizeof(const payload*)` and its shared
wrapper against `sizeof(std::shared_ptr<const payload>)`. C uses a raw const
payload pointer for borrowed mode. Rust uses `Option<&'a Payload>` and the real
Rust compiler rejected an escaped local owner with **E0597**. A separate Rust
consumer containing nested records, arrays and optional borrowed records compiled
with warnings denied and ran successfully. Shared/weak modes remain separate,
optional lifetime APIs.

The native Go module also passed `go generate .`, `go build -p 1 -o build/ .`
and `go test -p 1 ./...` from its maintained source directory.

## Native regression and generated compatibility

All **140 CTest entries** are qualified across the full run and focused reruns.
The initial run passed 133 entries. Six entries failed with Windows access-denied
errors in temporary-directory or MSBuild operations; all six passed after setting
`TEMP`/`TMP` to an ignored workspace directory, without source changes or elevated
execution. The remaining core entry exposed two new fixture schemas with invalid
default syntax; those fixtures now use the supported initializer syntax.

The final compiler/library and core test executable were rebuilt through CMake.
The refreshed core entry passed **590/590 tests**, including the new Rust
construction/conversion diagnostic cases. Setting `SERIALIZER_TEST_CLANG_FORMAT`
to the configured Visual Studio clang-format also exercised the formatting case
that the initial run skipped. Final CLI output reports compiler **2.6.0** and
schema language **1.19.0**.

After the final diagnostic-only Rust change, regeneration matched **36/36 files**
against SDK-qualified snapshots: 12 durable-reference outputs, 12 translator
outputs and 12 current example outputs across the eleven languages, including
TypeScript's JavaScript runtime copy. The final compiler SHA-256 is
`afc53c7825a57b86419acc2c49d699b0d343e82101b671984a5fe837f80fb2ad`.
No valid generated output changed during this final correction.

## Editor checks and packages

The current editor sources passed **147 Node checks**, **478 TextMate grammar
checks** and **1,293 Visual Studio .NET/Jint navigation checks**, including inline
owner-field and direct payload references. No native IDE/UI host was launched.
Both version **2.0.4** packages were built with the standard Windows
extension pipeline. The unchanged Visual Studio package validator passed after
explicitly importing the host's `Microsoft.PowerShell.Utility` module. Both
archives passed ZIP integrity, version/identity and current asset checks.

| Package | Bytes | SHA-256 |
| --- | ---: | --- |
| `serializer-vscode-2.0.4.vsix` | 84,523 | `171cde81984ed3af46f7bf77e6b080e96697f78f92a952d43968524b221ad102` |
| `serializer-visual-studio-2.0.4.vsix` | 1,170,229 | `3e645f6370b4a2280aba7d06ec1bbd3a642bc166355a2becc95b28c1aa967dcf` |

## Allocation and storage evidence

A Windows x64 C++ probe compiled with MSVC `/O2 /MD` serialized a nonempty
65,536-byte source payload into a preallocated bounded stream. It checked exact
source/buffer aliases and output equality before measurement. For each of the
four protocols it performed 256 real-buffer encodes through each of five paths:
ordinary owning storage, borrowed reference, aliasing shared reference,
independent shared payload and scoped translator.

All twenty measured cases reported **zero global `new`/`new[]` calls and requested
bytes**. A positive control that explicitly copied the payload detected two
allocations, confirming the interception was active. Setup, binding, warmup,
verification copies and reporting were outside the measured regions. This probe
does not intercept direct C allocation or allocations internal to external DLLs.
Output buffer growth, binding an independently allocated shared value and other
schemas can have their normal allocation costs.

The measured borrowed handle occupied **8 bytes**, shared handle **16 bytes** and
payload record **48 bytes** on this host. Encoded sizes matched the owning
baseline: JSON 73,068 bytes; binary-none 65,540; binary-integer 65,545;
binary-string 65,557. No throughput or relative speed claim is made while other
builds and applications are active.

## Commands and retained evidence

```powershell
./make.ps1 test -BuildDirectory out/build/ReleaseWindows -Configuration RelWithDebInfo -Jobs 3
python -B example/references/run.py --compiler out/build/ReleaseWindows/serializer.exe --language all
./make.ps1 extension
```

SDK qualification was split into sequential language subsets. The native build
uses its existing XML-disabled, managed/compression-enabled configuration. No new
Linux, sanitizer, alternate architecture or optional XML reference qualification
is claimed. Unsupported profiles reject the feature as documented.

Useful ignored evidence is retained under `out/`:

- `value-reference-sdk-qualified.log` and `value-reference-sdk-native-four.log`;
- `value-reference-borrow-lifetimes.log` and its Rust escape diagnostic;
- `value-reference-performance/results.log` and `build.log`;
- `value-reference-native-final.log`;
- `value-reference-native-environment-refresh.log`, `value-reference-core-final-build.log`
  and `value-reference-core-final-tests.log`;
- `value-reference-editor-unit.log`, `value-reference-vs-grammar.log` and
  `value-reference-extension-packaging.log`, plus both package-validation logs;
- `value-reference-vs-navigation-build.log` and `value-reference-vs-navigation.log`;
- `value-reference-legacy-check.log` and its generated-output hash summary.
- `value-reference-native-four-hashes.log`, `value-reference-rust-diagnostics.log`
  and `value-reference-go-native-final.log`.

Temporary allocation and lifetime probe sources and executables were removed
after retaining their results. Build outputs and final example SDK consumers are
intentionally retained for review. No Git commit, extension installation or
Marketplace publication was performed.
