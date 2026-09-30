# Proposed managed classes and language bindings

Current C++ representation: managed schema classes carry `persistent_id` directly
by default. Separate ID-free values/storage wrappers in the illustrations below
require `[managed] separate_values = true`; see the [runtime contract](cpp_runtime.md).
New documents get a namespace automatically. Root and managed descendants allocate
IDs `1, 2, 3...` within that document only; saved IDs/counters survive reload,
and undo/deletion never renumber survivors. Explicit member boundaries still apply.

Status: broader design sketches. The [C++ interface](cpp_runtime.md) implements
schema-driven identity, typed editors, transactions, and snapshot history. These
broader sketches are not its exact API or generated output. Other-language managed
runtimes remain proposals; their generators currently reject managed annotations.
See the [design index](README.md) and [data structures](data_structures.md).

The [C++ class walkthrough](data_structures.md#c-class-walkthrough) shows actual
ordinary data classes generated from [walkthrough.serializer](walkthrough.serializer),
with codec functions omitted. It maps those classes to the proposed runtime below;
the store templates, transaction guards, and tracked editors are not outputs of
that ordinary schema or implemented managed-code generation today.

The [capability contract](capabilities.md) now names history, collaboration,
authorization, and journaling. Every generated managed storage object
carries `persistent_id`, defaulting to `uint32` under central configuration.
The [recovery design](journal.md) separates durable journal sequence
from history cursor and requires platform storage adapters.

## History policy and independent capabilities

`Labels` defaults to `history_labels::disabled`; enable it explicitly for action
names. It is independent of the linear/tree choice.

The implemented API is `model_store<Root, Mode, Labels, Traits>`, with `Mode` fixed at
compile time to disabled, linear, or tree. Only that history representation is
stored. Linear history has ordered snapshots and a cursor, with `undo()`/`redo()`
and no revision IDs. Only tree history provides revision-based navigation.
Journaling need not depend on either representation or its revision IDs.
The broader capability sketches below are proposals, not the current
public signature.

Future collaboration, journal, and authorization capabilities should compose
independently of this one history policy. Each enabled capability contributes its
own storage and transaction hooks; disabled capabilities contribute no component
state. A journal's durable sequence and recovery retention are independent of
undo revisions and deque eviction. Collaboration tracks accepted shared changes
and requires its own conflict/undo semantics; choosing tree history does not itself
implement collaboration. Authorization checks permission before publication.
These components require concrete protocols and failure guarantees before they
can be exposed as supported template options.

## C++: a capability template is appropriate

The suggested `template <typename Root, supported_mechanism support>` shape works
for an owning managed store. Keep `model_store` as the descriptive implementation
name used throughout the proposal, and allow `managed` as a short alias for that
same type. This does not introduce a second owner or replace generated
`tracked_cylinder`/`tracked_task` editors.

```cpp
#include <cstdint>
#include <string_view>

// Illustrative native flags; these numbers are not a persistent wire contract.
enum class supported_mechanism : std::uint32_t {
  none = 0,
  history = 1u << 0,
  collaboration = 1u << 1,
  authorization = 1u << 2,
  journal = 1u << 3
};

// Change-record placement is a runtime policy independent of the feature mask.
enum class journal_storage_mode { appended, sidecar };

// Combine the optional mechanisms supported by this store specialization.
constexpr supported_mechanism operator|(
    supported_mechanism left, supported_mechanism right) noexcept {
  return static_cast<supported_mechanism>(
    static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

// Test whether every requested mechanism is present in a support mask.
constexpr bool has_support(
    supported_mechanism available, supported_mechanism requested) noexcept {
  const auto available_bits = static_cast<std::uint32_t>(available);
  const auto requested_bits = static_cast<std::uint32_t>(requested);
  return (available_bits & requested_bits) == requested_bits;
}

// Empty disabled slots contain no feature engine or feature-owned containers.
template <bool Enabled>
struct history_slot {};

template <>
struct history_slot<true> {
  history_state state;
};

template <bool Enabled>
struct collaboration_slot {};

template <>
struct collaboration_slot<true> {
  collaboration_state state;
};

template <bool Enabled>
struct authorization_slot {};

template <>
struct authorization_slot<true> {
  authorization_state state;
};

template <bool Enabled>
struct journal_slot {};

template <>
struct journal_slot<true> {
  journal_state state;
};

template <typename Root,
          supported_mechanism support = model_traits<Root>::default_support>
class model_store {
  static_assert(model_traits<Root>::supports_managed);

  managed_core<Root> core_;
  [[no_unique_address]]
  history_slot<has_support(support, supported_mechanism::history)> history_;
  [[no_unique_address]]
  collaboration_slot<has_support(support, supported_mechanism::collaboration)>
    collaboration_;
  [[no_unique_address]]
  authorization_slot<has_support(support, supported_mechanism::authorization)>
    authorization_;
  [[no_unique_address]]
  journal_slot<has_support(support, supported_mechanism::journal)> journal_;

public:
  // Begin the sole writer; outcome and store must outlive the returned guard.
  [[nodiscard]] transaction<Root> begin_transaction(
    std::string_view label, transaction_outcome& outcome);

  // Execute one synchronous void callback with a borrowed edit context, then complete.
  template <typename Callable>
  [[nodiscard]] transaction_outcome execute_transaction(
    std::string_view label, Callable&& callback);

  // Restore the parent revision atomically under current authorization.
  void undo() requires (has_support(support, supported_mechanism::history));
};

template <typename Root,
          supported_mechanism support = model_traits<Root>::default_support>
using managed = model_store<Root, support>;
```

`managed_core`, descriptors, transaction types, and optional engine types refer to
conceptual records in the accompanying design, not existing declarations. A real
implementation additionally validates unknown bits, backend availability, and
mechanism dependencies. The generated default support set is pinned to explicit
generation/build configuration; a compiler upgrade must not silently widen it.
Its precise configuration spelling remains undesigned.

This separates actual member storage by template specialization; `if constexpr`
alone would not remove an unconditionally declared history member. The
[`no_unique_address` attribute](https://eel.is/c++draft/dcl.attr.nouniqueaddr)
permits empty members to overlap other storage. It does not promise an identical
byte size on every compiler/ABI. Measure layout, allocation counts, code size, and
build times on supported C++ toolchains rather than promising universal zero cost.

Construction examples, assuming both schema and backend support these choices:

```cpp
using identity_store = managed<design, supported_mechanism::none>;
using local_history_store = managed<design, supported_mechanism::history>;
using shared_design_store = managed<design,
  supported_mechanism::history | supported_mechanism::collaboration |
  supported_mechanism::authorization>;
using recoverable_design_store = managed<design,
  supported_mechanism::history | supported_mechanism::journal>;

// Runtime tree policy does not require another C++ class specialization.
local_history_store store{
  design{}, history_options{.mode = history_mode::tree}};
```

For a journal-enabled store, runtime journal options select
`journal_storage_mode::appended` or `journal_storage_mode::sidecar`. Both use the
same recovery contract; changing physical layout requires a coordinated save or
migration, not simply changing a flag while writers retain old file handles.
See [full Save and cleanup](journal.md#full-save-and-cleanup).

Even `identity_store` retains identities, ownership validation, transactions, and
the requirement to reject missing policy support when authorization is required.
Its entities are managed; it is not an ordinary
`design`. A standalone `design{}` still creates the normal value representation.
An unavailable undo method can be excluded through constraints; a dynamic API must
return an unsupported-operation error instead of pretending that undo succeeded.

Compiling history support allows runtime disabled/linear/tree recording. A disabled
mode can avoid allocating revision payloads without removing the feature slot from
the type. Switching modes cannot instantiate mechanisms omitted at compile time.
Changing the template support set requires another store type and an explicit
transfer/import that validates identity, profile, and retained-format compatibility.
Use composition for adapters; deriving the store from `Root` would expose public
payload writes that bypass transaction tracking.

Avoid a permanent history/collaboration object in every entity. The engines belong
to the store. Generated typed editors carry a scoped context and entity locator;
they route operations into those engines only through the store's mutation path.

The optional `collaboration_state` also owns the
[session, presence, grant/lease, and replica-cache state](collaboration.md).
Portable record classes must be generated from `.serializer`; runtime indexes
and scoped handles wrap them. No generated payload needs a lock object or session
pointer. Every backend uses the same authoritative acceptance and expiry rules.
C++ RAII guards enqueue best-effort release without waiting for network I/O;
explicit release exposes completion. Other languages use explicit close/dispose,
scoped cleanup, or equivalent guards; garbage collection is not a timely release
guarantee. Host-driven leases cover crashed sessions and undelivered release.
Language bindings must represent pending remote acceptance explicitly: destruction
or local transaction completion alone cannot report an authority's acceptance.

## Transaction entry points

For a single synchronous action, prefer the
[callback transaction API](history.md#callback-based-transaction-execution).
The wrapper lends `transaction_edit<Root>&` to the callable, owns the RAII guard,
and returns the completion outcome after cleanup. The edit context permits revert
but not early commit. Constrain the C++ callable to a `void` result; invoke it
without required type erasure or callable allocation. These runtime facade types
are proposed interfaces, not new generated payload classes.

Use equivalent callback wrappers in other languages, with explicit cancellation
and recorded exceptions/errors under that backend's contract. Reject accidental
future/promise/task returns in the synchronous API. C and Go callbacks may use an
explicit error/status return because exceptions are not their failure mechanism;
the adapter must interpret that result before completion, never ignore it.
An asynchronous transaction API would need a separate lifetime/cancellation design.
`begin_transaction` remains available for longer caller-controlled scopes.
In C++, retain all three forms: explicit commit on the returned guard, automatic
commit on healthy normal destruction when not explicitly completed, and callback
execution. Explicit commit closes once; revert cancels. Deleting a guard invokes
the same destructor as leaving scope and is not a separate cancellation mechanism.

## Support, participation, and activation

The three levels cooperate as follows:

1. Schema generation resolves `managed`, `managed(history)`, and `exclude(...)`
   against a named/versioned feature profile. It validates managed target capability
   and every selected feature the backend is asked to generate.
2. A store specialization or factory selects the optional mechanisms it supports.
   Explicitly selecting a smaller set does not rewrite the schema's profile or
   delete identities; it limits which mechanisms this instance can activate.
3. Runtime options activate a subset of that support and configure policies/adapters.
   Member participation and ancestor exclusions further restrict each operation.

An explicit history-only store can deliberately leave collaboration inactive even
when schema metadata permits collaboration. This is different from accepting an
unknown/unimplemented schema feature silently. Reject requested runtime mechanisms
outside the supported set, invalid dependencies, and selected adapters unavailable
in that backend. A required adapter dependency must be explicit and validated;
collaboration does not automatically mean history, networking, or authentication.

The effective optional feature set is limited by compiled/factory support, runtime
activation, the profile, the member selector, and ancestor participation. Enforced
authorization and core transaction rules are mandatory outside that intersection.
An ordinary occurrence never activates nested managed children merely because
its type has a generated companion.

Feature-specific payloads can be owned inline, behind optional pointers, or in a
component table. Physical omission of a disabled engine is a backend optimization;
the absence of retained history records and rejection of unsupported operations
are semantic requirements. Saved metadata uses a versioned portable feature
contract, not the raw layout or numeric values of this example's C++ enum.

## Other languages: composition with the same logical records

A language does not need C++ non-type templates to implement this design. Generate
ordinary payloads plus typed editors/descriptors, then compose a store core with
optional history/collaboration components. A disabled component has no engine
allocation; a nullable field or component-table entry may still have small overhead.
If physical field omission matters, generate named store variants for selected
profiles rather than every possible combination of features.

| Backend | Proposed store representation | Optional data and completion strategy |
| --- | --- | --- |
| C++ | `model_store<Root, support>` / `managed` alias | Specialized slots and constrained APIs; RAII guard plus observable completion. |
| Rust | `ModelStore<Root, HistoryPolicy, CollaborationPolicy>` | Policy-associated storage types, including empty disabled states; result-aware transaction scope with drop cleanup. |
| Swift | Generic `ModelStore<Root>` with composed engines; policy types where useful | Optional engine storage or generated variants; throwing scoped edit closure with deterministic cleanup. |
| Java | `ModelStore<T>` plus generated `ModelDescriptor<T>` | Optional component references and a validated feature set; callback scope distinguishes success from exceptions. |
| Kotlin | `ModelStore<T>` plus generated descriptors | Same component approach as Java; scoped edit function, cancellation/failure handled explicitly. |
| C# | `ModelStore<T>` plus descriptors, optionally typed policy parameters | Optional engines or generated variants; result-aware edit scope, not unconditional commit from `Dispose`. |
| Go | `Store[T]` plus a generated descriptor and options | Optional engine pointers; explicit error-returning transaction wrapper and deferred cleanup. |
| Python | Store with generated descriptor and ordinary payload classes | Components created only when enabled; context manager or callback with explicit completion result. |
| JavaScript | Store object plus generated descriptors | Optional engine objects; synchronous scoped callback or an explicitly awaited async scope. |
| TypeScript | Typed `ModelStore<T>` facade over the JavaScript runtime | Runtime feature validation and optional engines; generic types alone cannot control runtime allocation. |
| C | Opaque store handle plus generated typed adapters | Component pointers or profile-specific structs; explicit finish/revert/destroy and allocator callbacks. |

These are implementation proposals, not promises of identical specialization or
object sizes. Java uses [type erasure](https://docs.oracle.com/javase/specs/jls/se25/html/jls-4.html#jls-4.6),
so a generic feature type by itself does not select different declared instance
fields. TypeScript [removes type annotations](https://www.typescriptlang.org/docs/handbook/2/basic-types.html#erased-types)
from emitted JavaScript, requiring a real runtime feature value. C# preserves
generic information with [different runtime behavior](https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/generics/generics-in-the-run-time);
that still does not make a boolean options flag remove an ordinary declared field.
Rust offers [type and const parameters](https://doc.rust-lang.org/reference/items/generics.html);
policy types are one way to select storage without relying on unrestricted const
expressions. Go's [type parameters](https://go.dev/ref/spec#Type_parameter_declarations)
can type the payload/descriptor; runtime component options remain a straightforward
way to choose mechanisms.

## Example construction in managed runtimes

Illustrative Java API; generated factories/descriptors avoid requiring reflective
construction of an arbitrary `T`:

```java
// Proposed API: generate typed editors and use runtime component configuration.
ModelStore<Project> store = ModelStore.create(
  ProjectManaged.descriptor(),
  new Project(),
  StoreOptions.historyOnly(HistoryMode.TREE));
```

The corresponding logical Java storage is:

```text
ModelStore<T>
  CoreState<T> core
  ModelDescriptor<T> descriptor
  FeatureSet supportedFeatures
  HistoryEngine history              nullable when unsupported
  CollaborationEngine collaboration nullable when unsupported
```

The core owns committed payloads and the registry; an editor receives the current
transaction context. Generated setters update its candidate, not the ordinary
object held by an unrelated caller. Neither garbage collection nor a language
finalizer determines when a transaction commits.

Illustrative TypeScript API with an actual runtime feature set:

```typescript
// Proposed API: the runtime options determine which engines are constructed.
const store = createModelStore(ProjectManaged.descriptor, new Project(), {
  supported: [Mechanism.History],
  history: { mode: HistoryMode.Tree }
});
```

A typed facade may restrict methods available to a caller, but the implementation
must still validate runtime options, imported record formats, and permissions.
Conditional TypeScript types alone cannot create or destroy a history engine.
JavaScript callers use the same runtime checks without the static facade.

A Rust binding could offer `ModelStore<Project, LocalHistory, NoCollaboration>`;
`LocalHistory` selects history storage, while a runtime mode chooses linear/tree.
A C binding can accept a generated root descriptor, a validated supported-feature
mask, and an allocation callback table when creating an opaque handle. Both bind
the same revision and entity-version records described in [data structures](data_structures.md).
The exact exported names and ABI belong to future backend API proposals.

## Memory, lifetime, and encoding portability

Use each backend's native ownership and collection tools without changing the
logical records. C++/Rust can keep immutable versions in owned blocks; Java/Kotlin/
C#/Go/Python/JavaScript can use private runtime-owned objects and encoded byte
buffers. Swift can combine value storage and owned reference components. Copying,
freezing, or copy-on-write must actually prevent aliases from mutating a committed
version. A readonly interface alone is insufficient if another mutable alias exists.

Generated entity IDs use the central `uint32` default, with explicit unsigned
range checks in languages without unsigned scalar types. Configuring `uint64`
requires exact representations too: Java/Kotlin may use
bit-preserving integer wrappers and unsigned operations, JavaScript/TypeScript
must use an appropriate exact representation such as `bigint`, and every backend
must reject lossy conversions. Reuse the existing payload codecs' documented
integer conventions, then specify managed-envelope fixtures for cross-language
verification. Native map order is not automatically a canonical wire order.

Do not require generic object allocation hooks in garbage-collected backends.
Offer buffer pools/storage providers where supported, while native backends may
support the [allocator contract](history.md#ownership-and-custom-allocation).
Ordinary payload types retain their existing layout and allocation behavior;
allocator-aware managed storage is an explicit additional representation.

Transaction lifetime is also backend-specific. A Java/C# cleanup method, Go
`defer`, or JavaScript `finally` cannot by itself infer whether an error return
represents success. Use a scope wrapper that captures the result, or require
explicit completion/cancellation. Rust error returns require equivalent treatment;
panic cleanup alone is not enough. Async scopes must await completion and must not
publish an unobserved commit in the background. C requires explicit cleanup paths.
The shared contract is [successful scoped completion or atomic failure](history.md#scoped-completion-and-raii).

## Generation and capability verification

For each root and output backend, generation should produce:

- The existing ordinary payload classes and codecs.
- Managed descriptors and typed root/entity/collection editors for eligible types.
- A declared feature profile and backend capability manifest.
- References to selected runtime components, with unsupported selections rejected.
- Explicit support status for allocator propagation, nested entities, exclusions,
  and supported snapshot/delta encodings.

Compilation or linking should not pull in optional network/effect adapters merely
because a schema contains `managed`. C++ conditional members alone do not guarantee
that a build system omits unrelated linked libraries; dependency selection is also
part of the build configuration. Avoid a generated subclass for every feature
combination unless a measured layout benefit justifies that code-size/build cost.

Future conformance checks must compare behavior and saved-record interpretation,
not class sizes across languages. Verify disabled feature storage, unsupported
operation errors, ordinary-versus-managed construction, atomic grouped edits,
branch retention, ID preservation, and deterministic resource cleanup. The C++
sketches have not been compiled and no other language binding is implemented by
this documentation change.
