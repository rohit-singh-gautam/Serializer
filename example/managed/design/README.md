# Managed design schema

[design.serializer](design.serializer) models a hollow cylinder as the difference
between two independently managed cylinders. A cylinder's position is an ordinary
point value; a marker demonstrates a point with its own managed identity.

This is a schema example, not a separate executable. The
[managed integration fixture](../../../test/resources/managed_generated.serializer)
includes it, and [managed generated tests](../../../test/managed_generated_test.cpp)
exercise its generated editors. The direct-representation and naming-profile tests
also generate that fixture.

In a repository build configured with `SERIALIZER_BUILD_MANAGED=ON` and
`SERIALIZER_BUILD_TESTS=ON`, build `managed_store_test`, `managed_direct_test`,
and `managed_profile_test`, then run those CTest entries.

For the simpler point-only progression, return to the [example index](../README.md).
