# Resource-owning payload containers

`<rohit/payload_allocator.hpp>` provides `payload_string`, `payload_vector<T>`
and `payload_map<K,V>` in `rohit::serializer`. Their standard allocator retains
shared ownership of a selected `std::pmr::memory_resource`, including rebound map
nodes and implementation storage in moved-from containers.

Use a thread-local `payload_allocation_scope{resource}` when constructing nested
values. Nested construction and explicit scoped copies use that domain; ordinary
copies retain their source ownership. Assignment keeps the destination allocator.
No process PMR default is changed. A resource must support every thread that
allocates or ultimately destroys its values.

`<rohit/payload_decode.hpp>` adds `clone_payload(value, resource)` and
`deserialize_payload_exact<Value, Protocol>(input, resource, limits)`. These
construct fresh candidates, check complete domain propagation and reject before
returning an inconsistent payload. Native binary strings/vector/map decoders
recognize the selected allocator. Codec boundaries and existing default standard
container behavior remain unchanged.

The existing `bounded_memory_resource` enforces concurrent requested bytes in
this domain, including overlapping growth requests. Its current/high-water
counters exclude upstream allocator metadata/retention, input/output buffers,
other domains and process RSS. Shared ownership keeps the domain alive until its
last payload allocation is released; it does not make a non-thread-safe upstream
resource safe for concurrent use.

Nine runtime cases pass on Windows MSVC 19.51 C++23 Release, covering nested
allocation domains, clone ownership, map decoder reuse, move/sentinel cleanup,
hard failed growth, overflow, thread selection and cross-thread destruction.
The independent `payload_allocator_test` target retains these checks. Generated
record selection and managed-store integration require their own explicit
generator contracts; these runtime checks establish the container/decoder
primitives only.
