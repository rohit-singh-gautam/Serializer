# JavaScript and TypeScript ZIP/OPC packages

[JS/TS output](portable_languages.md#javascript-and-typescript) ·
[Package contract](../serialization/xml_packages.md) ·
[Executed qualification](../verification/verification-javascript-packages-2026-10-20.md)

Compiler **2.14.0** generates asynchronous package facades from existing schema
language **1.20.0** `package` declarations. Ordinary models, schema identifiers,
module layout and native codec bytes retain their existing behavior. Typed package
roles currently use generated XML; opaque roles carry independent bytes. Runtime
JSON/binary codec selection and generated application key/reference rules remain
outside that original 2.14 profile; language 1.21 adds the separately documented
[application key/reference contract](../serialization/package_application_contracts.md).

Compiler/runtime 2.18.0 adds `options.codecs`, keyed by schema role name. Each
entry may supply synchronous `encode(model, limits, context)` returning XML text
and/or `decode(text, limits, context)` returning the declared native model.
Omitted directions use the generated codec. Pass the supplied public `XmlLimits`
into generated `encodeXml`/`decodeXml` calls to compose selected codecs with the
same operation ledger. `context` is immutable and exposes the public `budget`,
schema `role`, actual `part` and `relationshipId`; its lifetime is the call.
Unknown/binary roles, nonfunctions, Promise results and wrong model results
reject. Callback failures retain their exact thrown value, including falsy values.
Expanded root/type dispatch precedes selected decoders, and selected encoders'
root/type is checked before staging. Save carries the same table through encoded
and reloaded validation before file publication.

Matching TypeScript output exposes the facade's typed `Codecs`, `Options`,
`LoadOptions` and `SaveOptions`, using `SerializerPackage.Codec<Model>` and
`CodecContext`. Strict consumers need no private runtime access or declaration
checking exemptions. Existing option objects and native model APIs remain valid.

Generate matching outputs together:

```powershell
serializer --input model.serializer --language js,typescript `
  --js.output model.mjs --typescript.output model.d.mts
```

The generated module exports the flattened schema package name and a support
container normally named `SerializerPackage`. If the container, its private
declaration alias or parser import alias would collide with a schema type, the
generator selects `_2`, `_3`, and so on. Both outputs record the chosen name.
Existing valid schema names are retained. The existing XML runtime's reserved
names still apply. Package modules use the same pinned `saxes`, `xmlchars` and
`@xmldom/xmldom` dependencies as generated XML; see the maintained
[dependency lock](../../test/resources/xml_shapes/typescript/package-lock.json).

## Complete typed packages

For the maintained [OPC example schema](../../example/opc/model.serializer):

```javascript
import {SerializerCompression as C, SerializerPackage as P,
  OpcExampleCatalog, OpcExampleItem, OpcExampleDocument as Document} from './model.mjs';

const provider = await C.nodeProvider();
const model = new OpcExampleCatalog();
model.title = 'Parts & tools';
const item = new OpcExampleItem();
item.id = 7;
item.label = 'Bolt <M6>';
model.items.push(item);

const input = new Document.Input({main: model, attachments: [
  new P.BinaryPart(new Uint8Array([0, 1, 254, 255]),
    '/Attachments/sample.bin', 'application/octet-stream')
]});
const bytes = await Document.encode(input, {compressionProvider: provider});
const result = await Document.load(bytes, {compressionProvider: provider});
```

The facade owns standard manifests and relationship infrastructure. Applications
supply payload values and opaque resources. A fixed typed singleton accepts its
native model directly; dynamic or repeated typed roles use
`new P.ModelPart(value, name, contentType, sourceIndex)`. Opaque roles always use
`BinaryPart`. `sourceIndex` identifies the zero-based occurrence of the declared
source role. Role cardinality applies to the complete flattened role collection.
Names declared in the schema are encoding defaults: loading discovers actual
paths through source-scoped relationships, including alternate paths.

Loading checks relationship mode, target existence, MIME identity and unique part
claims before decoding a role. It returns a fresh `Result<Value>` only after every
selected and skipped payload passes integrity checks. An optional asynchronous
`validate(candidate, budget)` runs before publication. Validators must keep the
candidate stable through their asynchronous work and throw on failure; external
relationship targets remain inert data throughout.

`encode` snapshots binary spans and eagerly encodes native XML before its first
asynchronous provider call. Keep the supplied native model graph stable during
this synchronous preparation. Returned owning arrays are independent. Borrowed
source storage must remain stable until its delivered read is copied. Do not
mutate wrapper state or share a budget across overlapping operations.

## ZIP sources, sinks and explicit Node support

`P.ZipWriter` supports `add({name, data, compression, level, forceZip64})`,
`finish()` and `writeTo(sink)`. Compression is `stored` or `deflate`; the numeric
DEFLATE level is 0–9, default 6. Writer options select `descriptor: 'none'`,
`'signed'` or `'unsigned'`, and `forceZip64`. Output sorts physical names and uses
fixed DOS timestamps, UTF-8 names, metadata and stable attribute values.

`P.ZipReader.open(source, options)` accepts independent `Uint8Array` snapshots or
borrowed random sources with a safe-integer `size` and `readAt(offsetBytes,
maxBytes)`. Short reads are accumulated; empty progress and oversized returned
chunks reject. Indexing validates ZIP32/ZIP64 directory, locator, local-header and
descriptor extents before exposing entries. `read(name)` checks the complete
payload CRC, expanded size and exact raw-DEFLATE EOF. General ZIP entry identity
is case-sensitive; OPC applies its separate normalized part identity rules.

Source and sink requests are at most **32 KiB**. A sink consumes each independent
chunk completely or throws/rejects. `Document.write(sink, input, options)` prepares
the complete archive before the first sink call. A transport failure may leave a
written prefix. No borrowed `close`, `flush` or `finish` is called. Writers need
explicit `finish()` or `writeTo()`; `close()` abandons staged state without
publishing. Reader `close()` releases private snapshots/spools. Close and first
failure fence subsequent I/O, including close/reentry inside policy callbacks.

Stored ZIP needs no Node provider. DEFLATE requires the explicit result of
`C.nodeProvider()`. Ordinary module import and browser stored-ZIP operations do
not import Node builtins. Package metadata output currently uses DEFLATE, so OPC
encoding requires that provider even when payload roles select stored entries.

Filesystem access is separate and explicit:

```javascript
const host = await P.nodeHost();
await Document.save('catalog.opc', input, {host, compressionProvider: provider});
const reopened = await Document.load('catalog.opc', {host, compressionProvider: provider});
```

`nodeHost()` alone imports Node filesystem modules. Paths require that host.
Nonseekable `{read(maxBytes)}` sources additionally require an existing
`spoolDirectory` and optional independent `maxSpoolBytes`. The host exclusively
creates private files, cleans them on success or failure, and leaves the source
open. It never extracts package names as filesystem paths.

Save encodes, reload-validates, writes an exclusive private sibling, synchronizes
the file and replaces the destination. POSIX additionally synchronizes its parent
directory. Windows has no equivalent portable directory-sync call here. Failures
before replacement preserve the original file; failures after replacement report
`publication_uncertain` with the original cause. This is qualified against
injected faults on Windows, without a claim about every filesystem's crash behavior.

## Metadata, extensions and limits

OPC metadata supports Default/Override precedence and duplicate/conflicting
identity rejection; reserved media types, scoped IDs, internal URI resolution,
external RFC3986/3987 references and Relationships markup compatibility are checked.
The compatibility configuration understands only the Relationships namespace and
implements inherited `Ignorable`, `ProcessContent`, `MustUnderstand` and ordered
Choice/Fallback selection. Metadata accepts strict UTF-8 and BOM-marked UTF-16.
DTDs, entity declarations and malformed infrastructure reject. Part names use the
documented ASCII OPC subset; otherwise valid Unicode candidates fail explicitly.
Nonfile and non-PartName physical items are excluded after integrity validation.

`ExtensionPolicy` independently selects `reject`, `skip` or `preserve` for unknown
parts and links. Synchronous `isRequiredPart` and `isRequiredRelationship` callbacks
return booleans; required unknown items always reject. Preserve keeps complete
unknown bytes, their MIME values and source-scoped authored links. The result's
extensions are attached to its value for re-encoding.

From compiler/runtime 2.19.0, the loaded `Extensions` instance carries private
origin context for typed fixed singletons discovered at a different output path.
Reuse that instance on the loaded value or a fresh `Input`. Encoding clones
preserved links and applies all fixed-role moves simultaneously, resolving each
internal target against its original source before mapping either endpoint.
The exact suffix from the first literal `?` or `#`, external target text and
unchanged relative spelling are retained. Public object fields and constructors
stay unchanged; caller extensions, byte arrays and models are not mutated.

If a moved fixed role is removed, links to its original identity reject even
when another part now occupies that URI. Canonical destination conflicts and
source-local ID collisions also reject before publication. Newly authored
extensions have no loaded-origin context. Arbitrary dynamic/repeated renames
still require caller-updated links; encoding revalidates complete closure.

`Limits` is immutable. `Budget` exposes one cumulative operation ledger; repeated
work is charged again. Defaults are 256 MiB archive, 1 GiB transport I/O, 16 MiB
metadata, 64 MiB per expanded part, 256 MiB total expanded bytes, 128 MiB live
staging, 4,096 entries, 16,384 relationships, 128 MiB decoder allocation,
512 MiB work, 65,536 references and 64 MiB preserved bytes. A deadline requires
an explicit synchronous monotonic `clock`; `checkCancelled` preserves application
exception identity. These are logical limits, not process RSS guarantees.
In-memory output is additionally limited to supported owning array extents;
random input can address ZIP64 offsets above 4 GiB without materializing the gap.

Errors expose `code`, `part`, `relationshipId` and, where available, `fieldPath`.
Application transport/cancellation failures retain their exact identity.
Native XML token/scalar work, retained namespaces and each complete identity
request share the package ledger. Language 1.21 adds fixed/default storage policy
and automatic direct scalar key/reference selectors. Supply
`{profiles: {resources: 'stored'}}` in package options to override defaults by
schema role name; fixed roles and expected XML root/type identities remain
immutable. Duplicate, missing and wrong-model references fail before publication,
with part/field/link diagnostics. TypeScript declarations expose the same options.
See [application contracts](../serialization/package_application_contracts.md).
Provider workspace and process heap peaks remain separately bounded. The
[qualification record](../verification/verification-javascript-packages-2026-10-20.md)
defines the executed scope separately from these limits.
