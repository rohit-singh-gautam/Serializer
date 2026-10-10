# Magic headers and output exclusions

Magic identifies an artifact independently of its payload revision. Declare it
once in the schema; generated writers emit the constant and generated readers
verify and discard it. It consumes no mutable per-object storage.

The parser suggests this keyword when an ordinary initialized field's source or
wire name is `magic`, compared case-insensitively. This `[simplicity-magic]`
warning identifies likely intent from the name; it cannot reliably recognize
arbitrary signature values or prove that conversion preserves an existing layout.
To keep an intentional ordinary field, use language `1.5.0` and trailing
`ignore(warning magic)`. See [simplicity warning rules](schema_reference.md#simplicity-warnings)
and review existing bytes before migrating a field into a fixed header.

```text
serializer version 1;
class document stable_ids {
  private magic (99) { 'SRLFILE' };
  public version uint16 revision (100) { 1 };
  public uint64 id (1);
  public string description (2) omit(binary_positional, binary_integer, binary_string);
  public uint32 diagnostic (3) { 7 } omit(json);
}
```

For the legacy byte-literal form, the C++ generated declaration is an
`inline static constexpr char magic[N]`
containing exactly the declared bytes, with no implicit NUL terminator.

C++ output uses character literals and an escaped comment to show the header:

```cpp
// Fixed schema-owned header "SRLFILE"; no per-object storage.
inline static constexpr char magic[] = {'S', 'R', 'L', 'F', 'I', 'L', 'E'};
```

Quotes, backslashes, and control bytes are escaped. Other nonprintable and
non-ASCII bytes use fixed-width octal escapes, preserving the exact byte values
and array extent independently of the generated source encoding. This changes
only source presentation; codecs, runtime storage, and wire bytes are unchanged.

`public`, `protected`, and `private` control constant visibility independently of
serialization. Other languages expose their corresponding static immutable
constant representation. Python's nonpublic names follow its existing underscore
convention; C does not offer language-level member access control.

Native positional, integer-key, and string-key binary codecs write raw magic
before the version and ordinary object fields, without a count, key, or
terminator. Their readers validate the prefix before decoding payload fields.
JSON includes the fixed `magic` string by default and requires exactly one matching
value, wherever that key appears. Readers discard the value after validation.
Protobuf mappings use the magic numeric field ID and fixed string value.
Magic and payload versioning remain separate features.

## Typed scalar and enum magic

Schema language `1.1.0` supports an explicit type after `magic`:

```text
serializer version 1.1.0;
enum artifact_kind {
  document,
  image
}
class document stable_ids {
  private magic uint32 (99) {42};
  public uint64 id (1);
}
class image stable_ids {
  public magic artifact_kind (99) {artifact_kind::image};
  public uint32 width (1);
}
```

Supported types are `char`, `bool`, `int8/16/32/64`, `uint8/16/32/64`, finite
`float`/`double`, and declared enums. Each declaration requires exactly one
initializer compatible with its type. Integers must fit their declared width;
enum constants must belong to the declared enum. Strings, classes, collections,
type parameters, and `version2/3/4` component types are unsupported. `char` uses
its byte-valued scalar contract. Values 128..255 require `omit(json)` because
native JSON char values are ASCII; ProtoJSON/TextProto use numeric char values
and do not require that omission. The type does not introduce a member name: the constant remains
`magic`. C++ emits an `inline static constexpr` constant of the declared type;
other backends use their existing scalar/enum type mapping and immutable static
constant convention. There is no mutable per-instance storage.

Typed magic uses the selected codec's ordinary scalar/enum value representation:

| Format | Typed magic representation |
| --- | --- |
| Positional and integer-key native binary | Unkeyed scalar value; enums use compact ordinals |
| String-key native binary | Unkeyed scalar value; enums use their usual length-prefixed name |
| Native JSON | Required `magic` key with its scalar value or declared enum name |
| C++ Protobuf binary, ProtoJSON, TextProto | Declared magic field ID with the corresponding scalar/enum mapping |

Native binary writes the typed constant before the payload version and ordinary
fields, with no additional magic field key, numeric ID, or collection count.
An enum's string length prefix belongs to its existing value encoding. Readers
validate and discard magic before reading the native binary payload. Keyed formats
require exactly one matching magic value; omissions remove that requirement for
the selected format. Floating constants are rounded to their declared type and
must remain finite. Legacy `magic {'SRLFILE'};` continues to generate the exact
fixed byte array, raw native binary prefix, and string JSON/Protobuf value.

Typed magic requires a `serializer version 1.1.0;` header in the file that declares
it, including an included file. A `1.1.0` entry file does not enable typed magic in
a dependency declaring `1.0.0` or the original `1` alias. This language requirement
also applies to [inferred fixed arrays](generics.md#infer-an-extent-from-defaults).

The ID defaults to the first unused identity. Specify it explicitly for durable
Protobuf contracts. Only one magic declaration is allowed in an unmanaged owning
class. It cannot have lifecycle annotations or conflict with an ordinary member
named `magic`. The legacy single-quoted initializer contains 1 through 64 decoded bytes,
must form valid UTF-8, and supports `\\`, `\'`, `\"`, `\n`, `\r`, `\t`,
`\0`, and exactly two hexadecimal digits after `\x`. Escaped NUL is an explicit
byte; there is no automatic terminator. Views and managed classes currently reject
magic explicitly.

Any ordinary field, including a collection or union, can use trailing
`omit(format, ...)` in an owning class. C++ view modes currently reject omissions
explicitly because their byte-backed field descriptors require a complete fixed
layout. The same annotation can omit magic. Supported selectors are:

| Selector | Output |
| --- | --- |
| `json` | Native JSON |
| `binary_none`, `binary_positional` | Native positional binary |
| `binary_integer` | Native integer-key binary |
| `binary_string` | Native string-key binary |
| `protobuf`, `protobuf_binary` | C++ Protobuf binary |
| `protojson` | C++ ProtoJSON |
| `textproto` | C++ TextProto |

Omitted fields remain available in application memory but contribute no wire
bytes, keys, or positional slots in the excluded format. A fresh reader preserves
their schema defaults. Explicitly supplying an excluded known keyed field fails;
omission does not authorize arbitrary unknown fields. Version discriminators
cannot omit formats, and unknown or duplicate format selectors fail generation.
Native backends support the four native codecs; Protobuf mappings remain a
separate C++ feature rather than additional codecs in every generated language.

C++ codecs expose a `static constexpr wire_format format` marker. Generated
omission handling uses compile-time protocol selection, so it introduces no
per-object flags or runtime format tests. Existing custom protocol key markers
remain supported. Other languages keep their established protocol-selecting APIs.
`binary_positional` is also a C++ alias of `binary_none`, with identical template
parameters and bytes.

Changing a magic type or value, changing its Protobuf ID, or changing omissions alters
the relevant format contract. Compatibility checks account for those selections;
use retained historical schemas and explicit migration when changing an existing
persisted representation. Regenerate all producers and consumers together.

The [7 October 2026 verification record](verification-magic-2026-10-07.md)
documents the earlier byte-magic native-language suite and editor package checks.
See the [inferred arrays and typed magic verification record](verification-array-magic-2026-10-07.md)
for the language `1.1.0` and extension `1.1.24` checks.
