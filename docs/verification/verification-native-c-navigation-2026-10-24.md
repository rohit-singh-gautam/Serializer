# Native C navigation allocation repair — 24 October 2026

Both editor packages advance from **2.0.13** to **2.0.14**. Compiler/runtime
**2.30.1**, schema language **1.22.0**, generated files and wire formats are unchanged.
This is a native-resolver repair and qualification slice of `QA04`; interactive
editor-host qualification remains open.

## Reproduction and repair

Fresh compiler outputs from the C++23 migration reproduce a native Visual Studio
navigation failure. The package fixture's generated C header is 922,915 bytes.
The shared resolver tokenized every implementation expression in its bundled
runtime before finding schema declarations. The real .NET/Jint adapter rejected
the request after charging 272,497,240 bytes against its 268,435,456-byte limit.
The original failure is retained in
`out/build/cpp23-overnight/editor-native-navigation-full.log`.

Generated C now uses a direct tag index over comment/literal-masked source.
Existing typedef aliases, enum aliases and compiler-owned package identities
still use their original source ranges. Field lookup and reverse schema ownership
retain their existing behavior. Other languages retain their existing indexers.
The interpreter allocation limit, 30-second request timeout, cancellation,
inventory bounds and per-file size limit are unchanged. Runtime declarations may
still be indexed as tags; runtime implementation bodies no longer produce token
objects merely to locate schema declarations.

## Verification

Logs remain under ignored `out/build/editor-budget-overnight/`.

| Check | Result |
| --- | --- |
| VS Code/shared resolver tests | **161/161 pass**, including a large generated-C-style runtime with CRLF offsets, masked fake declarations, struct/enum tags and field selection endpoints |
| Native Visual Studio .NET Framework/Jint harness | **1,548 checks pass** using real compiler outputs, all eleven languages, package facades, unsaved/transitive includes, separate C++ ownership, generic inheritance, selection boundaries and cancellation |
| Fresh real-compiler bidirectional navigation | **45,292 checks pass** across eleven outputs and C++ naming/storage/separate-header layouts |
| Matching local VSIX packages | Both **2.0.14** packages rebuild and validate through `./make.ps1 extension` |

The native harness uses Jint **4.16.4**, .NET Framework target **4.7.2**, and the
original **256 MiB** allocation budget. Its generated fixtures are under
`out/extension-tests/generated-navigation-gR4JiG/`, emitted by compiler 2.30.1.
The complete native run passes the formerly failing package requests and its
remaining complex, generic and inherited-field checks. This is an in-process
adapter test, not a successful interactive Visual Studio or VS Code host run.
No extension installation or Marketplace publication is claimed.

Package SHA-256 hashes:

- `out/extensions/serializer-vscode-2.0.14.vsix`:
  `d9cac2afa24dbf7372b833316606b6a844e245ded0f6df460936772322defc6c`
- `out/extensions/serializer-visual-studio-2.0.14.vsix`:
  `61833ae21347fe221e198fbb040de93a0d1f54ec6daf511670b05887d33fd341`

See [navigation behavior](../editors/editor_navigation.md),
[C++23 qualification](verification-cpp23-2026-10-23.md), and the
[remaining QA04 requirement](../pending.md#qualification-and-distribution).
