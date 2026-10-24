# Managed token codec profiles

Python, Java and JavaScript/TypeScript's explicit token-only profiles pass actual
generated SDK and C++ exchange qualification, as recorded below. Token-only modes
do not supply native managed engines. The separate [Python](python_native_runtime.md),
[Java](java_native_runtime.md) and [JavaScript/TypeScript](javascript_native_runtime.md)
native engine options include these wire codecs
and have their own supported profiles and qualification.

## Python wire-record profile

Compiler/runtime 2.34.0 adds an explicit Python **wire-record** mode. It exchanges
native managed identity fields and typed-reference envelopes with C++ without
providing a Python managed engine. Enable it in generator configuration:

```ini
[python]
managed_token_codecs = true
[managed]
id_type = uint32
id_encoding = compact_varint
```

The equivalent CLI flag is `--python.managed_token_codecs true`. The default is
`false`, preserving the existing rejection of managed declarations in ordinary
Python generation. The C++ writer API selects
`portable_options::managed_token_codecs`, `managed_id_type` and
`managed_id_encoding` when calling `writer::portable::generate` for `python`.

Generate from language 1.20 or newer schemas that already declare `managed_ref<T>`.
This output option introduces no schema syntax or wire revision. Generated managed
records retain `persistent_id`; `ManagedReference` has `document_high`,
`document_low`, `class_id` and `object_id`. Each eligible target class exposes
`managed_class_id`, independent of its mutable payload layout. Explicit `class_id`
and the existing frozen qualified-class FNV-1a identity preserve C++ type tags.

```python
token = model.ManagedReference(document_high, document_low,
                               model.TokenWireItem.managed_class_id, object_id)
value = model.TokenWireEnvelope()
value.target = token
payload = value.encode(model.Protocol.BINARY_INTEGER)
copy = model.TokenWireEnvelope.decode(payload, model.Protocol.BINARY_INTEGER)
```

The token owns no entity, pointer or registry. Constructor arguments are untrusted
values until encode or decode checks their envelope. Both directions validate
unsigned widths, nonzero document scope/object ID and the exact target class tag.
Keyed envelopes require all four fields exactly once and reject unknown metadata,
including under flexible JSON input policy. A decoded token does **not** establish
document membership, liveness, ownership or permission. C++ must perform native
complete-candidate validation or registry resolution before using its target.
Python may carry a token whose target is absent from the exchanged record.

All four native codecs retain the C++ envelope layout. Only the object ID follows
`managed.id_encoding`; document halves and class tags remain fixed uint64 in
binary. `managed.id_type` selects uint32 or uint64 for object and persistent IDs.
JSON carries numeric identity values. Required tokens default invalid; optional
absence uses the ordinary empty tuple `()`, and presence uses `(token,)`.
Field-level arrays/maps and supported generic container wrappers carry tokens
without following their phantom targets. Concrete self/mutual tokens are allowed.

This initial mode supports concrete owning records and enums. It rejects generic
declarations/instantiations, schema behavior/construction/conversion/native blocks,
reference bindings, XML/package generation, caches, indexes, readonly exposure and
computed class helpers. `managed.separate_values = true` also rejects; this mode
uses direct identity payloads. Managed child/base occurrences require targets
that already declare managed eligibility. Managed ownership must remain finite and acyclic, with
distinct public bases and owning variants; identity name/ID reservations remain
checked. Required token fields, including nested records/fixed arrays and default
variant branches, cannot be transient or omitted. Empty optional/variable
containers remain valid resets. Private
Python attributes retain the normal convention rather than enforcing immutability.

The mode supplies no allocator, store, transactions, immutable snapshots, identity
allocation, history, journal, collaboration or migration engine. It does not
activate managed features through a raw model constructor. Other generated
languages beyond the Java profile below and broader named-generic token selection remain under MR03; native
non-C++ engines remain MG01. Qualification is recorded
[separately](../verification/verification-managed-token-codecs-2026-10-24.md).

## Java wire-record profile

Enable Java's independent opt-in with `[java] managed_token_codecs = true` or
`--java.managed_token_codecs true`. The default remains `false`. Java uses the
same central `[managed] id_type` and `id_encoding` settings, class-tag lowering
and direct identity payloads as Python and C++. `managed.separate_values = true`,
XML and computed helpers reject. The common concrete-record scope and unsupported
schema mechanisms described above also apply.

Each eligible Java class exposes `MANAGED_CLASS_ID`; records retain their
`persistent_id` field under the selected Java naming profile. Typed references
use immutable `ManagedReference<T>` values with `documentHigh`, `documentLow`,
`classId` and `objectId`. Java `long` preserves the full unsigned uint64 bit
pattern, including negative signed representations. These values must be passed
through unchanged; ordinary signed comparisons do not express unsigned ordering.
The required token default is invalid until populated. Optional absence retains
the ordinary generated Java optional representation.

All four native codecs enforce the exact typed envelope, unsigned ID width,
nonzero document and object identities, and complete keyed metadata. The three
scope/type uint64 values stay fixed-width in binary; only the object ID follows
the selected compact policy. Decode charges its configured allocation budget
before constructing a token and rejects duplicate, unknown or missing keyed
metadata. Token envelopes do not establish liveness or ownership, just as in
Python. C++ registry resolution remains a separate operation.

`test/managed_token_java_test.py` generates four width/encoding profiles, compiles
their actual SDKs with Java 17 `-Xlint:all -Werror`, exercises malformed envelopes
and unsigned boundaries, and exchanges all four native formats with the existing
C++ producer/consumer targets. It also checks CLI/configuration opt-in and output
preservation on generation failure. It passes **4,953 actual generated Java
checks** across the four identity profiles, with all **64 corpus messages**
round-tripped through the matching four C++ exchange targets. Microsoft OpenJDK
17.0.18 compiled the SDKs with `--release 17 -Xlint:all -Werror`. The independent
runtime-only probe is superseded by these actual generated SDK results; see the
[verification record](../verification/verification-managed-languages-2026-10-24.md).

## JavaScript and TypeScript wire-record profiles

Select `js.managed_token_codecs = true` and the matching
`typescript.managed_token_codecs = true` declarations explicitly, through CLI
flags or the respective INI sections. Both default to false. Generate paired
`model.mjs`/`model.d.mts` outputs with the same `managed.id_type` and
`managed.id_encoding`; engine selection is independent and implies token codecs.

Managed records retain `persistentId`. Immutable `ManagedReference<T, Id>`
envelopes carry `documentHigh`, `documentLow`, `classId` and `objectId`; eligible
classes expose `MANAGED_CLASS_ID`. Document halves and class tags are BigInts.
Object IDs use exact uint32 Numbers or uint64 BigInts. Optional absence/presence
uses the ordinary empty/single-element array representation.

All four native codecs retain existing C++ framing. Only the object ID uses
compact encoding; scope/class metadata remains fixed-width uint64 in binary.
Encoding validates native types and ranges before scalar coercion. Decoding
requires all keyed envelope fields exactly once and rejects unknown metadata,
even under flexible ordinary JSON policies. Frozen exact reference instances
prevent subtype validator replacement. TypeScript nominal brands distinguish
named targets even when their fields are identical. Decoding still does not
establish target liveness, document membership, ownership or permission.

The [actual SDK replay](../verification/verification-javascript-managed-runtime-2026-10-24.md)
passes **5,309 token checks** in four width/encoding profiles, strict TypeScript
consumers, four independent C++ profiles and 64 exchanged messages. Token-only
artifacts do not contain `ManagedStore`. Their finite public owning profile
retains explicit inheritance/generic/behavior/cache/index/variant/XML/separate
storage restrictions; the [native SDK](javascript_native_runtime.md) has its own
history, concurrency and native-journal qualification.
