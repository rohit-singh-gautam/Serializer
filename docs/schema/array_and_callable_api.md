# Array and callable APIs in language 1.17.0

Compiler/runtime **1.22.0** adds these source contracts without changing field
identities or codec representations. Every declaring file, including an included
file, selects `serializer version 1.17.0;`. Earlier language contracts keep their
meaning. [Qualification](../verification/verification-array-callable-api-2026-10-13.md)
records actual checks separately from implementation status.

## Select one owning array

```text
serializer version 1.17.0;
class triangle stable_ids {
  private array[3] uint32 indices (1) expose array(public);
  public constructor(uint32 a, uint32 b, uint32 c) {
    indices = {a, b, c};
  }
}
class grid<uint64 Rows, uint64 Cols, T = double> stable_ids {
  private array[Rows * Cols] T elements (1)
    expose array(public, const_value) dimensions(Rows, Cols);
}
```

`expose array(public)` selects the backing field explicitly; the generator does
not infer a class's array or axes. `protected` and `private` exposure are also
accepted. There may be one exposure per ordinary unpacked owning class, on fixed,
inferred fixed, or dynamic array storage. The builtin `array<T>` spelling is
also supported for dynamic storage. Views, packed/managed owners and metadata
without stored array elements reject. Wire IDs, names, field order and storage
remain the original field's.

The default API provides both writable and readonly access.
`expose array(readonly, public)` omits setters and mutable accessors.
`const_value` selects a value return for const indexing, preserving APIs that
deliberately return small coordinate scalars. Without it, C++ const indexing
borrows `const T&`; mutable indexing borrows `T&`.

`dimensions(Rows, Cols)` declares row-major axes. Axes must be positive and their
product must equal the fixed storage extent, within the existing 65536-element
limit. Open C++ owners check that relationship at specialization; closed output
resolves it before generation. At most eight axes are supported. A dynamic
container does not supply a fixed multidimensional shape.

## Native borrowing and portable logical access

C++ emits native `operator[]`, dimensioned `operator()`, `begin()`, `end()`,
`data()`, `size()` and an implicit conversion to a const reference to the actual
container. These forward directly to `std::array` or contiguous `std::vector`
storage. Range, data and container-reference operations borrow without element
copies or allocation; default indexing/get borrows references, while explicit
`const_value` indexing/get copies T. Index operators keep the container's
unchecked bounds precondition. Methods remain unqualified, so temporary-owner
calls retain ordinary standard-container semantics; they add no `noexcept`
promise. Dynamic `vector<bool>` storage has no contiguous element pointers and
rejects the native facade; fixed `array<bool, N>` remains supported.

Borrowed references, pointers, iterators and C++ `values()` ranges require a live
owner. Moving, replacing or growing storage can invalidate them. Access provides
no synchronization with concurrent writes. A borrow from a temporary cannot
outlive that temporary. These are separate from the lvalue-only
[`expose readonly` getter](owning_api.md#readonly-c-getters).

All backends expose logical `get`, `set`, `size` and `values` helpers, under their
normal naming profiles. Readonly exposure omits `set`. Logical access checks
every coordinate before row-major flattening, rather than accepting an offset
that happens to fall inside the array. Invalid indices use the target's ordinary
error contract. C++ `get` follows the selected reference/value policy and
`values()` borrows the original const container.

Managed-language `values()` creates a shallow snapshot for iteration: primitive
slots are copied, and reference-valued elements retain their native references.
That snapshot allocates and does not promise deep immutability. C uses a const
borrowed pointer and element count, with the owner's lifetime. Its checked
helpers return status and validate storage shape; successful replacement moves
owned inputs according to the existing C ownership contract. None of these APIs
adds codec copies or changes wire behavior. Native pointer/reference conversions
are C++ extras, rather than a portable signature promise.

Generated names must avoid fields, getters, functions and factories after the
selected naming profile. The common helpers reserve `get`, `set`, `size` and
`values`; C++ additionally reserves its native range names.

## Fresh owner-type field factories

```text
serializer version 1.17.0;
class placement<Origin, Basis> stable_ids {
  public Origin origin (1);
  public Basis basis (2) default(identity);
  public constructor() = default;
  public constructor(Origin point) { origin = point; }
  public constructor(Origin point, Basis matrix) {
    origin = point;
    basis = matrix;
  }
}
```

A bare owner type parameter may select `default(identity)` on an ordinary stored
field. C++ uses `Basis::identity()` only when that field's default initialization
is needed. Fully bound construction remains valid for a copy-only Basis with no
default constructor or identity member; no class-wide factory/defaultability
constraint is introduced. The factory result needs to construct Basis, without
an unnecessary exact-return-type constraint. Default and missing-field decoding
retain the authored identity default. Present decoded fields and supplied
constructor arguments retain their normal override behavior.

Host factory exceptions propagate with their original type; deferred factories
do not establish inferred `noexcept`. Constant evaluation depends on the selected
factory and representation. Non-C++ output requires a resolved public schema
zero-argument factory; an unresolved or native-only factory diagnoses that
boundary. Changing a released field's default can change missing-field meaning,
so preserve the existing policy when migrating an adapter.

## Callable-local availability and owning bindings

`when(Rows == Cols)` controls one constructor, named factory or function without
restricting its owner. Comparisons `==`, `!=`, `<`, `<=`, `>` and `>=`, Boolean
`!`, `&&`, `||` and parentheses compose owner dimension conditions.
`same_type(T, double)` compares bare owner types or primitive types. C++ uses a
callable-local constraint; closed output resolves availability before emission.
A square-only identity factory therefore leaves rectangular storage usable.
Function guards currently require ordinary readonly native/external C++ behavior;
closed constructor/factory guards retain their portable availability contract.

`same_type` follows native C++ representation identity in both open guards and
closed substitution. Distinct fixed digest extents compare unequal. A selected
32-byte algorithm such as `digest(sha256)` and `digest[32]` both represent
`std::array<uint8_t, 32>` and compare equal; equal-width algorithm policies share
that representation. Opaque `digest` and `array<uint8>` both represent
`std::vector<uint8_t>` and compare equal. Policy labels do not make these C++
storage aliases distinct types. Closed portable availability preserves this
same declared comparison contract.

Exact-cardinality array bindings use `{a, b, c}`. Bounded array generation uses
`elements = generate(row, column => checked<T>(row == column ? 1 : 0));`, with
axes taken from the selected field's explicit dimensions, or one axis for an
ordinary fixed array. Index names are local and values use typed scalar
expressions. The generator validates shape and owning parameter use. Backend
qualification for these new bindings is recorded separately; unsupported output
combinations diagnose rather than discarding bindings.

An adapter can delegate through its public structural projections:

```text
public constructor(inputs input const_ref) explicit
  delegates(input.first, input.second);
```

Here `inputs` is a declared schema record with typed `first` and `second` fields.
Delegation evaluates projections once and selects an existing typed constructor.
An independent input type can occur inside a known schema application, such as
`constructor<OtherTag>(sample<T, OtherTag> input const_ref)`; an unconstrained
arbitrary host type does not establish inspectable fields. Unconstrained
constructor inputs are accepted only for delegation. The current native
delegation and borrowed-input lowering requires C++ output.

## Static and richer callable signatures

Language 1.17 accepts `function static Result name(...) readonly`, fixed-array
function parameter/return types, and a trailing `const_ref` parameter qualifier.
Static functions cannot project instance state. A C++ `const_ref` parameter
borrows the original storage and does not move or copy it.

Ordinary readonly C++ native/external functions can declare unconstrained
independent inputs, for example
`function Vector apply_vector<Vector>(Vector input const_ref) readonly;`.
These declarations impose no implicit numeric, factory or defaultability
constraint. A portable numeric expression still requires its declared proof;
other targets diagnose the native/external boundary. Language 1.16 and earlier
retain mandatory explicit independent-input constraints.

Callable-only `expected<Result, Error>` maps to `std::expected` on C++23 hosts.
It adds no wire type or implicit exception/result conversion. Native bodies may
provide expected-returning validation and factories; the compiler does not invent
a portable expected runtime. Static, borrowed, fixed-array and expected callable
extensions currently require C++ output, with a clear diagnostic elsewhere.
Expected signatures do not establish automatic C++20 constant-capability;
explicit native qualifiers still require matching eligible definitions.

Builtin expected is accepted only as a direct C++ callable result. Result may be
`void`; Error may not. It cannot be a stored field, parameter, fixed-array return
element, or nested expected/container/schema-generic storage type. A user-declared
class named `expected` keeps type-name precedence. The generated `<expected>`
include and C++23 diagnostic appear only for emitted expected-result APIs;
ordinary models remain C++20. External generic definitions are checked when their
actual specialization is used; nongeneric static definitions use the generated
function-pointer definition checks.

## Migration and verification

Keep field IDs, names and exact defaults. Select the array field and const return
policy explicitly, preserve unchecked native calls and document their borrow
lifetimes. Replace supported adapter construction with typed bindings or
delegation; retain a narrowly scoped native implementation where the schema
cannot express the contract. Compare fresh/missing defaults, explicit copy-only
inputs and all four existing codec bytes before removing an adapter.

The feature passed the **134/134** native suite, all eleven SDKs and the recorded
editor/package checks, including a separate C++ rerun after the final value
qualification fix. Earlier
[1.16 qualification](../verification/verification-generic-owner-defaults-2026-10-12.md)
remains historical and does not certify this release. Public example sources
are unchanged by this feature work.
