# LZ4 frame

[main.cpp](main.cpp) uses generated static `document::serialize` and
`document::deserialize` calls with `lz4_options{.level = 0, .checksum = true}` and
explicit `format::lz4`. The output is a standard checksummed LZ4 frame, not a raw
LZ4 block or the legacy frame format.

Target: `serializer_compression_lz4`. Enable `SERIALIZER_WITH_LZ4=ON` with an LZ4
CMake package available. See [build instructions and shared limits](../README.md).
