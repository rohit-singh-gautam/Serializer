# Managed C++ custom IDs

This companion uses ordinary generated composite `book_id` keys while Serializer
retains compact document-local `persistent_id` values. `Scope` stores a customer
namespace; the empty `book_tag` supplies a compile-time domain through the generic
key type. A key's local number is independent of its current array position.

[model.serializer](model.serializer) defines the durable fields.
[model.ini](model.ini) opts the scope/key records into generated comparisons.
[main.cpp](main.cpp) demonstrates:

1. Declare scoped key uniqueness and resolve a custom key to an internal ID.
2. Pin the selected read/index, then rebind the application key without changing
   the persistent ID. The old pin continues to read its original snapshot.
3. Undo/redo the binding and reject a duplicate key without partial publication.
4. Adopt a value under fresh reserved scalar identity, retire its owned occurrence
   without reducing the allocator watermark, and undo retirement.

The example is built and registered with the native test suite when managed
support and tests are enabled:

```powershell
./make.ps1 test
ctest --test-dir out/build/make-Release -C Release -R custom_ids_managed_example --output-on-failure
```

```sh
make test
ctest --test-dir out/build/make-Release -C Release -R custom_ids_managed_example --output-on-failure
```

Use your configured build directory when it differs. CMake runs schema generation;
no generated header is checked into the source tree. The verified program prints
a `PASS` message and fails if an expected state-management guarantee is violated.

The [portable examples](../README.md) show application-owned key/binding policy
in every language. Their host registries do not supply Serializer's C++ managed
history or journal. See [state registries](../../../docs/managed/state_registry.md) for
typed field paths, hash/ordered indexes, provider lifetimes, memory admission,
and the authority boundary used by collaboration.
