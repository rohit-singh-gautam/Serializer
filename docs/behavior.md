# Schema behavior and native source

Schema 1.6 declares behavior alongside durable fields. Methods and target source
have no wire IDs and do not enter codecs, managed fingerprints, history comparison
or collaboration patches. [Runnable native schema](../test/resources/document_native_behavior.serializer)
and [SDK consumers](../test/native_behavior_languages.py) cover every target.

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
implicit integer arithmetic, loops, assignment, general calls and division are
rejected. Bounds are 1024 expression nodes and depth 64. This deliberately small
language leaves application-specific logic in native code.

Evaluation preserves the parsed order and Python symbols are converted to
binary64 after validation, even when application code assigns Python integers. Inputs and result must be finite; integer
conversions are exact over the declared 32-bit range, including Java unsigned
storage. Dynamic targets also reject fractional/out-of-range integer operands.

| Target | Nonfinite input/result |
|---|---|
| C++ | `std::domain_error` |
| Java, C#, Python, JavaScript | Arithmetic/value/range exception |
| Kotlin | `SerializerException` |
| Go, Rust | Panic/assertion failure |
| Swift | Throwing method |
| C | Checked `srl_status` API leaves output unchanged; convenience API returns NaN and sets `errno` |

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
