# Schema compatibility checking

`stable_ids` validates one schema. The optional compatibility checker compares
two resolved revisions and applies a persistent retirement policy. It is a
separate, read-only compiler mode; it does not generate code, change schema
syntax, renumber declarations, or introduce a new wire format.

## Compare revisions

```sh
serializer --input schemas/current.serializer \
  --check-against schemas/previous.serializer \
  --compatibility-protocol binary_integer \
  --compatibility-direction backward \
  --compatibility-policy schemas/compatibility.json
```

The protocol is required: `binary_none`, `binary_integer`, `binary_string`,
`json`, or `protobuf_binary`. ProtoJSON and TextProto are not supported by this
checker yet. Each schema uses normal versioned-file parsing, including its own
relative includes; preserve old dependency files with the old entry schema.
No formatter, generated header, Java runtime, or external Protobuf runtime is
needed. Generation/output options cannot be combined with `--check-against`.

| Direction | Required behavior |
| --- | --- |
| `backward` | The new reader accepts messages from the previous writer |
| `forward` | The previous reader accepts messages from the new writer |
| `both` (default) | Both directions |

Diagnostics identify the qualified type/field and affected reader direction.
Changes affecting only the unselected direction are still printed as
`other direction`. Exit status **0** means no incompatibility was detected for
the selected direction; **2** means a compatibility/identity hazard was found;
**1** means invalid options, policy, schema, or an I/O failure.

This is a conservative schema check, not proof of application compatibility.
All declarations are compared, including unused types and included declarations.
Field identity means the ID/wire-name pair, so changing one is flagged even when
the selected codec uses only the other. Renaming a source member while retaining
both through metadata is accepted. Changing a native schema default is flagged
because absent fields can change meaning. Human review still needs to establish
units, meaning, validation rules, ownership, and deployment order.

## Retain reserved identifiers

Keep a policy file under version control alongside the schema:

```json
{
  "version": 1,
  "reserved_fields": [
    {"type": "example::record", "ids": [7, 9], "names": ["old_name", "old_payload"]}
  ]
}
```

The policy version describes this JSON file, independently of schema syntax,
compiler release, and message format. `type` is the exact fully qualified schema
class name without a leading `::`. IDs and names are scoped to that class;
parents and fields share the scope. The checker requires **both** the old ID and
wire name to be reserved when removing a field/parent. It checks reservations
against every field and parent in the new revision, even if the previous schema
no longer contains the retired member.

A reserved union base name also reserves its `base:alternative` native keys.
Individual expanded wire names can be reserved too. IDs must be in the native
field-ID range `1..0x3fffffff`. Empty/duplicate names, duplicate IDs/scopes,
unknown/duplicate JSON properties, unsupported policy versions, and trailing data
are rejected. Policy input is bounded to 1 MiB.

Retain reservations permanently. This checker does not compare old/new policy
files or automatically recover history after a reservation is removed. Scopes
absent from both schemas are retained so a deleted class's reservations can still
protect it if it returns; review scope spelling carefully. Reservations are
enforced by the checker, not ordinary code generation, so run this mode in CI.

Enum values and union indices still follow declaration order. Reordering,
renaming, removing, or changing an existing alternative is reported; appending
an alternative affects older readers. Keep deprecated alternatives in their
original slots. Version 1 of this feature reserves class field/parent IDs and
wire names; it does not add explicit enum numbers, union tags, or reserved holes
to schema syntax.

## Protocol boundaries

- Positional binary requires the same field order and shape in both directions.
- Native keyed binary and JSON reject unknown fields. Adding a field can be
  backward-readable, but is not forward-readable. Removing a reserved field can
  be forward-readable, but new native readers reject old messages containing it.
- C++ Protobuf binary skips unknown fields, so added fields and properly reserved
  removed fields can pass in both directions. The checker first validates each
  schema against Serializer's supported Protobuf mapping. Unknown fields are
  discarded, unknown enum values are rejected, and presence/union limitations
  still apply; see [the Protobuf contract](protobuf.md).
- Byte order is an external agreement. Both revisions are checked under the same
  selected protocol; no payload or transport envelope is inspected. The schema
  language version identifies neither message byte order nor wire version.

Unknown-field skipping for native keyed binary is **not implemented**. Its fields
lack type/extent metadata, so the reader cannot safely discover an unknown value's
end. Adding that capability requires a separately versioned format with an agreed
encoding and negotiation mechanism. It must not be silently added to an existing
protocol or inferred from `serializer version 1;`.

## Library API

Include `<rohit/schema_compatibility.hpp>` and link `Serializer::serializer_lib`.
Use `parser::parse_file` for both revisions, optionally load
`read_compatibility_policy`, and call `check_schema_compatibility` with the explicit
`compatibility_protocol`. The returned diagnostics contain `path`, `message`,
`breaks_old_reader`, and `breaks_new_reader`. Invalid inputs throw; the checker
does not mutate either resolved schema or the policy.

## Verification

The [2026-09-17 follow-up record](verification-2026-09-17.md#compatibility-and-exact-decoding-follow-up)
identifies the exact source snapshot, passing Windows/Linux suites, focused
ASan/UBSan tests and CLI checks, and installed-package consumer verification.
These checks cover reader directions, retired ID/name reuse, enum/union changes,
positional order, maps/nested types/defaults, policy validation, and relative includes.
They do not establish semantic equivalence or a new wire format.
