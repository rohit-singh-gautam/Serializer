# Serializer documentation

Start with the guide for the kind of application you are building. The topic
indexes below link to the full reference and recorded verification.

| Goal | Start here | Continue with |
| --- | --- | --- |
| Define models and exchange data | [C++ usage guide](guides/usage.md) | [Schema examples](schema/schema_examples.md), [language outputs](languages/README.md) |
| Add generation to a build | [CMake integration](guides/cmake_integration.md) | [Build and generation](guides/build_and_generation.md), [compiler options](guides/command_line.md) |
| Evolve stored data | [Payload versioning](schema/versioning.md) | [Schema compatibility](schema/schema_evolution.md), [wire contract](serialization/wire_format.md) |
| Edit application state with undo and recovery | [Managed-state quick start](managed/getting_started.md) | [C++ runtime](managed/cpp_runtime.md), [journaling](managed/journal.md), [local collaboration](managed/local_collaboration.md) |
| Set up schema editing and navigation | [Editor extensions](editors/editor_extension.md) | [Navigation coverage](editors/editor_navigation.md), [generated-header IntelliSense](editors/intellisense.md) |
| Integrate through a coding agent | [Agent integration](guides/agent_integration.md) | [Canonical integration skill](../.agents/skills/serializer-integration/SKILL.md) |

## Browse by topic

| Collection | Contents |
| --- | --- |
| [Guides](guides/README.md) | Application usage, build setup, compiler commands, agent setup, and database storage guidance. |
| [Schema language](schema/README.md) | Syntax, types, construction, behavior, generics, field metadata, and compatibility. |
| [Serialization](serialization/README.md) | Wire formats, views, compression, Protobuf, constant evaluation, and C++ performance. |
| [Generated languages](languages/README.md) | Output configuration, coding profiles, target APIs, SDK prerequisites, and backend limits. |
| [Managed state](managed/README.md) | C++ transactions, identity, typed registries, history, journals, collaboration, and managed design contracts. |
| [Editors](editors/README.md) | Visual Studio and VS Code setup, schema/generated-output navigation, and IntelliSense. |
| [Implementation walkthroughs](internals/README.md) | How current C++ transactions and generated editors work. |
| [Project information](project/README.md) | Feature status, licensing, distribution, and product scope. |
| [Design work](design/README.md) | Proposed paged indexing and historical implementation specifications. |
| [Verification records](verification/README.md) | Dated test results, toolchain qualifications, and the historical wire-efficiency assessment. |

## Status and examples

The [feature-status reference](project/feature_status.md) distinguishes implemented
behavior from proposals. Native managed engines currently run in C++; other
languages can exchange managed records. Managed design documents retain their
individual implementation boundaries. Design specifications and dated reports
should be read with the current API guides and the versions they identify.

Use the [runnable example catalog](../example/README.md) for complete consumers.
Release information and the project quick start are in the [project overview](../README.md).

## Maintaining this collection

Keep each document in the collection matching its main subject and link it from
that collection's index. Keep proposals and historical verification distinct from
current usage contracts. When moving a document, update relative links, fragments,
repository paths, and GitHub links throughout the repository, including examples
and the integration skill. Preserve the tracked illustrative
[managed walkthrough schema](managed/walkthrough.serializer) alongside its guide.
