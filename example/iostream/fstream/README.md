# Bidirectional file stream

[main.cpp](main.cpp) uses `std::fstream` with integer-key binary.

Write and read through the same file handle. Explicitly flush and seek to the beginning before changing direction. The concrete file type selects 64 KiB serializer batches.

Input represents one complete EOF-delimited message. The example clears the resulting EOF/fail state before closing; the same reset is needed before another seek/read cycle.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch. Its exclusively created temporary file is removed when the example exits.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_fstream
ctest --test-dir out/build/iostream -R "^serializer_iostream_fstream$" --output-on-failure
```
