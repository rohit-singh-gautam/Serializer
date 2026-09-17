# Raw DEFLATE stream

[main.cpp](main.cpp) first serializes the document, then calls standalone
`compression::compress` with `deflate_options{.level = 6}`. It decompresses the
owned bytes with explicit `format::deflate` and performs exact inner decoding.
The expanded vector stays alive throughout decoding.

Raw DEFLATE has no wrapper or checksum and cannot detect every corrupt change.
The sender and receiver must agree on the format and message extent.

Target: `serializer_compression_deflate`. Enable `SERIALIZER_WITH_ZLIB=ON` with
zlib available. See [build instructions and shared limits](../README.md).
