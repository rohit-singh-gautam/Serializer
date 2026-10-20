# C11 ZIP and OPC packages

Generated C packages use a collision-selected `serializer_package` prefix and the
existing native C model codecs. XML infrastructure requires libxml2. Stored ZIP
does not require a compression provider; DEFLATE requires the existing
`SERIALIZER_COMPRESSION_ZLIB=1` profile and zlib linkage. These APIs do not add a
C++ runtime dependency.

Use one generated XML-enabled header per translation unit. Common native XML
helpers currently collide when distinct generated headers are included together;
repeated inclusion of the same header is supported by its include guard.

Namespace `profile`, package `archive` generates `profile_archive_input`,
`profile_archive_value`, `profile_archive_encode`, `profile_archive_load`,
`profile_archive_read`, `profile_archive_write`, `profile_archive_save`,
`profile_archive_as_input`, and `profile_archive_free`.

Input borrows all native models, occurrence arrays, opaque bytes and optional
extensions for one synchronous operation. Value owns decoded models and arrays,
discovered names/content types and preserved resources. Zero initialization is
valid. Call `profile_archive_free` once ownership is no longer needed; that function
also resets the value to its empty state. `as_input` creates a borrowed view of a
loaded value for edit/save, retaining discovered singleton paths and relationship
source scopes. The owning Value must outlive that view.
Typed singleton `_link` fields and typed occurrence `link` fields retain loaded
relationship IDs for automatic key/reference diagnostics, including final
resolution work. Keep them with the loaded value when creating a borrowed view.

```c
profile_archive_input input = {0};
input.main = &model;
srl_buffer bytes = {0};
serializer_package_budget budget = serializer_package_default_budget();
int status = profile_archive_encode(&input, &bytes, &budget);
if (status) {
  /* budget retains the first status and bounded part/field/link diagnostics. */
}

profile_archive_value loaded = {0};
budget = serializer_package_default_budget();
status = profile_archive_load(&loaded, bytes.data, bytes.size,
                             serializer_package_preserve_policy(), &budget);
profile_archive_free(&loaded);
srl_buffer_free(&bytes);
```

Fixed singleton typed roles use a borrowed `const Model*` in Input and an owning
`Model*` in Value. Dynamic/repeated typed roles use generated occurrence structs
with a native model, name, content type and parent `source_index`. Opaque roles
use `serializer_package_binary_part` arrays. Counts and required presence are
checked before publication. Initialize load destinations and encoded buffers
before passing them to the APIs. A failed load or encode retains the old
initialized destination.
Authoring metadata pointers must name live NUL-terminated strings; payloads use
explicit byte counts. Keep borrowed arrays, callbacks and contexts alive for the
whole call. A Budget and its destinations belong to that synchronous operation;
do not mutate or reuse them concurrently or from callbacks.

Each synchronous operation uses one caller-owned Budget. Its resource slots span
ZIP bytes, metadata, entries, relationships, XML nodes/attributes/scalars, declared
application keys/references, preservation and logical allocation. The optional
`is_cancelled` callback and context apply across the operation. Original transport
and fault callback status codes are returned unchanged; native codec failures
retain their `srl_status` in `budget.native_status`. Logical admissions do not
measure hard process peaks or provider allocator overhead.

`read` spools a borrowed nonseekable callback source into bounded private memory,
with no spool file or source ownership transfer. `write` prepares the complete
archive before borrowing the callback sink and handles legal short writes. A
failed sink may retain a prefix. `save` validates the complete candidate, creates
an exclusive private stage beside the destination, writes and syncs it, then
atomically replaces the destination. UTF-8 Windows paths are converted to native
wide paths. The optional fault callback observes `before_create`, `after_write`,
`before_replace` and `after_replace`; pre-replacement errors retain the old file,
and post-replacement errors retain the complete new file. Cleanup removes only
the stage created by that operation.

The ZIP runtime supports stored/raw-DEFLATE ZIP32/ZIP64 records and signed/unsigned
32/64-bit descriptors. It validates central/local extents, overlap, duplicate
names, CRC/size and exact compressed input consumption. `budget.force_zip64`
emits real ZIP64 structures on small fixtures. Owning byte sources are bounded by
`max_archive_bytes`; there is no sparse multi-gigabyte seekable-source contract.
Multi-disk archives, encryption and other ZIP compression methods reject.
Canonical output fixes timestamps and physical metadata and orders parts and
source-local links independently of caller occurrence order.
The independent ZIP gate includes real entry counts 65,534, 65,535 and 65,536 in
both directions, including an ordinary ZIP32 count of 65,535. Locator/directory
offset and count claims outside the bounded physical source reject before
allocation. Sparse sources beyond the configured archive cap remain unsupported.

OPC names, content types, scoped IDs, source membership and internal closure are
validated before any model decoder. DTD/entity input and external resolution are
disabled; external targets remain inert strings. Unknown standard vocabulary
rejects. Optional and explicitly required relationship types have independent
reject/skip/preserve policies. Preserved opaque payload bytes and relationship
scopes are checked again on encode/Save. Dynamic role renames require explicit
application rebinding.

Schema language 1.21 role `policy(fixed)` prevents changed compression overrides;
`policy(defaults)` permits stored or DEFLATE. Supply a borrowed
`serializer_package_override` array in Budget using method `0` or `8`. Unknown
roles and unsupported methods reject before native codecs. Generated
`profile_archive_profiles` metadata records immutable expanded root/type identity;
wrong XML roots or `xsi:type` values select no decoder.

Declarative `key("scope", field)` and
`reference("scope", field, target_role)` generate direct native field access.
Selectors support public persistent strings and fixed-width integers. Complete
candidate checks reject duplicate scoped values, missing keys and target-model
mismatches, while permitting forward references and exact `uint64` boundaries.
Nested, optional/container and inherited selectors remain unsupported.
Typed payload graphs that require XML document companions for QName/list/prefix
scopes or preserved XML extension slots reject, including nested/inherited uses.
Keep those graphs outside this package profile until companion integration is
implemented; ordinary native/XML document APIs retain their existing support.
Compiler/runtime 2.18.0 admits supported XSD key/unique/keyref graphs through
operation-aware native codecs. Each complete key, unique or keyref tuple spends
one reference before equal keyrefs are deduplicated. Nested owners keep separate
indexes. The primary codec owns XML node/attribute/scalar admissions; identity
replays spend their actual additional work/allocation and tuple references.

Caller-selected codecs use the additive `serializer_package_selected_codec`
table. Its `role` uses the schema role name; `userdata` remains caller-owned,
and either direction may be null to retain the generated codec. An encoder or
decoder receives a borrowed `serializer_package_xml_context*` containing the
same package budget, role, actual part and relationship ID. Compose generated
native descriptors with `serializer_package_xml_encode` /
`serializer_package_xml_decode` and
`srl_xml_operation_codec_<native-model>()`; do not create a replacement budget
inside the callback. The callback returns zero on success or its original
nonzero integer error. The first callback/native failure fences the context.

The collision-selected entry point prefix is recorded in the generated
`Generated C selected-codec prefix` comment. With the default helper and the
example package above, these are
`serializer_package_profile_archive_load_with_codecs`,
`serializer_package_profile_archive_encode_with_codecs`,
`serializer_package_profile_archive_read_with_codecs`,
`serializer_package_profile_archive_write_with_codecs` and
`serializer_package_profile_archive_save_with_codecs`.
Their trailing arguments are the borrowed codec table and
its count. Save runs selected encode and decode validation before publication.
Existing package entry points retain their signatures and choose the same
operation-aware native codecs automatically.

For ordinary XML composition, zero-initialize `srl_xml_operation`, then supply
its synchronous `consume`, `poll` and `userdata` callbacks. Descriptor
`encode`/`decode` calls accept existing native limits and optional validation
alongside the operation. `caller_status` retains the exact first caller integer;
`failure` explicitly distinguishes native resource/allocation/semantic failures,
and `field` borrows immutable generated schema owner/member metadata. Existing
`srl_limits`, XML output/identity aggregates, model methods and function-pointer
signatures remain unchanged. Separate operations may run concurrently; one
operation and its destination require exclusive synchronous access.
Set the optional borrowed `operation.policies` to select existing independent
map/mixed/digest/identity policies through `srl_xml_operation_policies`. These
objects and callbacks must outlive the descriptor call. Existing native policy
structs and ordinary entry points retain their layouts.

Run the actual Windows C11 SDK gate from a Visual Studio developer shell:

```powershell
python -B test/c_package_test.py --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/c-packages `
  --provider-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows
```

Add `--sanitize` to compile and rerun the same generated source with AddressSanitizer.
The test injects every generated allocation failure during encode/load and checks
complete rollback and released ownership; provider-internal allocations remain
under their native provider contracts.

This selected owning profile does not close the broader official OPC/3MF corpus,
UTF-16/MCE infrastructure, all XML graph combinations or other platform gates in
[the backlog](../pending.md).
