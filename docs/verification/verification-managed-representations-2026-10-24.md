# Managed representation boundary verification — 24 October 2026

Compiler/runtime **2.34.0**, Windows x64 MSVC **19.51.36260.0**, C++23 Release,
CMake **4.4.2**. The generated support/rejection matrix preserves existing
schema headers, native identities, separate-value wrapper layouts and wire tags.

The complete compiler suite passes **639/639** cases with the configured
clang-format check enabled. Its managed representation cases cover direct and
separate storage, uint32/uint64 identities, fixed/compact encoding, packed/view
roots and reachable ordinary children, recursive ownership, private/protected
bases and repeated-ancestor diamonds. Positive controls retain private fields,
distinct public bases, ordinary values and explicit managed children. Unrelated
ordinary packed generation remains accepted.

Direct managed Protobuf rejects before lowering because native metadata field
`1073741823` exceeds the Protobuf field-number domain. Separate-value Protobuf
retains identity field `1` and payload field `2`; four generated profiles pass
**eight runtime cases**, including exact binary golden bytes, ProtoJSON,
TextProto, identified import, checked editing and Undo.

Commands use the root Windows configure wrapper, single-worker CMake target
builds and serial CTest. The final compiler JSON is
`out/build/managed-language-core.json`; the earlier native wrapper evidence is
in `out/build/managed-contracts-windows-passing.log`. An initial new rejection
fixture accidentally reused an inherited field ID; the retained first-run log
records that failure, and the corrected test exercises the intended generator
boundary.

The [representation contract](../managed/representation_contracts.md) defines
these supported and explicitly rejected selections. Passing rejection checks do
not implement packed/view managed storage, recursive owning editors or direct
managed Protobuf. Linux and installed-consumer replay remain separate gates.
