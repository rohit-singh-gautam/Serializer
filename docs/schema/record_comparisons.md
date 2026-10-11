# Class-specific record comparisons

Compiler/runtime 1.13.0 can generate equality and lexicographic ordering for
selected owning records in every output backend. This is output configuration;
it adds no schema keyword, stored metadata, field identity, or wire bytes.
Existing output is unchanged when comparisons are disabled.

For example, a schema can contain `example::shipment_key` with durable fields
`warehouse`, `aisle`, and `parcel`, plus a transient cache. Enable comparisons in
the configuration used by its generation rule:

```ini
[cpp.class.example::shipment_key]
equality = true
ordering = lexicographic
fields = durable

[java.class.example::shipment_key]
equality = true
ordering = lexicographic
fields = warehouse, parcel
```

Use the same section form for `js`, `typescript`, `go`, `csharp`, `rust`,
`python`, `swift`, `kotlin`, and `c`. The class name is its fully qualified
**schema spelling**, before the backend's naming profile. Settings are independent
for each output language and each configuration file. Pass the file through
`--config` or `serializer_generate(CONFIG ...)`; the compiler's C++/Java/portable
generator APIs also expose `class_comparisons` settings.

`equality` accepts `true` or `false`, defaulting to `false`. `ordering` accepts
`none` or `lexicographic`, defaulting to `none`. Neither enables the other.
Omitting `fields`, or setting it to `durable`, selects base subobjects first,
then durable authored fields, each in declaration order. Transient fields,
static magic, and hidden managed identity metadata are excluded. An explicit
comma-separated list selects exactly those own fields, in that order; it may
select a transient field and excludes base subobjects. Unknown class/field
names, repeated fields, unsupported shapes and generated API collisions diagnose
before output publication.

## Generated APIs and value semantics

| Backend | Equality | Ordering |
|---|---|---|
| C++ | `operator==` | `operator<=>` and the corresponding relational expressions |
| Java, JavaScript, TypeScript, C#, Python, Swift, Kotlin | `equals_value(other)` | `compare_value(other)` |
| Go | `EqualsValue(other)` | `CompareValue(other) (int, bool)` |
| Rust | `equals_value(&other)` | `compare_value(&other) -> Option<std::cmp::Ordering>` |
| C | `type_equals_value(left, right)` | `type_compare_value(left, right) -> srl_comparison_result` |

Nullable ordering APIs return `-1`, `0`, or `1`, or the language's null/None/nil
value when unordered. Go's boolean reports whether its signed result is ordered.
C returns `srl_less`, `srl_equal`, `srl_greater`, or `srl_unordered`. TypeScript
declares the JavaScript runtime methods.

These methods compare values without changing either input. They do not replace
platform object identity/equality or hashing conventions. Use initialized records
whose selected fields hold values allowed by their schema. Integral comparisons
retain their signedness and full width, including unsigned 64-bit values. Boolean
ordering places false before true; enum ordering follows declaration ordinals.
Portable character comparison uses the supported ASCII scalar profile. Strings
compare exact valid Unicode content, ordered by unsigned UTF-8 bytes, with no
locale or normalization. In particular, Swift's comparison avoids its usual
normalization-aware string equality.

Floating equality treats signed zeros as equal and NaN as unequal, including to
itself. Floating ordering returns unordered for NaN. Lexicographic ordering stops
at the first unequal or unordered selected value. It does not impose a total
order or normalize stored values. Applications needing different NaN, tolerance,
or collation semantics should declare their own behavior.

## C++ and generic requirements

C++ uses references in `std::tie` comparisons, avoiding field copies. Its
three-way result retains the tuple's strong, weak, or partial comparison category.
Members providing `<` and `==` without `<=>` retain tuple synthesized ordering;
generic operators are constrained by the operations their selected members
support. Equality and eligible ordering remain usable in constant evaluation.
Selected ordinary collections can use their existing C++ comparison operations.
The initial profile rejects view/packed models and selected union/variant fields.
C++ direct managed values may compare their authored fields; hidden persistent
identity remains excluded. Managed `separate_values` output rejects comparison
opt-in in this release rather than silently omitting the APIs.

Named schema instantiations retain their existing concrete owning record types
and can select their own comparison settings. Generic source settings apply to
those records unless a concrete selection overrides them. Each backend binds
comparison methods to the actual emitted owner. Compiler-internal C++ generic
applications remain aliases of their native template, so configuration selecting
one of those aliases must agree with the template's selection; conflicts diagnose
instead of changing native type identity.

The portable comparison profile supports scalars, enums, and generated owning
record fields/base subobjects. Nested records must enable the corresponding
comparison operations in the same language's configuration. Selected collections,
optionals, digests, variants/unions and dotted revision types are outside this
initial portable profile and diagnose; an explicit field list may exclude them.
Native snippets are opaque, so avoid manually defining the same generated API
inside one.

See [construction](construction.md), [output configuration](../languages/output_configuration.md),
[behavior](behavior.md), and [qualification](../verification/verification-integration-enhancements-2026-10-11.md).
