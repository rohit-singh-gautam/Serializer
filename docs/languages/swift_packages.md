# Swift ZIP and OPC packages

[Usage guide](../guides/usage.md) · [OPC design](../design/opc_3mf_support.md) ·
[Qualification](../verification/verification-q02-q08-2026-10-21.md)

Serializer 2.17.0 generates owning Swift ZIP support and schema-bound OPC
aggregates. Schema language 1.21.0 adds package profile policies and direct scalar
key/reference selectors. Existing package declarations retain their meaning.
Generated XML model types and ordinary native wire formats remain independent of
the package container.

## Declare and use an aggregate

```serializer
serializer version 1.21.0;
namespace sample {
  xml(namespace("urn:serializer:swift-package"), scalars(attribute));
  class model {
    public uint32 id;
    public string label;
  }
  package archive {
    part main : model {
      name("/model.xml");
      content_type("application/vnd.serializer.model+xml");
      relationship("urn:serializer:main");
      compression(deflate);
      policy(defaults);
      key("models", id);
    }
    part attachments : bytes {
      name(dynamic);
      content_type(dynamic);
      relationship("urn:serializer:attachment");
      source(main);
      occurs(0, unbounded);
    }
  }
}
```

```swift
var model = SampleModel()
model.id = 7
model.label = "saved model"
var input = SampleArchive.Input()
input.main = model
input.attachments = [SerializerPackage.BinaryPart(
    [0, 1, 254, 255], name: "/Attachments/raw.bin",
    contentType: "application/octet-stream")]
input.profiles["main"] = .stored
let bytes = try SampleArchive.encode(input)
let loaded = try SampleArchive.decode(bytes)
try SampleArchive.save(loaded, to: destinationURL)
```

The generated helper normally has the name `SerializerPackage`. An existing
schema type with that name selects `SerializerPackage_2` or the next available
suffix. The generated source identifies its selected helper in a comment.
Schema roles keep their generated input-property names. Additional `extensions`
and `profiles` properties receive a numeric suffix when a role already uses the
same name. Profile dictionaries use original schema role names, including
underscores, independently of Swift property presentation.

Typed singleton roles use optional native model values. Repeated/dynamic typed
roles use `ModelPart<T>`; opaque roles use `BinaryPart`. Both retain the actual
part name, content type and source occurrence index. A fixed output path is a
canonical Save destination; input discovery follows source-scoped relationship
type and content type.

## Profiles, identities and publication

`policy(defaults)` permits a per-role stored/DEFLATE override through `profiles`.
`policy(fixed)` rejects changed storage and incompatible input compression.
Neither policy permits replacing a role's expected XML root or native model
type. Matching `xsi:type` values compare resolved namespace/local identities,
including equivalent prefixes. Wrong, malformed or unbound type identities
reject before entering the typed codec callback. This package-only behavior does
not relax ordinary native XML attribute handling.

`key("scope", field)` and `reference("scope", field, target_role)` select direct
public persistent string or fixed-width integer members. Optional, collection,
inherited and nested selectors remain generation errors. One key scope has one
primitive scalar type. The generated aggregate collects keys/references before
encoding and after decoding. Duplicate keys, missing targets and wrong model
types reject before returning a complete archive or aggregate. A target in
another role can satisfy a reference when its model identity and scope agree.
Scope and string-key identities compare exact UTF-8 bytes, including distinct
canonically equivalent Unicode spellings.

`ExtensionPolicy` selects independent reject/skip/preserve policies for unknown
parts and relationships. Required-capability callbacks can force rejection even
when optional items would be skipped or preserved. Unknown core metadata still
rejects. Preserved parts retain their exact opaque bytes and complete XML
namespace declarations. Preserved relationships retain source/target context;
loaded typed roles adopting canonical paths retarget those retained links before
Save. In 2.19.0, loaded extensions retain original relationship context and private
provenance only for fixed singleton roles whose discovered spelling differs from
the canonical output path. Keep the loaded `Extensions` with the edited input;
constructing a replacement from its public arrays does not copy that provenance.
Each internal target resolves against its original source before simultaneous
source/target rebinding. Raw query/fragment suffixes, external URI references and
unmoved target spelling remain unchanged, as do opaque bytes and caller-owned
records. Removed moved roles retain tombstones: old links reject even when a swap
puts another role at the removed origin. Occupied destinations, ambiguous origins,
source-local ID collisions and dangling edited links reject before publication.
Dynamic role renames require explicit application rebinding. See the
[independent relocation checks](../verification/verification-package-relocation-2026-10-21.md).

## ZIP, ownership and limits

`ZipReader.open` accepts owned bytes or a borrowed `Source` with an exact extent
and positional reads. `ZipWriter.add`, `finish` and `write(to:)` stage complete
archives. Stored/raw-DEFLATE, ZIP32, ZIP64 entries and directories, and signed or
unsigned 32/64-bit descriptors are checked against central/local metadata,
expanded size, exact DEFLATE extent and CRC. Output sorts entries and emits
fixed timestamps and deterministic metadata. Unicode ZIP names compare exact
UTF-8 identities. OPC payload names use the documented selected ASCII URI
profile.

One `Budget` snapshots limits and carries cumulative I/O, metadata, expanded
bytes, entry/relationship/reference counts, allocation admission, preservation
and work through ZIP, package infrastructure and generated XML callbacks.
`CodecContext` identifies a role, part and relationship; field-level resource
failures retain that context. Cancellation/deadline checks are cooperative, and
original policy and transport failures remain observable. Reentrant or failed
operations cannot resume.
Physical relationships, key selectors and request selectors share reference
admission; final reference resolution also consumes work. Load diagnostics keep
the actual discovered part and authored relationship ID, including singleton
roles with a different canonical Save path.
Completed native XSD key, unique and keyref tuples each consume reference
admission before duplicate keyrefs are folded. Native XML quota failures retain
the package resource category; malformed XML and schema facets remain codec
failures.

Sources and sinks remain caller-owned. Swift value snapshots preserve inputs
when callers subsequently mutate arrays or models. Reader storage belongs to
the reader until `close` or destruction; returned model and opaque values own
their logical contents independently. Operations are synchronous and require
external synchronization when shared between threads.

Nonseekable `SequentialSource` input requires `Limits.allowSpool`. It uses a
bounded private temporary directory and removes owned spool files on success
and failure. Opening the spool still creates an owning archive buffer. These
are logical byte/allocation admission limits; they do not impose a measured
process-memory ceiling or preempt provider-internal work.

`save(to:)` validates a complete staged archive before Foundation's
same-directory atomic replacement. Failures leave the previous file or a
complete new file; a cancellation after replacement can report failure after the
new file becomes visible. This API does not promise durable directory syncing.
Borrowed sink writes may leave a prefix on transport failure and do not provide
the file replacement guarantee.

Compiler/runtime 2.22.0 package facades select an OPC-specific inert-name policy;
ordinary `ZipReader.open`, `ZipWriter` and generic `save` retain their path checks.
Direct OPC consumers can select `ZipReader.openOpc` and then validate `Opc`;
generated aggregate publication selects `saveOpc` automatically. Relationship
targets remain authored IRI text, with no URL normalization or network access.
Fifth-edition namespace-scoped compatibility preprocessing precedes Relationships
core validation. See the [common metadata qualification](../verification/verification-opc-metadata-2026-10-21.md).

Encryption, multidisk archives, unsupported ZIP methods, self-extracting prefixes
and unindexed local data reject explicitly. Broader
3MF conformance, signatures, lexical metadata preservation, streaming without
owning archive buffers and hard provider peak/preemption contracts remain
separate pending requirements. Qualification records distinguish actual SDK
runs from those broader claims.

The maintained `test/swift_package_test.py` acceptance runner generates and
strictly compiles three Swift applications, then checks their ZIP/OPC output
with independent Python readers. The package acceptance recorded for this
release uses Swift 6.4 on Windows x86-64. The separate Windows/Linux Q08 replay
qualifies the selected owning XML profiles; it does not extend that package
acceptance to other platforms.
