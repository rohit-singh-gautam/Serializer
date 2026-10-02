# Managed wire records across languages

These four qualification examples exchange the existing runtime record schemas:

- [History](history/README.md): saved linear history, retained redo states and cursor.
- [Journal](journal/README.md): history-free baseline, allocation watermark and opaque frame bytes.
- [Collaboration](collaboration/README.md): accepted snapshot, field contribution versions and dependencies.
- [Authentication](authentication/README.md): application-defined opaque session envelope.

Each folder owns its schema and independent expected data. Maintained clients for
all eleven outputs live under `clients/`. Every selected language creates a
binary-integer message and consumes every other producer's message. C++ participates
in the same matrix as every other language; non-C++ pairs are included too.
Every client additionally checks all four codecs. Truncated and trailing input
must fail without publishing output.

```sh
python example/managed/multilanguage/run.py --compiler <serializer> --language all
python example/managed/multilanguage/run.py --compiler <serializer> --feature history --language cpp,python
```

The [standard SDK runner requirements](../../README.md) apply. Use
`--wsl-languages c,rust,swift` to select installed WSL SDKs explicitly on Windows.
No SDK is downloaded or silently skipped. C++ requires its normal compiler environment
and matching Serializer library, optionally supplied with `--cpp-library`.

These examples qualify **record codecs only**. Other-language managed engines are
not implemented by this matrix: it does not execute native undo/redo, acquire a
journal writer lock, flush/recover durable storage, accept a collaboration proposal,
or authenticate a session. Opaque snapshots and frame bytes are test payloads,
not valid managed model snapshots or complete journal files. A session envelope
carries identity; it is not proof of authentication. Trusted transport binding and
authorization remain host responsibilities.

See the [migration status](../../../docs/managed/native_migration.md) for the
remaining runtime and behavioral-test work.
The [verification record](../../../docs/managed/native_migration.md#verification-on-2026-10-02)
records the complete eleven-output matrix and its limits.

Cross-language history is useful for document handoff: one runtime saves retained
states, another resumes undo/redo with the same schema, identity width, history mode
and label policy. This record test is necessary groundwork for that example, not
evidence that the other-language undo engines are complete. Collaborative undo also
requires contribution/version checks so another session's edits are preserved.
