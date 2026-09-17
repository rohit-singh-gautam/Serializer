# Separate file streams

[main.cpp](main.cpp) uses `std::ofstream` and `std::ifstream` with integer-key binary.

Write one binary message, close the output file explicitly, reopen it for reading, and verify the decoded archive. Both concrete file types select the serializer's 64 KiB transfer policy.

Enable stream exceptions and open both handles in binary mode. Closing output before opening input makes the entire message visible and reports delayed output failures. The input adapter accepts normal EOF even with fail exceptions enabled.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch. Its exclusively created temporary file is removed when the example exits.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_file_streams
ctest --test-dir out/build/iostream -R "^serializer_iostream_file_streams$" --output-on-failure
```
