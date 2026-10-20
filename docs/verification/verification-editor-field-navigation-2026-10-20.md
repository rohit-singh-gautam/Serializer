# Generated inherited-field navigation, 20 October 2026

[Shared field lookup](../../editors/vscode/src/navigator.ts) ·
[Generated declaration reader](../../editors/vscode/src/generated_navigation.ts) ·
[Native regression](../../editors/visual_studio/test/navigation_test.cs) ·
[Editor navigation](../editors/editor_navigation.md)

This record qualifies the selected **G08** editor repair for synchronized local
extension release **2.0.8**. The actual compiler is Serializer **2.14.0**, schema
language **1.20.0**, SHA-256
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`.
The host is Windows 11 with the installed .NET Framework test host and Jint
**4.16.4**. The native host retains its original **256 MiB allocation limit and
30-second timeout per navigation request**.

## Reproduced failure and bounded repair

A generated inherited-field lookup returned definitions in all eleven output
languages. The resolver previously masked an entire generated output again for
each occurrence of the field in declarations, constructors and codec methods.
The actual native host exhausted its allocation limit on the first inherited
field lookup, reporting **268,435,584 bytes allocated** against a **268,435,456-byte**
limit in a fresh process. The failing schema token was `inherited_count`, at
offset **135** after a live unsaved comment moved its declaration. All eleven
language-filtered requests passed separately, establishing that the complete
request's repeated work caused the failure rather than cumulative test state.

The repaired lookup prepares an offset-preserving lexical mask once per output
within the current immutable request. Every candidate uses the same owner and
field-checking logic. There is no global cache, retained state across files or
resource-limit increase. Cancellation, UTF-16 source offsets, CR/LF boundaries,
live schema positions and reverse-navigation endpoints remain intact.

The maintained test now offers a fresh-process `--inherited-only` mode. With the
preserved old bundle, it fails with `MemoryLimitExceededException`. With the new
bundle, all **12 checks** pass, including all eleven output languages and both
reverse selection endpoints. The same checks remain in the complete native test.
The exact before/after bundle SHA-256 values are respectively
`5be571ec87db78bd6ef3d4f40477b7852bbf29da597e22d86ed0741e52c3f356`
and `fe01d8312be12109e9adea5115527223c4ff3b4c928803192ccc183ce0545503`.

## Complete selected verification

- **154/154** shared Node unit tests pass, with zero skips.
- **43,422** fresh compiler-generated Node navigation checks pass across all
  eleven output languages in **213.997 seconds**.
- **1,437** actual native .NET/Jint source/output checks pass, including its
  **42 package-facade checks**, under the unchanged interpreter limits.
- The separate fresh-process inherited-field mode passes **12/12** checks.
- Independent source review found no concrete offset, owner-scope, immutable
  reader or full-test-path regression.

The suites retain declaration/definition and reverse lookup, qualified names,
cursor endpoints, transitive includes, unsaved edits, generic inheritance,
separate C++ header ownership, naming profiles and cancellation. Earlier successful
2.0.7 results remain evidence for their earlier generated profiles; the newly
reproduced failure is retained rather than relabeled as a successful run.

Both extension IDs are preserved. The npm lockfile's extension root versions
are synchronized at **2.0.8**; nine signing-tool dependency version fields are
restored to their actual unchanged **2.0.6** tarballs. No signing-tool upgrade is
part of this repair.

## Retained evidence and limits

Ignored evidence under
`out/build/backlog-2026-10-20/current-regression-compression-final/` includes
`editor-208-unit.log`, `editor-208-navigation.log`,
`editor-208-navigation-verification.json`, `editor-208-jint.log` and
`jint-diagnostic/control-verification.json`. The diagnostic directory retains
the original complete failure, fresh-request failure, language isolation and
the maintained before/after control logs. The immutable generated fixture sets
are retained under ignored `out/extension-tests/`.

This record establishes the shared resolver and actual native interpreter
behavior for the selected generated profiles. It does not repeat installed
Visual Studio UI automation, qualify third-party language services or establish
support on another editor host or platform. Local package builds do not install
extensions or update Marketplace listings.

## Validated local packages

The real `./make.ps1 extension` pipeline builds and validates both synchronized
packages. Both extension identities remain unchanged, all required assets are
present, and the exact native assembly extracted from the Visual Studio VSIX
passes the **12-check fresh request and complete 1,437-check suite** under the
original limits. Its assembly SHA-256 is
`1197a9f67b45d63260ee90a13084ac046e9d1a7210b8a2f8ec0d078c918e8553`.

| Package | Bytes | SHA-256 |
| --- | ---: | --- |
| `serializer-vscode-2.0.8.vsix` | 87,526 | `cc1e2c444448651aa619f98e1087dc35e587347e5f4eb77cdbcdf310bbc95246` |
| `serializer-visual-studio-2.0.8.vsix` | 1,171,520 | `8cd578d8a3d87b4de651e724ad72c5e4abaa42569d8b5ffdb3b3f1d5a13a7df8` |

The packaged native resolver resource SHA-256 is
`f8f1657b3c819d5d8baca5cd8e9d47714f755bccd9db033ee9f30196bc70857c`.
MSBuild runs the bundler from its project directory, which changes only diagnostic
source-path comments relative to the earlier `fe01d831` test bundle. Their
non-comment bytes are identical; the exact packaged resource is independently
replayed rather than inferred from that comparison.

The **63-file** maintained editor/guide inventory has aggregate SHA-256
`90eec4bf69007001f4c25498dc40e568d61854884e1bc09720bf05c8cc2a573c`.
`editor-208-packages/verification.json` retains exact package manifests, resource
identity and source hashes; `native-tests/verification.json` identifies the tested
packaged assembly. These artifacts remain under the ignored evidence directory
above. No package is installed or published.
