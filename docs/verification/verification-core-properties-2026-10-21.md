# Fifth-edition core properties and package/compiler batch audit

Compiler/runtime **2.21.0**, schema language **1.21.0**, 21 October 2026.

This record closes the selected reusable C++ core-property adapter requirement
`OPC-OPT01`. It does not close the broader OPC, cross-backend profile, resource,
corpus, editor-host or release-publication gates audited below.

## Implemented standard contract

`include/rohit/core_properties.hpp` and `src/core_properties.cpp` expose a reusable
owning model, standalone XML encode/decode, OPC composition and discovery through
the caller's `profile_loader`. All fifteen fifth-edition properties are represented.
Keywords retain ordered mixed text, localized `value` children and optional
`xml:lang`; scalar properties retain optional absence versus empty values. Date
precision, lexical whitespace and timezone spelling are retained. The library
supplies no application authors, identifiers, revisions, clocks or trust decisions.

The implementation was checked against the actual
[ECMA-376 Part 2 fifth-edition bundle](https://ecma-international.org/wp-content/uploads/ECMA-376-2_5th_edition_december_2021.zip),
clauses 8.2-8.3, Annex C.3 and Annex G.2, rather than assuming the older sixteen-field
.NET property inventory was the selected edition's schema. In particular:

- Fifth-edition `contentType` was removed; fifteen core properties remain.
- Fifth-edition keywords permit mixed text and localized `value` children.
- Created/modified require namespace-resolved `xsi:type` identifying `W3CDTF`.
- Their imported Dublin Core datatype is the union of XML Schema `gYear`,
  `gYearMonth`, `date` and `dateTime`. The minute-only form from the general W3C
  date/time note is not a member of that imported union.
- OPC prose forbids `xml:lang` on ordinary Dublin Core properties and requires
  date `xsi:type`, even though schema-only validation permits those controls.
- Markup-compatibility elements/attributes and arbitrary property extensions
  reject in this closed typed adapter. Existing opaque whole-part preservation
  remains available for metadata outside the selected vocabulary.

The default `/docProps/core.xml` path is a writer convenience. Loading uses the
standard root relationship and checks singleton cardinality, internal mode and
content type before returning an optional candidate. Call the writing helper once
per writer. Generic OPC low-level builders do not themselves impose every
vocabulary-specific singleton rule. The normal staged full-Save API supplies
file-level transactional publication; a fallible stream writer can leave a partial
archive. These distinctions are documented in the
[usage contract](../serialization/core_properties.md).

## Windows execution

The new source passes MSVC **19.51.36260.0** with `/W4 /WX`, CMake **4.4**,
LibXml2 **2.15.3**, zlib **1.3.2** and GTest **1.18.0**. The targeted native package
suite passes **60/60** tests, including eleven new provider/core-property tests.
The new inventory covers all properties, independent prefix/local type binding,
UTF-8 and BOM-marked UTF-16, deterministic output, dates/calendar/timezones,
localized mixed keywords, malformed attributes/nesting/duplicates, package
discovery/mode/content-type/cardinality, cumulative resource limits, cancellation,
deadline expiry, 1,000 keywords and exact encoded-byte boundaries.

An explicit all-provider-disabled configuration (`SERIALIZER_WITH_XML`, `ZLIB`,
`ZSTD`, `LZ4` all `OFF`) builds and links the same API. Its package suite passes
**25 tests**, with **35 explicit provider-dependent skips**. The added provider
test verifies `package::error_code::unavailable` for standalone core-property
encode/decode. An existing identity-admission test unnecessarily invoked the XML
parser; its fixture now materializes the same two-element tree directly, keeping
the provider-independent identity-budget contract tested with XML disabled.

The independent runner `qualification/core_properties_oracle.py` uses the actual
official OPC/DC/W3C schemas through two independent XSD implementations:
Microsoft .NET Framework `System.Xml.XmlSchemaSet` (**4.8.10008.0**) and LibXml2's
schema API, separate from the adapter's handwritten date checks. It passes:

- **26** accepted/rejected date decisions against the normative imported union;
- **2** schema-admitted but OPC-prose-prohibited controls;
- **4** other fifth-edition schema rejection controls;
- **2** independent OPC producer/consumer exchanges, inspecting manifest,
  relationships, target modes, CRCs and all-property XML.

The .NET Framework oracle has four recorded differences from normative XML Schema
and LibXml2: it rejects midnight `24:00:00`, negative years and extended years,
and admits timezone offset `+14:01`. Native behavior follows XML Schema 1.0 and
LibXml2 for these cases. These differences are retained in the runner's summary;
they are not reported as universal provider agreement. Native ordinary output is
schema-valid in both providers. See the
[XML Schema dateTime lexical contract](https://www.w3.org/TR/xmlschema-2/#dateTime-lexical-representation).

The first private Windows build, configured through `CMAKE_PREFIX_PATH` without
the vcpkg toolchain's imported-configuration mapping, selected debug GTest/LZ4
imports for a `RelWithDebInfo` consumer. It faulted before GTest enumeration with
`-1073741819`; this was a private build ABI mismatch. The build was corrected with
`CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release;` (including generic fallback)
and relinked against the same release GTest DLLs as the repository build. Later
native children inherited `SetErrorMode` suppression. An intermediate fractional
date loop also timed out because an increment was inside a short-circuited OR;
the increment is now unconditional and the fractional-date regression completes.
Neither initial failure is counted as a passing run.

## Linux execution

The current runtime was configured through the repository's root `make configure`
wrapper and built with GNU **15.2.0**, CMake **4.2.3**, LibXml2 **2.15.2** and zlib
**1.3.1**. The native probe compiles with
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror` and links that actual CMake-built
runtime. Its explicit `libxml` oracle profile passes the same **26 date decisions,
2 prose controls, 4 other schema rejections and 2 independent OPC exchanges**.
The Linux oracle uses the same original schema hashes and the host's actual
LibXml2 schema implementation. .NET Framework was not run on Linux, and the
Windows sixty-case GTest result is not presented as a Linux GTest result.

The first default-provider Linux configuration failed because the Zstandard
development package configuration was absent. Successful Linux qualification
explicitly sets `SERIALIZER_WITH_ZSTD=OFF` and `SERIALIZER_WITH_LZ4=OFF`; it claims
XML/zlib only. No unavailable compression provider was silently substituted.
The selected package adapter adds no generator or schema-language requirement;
the library-only Linux build uses `SERIALIZER_BUILD_TESTS=OFF` and
`SERIALIZER_BUILD_MANAGED=OFF`.

## Independent source identities and replay

Official bundle SHA-256:
`1d489dc491168ea1f9e9a59063acc8dd5f02b4ad1d21aa7ec19ba9a58d020c70`.

| Original schema | SHA-256 |
| --- | --- |
| `opc-coreProperties.xsd` | `69688ea15f82a870053b9a60c79359063dbcfeec9cdabafabad51a53934bc35b` |
| `dc.xsd` | `bc1d454db1bb20dfb1867a3fe0cf007775fc4afa33a5df425c12d854f049f1f6` |
| `dcterms.xsd` | `6328c8b574c166f8ebc28d07231e6ee47121e9791689b70c3afa665f22099fed` |
| `dcmitype.xsd` | `4980845d0bedcf45e707fa64eb47492ae4f95ac4ae42f5c3c68b58c0bc9e8e49` |
| dated W3C `xml.xsd` | `0ed94db44402526e7a683424d0bd4b65e5bc50e65d9b9a762b11b30bae06d0c8` |

The Dublin Core schemas are the dated `2003/04/02` files imported by the official
OPC schema; their XML namespace import is the dated W3C `2001/03/xml.xsd`.
Validation resolves only listed local dependencies. LibXml2 uses disposable
copies whose `schemaLocation` URLs are localized; original schema bodies/hashes
remain retained. No candidate document can request remote resources from the
oracle. Windows replay, after building `package_test` and `core_properties_probe`:

```powershell
python qualification/core_properties_oracle.py `
  --probe out/build/pending-core-properties-2026-10-21/native/test/core_properties_probe.exe `
  --schema-directory out/build/pending-core-properties-2026-10-21/ecma/schemas `
  --libxml2 out/build/ReleaseWindows/vcpkg_installed/x64-windows/bin/libxml2.dll `
  --build out/build/pending-core-properties-2026-10-21/oracle
```

The runner requires .NET Framework's C# compiler for its default `dotnet-libxml`
profile. Its explicit `libxml` profile permits a Linux schema replay and records
only that executed oracle. Downloaded specification/schema inputs, useful builds,
native/oracle output and final logs remain intentionally under ignored
`out/build/pending-core-properties-2026-10-21/`. No source schema or package
fixture has been hidden by a new ignore pattern.

## Other assigned parent gates

All twenty-three assigned IDs were audited together. Only `OPC-OPT01` is closed
by this selected implementation. These twenty-two broader requirements remain
open; a narrower source or test improvement does not prove their full gate.

| IDs | Remaining acceptance gate |
| --- | --- |
| `OPC-R06`, `XML-B04`, `XML-B19` | Cross-backend vocabulary/type dispatch, declarative payload-reference combinations and pipeline/runtime profile selection beyond existing supported role graphs. |
| `OPC-R08` | Complete selected-edition Annex B/pass-through corpus, physical names, equivalence and metadata coverage. The core-property adapter is only one standard metadata part. |
| `OPC-R09`, `XML-B07`, `QA01`, `QA02`, `QA03` | Broad large-model/provider/platform stress, independent application corpora and reproducible performance evidence for each newly selected contract. Current selected checks remain distinct from broad parents. |
| `OPC-R11`, `XML-B05`, `XML-B21` | Complete cumulative accounting and bounded cancellation/deadline propagation across every selected codec, projection, compression, provider and publication layer in all backends. |
| `XML-B18` | All-backend ZIP64/spooling, large-archive stress, memory and provider/cancellation qualification. |
| `SC02` | Broader default validation/diagnostics and an explicitly defined bit-field/wire/compatibility contract. |
| `SC03` | Unsupported generic-function/default and fixed-array initializer/Protobuf combinations with portable contracts and actual SDK evidence. |
| `BE01` | Native-backend separate-file output, dependency/module naming and actual editor navigation. |
| `BE02` | Java packed output plus selected stream/channel, native-acceleration and output-buffer-reuse APIs with ownership/resource qualification. |
| `BE03` | Borrowed/mapped-view platform/lifetime/allocation/performance boundaries and explicitly selected additional format mapping contracts. |
| `OPC-OPT02` | Signature canonicalization/trust provider and independent signed editing qualification. No provider was invented by the core-property adapter. |
| `OPC-OPT03` | Separately selected outer package/Pack-IRI addressing APIs and the optional naming/interleaving/growth-hint contracts. |
| `QA04` | Actual supported editor-host/navigation qualification beyond interpreter/provider-only records. |
| `REL02` | Separately authorized publication of prepared artifacts and upstream metadata/ports. This coding task did not authorize a new release publication. |
