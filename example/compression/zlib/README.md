# zlib stream

[main.cpp](main.cpp) uses generated static APIs with `zlib_options{.level = 6}`
and explicit `format::zlib`. It transfers the encoded bytes from
`std::ostringstream` into an independently owned `std::istringstream`. The zlib
wrapper is distinct from gzip and raw DEFLATE.

Target: `serializer_compression_zlib`. Enable `SERIALIZER_WITH_ZLIB=ON` with zlib
available. See [build instructions and shared limits](../README.md).
