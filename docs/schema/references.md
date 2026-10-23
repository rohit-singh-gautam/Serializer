# Durable field addresses and projected translators

For value-only export, use language 1.19
[inline value references](value_references.md). They bind a runtime owner with one
argument and serialize the selected value exactly like an ordinary owning field.
The named declarations below intentionally serialize durable addresses and remain
available for applications that need that separate contract.

Compiler/runtime **2.5.0**, schema language **1.18.0**. These additions preserve
existing schemas, generated names and wire formats. Select the new language in
each file declaring a reference or translator, including included files.
Run the [durable address examples](../../example/address_references/README.md) for a shared
`owner.b` reference, live edits, explicit resolution and a nested scoped translator
that writes `{"label":"updated","count":2}` in all eleven output languages.

## Declare a field reference

```text
serializer version 1.18.0;

class owner stable_ids {
  public int32 b (7);
}

reference borrowed_b = borrowed(owner.b);
reference shared_b = shared(owner.b);
reference weak_b = weak(owner.b);

class consumer stable_ids {
  public shared_b d (1);
}
```

A named reference is a generated address record with runtime binding methods.
It refers to the **field slot**: reads follow the current `owner.b`, including
after assignment or replacement of a nested ordinary value. Each owner must be
a previously declared concrete ordinary owning record. Paths may traverse public
ordinary records and public bases; every traversed member/base needs an explicit
stable ID. Terminal values support scalars, enums, strings and supported ordinary
owning records. Collection elements, optional/variant traversal, private fields,
transient fields, packed/view records, managed owners and references to references
are rejected. Ordinary reference bindings also reject use inside managed storage
or editors. Stable field IDs identify schema slots, not runtime objects.

For generic storage, declare a named concrete `instantiate` alias first and use
that alias as the owner. Direct generic type applications and generic definitions
are unsupported as reference owners; the ordinary owning/public/ID rules still apply.

## Bind and resolve in C++

```cpp
auto value = std::make_shared<owner>();
value->b = 10;
consumer observer;
observer.d.bind(value, "document-1", 42, 1);
value->b = 20;
assert(observer.d.read_value() == 20);
observer.d.with_value([](const auto& selected) {
  assert(selected == 20);
});
```

`bind(owner, document_id, owner_id, generation)` prepares the complete binding
before assigning the canonical address. `resolve` instead checks an existing
decoded address against the supplied identity before attaching the owner.
The application owns document IDs, nonzero object IDs, generation allocation,
lookup and deletion policy. It must verify that the supplied owner actually has
that identity; a pointer is not proof of an application object's identity.
`unbind()` releases runtime state while retaining its serialized address.
`is_bound()` checks runtime availability and unchanged address metadata.

Borrowed C++ bindings accept an lvalue owner and retain no ownership. The owner
must outlive every access and stay at a stable address; temporary owners reject.
Shared bindings use `std::shared_ptr<const Owner>` and retain the whole owner,
including its inline member. Weak bindings use `std::weak_ptr<const Owner>` and
pin the owner only for each access. Neither binding adds synchronization.
Sequential owner edits are visible; the selected storage must remain valid and
unchanged during a read/callback. `read_value()` returns an owning value copy;
`with_value` borrows synchronously and requires a void callback that does not
retain the field's address. Arbitrary concurrent mutation remains unsupported.
When decoding an enclosing model, the caller must retain its destination owner
for the entire operation; a temporary pin inside one field decoder does not pin
the rest of an enclosing object graph after that field decoder returns.

To share a child's lifetime independently of its parent, the child needs its own
storage owner and object identity. An inline-field reference retains its parent;
it does not turn an inline member into a separately allocated object.

## Language mappings

| Output | Owner binding |
| --- | --- |
| C++ | Lvalue borrow, shared owner, or weak owner; synchronous const field callback. |
| Java, Kotlin/JVM, C#, Python, JavaScript/TypeScript | Strong owner for shared; weak owner for borrowed/weak. |
| Rust | `Rc<RefCell<Owner>>`; shared retains an `Rc`, other modes retain `Weak`; callbacks hold a checked immutable borrow. |
| Swift | `SrlReferenceOwner<Owner>` box preserves identity for generated value-type records; edit the box's value. |
| Go | Shared owner pointer; borrowed/weak use the generated reference scope, closed explicitly before discarding the owner. |
| C | A typed scope factory such as `shared_b_create_scope(owner, release)` creates the owner control block; checked retain/release operations manage its lifetime. |

Use the backend's generated method spellings: `readValue`/`ReadValue`/`read_value`,
`withValue`/`WithValue`/`with_value`, and binding methods. Native return shapes
follow the language's error and value model; use `with_value` for synchronous
access. Native C/C++/Rust can borrow the selected storage; value-type languages
retain their own parameter/value rules. The API itself supplies no mutation
operation; reference-based languages can still mutate compound values through
other aliases or returned native record handles.
Garbage-collection and reference-counting runtimes have different reclamation
timing. These APIs do not promise identical destructor timing or unrestricted
shared mutation. Reference-counted backends need weak edges or explicit cycle
breaking for strong ownership cycles; tracing garbage collectors can collect
unreachable cycles. TypeScript uses the JavaScript runtime. Borrowed/weak JavaScript modes
require native `WeakRef` (ES2021 or newer; qualified with Node.js 22); ordinary
codecs retain their existing ES2020 contract, and shared mode needs no `WeakRef`.
TypeScript declarations retain the existing ES2020 compilation target.

## Serialized address contract

Reference records encode these ordinary fields in JSON and all three native
binary protocols:

| ID | Name | Type |
| --- | --- | --- |
| 1 | `document_id` | `string` |
| 2 | `owner_id` | `uint64` |
| 3 | `generation` | `uint64` |
| 4 | `field_path` | `array uint32` |

All-empty metadata denotes null. A populated address requires a nonempty document,
nonzero owner/generation, and the declaration's exact stable field/base path.
Runtime bindings and pointer addresses never enter the payload. Loading produces
an unbound address; explicitly resolve it after the containing document has
constructed its owners. Multiple references resolved to the same owner share that
owner and observe the same field. Address edits invalidate an existing binding.

These are typed address codecs and binding APIs, not automatic arbitrary graph
traversal or an object-table document format. The application serializes owners
once and resolves edges after loading. Cross-document lookup, collection element
identity and cycle ownership remain explicit application decisions. C++ managed
registries provide separate entity/snapshot/editor contracts; these ordinary
references do not extend native managed engines to other outputs.

## Translate during one synchronous operation

```text
serializer version 1.18.0;
class target {
  public string d (1);
}
class source {
  public string b (1);
  public conversion target as to_target { d = b; };
  public translator target as target_projection { d = b; };
}
```

The existing conversion creates an independent owning result. A translator
instead exposes readonly target-field getters over the borrowed source, including
target defaults for unmapped fields. Its output traversal writes the target
schema directly, without constructing a complete owning target. It uses the
target's field keys, ordering, defaults, exclusions and native framing.

Use the generated scoped callback to consume a projection synchronously. C++ also
provides lvalue-only views whose source lifetime and stable address remain the
caller's responsibility. Rust views carry a compiler-checked source lifetime.
Keep a native view alive while borrowing its unmapped defaults. Do not retain
a scoped projection or its borrowed getter results after the operation. Swift's
value-type model requires the scoped form to avoid copying its source.
Keep the source unchanged throughout traversal and between separate count/write
passes. Callback/codec failures follow the existing backend failure contract;
output already written to a caller-owned stream cannot be rolled back implicitly.

A reference to a source record can compose both operations in one scope. In C++,
given a bound reference `selected_source` and an output stream `output`:

```cpp
selected_source.with_value([&](const auto& source) {
  source.with_target_projection([&](const auto& projected) {
    projected.template serialize_out<rohit::serializer::json>(output);
  });
});
```

The outer scope keeps the reference owner available; the inner scope writes the
target projection directly. Neither operation creates a complete owning target.

Translator mappings retain the conversion subset: exact matching types, public
dotted source paths and ordinary owning target shapes. Translator declarations
require `public` access; private/protected entry points reject. They do not insert numeric
narrowing, reverse setters, arbitrary user code, caching or delayed ownership.
Targets with non-defaulted authored constructors callable without arguments,
field default factories or construction hooks reject when their effects cannot be reproduced
by independent field defaults. Readonly getters do not create a deep immutable
proxy over native shared record handles in reference-based languages.
Preparing default fields and encoding leaf values can allocate; avoiding the
complete intermediate target is not a universal allocation-free or zero-copy guarantee.

## Capability selection and verification

Used: existing portable address-field codecs, stable member/base IDs, public
projection validation, explicit independent conversions, native output traversal
and the real CMake generation pipeline. Native managed registries remain a
separate supported facility; automatic graph persistence and mutable managed
reference editors are not part of this ordinary-record API.

Reference declarations reject C++ constant-evaluation/emission-only profiles;
their live bindings and validation require runtime state.
Translators also reject C++ constant-evaluation, emission-only and Protocol Buffers
output profiles, whose traversal contracts are separate from native runtime codecs.
Named durable address codecs reject optional XML generation in every backend;
there is no durable-address XML wire representation. Inline value-reference and
scoped-translator XML output supports the selected readonly graphs in all eleven
outputs. It emits payload values, retains source lifetime requirements and rejects
input into an export-only graph. See the [value-reference guide](value_references.md)
and [XML token qualification](../verification/verification-xml-tokens-2026-10-19.md).
Broader graph, resource and stress qualification remains tracked by
[OPC-R10](../project/completed_requirements.md#opc-and-3mf-requirements). Existing borrowed
`span`/`reference_wrapper` output and mapped binary views retain their independent
contracts. See the [native, SDK and editor qualification report](../verification/verification-references-translators-2026-10-18.md)
for executed checks and remaining verification boundaries.

See [conversions](conversions.md), [schema reference](schema_reference.md),
[usage](../guides/usage.md), [wire format](../serialization/wire_format.md), and
[managed registries](../managed/state_registry.md).
