# Rust ZIP and OPC packages

Generated owning Rust packages use a collision-selected `serializer_package`
module. Native model structs and ordinary JSON/binary bytes retain their existing
contract. XML packages require the same pinned `roxmltree` and `xmlparser`
dependencies as ordinary Rust XML output; ZIP and DEFLATE add no dependencies.
Loaded `reference_links` retain role/occurrence relationship IDs for automatic
key/reference diagnostics, alongside discovered singleton `paths`.

A schema `package archive` in namespace `profile` generates `ProfileArchive`,
`ProfileArchiveInput`, and `ProfileArchiveValue`. Input and Value name the same
owning aggregate: fixed singleton typed roles are `Option<Model>`, dynamic typed
roles are `Vec<ModelPart<Model>>`, and opaque roles are `Vec<BinaryPart>`.
Required presence and cardinality are checked before publication.

```rust
use generated::serializer_package as package;

let input = ProfileArchiveInput {
    main: Some(model),
    ..Default::default()
};
let bytes = ProfileArchive::encode(&input, &mut package::Budget::default())?;
let loaded = ProfileArchive::load(
    &bytes,
    &mut package::Budget::default(),
    package::ExtensionPolicy::preserve(),
)?;
ProfileArchive::save(
    std::path::Path::new("model.opc"),
    &loaded,
    &mut package::Budget::default(),
    None,
)?;
```

Use a fresh `Budget` for each operation. `used` records cumulative admissions;
`limits.maximum[Resource::References as usize]` and the other named slots set
shared limits. The ledger spans physical/expanded ZIP bytes, metadata, directory
entries, relationships, XML nodes/attributes/scalars, declared application keys
and references, preservation and logical allocation. Root/type dispatch precedes
the selected decoder; operation-aware native codecs own primary XML accounting.
Allocator overhead and native container spare capacity are not hard process-memory
guarantees. Cancellation is a borrowed `Fn() -> Result<()>`; original cancellation,
native and I/O error objects retain their concrete types through `Arc` handles.

`read` accepts a borrowed nonseekable `Read` and uses bounded private memory
spooling. It creates no spool file, does not close the source and observes legal
short reads. `write` prepares the complete archive before borrowing a `Write`;
a transport failure can leave a prefix in that sink. `save` validates the complete
candidate, creates a new private staging file beside the destination, syncs it,
and atomically replaces the destination. Its optional fault callback exposes
`before_create`, `after_write`, `before_replace` and `after_replace`. Failures before
replacement retain the old file; a failure after replacement retains the complete
new file. Staging cleanup never removes a caller-owned path.

ZIP32/ZIP64 stored and DEFLATE records, signed/unsigned 32/64-bit descriptors,
CRC/size/exact compressed consumption and checked directory/local extents are
supported. `Budget::force_zip64` emits genuine ZIP64 structures for small fixtures.
Input is an owning bounded-memory profile, so it does not provide sparse
multi-gigabyte random-access sources. Multi-disk archives and encrypted/other
compression methods reject. Output orders entries and standard metadata
deterministically, with fixed 1980-01-01 timestamps and ordinary file attributes.
The independent gate reads and writes real entry counts 65,534, 65,535 and 65,536,
including a ZIP32 count of 65,535 without a ZIP64 locator. Large locator/directory
offset/count claims outside the bounded physical source reject before allocation.

OPC infrastructure is parsed with exact expanded namespaces and no DTD, entity
resolver or external fetch. Names, content types, scoped relationship IDs, source
membership and internal target closure are checked before typed decoding.
Unknown core markup rejects. Optional and explicitly required relationship types
have separate reject/skip/preserve policies. Preserved payload bytes and original
source-local target strings are revalidated during encoding. `Value::paths`
retains discovered singleton paths, allowing a loaded package to keep its relative
relationship scopes. Dynamic role renames still require application rebinding.

Schema language 1.21 adds `policy(fixed)` and `policy(defaults)` to package roles.
Set `budget.overrides.insert("main".into(), 0)` for stored ZIP or `8` for DEFLATE.
Defaults permit either supported override; fixed rejects a changed method before
native encoding. Unknown role overrides reject. `profiles()` exposes immutable
role, model/root and expected-type metadata. XML roots and any `xsi:type` are
checked by expanded identity before selecting a model decoder.

Declarative `key("scope", field)` and
`reference("scope", field, target_role)` select direct public persistent string
or fixed-width integer fields. Generated adapters collect the complete aggregate,
then check scoped uniqueness and the target role's exact model type before encode/load
publication. Forward references are valid; duplicate, missing and wrong-model
keys reject. Numeric keys use exact native decimal values, including `uint64`
boundaries. Nested paths, optional/container selectors and inherited selectors are
not supported by this contract.
Typed package graphs requiring document companions for QName/list/prefix scopes
or preserved XML extension slots reject, including nested/inherited uses. Existing
ordinary native and XML document-companion APIs retain their support separately.
Compiler/runtime 2.18.0 admits supported XSD key/unique/keyref package graphs.
Each complete tuple spends one reference before equal keyrefs are deduplicated;
nested owners retain independent indexes. Identity replays add their actual
work/allocation and tuple counts without charging primary XML events again.

`encode_with_codecs` and `load_with_codecs` accept one typed `FnMut` argument
per typed role, in `profiles()` order. Encoders receive `&Model`; decoders receive
`&str`. Both also borrow `&mut package::XmlOperation`, whose public `budget`,
`role`, `part` and `link` identify the same synchronous operation. Use
`context.encode(model)` / `context.decode(text)` to compose selected generated
codecs with their automatic native policies and shared counters:

```rust
// This example has one typed role; additional typed roles add callback arguments.
let loaded = ProfileArchive::load_with_codecs(
    &bytes,
    &mut package::Budget::default(),
    package::ExtensionPolicy::preserve(),
    |text, context| context.decode(text),
)?;
```

The corresponding `read_with_codecs`, `write_with_codecs` and `save_with_codecs`
carry the same callbacks through source/sink/full-Save handling. Save takes all
encoder callbacks followed by all decoder callbacks and validates before file
publication. Callbacks return `package::Result`; the first caller error retains
the identical `Arc`, including cancellation. Native resource and semantic errors
retain actual part/member/link diagnostics. Callbacks and their captures remain
borrowed for the call and must preserve the selected model's codec contract.

Ordinary XML callers use the collision-selected `serializer_xml_operation`
module (see its generated marker), with `Operation`, `Options` and qualified
`Codec` trait calls. `Options` composes existing XML/map/mixed/digest/identity
limits and an optional validator. Existing `XmlLimits` struct literals,
`Error(pub &'static str)`, inherent methods and their `Result` signatures remain
valid. Each native operation borrows its own context; there is no global or
thread-local mutable ledger.

Run actual SDK and independent producer/consumer checks with:

```powershell
python -B test/rust_package_test.py --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/rust-packages --cargo-home out/build/cargo-home
```

The selected Windows Rust SDK gate covers these package work units. It does not
close the broader official OPC/3MF corpus, UTF-16 infrastructure, markup
compatibility, all XML graph combinations or other platform gates in
[the pending backlog](../pending.md).
