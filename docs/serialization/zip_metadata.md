# OPC ZIP records and explicit physical metadata replay

Serializer's package Save operations build a complete archive from the in-memory
parts and relationships. They use the complete-rewrite model expressly permitted
by ECMA-376 Part 2, fifth edition, Annex B, rather than editing individual ZIP
records in place. Logical opaque-part preservation retains part bytes and their
relationship context; it does not promise identical compressed streams, ZIP
headers, timestamps, comments, signatures or record order.

Annex B's table column for pass-through on editing applies to modification of
individual physical records. Its accompanying note permits a format to require
in-memory edits and rewriting all parts and relationships on every Save. This
distinction does not waive mandatory consumption requirements. Package readers
consume stored and raw-DEFLATE parts, signed/unsigned data descriptors, ZIP64
entry/end/locator records, comments and structurally valid opaque extra fields
within the configured resource limits. The optional central-directory digital
signature record is consumed with its data ignored; it is unrelated to OPC XML
digital signatures and provides no trust assertion.

OPC rejects split/encrypted archives and unsupported compression/flag semantics.
An OPC central-directory file header, including its fixed record, filename,
extras and comment, cannot exceed 65,535 bytes. Duplicate local/central names,
compression methods, flags and timestamps must agree; CRC and size fields follow
the descriptor exceptions. Extraction versions are validated independently:
ZIP64 offsets may require version 4.5 in a central header while its local header
still requires version 2.0. Narrow and expanded extents and descriptor values are
checked against each other and the archive bounds. ZIP64 OPC sizes/offsets must
have their high bit clear and entry counts cannot exceed 2,147,483,647.

The public C++ ZIP layer additionally exposes explicit metadata replay for callers
that need to retain inert physical attributes while building a replacement
archive. The established `zip_entry` layout is unchanged. `zip_reader` stores
noncanonical metadata separately, exposes `entry_metadata(name)` and
`archive_metadata()`, and returns shared immutable owners that can outlive the
reader and its borrowed input. A canonical entry may return a null metadata owner.

```cpp
namespace package = rohit::serializer::package;
auto shared = std::make_shared<package::budget>();
package::zip_reader reader{input, shared};
package::zip_writer writer{output, shared};
writer.set_archive_metadata(reader.archive_metadata());
for (const auto& entry : reader.entries()) {
  auto bytes = reader.read(entry.name);
  auto metadata = reader.entry_metadata(entry.name);
  package::entry_options options{};
  options.method = entry.method;
  options.force_zip64 = entry.zip64_sizes;
  options.metadata = metadata.get();
  writer.add(entry.name, bytes, options);
}
writer.finish();
```

`entry_options::metadata` is borrowed only through `add`/`write`; the writer copies
it after resource admission. Options remain a literal, trivially copyable type.
Archive metadata is shared and must remain immutable after assignment. Retained
fields are creator/extraction versions, supported general-purpose flags, DOS
times/dates, internal/external attributes, file/archive comments, opaque local
and central extra-field TLVs, descriptor signature/zero-field style and ZIP64
end-record version/extensible data. Attributes are never applied as filesystem
permissions. No extraction, filesystem path interpretation or signature
verification takes place.

CRC, compressed/expanded sizes, offsets, directory counts and ZIP64 extra-field
integers are regenerated from the actual output. Recompression can change bytes
and compressed sizes across providers. Extraction versions are raised when new
compression/ZIP64 usage requires it. ZIP directory signatures are deliberately
dropped by a rewrite because rewritten directory records invalidate them. Growth
hints and other retained opaque data are not interpreted as allocation or trust
instructions. Encrypted, unsupported or structurally malformed metadata fails
before the first local header is written when detectable from entry options;
final-directory failures leave a failed stream writer. Use staged Save when
publication must be transactional.

Metadata admission and replay use the same operation budget as input, payload
expansion, compression, output and cancellation. Explicit replay does not reset
resource counters. The independent
[Annex B qualification](../verification/verification-opc-annex-b-2026-10-21.md)
records the actual providers and tested physical fields. Relationships markup
compatibility, URI/content-type/Relationships and metadata text encoding are
qualified in the separate [eleven-output metadata replay](../verification/verification-opc-metadata-2026-10-21.md);
physical-record tests alone do not establish that complete contract.

Source: [ECMA-376, fifth-edition Part 2](https://ecma-international.org/publications-and-standards/standards/ecma-376/),
§7.3.6 and Annex B. Optional non-ASCII logical naming, piecewise interleaving and
growth-hint production remain excluded by the documented
[application responsibility and supported-profile policy](../project/application_responsibilities.md).
