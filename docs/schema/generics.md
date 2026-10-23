# Generic owning classes and dimensions

Serializer emits native C++ templates from ordinary generic declarations, including
declarations with no concrete schema uses. Managed declarations use concrete schema
specializations and generated editors instead; see [managed specializations](#managed-specializations). Other languages receive concrete models for applications
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
codec and the generated owning-value operations. C++23 is the minimum;
newer language modes use the same API. Naming profiles apply to class and member names;
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

## Owning C++ record map keys

Language `1.11.0` accepts generated owning record keys in `map<K,V>` and generic
key applications in the legacy `map(K) V` spelling. Both forms retain the same
map storage and native codec contract as the existing named-record key form:

```text
serializer version 1.11.0;
class entry_key<T> {
  public T value;
}
class catalog {
  public map<entry_key<uint64>, uint32> entries;
  public map(entry_key<uint64>) uint32 legacy_entries;
}
```

Select C++ class ordering for the key and any nested key records through the
[comparison settings](record_comparisons.md). Generic keys keep their arguments
through native template generation and concrete lowering. Other language outputs
and Protobuf retain their explicit scalar/enum key restrictions and diagnose
record keys before publishing generated output. Existing named `map(record)`
keys remain accepted under older schema language headers; the new spellings
require `1.11.0` in the file that declares them.

## Boolean type constraints

Language `1.10.0` supports category and exact-type predicates with `!`, `&&`,
`||`, and parentheses:

```text
serializer version 1.10.0;
class numeric_value<T: (integral || floating_point) && !bool = int32> {
  public T value;
}
class selected<T: int32 || double = int32> {
  public T value;
}
instantiate integer_value = numeric_value<int32>;
instantiate real_value = numeric_value<double>;
```

`integral` matches `bool`, `char`, and the supported signed/unsigned integer
types. `floating_point` matches `float` and `double`. A type name matches that
exact schema type, so `!bool` excludes booleans and `int32 || double` is an
allowed type list. Qualified generated records and enums can be exact-type
alternatives. `stateless` can also participate in Boolean constraints. `!`
binds before `&&`, which binds before `||`.

The parser checks known supplied/default arguments and concrete nested
applications for every selected output. A dependent category argument is checked
after its concrete substitution. Constraints belong to type parameters, not
dimension parameters, and add no wire metadata. The original atomic
`T: stateless` form retains its language `1.9.0` contract.

C++ open templates receive equivalent `requires` clauses. The numeric example
emits `(std::integral<T> || std::floating_point<T>) && !std::same_as<T, bool>`;
application uses with `bool` or `std::string` fail constraint checking. Other
languages receive the already checked concrete applications, as in the existing
generic workflow. Select only C++ in the output configuration when a schema is
intended for C++ alone; arbitrary native C++ concept names are not portable
constraint atoms. See the runnable [generic example](../../example/generics/README.md).
The named declaration ends with a semicolon; class declarations do not.

An application-only C++ specialization is invisible to other generators and to
schema compatibility analysis. Declare a contract when sharing it with another
language or checking its schema evolution. Native application-side generic APIs
in the other ten languages are not implemented. Unused generic declarations emit
C++ templates but no other-language models.

## Factory type constraints

Compiler/runtime `1.17.0` and schema language `1.12.0` add the explicit owning
factory constraint `factory(name)`:

```text
serializer version 1.12.0;
class identity_frame {
  public double diagonal {1.0};
  public factory identity() {
    diagonal = 1.0;
  }
}
class positioned_value<Frame: factory(identity) = identity_frame> {
  public uint32 count;
  public Frame placement;
  public constructor(uint32 count_input = 32, Frame placement_input = default(identity)) {
    count = count_input;
    placement = placement_input;
  }
}
```

For a known generated type, the constraint requires an owning record with a public
zero-argument named factory returning that exact owner. It is checked on supplied
and default concrete arguments, including nested applications. Factory predicates
can participate in Boolean type constraints; a generic parameter default
`default(identity)` requires a positive constraint that guarantees that specific
factory. An unrelated factory, negated requirement or unrestricted alternative
does not authorize the default.

C++ open templates receive a `requires` expression equivalent to
`{ Frame::identity() } -> std::same_as<Frame>`. Application C++ types can satisfy
that signature, while their actual storage, assignment, codec and lifetime
requirements remain independent. The factory returns a fresh value for each
omitted constructor argument. This allows identity-valued defaults without
assuming that arbitrary `Frame{}` has those semantics.

Open generic constructor parameter defaults require C++ output. Concrete owning
schema specializations retain portable fresh factory defaults. The factory predicate
does not promise `constexpr`, `noexcept`, execution registration or a managed
transaction. Check the selected implementation separately when those properties
matter. See [fresh generic defaults](construction.md#fresh-generic-factory-defaults)
and [authoring verification](../verification/verification-authoring-2026-10-11.md).

Language `1.16.0` separately allows `Frame placement = {}` on an owner's type
parameter without a factory or defaultability constraint. Native C++ initializes
`Frame{}` only for an omitted argument; explicit nondefault/nonassignable values
remain valid. Known supported owning generated records retain their portable
`1.15.0` defaults. Independent input deduction, function defaults and fixed-array
cardinality do not change. See [generic owner defaults](construction.md#generic-owner-parameter-defaults-in-language-116)
for the C++-only substitution boundaries and normal codec requirements.

## Independently generic C++ callables

Language `1.11.0` introduced numeric input parameters on construction declarations.
Language `1.12.0` allows those independent inputs inside an owning generated record
argument and accepts distinct C++ constructor overloads:

```text
serializer version 1.12.0;
class quantity<T: (integral || floating_point) && !bool = double> {
  public T value;
  public constructor<Input: (integral || floating_point) && !bool>(Input input) explicit {
    value = checked<T>(input);
  }
  public constructor<Input: (integral || floating_point) && !bool>(quantity<Input> input) explicit {
    value = checked<T>(input.value);
  }
  public function common_type<T, Input> scaled<Input: (integral || floating_point) && !bool>(Input factor) readonly
    expression(checked<common_type<T, Input>>(value) * checked<common_type<T, Input>>(factor));
}
```

Field projections substitute each record argument's independent input types.
Renaming an input parameter does not establish a distinct overload; duplicate
signatures and ambiguous shortened default signatures reject. Existing codec
default construction and native implicit copy/move behavior remain available.
These declarations preserve explicit strong-record conversion instead of weakening
the record's numeric input constraint.

Ordinary readonly C++ callables may also declare their own constrained input types.
The explicit scalar policy `common_type<T, Input>` lowers to
`std::common_type_t<T, Input>` for checked numeric representations. Both operands
must be bound with `checked` before arithmetic to preserve the intended lossless
mixed-representation contract. The policy adds no schema field type or automatic
quantity-record wrapper. Eligible templates keep `constexpr`; the selected
specialization and reached checked operations determine actual constant evaluation.
See [numeric behavior](behavior.md#constrained-readonly-inputs-and-result-representation)
for signed/unsigned, precision and division guarantees.

Independently generic construction/readonly APIs require C++ output. They do not
establish application-side generic methods in the other ten backends. Existing
class templates remain portable through their ordinary concrete schema applications.
Independent readonly inputs are deduced by host C++ callers; schema expression
calls to those generic functions diagnose as unsupported. Calls to ordinary
methods of a class-generic receiver retain their existing substitution contract.
Parameter defaults on independent generic input types remain unsupported; the
declared factory default above applies to class type parameters instead.

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
are the same as `array[N] T`. Compiler 1.11.0 supports explicit fixed extents
without nonempty initializers in all eleven native codec backends. Collection
initializers under the legacy language contract remain C++ only. Language 1.14+
typed aggregate defaults use the portable construction contract in supported
owning backends. Compiler/runtime 2.23.0 also maps fixed arrays to
[Protobuf repeated fields](../serialization/protobuf.md#mapping-contract), imposing
exact cardinality in generated C++ readers without adding wire metadata.

Generic definitions require unpacked owning storage without raw unions,
recursive ownership, variadics, or user specializations. Inheritance requires
language `1.8.0`, as described below. Managed definitions
are supported through concrete C++ specializations, as described below.
Language `1.4.0` owning `variant(...)` alternatives may reference generic type
parameters; C++ retains `std::variant<T, ...>` storage in the generated template.
Concrete schema uses retain the same native tagged-choice contract in other
backends. See [unions and variants](unions_and_variants.md).
Schema arguments require owning types; managed-capable classes are accepted, while
view classes remain unsupported. Activation still follows explicit managed occurrence
boundaries. Definition names resolve in
declaration scope; explicit arguments resolve in use-site scope. Definitions and
nondependent types must precede their uses, including through transitive includes.
Collections are field modifiers, not type arguments; wrap a collection in a class.

## Owning generated bases

Language `1.8.0` allows generic owning classes to inherit previously declared
concrete or generic generated classes. A concrete class may also inherit a
generic application:

```text
serializer version 1.8.0;
class metadata stable_ids {
  public uint32 revision (1);
}
class value_base<T> stable_ids : public metadata (10, "metadata") {
  public T value (1);
}
class annotated<T> stable_ids : public value_base<T> (10, "value") {
  public string label (1);
}
instantiate count_annotation = annotated<uint32>;
class reply stable_ids : public annotated<uint32> (10, "annotation") {
  public bool accepted (1);
}
```

C++ emits ordinary native inheritance and retains dependent arguments in template
base lists and codec calls. Application code accesses inherited public members
normally, for example `annotated<std::uint32_t>{}.value`. Concrete contracts in
other languages use their existing composed base records. Inheritance never
flattens fields onto a new wire layout: each base retains its declared occurrence
ID/name, nested payload and existing protocol encoding. Multiple bases follow the
existing owning inheritance rules; generic bases must be unpacked owning generated
classes. A bare type parameter cannot supply an arbitrary host base. Duplicate
concrete direct bases, forward references and recursive ownership reject.

Schema-native constructors and factories are copied with each concrete contract;
their parameter types are substituted and constructor/self return names bind to
the emitted owning class. Use these declarations for name-sensitive generic
construction. Opaque native code remains authored target text and is not renamed
or interpreted during specialization. See [behavior](behavior.md) for construction
syntax and default factories.

## Reusable C++ entry headers

Independently generated C++ entry headers may include the same schema. The compiler
guards individual enum/class/template definitions, concrete aliases, optional
application metadata and managed companions, so identical shared types appear once
per translation unit. Header order does not change the selected definition. Keep
generation options consistent for shared types; importing differing declarations
or output profiles under the same generated qualified name produces a clear
preprocessor error instead of silently selecting one definition. Native preambles
from a shared source are guarded independently as well.

These guards preserve all existing generated type names and wire identities.
They do not provide cross-language import modules; portable languages retain their
documented generated module/outer-class layout.

## Managed specializations

Language 1.23 adds [named managed generic self references](named_managed_references.md).
Direct copied token fields and supported container wrappers bind to the named
record; shared nested/base records and older instantiate contracts retain their
existing restrictions. Distinct names keep distinct target identities.

```text
serializer version 1.6.0;
class annotation<T = string> stable_ids managed {
  public T note (1);
  public transient uint64 cached_length {0};
}
instantiate string_annotation = annotation<string>;
```

```cpp
rohit::managed::model_store<string_annotation> notes{string_annotation{}};
notes.execute_transaction([](auto& tx) {
  tx.root().set_note("Review the introduction");
}).throw_if_failed();
notes.undo();
```

A named managed specialization is a concrete generated class with its own editor,
identity traversal and schema binding, rather than a C++ alias. An unused managed
generic definition does not emit an unconstrained application-side managed template.
Managed engines in other languages remain unsupported. Ordinary generic children
can remain values under an enclosing managed owner. Their replacement participates
in the owner's history without activating independent child identities.

Fixed managed child arrays keep their schema extent and allow ID-based `edit(id)`;
append/erase are unavailable. Direct and separate-values storage both perform the
required child traversal/conversion. The [document feature example](../../example/managed/document_features/README.md)
and generated tests exercise these contracts.

## Backend support

| Feature | C++ | JavaScript/TypeScript and Python | Other languages |
| --- | --- | --- | --- |
| Ordinary native generic declaration without concrete contracts | Native template and codecs | No model emitted | No model emitted |
| Concrete type/dimension applications without fixed arrays | Supported | Existing concrete records/codecs | Existing concrete records/codecs |
| Owning generic/concrete generated bases, language 1.8 | Native inheritance; inherited member access | Concrete composed base records | Concrete composed base records |
| Shared schema types in independently generated entry headers | Per-type and companion guards; incompatible definitions reject | Existing module layout | Existing module/outer-class layout |
| Fixed arrays without nonempty initializers | std::array, four native protocols | Array/list, four native protocols with exact extent checks | Native arrays/collections with exact extent checks |
| Fixed arrays with initializers or direct Protobuf output | Initializers supported; Protobuf rejected | Explicit generation-time unsupported diagnostic | Explicit generation-time unsupported diagnostic |
| Ordinary generic values in managed roots | Generated replacement setters/history/journals | Managed runtime remains unsupported | Managed runtime remains unsupported |
| Named managed generic specialization | Concrete class, identity traversal and typed editor | Managed runtime remains unsupported | Managed runtime remains unsupported |
| Constructor overloads and independent owning-record inputs, language 1.12 | Constrained native overloads | Explicit unsupported diagnostic | Explicit unsupported diagnostic |
| Generic factory constructor defaults, language 1.12 | Open factory-constrained APIs and concrete specializations | Concrete owning specializations with fresh defaults | Concrete owning specializations with fresh defaults |
| Constrained readonly inputs and common_type result, language 1.12 | Checked scalar native templates | Explicit unsupported diagnostic | Explicit unsupported diagnostic |

Portable fixed arrays initialize each element independently, preserving nested
class defaults without shared mutable values. Binary readers reject a count
different from the extent before decoding elements; JSON readers bound iteration
by the extent and reject short or extra arrays. Missing keyed fields retain
defaults. TypeScript uses ordinary `T[]` declarations and runtime cardinality checks.

Fixed-array diagnostics identify the backend, field, and schema byte offset.
There is no implicit variable-length fallback. The [qualification record](../verification/verification-dimensions-2026-10-04.md)
distinguishes native Windows results, WSL results, and remaining limitations.
See [portable fixed-array verification](../verification/verification-portable-fixed-arrays-2026-10-08.md)
for the later C++/JavaScript/Python wire and cardinality checks.

## Stateless type constraints

Language `1.9.0` can constrain a type parameter and its default:

```text
class empty_token {}
class counter<Reference: stateless = empty_token> {
  public uint32 count(1);
  public transient layout(empty) Reference reference;
}
```

`stateless` accepts generated unmanaged owning records with no stored members, parents,
managed identity, opaque hooks or polymorphic state. It checks defaults, concrete
and named instantiations, nested applications and forwarded type parameters.
Forwarding to a constrained parameter requires a matching stateless constraint.
C++ additionally checks the actual native type with `std::is_empty_v`; other
backends validate their generated concrete records. Physical overlap from
`layout(empty)` remains compiler/ABI dependent. The public
[stateless layout schema](../../example/schemas/stateless_layout/model.serializer)
contrasts constrained empty policies with ordinary active transient state.

Older unconstrained generic `layout(empty)` declarations retain their accepted
meaning and are checked when instantiated. The new constraint is an explicit
proof across generic forwarding; it does not change durable IDs or wire bytes.

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

See [usage](../guides/usage.md), [schema reference](schema_reference.md),
[wire format](../serialization/wire_format.md), [migration](../../migration.md), and the
[runnable cross-language generic examples](../../example/generics/README.md).
