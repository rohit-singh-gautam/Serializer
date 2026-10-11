# C++: expressions

The [schema](model.serializer) declares numeric expressions and checked predicates
on ordinary generated values. The [consumer](main.cpp) calls the same methods in
`static_assert` and at runtime. Generation uses the default C++ profile; no
`constant_evaluation` option is required for this behavior.

With schema language `1.10.0`, the `expression_math` namespace declares the
readonly free function `total`. `subtotal` calls that qualified function, which
multiplies a double by an explicitly converted `uint32` count. `adjusted` calls
the generated `subtotal()` member, then combines subtraction and addition. `unit_ratio`
demonstrates checked division. Integer equality retains the complete `uint64`
identity instead of converting it to double. Predicates demonstrate a bounded
array reduction, checked indexing and arithmetic validation.

An evaluated invalid `expression(...)` throws `std::domain_error` at runtime and
cannot produce a constant expression. Invalid inline arithmetic or indexing in a
`predicate(...)` returns false, including division by zero, nonfinite arithmetic
and an out-of-range index.
Checked expression methods retain their throwing exception specification;
eligible predicates and exact integer comparisons are `noexcept`. These methods
return meaningful results and are generated with `[[nodiscard]]`.

Build and run the example with the ordinary repository test command:

```powershell
./make.ps1 test
./out/build/make-Release/example/cpp/expressions/Release/serializer_expression_example.exe
```

On Linux use `make test`, then run the corresponding executable beneath
`out/build/make-Release/example/cpp/expressions`.

With an installed Serializer CMake package, this directory can also be configured
as a standalone project. It requires a C++20 compiler and the normal generation
prerequisites, including clang-format for the default profile. Generated output
stays in the selected build directory.

Expected output:

```text
subtotal: 54
adjusted: 52
invalid expressions: rejected
invalid predicates: false
```

See the [behavior guide](../../../docs/schema/behavior.md) for expression syntax,
validation rules, evaluation limits and runtime-only C++20 math functions, and
the [C++ examples](../README.md) for other maintained consumers.

The [readonly-call SDK check](../../../test/readonly_function_languages.py)
checks namespace and member calls in each selected language, including forward
declarations, full-width scalar returns and Java static namespace methods. Calls
invoke the declared generated function directly; a reached C++ callee exception
propagates through its caller, including a predicate caller. The predicate
examples above use inline checked arithmetic and retain their false-on-failure
behavior.

The SDK check also compiles readonly function overloads on C++, Java, C#, Swift,
and Kotlin, and checks explicit rejection for backends that require unique free
function names. Java overloads must retain distinct parameter types after native
type erasure; for example, schema `int32` and `uint32` both become Java `int` and
cannot distinguish two functions with the same name.
