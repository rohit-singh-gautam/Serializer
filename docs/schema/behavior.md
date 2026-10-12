# Schema behavior and native source

Language `1.17.0` adds [array and callable APIs](array_and_callable_api.md).
Use explicit storage/axes, owner field factories and callable-local restrictions
to preserve supported host contracts. Target limits and completed qualification
are recorded with the feature; older contracts below retain their semantics.

Language `1.14.0` adds [explicit arithmetic mappings and owning constructor calls](owning_api.md#explicit-arithmetic-mappings-and-owning-calls),
[readonly field getters](owning_api.md#readonly-c-getters), and declaration-site
C++ `constexpr`/`inline` additions. Eligible ordinary C++ native readonly/edit
bodies become constant-capable automatically; external definitions still need a
matching explicit declaration and visible implementation. Fields and wire layout
remain unchanged. See [examples](../../example/README.md#owning-record-apis).

Schema 1.6 declares behavior alongside durable fields. Methods and target source
have no wire IDs and do not enter codecs, managed fingerprints, history comparison
or collaboration patches. [Runnable native schema](../../test/resources/document_native_behavior.serializer)
and [SDK consumers](../../test/native_behavior_languages.py) cover every target.

For a focused C++ walkthrough, build the [custom code example](../../example/cpp/custom_code/README.md).
Its preamble, native class declarations, inline methods, and external `.cpp`
definition operate on ordinary schema-owned fields and use the generated codecs.
The editor extensions reuse available native language grammars for
[embedded syntax highlighting](../editors/editor_extension.md#embedded-source-highlighting).

## Three implementation forms

```text
serializer version 1.6.0;
class document_metrics stable_ids {
  public double words_per_page (1);
  public uint32 page_count (2);

  // Application supplies the definition outside generated files.
  public function double estimated_words(double scale) readonly;

  // Serializer checks and translates this bounded numeric expression.
  public function double total_words() readonly
    expression(words_per_page * to_double(page_count));

  // Each selected target receives exactly one authored body.
  public function void reset(double next) edit
    cpp { words_per_page = next; }
    csharp { words_per_page = next; }
    python { self.words_per_page = next };
}
```

`readonly` returns through a read receiver; `edit` uses a mutable receiver. C++
managed `edit` methods are generated on transaction editors and call their guarded
access path. Native code uses generated getters/setters explicitly: Serializer
neither rewrites native identifiers nor proves native side effects. Declare every
durable and runtime field in the schema (`transient` for runtime-only fields);
storage injected through an opaque class hook has no generated history, identity
or runtime-reset contract. A selected
target without a body uses its external-definition contract.

Native and external method signatures also accept `digest`, `digest[N]`, and
`digest(algorithm)` as return and parameter types with language `1.6.0` or newer.
For example, `public function digest(sha256) copy_hash(digest(sha256) payload)
readonly cpp { return payload; };` returns the supplied bytes. These signatures
use each target's existing digest storage type; they add no wire fields and do
not compute a hash. Native code remains responsible for its byte ownership and
length contract, including the lifetime of a returned C `srl_buffer`. Digest
arithmetic and comparisons are outside the portable expression profile. Language
`1.10.0` can forward exact digest types through declared readonly calls. See
[digest storage](digest.md).

| Target | External definition or attachment |
|---|---|
| C++ | Define the generated member in application `.cpp`; call `model::serializer_require_behavior_definitions()` during qualification to retain references to unused required definitions |
| C | Define the generated receiver function; call `model_require_behavior_definitions()` during qualification |
| C# | Implement required partial methods in the generated nested class under the partial outer unit |
| Go | Define methods in the generated package; generated method-expression checks validate missing methods |
| Rust | Define inherent methods in the same crate; generated associated function references validate missing methods |
| Swift | Supply an extension implementing the generated external behavior protocol |
| Java, Kotlin | Attach an implementation of the generated `Behavior` interface before calling required external methods |
| JavaScript, Python | Attach a checked callable object/dictionary; missing required attachment fails before invocation |
| TypeScript | Typed declarations describe the generated JavaScript methods and attachments |

TypeScript emits `.d.ts` declarations. Use `javascript { ... }` for executable
method bodies; a `typescript` class/file hook must contain valid ambient
declarations. A `typescript` method body is not emitted as executable code.

C++ external definitions for editor templates must remain visible where those
specializations are instantiated. The editor qualification method is called on a
live transaction editor. C++ examples use exceptions and RTTI normally.

## Explicit C++ noexcept contracts

Prefer a portable schema body when it can express the method's complete logic.
For a completion check based on two counters, declare
`public function bool complete() readonly predicate(completed == expected);`.
Serializer generates that check in C++, Java and the other selected targets,
retaining the documented predicate failure behavior and inferred C++ guarantees.
The explicit qualifier below serves native/external C++ signature requirements.

Language `1.13.0` supports `noexcept(cpp)` after a function's `readonly` effect.
It is a typed C++ exception qualifier for native and external C++ signatures,
including namespace functions and member dispatch contracts:

```text
serializer version 1.13.0;
class progress {
  public uint32 completed;
  public uint32 expected;
  public function bool complete() readonly noexcept(cpp);
  public function bool empty() readonly noexcept(cpp) cpp {
    return completed == 0;
  };
}
```

The external member is declared as `bool complete() const noexcept;` in C++.
Define it in application code using the matching exception specification:

```cpp
// Report whether all expected work is complete.
bool progress::complete() const noexcept {
  return completed == expected;
}
```

This migrates a native class hook such as
`public cpp storage(none) { bool complete() const noexcept; }` into a typed schema
function. Serializer generates the signature and the external-definition check;
the application still supplies its body. A function with bodies only for other
targets also uses this external C++ contract. If additional outputs are selected,
supply their normal native bodies or external attachments as well.

Native and external function signatures emit unconditional C++ `noexcept`.
It is an author promise, not a
proof that native code is pure or contains no throwing operations. An exception
escaping the C++ function invokes `std::terminate`; Serializer adds no catch or
fallback. Other targets ignore this qualifier and retain their ordinary failure
contracts, including Swift `throws` where otherwise generated. It does not mean
that Rust or Go cannot panic, or that other languages cannot raise exceptions.
The qualifier changes no wire values, model storage or codec behavior.

`virtual`, `abstract` and `override` readonly member functions may declare it.
An inherited explicitly qualified virtual/abstract signature requires the same
`noexcept(cpp)` on the derived matching declaration, even when the `override`
keyword is omitted.
C++ behavior-interface implementations must also match the exception contract.
The generated behavior adapter uses conditional `noexcept` for the complete
forwarding call. Passing a by-value argument to the interface may require a
throwing copy before entering its nonthrowing method, so an adapter accepting
such arguments can remain potentially throwing even though the interface method
itself is unconditionally `noexcept`.
Portable `expression(...)` or `predicate(...)` overrides of such signatures are
currently unavailable because those bodies reject an explicit qualifier.

Open-generic dispatched C++ qualification covers parameterless signatures.
Renamed or dependent parameter signatures inherited across open generic bases
retain existing backend limitations in resolving inherited behavior contracts.
The parser checks the explicit exception qualifier after generic argument
substitution; that check does not establish full generated dispatch support for
those signatures. Keep native adapters for affected models and compile the
selected generated output before migrating them.

Ordinary readonly native C++ functions may combine `constexpr(cpp)` and
`noexcept(cpp)` in either order. The existing `constexpr(cpp)` restrictions still
apply: dispatched and external declarations cannot use it. `noexcept(cpp)`
rejects `edit` functions and portable expression/predicate bodies; their existing
inferred guarantees remain unchanged. Native/external callees remain conservative
inputs to caller exception inference even when explicitly qualified.
Independently generic functions with their own input list, such as
`complete<Input: integral>(Input value)`, require a native `cpp` body when declaring
`noexcept(cpp)`: automatic external-definition checks cannot qualify every
specialization. Bodyless external methods using only their class's generic
parameters remain supported.
Conditional specifications such as `noexcept(cpp, condition)`, bare `noexcept`,
other target names and arbitrary C++ postfix blocks are unsupported. Each file
declaring the qualifier, including an included file, requires language `1.13.0`
or newer. See [verification status](../verification/verification-noexcept-2026-10-12.md).

## Virtual, abstract and override contracts

```text
class document_base stable_ids {
  public function abstract double score(double extra) readonly;
}
class document_data stable_ids : public document_base ("base", 1) {
  public double measure (2);
  public function override double score(double extra) readonly
    expression(measure + extra);
  public function virtual double weight() readonly expression(measure * 2.0);
}
```

Generated codec data remains constructible. An abstract declaration contributes
a separate checked behavior interface rather than an unconstructible persisted
object. C++ concrete virtual methods also support native member dispatch. Other
targets use interfaces, protocols, traits, function tables or checked callable
attachments with explicit data receivers. Adapters never serialize implementations.

An override must match an inherited return type, parameters and effect. Two bases
contributing one signature require an explicit override. Languages without method
overloading reject colliding names; erased target signatures are checked before
output is published. Managed `virtual`, `abstract` or `override` **edit** methods
are rejected: transaction editor dispatch currently supports ordinary edit methods.

## Target-specific source and ordering

```text
cpp preamble { #include <string_view> }
class document_data stable_ids {
  public double measure (1);
  private cpp { friend class document_reader; }
  public cpp { using label_type = std::string_view; }
  public function double score() readonly cpp {
    auto label = R"tag({ literal spacing })tag";
    (void)label;
    return measure;
  };
}
```

Target tags are `cpp`, `c`, `csharp`, `java`, `javascript`, `typescript`, `go`,
`rust`, `python`, `swift` and `kotlin`. File hooks use `target preamble { ... }`.
Class hooks use explicit `public`, `protected` or `private` access and have no
outer semicolon. Method declarations end in one semicolon after all bodies.

C++ retains field/method/native declaration order and restores generated access
sections after opaque hooks. C++ `friend` declarations grant friendship regardless
of their access section; the explicit section still matters for surrounding
members. Every backend retains relative native/method order. Go and C reject class
hooks because their methods/free functions live outside struct declarations.

Opaque source is preserved rather than renamed. Choose `naming = preserve` for
C++, Java, JavaScript, Go and C# when their body refers to generated identifiers.
Go public schema identifiers must already be exported when naming is preserved;
[its native fixture](../../test/resources/document_native_behavior_go.serializer) uses
`Measure`, `Update` and `Score` explicitly. Rust/Python use the existing fixed snake_case member spelling; Swift/Kotlin use
fixed camelCase. Preambles must be valid at the documented file location, such as
Kotlin imports before generated declarations. Native payloads are bounded to
1 MiB and nesting to 128; comments and quoted/raw literal delimiters are scanned.

Language `1.12.0` adds `cpp storage(none) { ... }` on class hooks. The author
asserts that the block adds no instance storage, lifetime/storage hooks, special
member implementations or polymorphic behavior. Methods, aliases and static
constants may preserve this contract. Serializer keeps the payload opaque and
does not infer that promise from arbitrary C++ tokens. It permits otherwise
eligible C++ construction to remain `constexpr`; native blocks suppress
Serializer's inferred `noexcept` annotation. See [construction qualification](construction.md#qualified-native-methods-and-scalar-calls).

## Portable arithmetic

```text
public function double average(double copies) readonly
  expression((words_per_page + 2.0) * to_double(page_count) * copies);
```

The portable form supports finite binary64 literals, declared scalar symbols,
`math.pi`, unary `+`/`-`, binary `+`/`-`/`*`, grouping and explicit
`to_double(int32_or_uint32)`. Through language `1.10.0`, arithmetic operands and the
result are `double`. Language `1.11.0` adds the exact numeric profile below.
Arbitrary loops, assignment and undeclared calls remain outside the bounded profile.
Bounds are 1024 expression nodes and depth 64. This bounded language leaves
application-specific logic in native code.

### Exact numeric representations

Language `1.11.0` preserves the representation of matching integer, `float`, and
`double` operands for unary `+`/`-` and binary `+`/`-`/`*`/`/`. Integer intermediates
remain integers across their full declared width; they never pass through binary64.
Overflow, division by zero, signed minimum divided by minus one, and fractional
integer quotients fail before evaluation can publish a result. Thus `3 / 2` is
invalid when bound to an integer representation. Nonzero unsigned negation also
fails. Floating arithmetic rounds in its declared representation and requires
finite inputs and intermediates. Python and JavaScript restore `float` operands
to binary32 before arithmetic, comparisons, guards, or checked value binding, since
their native numeric storage is binary64. Explicit `math.is_finite` inspection
continues to observe nonfinite values without rejecting the observation itself.

```text
serializer version 1.11.0;
class amount<Rep: (integral || floating_point) && !bool> {
  public Rep value;
  public function Rep added(Rep other) readonly expression(value + other);
  public function Rep half() readonly expression(value / 2);
  public function Rep scaled(Rep factor) readonly expression(value * factor);
  public function Rep bound(uint64 other) readonly expression(checked<Rep>(other));
}
```

`checked<Result>(value)` explicitly selects a numeric result representation and
requires lossless value binding. Signed/unsigned conversions check both endpoints;
integer-to-floating conversions require exact representability; floating-to-integer
conversions reject fractions and out-of-range endpoints; floating conversions
reject rounding, overflow, NaN, and infinity. This includes the entire `int64` and
`uint64` ranges: the integer `uint64` maximum cannot bind to `double`, while an
exact power of two can. Floating signed zero is retained by floating conversions;
binding either zero to an integer produces integer zero.

Numeric operands of different representations require an explicit selection,
for example `left + checked<int64>(right)`. There is no implicit signed/unsigned
promotion, unrestricted native promotion, or conversion between distinct generated
record types. Integral literals bind to an adjoining exact representation and retain
their authored bits; floating and dependent representations receive a checked binding.
Thus `float_value + 16777217` fails if reached instead of rounding that integral
literal to a different value. Arithmetic type parameters must have numeric category
constraints. Include `!bool` when the constraint otherwise includes Boolean values.

All eleven ordinary outputs implement concrete numeric expressions and checked
numeric bindings. C++ also retains open constrained numeric templates. Other outputs
use their existing concrete generic specializations. C++, C, Go, Rust, and Swift
check native operations before overflow. Java, C#, Kotlin, Python, and JavaScript
use exact integer intermediates where necessary; this bounded expression machinery
does not alter stored fields or their codec bytes. Reached invalid operations follow
the existing `expression` failure contract and return false in `predicate` bodies.
Existing `to_double(int32_or_uint32)` and older declared schema contracts retain
their prior meanings.

Application C++ code can use `<rohit/numeric.hpp>` directly. The `constexpr`
`checked_numeric_cast<Result>(value)` and matching-representation
`checked_add`, `checked_subtract`, `checked_multiply`, `checked_divide`, and
`checked_negate` throw `std::domain_error` for invalid values. Their
`try_numeric_cast<Result>` and `try_checked_*` counterparts are `noexcept` and
return `std::optional` without allocation, including during constant evaluation.

### Constrained readonly inputs and result representation

Compiler/runtime `1.17.0` and language `1.12.0` add independent constrained input
types on ordinary readonly C++ callables and the explicit numeric result policy
`common_type<T, Input>`:

```text
serializer version 1.12.0;
class amount<T: (integral || floating_point) && !bool = double> {
  public T value;
  public function common_type<T, Input> scaled<Input: (integral || floating_point) && !bool>(Input factor) readonly
    expression(checked<common_type<T, Input>>(value) * checked<common_type<T, Input>>(factor));
  public function common_type<T, Input> divided<Input: (integral || floating_point) && !bool>(Input divisor) readonly
    expression(checked<common_type<T, Input>>(value) / checked<common_type<T, Input>>(divisor));
}
```

C++ emits constrained function templates and lowers the result policy to
`std::common_type_t<T, Input>`. Both representations must be checked numeric types
excluding Boolean state. The two explicit `checked` bindings establish exact
values in that selected representation before arithmetic. For example, mixing
signed and unsigned input can select an unsigned result; a negative signed value
then rejects instead of wrapping. A `uint64` value not exactly representable in
the selected floating result rejects instead of rounding. Division retains exact
integer quotients when the selected representation is integral.

The result is a scalar representation. This policy does not implicitly convert
strong generated record types or synthesize an owning record wrapper, and it
cannot be used to change field/wire types. Preserve an application adapter when
its result must wrap the scalar in a distinct quantity type. These independently
generic callable APIs require C++ output; other backends diagnose them. Existing
class generics still use their portable concrete-specialization workflow. Generic
input constraints use the same [Boolean contract](generics.md#boolean-type-constraints)
as class parameters, while actual constant evaluation defers to the chosen
specialization. Eligible checked methods remain `constexpr` and potentially
throwing.

Independent generic methods and namespace helpers are callable through the
generated C++ API from host code. Schema expressions cannot call those generic
APIs; the compiler diagnoses such calls explicitly. Use a helper with a concrete
parameter signature when another schema expression must call it.

## Declared function calls

Language `1.10.0` lets expressions call schema-declared `readonly` functions and
adds namespace-level function declarations. A free function uses `function`
without an access prefix; class methods retain their existing access prefixes.

```text
serializer version 1.10.0;
namespace geometry {
  function double coordinate_add(double left, double right) readonly
    expression(left + right);
  class coordinate {
    public double value;
    public function double added(double other) readonly
      expression(geometry::coordinate_add(value, other));
    public function double doubled() readonly expression(added(value));
  }
}
```

Calls bind to declared signatures, including declarations appearing later in the
same schema. Arguments and return values keep their exact declared types; the
parser requires a matching overload and a non-void `readonly` result. It rejects
unknown functions, mutating callees, mismatched arguments and ambiguous overloads.
Unqualified member calls use the current receiver; free functions use lexical
namespace lookup or an explicitly qualified name. Existing builtins retain their
meaning. Typed call cycles are rejected. Conditional and boolean branches remain
lazy, and call arguments evaluate once, from left to right.

Language `1.11.0` also permits calls on generated owning record projections,
including nested fields, checked collection elements, reduction items, and
record-valued call results. Only declared public `readonly` methods and public
inherited methods are accessible through a projected receiver; arbitrary external
object signatures are not inferred. Each file declaring the expression needs its
own `1.11.0` header, including included files.

```text
serializer version 1.11.0;
class item_key {
  public uint64 value;
  public function bool valid() readonly expression(value != 0);
}
class selection {
  public item_key preferred;
  public item_key fallback;
  public function item_key selected() readonly
    expression(preferred.valid() ? preferred : fallback);
}
```

Conditional selection accepts exactly matching ordinary owning record types.
Different records, generic specializations, managed occurrences, or view values
are not implicitly converted. The condition and selected branch are evaluated
once; the other branch is not evaluated. The result follows the target's ordinary
record value/reference rules. C++ and Rust return owned values; Rust clones only
the selected branch. C supports returned record selections whose entire storage
graph contains no dynamically owned strings, digests, or collections. A selection
returning a dynamically owning C record reports a generation error because a
shallow copy would not establish a fresh ownership contract. Existing borrowed C
record projections remain unchanged.

Standalone overloads are supported in C++, Java, C#, Swift and Kotlin when their
generated signatures remain distinct. Other outputs require unique standalone
function names. Signatures that collide after target type erasure are rejected.

A readonly callee can use a portable expression, a target-specific native body,
or a supported external implementation contract. Serializer checks the declared
effect and signature; the selected language compiler checks native bodies.
`readonly` does not prove that opaque native code has no side effects.

Compound arguments/results follow the target's normal value/reference rules;
readonly does not imply a deep clone. C record, sequence and digest projections
borrow their existing storage, so the receiver/argument owner must outlive the
borrowed result and that copy must not be released as a new owner. Use an explicit
[record conversion](conversions.md) when a fresh owning copy is required.

Standalone external functions are supported by C++ definitions, C definitions,
Go package functions and C# static partial methods. Other executable outputs
currently require a portable expression or a native body for a standalone
function and report a generation error otherwise. TypeScript can declare the
signature, but executable JavaScript still needs an implementation. Existing
external class-method attachments remain available. C cannot call an abstract
method from a portable data-model expression because that receiver has no behavior
adapter; concrete readonly implementations are callable.

Java emits namespace functions as static methods on the generated namespace
class and class functions as instance methods. With Java naming enabled, the
example calls `Geometry.coordinateAdd(value, other)` and `added(value)`.
Portable expressions work in both C++ and Java; a `cpp { ... }` body requires a
separate `java { ... }` implementation when Java output is selected. Java executes
these functions at runtime and has no `constexpr` qualifier.

C++ uses `constexpr` for eligible ordinary typed functions, including calls to
other eligible typed functions. Language `1.12.0` can qualify an ordinary readonly
native C++ body explicitly:

```text
serializer version 1.12.0;
namespace native_values {
  function uint32 identity(uint32 value) readonly constexpr(cpp) cpp {
    return value;
  };
}
class forwarded_value {
  public uint32 value;
  public function uint32 read() readonly expression(native_values::identity(value));
}
```

`constexpr(cpp)` emits a C++ `constexpr` declaration and allows the callee to
participate in the caller's constant-evaluation proof, including scalar
[construction bindings](construction.md#qualified-native-methods-and-scalar-calls).
The selected C++ compiler checks its native implementation and each actual
constant evaluation. The qualifier requires an ordinary readonly function with a
native `cpp` body; dispatched/edit methods, bodyless external declarations and
portable expression bodies reject it. Portable expressions already use their
inferred proof. The qualifier does not prove purity, make every input
constant-evaluable, or infer `noexcept` for native callees/callers. Language
`1.13.0` permits an independent [explicit `noexcept(cpp)` promise](#explicit-c-noexcept-contracts)
on an eligible native body.

Unqualified native, external and dispatched callees retain their own runtime and
exception contracts and suppress inferred `constexpr` for their caller. Calls do not
change wire layouts, field IDs or persisted state. See the
[expression example](../../example/cpp/expressions/README.md) and
[constant evaluation guide](../serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers).
The borrowed `cpp.emission_only` profile currently rejects namespace functions;
ordinary owning C++ output supports them.

Language `1.8.0` adds binary64 division, `math.sqrt(value)`, `math.sin(value)` and
`math.cos(value)`, numeric comparisons `<`, `<=`, `>`, `>=`, matching scalar/enum
equality `==`/`!=`, boolean literals/parameters/fields, `!`, `&&`, `||`, and
`condition ? when_true : when_false`. Conditions must be boolean; both branches
must have the same scalar type, and the declared return type must match.
The language `1.8.0` arithmetic profile requires explicit double operands.
Language `1.11.0` adds representation-preserving operations above. Comparisons retain matching signed
or unsigned integer widths, float/double, bool equality, or declared enum identity;
an adjacent integer literal binds to that exact width and is checked for range.
Mixed signed/unsigned or integer/floating comparisons require an explicit supported
conversion. Qualified enum constants use `mode::active`.
The operators use conventional precedence, left associative arithmetic, and
right associative conditional branches. `&&`, `||` and `?:` evaluate only the
needed operands/branch, so a guard can exclude invalid arithmetic:

```text
serializer version 1.8.0;
class measurements {
  public function double root(double value) readonly
    expression(value >= 0.0 ? math.sqrt(value) : 0.0);
  public function bool ratio_exceeds(double value, double divisor) readonly
    expression(divisor != 0.0 && value / divisor > 1.0);
}
```

Both positive and negative zero divisors reject. Square root accepts finite
nonnegative inputs, including negative zero. Sine/cosine accept any finite
binary64 input and use radians; results follow the target's standard math library
without a guarantee of identical final bits across SDKs. Every evaluated numeric
intermediate must be finite, including arithmetic feeding a comparison. Skipped
branches perform no arithmetic/domain checks. Direct scalar floating input
symbols are validated for finiteness before `expression(...)`
evaluation, including symbols in skipped branches. Floating member/index/item
loads are checked when reached. Dynamic targets require the declared boolean,
integer range and enum identity without coercion. Older declared contracts reject
the new operators and boolean results.

Readonly boolean methods can instead declare `predicate(...)`. Reached invalid
arithmetic, projections or bounded traversal return false; skipped branches perform
no validation. These built-in predicate operations do not throw or panic for semantic failure. C checked
predicate calls publish false with `srl_ok`; null receiver/result arguments still
return `srl_invalid`. `math.is_finite(float_or_double)` observes NaN/infinity and
returns false in either form, allowing explicit finite guards. Bounds and arithmetic
inside that observation remain checked.

Public generated record fields can be projected with `point.coordinate`, including
public inherited fields. Sequences and fixed arrays support `items[index]` and
`size(items)`, which returns exact uint64. Indices must be exact integral values;
negative or out-of-range reached indices fail before reading storage.

```text
public function bool usable(uint64 index) readonly
  predicate(index < size(samples) && math.is_finite(samples[index].coordinate));
public function bool all_usable() readonly
  predicate(all(samples, 16, item => math.is_finite(item.coordinate)));
```

`all(collection, bound, item => boolean)` and `any(...)` evaluate readonly typed
items in order with short circuiting. The literal bound is 1 through 1,000,000;
the collection's length must satisfy it before any item is evaluated. Empty `all`
is true and empty `any` is false. Every invocation shares a 1,000,000 visited-item
budget across nested reductions, charged once per visited item. Exhaustion fails
the expression or returns false for a predicate. Binder names cannot shadow fields,
parameters or outer binders. Maps, optional/variant projection, mutation and
undeclared calls are outside this profile. Exact integer arithmetic requires
language `1.11.0`.

C++ emits `constexpr`/`noexcept` for eligible readonly typed methods, including
exact scalar predicates and bounded projections/reductions whose operations permit
constant evaluation. Compiler/runtime 1.15.0 also marks eligible readonly results
`[[nodiscard]]` and allows checked addition, subtraction, multiplication, division,
sign operations and integer-to-double conversion in constant evaluation.
Reached invalid input, arithmetic or indexing still throws for `expression` and
returns false for `predicate`; checked expressions remain potentially throwing.
Square root and trigonometry remain runtime under C++20; native/opaque bodies stay
application-owned, with the explicit language `1.12.0` qualification above when
appropriate. `noexcept` is inferred independently of `constexpr`.

Declared calls retain the callee's failure contract. In C++, an exception from a
callee propagates through a predicate, so predicates containing such calls do
not automatically receive `noexcept`. Rust likewise preserves callee panics.
Backends with existing predicate exception guards retain those guards. Keep
native callees total when the predicate must always return a boolean.

Evaluation preserves the parsed order. Python floating arithmetic inputs are
converted to binary64 after validation, even when application code assigns Python
integers; exact integer comparisons retain integer values. Arithmetic inputs,
intermediates and results must be finite. The original `to_double` conversion is
exact over its declared 32-bit range, including Java unsigned storage. Language
`1.11.0` checked bindings also cover full-width signed/unsigned representations.
Dynamic targets reject fractional/out-of-range integer operands.
Private arithmetic helpers avoid authored type, member and parameter names after
the selected naming conversion. Helper-like schema names keep their original
bindings, including under Java/C# `naming = preserve`.

| Target | Nonfinite input/result |
|---|---|
| C++ | `std::domain_error` |
| Java, C#, Python, JavaScript | Arithmetic/value/range exception |
| Kotlin | `SerializerException` |
| Go, Rust | Panic/assertion failure |
| Swift | Throwing method |
| C | Checked `srl_status` API leaves double/bool output unchanged; convenience API returns NaN (double) or false (bool) and sets `errno` |

## Beautification and diagnostic mapping

C++ assembles the complete translation unit before running the configured
clang-format 19+. Opaque C++ fragments are protected with formatter directives,
so token spelling, literal contents and authored order survive formatting.
Formatter failure leaves existing destination files intact. Other targets retain
their existing generated layout; native Python code is reindented without changing
multiline literal contents.

Each emitted method/native fragment carries `serializer-source-map` metadata:
schema path, one-based line/column, byte span and final physical generated line.
The generated line is computed after complete output assembly and C++ formatting.
Metadata is compact JSON; spaces in schema paths are escaped as `\u0020` so
comment wrapping cannot split the anchor. Decode the JSON string to recover the
original path.
C++ and C# additionally use `#line` around native fragments, restoring generated
locations afterward. Other target diagnostics can be mapped using the preceding
metadata anchor. This is a fragment map, not a statement-by-statement native AST.

```powershell
python test/behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all
python test/behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language java,csharp --preserve-names
python test/typed_behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all
python test/native_behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all
```

Missing SDKs fail qualification. Use `--language` and `--wsl-languages` only to
select explicitly installed toolchains. The native runner also verifies that an
unused missing C++ external definition fails linking through the generated helper,
and that every statically checked target rejects a removed external definition
or incomplete typed implementation. Dynamic targets check missing attachments
before invocation.

See the [behavior verification record](../verification/verification-behavior-2026-10-10.md) for
executed SDK checks and formatting/mapping evidence.
