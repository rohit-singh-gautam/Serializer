# Journal and recovery example

This runnable C++ example uses the generated [ledger schema](../ledger/ledger.serializer)
and exercises both appended and sidecar storage. Each run creates a fresh child
folder and leaves its files available for inspection. It never overwrites an
existing document.

1. Create a saved base with `create_journal(path, mode)`.
2. Commit an edit. Its snapshot is flushed to the journal automatically.
3. Execute an unchanged transaction and verify that its sequence does not advance.
4. Destroy the store without calling `save_journal()`.
5. Reopen with `recover_journal(path)` and verify the journaled edit is restored.
6. Undo and redo; both navigation operations are journaled automatically.
7. Call `save_journal()` for a full checkpoint, then reopen and verify retained undo.

The close/reopen demonstration uses the same replay path needed after a process
crash. It does not itself terminate abruptly; the separate subprocess crash tests
exercise interruptions during writes, synchronization, publication, and tail repair.

## Build and run

From the repository root in a C++ developer environment:

```sh
cmake -S . -B out/journal-example -DSERIALIZER_BUILD_MANAGED_EXAMPLES=ON -DSERIALIZER_BUILD_TESTS=OFF
cmake --build out/journal-example --config Release --target managed_journal_example
```

Run `out/journal-example/example/managed/journal/managed_journal_example` with an
optional output-directory argument. Windows adds `.exe`; multi-config generators
place it under `Release/`:

```powershell
.\out\journal-example\example\managed\journal\Release\managed_journal_example.exe .\out\journal-demo
```

With an existing managed test build:

```sh
cmake --build out/build/managed-ninja --target managed_journal_example
ctest --test-dir out/build/managed-ninja -R "^managed_journal_example$" --output-on-failure
```

The program prints one verification line for each mode and finishes with
`Journal examples passed; files retained in ...`. A failed expectation returns a
nonzero exit code.

## Files and Save behavior

- `appended.srj` contains its saved base plus subsequent records.
- `sidecar.srj` contains its saved base; `sidecar.srj.journal-<high>-<low>` contains
  subsequent records. Always retain the matching companion when moving the document.
- Each document has a stable `.lock` file. Releasing the store releases the lock;
  leaving this file on disk is intentional.

Every changed commit writes only the new snapshot and compact metadata. Calling
`save_journal()` is a full checkpoint: it writes a complete envelope including
retained undo/redo history, replaces the base, and rotates the sidecar. Destruction
does not perform this checkpoint. An unchanged transaction writes no snapshot;
object-ID reservations can still be durable independently of canceled edits.

`stream.hpp` supplies memory streams. The file adapter uses the reusable
[`file_stream`](../../../include/rohit/file_stream.hpp) for transport and explicit
durability; the shared frame codec also accepts supported iostreams. See
[file stream usage](../../../docs/usage.md#file-streams-and-journal-records) for
ordinary generated serialization to a file, and the
[journal guide](../../../docs/managed/journal.md) for limits, uncertainty handling,
and crash-recovery guarantees. No database sink is implemented.
