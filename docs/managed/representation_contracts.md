# Managed C++ representation contracts

The native managed store operates on generated unpacked owning records. Schema
eligibility, occurrence ownership, codec selection and editor capability are
separate decisions. This matrix records the supported boundary; an unsupported
representation must fail before output publication.

| Selection | Contract |
| --- | --- |
| Direct identity, `uint32` or `uint64` | The generated owning record carries native `persistent_id`; tracked roots and explicit managed descendants receive document-scoped identities. |
| Separate values, either identity width | Original schema records remain ordinary values; generated storage wrappers own identity and managed child storage. |
| Fixed or compact-varint native identity encoding | Changes the selected identity codec, preserving numeric IDs; select one representation consistently across matched compiler/runtime and persisted bindings. |
| Ordinary owned child | The nearest managed owner records the value; an eligible child type does not promote an unmarked occurrence into an entity. |
| Explicit managed scalar/array/map/owning-variant child | Generated editors preserve the declared identity boundaries and reject unsupported alternative/container combinations. |
| Distinct public base occurrences | Each generated base accessor resolves through the live candidate; ordinary base fields retain stable field/base paths. Explicit managed bases have their own identity boundary. |
| Packed or view record, including a reachable ordinary child | Rejected. A view's borrowed storage or packed native layout cannot substitute for managed owning storage. |
| Recursive ownership or repeated-ancestor diamond | Rejected. Cyclic logical links use checked typed managed references; they do not make recursive owning editors available. |
| Private/protected or unsupported virtual base layout | Rejected. Private schema fields themselves remain supported through generated friends. |
| C++ Protobuf with direct managed storage | Rejected before identity lowering. Direct native metadata uses field ID `1073741823`, beyond Protobuf's legal field-number domain; no silent remapping occurs. |
| C++ Protobuf with separate managed storage | Preserves the existing wrapper mapping: identity field `1`, value field `2`. Normal Protobuf schema restrictions apply; this does not select Protobuf as the managed Save/journal format. |
| Non-C++ output | Managed declarations reject by default. Explicit Python/Java token profiles provide wire records only; their separate native engines have narrower owning/qualification contracts. None inherits this C++ representation matrix automatically. |

The early rejection applies to direct storage, both identity widths, both native
identity encodings, nested namespace declarations, inferred managed roots and
managed bases. Separate-value and ordinary supported Protobuf records retain their
existing mapping. Selecting native JSON/binary output for the same supported
managed schema remains valid.

The [compiler matrix regression](../../test/managed_schema_test.cpp) checks this
early rejection against native-output controls. The [C++ runtime guide](cpp_runtime.md)
defines ownership, lifetime, mutation and navigation behavior. Executed platform
results must be recorded separately; a diagnostic contract does not implement
packed/view storage, recursive ownership or a managed Protobuf format.
