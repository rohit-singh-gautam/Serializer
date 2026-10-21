# OPC fifth-edition metadata qualification

21 October 2026. Development checkpoints for compiler/runtime **2.22.0**,
schema language **1.21.0**. Earlier immutable checkpoints retain their actual
2.21.0 version and compiler hashes in their machine-readable reports.
This record covers the mandatory URI/content-type/Relationships, infrastructure
encoding and fifth-edition Relationships compatibility contract in `OPC-R08`.
The [Annex B record](verification-opc-annex-b-2026-10-21.md) covers physical ZIP
consumption and complete-rewrite Save; the [usage contract](../serialization/zip_metadata.md)
distinguishes that usage from individual ZIP record editing.

## Independent normative inputs

The official ECMA-376 Part 2 fifth-edition December 2021 bundle SHA-256 is
`1d489dc491168ea1f9e9a59063acc8dd5f02b4ad1d21aa7ec19ba9a58d020c70`.
Its supplied content-types and Relationships XSDs determine the actual attribute
types and required fields. `PartName`, relationship `Type`/`Target` and `Id`
use XSD whitespace collapse; MIME, `Extension` and `TargetMode` retain their
declared string semantics. Section 6.2.5(a) requires UTF-8 or UTF-16 and prohibits
infrastructure encoding declarations naming other encodings. This is stricter
than merely offering XML1.0's optional UTF-16LE/UTF-16BE labels.

The official ECMA-376 Part 3 fifth-edition December 2015 bundle SHA-256 is
`42294159fbbbe9393ccadac95b859d7729cc68d908898bcbe31034dda059daa8`.
Its reference-processing steps select namespace-scoped Ignorable,
ProcessContent and MustUnderstand behavior, ordered Choice/Fallback selection,
ignored-subtree semantics and logical-root requirements. Legacy
PreserveElements/PreserveAttributes directives are not imported from another
edition. Official bundles are available from the
[ECMA-376 publication page](https://ecma-international.org/publications-and-standards/standards/ecma-376/).

IRI grammar uses [RFC3986](https://www.rfc-editor.org/rfc/rfc3986#section-3) and
[RFC3987](https://www.rfc-editor.org/rfc/rfc3987#section-2.2).
RFC3986 sections 5.2.4 and 5.4.2 clamp excess leading dot segments at the URI
root. Existing native behavior already did this. Two old C#/Kotlin test rows
incorrectly classified references to existing package parts as escapes; their
expected decisions were corrected to successful model/attachment assertions.
Missing-target controls remain negative. No filesystem path is resolved or
external resource opened by these operations. Rust's existing public strict
`target` utility retains its rejection contract; the OPC loader uses its own
RFC resolver.

## Maintained common corpus

- `test/package_annex_b_corpus.py`: **31** explicit physical-record decisions.
- `test/package_mce_corpus.py`: **43** well-formed Relationships documents,
  **16** accepted and **27** rejected according to the selected reference model.
  Python ElementTree independently verifies XML well-formedness before use.
- `test/package_opc_uri_corpus.py`: **30** physical/logical names, escaped
  reserved bytes including `%00`, case-equivalent identities, scoped relationship
  part names, selectors, dot segments, collisions, excluded physical records,
  CRC failures and schema-declared whitespace contracts.
- `test/package_iri_corpus.py`: **65** IRI component/authority/IP syntax,
  absolute type, missing required attribute and TargetMode string decisions.
- `test/package_utf16_corpus.py`: **27** UTF-16/UTF-32/declaration/scalar,
  XML1.1 and no-DTD decisions. Positive roots are independently parsed before
  generated runtime execution. Typed model XML remains under its own UTF-8 policy.

These are authored separately from generated writers. All inputs reach actual
SDK adapters, not a reimplementation of the reader under test. Existing suites
also retain precise error-category, cancellation, allocation, rollback,
publication, MIME parameter, scoped identity and independent-output checks.

## Executed profiles

Windows C's final shared-corpus replay passes **260** independent and **16** application cases, plus **68**
encode and **85** load allocation-failure decisions; see the
[C URI record](verification-c-opc-uri-2026-10-21.md). Rust passes the same
**260 + 16** inventory, including the four additional malformed IPv6 literals,
with its actual Cargo SDK and pinned XML/compression providers. Java passes
**416** SDK checks (**370** fixture decisions), followed by the complete **65**
IRI decisions through its existing compiled adapter. Kotlin similarly passes
**433** checks (**372** fixture decisions), then the **65** IRI adapter replay.
C#'s final full replay passes **437** checks (**376** fixture decisions). Python
passes **43** tests, and JavaScript/TypeScript each pass **371** independent
input decisions and **14** independently inspected output decisions. Go's real
owning/profile SDK replay passes the same common corpus plus its maintained
metadata, role, publication and native codec controls. Swift's full SDK gate
passes **63** independent ZIP fixtures, the same metadata corpus, **15**
application fixtures, **8** output wire forms, collision and identity profiles.
The final Swift owning/profile/application executable separately passes **183**
OPC rows with preserved unknown relationships for every negative IRI case,
including all **65** shared IRI decisions and four extra scalar cases. This
final strict replay uses compiler SHA-256
`7fe39e5a706112b8a9481fc7902bfd2f3678c7c828a71a568b972845d9b8ffb7`
and generated-source SHA-256
`4f03b40d703269cff99771c3660a4a0c753d389dcb6c8d40f5cbf47b58904678`;
it completes in **102.12 seconds**. Earlier full collision/identity profiles
remain separately recorded. C's final four-literal replay is recorded in its
linked result and in `c-opc-identity-final/iri65-verification.json` under the
retained XML qualification build directory. The common corpus contains **196** cases per backend:
**43** MCE, **30** URI, **65** IRI, **27** encoding and **31** Annex B decisions.

Native Windows and Linux C++ libxml2/zlib readers each independently pass all
**165** MCE/URI/IRI/UTF16 decisions through `zip_metadata_probe` and
`qualification/opc_mce_oracle.py`; neither platform substitutes a generated
writer as the input oracle.

Initial common-corpus execution exposed repeated internal slash collapse and
incorrect TargetMode whitespace collapse in eight generated profiles. Swift's
new anyURI normalization initially treated CRLF as one grapheme; the fix walks
Unicode scalars. C# accepted a trailing half UTF-16 code unit through System.Xml;
the metadata boundary now rejects odd UTF-16 byte counts. These cases remain
in the maintained shared corpus. Development failures described here, final
SDK command logs and machine-readable inventories are retained under ignored
`out/build/pending-package-completion-2026-10-21/`.

Swift additionally retains four targeted tag-plane combining-scalar controls
immediately after ASCII URI delimiters. These use preserved unknown
relationships for negative tests so an unrelated extension-policy rejection
cannot hide invalid IRI acceptance. The final helper advances scalar indices
after delimiters. `qualification/swift_iri_scalar_oracle.py` compiles the exact
generated helper and compares it with the retained old helper, rather than
substituting a second URI implementation. Four malformed IPv6 literals with
three consecutive colons exposed another helper defect; the new IPv6 check
rejects them while IPvFuture keeps its separate permitted opaque address
grammar. All **8** targeted controls reject in the final helper; the retained
counterfactual incorrectly accepts all **8**. Its result is recorded separately
from the earlier full SDK run; the aggregate release replay uses the current
generated helper. A separate positive oracle assertion retains
`https://[v1.:::]/`: IPvFuture's permitted opaque address grammar is evaluated
before the stricter IPv6 branch.

The aggregate Swift URI checkpoint SHA-256
`cecc04508b507a447f27517485c75b9f566f2b5ba3aa5dca362bdc45602098ae`
passes the eight-negative/one-positive compiled-helper oracle. A fresh strict
owning ZIP/OPC/profile/application replay also passes all **183 OPC rows** in
**79.14 seconds**, with generated-source SHA-256
`4f03b40d703269cff99771c3660a4a0c753d389dcb6c8d40f5cbf47b58904678`.
This is byte-identical to the earlier full Swift source above. The fresh report
is retained in `swift-metadata-release/verification.json`; the helper's separate
report is under `pending-runtime-providers-completion-2026-10-21/swift-iri-final/`.

Optional Unicode logical part naming, interleaving and growth-hint production
remain explicit unsupported profile choices, documented separately from these
mandatory consumption gates. Application signature trust, business vocabularies,
network resolution and deployment policy remain application responsibilities;
this metadata reader neither verifies OPC signatures nor opens external targets.
