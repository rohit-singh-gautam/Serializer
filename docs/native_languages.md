# Rust, Python, Swift, Kotlin, and C

The C++ compiler generates standalone owning codecs for these five targets. It
parses the schema once and can produce every supported language in one command.
There is no Python, Rust, JVM, or C# schema compiler, binding, or subprocess in
generation. Target SDKs are needed only to compile or run consumers.

The checked-in `src/*_runtime.inc` files are permanent C++ raw-string inputs to
the generators. Generated source embeds the required runtime. Applications do
not ship `.inc` files or require Serializer's C++ runtime for these targets.

## Generate

Create the output directory, then run the C++ `serializer` executable:

```sh
serializer --input example/interoperability/message.serializer \
  --language rust,python,swift,kotlin,c \
  --rust.output build/schema.rs --python.output build/schema.py \
  --swift.output build/Schema.swift --kotlin.output build/Schema.kt \
  --c.output build/schema.h --kotlin.package example.models
```

Append `cpp,java,js,typescript,go,csharp` and their destination options to produce
all outputs in that same invocation. Repeating `--language` also works.
`--output` accepts one target. Included declarations are emitted together; the
same depfile and publication validation apply to every backend. With CMake,
`serializer_generate_source(... LANGUAGE rust OUTPUT .../schema.rs)` and the
equivalent language names use the existing generation helper.

Kotlin defaults to the unnamed package; `--kotlin.package=` clears a configured
package. INI configuration supports `[kotlin]` / `package = example.models`.
Rust, Python, Swift, and C need no backend-specific INI section. The new targets
use their native naming conventions; configurable naming profiles are currently
provided only by the earlier backends. Place one generated schema module in
each consumer module/compilation unit: independently generated files embed
runtime names, so combining overlapping schemas in one module is unsupported.

## Models and SDKs

| Target | Generated source and runtime | Model and field names | Collections |
| --- | --- | --- | --- |
| Rust | Rust 2021, standard library | `InteropMessage`, `big_unsigned` | `Vec<T>`, `BTreeMap<K,V>` |
| Python | Python 3.10+, standard library | `InteropMessage`, `big_unsigned` | `list`, `dict`; models use `__slots__` |
| Swift | Swift 6, Foundation | `InteropMessage`, `bigUnsigned` | Arrays and dictionaries |
| Kotlin | Kotlin/JVM 2.0+, standard library and JDK | `InteropMessage`, `bigUnsigned` | Primitive numeric arrays, `MutableList`, `MutableMap` |
| C | C11, standard library, IEEE binary32/binary64 | `interop_message`, `big_unsigned` | Typed owning arrays and map-entry arrays |

The tested versions and platforms are in the [verification record](verification-native-2026-09-18.md).
Minimum versions other than those tested need their own qualification. Kotlin/JS,
Kotlin/Native, `no_std` Rust, Python extensions, and Apple device packaging are
not separate qualified targets in this change.

Schema namespaces flatten into type names. Parents are composed as `base0`,
`base1`, etc. Fixed-width integer types preserve all bits, including `uint64`;
Python uses checked arbitrary-precision integers, Kotlin uses unsigned types,
and C uses `<stdint.h>`. `char` is a byte (JSON accepts ASCII only). Native enums
preserve schema ordinals. A union uses an index and typed alternative fields;
only its active alternative appears on the wire.

Swift string **map keys** use `WireString`, whose equality, hashing, and ordering
use exact UTF-8 bytes. Swift's ordinary `String` equality can equate differently
encoded Unicode sequences; preserving these separate wire keys requires the
wrapper. It accepts string literals; construct dynamic keys with
`WireString(text)` and retrieve text with `.value`. Ordinary string fields use
`String`.

Rust, Swift, and Kotlin hide private/protected fields within generated models.
Python uses a leading underscore by convention. C exposes fields in structs;
schema access modifiers do not provide C access control.

## Encode and decode

All five support JSON, positional binary, integer-key binary, and string-key
binary. These use the existing [wire contract](wire_format.md), including
little-endian scalars, compact lengths, stable IDs, enum contexts, and map order.

```rust
let value = schema::InteropMessage::default();
let bytes = value.encode(schema::Protocol::BinaryInteger)?;
let copy = schema::InteropMessage::decode(&bytes, schema::Protocol::BinaryInteger)?;
```

```python
from schema import InteropMessage, Protocol
value = InteropMessage()
bytes_ = value.encode(Protocol.BINARY_INTEGER)
copy = InteropMessage.decode(bytes_, Protocol.BINARY_INTEGER)
```

```swift
var value = InteropMessage()
let bytes = try value.encode(.BINARY_INTEGER)
let copy = try InteropMessage.decode(bytes, .BINARY_INTEGER)
```

```kotlin
val value = InteropMessage()
val bytes = value.encode(Protocol.BINARY_INTEGER)
val copy = InteropMessage.decode(bytes, Protocol.BINARY_INTEGER)
```

```c
interop_message value = {0}, copy = {0};
srl_buffer bytes = {0};
srl_status status = interop_message_init(&value);
if (status == srl_ok) status = interop_message_encode(&value, srl_binary_integer, &bytes);
if (status == srl_ok) status = interop_message_decode(&copy, bytes.data, bytes.size,
                                                   srl_binary_integer, NULL);
interop_message_free(&copy);
interop_message_free(&value);
srl_buffer_free(&bytes);
/* Check status before using decoded data. */
```

Decoding requires exact input consumption, apart from trailing JSON whitespace,
and returns a fresh model. C instead replaces an initialized or zeroed destination
only after complete success. C encoding similarly replaces an initialized/zeroed
`srl_buffer` only on success. Call `*_init` only on fresh storage; release a live
model with `*_free` first. Strings and collection elements own their allocations;
do not shallow-copy owning C models. Use `srl_string_set` for length-bearing
strings (including embedded NUL), and `srl_string_set_cstr` for ordinary C text.
See the [C example](../example/c/interoperability/main.c) for collection construction.

Default decoding limits are 64 MiB per message, 16 MiB per string, one million
cumulative collection elements, and 64 object levels. Pass `Limits` as the third
decode argument in Python/Swift/Kotlin, use Rust `decode_with_limits`, or pass
`srl_limits` to C. Encoder nesting is also capped at 64 levels. Kotlin throws
`SerializerException`; Swift throws `SerializerError`; Rust returns `Result`;
Python raises validation exceptions; C returns `srl_status`. Native allocation
failure is not uniformly recoverable across managed runtimes or Rust containers.

Unknown keyed fields, malformed UTF-8, invalid enums/unions, truncated input,
invalid JSON numeric ranges, and trailing non-whitespace input are errors.
Repeated nested objects merge; a complete collection replaces its predecessor;
duplicate map keys keep the last complete value. JSON floating values must be
finite. Binary floating payloads allow nonfinite values. A schema shared with
every backend must use the common supported subset: owning classes, supported
map keys, and portable literal defaults. Views, packed output, compression, and
Protobuf remain C++ features. C float text conversion follows the active C
locale while emitting a period; callers must not concurrently change the global
locale during codec calls.

## Performance and qualification

Codecs access typed fields directly and parse JSON tokens without constructing
generic object trees. Field keys and positional metadata are cached; numeric
Kotlin arrays avoid per-element boxing and unnecessary final copies. Rust
decoding borrows its input; Python accepts a `memoryview`. Collection reservation
checks input lengths and limits first. C uses geometric growth, stack scratch
space for ordinary numeric tokens, and sorts map entry pointers without copying
owned values. C duplicate-map normalization is O(n log n).

These are implementation choices, not claims of equal throughput across runtimes.
The [examples](../example/README.md) include warmed local benchmarks where
provided. Compare optimized builds on the same machine and realistic payloads;
keep canonical-byte and malformed-input checks enabled when optimizing codecs.

The shared test corpus exercises every truncated prefix, exact decode, limits,
integer extrema, Unicode, duplicate maps, repeated fields, and C failure
transactionality. The full matrix covers ten runtimes plus a typed TypeScript
consumer: **11 × 11 × 4 protocols × 3 variants = 1,452 exchanges**.
