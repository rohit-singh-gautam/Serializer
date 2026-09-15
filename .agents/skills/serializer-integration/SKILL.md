---
name: serializer-integration
description: "Integrate Serializer into C++ applications using .def schemas, output coding profiles, owning classes or binary views, stable_ids, and JSON or binary codecs. Use for new integrations, schema evolution, and migrating Serializer callers."
---

# Serializer Integration

Implement the user's requested Serializer integration using the version present
in their project. Preserve established wire IDs, names, protocol choices, and
application ownership requirements.

## Locate the library and the requested work

Find the application's existing Serializer dependency, schemas, generated-header
build rules, and callers before adding another copy. In this bundled layout,
the Serializer repository root is three directories above this skill. If the
skill has been copied or installed elsewhere, locate the matching checkout from
the application's dependency/build configuration; resolve the following files
there instead of relying on the relative links.

- Read [README.md](../../../README.md) for supported syntax and feature status.
- Use [docs/usage.md](../../../docs/usage.md) for the schema, CMake, and codec examples.
- Use [docs/views.md](../../../docs/views.md) for view generation, mapping, and mutation.
- Use [docs/output_configuration.md](../../../docs/output_configuration.md) for
  language sections, coding profiles, naming, formatter configuration, and examples.
- Read [docs/wire_format.md](../../../docs/wire_format.md) when choosing protocols,
  limits, failure handling, or compatibility behavior.
- Read [migration.md](../../../migration.md) when updating older headers or APIs.

Use local instructions from the repository being edited. When a path or message
contract cannot be determined from the project, ask for that missing information.
Do not turn an integration request into a library redesign or implement a proposed
feature as a prerequisite without the user's request.

## Author or evolve the schema

- Use `class`, `enum`, and `namespace`. Every member needs an explicit access
  modifier and a trailing semicolon. Classes/enums have no trailing semicolon.
- In owning representations, map `array T` to `std::vector<T>` and `map(K) T` to `std::map<K, T>`.
  Preserve wire names and defaults. Check the output naming policy before writing
  callers; `naming = preserve` retains existing schema-defined C++ identifiers.
- Put class attributes after the name, before a parent list. `stable_ids` requires
  explicit IDs on every member and parent; it does not assign IDs or compare old
  schemas. For new evolving integer-key contracts, prefer this check.
- When enabling `stable_ids` on an existing class, determine the previously emitted
  IDs first and write those exact numbers. Implicit slots count parents followed
  by members in declaration order; an explicit override does not shift later
  implicit slots. Parent/member IDs share the containing class's ID space.
- Keep IDs unique in `1..0x3fffffff`. Preserve wire names with metadata such as
  `public string name ("fullname", 3);` when renaming a C++ field. Do not reuse old
  IDs/names or renumber enum/union alternatives in an existing contract.
- Keyed readers reject unknown fields. Explicit IDs do not make old readers accept
  new fields. Positional binary still requires matching declaration order/types.
  Resolve incompatible changes through the application's versioning contract.
- Check supported types before choosing raw unions: generated alternatives must
  be trivially destructible. Ordinary generated enums use names in JSON/string-key
  fields; binary collection enum elements use numeric values.
- Select only the requested representations: no keywords or `owning` means an
  owning class; `view` means both views; `view readonly` or `view mutable` restricts
  the views. Adding `owning` retains owning storage too. Attributes are unordered;
  `readonly`/`mutable` require `view`. One mode generates a plain class; multiple
  modes require `storage_mode::owning`, `read_only_view`, or `mutable_view`.
- Nested classes/parents must enable each mode their owner needs; map keys require
  read-only nested views. Declare referenced types before use. `packed` cannot be
  combined with `view`. `stable_ids` is independent of mapping, which is positional.
- `inplace` and `simd` are not implemented keywords. Views and little-endian
  defaults are implemented in source but their generation/build/tests remain
  deferred for the current implementation step; do not claim qualification.

For a new schema, a minimal explicit-ID example is:

```text
class person stable_ids {
  public string name (1);
  public uint32 age (2);
}
```

Adding `public string country;` is rejected; `public string country (3);` assigns
an unused ID. Existing explicit IDs work even without `stable_ids`.

## Integrate generated headers

Use the consuming project's existing dependency mechanism. When using CMake,
`add_subdirectory` exposes `serializer_lib` and the `serializer` generator target.
The library supplies the include path and C++20 minimum; its CMake project needs
3.28 or newer. Keep a newer language mode if the application already uses one.

Adapt the custom command in the usage guide: schema and generator dependencies,
build-directory output, and a generated include directory for the consumer.
Generate each shared output once when multiple targets consume it. Cross builds
need a host-runnable generator. Do not hand-edit generated headers.

Keep output settings in a generator INI config, with `[output] language = cpp`
and a `[cpp]` section. Only C++ is implemented. Supported `coding_standard` values
are `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, and
`qt`. These are presentation profiles, not whole-guide compliance guarantees.
See the [profile examples](../../../example/coding_styles/README.md).

Default `naming = profile` renames schema-derived C++ types, fields, enum values,
union helpers, and view accessors. Wire names/IDs and runtime-required hooks keep
their original spelling. Resolve generated naming collisions; do not silently
change wire names to fix a C++ naming problem. Enum defaults referring to declared
values are translated; opaque C++ default expressions require `naming = preserve`
or an explicitly chosen literal/default change.

Formatting requires clang-format 19+ at generation time. Pin its version and pass
`cpp.clang_format` or set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` for this repository's
CMake rules. `format_file` can replace layout rules. Config paths are relative to
the config; CLI paths are relative to the working directory. CLI overrides take
precedence over config values. Add configs and custom format files to generation
dependencies. Use `format = false` only when another pipeline formats the emitted
source. The test build includes all profile examples; with tests disabled,
`SERIALIZER_BUILD_STYLE_EXAMPLES=ON` enables them independently.

The CLI uses named arguments:

```sh
serializer input schemas/person.def output person.hpp
```

Use the actual built executable path when it is not on `PATH`. Regenerate through
the real build pipeline when execution is part of the task; preserve an explicit
user instruction to defer generation/builds/tests and report what remains unverified.

## Implement the codec calls

- Select `json`, `binary_none`, `binary_integer`, or `binary_string` according to
  the agreed message contract. Do not silently change protocols to improve size
  or speed. `format::compress` means JSON whitespace suppression.
- Binary fixed-width scalars default to little-endian. Inspect `Protocol::wire_endian`;
  an explicit third `binary` template argument selects another byte order when
  required. Compact prefixes keep their own encoding. No endian/protocol marker,
  auto-detection, or compatibility aliases are provided; agree on the format with
  the peer. JSON is unaffected by numeric byte order.
- Encode into an appropriate stream. Reuse `full_stream_auto_alloc` capacity when
  useful, resetting only after readers of the previous message have finished.
- Construct input views with the actual message length, such as
  `make_constant_full_stream(output.begin(), output.current_offset())`, never the
  spare allocation capacity. Keep input storage alive and independent of output
  destinations or any mutation that could invalidate it.
- For explicit limits and whole-message validation, construct the input protocol
  with `(input, decode_limits)`, call `decoder.serialize_in(destination)`, and then
  `decoder.finish()`. Convenience object input calls omit the final exact-message
  check. Share decoder sessions by reference; they are noncopyable.
- Choose limits from the application's message sizes and workload. Storage/work
  accounting is cumulative per decoder and is not an exact process-memory limit.
- Strings/vectors/maps replace contents. Missing keyed fields retain destination
  values; duplicate fields apply in order and maps keep the last complete duplicate
  entry. Decode failure can leave partial updates and consumed input.
- When the application requires atomic replacement, decode/finish a temporary
  object before committing it. Keep structured parse error handling at the message
  boundary and leave payload excerpts disabled unless the task needs them.

## Map generated views

Use `View::map(span, limits)` with the exact little-endian `binary_none` message.
Mapping validates the full message and builds inline field offsets. Getters return
scalars, borrowed strings, or nested views. Mutable mappings require writable
spans; read-only mappings never expose setters. Buffer owners must outlive every
view and must not relocate or change the layout while views remain live.

Use setters only for existing scalar/string fields or collection elements with
unchanged encoded sizes. Do not promise insertion, resizing, union switching,
or JSON/keyed/big-endian mapping. Map keys remain read-only; map entries retain
wire order and duplicates. Nested field access can construct another validated
view; variable-width collection indexing traverses preceding entries. Refer to
the view guide for parent accessors, union access, and complete examples.

## Verify and document the result

Use the task's permitted validation scope. For an integration, exercise a generated
record through the selected protocol with an exact-size input, destination reuse,
and relevant malformed/limited input. For schema migration, include the relevant
old/new schema behavior. Consult [qualification](../../../qualification/README.md)
for library-wide tests, fuzzing, or benchmarks only when that work is in scope.

Report files changed, how to generate/build/use the result, and what was actually
validated. When editing Serializer itself, keep its README and this skill current
together with affected linked guides. When editing a consumer, update that
application's relevant usage docs without modifying an upstream checkout merely
to restate an unchanged library contract. Keep public examples specific to
Serializer and public sources.
