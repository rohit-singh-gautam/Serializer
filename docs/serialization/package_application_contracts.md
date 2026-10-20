# Generated package profiles and application references

[Package APIs](xml_packages.md) · [Pending requirements](../pending.md)

Compiler/runtime **2.17.0** adds schema language **1.21.0** package policies and
scalar application selectors. Existing 1.20 declarations retain their defaults;
ordinary native serialization and managed storage layouts do not change.

```text
serializer version 1.21.0;
namespace catalog {
  xml(namespace("urn:catalog"), scalars(attribute));
  class resource { public uint32 id; public string label; }
  class selection { public uint32 resource_id; }
  package archive {
    part resources : resource {
      name(dynamic); content_type("application/vnd.example.resource+xml");
      relationship("urn:catalog:resources"); occurs(0, unbounded);
      compression(deflate); policy(defaults); key("resources", id);
    }
    part selections : selection {
      name(dynamic); content_type("application/vnd.example.selection+xml");
      relationship("urn:catalog:selections"); occurs(0, unbounded);
      reference("resources", resource_id, resources);
    }
  }
}
```

`policy(defaults)` permits `stored` or `deflate` ZIP storage overrides. It is
the default for older declarations. `policy(fixed)` requires the declared storage
on input and output; an identical override is allowed. Unknown roles, unsupported
methods and conflicting fixed overrides fail before final-file replacement.
Storage selection cannot replace the declared native model, XML root, expected
`xsi:type`, content type or container. Expanded root/type identity is checked
before native model decoding. This does not add a general compression pipeline.

Generated `key(scope, field)` and `reference(scope, field, target_role)` checks run
automatically. A key scope covers the complete unpublished aggregate. Duplicate
keys fail even across different model types. Forward and repeated references are
allowed, but missing or wrong-model targets fail. Two roles using the same native
model can share its keys. The target role must declare that key scope with the
exact same scalar type. Multiple distinct key scopes and references are allowed.
Every scope uses one exact scalar type across its declaring roles.

Selectors currently name direct public persistent `string`, `int8`–`int64` or
`uint8`–`uint64` fields. Optional, collection, nested-path, transient, XML-omitted,
version/magic, enum, floating-point and managed-token selectors reject explicitly.
Empty scopes, duplicate key scopes within a role, unresolved roles and incompatible
field types also reject. Selector metadata adds no payload fields or native IDs.
Integer keys retain their declared width; string keys remain exact and case-sensitive.

Indexing spends the same cumulative reference, metadata and work budget as
ZIP/OPC/XML. Failed work is not refunded. Domain callbacks run after automatic
reference validation. Save validates a complete staged candidate before replacing
the destination. Borrowed stream writes may retain a prefix on failure; use
complete-file Save for atomic final-file publication. Logical admission is distinct
from process peaks, and cooperative cancellation cannot preempt provider internals.

C++ inputs borrow a `package::profile_overrides` through `input.profiles`; keep
it alive until synchronous Save/write returns. `archive::select_profiles()` returns
owning resolved metadata. Python uses `Input(..., profiles={'resources': 'stored'})`
and `Archive.select_profiles(overrides)`. C#/Kotlin inputs expose `ProfileOverrides`
and `profileOverrides`, with `SelectProfiles`/`selectProfiles` facade methods.
Other backend overloads are documented in their package guides. Override maps use
schema role names.

C/Rust package roles reject XML graphs requiring document-companion namespace or
extension state, and XSD identity constraints requiring native operation hooks.
Their ordinary XML document/identity APIs remain available. Declarative package
`key`/`reference` selectors use the shared package ledger independently of XSD
identity constraints. Unsupported combinations fail during generation.

These additions do not close broader XML graph, official-format conformance,
provider/platform, hard-allocation, signature or format-specific requirements.
Verification records distinguish executed SDK/owning-model profiles from those gates.
