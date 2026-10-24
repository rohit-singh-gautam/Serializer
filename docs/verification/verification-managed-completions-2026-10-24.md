# Managed allocation and representation acceptance: 24 October 2026

This record closes **MG07** and **MG08** against their retained acceptance
wording in [pending requirements](../pending.md#managed-state-and-storage).
It records implemented native C++ contracts and executed profiles. It does not
claim every format, representation, operating system or future proposal.

## MG07: allocator-aware payloads and hard allocation hooks

Implemented contracts:

- Resource-owning strings, vectors and maps retain shared allocator lifetime,
  including nested values, rebound map nodes and moved-from storage.
- Exact cloning/decoding and the opt-in generated direct owning binary profile
  propagate the selected resource through payload construction, decoder
  temporaries, callbacks and history restoration. Foreign graphs require an
  explicit clone before publication.
- `bounded_memory_resource` rejects concurrent requested bytes above its hard
  domain limit, including overlapping container growth. Current and peak
  reservation counters expose that domain's accounting. Resident admission,
  routed allocation requests, upstream overhead and process RSS are documented
  as separate quantities; a whole-process ceiling is not promised.
- Unsupported formats, representations and graph metadata reject explicitly;
  existing default container APIs and native wire bytes remain unchanged.

Evidence: **9 runtime plus 6 actual generated Windows cases pass**, including
allocation failure/rollback, ownership transfer, decode temporaries, foreign
scopes, native preparation and Undo/Redo. The final runtime replay passes after
the compiler/backend merge. The nine kernel cases also pass with Linux GCC 15.2.
The compiler profile admission tests pass in the **647-case merged core suite**.
See [payload usage](../managed/payload_allocation.md), the
[kernel contract](../managed/payload_allocation_runtime.md) and the
[combined checkpoint](verification-managed-state-2026-10-24.md).

Generated Linux and installed-consumer replay are additional qualification work;
this completion does not report those checks as executed. JSON/string-key binary,
separated values and other excluded combinations retain their documented gates.

## MG08: supported representation and editor boundaries

The [representation matrix](../managed/representation_contracts.md) defines
owning direct/separate storage, both identity widths and encodings, ordinary and
managed child boundaries, private fields and distinct public base editors.
Packed/views, reachable unsupported storage, recursive ownership, ambiguous
diamonds and unsupported nonpublic/virtual base layouts reject before emission.

Direct managed Protobuf rejects its out-of-domain native identity field before
lowering. Separate-value Protobuf preserves the existing identity/value wrapper
and exact wire bytes. All four generated uint32/uint64 fixed/compact profiles
pass **8 runtime cases** after regeneration with the merged compiler. Native
positive/negative compiler controls pass in the **647-case core suite**, and the
current managed direct executable passes **154 cases with one existing
sidecar-only case inapplicable**.

The requirement asks for a defined, qualified support/rejection contract. It
does not require implementing the representations that this matrix explicitly
rejects. The recorded Windows profile satisfies that boundary; broader Linux
and installed coverage remains separately identified, not claimed complete.
See the [detailed boundary evidence](verification-managed-representations-2026-10-24.md).

## Final replay and remaining parents

The final focused replay passes **5/5 CTest targets**, **17 GTest cases**, in
**1.66 seconds**: allocator runtime nine and managed Protobuf eight. Logs and
JSON: `out/build/managed-completion-audit`. Earlier merged native controls pass
22/22 targets in 39.77 seconds; their logs remain under
`out/build/compiler-merge-native`.

The two managed sections retain **eight unfinished parents**. MR03 still needs
other-output/reference-selection work; MIG03 needs broader native schema/journal
migration; MG01 still requires six further native engines; MG09 retains its
broader method contracts. MG10 remains open because compact byte blocks and
notification policies do not implement the proposed normalized entity-version
history records. PG01/PG02/DB02 retain their explicitly authorized database
deferrals. No unfinished implementation is classified as will not fix here.
