# gzip member

[main.cpp](main.cpp) uses generated member APIs and `std::stringstream` with
`gzip_options{.level = 6}` and explicit `format::gzip`. It resets the read position
after writing and validates every decoded field. The memory stream contains one
gzip member; concatenated members are outside the supported message profile.

Target: `serializer_compression_gzip`. Enable `SERIALIZER_WITH_ZLIB=ON` with zlib
available. Standard streams still stage the whole message within the configured
budgets. See [build instructions and shared limits](../README.md).
