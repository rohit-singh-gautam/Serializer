# Allocation-aware C++ payloads

The [runtime primitives](payload_allocation_runtime.md) describe resource-owning
containers independently of generated record selection and managed stores.

The runtime, generated direct owning records and managed-store integration pass
**15/15 Windows checks** in the recorded MSVC 19.51 C++23 Release profile. Compiler
capability checks pass in the merged 647-case core. [MG07 is complete for this
contract](../verification/verification-managed-completions-2026-10-24.md); additional
platform/profile coverage remains separate. No whole-process memory ceiling is claimed.

The opt-in `cpp.payload_allocator = true` profile selects resource-owning strings,
vectors and ordered maps. Default generation keeps its existing standard types,
public APIs and wire layouts. The profile changes native payload container types
and adds a per-record allocation binding; applications explicitly select it for
all declarations that share generated types.

```ini
[cpp]
payload_allocator = true
```

The profile is intended for concrete unpacked owning records, including ordinary
nested records and direct managed payloads, and native positional/integer-key
binary. Generic declarations, variants, native/portable behavior, borrowed binding
classes, views, XML, Protobuf, separated values and computed/exposure helpers
require separate propagation contracts. Magic headers and class-valued ordered
map keys also reject in this profile; primitive and enum map keys are supported.
JSON and string-key binary require
allocator-aware lexical scratch before they can join the bounded profile.

## Domains and construction

`payload_allocator<T>` retains a shared `std::pmr::memory_resource` owner. Its
`payload_string`, `payload_vector<T>` and `payload_map<K,V>` aliases keep that
ownership through allocator rebinding, node storage and destruction. A null owner
uses the immortal standard new/delete resource. No process PMR default is changed.

`payload_allocation_scope` selects a domain for construction on the current
thread. Nested scopes restore the exact predecessor, including on exceptions.
Container element construction enters its own retained domain so nested strings,
map keys and default-constructed generated values inherit it. Distinct threads
have independent selections; a payload can be destroyed on another thread if its
resource supports the relevant concurrency.

```cpp
auto resource = std::make_shared<rohit::managed::bounded_memory_resource>(limit_bytes);
rohit::serializer::payload_allocation_scope scope{resource};
payload_graph::document draft{};
draft.children.emplace_back();
draft.children.back().label.assign(1024, 'x');
```

The generated `serializer_payload_resource()` accessor borrows the retained
shared owner. Ordinary copies retain their source domain; scoped copies select
the active destination. `clone_payload(source, resource)` performs that complete
clone and checks its resulting allocation domains before returning. Copy/move
assignment retains destination container allocators and the destination record
binding. As with standard stateful allocators, swapping unequal domains requires
an explicit compatible-domain transfer rather than an unchecked container swap.

Moving an independently allocated record into a wrapper can retain its original
domain. `payload_resources_match(value, resource)` checks the complete generated
record/container graph; foreign-domain values must be cloned before publication
under a store's single-domain bound. Constructing optional records directly also
needs a construction scope; wrapper types do not themselves carry allocators.

## Decode and failure

`deserialize_payload_exact<Value, Protocol>(input, resource, limits)` creates a
fresh candidate under the selected scope, verifies exact-message consumption and
checks all payload domains before returning. Decode/admission/allocation failure
destroys the unpublished candidate and releases its outstanding reservations.
Generated decoder entry points enter the existing record's retained domain so
nested temporary values and collection replacements remain in that domain.
Existing in-place decode failure semantics remain distinct from the new
exact-message candidate API.

`bounded_memory_resource` enforces a hard limit on concurrent **requested bytes**
through that resource. Container growth temporarily retains both old and new
allocations, and both requests count. Its `reserved_bytes()` and
`peak_reserved_bytes()` are actual resource-domain counters, independent of
managed resident-admission estimates. Allocation requests reject before an
upstream allocation can exceed the domain limit.

This bound excludes upstream allocator bookkeeping/retention, input/output byte
buffers, diagnostics, external callback allocations and any storage deliberately
using another domain. It is not peak RSS or a process-wide cap. A pooling or
monotonic upstream can retain more physical memory than outstanding requested
bytes. History admission and decoder estimates retain their separate contracts.

## Verification checkpoint

`test/payload_allocator_test.cpp` passes **9/9** runtime cases, and
`test/payload_allocation_generated_test.cpp` passes **6/6** actual generated-model
cases, all with MSVC 19.51 C++23 `/W4 /WX`. They cover nested strings/vector/map
nodes and optionals, owner lifetime, scope restoration, scoped clones, overflow,
hard failed growth, parallel domains, cross-thread destruction, real binary
codec temporaries, exact cleanup, moved-from allocator sentinel ownership,
foreign-domain baseline rebinding, complete callback scoping, runtime fields and
Undo/Redo. The generated fixture is emitted by the real CMake pipeline.

An initial generated run found moved-from map allocators losing their retained
resource owner; explicit allocator moves now preserve that ownership. A tracking
probe verified matching allocation/deallocation routing before the passing full
run. Full log: `out/build/managed-contracts-windows-passing.log`.
Generated decoder entry points also scope public keyed member dispatch; the
profile rejects magic/header declarations explicitly. The sixth generated case checks that
native-database identified preparation, acceptance and recovery keep the selected
store domain under a foreign outer allocation scope. The transient/runtime case
also checks `clone_value()` under that foreign scope. These additions pass in the
current **15/15** Windows generated/runtime target. Linux and installed-consumer
checks remain separate gates.
