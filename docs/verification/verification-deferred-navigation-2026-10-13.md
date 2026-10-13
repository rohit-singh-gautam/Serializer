# Deferred function navigation qualification — 13 October 2026

This record covers editor source packages **1.1.47**. Compiler/runtime **1.22.0**
and schema language **1.17.0** are unchanged. The earlier
[array and callable qualification](verification-array-callable-api-2026-10-13.md)
records the completed **1.1.46** package checks; those results do not establish
qualification for these changed editor packages.

## Behavior under qualification

Go to Definition on a typed external schema function finds available authored
C++ out-of-class definitions by qualified owner and overload signature, including
templated owners. Lookup is read-only and bounded, uses live buffers, and does
not require generation or CMake activation. Native blocks remain opaque; unknown
signature distinctions preserve matching candidates as choices. See
[navigation limits](../editors/editor_navigation.md#deferred-function-definitions-1147).

## Verification status

| Check | Result |
| --- | --- |
| Shared VS Code unit/provider suite | Passed: **138/138**, no skips |
| Fresh compiler output navigation | Passed: **37,967** bidirectional type/field checks across all eleven outputs, nine C++ profiles and three Java profiles |
| Visual Studio .NET/Jint resolver | Passed: **1,116** source/output checks, including deferred qualified owners, overloads, borrowed arrays, independent template inputs and live unsaved renames |
| Installed Visual Studio grammar assets | Passed: **424** syntax/boundary checks across all eleven embedded language tags and **48** grammar scopes |
| Isolated native VS Code navigation host | Passed: standard `editor.action.revealDefinition` to the authored templated body, every function-name cursor boundary, forward/reversed selections and live unsaved Unicode C++ source |
| Canonical VS Code VSIX package | Passed: root `./make.ps1 extension`, version/identity, current compiled resolver, grammar, configuration, snippets and packaged documentation |
| Canonical Visual Studio VSIX package | Passed: root `./make.ps1 extension` and maintained package validator, version/identity, navigation registration and canonical assets |
| Installation into the normal IDE profiles | VS Code **1.1.47** installed and CLI-verified; reload the running user window to activate it. Visual Studio package not installed |

The isolated VS Code host also retained its real Git staged-schema, complex model
and TypeScript navigation checks. Deferred definition lookup passed without
generated output or a CMake prerequisite. An initial host assertion incorrectly
required selected destination text; native F12 moves the caret without selecting.
The test now checks the identifier at the destination caret, and the full host
run passed. The final native host rerun followed canonical packaging and the
last basename/constraint regression fixes, and passed. The original failed-run
log remains available with both successful logs.

Final regression coverage also preserves authored `model.hpp`/`model.cpp` files
whose basename matches a possible generated output; exclusion requires actual
generated-file ownership. Constrained C++ prototypes with requires-expression
braces are not mistaken for definitions, while actual constrained bodies,
trailing return types and globally qualified owners retain their destinations.
Type identifiers containing `const` remain intact during token normalization.

No current native Visual Studio UI host run, non-Windows/remote host run, or C++
language-service equivalence is claimed. The .NET/Jint test validates the resolver
in the actual interpreter, and grammar checks validate installed tokenizer assets;
these checks are distinct from interactive Visual Studio commands.

## Reproduce and retained evidence

Use `npm test`, `npm run test:navigation:generated`, and `npm run test:navigation`
in `editors/vscode`. Set `SERIALIZER_COMPILER` to the current compiler for fresh
outputs and `VSCODE_EXECUTABLE_PATH` to reuse the installed host. Use the maintained
Visual Studio navigation test project for its .NET/Jint resolver and installed
grammar tests for tokenizer coverage. The root `./make.ps1 extension` wrapper on
Windows packages both extensions; both canonical package checks passed above.

Retained verification logs are under ignored `out/build/schema-adoption/`:

- `deferred-navigation-vscode-units.log`
- `deferred-navigation-all-language.log`
- `deferred-navigation-vscode-host-final.log`, the earlier successful `deferred-navigation-vscode-host-qualified.log`, and the original failed `deferred-navigation-vscode-host.log`
- `deferred-navigation-visual-studio-navigation-final.log`
- `deferred-navigation-visual-studio-grammar.log`
- `deferred-navigation-editor-packages.log` and `deferred-navigation-editor-package-audit.json`
- `deferred-navigation-vscode-install.log`

Useful generated fixtures and build output remain in ignored build directories.
The source changes are editor-only; existing compiler/runtime and wire-format
qualification remains in the named earlier records rather than being repeated
or attributed to this editor regression run.
