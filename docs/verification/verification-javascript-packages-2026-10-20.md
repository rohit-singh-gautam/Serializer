# JavaScript and TypeScript package qualification, 20 October 2026

[JS/TS package API](../languages/javascript_packages.md) ·
[Package contract](../serialization/xml_packages.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

This record qualifies **JS-P01–JS-P15, JS-P17–JS-P19** and the matching
**TS-P01–TS-P15, TS-P17–TS-P19** for the selected generated ES-module/TypeScript,
optional Node provider and Windows filesystem profile in compiler/runtime
**2.14.0**, schema language **1.20.0**. **P16, P20 and P21 remain open.**
These results do not imply complete Office/OpenXML support, automatic application
reference checks, arbitrary runtime codec profiles or Linux filesystem fault tests.

## Actual compiler and SDK evidence

The maintained [runner](../../test/javascript_package_test.py) uses the rebuilt
actual compiler, actual **Node 22.23.1**, **TypeScript 5.9.3** with strict NodeNext
ES2022 checking, and the existing exact XML lock: **saxes 6.0.0**, **xmlchars 2.2.0**,
**@xmldom/xmldom 0.9.12**. Runtime support comes from generated source, rather than
a hand-maintained test implementation. The complete final run exits zero.

| Executed check | Result |
| --- | --- |
| Independently authored input corpus | **175/175 decisions** pass |
| Independently inspected output | **14/14** pass: 12 ZIP variants and two complete OPC packages |
| Native interchange | **5/5** pass: three C++ ZIP outputs read by JS, one typed C++ OPC output read/re-encoded by JS, and that JS output consumed by the existing C++ example |
| Writer policy-driven close boundaries | **56/56** reject the active operation and fence later borrowed calls |
| Strict TypeScript profiles | **3/3** pass: ordinary typed facade, helper-family collisions and previously legal builtin-like XML model names |
| Browser isolation | Ordinary import and stored ZIP work with zero Node builtin loads; explicit Node compression admission rejects when Node loading is forbidden |
| Atomic Save | All five injected checkpoints produce the exact original file before replacement or a complete readable replacement plus an uncertain-publication error afterward |

The final immutable compiler has SHA-256
`9714898052e8be4c158e5de2a256672f173014ae980f35052d8a4ec517ef21be`.
It is retained with deployed DLLs and `source-manifest.json` in ignored
`out/build/backlog-2026-10-20/compiler-package-final-97148980/`.
The full final transcript is `javascript-package-final.log`; the machine-readable
summary is `javascript-package/final/verification.json` under that backlog build
directory. Normal generated source is **278,602 bytes**; helper and builtin
collision profiles are **277,341** and **276,934 bytes** respectively.
All per-run temporary fixture/generated/SDK directories clean themselves up.

## Independent archive and OPC checks

Python `zipfile` and explicit wire authoring supply the inputs; Python `zipfile`,
zlib, ElementTree and the existing native example independently consume output.
The [shared malformed corpus](../../test/resources/package_malformed_fixtures.py),
[descriptor authoring](../../test/python_package_test.py) and
[ZIP64 segment/count authoring](../../test/package_zip64_test.py) are reused
without either generated ZIP or OPC metadata writer authoring the controls.

ZIP cases cover stored/DEFLATE, empty/binary/UTF-8 names, exact CRC and expanded
size, malformed local/central offsets and lengths, truncated/trailing raw DEFLATE,
and signed/unsigned ZIP32/ZIP64 descriptors. A payload whose CRC is exactly the
descriptor signature distinguishes the ambiguous unsigned case. The 12 output
variants cross two storage methods, three descriptor forms and forced-small
ZIP32/ZIP64; independent Python checks every payload and complete framing.

The count fixtures contain **65,534, 65,535 and 65,536 actual entries**, plus
early/middle/late duplicate identities in a 65,536-entry archive. Segmented random
sources exercise local-header and directory offsets immediately below, equal to
and above `UINT32_MAX`, the classic directory field exactly at `UINT32_MAX`, and
ordinary narrow fields accompanying an actual ZIP64 locator. Locator/record
extents, maximum 64-bit values, split counts and contradictory ordinary/extended
fields reject. Actual requested reads stay at most 32 KiB and total physical reads
stay below 1 MiB per segmented case; no multi-gigabyte allocation/file is used.

OPC cases include alternate typed root/resource paths, total role multiplicity,
first/middle/last invalid XML resources, Default/Override duplicate/conflicting
identities and precedence, missing/malformed/reserved media types, UTF-16 BOM
metadata, excluded physical directories/nonfiles/non-PartName items, and explicit
unsupported Unicode candidates. Scoped internal URI resolution, query/fragment
selectors and inert external RFC3986/3987 Unicode authorities are exercised.
Relationships preprocessing checks inherited namespace identities, Ignorable,
ProcessContent, MustUnderstand, ordered Choice/Fallback and root AlternateContent;
malformed scope/wrapper/required-capability combinations reject.

The whole-part policy matrix independently crosses reject/skip/preserve for parts
and links. Required unknown items reject even under skip; preserve retains exact
unknown bytes and scoped links. Edited output with a removed referenced part
rejects. Independent output inspection verifies XML expanded names, generated
manifests, relationship sources and opaque bytes. Reversing payload/link input
order yields exact equal archive bytes.

The native interchange uses the maintained
[OPC example](../../example/opc/model.serializer) and its existing typed C++ reader,
checking titles, repeated native items, IDs, labels and attachment bytes in both
directions. It also reads all three existing C++ canonical/forced-ZIP64 outputs.

## Ownership, bounds and confirmed repairs

Complete mutable byte input is copied before asynchronous yielding. Short random
reads, original transport/cancellation identity, no borrowed close/flush, exact
and one-tight cumulative budgets, asynchronous final validation, nested operation
first-error fencing and callback-driven closure are exercised. Missing provider
admission occurs before model, cancellation or sink callbacks. Reader callback
closure prevents later reads. Private sequential spooling rejects overflow and
cleans up on success, mid-spool cancellation and transport failure; borrowed
sources stay open. A separate close-while-awaiting-sink check prevents every later
chunk. Invalid asynchronous clock/required-policy hooks reject without detached
unhandled rejections. Terminal reader/writer cleanup releases logical reservations
and clears private references to snapshot/staged payload storage.
Save reload-validates before exclusive sibling creation and replacement.

Two concrete admission defects were reproduced before repair. The first actual
compiler accepted otherwise valid Unicode OPC part candidates by silently
excluding them; the maintained unsupported-candidate fixture rejected that result.
The next actual compiler accepted an invalid triple-colon IPv6 literal
`http://[:::]/`; Python's independent `ipaddress.IPv6Address` rejects that spelling,
and the actual generated regression failed with **Missing expected rejection**.
The repair validates exactly one `::` compression with nonempty well-formed groups
on either side and checks embedded IPv4 placement/range. Final independent load,
encode and Save probes reject the malformed authority while preserving an old
destination. Evidence is retained in `javascript-package-unicode-before.log` and
`javascript-package-ipv6-before.log`; the useful minimal IPv6 probe is retained in
`javascript-package/ipv6-before/`. No payload excerpts are emitted in package errors.

## Boundaries and remaining tasks

**P16 stays open:** generated XML per-message limits are present, but every nested
XML object/reference does not automatically consume the package ledger, and full
part/field/link diagnostics have not been qualified across that bridge.
**P20 stays open:** the schema has no fixed/default/runtime codec-profile policy
declaration, and this facade binds typed roles to generated XML.
**P21 stays open:** application payload keys and cross-part typed reference rules
have no generated declarative contract here. An explicit caller validator is
useful but does not close automatic generated validation.

The reader accepts exact safe-integer ZIP64 offsets; larger integers fail before
seeking. Output stages a bounded complete owning archive, rather than streaming a
multi-gigabyte result. The selected writer uses a deterministic fixed metadata
policy; no claim requires equal bytes between independently implemented backends.
Part URIs use the documented ASCII subset, other methods/encryption/split archives
remain unsupported, and external links are never fetched. Windows Save fault
qualification does not establish every filesystem's crash durability, POSIX
directory synchronization or process-RSS bounds. The broader performance/fuzzing
and unsupported backend matrices remain separate backlog work.

ZIP wire-field interpretation follows the official
[PKWARE APPNOTE](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).
The existing package contract documents its selected OPC edition and limits;
the optional host uses maintained
[Node filesystem promises](https://nodejs.org/docs/latest-v22.x/api/fs.html#promises-api).
