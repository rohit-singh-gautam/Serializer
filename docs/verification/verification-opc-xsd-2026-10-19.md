# OPC and XSD implementation checks, 2026-10-19

Working source: compiler/runtime **2.7.0**, schema language **1.20.0**. This is a
qualification record for the checked slices, not a declaration of complete OPC,
3MF or cross-language package conformance. Their remaining scope is tracked separately.

Windows x64 builds use MSVC 19.51, C++20, Ninja and `/W4 /WX`; XML is enabled with
libxml2 2.15.3 and DEFLATE uses zlib 1.3.2. Build/log/report output is retained under
ignored `out/build/opc-implementation` and `out/build/xsd-examples`.

| Check | Executed result |
| --- | --- |
| Full Windows native regression | The complete `make.ps1 test` wrapper exits 0: 691 build steps and 169/169 CTests pass in 236.76 seconds with XML, managed runtime and all optional compression providers enabled. |
| XML authoring parser | 9 cases pass, including per-file version gates, included files, sparse defaults, lexical flags and package role validation. |
| XSD importer and binding | 7 importer cases and 11 existing binding cases pass; unsupported combinations reject. |
| Real importer CLI | Import to relative output, dependency tracking, repeated identical import, authored-source protection and manual binding pass. |
| All-language XSD consumers | Both automatic import and manually authored binding pass all eleven SDKs: 22 builds, 121 producer/consumer exchanges per route and 22 cross-route exchanges, 264 total. |
| Rich C++ generated XML | 3 cases pass for facets/defaults, exact uint64 bounds, normalization, QName values, optional/fixed typed lists, choices and positioned foreign wildcard content. |
| XML runtime context | 10 cases pass for cancellation, deadlines, cumulative charges, foreign retention, strict core vocabulary and lexical checks. |
| C++ generated package | 3 cases pass for typed round trip, dynamic binary attachments, occurrence/source checks, shared limits and deadlines. |
| Independent package interchange | Existing Python ZIP/OPC producer/consumer interoperability passes. |
| Existing XML | Existing generated/runtime XML target passes. |
| Existing package regression | All 27 package runtime cases pass after callback-budget assertions account for shared XML infrastructure charges. |
| Referenced C++ XML | Generated inline shared/borrowed value output and scoped projection tests pass; durable address XML remains explicitly rejected. |
| Ordinary native generated code | SHA-256 and byte-size comparison against captured 2.6.0 outputs is identical in all eleven languages for the unchanged ordinary-record fixture. |
| C++ managed-reference code generation | 7 cases pass for stable class tags, collision diagnostics, forward/cyclic tokens, output/default restrictions and bounded recursive view/value graph detection. Real CLI generation of the existing views and generics fixtures passes. |
| C++ persistent-ID migration | Direct uint32 and separated uint64 profiles pass 7 cases each, plus the helper-generated executable/file workflow: 3/3 CTests. All five supported history/label shapes, authority lease preservation, all nine raw binary codec pairs, ID boundaries, typed token overrides, an ordinary unmanaged root and stale managed-header rejection are covered. Independent fixed-wire fixtures are used; a separately built 2.6 compiler artifact is not qualified. |
| C++ typed-reference consumers | 8 cases pass in each of 8 generated profiles: direct/separated uint32/uint64 with compact/fixed ID encoding. Checks include complete-candidate cycles, foreign/dangling/class rejection, staged adoption, identity-preserving import and boundary encodings. |
| Checked draft preview | All 4 generated registry profiles pass: canceled IDs stay burned, copied tokens cannot alias later objects, failed/moved transactions remain bounded, acceptance rejects validator/publication reentry, and cancel/accept journal recovery in both storage modes plus undo/redo preserve identity. |
| Strict collaboration | 9 strict-identity cases and 94 collaboration cases pass, including offline disjoint native IDs, exact acceptance identities, journal recovery/nonreuse, exhausted capacity, forged-range rejection and same-sequence lock conflict retention. |
| Installed managed migration consumer | The standalone example builds from installed `find_package(Serializer 2.7)` with `/W4 /WX /EHsc`; installed compiler version and tool help pass. Actual fixture conversions compare exactly: managed 329 to 317 bytes; raw positional to string-key 38 to 114 bytes. These are fixture results, not performance or general compression claims. |
| Minimal installed package | Managed runtime, XML and all optional compression backends OFF: compiler/runtime build and install pass; 42 installed public headers compile together under `/W4 /WX`, and the provider-exclusion executable passes with only `none` available. Installed compiler reports 2.7.0 and schema 1.20.0; managed-dependent headers are excluded as intended. |
| Editors | VS Code 152 tests, shared grammar 524 checks across 48 installed scopes, generated navigation 43,010 checks across eleven ordinary output languages, and Visual Studio shared navigation 1,395 checks in the native .NET interpreter pass for XML/package and managed-reference authoring. Both 2.0.5 VSIX packages build and validate. No installation/publication or current editor-host UI run was performed. |

The XSD runner uses the same independently parsed namespaced XML fixture for
both workflows. Actual SDK clients check their generated round trips and resource
failures. Python ElementTree checks expanded names, values and ordering. This
does not qualify arbitrary schemas or imply that a general external XSD validator
ran on every backend. Kotlin uses the installed native compiler wrapper.

The final wrapper ran from an MSVC x64 Developer PowerShell using the existing
Release/Ninja build cache:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File make.ps1 test -BuildDirectory out/build/opc-implementation -Jobs 4
```

The cache selects XML, managed runtime, zlib, LZ4 and Zstandard ON, with
`VCPKG_MANIFEST_INSTALL=OFF` and the existing
`out/build/make-Release/vcpkg_installed/x64-windows` dependencies. `TEMP` and `TMP`
point to `out/build/opc-implementation/temp` for accessible test-owned fixtures.
The successful full output is retained as
`out/build/opc-implementation/full-checkpoint-final.log`; CTest's `LastTest.log`
records the final individual case counts. These elapsed times are regression-run
results, not application throughput or memory benchmarks.

The latest Linux full build/test, complete OPC and official 3MF schema/corpus
conformance, independent 3MF application interoperability, broader performance
measurements and remaining cross-language package implementations are not
qualified by these checks. Retain them in [pending work](../xml_pending.md).
C++ managed-reference and collaboration results do not establish those contracts
for other backends. Migration logs are retained as
`out/build/opc-implementation/migration-focused-{build,test}.log`; recursive
reference regression logs are under `out/build/opc-doc-review/recursive-reference-*.log`.
Installed-consumer logs are under
`out/build/opc-doc-review/migration-installed-*.log`; minimal-package logs are
retained under `out/build/minimal-checkpoint`.
