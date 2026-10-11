# Serializer enhancement checklist

Implementation target: compiler/runtime **1.16.0**, schema language **1.11.0**,
editor extensions **1.1.39**. Existing schema contracts, scalar managed identities,
wire formats and journal/collaboration protocol versions are retained.

## Implemented capabilities

- [x] Lossless numeric representation binding with `checked<T>(value)`, including
  full-width signed/unsigned ranges, exact integer/floating conversion, rejection
  of fractions, non-finite values and lossy floating narrowing. Distinct generated
  records retain explicit conversion boundaries.
- [x] Matching-representation checked integer arithmetic: overflow, zero division,
  signed-minimum/-1 and fractional integer division reject. Mixed representations
  require explicit checked binding; older language headers retain their behavior.
- [x] Fixed-array construction bindings, trailing parameter defaults and generated
  shortened factories; explicit C++ constructors, separately constrained numeric
  input types, and checked scalar/projection bindings. Codec default construction
  remains available. Checked construction/open input templates currently require
  C++; portable fixed-array/default bindings cover ordinary output backends.
- [x] Declared readonly calls on projected generated records and lazy conditionals
  selecting exactly matching owning record values. C supports record selection
  only where its existing storage needs no dynamic ownership; unsupported shapes
  diagnose. Arbitrary external signatures and implicit record conversion remain
  excluded.
- [x] Generic `map<K,V>` and legacy generic-key syntax accept owning C++ record
  keys with compatible comparisons. Unsupported backends and Protobuf diagnose.
- [x] Typed managed registries retain compact internal IDs and expose application
  keys through generated fields. Composite keys express scope; typed descriptors
  distinguish domains. Unique-key rebinding preserves persistent identity.
- [x] Explicit reservation, validated adoption/import, binding, membership/type
  lookup and retirement APIs share normal transaction and no-reuse boundaries.
- [x] Generated snapshot object locators and exact member/base-path descriptors
  avoid persisted pointers, ABI offsets and RTTI names. Mutable editors re-resolve
  after structural changes and expire with their transaction.
- [x] Optional snapshot-owned ordered/hash field indexes map keys to stable IDs.
  Indexes rebuild on request after commit/load/recovery/navigation; old pins retain
  their own generation. Uniqueness invariants validate before publication, including
  raw callback edits. Memory admission and construction costs are documented.
- [x] Typed external resolvers return owning immutable handles; durable binding keys
  remain ordinary fields. Provider synchronization, authorization and versioned
  transactions remain explicit host responsibilities.
- [x] README, integration skill, usage, schema, wire, migration and both editor
  packages describe the new supported syntax and API boundaries.
- [x] Dedicated [custom-ID examples](example/custom_ids/README.md) provide maintained
  consumers for all eleven languages and a C++ managed snapshot/undo companion.

See [numeric/record behavior](docs/schema/behavior.md), [construction](docs/schema/construction.md),
[record map keys](docs/schema/generics.md), and [state registries](docs/managed/state_registry.md).

## Conditional or separate work

- A custom record replacing managed `persistent_id` is **not required by this
  design and is not implemented**. Application-key bindings meet custom addressing
  while retaining existing scalar allocation, recovery and collaboration.
- [x] Research low-memory mostly-disk indexing and existing database support; see
  the [paged indexing study](docs/design/paged_indexing.md), including customer-provider
  boundaries and the journal/history/authorization/collaboration invariants.
- [ ] Implement and qualify a bounded paged backend or database adapter. This
  remains a proposal, with no new dependency, benchmark or memory guarantee.
- [ ] Lazy managed model/history/recovery storage. Paging an index alone cannot
  reduce the eager root, transaction candidates or retained history to page size.
- [ ] Replace internal identity validation/ownership tables through a customer
  backend. A future provider must preserve document/snapshot selection, typed
  lifetime, atomic publication, allocator watermarks and policy/ancestry checks.

## Qualification

Native regression, SDK and editor results are recorded in
[the enhancement verification record](docs/verification/verification-registry-numeric-2026-10-11.md).
Unsupported combinations are separate from completed checks; no all-configuration
performance claim is made.
