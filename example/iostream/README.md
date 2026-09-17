# Iostream examples

Seven independently runnable C++20 examples demonstrate direct serialization with
standard memory streams, file streams, base stream references, and a custom
non-seekable stream buffer. Each stream category has its own folder, source, and
README. The shared model is generated once through `serializer_generate`.

All examples use the generated static API for both directions:

```cpp
archive::serialize<codec::binary_integer>(stream, original);
const auto decoded = archive::deserialize<codec::binary_integer>(stream, message_limits());
```

The input factory creates the destination internally, accepts explicit resource
limits, and uses the same implicit stream adaptation as the member API.
Omit the second argument to use default limits. Existing member
`serialize_out` and `serialize_in` calls remain available.

| Folder | Standard stream types | Protocol | Serializer adaptation |
| --- | --- | --- | --- |
| [stringstream](stringstream/README.md) | `std::stringstream` | JSON | Borrow unread memory for input; generic buffered output. |
| [string_streams](string_streams/README.md) | `std::ostringstream`, `std::istringstream` | Integer-key binary | Move the encoded string between streams; borrow memory for input. |
| [file_streams](file_streams/README.md) | `std::ofstream`, `std::ifstream` | Integer-key binary | Concrete file types select 64 KiB batches. |
| [fstream](fstream/README.md) | `std::fstream` | Integer-key binary | Flush and reposition one file handle; 64 KiB batches. |
| [buffered_file](buffered_file/README.md) | `std::filebuf`, `std::iostream` | Integer-key binary | Offer a 256 KiB file buffer; use the generic 8 KiB serializer adapter. |
| [base_streams](base_streams/README.md) | `std::ostream&`, `std::istream&` | Integer-key binary | Erased static types select generic 8 KiB batching and owned input staging. |
| [custom_streambuf](custom_streambuf/README.md) | Custom `std::streambuf` wrapped by `std::ostream`/`std::istream` | Integer-key binary | Non-seekable input exposed in 257-byte chunks, with generic adaptation. |

Stream selection uses the **static C++ type**, not runtime buffer inspection.
Ordinary file streams already have standard-library buffering. An explicitly
supplied file buffer is a separate layer from the serializer's scratch storage;
`pubsetbuf()` behavior depends on the standard library.

## Large schema and payload

[shared/telemetry_archive.serializer](shared/telemetry_archive.serializer) contains
52 classes, four enums, and 645 explicitly numbered fields across 864 lines.
It models device inventory, services, sites, metrics, logs, traces, alerts,
deployments, storage, dashboards, and archive metadata. It includes nested objects,
arrays, maps, enums, and a numeric union, all under `stable_ids`.

[shared/example_support.cpp](shared/example_support.cpp) constructs 24 devices,
each with three 1,024-sample metric series, plus inventory and 128 long log
messages. Every example checks that its encoded message exceeds 256 KiB, so both
the 8 KiB and 64 KiB paths have substantial data to transfer. It compares native
binary encodings of the original and decoded objects to check every serialized
field, including nested/default-valued data. The schema's size and the runtime
payload's size are intentional, separate parts of this demonstration.

The executable prints the verified encoded byte count and returns zero on
success. Exceptions, undersized fixtures, or mismatches produce a nonzero exit.
File examples create their own temporary directories and remove their files
after the streams close; they require no input file and do not replace user files.
These examples demonstrate correctness and usage, not measured performance.

Verification: all seven examples passed on Windows x64 with MSVC and on Linux
with Clang 21. The Windows Debug build passed all 24 CTest targets; the Linux
standalone build used `SERIALIZER_BUILD_TESTS=OFF` and needed no GoogleTest.
The fixture produced 1,799,682 binary bytes and 2,470,757 JSON bytes on Windows.

## Build and run

Run from the repository root with CMake 3.28+, a C++20 compiler, and
[clang-format 19+](../../docs/cmake_integration.md#formatter-setup).
On Windows, use an x64 Visual Studio developer shell.

```sh
cmake -S . -B out/build/iostream -DSERIALIZER_BUILD_TESTS=OFF -DSERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON
cmake --build out/build/iostream --target serializer_iostream_examples --parallel 2
ctest --test-dir out/build/iostream -L serializer_iostream --output-on-failure
```

This standalone option needs no GoogleTest. The standard
`SERIALIZER_BUILD_TESTS=ON` build also includes all seven examples. For a
multi-configuration generator, pass `--config Debug` when building and
`-C Debug` to CTest. Executables reside below the build directory's
`example/iostream/`, with a configuration subdirectory when applicable.
Each folder's README lists its individual build target and CTest name.

Generated headers stay in the build tree. Build
`serializer_iostream_models_serializer_headers` to generate the shared model
without compiling its consumers.

## Message boundaries and ownership

Each input contains **one message extending to EOF**. The examples use explicit
limits: 16 MiB of encoded input, 1 MiB per string, 1,048,576 entries per collection,
nesting depth 32, 32 MiB of cumulative decoded allocation accounting, and
67,108,864 parsing work units. Byte streams are not incrementally decoded. Generic/file
input is fully staged; concrete standard memory input borrows existing storage.

Keep borrowed storage unchanged until decoding finishes. The adapters do not
close or explicitly flush caller-owned streams. The examples flush, close, or
reposition where their transport requires it. Normal EOF can set EOF/fail state;
clear it before reusing a stream. I/O or decoding failures can consume input,
write an output prefix, or partially modify the destination.

For applications with multiple messages on a long-lived connection, provide an
input bounded to one frame or an exact-size buffer. No framing is added here.
See the [stream concepts and adapter contract](../../docs/usage.md#stream-concepts-and-implicit-adapters)
for the full requirements and customization points.
