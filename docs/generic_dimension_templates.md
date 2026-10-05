# Dimension templates and fixed arrays: implementation specification

**Status:** Implementation and qualification tracked in [the 4 October report](verification-dimensions-2026-10-04.md).

**Date:** 3 October 2026.

**Inspection baseline:** Serializer `d9c3508934a4add65efdec90f402eacd773a7d38`.

## Accepted scope update (4 October 2026)

The implementation request was extended to native C++ templates. C++ declarations
must work without any `instantiate` declaration or concrete schema field, including
`result<T>`, `response<T>` containing `result<T>`, and `message<T>` containing
`response<T>`. Application code may choose `response<std::uint32_t>` directly.
The original finite C++ binding requirement below is superseded by this instruction.
`instantiate` and concrete field applications remain optional cross-language and
compatibility-check contracts. Other-language native generic APIs are not required;
fixed arrays there use the explicitly permitted unsupported diagnostic path.

The sections below retain the original acceptance criteria except where this
scope update explicitly supersedes them. See [implemented usage](generics.md)
and the qualification report for actual support and executed checks.

## 1. Purpose and current behavior

Extend Serializer's existing owning-class generics with positive integer dimension
parameters, trailing default arguments, and fixed arrays whose extents are checked
constant expressions. All records and codecs must continue to originate in
`.serializer` schemas and pass through the shared parser and lowering pipeline.

At the inspection baseline, the generics contract supported type parameters and
finite concrete instantiations, excluding value parameters and defaults. The
[current contract](generics.md) documents the implementation after this change.
The parser represents generic parameters as names; lowering resolves arguments as
types. This extension needs explicit parameter/argument kinds, not numeric values
disguised as type names or handwritten codecs.

Required examples are `point<N>`, `frame<N>`, and
`matrix<Rows, Cols = Rows, T = double>`. These are reusable schema examples, not
new built-in geometry types or a mathematics library.

This specification proposes additive schema-version-1 syntax. Before release,
document that older generators reject it and identify the minimum supporting
compiler revision. Preserve existing schema syntax, concrete generated identities,
wire encodings, and type-only generic behavior.

## 2. Scope

Implement:

- Positive `uint64` dimension parameters and serializable type parameters in one
  generic declaration, with ordered argument binding.
- Trailing type/value defaults, including a value default referencing an earlier
  dimension parameter.
- Fixed owning arrays with extents evaluated from dimension parameters and
  checked integer arithmetic.
- Canonical concrete specialization identity after default substitution.
- C++ aliases for the finite declared specialization set and concrete outputs or
  explicit unsupported diagnostics for every other backend.
- Ordinary generated values containing these types inside managed C++ roots,
  including editors, history, serialization and native journal recovery.
- Compatibility analysis, bounded decoding, editor tooling and executable evidence.

Out of scope: arbitrary C++ expression evaluation, function templates, variadics,
partial/explicit user specializations, generic inheritance, recursive ownership,
managed generic declarations, managed types as generic arguments, new matrix
arithmetic, collaboration qualification, and unrestricted native template
instantiation. Existing restrictions on unions, packing and views remain unless a
separate change qualifies them. Unsupported combinations must diagnose explicitly.

## 3. Proposed schema syntax

The syntax below is the implementation target, not accepted syntax in the
inspection baseline. If implementation requires a different grammar, update this
specification and its fixtures together before publishing the feature.

```text
serializer version 1;

namespace dimension_example {
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

  instantiate point_2 = point<2>;
  instantiate point_3 = point<3>;
  instantiate frame_2 = frame<2>;
  instantiate frame_3 = frame<3>;
  instantiate matrix_3 = matrix<3>;
  instantiate matrix_3_explicit = matrix<3, 3, double>;
  instantiate matrix_2_by_3_float = matrix<2, 3, float>;

  class sample stable_ids managed {
    public point<3> position (1);
    public frame<3> coordinate_frame (2);
    public matrix<3> implicit_square (3);
    public matrix<3, 3, double> explicit_square (4);
  }
}
```

`array Type` remains a variable-length collection. `array[expression] Type`
introduces an exact extent and is also permitted in a non-generic owning class.
Class declarations have no trailing semicolon; named instantiations retain their
existing semicolon and distinct named-root semantics.

### Parameter binding and defaults

1. Bare names declare type parameters; `uint64 Name` declares a dimension
   parameter. The first profile supports no other value-parameter kind. Despite
   using `uint64` for storage/evaluation, dimension arguments must be positive.
2. Bind arguments left to right. Once a parameter has a default, every subsequent
   parameter must have a default. Do not permit skipped interior arguments.
3. Defaults may reference earlier parameters of the appropriate kind. Reject
   self-reference, forward references and default cycles. Validate defaults even
   when a caller supplies an explicit argument where the error is independent of
   substitution.
4. Type defaults may be visible serializable types, earlier type parameters, or
   supported generic applications. Value defaults use the expression grammar
   below. Definition names resolve in declaration scope; explicit arguments
   resolve in use-site scope, preserving existing generic lookup behavior.
5. Reject excess/missing arguments and type/value-kind mismatches. An empty
   argument list is valid only if every parameter has a default.
6. Explicit `matrix<3, 3, double>`, `matrix<3, 3>` and `matrix<3>` bind to the same
   concrete specialization. `matrix<3, 4>` is distinct even if another shape has
   the same total element count.

### Checked constant expressions and budgets

The initial grammar accepts decimal integer literals, earlier bound dimension
parameters, parentheses, addition and multiplication. Multiplication precedes
addition; both are left-associative. No macros, function calls, floating-point
values, environment access, casts, division, shifts or arbitrary host evaluation.
Recognize negative input sufficiently to report an invalid dimension rather than
allowing unsigned wraparound.

Evaluate with checked `uint64` arithmetic. Reject literal overflow and every
intermediate addition/multiplication overflow before executing it. Do not use a
host-width `size_t` for canonical evaluation. Validate the final dimension/extent
as positive; conversion to a backend index/storage type is a separate checked step.
The literal zero can occur inside arithmetic; a bound dimension or final extent
of zero is invalid. No simplification may hide an overflowing intermediate.

Retain the existing limits of 32 parse/expansion levels, 1,024 concrete generic
applications, and 4,096 canonical identity bytes. Proposed additional hard limits
are 32 expression levels, 256 expression nodes per expression, and 65,536 elements
per fixed array. Count nodes while parsing and validate extents before allocating
or emitting storage. Test exact limits and adjacent values. Check nested extents
and minimum fixed-storage byte products for overflow and backend representability;
ordinary runtime aggregate byte, element and depth budgets still apply.

These limits are part of the implementation contract. Any adjustment must be
recorded in this document, public documentation and evidence before acceptance.
Do not silently truncate an extent or relax runtime decode limits because its size
was known during generation.

## 4. Shared representation and canonical identity

Extend the shared syntax representation with explicit type/value parameter kinds,
optional default expressions, type/value arguments, expression source spans and
fixed-versus-variable collection metadata. Retain the parsed form long enough for
diagnostics and navigation. Lower a specialization once before language writers,
managed generation or compatibility analysis consume it.

Canonical keys include the qualified generic declaration and the complete ordered
list of bound argument kinds and values. Normalize value arguments to evaluated
integers and type arguments to resolved canonical types. Whitespace, parameter
names, literal spelling and omission of defaults do not create new identities.
Value `3` and a type whose spelling resembles a number must never collide.

Preserve existing keys and generated names for type-only generics that do not use
the new features. Extend the identity encoding without making old names unstable.
Equivalent defaulted/explicit uses share one lowered node and one C++ alias target.
Continue detecting recursive expansion and generated-name collisions, including
names produced under every C++ naming profile.

Named `instantiate` roots remain distinct classes under the existing contract.
The identity requirement concerns the underlying generic specialization, not
`matrix_3` versus `matrix_3_explicit` as named roots. Test both cases explicitly.

Changing a default does not rebind an explicit argument. Omitted arguments use the
new default and can therefore change the concrete type and compatibility result.
Renaming a template parameter alone must not change schema identity or wire data.

## 5. Generated APIs and codecs

### C++

Generate fixed owning storage as `std::array<T, extent>`. Emit finite template
bindings with positive integral non-type parameters and matching trailing
defaults, so declared uses of `matrix<3>` and `matrix<3, 3, double>` are the same
C++ type. Preserve naming-profile transformations and existing codec entry points.

An undeclared specialization must fail clearly at compile time. Invalid dimensions
must not become huge unsigned allocations or silently select another binding.
Do not instantiate arbitrary schema classes from C++ alone. Demonstrate generated
consumers in C++23 while preserving Serializer's existing C++20 minimum.

### Wire behavior and bounded input

Keep the existing sequence representation, including its element count, for fixed
arrays in `binary_none`, `binary_integer`, `binary_string`, and JSON. A fixed array
and an ordinary array with the same elements encode identically under the same
protocol, field IDs and order. Dimensions add no runtime type tags or template
metadata. Freeze exact golden bytes for the three native binary protocols.

Fixed extent is a schema constraint: a present field must contain exactly that
many elements. Reject shorter/longer collections, excessive counts, truncation,
invalid elements and budget overflow. Binary readers validate count before reading
elements; JSON readers bound iteration and reject both early termination and extra
elements. Preserve existing missing-field/default rules and document them in the
fixtures; do not reinterpret a missing field as a valid explicitly empty array.

Exact fresh-value decoding must leave caller-owned published state unchanged on
failure and reject trailing bytes. Ordinary in-place readers retain their existing
documented failure behavior. Check aggregate budgets even for stack/fixed storage.
Byte-order conversion and any bulk/SIMD path must match scalar reference bytes.

For direct Protobuf output, specify and test the repeated-field mapping and exact
cardinality checks, including omitted-field behavior, before advertising support.
Otherwise reject this feature explicitly in Protobuf generation. Do not silently
weaken cardinality. Existing managed Protobuf restrictions remain in force.

### Other output languages

All eleven generators receive the same resolved element type and extent. Java,
JavaScript, TypeScript, Go, C#, Rust, Python, Swift, Kotlin and C may expose ordinary
concrete records without public generics. Use fixed native storage where suitable;
where storage is dynamically sized, validate exact cardinality on encoding and
decoding. Document ownership, initialization and each language's length limits.

For each backend, supply a working concrete mapping with round-trip/boundary tests,
or a deterministic generation-time unsupported-feature diagnostic naming the
feature and source span. No backend may silently emit a variable-size contract.
Record the support matrix explicitly; only tested mappings count as supported.

## 6. Managed records, history and journaling

Qualify a non-generic managed root containing ordinary `point<3>`, `frame<3>` and
`matrix<3>` values. The template classes themselves need not be managed. Use the
existing generated traits/editors and `Serializer::managed`, with
`[managed] id_type = uint64` and `separate_values = false` for the primary fixture.

The minimum editing API replaces an ordinary contained value through a generated
setter. Element setters are optional; any provided element edit must check bounds
and the transaction lifetime without permitting insert/erase/resize operations.
No manual codec or parallel history/journal implementation may stand in for this
integration.

Use tree history with labels enabled, commit two distinct values, undo, create an
alternate branch, redo/checkout retained revisions, save, and recover a native
sidecar journal in a fresh process. Verify complete arrays and nested fields, root
identity, document namespace, revision/cursor behavior and retained branches.
Exercise rejection/no-change outcomes and over-budget or malformed recovered
snapshots without partial state publication. A failed transaction must preserve
the previous root and cursor. Preserve the library's indeterminate-outcome fencing
and single-writer behavior.

Retain regression coverage for the existing `uint32` ID and separate-values
configurations; test the new containment path or reject unsupported combinations
explicitly. Collaboration, authorization and replicas are not prerequisite tests
for this local feature.

## 7. Compatibility and diagnostics

Extend compatibility checks to compare lowered element types and exact extents,
not raw parameter spellings. For identical extents and element contracts,
defaulted/explicit forms and parameter renaming are compatible. Different extents
are incompatible even if total byte sizes match. For fixed versus variable arrays,
an unrestricted writer cannot satisfy a fixed reader; a fixed writer can satisfy
a variable reader when existing element/protocol rules allow it. Respect the
selected reader direction and make both-direction checks conservative.

Test changes in defaults separately for explicit and omitted uses, stable field
IDs, reservations, field order and each supported compatibility protocol. Prove
pre-extension type-only schemas retain their generated names and wire bytes.

Diagnostics must distinguish invalid dimension, arithmetic overflow, kind mismatch,
invalid default, excessive extent, expansion/resource limit, unsupported backend
and unsupported host specialization. Report the offending source span and include
the instantiation chain for a nested failure. Runtime length errors must identify
the expected and observed cardinality where the existing error interface permits.

## 8. Implementation order and tooling

1. Add shared parameter/argument/expression and fixed-array metadata; update parser
   tests without changing existing type-only lowering behavior.
2. Implement bounded evaluation, default substitution, canonicalization and
   specialization caching in the shared lowering path.
3. Implement C++ fixed storage, alias defaults, codecs and compatibility checks;
   run scalar wire and bounded-decode tests before optimizing.
4. Qualify ordinary generic values inside generated managed roots and journals.
5. Implement/test each other backend mapping or its explicit rejection path.
6. Update both editor extensions for syntax, parameter references, defaults,
   expressions, instantiations and generated-output navigation. Follow repository
   version synchronization and package-validation rules when changing extensions.
7. Update `README.md`, the integration skill, `docs/generics.md`, schema reference,
   usage, wire format, feature status and migration documentation to match actual
   support. Publish reproducible qualification evidence with the final commit.

The likely implementation surfaces are `src/parser.cpp`,
`src/schema_generics.hpp`, shared nodes in `include/rohit/serializer_creator.hpp`,
language writers, collection codecs, managed generation and compatibility code.
Confirm ownership in the current tree before editing; this list is not a mandate
to add duplicate paths. Generate headers through `serializer_generate` into the
build directory and test clean regeneration.

This document alone does not change supported syntax or installed extensions.
Keep current feature claims unchanged until implementation and qualification pass.

## 9. Acceptance matrix

| ID | Required executable evidence |
|---|---|
| D01 | Generate/build C++23 consumers for 2D/3D points and frames, square/defaulted matrices, and rectangular float/double matrices; C++20 regression build also passes. |
| D02 | Defaulted/explicit square references share lowered identity and C++ type; named instantiation roots retain their documented distinct identity. |
| D03 | Mixed type/value arguments, earlier-parameter defaults, qualified/include lookup and nested applications resolve correctly; invalid arity/kinds/forward/self defaults reject. |
| D04 | Reject zero/negative dimensions, oversized literals, addition/product overflow, excessive extents and unsupported operators without wraparound or unbounded allocation. |
| D05 | Test exact/adjacent expression, depth, specialization, identity and extent limits, including nested fixed-array products and backend size conversions. |
| D06 | Round-trip distinct nonzero elements in all four native protocols; golden binary bytes and JSON values equal equivalent ordinary-array records. |
| D07 | Reject count minus/plus one, huge counts, truncation, invalid elements, trailing bytes and byte/element/depth budget excess; failed exact decode preserves published state. |
| D08 | Compatibility checks cover extents, element types, changed defaults, parameter renaming, fixed/variable reader directions and stable-ID reservations. |
| D09 | Generated managed setters, no-change/rejection, tree branch navigation, memory save/load, sidecar full Save and fresh-process recovery preserve all nested values and identities. |
| D10 | Corrupt/over-budget journal data and interrupted tail recovery follow existing journal contracts; no partial managed state becomes visible. |
| D11 | Every language has tested concrete mappings or a tested unsupported diagnostic; Protobuf support/rejection is separately explicit. |
| D12 | All C++ naming profiles and both editor extensions preserve parameter/include/generated-output navigation; extension packages use matching versions. |
| D13 | Existing type-only generics, ordinary arrays, managed examples, codecs and compatibility fixtures pass without changed historical names or wire bytes. |

## 10. Evidence and completion

Store checked-in input schemas, positive/negative test cases and runnable consumers
in the maintained example/test layout. The qualification report must record source
commit/dirty state, generator and runtime agreement, schema/output digests, compiler
and standard-library versions, C++ mode, OS/architecture, managed configuration,
resource limits, commands, exit results, fixture artifacts and backend limitations.

Windows native execution is required for the primary report. Record Linux native
or other platform results separately; do not infer them from another environment.
Tie journal recovery evidence to the actual filesystem/storage profile and process
termination boundaries tested; process interruption is not proof of every possible
power-loss scenario.

Acceptance requires D01-D13 with explicit backend support boundaries and no missing
required C++ or managed case. Parser success alone, a generated header alone, or
an ordinary type-generic test does not qualify dimension templates. Publish the
supporting immutable revision only after the evidence is complete. Gate results and environment details are recorded in the linked qualification report;
this document alone is not a verification claim.
