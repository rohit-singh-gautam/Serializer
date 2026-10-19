# Explicit record conversions

Language **1.19.0** adds [inline value references and payload projection](value_references.md)
for value-only export. Scoped translators can borrow ordinary records containing
arrays while owning conversions keep their existing independent-copy restrictions.

Language 1.18.0 adds [typed field references and scoped projected translators](references.md);
reference addresses preserve identity while ordinary conversions produce independent values.

Language **1.9.0** declares portable conversion methods without native code blocks:

```text
serializer version 1.9.0;
class postal_address {
  public string street(1);
  public uint32 postcode(2);
}
class shipment_ticket {
  public postal_address contact(1);
  public conversion postal_address as to_address {
    street = contact.street;
    postcode = contact.postcode;
  };
}
```

All eleven output languages generate a named instance method using their selected
naming profile. C++ also generates an explicit conversion operator when the
target type is unique. There is no implicit cross-record conversion. Other
languages use the named method; C uses its normal status and output-pointer API.

Assignments name public target fields and read public source fields, including
dotted nested projections and inherited fields. Types must match exactly;
numeric narrowing and user code calls are not inserted. Every call creates a
fresh target with its schema defaults, applies assignments, and leaves the source
unchanged. Strings and supported nested records copy their owned values; mutable
results do not alias source records. Eligible C++ methods are `constexpr`.
Compiler/runtime 1.15.0 includes dotted revisions in that subset, marks conversion
results `[[nodiscard]]`, and emits `noexcept` for proven nonthrowing scalar value
copies/defaults. Allocating or unproven generic conversions remain potentially throwing.
C builds an initialized candidate, releases it on failure and publishes only
success, including allocation failure during string copies.

Supported values are scalar primitives, enums, strings, and recursively ordinary
unpacked owning records with public fields and public bases. Arrays, maps,
optional values, variants, digests, views, packed/managed records, opaque native
storage and polymorphic shapes are diagnosed. Wire-only metadata is excluded
from recursive value copies and cannot be assigned or projected explicitly.
Generic targets bind to actual arguments of each generated instance.

Conversion declarations have no field ID, do not enter codecs and do not change
existing wire bytes. Use class-specific INI construction/comparison settings for
those independent concerns; conversions define an explicit cross-record mapping
in the schema. Arbitrary native casts remain application-defined.

The [public schema](../../example/schemas/conversions/model.serializer) and
[all-language consumers](../../example/README.md#record-conversions) exercise fresh
defaults, nested copies, generic targets, and the generated APIs. See
[usage](../guides/usage.md), [migration](../../migration.md), and the
[qualification record](../verification/verification-deferred-features-2026-10-11.md).
