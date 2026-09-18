# Cross-endian compatibility verification - 2026-09-18

This change makes C++ binary strings require UTF-8, matching the other generated
language runtimes, and strengthens positional interoperability qualification.
The existing fixed-width numeric codecs already convert between native host
order and configured wire order; this work found no scalar byte-order mismatch.

## Wire checks

- The shared positional fixture is specified independently in
  `test/positional_binary_fixture.json`. Both the five-runtime CMake matrix and
  full example runner compare every positional producer against it.
- The full runner can cross-compile its existing C/C++ consumers for s390x and
  execute them through QEMU. Compile-time assertions require native big-endian
  representations. These programs select the same little-endian wire profile
  as the existing native consumers.
- The expanded matrix passed **2,028 exchanges**: 13 producers x 13 consumers x
  four protocols x three union alternatives. **507** of these exchanges exercise
  positional binary. The participants are ten native runtimes, TypeScript using
  the JS runtime, and two s390x C/C++ programs.
- The fixture covers integer widths and extrema, 64-bit JS BigInt values, floats
  and negative zero, booleans, Unicode strings, nested objects, inheritance,
  arrays, maps, enum contexts, and union payloads. Frozen positional message
  sizes are 319, 315, and 312 bytes.

## Environment and regression checks

Native qualification used the existing Windows x64 Release build and installed
SDKs. Rust, Swift, and sanitized C ran in WSL. Big-endian consumers used GCC
15.2.0 targeting s390x, static linkage, and QEMU s390x 10.2.1 under WSL Ubuntu.
The s390x C++ runtime used the scalar fallback with SIMD disabled.

The targeted CTest run passed all eight entries: the C++ core suite (**213
cases**), two-way Java interoperability, JS/Go/C# codec tests, strict TypeScript
declarations, the five-runtime matrix, and browser interoperability.

New C++ cases cover malformed UTF-8 in every binary key mode and both explicit
wire byte orders, ASCII/SIMD boundary positions, embedded NULs and Unicode
boundaries, unchanged destinations on rejection, view mapping and failed view
mutation, batched dynamic field names, and arbitrary byte-array payloads.
Validation is allocation-free; constant generated binary names are validated at
compile time. The initial change reused the bounded JSON ASCII scanner. A
subsequent [UTF-8 follow-up](verification-utf8-2026-09-18.md) records measurements,
dedicated SIMD validation, and an explicit trusted-data opt-out. The matrix above
describes the initial cross-endian run; the follow-up lists its additional checks.

This is an emulated big-endian execution check for C/C++ owning consumers, not a
claim that every runtime, platform, or minimum SDK has been executed on
big-endian hardware. Optional fields and big-endian wire selection for non-C++
backends remain outside this change. UTF-8 tightening and raw-byte migration are
documented in [migration](../migration.md#binary-string-validation).

Reproduction commands and package requirements are in
[the interoperability guide](../example/interoperability/README.md#different-machine-byte-orders).
