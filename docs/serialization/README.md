# Serialization contracts and C++ runtime

[Documentation index](../README.md)

Read the wire contract before selecting representations or optional runtime
features. The guides below document implemented behavior with their support and
verification limits.

| Guide | Read it for |
| --- | --- |
| [Wire format and decoding](wire_format.md) | Native protocol layouts, byte order, bounded decoding, and failure contracts. |
| [Owning objects and binary views](views.md) | Mapping, reading, updating, and traversing binary views safely, including ownership and lifetime rules. |
| [Message compression](compression.md) | Optional C++ compression backends, complete-message framing, and bounded exact decoding. |
| [XML and packages](xml_packages.md) | Generated XML mappings, fixed/optional runtime XSD, C++ compression streams, C++/Python OPC profiles and complete file Saves; backend boundaries and pending work. |
| [Normalized XML events](xml_events.md) | Provider-independent C++ borrowed events plus incremental UTF-8 LibXml2 input, namespace handling, cumulative bounds and explicit completion. |
| [Shared XML content contracts](xml_shared_contracts.md) | Checked presence/nil, mixed-content, map, variant and extension IR/configuration; reference validation, public raw preservation helpers and pending generated-codec boundaries. |
| [Advanced shared XML contracts](xml_advanced_contracts.md) | Portable scalar/regex and identity validation, content particles, advanced model/profile metadata, restricted HTML and the eleven-language fixture manifest. |
| [Fixed XSD bundle tools](xsd_tooling.md) | Pinned local imports/includes, supported nested bindings, XSD export and directional compatibility diagnostics. |
| [Protobuf representations](protobuf.md) | C++ Protobuf binary, ProtoJSON, and TextProto support and compatibility boundaries. |
| [Constant evaluation](constant_evaluation.md) | Implemented C++ positional codecs at compile time, generator opt-in, public APIs, and qualified model shapes. |
| [C++ performance features](performance.md) | Implemented compiler and codec optimizations, regeneration needs, and measurement limits. |
| [Runtime SIMD](runtime_simd.md) | SIMD protocol coverage, build configuration, linking, and scalar fallback behavior. |

For OPC application/framework ownership, a restricted 3MF schema and compilation
workflow, and the remaining requirements, see the
[OPC/3MF authoring design](../design/opc_3mf_support.md). It distinguishes
implemented package declarations from remaining profile extensions.
