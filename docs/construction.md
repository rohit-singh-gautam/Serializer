# Portable value construction and transient layout

Compiler/runtime 1.13 and schema language 1.8 add typed construction declarations.
They initialize ordinary owning values, with no new wire metadata or executable
payload. Parameters bind to exactly matching field types; implicit numeric
conversions, arbitrary code, and repeated use of an owned parameter reject.
Record parameters, including records inside optional, array, and map wrappers,
must enable an owning representation.
Unassigned fields keep their fresh schema defaults.

```text
serializer version 1.8.0;
class point {
  public double x;
  public double y;
  public constructor(double x_value, double y_value) {
    x = x_value;
    y = y_value;
  }
  public factory origin() {
    x = 2.0;
    y = 3.0;
  }
}
class segment {
  public point start default(origin);
  public point end;
  public factory starting(point input) {
    start = input;
  }
}
```

A constructor requires at least one parameter and cannot take its own record
type by value; named factories may accept that type. C++ emits the constructor and
retains default construction for codecs and containers. Every backend emits its
portable `create` factory; ordinary named factories use the selected naming
profile. C++ and Rust use owner-qualified static functions, Go uses
`Type_Factory`, and C uses `type_factory(destination, arguments...)` returning
`srl_status`. TypeScript declares the JavaScript static APIs. Kotlin factories
belong to the companion object. Private/protected construction is nonpublic
where the backend can express it; C cannot enforce schema access.

Assignments accept a parameter, a checked scalar/string/enum literal, or
`default(factory_name)` on an owning generated record. Record fields can select
the same public zero-argument factory with `default(factory_name)` after their
wire ID. Each invocation constructs fresh defaults. Missing keyed fields retain
the selected default; present fields still use their ordinary codecs. Choosing
a different default can change the meaning of an omitted field, so review
compatibility before changing defaults in a released schema.

C++ takes parameters by value and moves them into their bound fields. Rust and
Swift use native value semantics. Reference-based backends assign native
references; callers must transfer their ownership discipline or explicitly copy
an argument if they intend independent mutation. This does not provide a new
deep-copy API. C moves bound owned pointer arguments once after allocations
succeed, zeroing moved inputs; unused arguments remain owned by the caller.
Destinations must be initialized or zeroed, and owned
input objects must be disjoint from the destination and each other. A failed C
construction preserves both destination and inputs.

C++ marks eligible constructors, retained default constructors and factories
`constexpr` under its C++20 baseline. Scalars, enums, ordinary owning records,
strings, vectors, optionals, fixed arrays and dotted revisions support this declaration when their
contained values do. Literal bindings and `default(factory)` retain constant
construction. Open type parameters defer constant evaluation to the chosen C++
specialization; named schema instantiations use their resolved types. Allocating
standard-library values still follow C++ constant-evaluation lifetime rules.
Maps, variants, recursive storage, managed records, polymorphic behavior and
opaque class storage retain ordinary construction. No constant evaluation is
promised for those shapes, and their runtime APIs remain available.

In compiler/runtime 1.15.0, factories are `[[nodiscard]]` and construction with
proven scalar/enum/dotted defaults and bindings is `noexcept`. Allocating storage,
opaque defaults and unproven generic arguments retain potentially throwing APIs.
Constructors and factories remain callable with runtime inputs; `consteval`
would prevent that intended use.

C++ managed records retain scalar and ordinary owned field bindings in both
direct and separate-value representations. Separate generated payloads also
retain their owner-bound construction APIs; managed child occurrences continue
to require transaction APIs rather than factory assignment.

Open C++ generic definitions and concrete schema instantiations retain the
construction bindings. The compiler emits each actual owner name and return
type, rather than rewriting opaque native C++ tokens. Existing native blocks
remain verbatim; migrate self-naming constructors to these declarations when
using named concrete instances. Exactly matching schema-native optional, array,
and map parameters may bind ordinary owned fields. Modifier-based collection
and variant fields, inherited field assignment, view/packed construction, and
arbitrary construction expressions are outside this initial contract. Borrowing `cpp.emission_only`
models reject construction APIs, and structured defaults cannot embed their
owning record recursively.

## Extended bindings in language 1.11

Compiler/runtime 1.16.0 and schema language 1.11.0 add fixed-array parameter
bindings, trailing parameter defaults, explicit C++ construction, independent
numeric input types, and bounded checked scalar/projection bindings. Older
declarations retain their generated APIs and wire layouts. Each file using the
new syntax must declare `serializer version 1.11.0;`.

```text
serializer version 1.11.0;
class coordinate_vector<T, uint64 N> stable_ids {
  public array[N] T coordinates (1);
  public constructor(array[N] T input) explicit {
    coordinates = input;
  }
}
instantiate coordinate_triplet = coordinate_vector<double, 3>;

class positioned_value stable_ids {
  public double x (1);
  public double y (2);
  public constructor(double x_input, double y_input = 0.0) explicit {
    x = x_input;
    y = y_input;
  }
}

class numeric_value<T: (integral || floating_point) && !bool> stable_ids {
  public T value (1);
  public constructor<Input: (integral || floating_point) && !bool>(Input input) explicit {
    value = checked<T>(input);
  }
  public factory zero() {
    value = checked<T>(0);
  }
}
instantiate small_numeric_value = numeric_value<uint16>;
class value_with_default {
  public small_numeric_value value default(zero);
}
```

`array[N] T` parameters bind only exactly matching dimensioned fixed-array
fields declared by the owning record. Dimensions use the existing checked
literal/type-parameter grammar and bounds. C++ preserves `std::array<T, N>`;
the other languages keep their existing owning field containers and check the
argument cardinality before assigning it. C passes a pointer to the generated
collection type for the bound field, validates cardinality/storage, and moves
it only after candidate initialization succeeds. Wrong cardinality fails rather
than padding/truncating the field or changing its wire shape. This does not add
modifier-based variable arrays, maps, or variants to construction bindings.

Parameter defaults use `= literal` or `= default(factory_name)` on a concrete
owning record. All parameters after the first default must have defaults.
Scalar/string/enum defaults use the existing checked literal rules; record
defaults select an existing public zero-argument factory. Independent generic
input parameters and fixed-array parameters cannot have defaults. Record
defaults construct a fresh value on every invocation, including Python; no
shared mutable default is introduced.

Every backend emits named default overloads
`<factory>_default_<required_argument_count>` for each permitted shortened
argument list. A constructor's portable factory is named `create`, so an
all-default two-parameter declaration also provides `create_default_0()` and
`create_default_1(first)`. C uses its ordinary owner prefix and destination
argument. C++ additionally supports native trailing defaults; JavaScript,
Swift, Kotlin, and eligible scalar-only Python signatures use native defaults
where supported. The wrappers remain available uniformly. TypeScript declares
the corresponding JavaScript signatures. Factory names are converted by the
selected naming profile before the `_default_<count>` suffix is added.

The optional `explicit` suffix belongs only to constructor declarations. C++
emits an explicit constructor; portable named factories already require an
explicit call. A constructor with all parameters defaulted keeps its first
native C++ argument required, avoiding ambiguity with the retained codec default
constructor; `create()` and the named overloads expose the declared parameter
defaults. Default construction still initializes fresh schema field defaults,
not parameter defaults. Missing keyed fields therefore keep their established
codec semantics.

Independent constrained input types and checked expression bindings currently
require C++ output. Other output backends diagnose those declarations explicitly;
portable fixed-array/default bindings remain available. Select C++ alone when
using the `numeric_value` example. Existing class generics remain portable through
their concrete schema instantiations; open method-level input templates are a
separate native API feature.

`checked<T>(input)` requires a numeric input and destination representation,
excludes Boolean state, and rejects lossy conversion with `std::domain_error`.
It covers signed/unsigned integer ranges, fractional floating-to-integer input,
integer-to-floating precision, floating narrowing, and non-finite values without
silently promoting through binary64. See [checked numeric expressions](behavior.md)
and the public numeric runtime contract. Distinct generated quantity records
remain separate types; the numeric input constraint does not make them implicitly
convertible. A concrete record input may supply a readonly scalar projection,
such as `value = checked<int32>(input.value);`.

Construction expression bindings use the existing bounded expression AST and
checked numeric operations. Every result must exactly match its scalar field;
cross-representation conversion requires an explicit checked cast. Bindings may
read parameters and accessible generated record fields, with checked arithmetic
and guards. They cannot read partially initialized destination fields, call
arbitrary constructors/functions, index collections, iterate, execute callbacks,
or assign inherited fields. Record/collection expression results are rejected;
direct exactly matching owned parameters retain their existing move semantics.
A structured default can use a zero-argument factory whose scalar assignments
are checked expressions. Potentially failing bindings do not claim `noexcept`.

Focused generated fixtures exercise fixed native storage, scalar/projection
checks, full-width integer inputs, explicit conversions, fresh defaults, and
unchanged codec default construction. The portable runner
`test/extended_construction_languages.py` checks fixed arrays and default
overloads in selected SDKs. On 2026-10-11 its regenerated consumers compiled and
executed in C++, C, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift,
and Kotlin with the runner's strict SDK checks. The focused legacy/new C++
construction suite also passed; full-repository qualification is recorded
separately in the verification guide below.

## Stateless transient layout

```text
class empty_reference {}
class passive_record {
  public double radius;
  public transient layout(empty) empty_reference execution;
}
```

`layout(empty)` is a typed layout hint restricted to a transient generated record
without fields, bases, managed identity, native storage hooks, or polymorphic
methods. Active execution references and stateful records reject. The field
retains its generated API and normal transient reset behavior. It never enters
wire bytes, schema field paths, history, or journal payloads.

C++ emits `[[no_unique_address]]` (the supported MSVC spelling on MSVC) and
checks `std::is_empty_v` at compilation. Rust and Swift already represent
stateless records with zero-sized values. C and reference-based runtimes retain
their normal empty-record representation; the hint does not promise physical
overlap there. Alignment, repeated empty types, enclosing alternatives, and
ABI rules can still determine the containing stride. Check actual size/alignment
on the selected compiler instead of assuming a universal byte saving.

See [generic construction](generics.md), [behavior expressions](behavior.md),
[usage](usage.md), and [verification](verification-integration-enhancements-2026-10-11.md).
