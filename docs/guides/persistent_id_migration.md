# C++ persistent-ID encoding migration

[Application and build guides](README.md) | [2.7 release migration](../../migration.md#compilerruntime-270)

Compiler/runtime **2.7.0** defaults generated C++ `persistent_id` payloads to
`compact_varint`. This changes their binary representation, while preserving the
numeric ID, configured `uint32`/`uint64` storage type and allocation limit.
`[managed] id_encoding = fixed` selects width-based payloads instead. Encoding is
independent of the direct/separated managed representation.

The policy applies to `binary_none`, `binary_integer` and `binary_string`.
`compact_varint` uses canonical unsigned LEB128; `fixed` uses fixed-width
little-endian payloads. JSON IDs remain numbers. A typed `managed_ref<T>` uses
the selected policy for its object-ID component; its document namespace halves
and uint64 class discriminator remain fixed-width. Ordinary application integer
fields, collection counts and `binary_integer` field tags keep their existing
contracts. See the [wire-format contract](../serialization/wire_format.md#generated-c-managed-id-encoding).

## Select and regenerate

Use the compact default for new documents. Make the policy explicit in a shared
generator INI when several targets or applications use the same model:

```ini
[managed]
id_encoding = compact_varint
```

Regenerate the C++ headers, then rebuild all consumers with the matching runtime:

```sh
serializer --input model.serializer --config model.ini --language cpp --output generated/model.hpp
```

A command-line override takes precedence over the INI:

```sh
serializer --input model.serializer --config model.ini --language cpp --output generated/model.hpp --managed.id_encoding compact_varint
```

To select width-based payloads, use `id_encoding = fixed` in the INI or replace
the final override with `--managed.id_encoding fixed`. Keep the existing
`id_type`, `separate_values`, schema field identities and class discriminators
when changing only encoding. This option requires no schema-language change;
language 1.20.0 is required separately for `managed_ref<T>` declarations.

Use the same policy throughout each generated include graph, all translation
units and cooperating participants. Generated managed schema identities
distinguish the policies. Raw model codecs have no document envelope identifying
the schema or encoding: their reader must already know both. See
[compiler options](command_line.md) and [CMake generation](cmake_integration.md).

## Build the reusable migration tool

Changing an INI does not convert stored data. Use the schema-specific converter
for an explicit offline conversion. In a consumer project containing
`model.serializer` and `model.ini`, add:

```cmake
cmake_minimum_required(VERSION 4.0)
project(model_migration LANGUAGES CXX)
find_package(Serializer 2.7 CONFIG REQUIRED)

serializer_add_migration_tool(
  TARGET model_migrate
  SCHEMA model.serializer
  ROOT application::document
  CONFIG model.ini)
```

Replace `application::document` with the concrete root class name in the
generated C++ API. The helper generates the entry point and model headers and
links `Serializer::managed`; no handwritten C++ driver or Python is needed.
It also works with a source dependency added through `add_subdirectory`.
Serializer must have `SERIALIZER_BUILD_MANAGED=ON`.
Build the tool from regenerated current headers: managed traits without the
`id_encoding` metadata reject before decoding because their older codecs cannot
honor policy overrides. This guard checks generated code, not the age of a fixed
input file whose model and supported container match.

```sh
cmake -S . -B out/build/migration -DCMAKE_PREFIX_PATH=/path/to/serializer
cmake --build out/build/migration --config Release --target model_migrate
```

The commands below use `model_migrate` for the resulting executable. Supply its
actual path, including `Release/` for a multi-configuration build and `.exe` on
Windows. See the [runnable migration example](../../example/migration/README.md)
for a complete project.
On Windows, make the enabled dependency DLLs available on `PATH` when running
the installed compiler and migration executable.

## Convert a managed save

```sh
model_migrate --kind managed --from fixed --to compact_varint --input old.save --output new.save
```

Managed conversion preserves the document namespace, every object ID, allocation
high-water mark and authority lease ledger. It converts the current snapshot and
every retained history snapshot, keeping labels, cursors, revision numbers and
tree ancestry. History-disabled, linear and tree saves select their supported
native history/label shape from the container. The tool validates the complete
candidate before publishing it; it does not recreate entities through constructors
or allocate replacement IDs. Typed references retain their targets.

Both `--from` and `--to` are required. Swap their values for compact-to-fixed
conversion. The generated root, ID width and managed representation must match
the source model; this converter changes encoding, not schema layout or class
discriminators. The managed container uses its native binary codec and rejects
raw-format flags. Encoding itself is never autodetected.

## Convert a raw model

```sh
model_migrate --kind raw --codec binary_integer --from fixed --to compact_varint --input old.model --output new.model
```

Raw conversion supports ordinary generated models as well as generated managed
storage. Select the known raw codec with `--codec binary_none`,
`--codec binary_integer` or `--codec binary_string`. The default is
`binary_integer`. To change codec at the same time, replace `--codec` with
`--input-format binary_none --output-format binary_string`, for example.
Raw payloads do not carry their root schema or encoding; the tool must be built
for the correct root and told the correct source policy.

A raw model contains neither document-history metadata nor allocator lease state.
Converting one does not turn it into a managed save. Keep those external records
with their owning application instead of inventing replacement identities.

## Bounds, publication and cutover

The CLI accepts positive byte limits through `--max-input-bytes`,
`--max-output-bytes`, `--max-snapshot-bytes`, `--max-history-bytes` and
`--max-working-bytes`. Defaults are 64 MiB each for input, output and snapshots,
256 MiB for history and 512 MiB for migration staging/cache accounting. The
managed runtime's decoder limits and 1,024-revision default also apply. The
library exposes `migration::migrate_save<Root>` and `migrate_raw<Root>` in
`<rohit/managed_migration.hpp>` for callers supplying `migration::options`.

Input and output must be distinct, and the destination must not exist. The tool
writes a same-directory temporary file, syncs it and publishes without replacing
an existing destination. It returns zero on success and reports errors to stderr
with a nonzero exit status. A directory-sync failure after publication reports
uncertain durability and retains the published output.

Stop writers and convert a bounded, consistent checkpoint. Keep the original
until the application cutover is complete, then restart all readers/writers with
the same selected policy. Journals, collaborative client checkpoints and pending
session requests are explicitly unsupported inputs; do not copy their encoded
frames into the converted data set. Resolve or export pending work first and
establish a new journal/session baseline according to the application's retention
policy. Preserve issued/consumed allocation ranges, including canceled allocations
and unused grants: copying visible values alone can allow an old ID to be reused.

Selecting `fixed` chooses a payload policy; it does not establish compatibility
with an arbitrary earlier reader or file. The converter covers the supported
native containers and matching generated model, with no general backward-format
compatibility qualification. Other historical formats or schema changes require
the original reader and an application-owned identity-preserving import, or
continued use of their selected reader/writer. Such an import must retain exact
document/object/class IDs, reference targets and allocator metadata; ordinary
construction that renumbers entities is unsuitable.

Encoding conversion does not change collaboration's strict identity contract:
the authority and every replica retain the same document/object IDs. Persistent
nonreuse still requires the authority journal before issuing ranges and durable
client cursor state before exposing IDs. An old snapshot cannot remember leases
issued after that snapshot. See [local collaboration and recovery](../managed/local_collaboration.md)
and [managed persistence](../managed/cpp_runtime.md#history-persistence-and-limitations).

## Verification

Windows x64 MSVC qualification passed both generated profiles, direct uint32 and
separated uint64, with seven cases each, plus the actual helper-generated CLI file
workflow: **3/3 CTests**. Checks cover all five supported history/label shapes,
authority lease preservation, all nine raw binary codec pairs, ID boundaries,
typed token policy overrides, an ordinary unmanaged root and stale managed-header
rejection. The fixed-wire
fixtures are independent expected-byte checks. The standalone
consumer also builds with `/W4 /WX /EHsc` against an installed 2.7 package and
converts both managed and raw fixture files to the exact expected output. See the
[qualification record](../verification/verification-opc-xsd-2026-10-19.md).

The [20 October historical qualification](../verification/verification-managed-legacy-migration-2026-10-20.md)
separately rebuilds compiler/runtime **2.6.0** from pinned Git source and uses its
own generated headers/runtime to produce fixed-ID artifacts. Both direct uint32
and separated uint64 profiles pass **92 actual current-CLI migrations**, covering
all nine raw codec pairs, boundary IDs and all five supported native history/label
shapes. Reverse conversion reproduces every old input byte-for-byte; current
native consumers verify exact identities, labels, cursors, tree ancestry, redo,
allocation watermarks and nonreuse. This closes the selected older-artifact
qualification, without claiming arbitrary historical containers, journals or
collaboration checkpoint conversion.
