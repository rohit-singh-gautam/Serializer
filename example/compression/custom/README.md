# Custom backend interface

[main.cpp](main.cpp) implements `compression::backend`, passes a borrowed pointer
through `custom_options` / `decode_options.custom_backend`, and uses the free
`serialize_to` and `deserialize_exact` helpers. The stateless backend remains alive
through both calls and delegates to built-in gzip, producing the same standard
format without defining a new algorithm.

The bounded output sink checks emitted bytes. An external adapter must also
validate a complete stream, enforce the supplied window limit, manage its own
allocations, and document thread safety. This demonstration delegates those
format checks and uses additional owned vectors with fixture-specific limits;
a direct library adapter can emit chunks to the sink instead.

Target: `serializer_compression_custom`. This demonstration requires
`SERIALIZER_WITH_ZLIB=ON`; the custom interface itself does not require zlib.
See [build instructions and shared limits](../README.md) and the
[backend contract](../../../docs/compression.md#backend-options-and-extensibility).
