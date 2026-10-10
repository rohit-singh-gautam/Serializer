# Generic optional values

Language **1.7.0** and compiler/runtime **1.12.0** introduce an owning
`optional<T>` built-in generic. C++ storage is exactly `std::optional<T>`,
without another owning optional container. See [usage](usage.md#optional-owning-values)
for schema and codec examples.

## Capability selection

This feature uses owning generics, existing collection codecs, bounded exact
decoding, stable wire identities, schema compatibility checks, transient reset,
and the existing C++ identity/history/journal/collaboration framework.
Naming profiles and all native language outputs apply. Payload revisions remain
the migration mechanism for changing released fields. View/packed storage and
direct Protobuf optional mappings are unavailable and reject explicitly.
Compression and stream adapters operate at the existing message boundary; they
need no optional-specific format. Built-in authorization policies, database
adapters, and non-C++ managed engines remain proposals outside this feature.

## Type and default contract

`optional<T>` requires exactly one type argument. Scalars, enums, digests,
generated records, concrete generic records, and nested optionals are supported.
`array<T>` and `map<K,V>` are owning generic collection expressions, including
inside optionals. Existing `array optional<T>` and `map(K) optional<T>`
modifiers and owning variant alternatives may also contain optionals. Map keys
retain the scalar/enum contract; optional and collection keys reject.

Every optional initially has no value. Omitting an initializer or using `{}`
means absent. Nonempty initializers on `optional<T>`, `array<T>`, and `map<K,V>`
reject; existing collection-modifier defaults retain their previous contract.
Initialize present values through the generated target's standard representation. A
present record starts with its own schema defaults. Recursive inline ownership,
views and packing are unsupported. User-declared types named `optional` retain
precedence; new syntax does not reinterpret previously valid schemas.

## Presence and failure

Each native binary optional uses one byte: `0` for absent or `1` for present,
followed by the existing payload encoding. Unknown tags fail. JSON uses `[]`
for absent and `[value]` for present. Every nested level preserves its state.
There is no schema null payload type: JSON `null` is invalid, including inside
a present scalar/record. Empty strings and empty collections remain present
values. Serialized fields are emitted even when their optional is absent.

An explicit absent value resets an existing engaged destination. A present value
is decoded from its own defaults and replaces the old payload after its optional
frame succeeds. C++ in-place readers retain the destination's existing value
for missing keyed fields. APIs that decode a fresh object, including C's
transactional replacement API, retain schema defaults and absence for missing
fields. Ordinary C++ member input can update earlier fields before a later error.
Decode/finish a fresh candidate with
`deserialize_exact` before assignment when whole-object replacement must be atomic.
Input can be consumed on failure; exact-message checks reject trailing bytes.

Optional framing participates in each target decoder's existing input, nesting,
string and collection-element limits. C++ additionally accounts for logical
allocation and work; other targets retain their existing four-limit policy.
Absence does not allocate a payload. Present payloads use normal contained-type
budgets; logical allocation accounting is not a hard process-memory bound.
See [wire framing](wire_format.md#optional-presence).
Standalone C++ JSON `std::optional` values retain their legacy null/value codec.
Generated schema records and concrete schema generic applications select the
canonical contract automatically. Serializing a raw optional extracted from a
generated field uses the standalone codec. Hold the JSON reader's or writer's
`schema_values()` scope while serializing a raw optional to select canonical
zero/one-element framing explicitly.

## Standard target representations

| Target | Owning representation |
| --- | --- |
| C++ | `std::optional<T>` |
| Java, Kotlin/JVM | `java.util.Optional<T>` (boxed Java scalars) |
| Rust | `Option<T>` |
| Swift | `Optional<T>` |
| Go | Pointer to the contained representation, preserving each pointer level |
| C# | Nullable standard `System.ValueTuple<T>` |
| JavaScript/TypeScript | Standard zero/one-element array/tuple `[] | [T]` |
| Python | Standard zero/one-element tuple |
| C | Generated presence byte/boolean and owned value; C has no standard optional generic |

Nullable reference forms that collapse nested presence are deliberately avoided.
Use each target's ordinary constructors and APIs. No non-C++ managed engine is
introduced by these mappings.

## Managed occurrences and compatibility

An optional value without a managed occurrence remains an atomic owning field.
A managed optional occurrence visits the engaged generated child through the
existing model traits; absent children contribute no entity. Optional arrays/maps
can contain managed children. Transaction candidates, notifications, immutable
read pins, history, Save/load, journals and collaboration use the same generated
codec and traversal. Runtime transient state is excluded and reset recursively
when cloning/restoring durable values. Preserve existing ownership and ID rules
when inserting, removing or replacing an engaged child. Managed variant
alternatives must remain directly identified records; optional/collection
wrappers as managed variant alternatives are unsupported and reject. Ordinary
owning variants may contain optionals, and optional records may own variants.

Optional presence and all inner type arguments are included in compatibility
identities. Switching between scalar, optional, nested optional, or different
payloads requires an explicit migration. Language version, compiler release and
application payload revision remain independent. See [migration](../migration.md#generic-optionals-10-october-2026).

## Qualification

All 102 native CTest checks pass, including optional parser/codec tests, direct
and separate-values managed tests, and fresh-process journal recovery. Actual
generated consumers pass 431 optional and 80 variant fixture cases in each of
the eleven language SDKs. Basic, collections, complex and optional examples
pass in every language and all four protocols. See the
[dated verification record](verification-optionals-2026-10-11.md) for toolchains,
constant-evaluation checks, editor results and remaining qualification boundaries.
