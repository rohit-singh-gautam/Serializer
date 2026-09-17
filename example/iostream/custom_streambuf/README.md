# Custom non-seekable stream buffer

[main.cpp](main.cpp) uses `std::streambuf`, `std::ostream`, and `std::istream` with integer-key binary.

Wrap the small [chunked_streambuf.hpp](chunked_streambuf.hpp) transport in ordinary standard streams. It stores output and exposes input in 257-byte chunks, with no seek support. The default streambuf bulk reader combines these chunks to satisfy `istream::read()`.

Finish all writes before reading and keep the buffer alive longer than its streams. Input ends at the stored message's EOF. This is a deterministic transport demonstration, not an asynchronous network implementation; the serializer still stages the whole bounded input before decoding.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_custom_streambuf
ctest --test-dir out/build/iostream -R "^serializer_iostream_custom_streambuf$" --output-on-failure
```
