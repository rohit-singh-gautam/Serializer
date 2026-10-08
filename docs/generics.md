# Generic owning classes and dimensions

Serializer emits native C++ templates from generic declarations, including declarations
with no concrete schema uses. Other languages receive concrete models for applications
in schema fields or optional `instantiate` declarations. Both routes preserve the
existing native wire protocols.

## Native C++ templates

```text
serializer version 1;
class result<T> stable_ids {
  public T value (1);
  public bool success (2);
}
class response<T> stable_ids {
  public result<T> result (1);
}
class message<T = uint32> stable_ids {
  public response<T> response (1);
}
```

After generation, C++ application code can use `response<std::uint32_t>` or
`message<std::string>` directly and call the ordinary `serialize_out`, `serialize_in`,
`serialize`, `deserialize`, and `deserialize_exact` APIs. No `instantiate` declaration
or concrete schema field is required. Host type arguments must support the selected
codec and the generated owning-value operations. C++20 remains the minimum; C++23
consumers use the same API. Naming profiles apply to class and member names;
template parameter names retain their declared spelling.

## Optional cross-language contracts

```text
instantiate count_result = result<uint32>;
class service_reply stable_ids {
  public response<uint32> response (1);
}
```

These optional contracts tell the shared compiler which concrete types to emit for
Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin, and C. C++ may also
use them. `count_result` remains a distinct named class with the same fields and
bytes as `result<std::uint32_t>`; ordinary fields use the native C++ specialization.
The named declaration ends with a semicolon; class declarations do not.

An application-only C++ specialization is invisible to other generators and to
schema compatibility analysis. Declare a contract when sharing it with another
language or checking its schema evolution. Native application-side generic APIs
in the other ten languages are not implemented. Unused generic declarations emit
C++ templates but no other-language models.

## Parameters, defaults, and fixed arrays

```text
class point<uint64 N> stable_ids {
  public array[N] double coordinates (1);
}
class matrix<uint64 Rows, uint64 Cols = Rows, T = double> stable_ids {
  public array[Rows * Cols] T elements (1);
}
class frame<uint64 N> stable_ids {
  public point<N> origin (1);
  public matrix<N> basis (2);
}
instantiate matrix_3 = matrix<3>;
```

Bare parameter names denote serializable types; `uint64 Name` denotes a positive
integer dimension. Bind arguments left to right. Defaults are trailing and may
refer to earlier parameters of the appropriate kind; self/forward references,
zero dimensions, and kind mismatches reject. Type defaults may be visible types
or supported generic applications. `matrix<3>`, `matrix<3,3>`, and
`matrix<3,3,double>` share one specialization. Parameter renaming does not change
its schema identity. Changing a default affects omitted arguments only.

Dimension expressions accept decimal literals, dimension parameters, parentheses,
`+`, and `*`, with multiplication precedence and left associativity. Arithmetic
checks every intermediate result in uint64; zero may occur inside an expression
but cannot be a bound dimension or final extent. Negative values, overflow,
division, shifts, casts, and arbitrary host expressions in schemas reject.

`array T` remains variable-length. `array[expression] T` is fixed and may also
appear in non-generic owning classes. C++ uses value-initialized `std::array<T,N>`.
Generated consteval helpers check dependent arithmetic and extents before storage
instantiation. Each extent is 1..65,536; expression parsing permits 32 levels and
256 nodes (including parenthesized groups). Schema type parsing/expansion retain
32 levels, 1,024 concrete applications, and 4,096 canonical identity bytes. Nested
minimum inline storage must fit the generator host's ptrdiff_t range. Native C++
instantiations also obey the target compiler's object-size limits.

### Infer an extent from defaults

With schema language `1.1.0`, omit the extent expression to infer it from a
nonempty initializer:

```text
serializer version 1.1.0;
class header stable_ids {
  public array[] uint32 values (1) {1, 2, 3};
  public array[] char signature (2) {'SRLFILE'};
}
```

These fields generate `std::array<std::uint32_t, 3>` and `std::array<char, 7>`.
A char text literal uses its decoded byte count, including explicit escaped NUL
bytes and excluding any implicit terminator. Element lists infer their element
count. UTF-8 text contributes bytes rather than Unicode character count; ordinary
char-array JSON encoding still requires ASCII elements. Strings containing commas
and nested aggregate delimiters still count
as single top-level elements; class and generic aggregate defaults are supported
when they are valid C++ initializers for the declared element type.
Missing/empty initializers, malformed literals, invalid element defaults,
and inferred extents beyond 65,536 reject. The element type remains explicit;
extent inference does not deduce it. Dependencies need their own `1.1.0` header.
The legacy `1` header remains `1.0.0` and rejects empty extent syntax.

The inferred extent is fixed at generation time. Its native C++ storage,
wire count, cardinality checks, compatibility analysis, and backend limitations
are the same as `array[N] T`. JavaScript/TypeScript and Python support explicit
extents without initializers from compiler 1.6.0; collection initializers,
including inferred extents, remain unsupported in those backends. Other language
outputs and direct Protobuf generation continue to reject fixed arrays.

All generic definitions require unpacked, unmanaged owning storage, without
inheritance, unions, recursive ownership, variadics, or user specializations.
Schema arguments cannot be managed/view classes. Definition names resolve in
declaration scope; explicit arguments resolve in use-site scope. Definitions and
nondependent types must precede their uses, including through transitive includes.
Collections are field modifiers, not type arguments; wrap a collection in a class.

## Backend support

| Feature | C++ | JavaScript/TypeScript and Python | Other languages |
| --- | --- | --- | --- |
| Native generic declaration without concrete contracts | Native template and codecs | No model emitted | No model emitted |
| Concrete type/dimension applications without fixed arrays | Supported | Existing concrete records/codecs | Existing concrete records/codecs |
| Fixed arrays without initializers, including point/frame/matrix above | std::array, four native protocols | Array/list, four native protocols with exact extent checks | Explicit generation-time unsupported diagnostic |
| Fixed arrays with initializers or direct Protobuf output | Initializers supported; Protobuf rejected | Explicit generation-time unsupported diagnostic | Explicit generation-time unsupported diagnostic |
| Ordinary generic values in managed roots | Generated replacement setters/history/journals | Managed runtime remains unsupported | Managed runtime remains unsupported |

Portable fixed arrays initialize each element independently, preserving nested
class defaults without shared mutable values. Binary readers reject a count
different from the extent before decoding elements; JSON readers bound iteration
by the extent and reject short or extra arrays. Missing keyed fields retain
defaults. TypeScript uses ordinary `T[]` declarations and runtime cardinality checks.

Fixed-array diagnostics identify the backend, field, and schema byte offset.
There is no implicit variable-length fallback. The [qualification record](verification-dimensions-2026-10-04.md)
distinguishes native Windows results, WSL results, and remaining limitations.
See [portable fixed-array verification](verification-portable-fixed-arrays-2026-10-08.md)
for the later C++/JavaScript/Python wire and cardinality checks.

## Wire and compatibility behavior

Generic parameters add no wire metadata. Fixed arrays retain the existing sequence
count: identical elements produce identical binary_none, binary_integer,
binary_string, and JSON bytes to an ordinary array. Present fields must have
exactly the declared cardinality. Missing keyed fields retain existing destination
values; fresh values retain value-initialized/default storage. Explicit empty
arrays fail when the extent is positive. Positional fields cannot be omitted.

Decoders retain input, allocation, collection, work, and depth budgets even for
fixed storage. Exact fresh-value decoding rejects trailing input and does not
publish partial values. Ordinary in-place decoding retains its documented partial
update behavior. Fixed-array input currently uses scalar element decoding; output
can use the existing endian-aware bulk scalar path.

Compatibility checks compare lowered fields and extents. Defaulted/explicit forms
and parameter renaming are equivalent. Different extents are incompatible; a
variable writer cannot satisfy a fixed reader, while a fixed writer can satisfy a
variable reader under the existing element/protocol rules. Named instantiations
remain useful reservation targets. Open templates alone are not concrete wire
contracts and are excluded from this check.

See [usage](usage.md), [schema reference](schema_reference.md),
[wire format](wire_format.md), [migration](../migration.md), and the
[runnable cross-language generic examples](../example/generics/README.md).
