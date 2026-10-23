# Reciprocal quantity constructors

Requires compiler/runtime **2.0.0**, schema language **1.14.0**, and C++23.
The schema declares `radius<Rep>` before `diameter<Rep>` and gives both a typed
constructor accepting the other. Callable signatures resolve in a second pass;
the quantity storage stays finite and consists of one `Rep` field.

`public cpp { constexpr } constructor(diameter<Rep> extent);` declares the C++
signature. [definitions.hpp](definitions.hpp) supplies matching visible definitions
after both generated classes are complete. These additions provide no schema body
or portable conversion implementation; other output backends reject external C++
constructor declarations. Keep every constant definition visible where it is used.
Named `radius64` and `diameter64` instantiations have distinct schema identities;
their external definitions are supplied separately with the exact open counterpart
parameter types selected by the schema.

The generated scalar constructor and native readonly `native_scalar()` are
automatically constant-capable. Codec/default construction remains available.
The program checks constant conversions and exact round trips through JSON and
all three native binary key modes. Only the original scalar field is serialized.

Build the `serializer_reciprocal_quantities` target through the repository wrapper
from the repository root:

```powershell
./make.ps1 test
```

To build this directory independently, install Serializer **2.0.0** first. Its
package supplies the compiler and runtime used by `serializer_generate`. Replace
`<Serializer install prefix>` with that installation directory; include any required
dependency installation prefixes in `CMAKE_PREFIX_PATH` as well. From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<Serializer install prefix>"
cmake --build build --config Release --target serializer_reciprocal_quantities
ctest --test-dir build -C Release --output-on-failure
```

These commands select Release for single-configuration and multi-configuration
generators. Generated headers stay in the build directory. Expected output:

```text
radius = 6, diameter = 12
```
