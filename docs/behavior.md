# Schema behavior and native source

Schema 1.6 declares behavior alongside durable fields. Methods and target source
have no wire IDs and do not enter codecs, managed fingerprints, history comparison
or collaboration patches. [Runnable native schema](../test/resources/document_native_behavior.serializer)
and [SDK consumers](../test/native_behavior_languages.py) cover every target.

For a focused C++ walkthrough, build the [custom code example](../example/cpp/custom_code/README.md).
Its preamble, native class declarations, inline methods, and external `.cpp`
definition operate on ordinary schema-owned fields and use the generated codecs.
The editor extensions reuse available native language grammars for
[embedded syntax highlighting](editor_extension.md#embedded-source-highlighting).

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
[its native fixture](../test/resources/document_native_behavior_go.serializer) uses
`Measure`, `Update` and `Score` explicitly. Rust/Python use the existing fixed snake_case member spelling; Swift/Kotlin use
fixed camelCase. Preambles must be valid at the documented file location, such as
Kotlin imports before generated declarations. Native payloads are bounded to
1 MiB and nesting to 128; comments and quoted/raw literal delimiters are scanned.

## Portable arithmetic

```text
public function double average(double copies) readonly
  expression((words_per_page + 2.0) * to_double(page_count) * copies);
```

The portable form supports finite binary64 literals, declared scalar symbols,
`math.pi`, unary `+`/`-`, binary `+`/`-`/`*`, grouping and explicit
`to_double(int32_or_uint32)`. Arithmetic operands and the result are `double`;
implicit integer arithmetic, arbitrary loops, assignment and undeclared calls are rejected.
Bounds are 1024 expression nodes and depth 64. This bounded language leaves
application-specific logic in native code.

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
other eligible typed functions. Native, external and dispatched callees retain
their own constant-evaluation and exception contracts. A call to such a function
prevents automatic `constexpr`/`noexcept` inference for the caller. Calls do not
change wire layouts, field IDs or persisted state. See the
[expression example](../example/cpp/expressions/README.md) and
[constant evaluation guide](constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers).
The borrowed `cpp.emission_only` profile currently rejects namespace functions;
ordinary owning C++ output supports them.

Language `1.8.0` adds binary64 division, `math.sqrt(value)`, `math.sin(value)` and
`math.cos(value)`, numeric comparisons `<`, `<=`, `>`, `>=`, matching scalar/enum
equality `==`/`!=`, boolean literals/parameters/fields, `!`, `&&`, `||`, and
`condition ? when_true : when_false`. Conditions must be boolean; both branches
must have the same scalar type, and the declared return type must match.
Arithmetic requires explicit double operands. Comparisons retain matching signed
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
parameters or outer binders. Maps, optional/variant projection, integer arithmetic,
mutation and undeclared calls are outside this profile.

C++ emits `constexpr`/`noexcept` for eligible readonly typed methods, including
exact scalar predicates and bounded projections/reductions whose operations permit
constant evaluation. Compiler/runtime 1.15.0 also marks eligible readonly results
`[[nodiscard]]` and allows checked addition, subtraction, multiplication, division,
sign operations and integer-to-double conversion in constant evaluation.
Reached invalid input, arithmetic or indexing still throws for `expression` and
returns false for `predicate`; checked expressions remain potentially throwing.
Square root and trigonometry remain runtime under C++20; native/opaque bodies stay
application-owned. `noexcept` is inferred independently of `constexpr`.

Declared calls retain the callee's failure contract. In C++, an exception from a
callee propagates through a predicate, so predicates containing such calls do
not automatically receive `noexcept`. Rust likewise preserves callee panics.
Backends with existing predicate exception guards retain those guards. Keep
native callees total when the predicate must always return a boolean.

Evaluation preserves the parsed order. Python floating arithmetic inputs are
converted to binary64 after validation, even when application code assigns Python
integers; exact integer comparisons retain integer values. Arithmetic inputs,
intermediates and results must be finite; integer conversions
are exact over the declared 32-bit range, including Java unsigned
storage. Dynamic targets also reject fractional/out-of-range integer operands.
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

See the [behavior verification record](verification-behavior-2026-10-10.md) for
executed SDK checks and formatting/mapping evidence.
