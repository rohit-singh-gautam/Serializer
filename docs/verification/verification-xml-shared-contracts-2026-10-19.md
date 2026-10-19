# XML shared-contract verification - 19 October 2026

Compiler/runtime **2.10.0**, schema language **1.20.0**, editor packages **2.0.5**.
Shared tasks **S01-S07 are completed for their defined shared-contract scope**.
This record covers S02-S07 and integration with the existing
[S01 event boundary](verification-xml-events-2026-10-19.md). See the
[shared contract guide](../serialization/xml_shared_contracts.md),
[event-reader contract](../serialization/xml_events.md) and
[central backlog](../pending.md).

Source identity: the uncommitted S02-S07 working tree based on **`ebd8a90`**
(`Define bounded XML event ownership and validation for S01`). No new commit
was created. The final Linux `source-hashes.json` audit matches 625 source,
build and test files against this working tree and its isolated source snapshot.

## Implemented scope

S02 supplies the C++ LibXml2 SAX2 push reader, with incremental UTF-8/XML 1.0
validation, borrowed normalized events, expanded names and namespace scope,
explicit checked EOF completion, DTD/user/external-entity rejection and permanent
failure fencing. S03-S07 supply checked shared compiler IR/configuration,
reference validators and public raw extension-slot helpers. New valid contracts
bind but reject generated-code emission with an explicit capability diagnostic;
malformed contracts reject before that gate. Existing generated/native wire
behavior and editor syntax remain unchanged.

This does not establish generated nil/mixed/map/variant/extension codec parity,
fixed-XSD support for the new configured contracts, XSD export or XML
compatibility checking for those shapes, arbitrary vocabulary support, complete
OPC/3MF conformance or independent corpus/performance qualification.
Per-language X05/X06/X07/X08/X16 remain pending.

## Final source checks

The Windows full-provider regression baseline passes **177/177 CTests in
213.70 seconds** through the root `./make.ps1 test` wrapper, using
`out/build/opc-implementation` with XML, Zstandard, LZ4, zlib and managed enabled.
The retained log is `s02-s07-full.log` in that directory. Windows uses MSVC
14.51 and LibXml2 2.15.3.

That regression baseline uses binaries built before the final shared-contract
configuration corrections. The subsequent **35-step targeted rebuild** succeeds
against the frozen final source, and its final focused selection passes **8/8
CTests in 0.92 seconds**: seven suites containing **80 GTest cases** plus
`serializer_package_version`. Final logs are `s02-s07-final-build.log` and
`s02-s07-final-tests.log` in the same directory. Both enabled and disabled
compiler profiles report compiler **2.10.0** and schema language **1.20.0**.

The final Windows build with XML, Zstandard, LZ4 and zlib all disabled passes
**5/5 focused CTests in 2.42 seconds**: `xml_event_reader_test`,
`xml_container_contract_test`, `xml_content_contract_test`,
`xml_extension_slot_test` and `serializer_package_version`. The reader's one
unavailable-provider boundary case runs and passes; its eight provider-dependent
cases are explicitly skipped. Container/content/extension reference tests run
with their 14/17/8 cases. The disabled compiler reports:

```text
Serializer compiler 2.10.0
Supported schema language version: 1.20.0
```

The cache is `out/build/s01-minimal`; final logs are
`s02-s07-final-build.log` and `s02-s07-final-tests.log` in that directory.

The separate XML/all-providers-disabled installed-package CTest passes **1/1 in
146.03 seconds**. It compiles installed public headers individually, including
`xml_event_reader.hpp` and `xml_extensions.hpp`, links runtime capabilities and
builds/tests installed C++, C and Python examples. The retained log is
`out/build/s01-minimal/s02-s07-installed-package.log`.

The isolated **Ubuntu 26.04.1 LTS / WSL2** qualification passes **7/7 focused
CTests in 0.17 seconds**, including **70 GTest cases** across the six new/shared
suites and `serializer_package_version`. The configuration is Release
with XML/zlib enabled and Zstandard/LZ4/managed disabled, using GCC **15.2.0**,
CMake **4.2.3**, LibXml2 **2.15.2**, zlib **1.3.1**, Ninja **1.13.2** and GNU Make
**4.4.1**. Strict repository warning flags remain enabled, including `-Werror`
and the runtime/compiler libraries' `-Weffc++` checks.

The source snapshot is
`/var/tmp/serializer-xml-contracts-20261019-bWOjYd`, with the build at
`out/build/native` beneath it. The qualification audit matched **625
source/build/test-file hashes** with no mismatches. Windows-retained evidence in
`out/build/s02-s07-linux` includes
`commands.txt`, `toolchain.log`, `qualification.json`, `snapshot-hash-audit.json`,
`build-final.log` and `tests.log`.

The Linux target/CTest selections are:

```sh
cmake --build out/build/native --target \
  xml_event_test xml_event_reader_test xml_content_contract_test \
  xml_container_contract_test xml_extension_slot_test xml_shared_contract_test \
  --parallel 4
ctest --test-dir out/build/native \
  -R '^(xml_event_test|xml_event_reader_test|xml_content_contract_test|xml_container_contract_test|xml_extension_slot_test|xml_shared_contract_test|serializer_package_version)$' \
  --output-on-failure
```

`commands.txt` also records the root `make configure` command, exact provider
options and the existing local GTest prefix used for the isolated build.

The shared/regression suite inventory is:

| Suite | Cases | Scope |
| --- | --- | --- |
| `xml_event_test` | 15 | S01 ownership, normalized shape, structural/resource and completion checks. |
| `xml_event_reader_test` | 9 | S02 incremental UTF-8, namespace scope, EOF, DTD/resource-loader and failure checks. |
| `xml_content_contract_test` | 17 | S03/S04 presence/nil, legacy defaults/lists, ordered fragments, malformed shapes and bounds. |
| `xml_container_contract_test` | 14 | S05/S06 canonical map keys/cardinality, tags/types, conflicting metadata and limits. |
| `xml_extension_slot_test` | 8 | S07 policies, positions, owning preservation, namespace scope, capabilities and work quotas. |
| `xml_shared_contract_test` | 7 | Production INI/parser/binder integration and generation capability rejection. |
| `xml_rich_test` | 10 | Existing rich XML regression cases. |

Reader fixtures cover every two-chunk byte split and single-byte feeds of
multibyte names/namespace IRIs/text, BOM/declaration boundaries,
character/predefined references, normalized attributes, CDATA and empty elements.
They check namespace default reset/shadow/pop, duplicate/unbound names, invalid
UTF-8, trailing/incomplete input, DTD forms at every split and a counting
external loader that must remain unused. Observer/context exceptions, reentry,
completion, cumulative limits, cancellation and deadlines are covered.

The three C++ snippets in `xml_events.md` and `xml_shared_contracts.md` pass
MSVC `/std:c++20 /EHsc /W4 /WX /Zs` syntax checks against the public headers.
This is separate from linking/running the examples. The final local audit
resolves **870 relative file links** and **228 heading-fragment links** across
the twelve updated documents, including `docs/pending.md`, excluding fenced
code. UTF-8 decoding, CRLF line endings, balanced fences and whitespace checks
also pass.

## Limits and retained artifacts

The event reader retains no XML tree, but provider dictionaries, token/input and
namespace buffers, URI-parser allocations, allocator overhead and spare capacity
are outside exact logical state/context accounting. Total input bounds and the
provider's normal non-HUGE limits still apply. No process peak-memory or hard
provider preemption bound is claimed. Raw preservation requires complete parsed
namespace scope and semantic capability declarations; opaque retention never
establishes required vocabulary understanding.

Useful qualification builds/logs remain under ignored
`out/build/opc-implementation`, `out/build/s01-minimal` and
`out/build/s02-s07-linux`, together with the Linux snapshot named above.
Temporary direct-compile and documentation snippet probes were removed. No
generated outputs require a new `.gitignore` pattern.
