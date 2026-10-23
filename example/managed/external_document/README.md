# External JSON document with native recovery

This runnable C++23 example supplies bounded owning JSON callbacks through
`make_journal_document_codec`. The ordinary document contains the current saved
model. Its generated `persistent_id` fields are zero placeholders; authoritative
IDs, allocation marks, history and session metadata belong to native companions.

The lesson edits a ledger, burns a canceled allocation, saves at an undone cursor,
reopens the exact native state, redoes the original child identity and verifies
that later creation cannot reuse the burned gap. It creates a unique private
temporary directory and removes its document/companions on exit.

It also runs both application storage choices: a full owned main for a temporary
source, and differences attached directly to a durable original. The owned-copy
case removes the external source and shared capture before native recovery.
Both retain bounded opaque origin metadata, prepare an external Save, publish
through separate host code, independently reread publication evidence, confirm
the exact token, retry harmlessly, cold reopen and Undo.

From a configured repository build with managed examples enabled:

```powershell
./make.ps1 configure -BuildDirectory out/external-document -CMakeArgs @('-DSERIALIZER_BUILD_MANAGED_EXAMPLES=ON')
cmake --build out/external-document --config Release --target managed_external_document_example
ctest --test-dir out/external-document -C Release -R '^managed_external_document_example$' --output-on-failure
```

On Linux, configure through the root wrapper:

```sh
make configure BUILD_DIR=out/external-document CMAKE_ARGS=-DSERIALIZER_BUILD_MANAGED_EXAMPLES=ON
cmake --build out/external-document --target managed_external_document_example
ctest --test-dir out/external-document -R '^managed_external_document_example$' --output-on-failure
```

Expected output:

```text
JSON Save/reopen preserved native IDs, redo and burned allocation gaps.
Application-selected owned copies/durable originals retain origin and confirmed Save.
```

Application callbacks enforce external-format resource limits and exact durable
values. Keep the saved main and its whole `.<filename>.journal/` directory
together. Legacy flat layouts remain supported. A stale byte digest, foreign generation or lossy codec rejects. Full
Save updates the document; ordinary edits append native differential journal
records. The default journal API remains appended; external formats explicitly
select `sidecar`. This example does not add an automatic XML/OPC profile adapter.
See the [usage contract](../../../docs/guides/usage.md#external-documents-with-a-differential-sidecar)
and [journal guide](../../../docs/managed/journal.md).
See [origin, confirmation and unresolved-restart rules](../../../docs/managed/document_origin.md).
