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
strings, vectors, optionals and fixed arrays support this declaration when their
contained values do. Literal bindings and `default(factory)` retain constant
construction. Open type parameters defer constant evaluation to the chosen C++
specialization; named schema instantiations use their resolved types. Allocating
standard-library values still follow C++ constant-evaluation lifetime rules.
Maps, variants, recursive storage, managed records, polymorphic behavior and
opaque class storage retain ordinary construction. No constant evaluation is
promised for those shapes, and their runtime APIs remain available.

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
