# Serializer — State Framework

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

This is the editorial reference for current project descriptions. Use the
implementation and [feature status](feature_status.md) to qualify individual
capabilities; the descriptor does not rename any API or distribution.

## Shared wording

| Context | Wording |
| --- | --- |
| Product name | `Serializer` |
| Exactly two-word descriptor | `State Framework` |
| Combined reference | `Serializer — State Framework` |
| Editor display name | `Rohit Serializer` |
| Product website | <https://www.singh.org.in/serializer.html> |

Use the opening sentence verbatim in the project introduction and editor
"About Serializer" sections. Component descriptions should explain their own
scope rather than repeat the whole framework feature list.

GitHub About and project-card description:

> State Framework for schema-driven, multi-language serialization and schema evolution, with C++ transactions, undo/redo history, crash-recovery journals, and collaboration.

Short page metadata:

> Serializer — State Framework: multi-language serialization and schema evolution, plus C++ transactions, undo/redo, crash-recovery journals, and collaboration.

Prepared GitHub homepage: <https://www.singh.org.in/serializer.html>.
Suggested topics: `cpp`, `cpp20`, `serialization`, `schema`, `code-generation`,
`binary-serialization`, `json`, `state-management`, `undo-redo`, `journaling`,
`crash-recovery`, and `collaboration`. Merge these with current curated topics
when applying settings. Local documentation does not update GitHub About.

## Capability boundaries

- Generated models/codecs cover eleven language outputs; TypeScript declarations
  use the JavaScript runtime. Native managed engines currently run in C++.
- C++ managed stores provide identity, typed editors, transactions, snapshot
  history, journals, and authority-coordinated collaboration. Applications supply
  transport, synchronization scheduling, authentication, and trusted sessions.
- History is application-model undo/redo. Journals recover committed work under
  the [documented durability and failure contract](managed/journal.md).
  Client journal recovery and authority restart guarantees are separate; see
  [collaboration limits](managed/collaboration_runtime.md).
- Disjoint field edits can merge; same-field and ownership conflicts require
  application resolution. Authorization uses authority gates and host policy
  hooks. Built-in accounts, roles, read filtering, and a hosted service are not
  provided.
- Payload versioning supports unmanaged owning models across the native codecs.
  Managed models, views, and Protobuf mappings reject it. Fixed arrays currently
  require native C++ codecs; concrete generic instances are required for other
  language contracts. Follow the individual feature guides for combinations.
- Managed support, SIMD, and compression backends default ON in fresh source
  builds. Application adoption remains optional; store templates independently
  select history, journal, and collaboration support.
- Database adapters, schema feature selectors, delta history/journals, and
  other-language managed engines remain proposals. Verification records support
  only the configurations they describe, not universal performance claims.

## Distribution and identity

The editor extensions provide schema editing and navigation. They do not install
the compiler/runtime or implement application history, journals, or collaboration
inside the IDE. Source, published VSIX packages, and the vcpkg port can contain
different versions and capabilities; consult [distribution guidance](distribution.md).

Preserve the repository name, CLI, `.serializer` suffix, namespaces, public
includes/APIs, schema/wire identities, CMake package/targets, extension IDs and
display names, and `rohit-singh-gautam-serializer` port identity. Generated-file
banners are part of navigation discovery and should not be rebranded.

Use "schema compiler," "code generator," "serialization runtime," and "managed
runtime" for those components. Use "state management" for the capability and
**State Framework** for the descriptor.

Contributor implementation guidance lives in the
[repository/editor/vcpkg specification](02_Serializer_Repository_Extensions_vcpkg_Spec.md).
The website specification is maintained in the website checkout; these
descriptions do not create a build dependency on website files or Markdown.
