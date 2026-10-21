# Generated C OPC URI and physical mapping verification

This record qualifies the generated owning C11 package profile on Windows. It
extends the [UTF-16 infrastructure replay](verification-opc-utf16-2026-10-21.md)
and [operation publication checks](verification-operation-publication-2026-10-21.md).
It supplies concrete `OPC-R08` evidence without claiming complete OPC/3MF
conformance across every graph, output or platform.

## Implemented behavior

Generated OPC loading and output now opt into inert colons in physical ZIP names;
the existing generic ZIP entry points retain their colon restriction. Part URI
validation admits the mandatory ASCII `pchar` alphabet and permitted escapes,
including `%00`, while rejecting encoded unreserved characters and separators.
Logical part identities compare ASCII case, including escape-hex spelling,
without decoding or rewriting their authored names.

The loader distinguishes exact physical ZIP identities from case-equivalent
logical part identities. Directory, device and link records, and physical names
that do not map to supported part URIs, never enter the logical catalog. Their
bytes still pass the same bounded decompression, size and CRC checks. Two
case-distinct excluded physical records are accepted; two equivalent logical
part records reject. Optional raw or escaped non-ASCII part naming returns the
explicit unavailable status.

Internal references validate their complete URI syntax before discarding query
or fragment selectors for lookup. They resolve dot segments with the native
runtime's root-clamping policy, reject scheme/authority targets and empty path
segments, and retain stored relationship spelling. Relationship sources are
recognized by their immediate case-equivalent `_rels` parent rather than any
earlier directory with that spelling. XSD whitespace collapse applies to IDs,
types, targets and override part names; MIME parameter string whitespace remains
significant.

The new bounded C URI helper validates RFC 3986/3987 components, including
userinfo, reg-name, decimal ports, IPv6/IPv4 tails and IPvFuture literals.
Relationship types are fragment-free absolute IRIs. External targets can be
relative, empty or absolute, and preserve their Unicode and percent spelling.
Private Unicode characters are accepted only in queries. These are identifier
checks; they perform no DNS, URI dereferencing or filesystem extraction. The
grammar references are [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986.html#section-3)
and [RFC 3987](https://www.rfc-editor.org/rfc/rfc3987.html#section-2.2).

## Actual SDK acceptance

`test/c_package_test.py` generates a fresh consumer and compiles it with MSVC
19.51 using C11, `/W4 /WX` and the installed release libxml2/zlib providers.
The replay passes **256 independent fixtures and 16 application fixtures**:

- The shared `package_opc_uri_corpus.py` contributes 30 naming/mapping decisions:
  mandatory reserved characters and escapes, case/hex-equivalent lookup,
  relative selectors and root-clamping, excluded physical items, their corruption and case pairs,
  logical collisions, XSD whitespace and optional Unicode boundaries.
- `package_iri_corpus.py` contributes 61 external/type/attribute decisions,
  including valid IPv6/IPvFuture and non-ASCII IRIs, empty references,
  query-only private Unicode, malformed authorities/ports/escapes, invalid IP
  literals, missing attributes and significant `TargetMode` whitespace.
- The existing 165 cases retain ZIP extent/descriptor/ZIP64, UTF-16, markup
  compatibility, MIME, selected-codec, graph and package qualification.

Every admitted URI naming fixture is encoded and loaded again, retaining its
discovered main part name. Preserved-relationship cases also complete encode/load
replay. Dedicated actual C controls show that generic ZIP reader/writer calls
still reject colons while the internal OPC policy accepts them with identical
payload bytes. Existing 68 encode and 85 load allocation-failure rollback cases,
exact operation quotas, last-poll destination rollback and atomic Save checks pass.

The executed immutable compiler is
`out/build/pending-xml-completion-2026-10-21/compiler-opc-identity/serializer.exe`,
SHA-256
`93906e87453be863a47fc29915192cf5c1fc06f7cfec07aa6a2cfdba13131e35`.
The generated consumer header SHA-256 is
`e3c944b15224b082086aeae119a28db6c90ca8ac689cb387d0256ec6e917839c`.
Commands, source hashes and final SDK logs are retained in the ignored
`out/build/pending-xml-completion-2026-10-21/c-opc-identity-final/` directory,
including `verification.json`, `commands.json`, `compile.log` and `acceptance.log`.
Native processes inherit Windows error-dialog suppression.

No Linux or sanitizer replay, arbitrary Unicode part-name support, complete
eleven-output corpus equivalence, or provider-internal peak/preemption claim is
made by this C-specific record.
