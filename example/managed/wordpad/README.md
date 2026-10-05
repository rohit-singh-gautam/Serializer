# Managed wordpad schema

[wordpad.serializer](wordpad.serializer) declares a document with a title and an
array of independently managed paragraphs.

This is a schema example, not a separate executable. The
[managed integration fixture](../../../test/resources/managed_generated.serializer)
includes it, and [managed generated tests](../../../test/managed_generated_test.cpp)
exercise paragraph insertion, editing, and removal. The direct-representation and
naming-profile tests also generate that fixture.

In a repository build configured with `SERIALIZER_BUILD_MANAGED=ON` and
`SERIALIZER_BUILD_TESTS=ON`, build `managed_store_test`, `managed_direct_test`,
and `managed_profile_test`, then run those CTest entries.

Return to the [example index](../README.md).
