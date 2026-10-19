# Inline value references

Compiler/runtime **2.6.0**, language **1.19.0**, adds nonowning export views over
existing payload storage. The application keeps that storage alive; its owner
needs no knowledge of the consumer. Output remains an ordinary field value.
See the [maintained reference examples](../../example/references/README.md).

## Basic borrowed view

```text
serializer version 1.19.0;
class payload {
  public array float values (1);
  public array uint32 codes (2);
}
class owner { public payload b (1); }
class consumer { public borrowed(payload) d (1); }
class owning_consumer { public payload d (1); }
```

In C++, assign the existing payload lvalue:

```cpp
owner a;
a.b.values = {4, 2, 3};
a.b.codes = {0, 1, 2};
consumer c;
c.d = a.b;

rohit::full_stream_auto_alloc output;
c.serialize_out<rohit::serializer::json>(output);
```

`a` remains an ordinary owner. `c.d` stores exactly a `const payload*` and reads
`a.b` without copying its arrays or allocating another payload. Keep `a.b` alive
at a stable address until the last access. Temporary payload assignment rejects.
`c.d.bind(a.b)` is the equivalent explicit binding for this direct type form.

The bytes match `owning_consumer` with the same value:

```json
{"d":{"values":[4,2,3],"codes":[0,1,2]}}
```

JSON and the three native binary protocols add no reference tag, wrapper object,
document ID, owner ID, generation, path or presence marker. The containing field's
normal name/ID and the payload's ordinary codec define the output.

`with_value(callback)` gives C++ a synchronous const borrow. `read_value()` is an
explicit owning copy; use scoped access for large payloads. Default construction
and `unbind()` leave an unbound handle. Reads and output require a live binding;
missing bindings fail instead of substituting a default or null. The view stores
no replacement payload or source defaults.

Complete edits before reading or encoding, including separate count/write passes.
The API adds no synchronization. Moving or destroying borrowed storage invalidates
the view; the application controls its lifetime.

## Nonowning access across languages

| Output | Basic lifetime model |
| --- | --- |
| C++ | Const payload pointer; the application keeps the referent alive at a stable address. |
| C | Raw const payload pointer; the application keeps the referent alive at a stable address. |
| Rust | `Option<&'a Payload>` carries the compiler-checked borrow through containing records; escaping the source lifetime rejects. |
| Java, Kotlin/JVM, C#, Python, JavaScript/TypeScript | Nonowning weak reference; retain the source elsewhere for the operation. |
| Swift | Weak owner box preserves identity for generated value-type records; retain that box for the operation. |
| Go | Explicit reference scope; close it before discarding the source. |

Native value/reference parameter rules and reclamation timing differ. TypeScript
uses the JavaScript runtime. Review the recorded qualification for the selected
backend rather than assuming identical memory representation in every language.
Rust native borrows currently support plain data records. Authored behavior or
callable parameters/results involving containing borrowed records reject with a
diagnostic; shared/weak modes use their separate owning handle API.

## Optional shared or weak ownership

Use these modes only when the application wants runtime ownership management.
An independently allocated payload can be shared by ordinary containing records:

```text
class shared_owner { public shared(payload) b (1); }
class shared_consumer { public shared(payload) d (1); }
```

```cpp
auto m = std::make_shared<payload>();
shared_owner a;
a.b.bind(m);
shared_consumer c;
c.d.bind(std::move(m));
```

Each field stores the supplied `std::shared_ptr<const payload>` directly.
Moving `m` transfers a handle without moving or copying the payload. Releasing
`a.b` leaves `c.d` alive. Neither containing object retains the other or needs
shared ownership.

`weak(payload)` stores the corresponding weak handle and pins it during
synchronous access. Missing or expired bindings fail. Shared/weak wrappers have
their native smart-pointer representation without another payload object.

Managed runtimes use strong references for shared mode and weak references for
weak mode. Rust uses its qualified owning handle API, Swift uses owner boxes,
and C/Go expose their corresponding explicit ownership/scope facilities.

## Optional owner-field selectors

A consumer may also declare `borrowed(owner.b)` and bind the ordinary owner.
This selects its current field without changing the owner's schema or lifetime:

```text
class field_consumer { public borrowed(owner.b) d (1); }
```

In C++, `field_consumer c; c.d.bind(a);` or `c.d = a.b;` borrows the selected
storage. The application still keeps it alive.

The separate `shared(owner.b)` form uses an aliasing shared pointer and retains
the complete owner because the payload is inline in that object. Choose the
independent `shared(payload)` form when the payload has its own allocation.
Public field paths and public bases need no stable source IDs because their
paths are not serialized. Collection-element and reference-to-reference paths reject.

## Translate during one operation

A scoped translator can borrow an existing ordinary payload without constructing
a complete owning output record:

```text
class projection_source {
  public payload b (1);
  public translator owning_consumer as as_consumer { d = b; };
}
```

```cpp
source.with_as_consumer([&](const auto& projected) {
  projected.template serialize_out<rohit::serializer::json>(output);
});
```

Its getters and traversal borrow `source.b`, including its arrays. Encode inside
the callback and let only output bytes escape. No complete owning target or copied
payload arrays are needed. Keep the scope synchronous and do not retain borrowed
results. Reference fields already encode directly and need no translator.

Language 1.19 permits ordinary record payloads containing collections. Existing
owning conversions and language 1.18 translators retain their original subsets.
Target defaults and individual encoding operations can still allocate.

## Read with an ordinary owning schema

These reference wrappers are export-only. Reading a reference field rejects
input; it neither allocates an owned replacement nor writes through the binding.
A keyed containing record can accept input that omits this field under its usual
missing-field rules, leaving the reference unbound. Read exported values with
`owning_consumer`. Output does not reconstruct a memory binding. Repeated
reference fields each write their value like ordinary fields.

## Supported shapes

Direct `borrowed(payload)`, `shared(payload)` and `weak(payload)` forms require
a previously declared concrete ordinary record type. Named concrete generic aliases
are supported; open generic definitions and scalar/container type operands reject.
Records may contain arrays/maps under the borrowed-record subset.

Owner-field forms traverse public ordinary fields and public bases, ending in a
supported scalar/string/enum or ordinary record. Direct legacy `array`, `map` and
`variant` terminal fields reject; wrap collections in an ordinary payload record
or use a supported owning type application. Private paths, optional/variant
traversal, managed storage, packed/view owners and nested reference payloads remain
unsupported. Do not add storage defaults, compact/auto-size policies or array/getter
exposure to the reference field. Optional XML and C++ emission-only, constant-
evaluation and Protocol Buffers profiles reject this runtime feature.

The older [named durable address references](references.md) remain a separate
language 1.18 feature for applications that need serialized identity and resolution.
Both editors navigate inline reference types and optional owner-field paths;
anonymous generated wrappers have no separate authored schema declaration.
See [verification records](../verification/README.md) for recorded checks.
