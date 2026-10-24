# Qualification branch integration, 24 October 2026

The qualification checkout's actual tip is
`c54bd7d0b968175e2fcfbdbd447ee0ec6419f3bb`. Its 15 commits after `ca04f1a`
were absent from the main checkout's stale local branch reference. Fetching
from that repository before merging brings in the qualification tools, records,
measurements, editor declaration fallback and two separately tracked findings.

## Combined contracts

Main retains compiler/runtime **2.34.0**, schema language **1.24.0**, and its
managed, compiler/backend and XML/package implementations. After resolving
duplicate Core admission helpers, `src/`, `include/`, root CMake and `AGENTS.md`
are byte-identical to pre-merge main `c6a2ef4`. The crash harness now uses short
private temporary fixtures and distinct `_oracle` report paths. Scalar-union
fixtures retain the qualification branch's independent lexical controls.

Both editor packages advance to **2.0.23**, combining the qualified absent-grammar
generated declaration fallback with main's current navigation and language
contracts. Historical 2.33.0/2.33.1 and editor 2.0.19 records keep their actual
source epochs; they do not certify every feature subsequently added to main.

The tracker retains **36 requirements: 26 closed and 10 unfinished**, including
seven active items and three with deferred database work. QA01–QA04 record
completed qualification execution within their stated profiles; QA-B01 and
QA-B02 remain open. The catalog now contains **838 records**. The two completed
managed parents remain MG07 and MG08.

## Executed merge checks

- Root Windows wrapper configuration passes against the existing C++23 Release
  build. The unchanged compiler/core suite passes, retaining its 652 cases.
- The Python Core regression passes. The updated direct32 managed-reference
  crash oracle passes in **13.14 seconds**.
- All **323 XSD tooling decisions** pass against the current compiler and
  independent lxml provider. Python rich XML passes **20 tests** and token/HTML
  XML passes **14 tests**, including both branches' retained controls.
- All **171 shared editor tests** pass, with no failures or skips. Fresh generated
  navigation passes **1,158 managed SDK checks**, **80 native bundle checks** and
  **1,943 ordinary type/field checks across eleven outputs**.
- The actual Visual Studio adapter passes **560 managed SDK checks** within its
  existing 256 MiB interpreter limit. Both 2.0.23 packages rebuild; the Visual
  Studio validator and independent archive manifest/version checks pass.

Retained package SHA-256 values:

- `serializer-vscode-2.0.23.vsix`:
  `4152276a004e3d890fbd7d1893a6e797c9f057dbb0c03304b93ea70aba97a145`.
- `serializer-visual-studio-2.0.23.vsix`:
  `80ba3aec594bf4875f84a044d637fec987c98b091c8b11ad02a60e1b79c9c622`.

## Evidence and limits

Logs and the independent XSD report remain under
`out/build/qualification-merge`; generated navigation fixtures and packages remain
under `out/extension-tests` and `out/extensions`. Serial runners are retained in
`out/build/qualification_merge_checks.ps1` and
`out/build/qualification_merge_editor_checks.ps1`.

Initial setup failures are retained: PowerShell converted a CMake stderr warning
into an error; the first crash run inherited an inaccessible user temporary path;
the first XSD run omitted the staged lxml deployment; sandbox networking initially
blocked NuGet restore. Replays use process-boundary stderr capture, a writable
ignored temporary directory, Python 3.14 with the existing `out/python-dependencies`
deployment and the unchanged locked NuGet dependencies. No warning suppression,
runtime repair or dependency-version change was introduced to obtain the passes.

This is focused merge verification. A new Linux/provider/complete SDK campaign,
interactive IDE host replay, package installation and Marketplace publication
were not performed. The qualification branch's explicitly failed inventories
and open bugs retain their original disposition.
