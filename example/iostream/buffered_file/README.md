# Explicit file buffering

[main.cpp](main.cpp) uses `std::filebuf` and `std::iostream` with integer-key binary.

Offer 256 KiB of caller-owned storage through `pubsetbuf()` before opening the file, then attach a bidirectional standard stream. Keep the storage alive until after the file buffer closes.

The standard library decides how it uses `pubsetbuf()` storage; this example does not promise an exact operating-system transfer size. Serializer sees the static `std::iostream` type and uses its generic 8 KiB adapter. The file buffer and serializer scratch buffer are separate layers; ordinary file streams are also buffered.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch. Its exclusively created temporary file is removed when the example exits.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_buffered_file
ctest --test-dir out/build/iostream -R "^serializer_iostream_buffered_file$" --output-on-failure
```
