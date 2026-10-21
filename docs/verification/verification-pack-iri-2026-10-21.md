# Fifth-edition outer package addressing qualification

Development source following compiler/runtime **2.21.0**, targeted for aggregate
release **2.22.0**; schema language **1.21.0** is unchanged. 21 October 2026.

`OPC-OPT03` is implemented through the public C++ `pack_address`,
`compose_pack_iri`, `parse_pack_iri`, `resolve_pack_reference` and
`equivalent_pack_iri` APIs in `<rohit/package.hpp>`. The implementation in
`src/package_opc.cpp` uses the existing URI/IRI and OPC part validators but leaves
internal relationship resolution unchanged. The
[usage contract](../serialization/pack_iri.md) defines every exposed component,
resource boundary, optional feature policy and application-owned outer-scheme
equivalence callback.

## Standards and scope

The implementation follows the actual fifth-edition ECMA-376 Part 2 clauses
6.2.2.2 and 6.3-6.4, RFC 3986 sections 2.1 and 5.2-5.4, and RFC 3987 section 2.2.
The official normative bundle and extracted text used for review are the same
SHA-qualified inputs retained by the
[core-property qualification](verification-core-properties-2026-10-21.md#independent-source-identities-and-replay).

The published fifth-edition examples/notes use `%3c` for colon. Hexadecimal `3C`
is `<`; RFC percent encoding of colon is `3A`. The normative composition algorithm
and ordinary percent encoder therefore yield `%3A`. Tests independently require
that output and reject a decoded invalid `%3c` example, rather than silently
changing either resource. Legacy unescaped-authority-colon spelling also rejects.
This publication inconsistency is explicit in the usage documentation.

The API covers composition/decomposition, exactly-once ASCII authority decoding,
absolute package validation, forbidden nested pack locators, package-versus-part
identity, optional/empty outer selectors, reserved delimiter encoding, IPv6 outer
locators, UTF-8 outer names and complete generic RFC relative resolution.
External and directory results remain generic resolved IRIs until a separate
Pack-IRI/PartName parse is requested. No archive/file/network access occurs.
Scheme-specific package identity is a required caller policy; the library supplies
OPC part identity and propagates original policy exceptions.

Non-ASCII part names, interleaving and growth-hint production are deliberate
optional exclusions, using the selected edition's explicit boundaries. Their
documented policy does not waive mandatory ZIP/OPC consumption or editing
pass-through work tracked separately in `OPC-R08`. Transport, private identifier
schemes, trust and external resource aliases are
[application responsibilities](../project/application_responsibilities.md).

## Actual execution

The Windows build uses MSVC **19.51.36260.0**, strict `/W4 /WX` test/probe flags,
CMake **4.4**, the actual installed XML/compression providers and release GTest
**1.18.0**. The complete native package suite passes **66/66** tests, including
all six new Pack-IRI tests.
Tests include all literal RFC normal/abnormal vectors, malformed/nested/Unicode
part rejection, preserved selectors, callback failure propagation, cumulative
metadata/allocation limits, exact output admission and cooperative cancellation.

The independent Python runner `qualification/pack_iri_oracle.py` uses Python's
standard percent encoder and RFC reference resolver, not the native implementation,
and checks **40 composition/decomposition round trips, 42 RFC reference vectors,
20 invalid/unsupported inputs, a Unicode outer locator, two selector controls and
an exactly-once percent-decoding tripwire**. Native inputs use ASCII hex transport
so Windows argument codepages cannot turn a Unicode corpus case into a different
test. The runner inherits Windows error-dialog suppression before native launches.

Linux GNU **15.2.0** and CMake **4.2.3** build the actual runtime through the
repository's `make configure` wrapper. `SERIALIZER_WITH_XML`, `ZLIB`, `ZSTD` and
`LZ4` are all explicitly `OFF`; the standalone native probe builds with
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror`. The same complete independent
oracle inventory passes on that provider-free runtime. This demonstrates the
addressing API's independence from all optional XML/compression providers.
It does not claim an unexecuted Linux GTest suite or broader OPC corpus completion.

```powershell
python qualification/pack_iri_oracle.py `
  --probe out/build/pending-package-completion-2026-10-21/windows/test/pack_iri_probe.exe `
  --build out/build/pending-package-completion-2026-10-21/oracle-windows
```

The Linux command uses its actual `linux/pack_iri_probe` and `oracle-linux`
directories. Build outputs, final package logs and the two JSON summaries remain
intentionally under ignored `out/build/pending-package-completion-2026-10-21/`.
No new ignore rule is needed; no commit or publication was performed.
