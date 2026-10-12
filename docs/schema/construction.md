# Portable value construction and transient layout

Language `1.17.0` adds [array and callable APIs](array_and_callable_api.md).
Use explicit storage/axes, owner field factories and callable-local restrictions
to preserve supported host contracts. Target limits and completed qualification
are recorded with the feature; older contracts below retain their semantics.

Language `1.16.0` adds empty constructor/factory defaults on an owning class's
type parameters. Native C++ defaults are instantiated only when omitted, so
explicit nondefault/nonassignable representations remain valid without a factory
constraint. See [generic owner defaults](#generic-owner-parameter-defaults-in-language-116).

Language `1.15.0` adds empty class-valued constructor/factory parameter defaults
for records with fields, including nested empty record literals. See
[the default contract below](#empty-class-valued-parameter-defaults-in-language-115).

Language `1.14.0` adds direct C++ member initialization, typed recursive defaults,
reciprocal constructor signature resolution, and external/defaulted constructor
declarations. See [owning APIs](owning_api.md) for the complete contract and
[construction defaults](../../example/cpp/construction_defaults/README.md) for
fresh unit-Z values, custom coordinates and codec parity. Older schemas retain
their previous lowering and default contracts.

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

A schema-defined binding constructor requires at least one parameter and cannot take its own record
type by value; named factories may accept that type. C++ emits the constructor and
retains default construction for codecs and containers. Every backend emits its
portable `create` factory; ordinary named factories use the selected naming
profile. C++ and Rust use owner-qualified static functions, Go uses
`Type_Factory`, and C uses `type_factory(destination, arguments...)` returning
`srl_status`. TypeScript declares the JavaScript static APIs. Kotlin factories
belong to the companion object. Private/protected construction is nonpublic
where the backend can express it; C cannot enforce schema access.

C++ schema constructors also generate a public defaulted zero-argument
constructor unless language `1.14.0` explicitly declares one; no separate schema `constructor()` or native `owner() = default;`
declaration is needed. Default construction initializes fresh schema field
defaults; it does not supply default arguments to a parameterized constructor.
Opaque constructors inside `cpp { ... }` remain verbatim and alone do not trigger
that generated declaration. Retaining a native parameterized constructor may
therefore still require an explicit native default constructor under C++ rules.

Assignments accept a parameter, a checked scalar/string/enum literal, or
`default(factory_name)` on an owning generated record. Record fields can select
the same public zero-argument factory with `default(factory_name)` after their
wire ID. Each invocation constructs fresh defaults. Missing keyed fields retain
the selected default; present fields still use their ordinary codecs. Choosing
a different default can change the meaning of an omitted field, so review
compatibility before changing defaults in a released schema.

C++ takes parameters by value, delegates to the default constructor, and then
move-assigns them into their bound fields. The owner must be default-constructible
using its field/base initializers, and bound fields must support assignment;
verify those requirements when migrating native
member-initializer construction. Rust and Swift use native value semantics.
Reference-based backends assign native
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

## Empty class-valued parameter defaults in language 1.15

Compiler/runtime `1.20.0` and language `1.15.0` allow `= {}` on an owning generated
record parameter in a constructor or named factory. Omitting the argument creates
a fresh instance using its schema field defaults, without invoking parameterized
authoring constructors or their validation/initialization hooks. This includes
known generic schema record applications such as `area<T>`; under the `1.15.0`
contract it does not default an unconstrained type parameter such as `Value`.

```text
serializer version 1.15.0;
class area stable_ids {
  public int32 width (1) {10};
  public int32 height (2) {20};
}
class rectangle stable_ids {
  public int32 x (1);
  public int32 y (2);
  public area extent (3);
  public constructor(int32 x_value, int32 y_value, area input = {}) {
    x = x_value;
    y = y_value;
    extent = input;
  }
  public factory sized(int32 x_value, int32 y_value, area input = {10, 20}) {
    x = x_value;
    y = y_value;
    extent = input;
  }
}
```

With the default naming profile, JavaScript's `Rectangle.create(1, 2)` receives a
new `Area` whose width and height are `10` and `20`; C++ exposes
`rectangle::create(1, 2)`. A supplied
`area` retains its supplied values. Portable
`create_default_2(1, 2)` and `sized_default_2(1, 2)` wrappers expose the same
omitted-argument behavior in every backend's naming profile; C also takes the
destination argument. Native shortened signatures remain backend-specific.
Mutable records are never shared across omitted calls, including in Python.

Full positional record defaults such as `area input = {10, 20}` already work
in language `1.14.0`. They require exactly the supported fields in declaration
order. The new empty form chooses all field defaults instead; partial defaults
such as `{10}` and named forms such as `{width: 10}` are unavailable. Nested
empty literals for records with fields in positional parameter defaults also
require `1.15.0`.
An actually empty record's already-valid `1.14.0` default remains valid under
that header. Earlier schemas retain their existing default semantics.

Record literal defaults require ordinary unpacked owning generated records
without bases, managed storage or native storage. Method-only native blocks
qualified with `storage(none)` remain eligible. The empty default form requires
public default construction. Fixed arrays retain their exact cardinality
requirements; `{}` does not request an empty or zero-filled
fixed-array parameter. All parameters after the first default must still have
defaults. Ordinary member/namespace function parameter defaults are unavailable.
Use `default(factory_name)` for the existing explicit public zero-argument
factory contract, including constrained generic factory defaults.

An external public zero-argument constructor retains its host-defined behavior;
this feature does not inspect or suppress that implementation. Empty defaults
apply to constructor/factory parameters and do not change member initializer syntax.

Parameter defaults change authoring APIs only. They introduce no field IDs,
payload revisions or wire metadata and leave codec default construction and
missing-field decode behavior unchanged. See [owning APIs](owning_api.md) for
the `1.14.0` positional-literal and direct-construction contracts.

## Generic owner-parameter defaults in language 1.16

Compiler/runtime `1.21.0` and language `1.16.0` also allow `= {}` when a constructor
or named-factory parameter names a type parameter of its owning class:

```text
serializer version 1.16.0;
class framed_value<Frame> stable_ids {
  public int32 value (1);
  public Frame placement (2);
  // Bind supplied values directly; omitted placement uses fresh Frame{}.
  public constructor(int32 input, Frame frame_input = {}) {
    value = input;
    placement = frame_input;
  }
  // Construct another owner through the same direct binding and optional default.
  public factory located(int32 input, Frame frame_input = {}) {
    value = input;
    placement = frame_input;
  }
}
```

For an open native C++ representation, the omitted value is exactly `Frame{}`.
It uses the selected type's ordinary default/value initialization, including its
host constructor behavior. It does not select `Frame::identity()` or another
factory. Each omitted call creates a fresh value. Default arguments and shortened
factory wrappers instantiate this expression only when that argument is omitted;
merely declaring the owner or calling its full signature adds no defaultability
requirement. Explicitly supplied nondefault/nonassignable values continue through
direct member binding, using their normal copy/move construction. Regular owner
default construction and decoding still have their separate representation
requirements.

Do not add a `factory(name)` or default-constructibility constraint to the class
just to express this optional argument. An actual omitted call must compile for
the chosen C++ type. Its default may throw or lack constant-evaluation support;
this feature introduces no implicit nothrow or constexpr promise. Use the existing
`default(factory_name)` contract instead when the named factory has deliberately
different semantics.

Known supported owning generated record arguments reuse language `1.15.0`'s
portable fresh-record defaults. Open native arguments and concrete primitive,
enum or container substitutions use the C++ default contract; other outputs
diagnose unsupported defaults instead of inventing their initialization rules.
Independent constructor/factory input type defaults remain unsupported: omission
does not provide a type to deduce. This adds no ordinary function defaults, direct
scalar `{}` defaults, partial/named literals or empty fixed-array defaults.
Fixed arrays retain exact cardinality; containing-record storage/mode checks stay
unchanged.

The declaring file, including an included file, needs `1.16.0`. Earlier headers
retain their rejection and default semantics. This parameter-only addition adds
no fields, revisions, managed IDs or wire bytes; codec field defaults do not change.
See [qualification](../verification/verification-generic-owner-defaults-2026-10-12.md)
for actual tests and backend limits.

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

Language `1.11.0` parameter defaults use `= literal` or `= default(factory_name)` on a concrete
owning record. All parameters after the first default must have defaults.
Scalar/string/enum defaults use the existing checked literal rules; record
defaults select an existing public zero-argument factory. Class type parameters,
independent generic input parameters and fixed-array parameters cannot have
defaults. Under `1.11.0`, a generic `Value input = {}` or
`Value input = default(factory)` rejects even when `Value` has a class template
default. Language `1.12.0` adds the explicit
generic factory contract below. Record defaults construct a fresh
value on every invocation, including Python; no
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

## Authoring construction in language 1.12

Compiler/runtime `1.17.0` supports the following opt-in C++ contracts under
`serializer version 1.12.0;`. Existing language headers retain their selected
meaning. Constructor overloads, independently generic record inputs, generic
factory parameter defaults and authoring hooks require C++ output; other outputs
diagnose these declarations rather than approximate their semantics. These
declarations add no field IDs, wire metadata, managed identities or codec hooks.

### Qualified native methods and scalar calls

```text
serializer version 1.12.0;
class sample {
  public uint32 value;
  public cpp storage(none) {
    constexpr std::uint32_t doubled() const { return value * 2; }
  }
  public function uint32 read() readonly constexpr(cpp) cpp {
    return value;
  };
  public constructor(uint32 input) {
    value = input;
  }
  public factory from_other(sample input) {
    value = input.read();
  }
}
```

`cpp storage(none) { ... }` is an author assertion that the block adds no
nonstatic data members, constructors, destructor, copy/move special members,
storage or lifetime hooks, or polymorphic behavior. Methods, static constants,
type aliases and other declarations that preserve that contract may remain
native. Serializer never infers the assertion by inspecting arbitrary C++ tokens.
Every C++ class block in the relevant owner/base/contained-value graph must be
qualified before it can pass the constant-construction proof. A native block for
another output language does not add C++ storage. The selected C++ compiler checks
the emitted declarations and each actual constant evaluation.

Qualified native storage preserves eligible `constexpr` constructors, retained
default constructors and factories. It does not prove `noexcept`: any native class
block suppresses Serializer's inferred `noexcept` annotation. The C++ compiler
still determines the implicit exception specification of a defaulted constructor.
Managed values, maps, variants,
recursive storage and dispatched behavior retain their existing restrictions.
Open generic values defer their actual constant-evaluation requirements to the
chosen specialization.

Construction expressions may call declared readonly namespace functions or public
readonly methods on input records and their accessible generated projections.
Calls must return a scalar or enum; arbitrary constructors, undeclared calls,
record-returning calls, collection indexing and reductions remain outside binding
expressions. Bindings cannot access the destination's partially initialized fields
or call its instance methods. Arguments and reached operands are evaluated once in
schema order; conditional and Boolean branches remain lazy. Callee failures
propagate, and bindings containing calls remain potentially throwing.

Typed callees use the same C++ constant-evaluation proof as ordinary readonly
methods. `constexpr(cpp)` qualifies an ordinary readonly native C++ body and emits
its C++ `constexpr` declaration; it does not interpret that body or prove that every
input is constant-evaluable. It cannot qualify edit/dispatched methods, portable
expression bodies or bodyless external declarations. Unqualified native and
external callees retain runtime construction. See [behavior](behavior.md) for the
callee contract and [the generated fixture](../../test/resources/hybrid_construction.serializer).

### Overloaded checked inputs

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
}
```

Distinct schema constructor signatures generate native C++ overloads and matching
`create` overloads. Their independent constrained input parameters may appear
inside generated owning-record applications, so `quantity<Input>` resolves its
public field projection using `Input`. Checked bindings preserve strong generated
record types and reject lossy conversions; `explicit` preserves deliberate
authoring calls. Default codec construction remains available. Duplicate signatures,
including independent type parameters differing only in their names, and ambiguous
shortened default signatures reject. Native implicit copy/move construction keeps
its ordinary C++ behavior; this does not add an arbitrary own-record-by-value
constructor declaration.

### Fresh generic factory defaults

```text
serializer version 1.12.0;
class basis_frame {
  public double diagonal {1.0};
  public factory identity() {
    diagonal = 1.0;
  }
}
class sampled_value<Frame: factory(identity) = basis_frame> {
  public double size;
  public uint32 count;
  public Frame placement;
  public constructor(double size_input, uint32 count_input = 32,
                     Frame placement_input = default(identity)) {
    size = size_input;
    count = count_input;
    placement = placement_input;
  }
}
```

The positive `factory(identity)` constraint declares the generic type's named
factory contract. `default(identity)` is allowed on that class type parameter
only when its constraint supplies the same factory. The parser checks each known
generated concrete argument for an owning representation and a public zero-argument
factory returning that exact owner. Open application C++ types satisfy the emitted
`requires` check for an exact `Frame` result. This is independent of native host
initialization or execution ownership.

Each omitted argument invokes `Frame::identity()` afresh, so a nonzero identity
basis can be supplied independently of default field initialization. Standard
trailing-default rules still apply: `count` and `placement` are both optional in
the example. A custom C++ frame must also satisfy the generated owner's ordinary
default-construction, assignment and codec requirements. A factory constraint
does not promise constant evaluation or `noexcept`; qualify the selected factory
implementation when those guarantees matter. Literal defaults on open type
parameters, independent input defaults and fixed-array defaults retain their
language-selected restrictions. Language `1.16.0` separately permits `{}` on owner
type parameters without the factory constraint; it chooses ordinary default
initialization rather than the factory's result. See [generic constraints](generics.md).

Other ordinary backends retain fresh factory defaults after an owning generic
model is concretely specialized in the schema. They do not expose the open C++
template API. Native authoring hooks require C++ output separately;
a concrete model using only portable bindings/defaults keeps its portable API.

### Validation and host initialization

```text
serializer version 1.12.0;
class authoring_value {
  public double radius;
  public uint32 count;
  public constructor(double radius_input, uint32 count_input = 32) {
    radius = radius_input;
    count = count_input;
    validate expression(math.is_finite(radius) && radius > 0.0 && count >= 3);
    validate cpp {
      validate_placement(*this);
    };
    initialize cpp {
      register_execution_reference(*this);
    };
  }
}
```

`validate expression(...)`, `validate cpp { ... };` and `initialize cpp { ... };`
belong only to a declared authoring constructor. Bindings precede validations,
which precede host initialization. Typed validation reads the bound candidate's
fields and must produce Boolean state; a false result throws `std::domain_error`.
Reached expression/callee failures also propagate. The optional native validation
block follows typed validations, and the optional initialization block runs last.
Repeated or misordered native hooks reject. Keep the total typed validation count
within the compiler's 64-check bound.

Declare and implement the shown host functions in application C++, using a native
preamble or the selected application's include configuration. Serializer does not
define registration, session ownership, placement rules or callback implementations.
Native bodies keep authored field names, so use `cpp.naming = preserve`. Generic
host validation can require additional C++ representation operations; the compiler
checks them independently of the generic factory contract.

The generated authoring `create` factory invokes the native constructor once.
Default construction, ordinary copies/moves and codec decoding do not run these
validation or initialization hooks. A native hook suppresses constant construction
for that authoring overload; it does not change the retained schema-default
constructor. Host side effects are not rolled back automatically if a hook throws;
the application must use RAII or its own transaction/session boundary. A successful
hook does not turn a transient registration into a durable managed identity.

See [authoring verification](../verification/verification-authoring-2026-10-11.md) for executed
checks and platform limits; the examples above specify behavior, not a claim that
every consuming application or SDK has been qualified.

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
[usage](../guides/usage.md), and [verification](../verification/verification-integration-enhancements-2026-10-11.md).
