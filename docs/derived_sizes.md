# Derived record sizes

Schema language **1.9.0**, supported by compiler/runtime **1.14.0**, adds
`auto_size` for the three native binary protocols. It counts encoded bytes of the
containing record, rather than `sizeof` or heap allocations. JSON and Protobuf
cannot be selected. Existing unannotated fields retain their encoding.

```text
serializer version 1.9.0;
class packet {
  public uint32 encoded_size(1)
    auto_size(binary_none, binary_integer, binary_string)
    omit(all_except(binary_none, binary_integer, binary_string));
  public string text(2);
}
```

The encoder computes the value without changing `packet.encoded_size` in the
source object. A fresh decoder stores the wire value and checks it against the
actual record span. The default scope is `size_scope(record)`: it includes the
size field's key and value, magic bytes, other fields, and the record terminator.
An enclosing field key or array count is outside the nested record's span.
Each nested record computes its own size.

## Storage and counting controls

Use `wire_only` when the application does not need a stored size member:

```text
class transport_packet {
  public wire_only uint32 encoded_size(1)
    auto_size(binary_none, binary_integer, binary_string)
    omit(all_except(binary_none, binary_integer, binary_string));
  public string text(2);
}
```

This emits and validates a wire header but creates no per-instance member,
constructor binding, comparison component, conversion projection or behavior
projection for it. The declaration still owns its normal wire name and identity.
It must omit every unselected format and cannot have a default initializer.

Add `size_scope(payload)` after `auto_size(...)` to exclude the complete size
field itself: its key and fixed scalar value. Magic, record termination and all
other fields still count. `size_scope(record)` is the explicit default. Scope and
storage are independent; either scope can use an ordinary or wire-only field.

Use `uint32` or `uint64` to select the header width. Output fails if the measured
value cannot fit. Select one size field per record per protocol; separate fields
may select disjoint protocols. `binary_positional` aliases `binary_none`.

## Decoder policy

Keep the wire meaning in the schema. Choose a decoder policy per language and
source-qualified class in INI:

```ini
[cpp.class.packet]
auto_size_validation = verify

[python.class.transport_packet]
auto_size_validation = retain
```

Every backend supports `verify` (default) and `retain`. Exact concrete class
settings take precedence over authored generic settings. `retain` skips only
the declared-value comparison. It stores ordinary members and discards
wire-only values. Both policies still require the selected header exactly once
and retain each backend's normal resource limits. The size is
never trusted as an allocation or record-boundary authority.

The field must be a fixed, noncompact `uint32` or `uint64` scalar in an unpacked,
unmanaged owning record. Transient, view, managed, lifecycle and revision fields
are ineligible. In unselected formats, an ordinary size field follows its usual
encoding unless omitted explicitly; the example omits it everywhere else.
C++ constant-evaluation and emission-only profiles support positional
`binary_none` sizes, including nested records, stored/wire-only metadata and
both scopes. They count and write schema values at compile time with fixed-width
placeholders during counting, preserving the ordinary runtime bytes. Existing
constant-evaluation shape restrictions still apply; see the
[compile-time output guide](constant_evaluation.md).

## Raw structure bytes

`binary_none` writes declared values in positional order; it does not copy the
object's memory. `sizeof(T)` includes ABI padding and says nothing about encoded
strings or containers. Raw object bytes can contain addresses and depend on
compiler, ABI and endianness. Serializer has no raw ABI dump mode. Applications
needing such a local representation must define a separate fixed, trivially
copyable layout and its lifetime and ABI contract. `wire_only` creates no memory
slot for a raw dump to include.

See the [public size schema](../example/schemas/derived_sizes/model.serializer),
[runnable examples](../example/README.md#derived-sizes-and-stateless-layout),
[wire contract](wire_format.md), [migration](../migration.md), and
[qualification record](verification-deferred-features-2026-10-11.md).
