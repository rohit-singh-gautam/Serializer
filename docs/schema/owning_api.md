# Owning record APIs in language 1.14.0

Language `1.17.0` adds [array and callable APIs](array_and_callable_api.md).
Use explicit storage/axes, owner field factories and callable-local restrictions
to preserve supported host contracts. Target limits and completed qualification
are recorded with the feature; older contracts below retain their semantics.

Compiler/runtime **1.19.0** supports these API additions without changing field
IDs, names, payload revisions, or the managed journal/collaboration protocols.
Each declaring file, including included files, must select language **1.14.0**.
Earlier schema contracts keep their previous generation behavior.

Language `1.16.0` separately permits `Frame placement = {}` on an owner's type
parameter, with omitted-only C++ default initialization and no factory or
defaultability constraint. Explicit nondefault/nonassignable inputs retain direct
binding. See [generic owner defaults](construction.md#generic-owner-parameter-defaults-in-language-116)
for backend limits and the independent codec/default requirements.

Language `1.15.0` separately adds `area input = {}` for class-valued constructor
and named-factory parameters, including nested empty record literals. An omitted
argument receives a fresh owning value with its schema field defaults. Full
positional defaults remain a `1.14.0` feature; earlier actually empty-record
defaults keep their meaning. See [empty parameter defaults](construction.md#empty-class-valued-parameter-defaults-in-language-115).

## Readonly C++ getters

```text
serializer version 1.14.0;
class definition stable_ids {
  private int32 stored_value ("value", 1) expose readonly(public) as value;
}
```

An explicit `expose readonly(public) as value` adds a `constexpr` const-reference
getter over the original stored field. `protected` and `private` getter access
are also supported. It introduces no setter, copy, allocation, or extra storage.
The getter accepts only lvalue owners; its rvalue overload is deleted, preventing
a temporary owner from producing a dangling reference. Its return cannot mutate
the field. Existing C++ friends may still write the private backing field.

The owner must outlive every borrowed reference. Destroying or relocating the
owner invalidates references to its fields. Moving from or assigning a surviving
owner can change the observed value and invalidate references into nested
container storage; a direct field reference remains bound while that field lives.
Getter access does not synchronize with concurrent writes. Ordinary transient
fields keep their existing memory-only semantics, and codecs still access the
original fields with their original wire identities.

This contract initially supports ordinary unpacked owning C++ records, including
open generic types and records stored inside managed roots. Views, packed records,
managed editor getters, metadata without storage, raw unions and other output
backends diagnose unsupported exposure. Getter names must avoid source and
generated API collisions. For an authored backing name such as `intent_` with a
getter named `intent`, use `[cpp] naming = preserve`; profiles that fold those
names together report the collision. No new friend declaration syntax is needed.

Run the [readonly getter example](../../example/cpp/readonly_getters/README.md).

## Direct construction and portable defaults

New C++ constructor bindings initialize members directly in declaration order.
A bound member does not need default construction or assignment: copyable
nondefault/nonassignable representations can be passed into open generic records.
Named factories use the same construction path. Unbound members retain schema
defaults; ordinary codec/default construction and authoring hooks keep their
documented separation. Default decoding requires the selected representation's
normal codec/default operations, independently of authoring a bound value.

Typed defaults can recursively initialize fixed arrays and owning generated
records. For example:

```text
serializer version 1.14.0;
class vector3<T> stable_ids {
  public T x (1) {0};
  public T y (2) {0};
  public T z (3) {1};
  public constructor(T x_value = 0, T y_value = 0, T z_value = 1) {
    x = x_value;
    y = y_value;
    z = z_value;
  }
}
class frame<T> stable_ids {
  public vector3<T> normal (1) {{0, 0, 1}};
  public array[2] vector3<T> axes (2) {{1, 0, 0}, {0, 1, 0}};
  public constructor(vector3<T> input = {0, 0, 1}) { normal = input; }
}
```

The outer field initializer braces retain the existing field syntax; nested
braces describe the record or array's typed values in declaration order. Defaults
are fresh owning values, not shared mutable instances. Concrete literals are
checked against their target widths and aggregate shape; generic scalar literals
retain typed initialization rather than converting through `double`. Existing
custom coordinate arguments and decoded values override these defaults. Missing
keyed fields in a fresh decode retain the defaults. No new factory constraint is
required on an open coordinate representation.
Headerless fragments passed to the public parser retain legacy C++ member
initializer spelling, including opaque expressions. Include an explicit language
`1.14.0` header when requesting portable typed member defaults through that API.

Compiler 2.15 diagnoses out-of-range unsuffixed canonical decimal/hex integer
defaults at the declaring member's original path, line and column, including
included declarations and generic specializations. It preserves native scalar
expressions, numeric Boolean defaults, suffixed/octal literals and legacy/headerless
spelling. Existing portable aggregate/generic initializer failures also identify
their original member declaration. Bit-field defaults and broader expression
validation remain separate [pending work](../pending.md).

See [construction](construction.md) for authoring validation and initialization,
and run [construction defaults](../../example/cpp/construction_defaults/README.md).

## Reciprocal signatures and C++ additions

Constructor parameter types resolve after finite storage layouts are checked.
`radius(diameter<T>)` and `diameter(radius<T>)` therefore work in either declaration
order. This permits callable dependencies; it does not permit recursive by-value
storage, incomplete field types, or unbounded generic specialization.

```text
public cpp { constexpr } constructor(diameter<T> extent);
public cpp { constexpr inline } function T as_scalar() readonly;
```

The declaration-site C++ addition supports `constexpr` and `inline`, each once.
It does not inject arbitrary C++ declaration tokens or storage. A constructor
ending in `;` is a C++ external declaration; the host supplies its definition.
Match the generated signature and keep a `constexpr` definition visible wherever
constant evaluation uses it. Other backends reject external C++ constructors.
Readonly function effects and existing postfix `noexcept(cpp)` remain separate.

Ordinary schema-defined C++ constructors and eligible function bodies
generate `constexpr` automatically. Definitions inside a class are already inline;
eligible namespace definitions use `constexpr` or `inline`. External declarations
need the explicit prefix when the host definition promises constant evaluation.
Managed editor guards, dispatch contracts, nonconstant storage lifetimes and C++20 math or
other runtime-only callees can prevent automatic constant evaluation. Generated
comments and the qualification record state these boundaries; an explicit native
promise still needs to compile and hold for the selected specialization.
`constexpr` does not imply `noexcept`. Ordinary native edit methods can mutate a
local owning value during constant evaluation when their selected storage and
signature permit it, including methods returning `void`.
Native C++ token bodies remain opaque to the schema compiler. Their automatic
qualification checks the signature and storage; the selected C++ compiler checks
the body's legality, and constant evaluation still requires an eligible visible
implementation for the selected specialization.

Public codec default construction remains available automatically. The separate
syntax `public constructor() = default;` explicitly declares a default constructor;
`public constructor();` instead requires a matching external default definition.
Neither means that a parameterized authoring constructor should run during decode.

Run [reciprocal quantities](../../example/cpp/reciprocal_quantities/README.md).

## Explicit arithmetic mappings and owning calls

Map only the operations the schema declares:

```text
class distance<T: (integral || floating_point) && !bool>
    arithmetic(unary(-) -> negated) {
  public T value (1);
  public constructor(T amount) { value = amount; }
  public function T negated_value() readonly expression(-value);
  public function distance<T> negated() readonly
    expression(distance<T>(negated_value()));
  arithmetic(binary(+, distance<T> rhs) -> expression(distance<T>(value + rhs.value)));
}
```

A named unary mapping selects one ordinary readonly method with zero arguments;
a named binary mapping selects one with a typed right argument. Its complete
signature determines the result. Inline forms require an explicit owning result
constructor; scalar-return helpers remain scalar-return operators unless the
schema wraps their result deliberately. Supported mappings are unary `+`/`-`
and binary `+`/`-`/`*`/`/`/`%`. Mappings can be class annotations or semicolon-ended
declarations in the class body. No automatic operator set, field selector, guessed
unit conversion, or implicit conversion between distinct owning records is added.

Owning constructor calls in portable expressions select public constructors with
exact argument types and available trailing defaults. Zero-argument calls preserve
fresh codec/default construction. Unsupported deduction or ownership combinations
diagnose instead of approximating a conversion. Portable helper methods use the
same checked numeric operations as ordinary schema expressions; C++ operators
forward to those helpers without type erasure or runtime dispatch. Arity,
readonly effect, duplicate/ambiguous overloads and result mismatches are checked.
These calls accept ordinary owning parameter types, including built-in
`array<T>` types. Legacy `array[N] T` parameter modifiers and independently
generic constructor-input deduction currently diagnose; direct authoring
constructors and factories retain their separately documented support.

Eligible operators and visible helpers are constant-capable; their exception
specifications follow the full forwarding call. Overflow, unsigned negation,
zero division, fractional integer division, and signed-minimum/-1 preserve the
existing checked rejection categories. The fields and wire layout are unchanged.
No benchmark speedup is claimed from source-level forwarding alone.

Run [arithmetic operators](../../example/cpp/arithmetic_operators/README.md).

## Capability selection and qualification

| Capability | Selection |
| --- | --- |
| Schema-owned behavior, construction and generic constraints | Used for typed calls, defaults and operator contracts. |
| Checked numeric bindings/arithmetic | Used without lossy intermediate representations or changed rejection categories. |
| Owning versus borrowed lifetime | Used: records own storage; readonly getters borrow only from lvalues. |
| Stable fields, native codecs, transient fields and managed history | Retained; API additions introduce no wire identities or persistent storage. |
| Native C++ declarations and backend diagnostics | Used only for C++ qualifiers/external definitions and unsupported output contracts. |
| Views, packing, Protobuf, compression, paging and persistence changes | Not applicable to these owning API additions; existing supported contracts remain independent. |
| Editor grammar, navigation and package versioning | Updated together for both supported editor packages. |

See [verification](../verification/verification-owning-api-2026-10-12.md) for actual
builds, tests, SDK checks, and remaining qualification limits.
