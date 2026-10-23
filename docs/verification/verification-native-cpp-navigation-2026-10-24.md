# Native generated C++ navigation allocation repair — 24 October 2026

Both editor packages advance to **2.0.16**. Compiler/runtime **2.31.0** and schema
language **1.23.0** are unchanged. This is a resolver allocation repair under
`QA04`; interactive editor-host qualification remains separate.

The fresh separate-values named-reference C++ fixture is **300,175 bytes**.
The 2.0.15 adapter exceeds its **268,435,456-byte** allocation budget while
resolving fields, charging 268,564,452 bytes. The interpreter budget, 30-second
timeout, cancellation, inventory bounds and file-size limit remain unchanged.

## Implemented behavior

Banner-marked generated C++ uses a namespace-aware declaration index rather than
creating tokens for implementation expressions. Template parameters, forward
declarations, scoped enums and source offsets preserve their existing meaning.
Authored C++ retains its existing indexer.

Field lookup matches only complete candidate spellings. One prepared brace index
supplies matching block ends and binary-search depth lookup; it replaces repeated
character walks through every earlier class body for each occurrence. Closed
owners and method-local references remain excluded. Lexical masks still preserve
UTF-16 offsets, comments/literals and each backend's field rules.

## Verification

| Check | Result |
| --- | --- |
| Shared/editor tests | **163/163 pass**, including large C++ declaration/namespace/template/enum offsets and unchanged authored-source behavior |
| Fresh compiler navigation | **50,713 bidirectional checks pass**, all eleven ordinary outputs and all C++ naming/storage layouts, including named self references |
| Native named-reference adapter | **52 checks pass**, direct/separate values, exact named declarations and copied field provenance |
| Complete native adapter harness | **1,548 checks pass**, all eleven outputs, package facades, includes, unsaved edits, selection boundaries and cancellation |
| Matching 2.0.16 packages | Both packages rebuild and validate through `./make.ps1 extension`; no install/publication |

The real .NET Framework 4.7.2/Jint 4.16.4 adapter passes **1,600 checks** under the
original 256 MiB allocation budget. This is an in-process resolver qualification;
it does not establish an interactive IDE host run. No installation or Marketplace
publication is claimed.

Logs, failed controls and reproducible drivers remain under ignored
`out/build/recursive-reference-overnight/`. The retained initial failure is
`editor-native-before-cpp-index.log`; `editor-native-navigation.log` records the
successful named-reference and complete adapter replays. Fresh compiler fixtures
are under `out/extension-tests/generated-navigation-Phjaxa/`.

See [named-reference qualification](verification-named-managed-references-2026-10-24.md),
[navigation behavior](../editors/editor_navigation.md), and the earlier
[generated C repair](verification-native-c-navigation-2026-10-24.md).

Package SHA-256 hashes:

- `serializer-vscode-2.0.16.vsix`: `5b56ad43af9c370f3b31b36fd2299b109baaddaed754bcd44c4f27b59aede584`
- `serializer-visual-studio-2.0.16.vsix`: `323d6ae759c248a42d05941ce260395c4c740376c47a65a3a88ed6ecab53da7e`
