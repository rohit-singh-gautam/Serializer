# Java ZIP and OPC packages

Compiler/runtime 2.14.0 adds generated Java package facades for existing schema
language 1.20.0 `package` declarations. The core uses Java 17 standard APIs and
requires no package or compression jars. Typed roles call the existing generated
XML model codecs; opaque roles retain complete byte arrays. The
[shared package contract](../serialization/xml_packages.md) describes schema
roles, part URIs and relationship routing.

Generate a schema containing package declarations normally:

```powershell
serializer --input model.serializer --language java --output Schema.java
javac --release 17 Schema.java Application.java
```

The generated outer class contains a `SerializerPackage` helper. When that name
conflicts with an existing generated model, the compiler selects a free suffixed
name and records it in the generated comment. Existing native model names and
wire APIs stay unchanged. Package facades are nested alongside model classes in
their schema namespace. A package named `archive` in namespace `profile` normally
becomes `Schema.Profile.Archive`.

## Typed profiles

Each facade has an `Input`, an owning decoded `Value`, and static `encode`,
`write`, `load` and `save` methods. Nested `Input`/`Value` names also avoid enclosing
class collisions; use the names in the generated source. For the repository's
[example schema](../../test/resources/opc_profile.serializer):

```java
var input = new Schema.Profile.Archive.Input();
input.main = new Schema.Profile.Model();
input.main.name = "sample";
input.attachments = java.util.List.of(
    new Schema.SerializerPackage.BinaryPart(
        new byte[] {1, 2, 3}, "/assets/data.bin", "application/octet-stream"));

byte[] archive = Schema.Profile.Archive.encode(input);
var loaded = Schema.Profile.Archive.load(archive);
Schema.Profile.Model model = loaded.value.main;
```

Fixed singleton typed roles accept the generated model directly. Dynamic or
repeated typed roles use `ModelPart<T>(value, name, contentType, sourceIndex)`;
opaque roles use `BinaryPart(data, name, contentType, sourceIndex, compression)`.
`sourceIndex` selects the zero-based occurrence of the declared parent role. A
role's authored fixed name and media type remain writer defaults. Load discovers
the actual target through the declared source, relationship type and media type,
so an equivalent input can use a different valid part name.

An input borrows application model and byte values for its synchronous call.
Callers must keep them stable during that call. Load creates fresh models and
byte arrays and returns only after all declared cardinalities, relationship
membership, selected model codecs and unknown-capability policy checks pass.
The optional validator sees the complete aggregate, including preserved extension
state. A validator failure publishes no result.

## Limits and unknown capabilities

Construct `new SerializerPackage.Budget(limits)` to snapshot a mutable
`SerializerPackage.Limits`. Pass the same budget to every stage of one operation;
Save uses it for composition, staged round-trip validation and publication.
Limits cover archive/I/O/metadata/part/expanded bytes, entries, relationships,
references, work, preserved bytes and logical internal buffer storage. Decoder
allocation charges are conservative estimates for the package runtime. These
ledgers do not measure JVM heap, spare collection capacity, parser/native-provider
storage or process RSS.

`checkCancelled` is a synchronous `Runnable`; `deadlineNanos` uses
`System.nanoTime()`. Policy, transport and model boundaries check cancellation
before continuing or publishing. Wrapper close/reentry and the first failure
fence subsequent work. Caller callbacks should return promptly; synchronous
native model codec calls cannot be interrupted in the middle of a callback.

Unknown parts and links reject by default. An explicit `ExtensionPolicy` selects
`Unknown.REJECT`, `SKIP` or `PRESERVE` independently for whole parts and
relationships. Required-capability predicates reject regardless of that optional
selection. Preserved `Extensions` retain complete payload bytes and source-local
link context; they do not claim that an unknown required vocabulary is understood.
Skipped physical entries still undergo size, framing and CRC validation.
External relationship targets remain inert URI/IRI strings and are never opened.
From compiler/runtime 2.19.0, loaded `Extensions` privately retain the original
part identity of a typed fixed singleton when its discovered name differs from
the declared output name. Keep that same `Extensions` instance when re-encoding
the loaded value or binding its models to a fresh `Input`. Composition retargets
preserved sources and internal endpoints simultaneously, resolving every target
against its original source first. It retains the exact suffix beginning at the
first literal `?` or `#`, external target text and unchanged relative spelling.
The supplied extension parts, links and models are not mutated.

Removing a moved fixed role rejects preserved links that still use its original
identity, even when another output part occupies that URI. Occupied canonical
destinations, conflicting mappings and source-local ID collisions also reject
before publication. Newly authored extension state has no loaded-origin context.
Arbitrary renaming of dynamic/repeated occurrences remains outside automatic
retargeting; callers must update those links and satisfy the final closure check.

## ZIP and source ownership

`ZipReader.open(source, budget)` accepts a snapshotted `byte[]`, a `Path`, a
borrowed `RandomSource`, or an explicitly spooled borrowed `InputStream`.
`RandomSource` implements `size()` and `readAt(offset, maximum)`; legal short reads
are accumulated and empty/oversized reads reject. Borrowed requests are at most
32 KiB. The reader closes only channels opened from `Path` and private staging.

Sequential input requires `limits.allowSpool = true` and an explicit
`limits.spoolDirectory`, with a separate `maxSpoolBytes` ceiling. Private files
are deleted on success, failure or close. Entry names never become filesystem
paths; the runtime performs no extraction.
Each sequential request also fits the remaining I/O budget. Checking true EOF
requires capacity for a one-byte request; a zero-byte EOF response consumes no
additional I/O bytes.

ZIP reading checks ZIP32/ZIP64 extents, directory/local-record agreement,
signed/unsigned descriptors, exact raw DEFLATE extent, expanded size and CRC.
Stored and raw-DEFLATE entries are supported. Encryption, multidisk and other
methods reject. Non-files, directories and malformed non-part names are excluded
from OPC mapping after their physical payload is validated. Otherwise valid-shaped
non-ASCII or UTF8-percent-escaped part candidates explicitly fail with
`Code.UNSUPPORTED_FEATURE`; they are never silently ignored. Content-type XML,
source-local relationship IDs, internal targets and
Relationships markup compatibility processing use their standard OPC namespaces.
Metadata accepts strict UTF8 or BOM-marked UTF16 with a matching XML1.0 declaration;
DTDs, external entities and XInclude are unavailable.

`ZipWriter.add(name, bytes, "stored" | "deflate")` snapshots inputs and
`finish()` returns a complete owning archive. `forceZip64` and `descriptor`
select standard framing; no application envelope is added. `writeTo` and profile
`write` borrow an `OutputStream` after complete preparation. They do not close or
flush it; a failed sink can retain a prefix. Archive output and individual
decoded parts currently require Java arrays, so ZIP64 framing alone does not
provide a larger-than-array streaming output path.

## Save and diagnostics

Profile `save(path, input, budget, validator, fault)` encodes and reloads staged
bytes before creating a file. It writes a private sibling, synchronizes that
file, then requests atomic replacement of the caller-selected destination. The
destination directory is synchronized where the platform exposes POSIX directory
channels. Atomic-move unavailability fails before replacement. Pre-replacement
failure preserves the previous file and removes staging; failure after replacement
reports `Code.PUBLICATION_UNCERTAIN` with the original cause.

`PackageError` extends `IOException` and exposes `code`, `part`, `offsetBytes`,
`fieldPath`, `reference` and `relationshipId`. Diagnostics contain logical context
without input payload excerpts. Transport/cancellation application failures keep
their original identity. Later operations on a failed/closed wrapper use an
inactive-state `IOException` with its retained cause. Optional `SaveFault` hooks
name publication boundaries for deterministic fault testing.

## Current scope

Selected typed package roles use XML. Other authored typed codecs reject before
generated output publication. Nested XML token/scalar work, retained namespaces,
identity tuples and declared application references share the package ledger.
Native codec failures retain the role/field, actual part and relationship ID.
Each complete repeated keyref request consumes admission before deduplication.

Language 1.21 adds `policy(fixed)`/`policy(defaults)`, `key("scope", field)` and
`reference("scope", field, target_role)`. Pass a schema-role-keyed map to
`Archive.encode(input, budget, Map.of("resources", "stored"))` to select storage
defaults; fixed roles reject changed methods. Root and explicit `xsi:type`
identities remain immutable. Automatic scalar references validate the complete
unpublished aggregate, including forward requests and wrong-model targets.
See [application contracts](../serialization/package_application_contracts.md)
for selector restrictions. Provider internals and JVM heap peaks remain outside
the logical operation budget.

The [20 October qualification](../verification/verification-java-packages-2026-10-20.md)
records **224/224** actual JDK17 decisions, nine independent ZIP/XML output checks,
two C++/Java typed exchanges and helper/outer-class collision profiles. The same
Java 17 classfiles pass **224/224** on isolated Linux OpenJDK 25, including Save
fault cases. The exact SDK/platform scopes and remaining framework limits are
recorded separately.
