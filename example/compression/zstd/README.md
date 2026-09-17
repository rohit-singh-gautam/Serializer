# Zstandard frame

[main.cpp](main.cpp) writes one standard Zstandard frame using
`zstd_options{.level = 3, .checksum = true}` and generated member output. It selects
`format::zstd` explicitly and uses `deserialize_exact` to return a fresh document
only after validating the frame and complete inner message.

Target: `serializer_compression_zstd`. Enable `SERIALIZER_WITH_ZSTD=ON` with a
Zstandard CMake package available. See [build instructions and shared limits](../README.md).
