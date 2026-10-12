# Typed arithmetic operators

Requires compiler/runtime **1.19.0**, schema language **1.14.0**, and C++20.
This example keeps distance and duration as distinct owning records. Operators
forward to exact readonly methods; the schema declares each permitted operation.
Both unary forms construct the owning result explicitly. Binary inline mappings
declare their right operand as `binary(+, distance<Rep> rhs)`.

Generated C++ operators are inline and become constexpr when their shared method,
constructor, and storage permit it. Checked arithmetic keeps its existing overflow,
zero-division, nonfinite, and fractional integer-division failures. The generated
record codecs and field identities are unchanged. Other output languages retain
the readonly methods and construction behavior; operator syntax is generated for
C++ only.

The root test build generates and runs the `serializer_arithmetic_operators`
target. To build this directory independently, install Serializer **1.19.0**
first. Its package supplies the compiler and runtime used by `serializer_generate`.
Replace `<Serializer install prefix>` with that installation directory; include
any required dependency installation prefixes in `CMAKE_PREFIX_PATH` as well.
From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<Serializer install prefix>"
cmake --build build --config Release --target serializer_arithmetic_operators
ctest --test-dir build -C Release --output-on-failure
```

These commands select Release for single-configuration and multi-configuration
generators. Generated headers stay in the build directory.
See the [behavior guide](../../../docs/schema/behavior.md).
