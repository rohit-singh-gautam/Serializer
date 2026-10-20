# C++ and Python OPC/XSD checks, 2026-10-19

Working source: compiler/runtime **2.8.0**, schema language **1.20.0**, based on
`a2daa6e` with the accompanying uncommitted changes. This record qualifies the
specified implementations and fixtures. Complete OPC-standard conformance,
3MF application conformance and parity in the other nine backends remain in the
[central backlog](../pending.md).

The earlier [2.7 checkpoint](verification-opc-xsd-2026-10-19.md) retains its own
eleven-SDK ordinary-XSD, managed-identity and migration results. Those historical
results are not substituted for execution of changed 2.8 contracts.

## Executed checks

| Check | Result |
| --- | --- |
| XML build default | Fresh Windows `make.ps1 configure` and Linux `make configure` select `SERIALIZER_WITH_XML=ON` without an XML argument; fresh Windows explicit `OFF` remains `OFF`. Existing caches retain their selection. |
| Windows focused native | Five CTests pass: Core generated consumer (3 cases), importer (23), XML operation/contract (19), package (36), and rich generated XML (10). Import, C++ generation and strict compilation of the published Core bundle complete successfully. |
| Linux focused runtime | XML operation/contract (19 cases) and package (36) pass, including UTF-16 metadata, MCE, MIME parameter identity, raw numeric-pattern callbacks and physical ZIP item mapping. |
| Windows full native | Full root `make.ps1 test` exits 0: 171/171 CTests pass in 316.08 seconds, including runtime-template checks, installed consumers, generated Core/rich XML, package and managed-reference regressions. |
| Linux full native | Full root `make test` exits 0: 170/170 CTests pass in 232.63 seconds. Installed headers/exports/providers and standalone C++, C and Python consumers pass; the installed-package test takes 171.09 seconds. A freshness audit of 1,601 production/test/build/example files reports no mismatch against the recorded source snapshot and overrides. |
| Python generated consumers | All four fresh-generation SDK suites pass: package (11 cases), rich XML (11), repeated choice (4, including the 1,275-input independent occurrence matrix), and published Core (2). No source overlays are used. |
| Minimal installed package | XML/managed/all optional compression providers OFF: configure, compiler/runtime build and install pass. All 44 installed public headers compile together under `/W4 /WX`; the provider-exclusion executable passes. Thirteen record-dependent managed/generated headers and the managed target are absent as intended. Installed compiler reports 2.8.0 / language 1.20.0. |
| CMake package compatibility | Actual installed discovery accepts requests 2.0.0, 2.6.0, 2.7.0, 2.8.0 and exact 2.8.0; rejects 2.8.1 and 3.0.0. This qualifies version discovery, not every historical generated-code ABI. |
| Runtime template packaging | Canonical source-license and portable literal-size checks pass; largest token is 14,972 bytes. Four concatenated runtime payloads and three regenerated Python package/rich/numeric modules are byte-identical before/after the source-only packaging correction. |
| Ordinary native output | All eleven unchanged ordinary-record outputs match the captured 2.6 baseline by SHA-256 and byte size. This single fixture does not measure application throughput or peak memory. |
| Editor provider review | Unchanged language 1.20 grammar/layout requires no editor edits. A focused shared-provider probe passes 100 assertions for generated Python package/model navigation in both directions and role-type navigation through an include, using renamed output and a path containing spaces. This is not an editor-host UI or fresh Visual Studio .NET-engine run. |
| Documentation links | All 28 changed/new Markdown files pass local destination/anchor checks: 1,333 local destinations and 342 fragment anchors, with no issues. External destinations are outside this audit. |

Windows uses MSVC 19.51, C++20, Ninja, `/W4 /WX`, libxml2 2.15.3 and zlib 1.3.2.
The full native cache enables XML, managed runtime, zlib, Zstandard and LZ4.
Linux uses an isolated persistent ext4 snapshot under WSL Ubuntu, GCC 15.2.0,
CMake 4.2.3, libxml2 2.15.2, zlib 1.3.1 and an isolated source build of official
GoogleTest 1.18.0. Linux Zstandard/LZ4 development packages are absent and those
providers are disabled; this is not all-provider Linux qualification.
The installed Python CMake example uses `LANGUAGES NONE`; its test explicitly
forwards the parent toolchain's detected library architecture for multiarch
provider discovery. This does not qualify automatic compiler-only package
discovery, which remains **BE04**. Native qualification also corrects three
test-generated directories that collided with extensionless executable names and
adds the existing constructor test's missing post-mutation assertion for GCC.
Python uses 3.14.3 (64-bit AMD64), with optional lxml 6.1.3, libxml2 2.11.9 and
libxslt 1.1.45 for the pattern/provider checks. Package framing uses the Python
standard library.

The Windows wrapper is run from an MSVC x64 Developer PowerShell:

```powershell
./make.ps1 test -BuildDirectory out/build/opc-implementation -Jobs 3
```

Its cache reuses `out/build/make-Release/vcpkg_installed/x64-windows` with manifest
installation disabled. `TEMP`/`TMP` select the build's `temp` directory. Linux uses
the root `make test CONFIG=Release BUILD_DIR=out/build/native JOBS=8` in its isolated
source snapshot. Build/test outputs and final qualification logs remain ignored
under `out/build/opc-implementation`, `out/build/opc-linux-native` and
`out/build/python-package`.
Minimal install/probe logs remain under `out/build/minimal-checkpoint` with
`-2_8.log` suffixes, and its install prefix is `out/install/minimal-checkpoint`.
Useful builds, provider files, probes and final verification logs are retained;
superseded diagnostic scripts and the temporary Core prototype were removed.
The reproducible Python package navigation probe and its generated fixture are
retained under `out/build/editor-python-package-review`, with `final-probe.log`.

## Selected OPC contract

The normative baseline is [ECMA-376 Part 2, fifth edition, December 2021](https://ecma-international.org/publications-and-standards/standards/ecma-376/),
with Part 3 fifth-edition Relationships markup-compatibility preprocessing.
These checks cover a documented subset:

| Contract | Implemented and exercised scope |
| --- | --- |
| Part identity and resolution | ASCII PartName grammar, literal colons, checked escapes/case equivalence, ancestor collisions, relative/dot/root-clamped targets and retained query/fragment selectors. Selectors resolve an underlying part; applications interpret their meaning. |
| Media types and relationships | MIME tokens/quoted values, reserved OPC MIME rules, typed role matching with case-insensitive type/parameter names and case-sensitive decoded parameter values, source-local Unicode NCName IDs, target modes and inert external links. |
| Metadata XML | UTF-8 and BOM-marked UTF-16 in either byte order for OPC metadata. UTF-16 declarations must be absent or identify `UTF-16`; conflicting/unsupported encodings reject. Ordinary typed XML remains UTF-8. |
| Relationships MCE | Inherited Ignorable/ProcessContent, MustUnderstand and ordered AlternateContent/Choice/Fallback. Only the Relationships namespace is understood, as required by Part 2 section 6.5.3.1; resolving the `xml` prefix does not make its namespace understood. |
| Physical mapping | Trailing-slash directories and supported creator-specific directory/volume/Unix nonregular entries do not become logical parts. ASCII non-PartName items are excluded. Excluded payloads still pass shared expansion/size/CRC checks; links and content-type overrides cannot claim them as parts. General ZIP filename policy is separate. |
| Assembly and loading | Standard content types and `.rels` are infrastructure-owned. Generated C++/Python profiles compose typed XML and opaque parts under one cumulative operation budget, validate complete candidates and support staged full Save. |

Optional interleaving/growth hints, non-ASCII part naming, outer Pack-IRI APIs,
Core Properties adapters, signatures and complete ZIP Annex B/pass-through
qualification remain outside this scope. Python's general Unicode authority IRI
coverage also remains unqualified where its URL provider rejects Unicode forms.
Fixed-name typed-role saves use the declared canonical path; automatic retargeting
of retained links after a renamed typed part remains pending.

## XSD and Core scope

The [pinned Core 1.4 bundle](../../test/resources/3mf_core_1_4/README.md) contains
the published Core XSD and unchanged W3C XML namespace schema; the only Core edit
is a local `schemaLocation`. Its bundle SHA-256 is
`05216345de8700ae8bba7ba90055acd326f280a2d6bc1f8969640b6ff91b1e2b`.
The compiler's documented provider grammar projection leaves source bytes, bundle
identity and exact native occurrence limits intact.

Generated consumers exercise an authored tetrahedron XML model, text metadata,
language-or-empty union, positive/bounded IDs, QName binding, numeric/matrix/color
patterns, repeated known resources and positioned foreign wildcard content. The
fixture is authored for Serializer; it is not an upstream 3MF application corpus
or a complete 3MF package. Geometry, resource-link and manufacturing rules still
need the application's validation and independent 3MF consumers.

The occurrence validator is checked against 1,275 independent .NET XML Schema
oracle results across five branch/group-bound contracts. Rich numeric tests
cover selected value-space enumeration/fixed/bound identity, float/double width
and XSD 1.0 NaN semantics. These are finite boundary fixtures, not exhaustive
qualification of every floating overflow/underflow/rounding case.
In particular, C++ native conversion rejects out-of-range/underflow-to-zero
lexemes; complete XSD floating conversion parity remains pending. Original
patterned numeric input is checked before native conversion; canonical output
must independently satisfy the pattern.

See [the exact supported subset](../serialization/xsd_tooling.md) for unsupported
union/QName/list shapes, general choices, schema derivations and export limits.
C++ cannot retain every child-local QName binding in its record namespace map;
such unrepresentable inputs fail rather than silently changing expanded identity.

Provider-internal allocations are not all exposed to the logical operation
ledger. Libxml2/lxml validation calls cannot be interrupted internally; checks
surround them. Tree/part staging remains bounded by the documented logical
limits, not a guarantee of constant memory or hard process peak allocation.
