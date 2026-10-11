# Schema and generated model contracts

[Documentation index](../README.md)

Begin with small examples, then use the reference to check declarations and
language-version requirements. These guides describe implemented features and
identify unsupported combinations in their respective contracts.

| Guide | Read it for |
| --- | --- |
| [Small schema examples](schema_examples.md) | Complete introductory declarations for classes, collections, enums, identities, and defaults. |
| [Schema reference](schema_reference.md) | Declaration syntax, field IDs, representations, annotations, and protocol choices. |
| [Generic classes and dimensions](generics.md) | Native C++ templates, concrete specializations, fixed dimensions, and type constraints. |
| [Optional values](optional.md) | Presence, defaults, target representations, decoding failures, and managed occurrences. |
| [Unions and owning variants](unions_and_variants.md) | Choice declarations, owning storage, discriminator contracts, and backend limits. |
| [Behavior and native source](behavior.md) | Portable functions, native fragments, checked expressions, and transient members. |
| [Value construction](construction.md) | Typed constructors and factories, numeric bindings, authoring hooks, and transient layout. |
| [Record conversions](conversions.md) | Explicit portable conversion methods and supported value shapes. |
| [Record comparisons](record_comparisons.md) | Generator configuration for owning equality and ordering; no additional schema or wire metadata. |
| [Digest fields](digest.md) | Stored digest bytes, fixed algorithm lengths, and application-supplied digest creation. |
| [Magic and output exclusions](magic_and_omission.md) | Constant artifact signatures, typed magic, protocol exclusions, and simplicity warnings. |
| [Compact integers](compact_integers.md) | Optional unsigned scalar encodings and strict or lenient prefix validation. |
| [Derived record sizes](derived_sizes.md) | Encoded-byte counts, stored and wire-only fields, and native binary validation. |
| [Payload revisions](versioning.md) | Historical layouts, field lifetimes, reservations, read policies, and compile-time release policies. |
| [Schema compatibility checks](schema_evolution.md) | Comparing schema revisions and retaining stable identities and serialized meaning. |
