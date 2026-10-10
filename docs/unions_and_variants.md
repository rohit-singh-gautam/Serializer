# Unions and owning variants

Compiler/runtime **1.9.0** and schema language **1.4.0** support two choice
declarations with the same native wire format. `union(...)` preserves the
existing generated APIs, including raw C++ union storage. `variant(...)`
generates owning C++ `std::variant` storage and a single JavaScript/TypeScript
discriminated payload. Both use schema-defined alternative names and declaration
order; application code does not define another discriminator enum.

Use these declarations for wire or file transfer models. Internal application
types do not need to become Serializer models merely because transfer data uses
them.

## Schema

```text
serializer version 1.4.0;

namespace example {
  class event {
    public string message;
    public array uint32 numbers;
    public map(string) string attributes;
  }

  class record {
    public variant(string = text, event = event) payload;
  }
}
```

An owning alternative may contain strings, arrays, maps, or generated owning
classes. Repeated alternative types are legal when their names differ. The
first declared alternative is active after value initialization. Selector syntax
requires `serializer version 1.4.0;` in the file declaring the field, including
included files. Existing user types and generic parameters named `variant`
remain valid when used as ordinary types.

Raw C++ `union(...)` alternatives must remain trivially destructible and their
active member's lifetime must be legal before encoding. Use `variant(...)` when
the transfer model requires nontrivial ownership instead of manually adding a
destructor, placement construction, or a separate application tag.

## C++ owning API

```cpp
#include "record.hpp"

example::record record{};
auto& details = record.emplace_payload<example::record::e_payload::event>();
details.message = "completed";
details.numbers = {1, 2, 3};
details.attributes = {{"status", "ready"}};

auto copy = record; // Copies the active event and all its owned values.
record.emplace_payload<example::record::e_payload::text>("replacement");
```

Generated members and helpers for a field named `payload` are:

| API | Meaning |
| --- | --- |
| `e_payload` | Schema-defined alternative enum, in wire order. |
| `u_payload` | Alias for `std::variant<...>` containing the declared types. |
| `payload` | The sole owned active value. |
| `get_payload_type()` | Computes `e_payload` from `payload.index()`. |
| `emplace_payload<e_payload::event>(args...)` | Constructs the selected alternative and returns its reference. |
| `visit_payload(visitor)` | Visits the active payload; const and mutable overloads are generated. |
| `to_string(e_payload::event)` | Returns the alternative's unchanged wire name. |
| `to_e_payload("event")` | Converts a complete wire name to its schema enum. |

There is no mutable `payload_type` member for an owning variant. `std::get<I>`,
`std::get_if<T>`, ordinary copy/move, and destruction have standard C++ semantics.
Use index-based access or the generated enum helper when alternative C++ types
repeat. A valueless `std::variant` is rejected during encoding.

C++ generic classes retain owning variant storage for template alternatives.
Existing naming profiles apply to helper names, enum names, and members while
preserving wire identities. Owning and binary-view class modes may coexist;
view alternatives use the existing positional byte accessors. A view cannot
switch the encoded alternative or resize it.

## JavaScript and TypeScript

```javascript
const record = new ExampleRecord();
const details = new ExampleEvent();
details.message = 'completed';
details.numbers = [1, 2, 3];
details.attributes = new Map([['status', 'ready']]);
record.payload = {kind: 'event', value: details};

const bytes = record.encode(Protocol.BINARY_NONE);
const decoded = ExampleRecord.decode(bytes, Protocol.BINARY_NONE);
if (decoded.payload.kind === 'event') {
  console.log(decoded.payload.value.message);
}
```

TypeScript declarations expose the corresponding discriminated union of
`{kind, value}` types. JavaScript constructs only the default active alternative;
decoding creates the alternative selected by the wire data. Invalid kinds and
native decoder resource limits are checked normally. Existing JavaScript
`union(...)` fields keep their `payloadIndex` and `payloadAlternative` members.

Java, Go, C#, C, Rust, Python, Swift, and Kotlin map both schema keywords through
their existing generated tagged-choice representation and native codecs.
Their field names and construction APIs remain backend-specific. Follow the
[paired examples for all eleven outputs](../example/README.md#union-and-variant)
for the exact API in each language. Their example root declares a Serializer
`version` field, retains revision 1 during round trips, and rejects unsupported
revision 2 through generated JSON readers. C++ and JavaScript/TypeScript also check
unsupported revision rejection through all four generated encoders. TypeScript
uses the generated JavaScript runtime.

## Native wire contract

Changing a field from `union(...)` to `variant(...)` without changing its IDs,
wire names, alternative names, order, or types preserves native bytes:

| Protocol | Choice representation |
| --- | --- |
| `BINARY_NONE` | Compact zero-based alternative index, followed by the selected value. |
| `BINARY_INTEGER` | Field ID, compact alternative index, then the selected value. |
| `BINARY_STRING` | `field:alternative` wire key, followed by the selected value. |
| `JSON` | `field:alternative` JSON property, containing the selected value. |

For example, the `event` choice in the schema above produces
`{"payload:event":{"message":"completed","numbers":[1,2,3],"attributes":{"status":"ready"}}}`.
JSON is produced and consumed by generated Serializer codecs. No intermediate
JSON parser, string-key builder, or additional transfer wrapper is required.

The choice declaration does not add magic or a version. Classes acquire these
only when their schema explicitly declares them. Follow the application's
framing and version requirements independently of the union/variant selection.
The schema compatibility checker compares the shared wire shape, so ownership
selection alone is not reported as a native format migration. Alternative
reordering or changing field identities retains the usual migration risks.

Unknown numeric indices are rejected before replacing an existing C++ owned
payload. Once a valid alternative has been selected, it is default-constructed
and decoded with the normal partial-update-on-failure semantics. Use
`deserialize_exact<T, Protocol>` to obtain a fresh value without returning a
partial object on failure.

## Limitations and verification

C++ `cpp.protobuf true` and borrowed `cpp.emission_only true` explicitly reject
owning `variant(...)` fields. Existing raw unions remain supported by those
profiles under their established restrictions. Managed C++ class modes currently
reject both choice declarations. IDE syntax and navigation support both keywords.
These restrictions do not affect the four
native protocols or the ordinary generated models in other languages.

The feature tests cover all active arms, repeated C++ types, nested owned data,
copy/move/replacement, visitors, generic classes, binary views, decoder limits,
truncated/unknown input, language headers and includes, legacy identifiers,
and raw/owning byte parity. The JavaScript interoperability test consumes real
generated C++ bytes for all four protocols, re-encodes them byte-for-byte, and
checks the generated TypeScript declarations when `tsc` is available:

```text
core_serializer_test
serializer_owning_variants_portable
serializer_owning_variants_typescript
```

See the [wire contract](wire_format.md), [usage guide](usage.md), and
[all-language example runner](../example/README.md) for the surrounding workflows.
