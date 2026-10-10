# Managed examples

New document-tool examples: [inherited data, variants, versions and generics](document_features/README.md)
and [runtime state, migration, notifications and snapshot deltas](runtime_state/README.md).
See the [state enhancement guide](../../docs/state_enhancements.md) for concrete schemas and APIs.

The separate [multilanguage wire matrix](multilanguage/README.md) exchanges runtime
records across language codecs. It is preparation for native managed ports; the
feature demonstrations below still use the C++ runtime.

Start with the point examples in the order below. Each folder is a separate executable
and introduces one small idea. The only application model is a point with x
and y. Every example generates its point class and editor from a local
`point.serializer` schema and uses the actual managed runtime.

| Order | Example | What to read for |
| --- | --- | --- |
| 1 | [Read and update a point](plain_point/README.md) | Generate a point, read x and y, then update them in a transaction. |
| 2 | [Callback](callback/README.md) | Pass a named function to transaction.update and see its candidate argument. |
| 3 | [Generated editor](access/README.md) | Use the generated getters and setters on a candidate point. |
| 4 | [Managed edit](managed_edit/README.md) | The actual generated point, editor, channel, and transaction. |
| 5 | [Manual commit](manual_commit/README.md) | Candidate edits versus the committed point. |
| 6 | [Scope-exit commit](scope_commit/README.md) | Destruction, automatic completion, and checking the outcome. |
| 7 | [History](history/README.md) | Undo and redo of both coordinates as one action. |
| 8 | [Revert](revert/README.md) | Cancel an unpublished edit. |
| 9 | [Optional labels](labeled_history/README.md) | Enable transaction names at compile time and display undo/redo labels. |

Labels are disabled by default. The first eight examples and ledger use unnamed
transactions; only `labeled_history` opts into storing names.

Each README includes expected output. Start with `plain_point` for the smallest
managed program. The `callback` example exposes the real low-level candidate
callback; `access` and `managed_edit` show the preferred generated editor API.

For the implementation behind these examples, see
[how managed transactions work](../../docs/internals/managed_transactions.md).
It explains the nested classes and callbacks using the same point model.

## Build the point examples

From the repository root, in a C++ developer environment with CMake and
clang-format available:

```sh
cmake -S . -B out/managed-points -DSERIALIZER_BUILD_MANAGED_EXAMPLES=ON -DSERIALIZER_BUILD_TESTS=OFF
cmake --build out/managed-points --config Release --target managed_point_examples
```

The aggregate target builds all nine examples. To build just one, use its
target name, such as `--target managed_point_edit`. Each folder's README lists
its target. All nine require schema generation and `Serializer::managed`.

Executables are under `out/managed-points/example/managed/<example-folder>/`.
Multi-config generators add a `Release/` subdirectory; Windows adds `.exe`.
For example, with Visual Studio:

```powershell
.\out\managed-points\example\managed\managed_edit\Release\managed_point_edit.exe
```

With a single-config generator, run
`out/managed-points/example/managed/managed_edit/managed_point_edit`
(add `.exe` on Windows). For a Release single-config build, also configure
`-DCMAKE_BUILD_TYPE=Release`.

When using an existing repository build with `SERIALIZER_BUILD_TESTS=ON`
(and GoogleTest available), all nine are registered with CTest:

```sh
cmake --build out/build/managed-ninja --config Release --target managed_point_examples
ctest --test-dir out/build/managed-ninja -C Release -R "^managed_point_" --output-on-failure
```

Replace that build directory with your configured directory. Each executable
returns a nonzero exit code if its final coordinate/status checks fail.

## Larger examples

Each model has its own folder:

| Example | Contents |
| --- | --- |
| [Journal](journal/README.md) | Runnable appended/sidecar lifecycle, no-change writes, recovery, undo/redo, and full Save. |
| [Collaboration](collaboration/README.md) | Generated-editor proposals, application-owned session types, server/client roles, read-only observation, retries, presence, and subtree leases. |
| [Ledger](ledger/README.md) | Runnable C++ program, schema, CMake target, and transaction/save-load walkthrough. |
| [Design](design/README.md) | Hollow-cylinder schema used by the managed integration tests. |
| [Wordpad](wordpad/README.md) | Paragraph-array schema used by the managed integration tests. |

The ledger target remains `managed_example`. Design and wordpad are schema
examples exercised by tests, rather than separate executable programs.
