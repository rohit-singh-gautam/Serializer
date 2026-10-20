# C# and Kotlin/JVM ZIP and OPC packages

Compiler/runtime **2.17.0** emits synchronous owning package APIs for .NET 8 and
Kotlin/JVM 17. Existing language **1.20.0** package declarations select typed XML
roles and opaque binary roles. Language **1.21.0** adds fixed/default storage
policies and declarative application keys/references; native wire formats retain
their contracts. The [original package batch](../verification/verification-pending-50-2026-10-20.md)
and [current batch](../verification/verification-pending-105-2026-10-21.md) identify
executed acceptance gates and remaining qualification.

Physical ZIP support is emitted with ordinary models. OPC XML metadata/provider
support is emitted only when the schema declares packages. C# uses
standard .NET providers; Kotlin uses standard JDK compression and hardened JAXP
providers. Ordinary XML-disabled models require no XML package provider.
The generated `SerializerPackage` helper receives a suffix on a schema-name
collision; read its discovery comment rather than reserving that name in schemas.

## Generate and compose a package

The maintained [profile schema](../../test/resources/opc_profile.serializer)
declares a fixed XML `main` role and repeated binary `attachments` sourced from
that role. Generate it with the normal compiler workflow:

```powershell
serializer --input test/resources/opc_profile.serializer --language csharp `
  --csharp.namespace SchemaGenerated --output out/example/Schema.cs
serializer --input test/resources/opc_profile.serializer --language kotlin `
  --output out/example/Schema.kt
```

C# package facades are static classes nested in the generated schema owner:

```csharp
using S = SchemaGenerated.Schema;
using P = SchemaGenerated.Schema.SerializerPackage;
using A = SchemaGenerated.Schema.ProfileArchive;

var input = new A.Input {
  Main = new S.ProfileModel { Name = "example" },
  Attachments = new[] {
    new P.BinaryPart(new byte[] { 1, 2, 3 }, "/Textures/data.bin",
      "application/octet-stream")
  }
};
byte[] bytes = A.Encode(input);
var loaded = A.Load(bytes).Value;
A.Save("model.opc", loaded);
```

Kotlin facades are top-level objects, with ordinary generated owning models:

```kotlin
val input = ProfileArchive.Input().apply {
    main = ProfileModel().apply { name = "example" }
    attachments = listOf(SerializerPackage.BinaryPart(
        byteArrayOf(1, 2, 3), "/Textures/data.bin", "application/octet-stream"))
}
val bytes = ProfileArchive.encode(input)
val loaded = ProfileArchive.load(bytes).value
ProfileArchive.save(java.nio.file.Path.of("model.opc"), loaded)
```

Repeated typed roles use `ModelPart<T>` records containing the owning model,
part name, media type and source-role occurrence index. Binary roles use
`BinaryPart`. Infrastructure names, content-type declarations and relationship
files are compiler-owned; do not insert them as application parts. Dispatch uses
source role, relationship type and media identity. All required roles must be
present before a newly decoded aggregate can be returned.

## Sources, budgets and publication

Use `ZipReader.Open` / `ZipReader.open` for physical ZIP access and dispose/close
the reader. Byte arrays are snapshotted; path input owns its file handle. Explicit
random sources (`IRandomSource` / `RandomSource`) borrow a stable length and
bounded short-read callback. Keep their contents stable for the reader lifetime.
Sequential streams require explicit `AllowSpool` / `allowSpool` and an admitted
private temporary directory/byte limit. Borrowed sources and sinks remain owned
by the caller. C# seekable streams borrow the span from their current position
through their reported length. Keep models and borrowed byte arrays stable during
synchronous calls; budgets, readers and writers require serialized use.
Kotlin requests owner-only POSIX permissions where supported. On Windows it uses
inherited directory ACLs; supply access-restricted spool and Save directories.

`ZipWriter.Add` / `add` accepts a physical entry name, bytes and `"stored"` or
`"deflate"`. `Finish` / `finish` returns independent archive bytes. ZIP32 is the
default; size/count boundaries or `ForceZip64` / `forceZip64` select ZIP64.
Readers validate the directory, local records, descriptor forms, CRC, exact
expanded size and compressed-frame completion before returning part bytes.

Pass one `Budget` across composition, typed loading, optional domain validation
and staged Save when those operations must spend one cumulative ledger. Its
`PackageLimits` / `Limits` snapshot covers I/O, metadata, expanded bytes, buffers,
entries, relationships, decoder admission, work, references and preservation.
Cancellation callbacks and monotonic deadlines are cooperative. Logical counters
bound admitted work; they do not measure exact managed-heap or provider allocation.
Owning byte-array results remain bounded by the platform's array limits even when
logical random-source offsets exceed 4 GiB.

Package `Load` / `load` constructs a fresh unpublished aggregate. Its optional
validation callback runs only on that complete candidate; throwing prevents
publication. `Write` / `write` prepares the package before borrowing the sink,
but a transport failure can leave a prefix. `Save` / `save` validates the staged
round trip, writes a private sibling, flushes it and replaces the destination.
Failures before replacement preserve the old file; failures after replacement
may report uncertain publication with a complete new file. Filesystem durability
and replacement guarantees remain host-specific.

## Extensions and limits

Unknown optional parts/relationships reject by default. Select explicit
`ExtensionPolicy` values to skip or preserve them. Required unknown capabilities
still reject. Keep the loaded `Extensions` on the aggregate when editing and
saving; preservation retains opaque bytes and relationship context, then checks
that edited internal links resolve. It does not establish vocabulary support.
Canonical output uses stable part ordering and metadata for equivalent inputs.

The selected OPC part-URI profile accepts ASCII URIs. Valid non-ASCII logical
part URIs report an explicit unsupported-feature error; external relationship
IRIs preserve Unicode. This boundary is separate from XML Unicode names and
physical ZIP filename decoding.

Generated profiles use the declared XML codec and validate expanded root/type
identity before model decoding. Input `ProfileOverrides` / `profileOverrides`
maps schema role names to `"stored"` or `"deflate"`; facade `SelectProfiles` /
`selectProfiles` returns resolved metadata. A fixed policy rejects changed
storage on input and output. Generated scoped keys/references validate the complete
candidate before domain callbacks and publication; follow the
[application contract](../serialization/package_application_contracts.md).
Wider vocabulary/platform qualification remains in [pending requirements](../pending.md).
Use complete-aggregate validation for additional application invariants. See the
[shared XML/package contract](../serialization/xml_packages.md) for unsupported
XML graph combinations and namespace/preservation limits.
