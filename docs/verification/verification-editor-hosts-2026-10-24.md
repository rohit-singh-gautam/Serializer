# Current editor-host qualification — 24 October 2026

[Pending requirements](../pending.md#qualification-and-distribution) · [Navigation guide](../editors/editor_navigation.md) · [Verification index](README.md)

Compiler/runtime **2.33.0**, schema language **1.23.0**, and matching editor packages **2.0.19**. The source baseline is `ca04f1af33af9927110eb5ca67bf9e7bf82c478c` plus the focused editor changes. Logs and fixtures remain under ignored `out/qualification-2026-10-24/` and `out/extension-tests/`.

## QA04: actual editor hosts and matching distribution payloads

**Complete within the current Windows host profile.** Compiler/runtime and
schema language are unchanged. This closes QA04 rather than a deferred compiler,
storage or package requirement.

Real VS Code host qualification exposed a missing declaration-provider
registration for generated Kotlin opened as plain text without an optional
grammar. Version 2.0.19 adds a filename-limited fallback for marked generated
files, uses the existing output-extension inventory, and returns before discovery
for unmarked plain-text files. Workspace schema discovery, opening a schema or
invoking the Serializer declaration command activates this support. Caller
expressions still require their native language service.

The maintained [generated-host launcher](../../editors/vscode/test/run_generated_host.cjs)
generates all eleven outputs with the current compiler before starting a fresh
VS Code profile/workspace. Actual native Go to Declaration actions check both
selection directions, qualified included types, cursor endpoints and exact live
source locations after unsaved UTF-16 edits. Ignored build outputs remain
discoverable. The successful replay passes **92 checks**; the original real-host
failure is retained in `vscode-generated-host-final.log`, and the repaired run in
`vscode-generated-host-fixed.log`. An initial test assumed native commands select
the destination text; the corrected assertion checks the exact destination word
and offset, allowing the native command's empty selection.

The unit/provider suite passes **166/166** with no skips, including marked Kotlin
plain-text navigation, the explicit declaration command, and untouched authored
plain-text controls. The recovered storage-branch tests are preserved behind the
presence of that branch's `schema_indexes.serializer` fixture; this branch does
not supply or claim the language 1.24 index implementation.

The first registry restore failed because a locked `inherits` tarball was
unavailable. Restoring the unchanged lock from the existing local npm cache
succeeded offline. Both attempts remain recorded. One navigation retry overlapped
restore and lacked `tsc`; the successful retry uses the completed dependency tree.
No Marketplace publication is authorized or claimed.

| Current check | Result | Retained log |
| --- | --- | --- |
| Unit/provider checks | 166/166, no skips | `editor-unit-final.log` |
| Real compiler mapping | 50,713 bidirectional type/field checks | `generated-navigation.log` |
| Real VS Code generated-output host | 92 checks across eleven outputs | `vscode-generated-host-fixed.log` |
| Existing VS Code navigation host | Pass, including actual Git snapshot/type-definition handling | `vscode-navigation-restored.log` |
| Real CMake/C++ project host | 117 native caller/alias/selection/ownership checks | `vscode-project.log` |
| .NET Framework/Jint adapter | 1,548 complete source/output checks | `native-adapter-final.log` |
| Installed Visual Studio generated-output host | 24 native checks across eleven outputs, transitive includes and unsaved buffers | `visual-studio-generated-host-final.log` |
| Installed Visual Studio existing command host | 14 declaration/definition/type-definition and reverse-navigation commands | `visual-studio-current-host-final.log` |
| Matching package build/validation | Both 2.0.19 packages pass | `editor-packages.log` |

The first Visual Studio startup control ran the older installed 1.1.12 package
and is not current-package evidence. The global VSIX installer subsequently
returned `2004`, reporting active MSVC/Ninja processes. Those builds were left
running. With no IDE open, the installed SDK's supported `Build` deployment
pipeline deployed and enabled the current per-user extension; its root suffix
was explicitly empty and its instance ID selected explicitly. The required
`devenv /UpdateConfiguration` refresh then completed with exit zero. Current
host runs verify the package version and installed assembly hash before starting.
The installed and locally built assembly SHA-256 is
`cfcd1d2dea3e5b867541e52774bc05d06aa57cfcb1756a8bfb0f54299015405c`.
The tests terminate only their own hidden automation process.

Native-host fixture controls are retained: waiting for editor command filters,
refreshing configuration, selecting a generated diagnostic literal instead of a
declaration, and using a changing active-document selection. The final harness
anchors actual declarations and captures the intended document's selection;
it asserts exact destination text and live line after an unsaved edit. The
initial .NET inherited-fixture path typo is separate from its complete passing
replay. Interpreter resource and cancellation limits were not raised.

Package SHA-256 hashes:

- VS Code: `54c6c03f4561012f3e1ea7e4349b6dcabb682f1c6cbb34d8fe45aabcf06922c0`.
- Visual Studio: `f4c554fe59c53c38291cb2cca7e6832873c7c5b6b5905e0e5689df756ff63335`.

These checks establish the named local Windows hosts and package payloads.
Optional third-party caller language services beyond the exercised native C++
and built-in TypeScript services, remote/editor-OS profiles, macOS and ARM require
their own environments. Direct generated declarations need no such caller
service. Local builds, deployment and listing-file synchronization do not update
either Marketplace listing.
