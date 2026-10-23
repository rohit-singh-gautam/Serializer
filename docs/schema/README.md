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
| [Named managed self references](named_managed_references.md) | Versioned self-token fields in distinct named C++ generic records, supported wrappers and limits. |
| [Optional values](optional.md) | Presence, defaults, target representations, decoding failures, and managed occurrences. |
| [Unions and owning variants](unions_and_variants.md) | Choice declarations, owning storage, discriminator contracts, and backend limits. |
| [Behavior and native source](behavior.md) | Portable functions, native fragments, checked expressions, and transient members. |
| [Rebuildable caches](../design/cache_attributes.md) | Owning C++ cache declarations, optional priority, source validity, providers and close-time retention. |
| [Value construction](construction.md) | Typed constructors and factories, concrete/generic parameter defaults, numeric bindings, authoring hooks, and transient layout. |
| [Array and callable APIs](array_and_callable_api.md) | Explicit array exposure/axes, native borrowing, logical helpers, owner field factories, availability, typed bindings and richer signatures. |
| [Owning record APIs](owning_api.md) | Readonly getters, direct initialization, nested defaults, reciprocal signatures and arithmetic mappings. |
| [Record conversions](conversions.md) | Explicit portable conversion methods and supported value shapes. |
| [Record comparisons](record_comparisons.md) | Generator configuration for owning equality and ordering; no additional schema or wire metadata. |
| [Digest fields](digest.md) | Stored digest bytes, fixed algorithm lengths, and application-supplied digest creation. |
| [Magic and output exclusions](magic_and_omission.md) | Constant artifact signatures, typed magic, protocol exclusions, and simplicity warnings. |
| [Compact integers](compact_integers.md) | Optional unsigned scalar encodings and strict or lenient prefix validation. |
| [Derived record sizes](derived_sizes.md) | Encoded-byte counts, stored and wire-only fields, and native binary validation. |
| [Payload revisions](versioning.md) | Historical layouts, field lifetimes, reservations, read policies, and compile-time release policies. |
| [Schema compatibility checks](schema_evolution.md) | Comparing schema revisions and retaining stable identities and serialized meaning. |

[Inline value references](value_references.md) describe language 1.19.0 memory
ownership with ordinary value output and record translators that borrow arrays.
[Field references and scoped translators](references.md) describe language 1.18.0
ownership, durable address resolution and synchronous projected output.

[Package application contracts](../serialization/package_application_contracts.md)
describe language 1.21.0 fixed/default storage policies and scoped scalar keys/references.
