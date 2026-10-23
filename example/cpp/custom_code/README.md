# C++: custom code

This example adds application-owned C++ behavior to an ordinary generated value.
The [schema](model.serializer) demonstrates all three native source locations:

- `cpp preamble { ... }` adds standard-library includes before generated declarations.
- `public cpp { ... }` adds a public type alias; `private cpp { ... }` adds an
  overflow-checking helper without introducing storage.
- `readonly cpp { ... }` and `edit cpp { ... }` implement schema-declared methods
  directly in C++.

For example, the schema defines an inline editing method:

```text
public function void add_units(uint32 amount) edit cpp {
  validate_addition(quantity, amount);
  quantity += amount;
};
```

The helper checks addition before mutation. Ordinary `edit` methods have no
automatic transaction or rollback; this method preserves quantity on overflow
because its authored body validates first. The readonly `total_cents()` method
widens a price to `uint64` before multiplication, preserving an exact integer total.

`summary()` has a schema declaration and an application definition in
[behavior.cpp](behavior.cpp). Regeneration replaces only build-directory headers,
so the external implementation stays intact. [main.cpp](main.cpp) qualifies the
external definition, calls each method, checks rejected edits, and exercises
exact-message serialization with all four native protocols. It also rejects
truncated messages and non-whitespace trailing bytes. Default decoder resource
limits remain active for each fresh message.

Only the three schema fields are serialized; native hooks and methods have no
wire IDs. Keep durable and runtime storage in schema fields rather than hiding
it in an opaque C++ hook. [codegen.ini](codegen.ini) selects only C++ and uses
`naming = preserve` because native source refers to the generated identifiers.
Serializer preserves native source; the C++ compiler checks it.

From the repository root, build and run with the maintained wrapper:

```powershell
./make.ps1 test
./out/build/make-Release/example/cpp/custom_code/Release/serializer_custom_code_example.exe
```

On Linux use `make test`, then run
`./out/build/make-Release/example/cpp/custom_code/serializer_custom_code_example`
for a single-configuration build. An installed Serializer CMake package also
allows this folder to be configured as a standalone project. It requires a
C++23 compiler, CMake 4.0+, and clang-format 19+ for the default profile; generated
headers remain in the chosen build directory.

Rohit Serializer extension 1.1.38 highlights these native blocks in VS Code and
Visual Studio using the editor's registered C++ grammar. Other target tags use
their corresponding available language grammars; install a grammar provider if
the editor lacks one. This provides syntax coloring;
IntelliSense and compiler diagnostics inside schema blocks require separate
language-service integration. See [embedded source
highlighting](../../../docs/editors/editor_extension.md#embedded-source-highlighting).

Expected output:

```text
summary: notebook x5
total cents: 1250
overflow: rejected, quantity unchanged
four protocols: exact round trips passed
truncated and trailing input: rejected
```

See the [behavior guide](../../../docs/schema/behavior.md) for external implementations,
language tags and native source ordering, and the [C++ examples](../README.md) for
portable expressions and other maintained consumers.
