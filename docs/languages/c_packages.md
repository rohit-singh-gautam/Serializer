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

OPC part names support the required ASCII URI character set, including inert
colons and permitted percent escapes. Part identity compares ASCII case and
escape-hex spelling without rewriting stored names. Internal targets resolve dot
segments and selectors for part lookup; stored relationship spelling remains
intact. External targets use component-aware RFC 3986/3987 validation, including
IPv6/IPvFuture authorities and query-only private Unicode characters, and never
perform network access. Relationship types require a fragment-free absolute IRI.
The generic ZIP reader/writer retains its existing colon restriction.

Directory/device/link records and physical names that do not map to supported
OPC part URIs are excluded from the logical part catalog, while their payloads
still undergo bounded decompression and CRC checks. Exact physical ZIP names and
case-equivalent logical part identities are checked separately. Optional raw or
percent-encoded non-ASCII part naming reports the unavailable status. XML schema
whitespace collapse applies to relationship IDs, types, targets and override part
names; quoted MIME parameter whitespace remains significant. See the
[C URI and mapping replay](../verification/verification-c-opc-uri-2026-10-21.md).

Release 2.19.0 adds opt-in canonical fixed-singleton output. Existing `encode`,
`write`, `save` and their selected-codec forms retain discovered `_path` values
and their previous output behavior. Use the collision-selected entry point
prefix recorded by `Generated C selected-codec prefix` for the new
`_encode_canonical`, `_write_canonical` and `_save_canonical` functions. For the
example package, encoding a borrowed view of a live loaded Value is:

```c
const profile_archive_input edited = profile_archive_as_input(&loaded);
budget = serializer_package_default_budget();
status = serializer_package_profile_archive_encode_canonical(&edited, &bytes, &budget);
```

The new functions accept the same arguments as the corresponding ordinary
functions. Their `_with_codecs` variants append the borrowed codec table and
count. Canonical output borrows the typed models, routes present fixed singleton
roles to their declared paths, and copies preserved links into private output
ownership. It resolves each internal target against its original source before
mapping either endpoint. Moves are simultaneous, including swaps; raw query and
fragment suffixes remain exact, and relative or absolute external targets remain
inert unchanged text. Unmoved links keep their original relative spelling.

Keep loaded `_path` metadata when removing an optional role. A moved origin for
that absent role remains a tombstone: a preserved source or target referring to
it rejects even if another role moves into its old path. Unchanged role paths do
not acquire tombstones. Unknown part bytes, the borrowed input and native model
payloads remain unchanged. Conflicting origins, occupied destinations, duplicate
source-local IDs and dangling references reject before publishing an aggregate,
buffer or file. Routing metadata, copies and lookup work use the same budget and
cancellation callbacks; an empty move set uses the existing composition path.
These APIs preserve the old public aggregate layouts and relationship-ID rules.
Arbitrary renames of dynamic occurrences remain an application responsibility.

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
Typed payload graphs with QName/list/prefix scopes or preserved XML extension
slots compose through per-part XML companions, including supported nested and
inherited graphs. Keep the aggregate-owned sidecars with their native parts;
`as_input` borrows both for encoding. See [Profile pipelines and XML companions](#profile-pipelines-and-xml-companions)
for ownership, selected codecs and runtime envelope selection.
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

Content-type and Relationships infrastructure accepts strict UTF-8 or BOM-marked
UTF-16 in either byte order. A declaration must be absent or identify `UTF-16`;
conflicting labels, UTF-32, malformed scalars, DTDs and XML1.1 reject. Infrastructure
output remains UTF-8, and ordinary generated typed XML retains its UTF-8 contract.
The metadata ledger counts original encoded bytes. See the independent
[C/Rust encoding replay](../verification/verification-opc-utf16-2026-10-21.md).

Relationships metadata applies fifth-edition namespace-scoped Ignorable,
ProcessContent, MustUnderstand and ordered Choice/Fallback preprocessing before
core validation; see the [common metadata corpus](../verification/verification-opc-metadata-2026-10-21.md).
This selected owning profile does not close the broader official 3MF corpus,
all XML graph combinations or other platform gates in
[the backlog](../pending.md).

## Profile pipelines and XML companions

Version 2.28 composes the [generated profile pipeline](../serialization/xml_packages.md#generated-package-pipelines)
with namespace/foreign XML graph roles. Singleton Value `_srl_xml_<role>` and
repeated part `xml_state` pointers own presentation independently from native
models; `as_input` borrows them. Free the aggregate once, after all borrowed
inputs finish. Keep whole repeated occurrences together during reordering.
Selected document callbacks prepare XML presentation without constructing the
model first and encode the exact borrowed payload under the supplied operation.

Selected helpers `_encode_renamed_with_profiles` and `_save_renamed_with_profiles`
combine explicit dynamic/repeated rename provenance with runtime envelope
selection. Old APIs and model/role/budget layouts retain their contracts. XML
sidecars never become native JSON/BINARY_NONE fields. Preservation callbacks
share the same operation and admit owned bytes before capture. See the
[actual SDK and archive checks](../verification/verification-package-pipelines-2026-10-22.md).
