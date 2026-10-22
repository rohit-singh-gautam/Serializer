# Native namespace companion graphs — 22 October 2026

Compiler/runtime **2.28.0**, schema language **1.21.0**. C/Rust XML document
companions now carry plain namespace-valued fields through wrapped scalar and
record collections, nil scalars/records/collections, mixed and ordered typed
children, reordered all groups and lifecycle metadata. Native model fields,
visibility and wire codecs retain their contract. This advances `OPC-R01`;
additional constraints and graph combinations still require implementation, so
that parent remains open. Subsequent
[foreign-content graph work](verification-native-extension-graphs-2026-10-22.md)
adds wrapped/nil/lifecycle preservation and variant dispatch; it has distinct
verification counts and does not change this original 23-decision inventory.

The generator describes wrapper ownership with static nested metadata. Namespace
discovery follows typed mixed/ordered child payloads. Input filtering defers
known-child order to the owning generated reader when there are no positioned
foreign slots, allowing both all-group permutations and heterogeneous fragment
order. Revision metadata stays in the known transport; nil markers suppress
QName-text validation only for absent values. The ordinary typed reader still
validates nil spelling, content and presence policy. Foreign extension slots
retain their existing positioning/capability gates; namespace state is not an
authorization to accept unknown core names.

`native_xml_namespace_graph_test.py` passes **23 decisions per actual SDK** under
Rust warnings denial and MSVC C11 warnings-as-errors. Twelve valid documents and
eleven malformed/unbound controls cover nine owning graph families. Qualified
Unicode attributes, child-local aliases, shadowed prefixes and explicit default
absence retain independently observed expanded identities. Sequence observations
preserve occurrence order; all-group observations allow legitimate canonical
reordering.

Consumers free/drop the original XML input before output, then compare all four
native protocol encodings after companion replay. C verifies rejected input leaves
the initialized destination value and sidecar pointer unchanged. Each retained
scope graph passes exact logical preservation admission, rejects one byte below
that amount and recovers on a fresh ordinary policy. A nil collection with no
namespace-valued occurrences correctly needs zero retained namespace bytes; its
zero-byte policy succeeds rather than manufacturing a one-tight negative bound.

Existing suites also pass on both actual SDKs: **13 namespace decisions**, including
independent retained-scope accounting, and **29 extension/XSD/position/admission
decisions**, owned and recursive companions, exact resource policies and native
controls. The final release graph log is `out/opc-2-28-namespace.log`; regression logs are
`out/opc-namespace-regression.log` and `out/opc-extension-regression-final.log`.
The extension runner uses the already populated ordinary Cargo cache; an initial
empty task-local cache could not fetch the registry in the sandbox and is not
counted as a pass. Initial harness-only C argument-count and zero-byte assertion
errors were corrected before these final runs.

The maintained harness removes its temporary SDK directory on success or failure;
useful existing regression consumers and final logs remain under ignored `out/`.
These are Windows SDK checks, with the existing pinned XML providers. They do not
claim namespace-sensitive outer-union/default/fixed/enumeration support in C/Rust,
foreign preservation in mixed/all/nil/wrapped/lifecycle graphs, map/variant
companions, or readonly namespace projection. Those require further implementation
and qualification under the remaining parent requirements. Later enumeration,
foreign-content and projection changes must be assessed using their own evidence.

The final 2.28.0 root build and **283/283** default CTests pass; executable/source
provenance and package/version checks are recorded with the
[ordered-XSD release verification](verification-ordered-xsd-2026-10-22.md#release-verification).
