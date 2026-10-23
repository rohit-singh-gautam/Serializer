# Readonly XML namespace graphs and depth bounds — 22 October 2026

Compiler/runtime **2.28.0**, schema language **1.21.0**. This qualification extends
the [25,000-value stress checkpoint](verification-readonly-xml-stress-2026-10-21.md)
with namespace-sensitive scoped translators in all eleven actual Windows outputs
and independent exact/one-tight traversal-depth bounds.

## Implemented contract

Scoped translator output reads its original native getters throughout one
synchronous traversal. Namespace presentation remains XML metadata. It does not
materialize an owning target, clone nested records, copy collection payloads, or
change native field identities or the four native encodings. Existing borrowed,
shared and weak reference lifetime rules, unbound/expired failures, export-only
input barriers, and fresh target defaults remain in force.

- C++ provides `view.xml_bind_namespaces(bindings, optional_child_scopes)` for
  direct token and tree output. It borrows caller-owned namespace maps and
  occurrence histories through const pointers; they must outlive the traversal.
  Rvalue map binding is deleted. The view retains its constexpr native getter
  behavior and contains no owning namespace map. Direct export succeeds with a
  zero retained-tree allowance.
- Kotlin and Swift derive target scalar namespace histories from the assigned
  source paths, including renamed target fields and nested source records. Each
  repeated scalar retains its original occurrence vocabulary. Kotlin borrows the
  companion lists; Swift shares their value-semantic storage. Conflicting owner
  prefix bindings fail before publishing output.
- Python and JavaScript/TypeScript bind view-local presentation through
  `xml_bind_namespace` and `xmlBindNamespace`. Java and C# use
  `setXmlNamespaces` and `SetXmlNamespaces`. Go now supports
  `view.SetXMLNamespaces`, with its metadata map stored on the synchronous view.
  These APIs validate/copy namespace metadata while lending native payloads.
- C and Rust accept translator targets with namespace/extension companions.
  C exposes `view_encode_xml_document(view, extensions, ...)`; Rust exposes
  `view.encode_xml_with_extensions(&extensions, ...)`. Both borrow a sidecar
  shaped for the target XML contract. They do not construct an owning document
  around the view or enable XML input into it. The sidecar and native source must
  remain valid for the call; the caller must not shallow-copy an owning C model.
- Native borrowed/shared/weak selected record handles delegate namespace and
  foreign-payload metadata to their selected payload. Direct handle export lends
  that payload's companion; an enclosing reference graph uses its own XML root
  and field paths. `prepare_xml_extensions` in Rust and the generated
  `record_prepare_xml_extensions` function in C validate bounded input metadata
  into a sidecar without decoding or binding any native value. Actual output
  still comes from the live caller-bound handles. Preparation and export preserve
  the previous C result on failure; ordinary XML decode into reference graphs
  remains export-only. A companion never extends a weak handle's lifetime.

Namespace bindings do not grant foreign vocabulary permissions. QName identity,
foreign preservation slots and semantic vocabulary understanding retain their
separate contracts. A caller supplying a C/Rust companion for a reordered or
renamed projection must supply metadata for the target contract's field paths.

## Executed checks

| Actual output | Executed coverage |
| --- | --- |
| C++ | **7/7 GTests**; borrowed QName getter, rvalue-map rejection at compile time, exact expanded identity through token/tree output, zero token-tree allowance, byte failures and recovery; 25,000 original collection values and exact depth 2 / rejection at 1 |
| Python | **8/8 maintained tests**; QName scoped target constructor tripwire, string identity, four native protocols, exact/one-tight bytes, independent lxml QName observation; 25,000 values with exact depth 3 / rejection at 2 |
| JavaScript/TypeScript | **8/8 maintained tests**, including a strict actual TypeScript consumer; scoped QName export, native controls, independent lxml namespace inspection; 25,000 values with exact depth 3 / rejection at 2 |
| Java/C#/Go | All **3 SDK consumers** pass; scoped QName text and attribute namespace identities, target constructor tripwires, native controls, exact/one-tight byte limits and fresh recovery; 25,000 values with exact depth 3 / rejection at 2 |
| Kotlin/Swift | Both actual SDK consumers pass; renamed/nested scalar and repeated QName projection preserves three distinct namespace identities, original collection storage, native controls and byte recovery; 25,000 values with exact depth 3 / rejection at 2 |
| C/Rust | Both actual SDK consumers pass; original descriptor/string pointers, borrowed QName text/attribute and opaque foreign attribute companion replay; direct borrowed/shared/weak payload export and enclosing reference graph metadata preparation; C exactly-once owner release and Rust conflicting mutable borrow rejection; weak expiry, export-only input rejection, exact/one-tight bytes, unchanged prior C output on failure, four native protocols and independent lxml identities; 25,000 values with exact depth 2 / rejection at 1 |

The existing cooperative callback interruption, prepared-provider completion,
source mutation and owner-expiration checks remain part of these maintained
consumers. A failed traversal leaves the source reusable. These are public
resource-policy checks; they do not establish hard process memory ceilings or
asynchronous preemption.

Logs are retained under ignored `out/`: `opc-r10-cpp.log`,
`opc-r10-python.log`, `opc-r10-javascript.log`, `opc-r10-portable.log`,
`opc-r10-mobile.log`, `opc-r10-swift.log` and `opc-r10-native.log`.
Generated native/mobile/portable SDK consumers remain under
`out/build/opc-completion/r10-*`; Python/JavaScript temporary generated consumers
are removed by their maintained temporary-directory scopes. These output paths
are verification artifacts, not source deliverables.

Initial probe failures are not counted as passes: a C++ test first requested
depth 2 as a failure even though its nested payload stores scalars as attributes;
the corrected exact/one-tight boundary is 2/1. Swift's first attempt was denied
permission to launch its installed frontend; the permitted replay passed. A
portable C# replay initially exposed a scalar-runtime helper spelling error; the
corrected generated runtime compiled with zero warnings and passed.

## Durable field addresses

Durable addresses have a separate explicit policy: **XML generation rejects
them**. XML value export selects payload data; it cannot encode process-local
handles or infer a document/object/generation/path address representation.
Implementing such a representation requires its own future wire contract and
resolver/lifetime semantics. Native durable-address formats remain unchanged.

The maintained Python harness successfully generates the native durable-address
control for **each of eleven outputs**, then requests XML generation for that same
schema. All eleven requests reject with an address-contract diagnostic and retain
the previous output file byte-for-byte. This verifies generation and atomic
publication policy; it is not a claim that a durable-address XML codec exists.

See the [value-reference guide](../schema/value_references.md),
[durable address contract](../schema/references.md), and
[pending requirements](../project/completed_requirements.md#opc-and-3mf-requirements).
