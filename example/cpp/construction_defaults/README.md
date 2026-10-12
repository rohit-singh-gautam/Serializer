# Owning construction and defaults

This consumer requires compiler/runtime **1.19.0**, schema language **1.14.0**,
and C++20. It initializes constructor-bound C++
members directly and expresses independent nested/fixed-array defaults in the schema.
The vector's neutral value is `(0, 0, 1)`, while explicitly supplied coordinates
survive construction and serialization unchanged.

A scalar field retains its ordinary default wrapper, such as `T z {1}`. A nested
record supplies a positional list inside that wrapper, such as
`vector3<T> normal {{0, 0, 1}}`. Fixed-array lists contain exactly the declared
number of elements; nested record lists contain every ordinary stored field in
declaration order. Constructor parameter lists use `= {0, 0, 1}` directly.

Open C++ scalar types retain their existing constraints. An explicit generic
literal default requires `T{literal}` to be valid when the host instantiates the
template. No factory constraint is added. Concrete portable specializations
validate each scalar literal against the selected type before generation.

Default construction and decoding continue to use schema defaults. Authoring
constructors do not run during default construction, copying, or decoding. Existing
schemas using older language versions retain their established constructor lowering.

Build and run the `serializer_construction_defaults_cpp` target through the root
build wrappers. See [construction](../../../docs/schema/construction.md).

To build this directory independently, install Serializer **1.19.0** first. Its
package supplies the compiler and runtime used by `serializer_generate`. Replace
`<Serializer install prefix>` with that installation directory; include any required
dependency installation prefixes in `CMAKE_PREFIX_PATH` as well. From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<Serializer install prefix>"
cmake --build build --config Release --target serializer_construction_defaults_cpp
ctest --test-dir build -C Release --output-on-failure
```

These commands select Release for single-configuration and multi-configuration
generators. Generated headers stay in the build directory.
