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
| [XML and packages](xml_packages.md) | Generated XML mappings, fixed/optional runtime XSD, C++ compression streams, ZIP/OPC and complete file Saves; backend boundaries and pending work. |
| [Protobuf representations](protobuf.md) | C++ Protobuf binary, ProtoJSON, and TextProto support and compatibility boundaries. |
| [Constant evaluation](constant_evaluation.md) | Implemented C++ positional codecs at compile time, generator opt-in, public APIs, and qualified model shapes. |
| [C++ performance features](performance.md) | Implemented compiler and codec optimizations, regeneration needs, and measurement limits. |
| [Runtime SIMD](runtime_simd.md) | SIMD protocol coverage, build configuration, linking, and scalar fallback behavior. |
