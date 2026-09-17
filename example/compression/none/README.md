# Uncompressed message

[main.cpp](main.cpp) passes `none_options` to generated member APIs, checks that
the result exactly matches ordinary uncompressed serialization, then decodes with
an explicit `format::none` and compares every field.

Target: `serializer_compression_none`. No optional compression dependency is
needed. See [build instructions and shared limits](../README.md).
