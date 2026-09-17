# Bidirectional memory stream

[main.cpp](main.cpp) uses `std::stringstream` with JSON.

Encode to one stream, rewind its read position, and decode directly. The serializer borrows the stream's unread `view()` for input, so the stream and its storage stay alive throughout decoding. Output uses the generic 8 KiB adapter.

The get and put positions are independent. A successful read consumes through EOF; call `clear()` before repositioning if you want to reuse the stream.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_stringstream
ctest --test-dir out/build/iostream -R "^serializer_iostream_stringstream$" --output-on-failure
```
