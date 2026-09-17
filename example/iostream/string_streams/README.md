# Separate memory streams

[main.cpp](main.cpp) uses `std::ostringstream` and `std::istringstream` with integer-key binary.

Encode into an output-only memory stream, move its string into an input-only memory stream, and decode. The input adapter borrows the unread string storage rather than copying the encoded message into another staging buffer.

Moving `str()` transfers the message between owners; it is distinct from the adapter's borrowed input path. Keep the input stream's storage unchanged while decoding.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_string_streams
ctest --test-dir out/build/iostream -R "^serializer_iostream_string_streams$" --output-on-failure
```
