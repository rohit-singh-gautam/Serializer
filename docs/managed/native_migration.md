# Native managed migration status

The managed runtime is still implemented only in C++. The work described here is
preparation for native ports, not a completed feature migration.

Implemented groundwork:

- Kotlin now accepts the contextual property name `field`, which the canonical
  collaboration record schema uses. Wire names and IDs are unchanged.

- `src/managed_schema.hpp` owns the unchanged C++ schema-fingerprint algorithm so
  future target generators can bind to the same existing documents.
- `src/managed_lowering.hpp` makes an independent direct-identity AST with the
  reserved `persistent_id` field, uint32/uint64 identities, and copied type/namespace
  references. Tests exercise all target codec emitters without enabling native
  managed generation. Separate-values lowering is not included.
- The [wire matrix](../../example/managed/multilanguage/README.md) exchanges the
  canonical record schemas in four independent folders, checking every selected
  producer/consumer pair, full-width integers, byte payloads and exact consumption.
  `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` registers one CTest per record family
  under the `serializer_managed_wire` label.

Required before enabling a native backend's `managed` support:

1. Generated entity traversal, identity validation and allocation, schema binding,
   transactional publication, and language-appropriate edit APIs. Preserve both
   direct and separate-values representations and their current wire contracts.
2. Disabled, linear and tree history; optional labels; bounded save/load;
   undo/redo/checkout, retention and non-reuse of allocated identities. Test actual
   stores, including failures and canceled/no-op edits, in every language.
3. Version-two journal framing and replay, appended/sidecar containers, native
   exclusive writer locks, flush-before-publication, full Save replacement, bounded
   recovery, torn tails and indeterminate-outcome fencing. Test crash boundaries
   with real files; opaque bytes in a codec test are not durable journal coverage.
4. Authority/replica and store-owned session engines, protocol/version checks,
   retries, conflict detection, leases/presence, local queues, and contribution-aware
   collaborative undo/redo. Test stale/gapped messages, ownership changes and
   authority restart boundaries, including mixed-language participants.
5. Trusted-session binding and authorization hooks. Authentication remains
   host-owned in the current C++ contract. Decoding an opaque session envelope
   does not authenticate the sender or authorize its changes.
6. Replace the record-only examples with real history handoff, journal recovery,
   authenticated collaboration and per-session undo demonstrations. Run every
   producer/consumer direction, including C++ with every target and non-C++ pairs.

Cross-language history is meaningful for document handoff when schema binding,
identity width, history mode and label policy agree. The receiving runtime must
resume the saved cursor and retained future, not simply copy the current value.
Collaborative undo must additionally preserve other sessions' contributions;
loading an old document snapshot is not a substitute.

No schema syntax, public managed API, saved format, or generated naming change is
introduced by this groundwork. Both editor implementations were reviewed for
those boundaries; their source/packages and synchronized release versions remain
unchanged. Navigation of new native managed APIs must be verified when those APIs
are implemented.

## Verification on 2026-10-02

The Windows Release compiler and core test target built with MSVC. All **238 core
tests passed**, including the three lowering tests and the Kotlin `field` regression.
The four wire suites each passed **11 producer checks, 121 producer/consumer
exchanges, and 22 malformed-input checks**: **484 exchanges and 88 rejection
checks** across all four families. Each successful client invocation also round
trips the value through all four native codecs.

C++/Java/JavaScript/TypeScript/Go/C#/Python/Kotlin ran on Windows; C/Rust/Swift ran
with explicitly selected WSL SDKs. Kotlin 2.3.10 was invoked directly with its
installed compiler jars and JRE 21 after the packaged batch launcher failed to
locate its preloader. The initial all-language run exposed the contextual `field`
rejection; collaboration and authentication passed after the generator correction.
History and journal record tests passed before that correction, which does not
affect their schemas.

These are codec/generator results. Native managed engines, real cross-language
history navigation, journal crash recovery, authority acceptance, and trusted-session
authorization behavior have **not** been implemented or qualified by this change.
The existing C++ managed feature/crash suites were not rerun. No sanitizers or
performance measurements were run, and neither editor extension was rebuilt.
