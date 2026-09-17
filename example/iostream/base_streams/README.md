# Standard base stream references

[main.cpp](main.cpp) uses `std::ostream&` and `std::istream&` with integer-key binary.

Expose a memory-backed stringstream through standard base-class references, as an application interface might do. Serialization still works directly with the references and requires no explicit wrapper.

Static type recognition cannot select the specialized memory input path through `std::istream&`. This example deliberately exercises generic 8 KiB batching and an owned complete-message input buffer.

The example uses the shared [large schema](../shared/telemetry_archive.serializer)
and deterministic fixture, checks every decoded field, and fails on an exception
or mismatch.

After [configuring the suite](../README.md#build-and-run), build and run this example:

```sh
cmake --build out/build/iostream --target serializer_iostream_base_streams
ctest --test-dir out/build/iostream -R "^serializer_iostream_base_streams$" --output-on-failure
```
