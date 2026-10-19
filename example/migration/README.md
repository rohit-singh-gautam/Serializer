# Schema-linked migration tool

This consumer builds an executable with the codecs for `model.serializer`. Replace
that schema with the exact source model before converting its files; managed
saved files also check the complete generated schema binding. No field mapping or
handwritten `main` is needed.

```cmake
serializer_add_migration_tool(TARGET model_migrate
  SCHEMA model.serializer ROOT migration_example::document CONFIG model.ini)
```

Build against an installed package with `CMAKE_PREFIX_PATH`, or against a checkout:

```powershell
cmake -S example/migration -B out/build/migration-example -G Ninja -DSERIALIZER_SOURCE_DIRECTORY=C:/src/Serializer
cmake --build out/build/migration-example --target model_migrate
```

On Windows use a developer shell and make enabled dependency DLLs available on
`PATH` when running the installed compiler and tool.

Select the input's identity policy explicitly. The destination must be a distinct
new file; the tool stages, syncs and atomically publishes it without overwriting.

```powershell
out/build/migration-example/model_migrate --kind managed --from fixed --to compact_varint --input old.save --output compact.save
out/build/migration-example/model_migrate --kind raw --from fixed --to compact_varint --input-format binary_none --output-format binary_integer --input old.bin --output compact.bin
```

Managed migration preserves document and object IDs, allocation watermarks,
retained history, labels, redo/tree cursors and native authority lease records.
Raw migration supports positional, integer-key and string-key binary files,
including ordinary generated models without managed metadata. Source and target
formats are independent. A raw file carries no saved document/history binding;
the caller selects its correct schema and source encoding.

The initial converter handles the current native managed save envelopes and
explicit fixed/compact identity policies. It rejects journals, collaboration
client checkpoints and unknown metadata. Use a dedicated converter for another
schema/layout or container version. Normal `load` never invokes migration.

Default file bounds are 64 MiB each, with existing snapshot/history decoder
limits and a 512 MiB staging/cache estimate. `--help` lists explicit byte bounds.
The staging estimate supplements decoder allocation limits; it is not a process
resident-memory guarantee. A directory-sync failure after publication reports an
uncertain durability result and retains the published destination for inspection.

See [the migration guide](../../docs/guides/persistent_id_migration.md) for the API,
storage prerequisites and verification status.
The installed-package CMake route was checked with MSVC `/W4 /WX /EHsc`, tool
help and actual managed/raw fixture conversions compared with their complete
expected files.
