# JavaScript, Go, and C# output

The `.serializer` parser, schema validation, and every code generator are written
in **C++20**. The compiler emits standalone owning codecs for C++, Java,
JavaScript, Go, and C#. Generating JS, Go, or C# requires no target-language SDK,
native binding, reflection, or third-party runtime. Target-language tools are
needed only to compile or execute the generated application.

The C++ implementation is in `src/portable_writer.cpp`. The checked-in
`js_runtime.inc`, `go_runtime.inc`, and `csharp_runtime.inc` files contain C++ raw
string literals holding runtime source, following `java_runtime.inc`. They are
permanent generator inputs, not temporary output or separate schema compilers.
Generated files embed their runtime and do not need the `.inc` files at runtime.

## Generate from one schema

Create the destination directory first, then run:

```sh
serializer --input example/interoperability/message.serializer \
  --language cpp,java,js,typescript,go,csharp \
  --cpp.output build/message.hpp --java.output build/Schema.java \
  --js.output build/schema.mjs --typescript.output build/schema.d.mts \
  --go.output build/schema.go --go.package generated \
  --csharp.output build/Schema.cs --csharp.namespace SerializerGenerated
```

PowerShell users can put the command on one line or use backticks for line
continuation. `typescript` emits declarations for the JS module, not a distinct
runtime or protocol. Match `schema.mjs` with `schema.d.mts`, or ES-module
`schema.js` with `schema.d.ts`. For `.js` in Node, configure `"type": "module"`.
Select identical `js.naming` settings for JS and declarations.

`--output` is also supported when selecting exactly one language. The compiler
validates and generates every requested output before replacing destination
files. Includes and dependency files work for all backends. See the
[command-line reference](command_line.md) for precedence and path validation.

```ini
[output]
language = js, typescript, go, csharp
[js]
naming = profile
[go]
naming = profile
package = generated
[csharp]
naming = profile
namespace = SerializerGenerated
```

These backends use native presentation without an external formatter. `naming =
preserve` keeps valid identifiers; keywords, generated helper collisions, and
ambiguous names fail generation. Public Go fields must remain exported.

## Names and type mappings

Schema namespaces are flattened into qualified type names for the three new
backends: `interop::message` becomes `InteropMessage`. Naming collisions caused
by flattening are rejected. Go puts types in the configured package. C# nests
types in an outer class named after the output file, inside the optional
configured namespace. `Schema.cs` therefore exposes
`SerializerGenerated.Schema.InteropMessage`. JS exports types from its module.

| Schema | JavaScript / TypeScript | Go | C# |
| --- | --- | --- | --- |
| `int8..int32`, `uint8..uint32` | Range-checked `number` | Corresponding fixed-width integer | Corresponding signed/unsigned integer |
| `int64`, `uint64` | Range-checked `bigint` | `int64`, `uint64` | `long`, `ulong` |
| `char` | Byte-valued `number` | `byte` | `byte` |
| `float`, `double` | `number`, rounded to float32 where required | `float32`, `float64` | `float`, `double` |
| `bool` | `boolean` | `bool` | `bool` |
| `string` | `string`, strict Unicode | `string`, strict UTF-8 | `string`, strict Unicode |
| `array T` | `T[]` | `[]T` | `List<T>` |
| `map(K) T` | `Map<K, T>` | `map[K]T` | `Dictionary<K, T>` |
| Class field | Generated class | Pointer to generated struct | Generated class |
| Enum | Frozen named ordinal object, typed declaration | Named integer type and constants | Enum |
| Parent | Composed `base0`, `base1`, ... | `Base0`, `Base1`, ... | `Base0`, `Base1`, ... |
| Union `payload` | `payloadIndex`, `payloadNumber`, ... | `PayloadIndex`, `PayloadNumber`, ... | `PayloadIndex`, `PayloadNumber`, ... |

JS uses lower-camel fields. Go and C# public fields use upper-camel names.
Private/protected fields become JS private fields, unexported Go fields, and
private C# fields. Parent composition preserves wire order and identity; it does
not establish subtype inheritance. Only the selected union alternative is
serialized; inactive fields remain in memory.

## JavaScript and TypeScript

The generated module targets modern browsers and Node.js with ES2020 BigInt,
`DataView` BigInt accessors, `TextEncoder.encodeInto`, and fatal UTF-8 decoding.
It contains no Node-specific imports. The current verification uses Node 22
and a Chromium browser; older engines require separate qualification.

```js
import {InteropMessage, Protocol, Limits} from './schema.mjs';

const value = new InteropMessage();
value.bigUnsigned = 18446744073709551615n;
const bytes = value.encode(Protocol.BINARY_INTEGER);
const copy = InteropMessage.decode(bytes, Protocol.BINARY_INTEGER,
  new Limits(1024 * 1024, 64 * 1024, 10000, 32));
```

Both `Uint8Array` and Node `Buffer` input work, including nonzero-offset slices.
The returned encoding owns its bytes. The decoder borrows input only during its
synchronous call and returns an independent owning value.

The custom JSON token reader preserves exact 64-bit integers and duplicate-field
order. Serializer JSON emits integer tokens, including full-width `bigint`
values; it does not quote integers or use `$bigint` wrappers. Use the generated
codec rather than `JSON.stringify`/`JSON.parse` on the whole message. Display
64-bit fields using `.toString()` when needed. Binary float NaN payload identity
is not promised by the JavaScript number representation.

The [browser example](../example/javascript/interoperability/browser.html) uses
the same module as Node and reads C++-produced bytes. In React or Next.js, import
the generated module and decode a `Uint8Array` from `response.arrayBuffer()`.
Framework server/client transfer rules remain separate from Serializer's wire
contract. No React, Next.js, or WebAssembly adapter is implemented by this change.

## Go

Generated sources target **Go 1.22+** and use only the standard library. Generate
one combined entry schema per Go package; each standalone output embeds package
runtime declarations. Use includes to combine shared types in that entry.

```go
value := generated.NewInteropMessage()
value.BigUnsigned = math.MaxUint64
bytes, err := value.Encode(generated.BINARY_INTEGER)
if err != nil { return err }
copy, err := generated.DecodeInteropMessage(bytes, generated.BINARY_INTEGER)
```

Use `New<Type>()` to apply schema defaults and initialize nested objects and maps.
The Go zero value can differ from schema defaults. Nil slices/maps encode empty
collections; nil class pointers fail encoding. Public encode/decode methods
return errors; failed decoding returns a nil object. An optional final `Limits`
argument overrides `DefaultLimits()`. Input must not change concurrently with
decoding. Maps are sorted explicitly during encoding, independently of insertion
and iteration order. Generated code can be formatted with `gofmt` in the consumer.

## C#

Generated sources target **.NET 8+** with nullable reference types enabled. They
use the standard library, `ArrayBufferWriter<byte>`, spans, and `BinaryPrimitives`.

```csharp
using static SerializerGenerated.Schema;

var value = new InteropMessage { BigUnsigned = ulong.MaxValue };
byte[] bytes = value.Encode(Protocol.BINARY_INTEGER);
InteropMessage copy = InteropMessage.Decode(bytes, Protocol.BINARY_INTEGER,
    new Limits(maxBytes: 1024 * 1024));
```

`Decode` accepts `ReadOnlyMemory<byte>`, including array segments. Encoding returns
independent bytes. Null values have no wire representation; retain initialized
fields or replace them with valid values. Validation failures throw exceptions;
decoding never returns the partially built object. Generated owning types are
sealed; private/protected schema access becomes private because parents are
composed rather than inherited.

## Compatibility and limits

All five languages implement `JSON`, `BINARY_NONE`, `BINARY_INTEGER`, and
`BINARY_STRING` on the common owning subset. Binary encoding uses the existing
little-endian contract and Serializer compact prefixes, with no extra header.
JSON maps remain arrays of `{key, value}` entries. JSON escaping and float text
formatting can differ across implementations while representing the same data.
See the full [wire contract](wire_format.md).

Portable defaults are decimal numeric literals, booleans, quoted strings with
ordinary escapes, ASCII character literals, and declared enum constants.
Arbitrary C++ expressions, collection/union defaults, direct self-containing
owning defaults, views, and `packed` output are rejected. Map keys support
integers, characters, booleans, strings, and enums; floating-point and object
keys are unsupported. Binary strings must be valid UTF-8 in every backend,
including C++; malformed byte strings are rejected on both encoding and decoding.
JSON characters must be ASCII. Use `array uint8` for arbitrary byte payloads.

Missing keyed fields retain schema defaults. Duplicate fields apply in order;
ordinary nested fields merge, collections replace, selected union payloads start
fresh, and duplicate map keys keep the last complete entry. Unknown fields,
enums, and union alternatives fail. `stable_ids` preserves identity; it does not
enable unknown-field skipping. All new public decoders require exact message
consumption, with trailing JSON whitespace permitted.

| Portable decode limit | Default |
| --- | --- |
| Input message | 64 MiB |
| One string, including escaped JSON content | 16 MiB |
| Cumulative collection entries | 1,000,000 |
| Object nesting | 64 |

Encoding also bounds object nesting to 64, including object cycles. Limits bound
input and logical entries, not exact heap use. They are separate from C++'s
additional accounted-storage/work budgets. Source objects and input bytes must
not be mutated concurrently with codec calls. Output-buffer reuse, borrowed
views, big-endian codecs, compression APIs, Protobuf, and SIMD acceleration are
not implemented by these new backends.

## Build integration and verification

The installed CMake package and source build provide:

```cmake
serializer_generate_source(TARGET application_go
  SCHEMA schemas/message.serializer
  LANGUAGE go
  OUTPUT generated/message.go
  OPTIONS --go.package application)
```

The helper generates sources only. Attach `go build`, `dotnet build`, or your
JavaScript build as a consumer step. `CONFIG`, `GENERATOR`, and `DEPENDS` work as
in `serializer_generate_java`; transitive includes are tracked with depfiles.
Cross builds must supply a host compiler executable. `serializer_generated_sources`
is the aggregate generation target. Native npm/Go/MSBuild consumers can invoke
the same C++ executable directly during their build; no compiler port is required.

Enable `SERIALIZER_BUILD_INTEROP_EXAMPLES=ON` for the runnable
[five-language example](../example/interoperability/README.md). Its CTest covers
**5 producers × 5 consumers × 4 protocols × 3 union selections = 300 exchanges**,
checks complete values through independent fixtures, and compares binary bytes.
With normal tests enabled, additional tests cover malformed inputs, truncation,
Unicode, numeric limits, exact consumption, duplicate fields, and resource limits.
TypeScript and browser tests are added when their tools are found. The optional
benchmark commands report encoded sizes and local throughput; Go's allocation
benchmark also reports bytes and allocations per operation.

Pre-encoded schema keys, native switch dispatch, checked binary-array capacity,
direct field access, explicit endian primitives, and reusable JS `DataView`s
reduce avoidable work. Generated codecs do not use reflection or an intermediate
generic object tree. These choices do not establish performance parity with C++.
See the [verification record](verification-multilanguage-2026-09-17.md) for the
tested environments, measurements, and outstanding qualification.

## Additional native targets

Rust, Python, Swift, Kotlin, and C now use the same C++ compiler and wire protocols.
See [native language APIs](native_languages.md) and [four examples per language](../example/README.md).
The full opt-in suite expands the five-runtime subset above to all ten runtimes
plus TypeScript, with 1,452 producer/consumer exchanges.
