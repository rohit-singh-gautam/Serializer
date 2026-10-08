# Serializer — State Framework
## Specification 2: Repository, editor extensions, and package positioning

**Repository:** <https://github.com/rohit-singh-gautam/Serializer>

**Prepared:** 8 October 2026

**Purpose:** Implementation instructions for Codex or another coding agent working in the Serializer source checkout.

**Change type:** Product positioning, documentation, discoverability, and descriptive package metadata. Not a repository, API, schema-language, or package-identity rename.

**Repository-context review:** 8 October 2026. The local working tree is the
implementation reference, including the compiler `1.5.0` work present when this
review began. The dated remote inspection below is evidence to recheck, not a feature
ceiling. Shared copy is maintained in [product positioning](product_positioning.md);
package observations and follow-up are in [distribution guidance](distribution.md).

---

## 1. Objective and execution boundary

Keep the product name **Serializer** and use **State Framework** as its exactly two-word descriptor. Update the project overview and distribution descriptions so they reflect the broader functionality without implying capabilities that a particular runtime, extension, or package does not provide.

Read applicable `AGENTS.md` files and the current checkout before changing files. Preserve unrelated working-tree changes. Treat the source and packaging behavior actually present in the checkout as authoritative when it differs from this dated inspection baseline.

Before editing, record a short evidence matrix: current source facts, existing
documentation gaps, distribution facts, intended corrections, and verification
required. Resolve discrepancies first; do not treat an AI-written specification
as evidence of implemented behavior. Keep recommendations tied to public
Serializer source, checked examples, and dated verification records.

Complete local documentation and metadata changes, validations, and local package previews. Prepare external changes as separate deliverables. **Do not push, create public releases, publish VSIX files, change GitHub settings, or submit/merge an upstream vcpkg PR without a separately authorized publishing step.** Missing remote access must not prevent completion of local work.

Do not edit the personal website in this task; it has a separate specification. Both specifications share the wording in section 2 and can be implemented independently.

## 2. Approved wording: shared contract

| Context | Approved wording |
|---|---|
| Product name | `Serializer` |
| Exactly two-word descriptor | `State Framework` |
| Combined reference | `Serializer — State Framework` |
| Existing extension display name | `Rohit Serializer` |
| Product website | `https://www.singh.org.in/serializer.html` |

### Canonical one-sentence definition

> Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

Use this verbatim, allowing normal Markdown line wrapping, in the root README introduction and the full “About Serializer” text shared by extension documentation. It describes the overall project; it is **not** a claim that editor extensions implement the managed runtime or that all published packages already ship the latest source capabilities.

### Short GitHub About description

> State Framework for schema-driven, multi-language serialization and schema evolution, with C++ transactions, undo/redo history, crash-recovery journals, and collaboration.

This is 171 Unicode characters and suitable for a compact repository description.

### Short metadata description

> Serializer — State Framework: multi-language serialization and schema evolution, plus C++ transactions, undo/redo, crash-recovery journals, and collaboration.

### Terminology rules

“State Framework” is the descriptor. “State management” names a capability. “Managed runtime,” “schema compiler,” “code generator,” and “serialization runtime” remain valid component terms.

Do not switch between “State Framework” and competing labels such as “State Engine,” “Data Platform,” or “Collaboration Platform.” Do not rename a compiler or serialization-specific API merely because its accurate technical description contains the word serialization.

## 3. Inspection baseline and important discrepancies

The original remote observations were made on 8 October 2026. The subsequent
local review established compiler/runtime **1.5.0**, schema language **1.2.0**,
and both extension sources **1.1.25** before this documentation update. The local
base commit was `d69741f7ea5846f3eeab264bec19ec6f0abde8ce`; existing working-tree
changes already implemented batch configuration generation. Recording that commit
alone does not describe those uncommitted changes.

During the review, that work was committed as
`087083c9b2d87a7ead4605412d2198fa62a46f62` ("Generate multiple INI variants from one
schema parse"). Keep that change intact and record the final implementing revision
separately; these prose/metadata edits do not add another compiler feature.

Recheck these surfaces before implementation:

| Surface | Observed state | Consequence |
|---|---|---|
| Root `README.md` | Originally began with a narrow JSON capability note; local review reports compiler/runtime `1.5.0` and schema language `1.2.0`. [S1] | Put product positioning first; retain the JSON contract and current batch-generation guidance. Do not equate compiler and schema versions. |
| GitHub About | Still describes C++ serialization only; homepage is unset and topics are empty in the inspected repository metadata. [S2] | Prepare separate About, homepage, and topic updates. |
| VS Code manifest | `serializer-language`, publisher `rohitjairajsingh`, display name `Rohit Serializer`, version `1.1.25`. [S3] | Change descriptive metadata without changing identity or functional contributions. |
| Visual Studio VSIX manifest | Stable Identity ID recorded in section 4; display name `Rohit Serializer`, version `1.1.25`. [S4] | Preserve identity, assets, installation targets, and assembly names. |
| Visual Studio documentation | Requires both editor extension versions to match; also contains publication-pending wording. [S5] | Preserve version synchronization, but distinguish current source from already existing Marketplace listings. |
| Marketplace | Both official listings report published version `1.1.22`; some overview text still describes the project only as a schema compiler/library. [M1][M2] | Update the actual listing source and prepare any portal-only edits. Source `1.1.25` and the next local package are not those published packages. |
| Root `vcpkg.json` | A development dependency manifest containing test/compression dependencies; not the upstream port definition. [S6] | Updating this file alone cannot update vcpkg's published description. |
| Upstream vcpkg port | Manifest reports `1.0.0`; source is pinned to `f1966b6a25c7b43f0257b60b7451ce99c7162f18`. The pin contains managed code, but its managed build defaults OFF and the port does not enable it. [V1][V2] | The port configuration omits `Serializer::managed` and generated managed/collaboration records. Some managed headers alone do not establish a usable managed package. This inspection does not replace an installed-consumer test. |

The inspected `main` Git tree was `642301bdbcab903fa253bd169be0f3d0eaba30ce`. This is a tree identifier, not a release tag or a substitute for recording the implementing checkout's commit. [S9]

### Repository-context improvements to preserve

| Evidence in this checkout | Documentation treatment |
| --- | --- |
| [CMake and installed-package contracts](cmake_integration.md) | Explain source dependency and installed package routes, host generation/formatting prerequisites, managed record installation, and default-enabled compression dependencies. |
| [Batch configuration generation](command_line.md#generate-several-configurations-from-one-parse) | Preserve compiler `1.5.0` repeatable `--config` and `serializer_generate_variants`; independent INIs share a parse, not a new schema-language construct. |
| [Managed API](managed/cpp_runtime.md) and [examples](../example/managed/README.md) | Explain typed editors, persistent document/object identities, transaction forms, and independent compile-time history/journal/collaboration choices. |
| [Versioning](versioning.md) and [schema checker](schema_evolution.md) | Separate language headers, payload revisions, release policies, and compiler versions; qualify flexible JSON and conservative checker results. |
| [Constant binary output](constant_evaluation.md) and [compact scalars](compact_integers.md) | Preserve C++ opt-in constant/borrowed output and all-language owning compact scalar capabilities without extending them to unsupported combinations. |
| [Wire contract](wire_format.md), [views](views.md), and [qualification](../qualification/README.md) | Preserve encoding, borrowing/lifetime, limit, and verified-configuration distinctions. Avoid universal zero-copy or performance claims. |

This review improves existing guidance; it does not request additional runtime
features, schema annotations, package ecosystems, or a compatibility migration.

## 4. Stable identities: must not change

This is a positioning update. Protect these identities and contracts:

| Surface | Preserve |
|---|---|
| Repository | `rohit-singh-gautam/Serializer`, its clone URLs, and repository name |
| CLI and source files | `serializer`, `.serializer`, schema language IDs, grammar scope `source.serializer` |
| C++/CMake | Existing namespaces, public APIs, include paths, `find_package(Serializer CONFIG REQUIRED)`, `Serializer::` target namespace, `Serializer::managed`, and `serializer_generate` |
| VS Code identity | `name: serializer-language`, `publisher: rohitjairajsingh`, extension ID `rohitjairajsingh.serializer-language` |
| VS Code behavior | Existing command IDs, setting keys, activation events, language contributions, grammar names, engine range, entry point, capabilities, and extension kind |
| Visual Studio identity | `Rohit.Serializer.VisualStudio.40c33349-6c7b-42a6-b843-390d7120b1b9` |
| Visual Studio packaging | Publisher identity, assembly/package GUIDs, MEF registrations, asset paths, installation targets, architecture and prerequisite declarations |
| Published Marketplace URLs | `rohitjairajsingh.serializer-language` and `rohitjairajsingh.rohitserializervisualstudio` item names |
| vcpkg | Port name `rohit-singh-gautam-serializer`, install command, existing feature IDs, dependencies, and platform policy in a metadata-only change |
| Compatibility | Schema syntax, language-version headers, binary/JSON wire formats, magic bytes, protocol identifiers, generated-file identification banners, and existing file layouts |
| Legal/attribution | License files and expressions, copyright holders, notices, author, and publisher attribution |

Keep both extension display names as **Rohit Serializer**. Add the descriptor through descriptions, README subtitles, and About text. Do not rename the listings to the generic term “State Framework.” [S3][S4][S5]

Do not perform repository-wide search-and-replace. In particular, generated-file banners may participate in source/navigation discovery and are not decorative branding text.

## 5. File and surface inventory

### Files to inspect and update directly where relevant

| Path | Required treatment |
|---|---|
| `README.md` | Add canonical positioning at the top; expose both onboarding routes; preserve all detailed content and useful anchors. |
| `docs/feature_status.md` | Align whole-product introduction while retaining implemented/proposed distinctions. |
| `docs/managed/getting_started.md` | Add a concise relationship to the overall State Framework where appropriate; retain technical detail. |
| `docs/usage.md`, `docs/agent_integration.md`, and `.agents/skills/serializer-integration/` | Keep both onboarding routes and agent instructions consistent; explain actual dependency capabilities rather than assuming published packages equal current source. |
| `docs/versioning.md` and `docs/schema_evolution.md` | Correct stale header/unknown-field prose against the parser, readers, and compatibility checker; preserve protocol distinctions. |
| `docs/cmake_integration.md` and `docs/distribution.md` | Distinguish current-source package-build guidance, development manifest, and the published upstream port. |
| `docs/editor_extension.md` | Align extension/framework terminology, scope, and publication guidance. |
| `editors/vscode/README.md` | Update opening, About text, project links, and stale publication statements. |
| `editors/vscode/package.json` | Update description and relevant keywords; preserve functional metadata and stable identity. |
| `editors/visual_studio/README.md` | Update opening, About text, project links, and stale publication statements. |
| `editors/visual_studio/source.extension.vsixmanifest` | Update Description and relevant Tags only, except a coordinated version change during release preparation. |
| `editors/visual_studio/serializer_language.csproj` | Inspect for user-facing descriptive metadata or packaging references; no assembly or runtime-identity rename. |
| `vcpkg.json` | Optional upstream-project description/homepage fields only; retain development dependency semantics. |
| `vcpkg-configuration.json` | Inspect to locate registry/overlay ownership. Do not change baselines or registries for branding. |
| `CMakeLists.txt` and `cmake/` | Inspect existing descriptive project/package metadata. Update stale prose if present; do not introduce functional changes. |
| `docs/product_positioning.md` | Proposed new short source-of-truth document; use an equivalent existing document instead if the repository already has one. |

### Discover rather than assume

Inspect the current checkout for localized `package.nls*.json` strings, extension changelogs, Marketplace overview files, release-note templates, package-generation scripts, About/help prose, website links, agent integration summaries, and locally maintained vcpkg overlays or port templates.

Inspect `editors/build.ps1`, the individual editor build/package scripts, `install_extension.ps1`, and package validation tests before changing extension versions. They may embed filenames or require versions to match.

Do not create nonexistent package ecosystems just because the generators support their languages. There is no requirement to add npm, NuGet, PyPI, Maven, crates.io, or other runtime packages. Inspect and update existing descriptive metadata only.

Suggested discovery commands, run from the repository root:

```sh
git status --short
git rev-parse HEAD
git ls-files
rg -n -i 'schema compiler and serialization library|C\+\+.*serialization|Rohit Serializer|publication.*pending|not.*published' README.md docs editors
rg -n 'displayName|description|DisplayName|Description|Tags|homepage|PackageDescription|CPACK_PACKAGE_DESCRIPTION' editors CMakeLists.txt cmake vcpkg.json
rg -n 'rohit-singh-gautam-serializer|marketplace\.visualstudio\.com|1\.1\.25' .
```

Searches are discovery aids. Do not alter old historical release entries merely because they accurately describe an earlier state.

## 6. Root README: required structure and replacement copy

### 6.1 Replace the opening, not the whole README

Use the following opening block:

```markdown
# Serializer

**State Framework**

Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.

Define models in `.serializer` schemas and generate language-specific code for
exchanging data. For editable C++ applications, add the managed runtime to group
changes into transactions, retain history, recover committed work, and coordinate
collaborative edits.

Serialization is available across the supported language outputs. Native history,
journaling, and collaboration engines currently run in C++; other languages can
exchange managed wire records but do not have equivalent native managed engines.

[Project website](https://www.singh.org.in/serializer.html) ·
[Serialization quick start](#get-started) ·
[State-management quick start](docs/managed/getting_started.md) ·
[Feature status](docs/feature_status.md)
```

Keep `# Serializer` for a stable heading and place the descriptor directly below it. Do not make the first screen a list of recent release details.

Move the existing opening JSON capability note to a relevant JSON/format section, retaining its restrictions, regeneration requirement, and documentation link. Preserve the compiler/runtime and schema-version information in a clear section after the overview. Do not delete supported-language details, release policies, compact-field material, or constant-evaluation documentation to simplify the page. [S1]

### 6.2 Add a concise capabilities map

Add one compact table near the introduction; reuse an equivalent existing table rather than duplicating it:

| Need | Component or capability | Entry point |
|---|---|---|
| Define and exchange data | Schema compiler and generated serialization codecs | `docs/usage.md` |
| Evolve serialized payloads | Supported schema versioning and compatibility rules | `docs/versioning.md` |
| Group editable-model changes | C++ managed transactions | `docs/managed/getting_started.md` |
| Undo, redo, and retain branches | C++ history modes | `docs/managed/cpp_runtime.md` |
| Recover committed work | C++ journaling | `docs/managed/journal.md` |
| Coordinate edits across clients | C++ collaboration and authority hooks | `docs/managed/local_collaboration.md` |

Convert paths to Markdown links in the README. Verify their existence before committing.

### 6.3 Preserve two onboarding routes

Keep the existing minimal schema/code-generation example as the serialization-only quick start. Add a small **Build an editable C++ application** entry pointing to the existing managed guide and runnable examples under `example/managed/`.

Do not invent a simplified managed API or present pseudocode as compilable C++. Reuse a checked example when additional code is necessary. State clearly that the managed runtime is not required for serialization-only use.

Keep relevant existing anchor names stable, especially `#get-started`. If headings must change, update inbound repository links or retain a compatible explicit anchor.

## 7. GitHub About, homepage, and topics

Prepare the following external metadata update; do not confuse it with a README edit:

```text
Repository name: Serializer (unchanged)
Description: State Framework for schema-driven, multi-language serialization and schema evolution, with C++ transactions, undo/redo history, crash-recovery journals, and collaboration.
Website: https://www.singh.org.in/serializer.html
```

Suggested relevant topics:

```text
cpp, cpp20, serialization, schema, code-generation, binary-serialization,
json, state-management, undo-redo, journaling, crash-recovery, collaboration
```

Merge useful topics with any that have appeared since this inspection. Do not overwrite a newer curated topic list or add unsupported labels such as `crdt`, `orm`, or `distributed-database`.

Record this work as **prepared** until an authorized settings change succeeds and the repository metadata is read back. A documentation commit does not update GitHub's About panel. [S2]

## 8. Visual Studio Code extension

### 8.1 Manifest

In `editors/vscode/package.json`, replace the description with:

```json
{
  "description": "Editor support for Serializer — State Framework: schema highlighting, snippets, schema/generated-code navigation, and CMake assistance."
}
```

This is a **partial replacement fragment**, not a complete manifest. Preserve all other fields except the intentionally updated keywords or authorized release-version changes.

Keep `displayName` as `Rohit Serializer`, `name` as `serializer-language`, and `publisher` as `rohitjairajsingh`.

Retain useful existing keywords and add focused discovery terms such as `state-framework`, `serialization`, and `code-generation` where appropriate. Stay within Marketplace validation limits. Do not categorize the extension as a collaboration service, database, or source-control provider. The manifest's descriptions and README describe the extension; framework capabilities belong in a separately labeled About section. [S3][P1]

Keep the current extension-documentation `homepage` unless there is an established project-wide reason to change it. Add a distinct **Project website** link in the README so users can reach the product page without losing extension-specific help.

### 8.2 README and Marketplace overview

Keep the existing extension title and insert this subtitle immediately below it:

```markdown
**Editor support for Serializer — State Framework**
```

Opening paragraph:

> Rohit Serializer helps you edit `.serializer` schemas and navigate between schemas and existing generated code in Visual Studio Code. It provides syntax highlighting, snippets, and CMake assistance for projects using Serializer — State Framework.

Add or revise **About Serializer** to contain the canonical definition from section 2, followed by:

> The extension provides editor support; it does not install the Serializer compiler or runtime. Application history, journals, and collaborative editing are C++ framework capabilities integrated by the application, not editing features supplied by this extension.

Preserve all navigation/build distinctions, workspace-trust behavior, language-service prerequisites, existing-file lookup limitations, and supported hosts. Do not suggest that navigation automatically generates code or that ordinary VS Code undo is the managed-history feature.

VS Code packages normally use the extension-root README as their Marketplace body, but verify the repository's actual packaging process and included files. Update the maintained source rather than generated `dist` copies. [P1]

## 9. Visual Studio extension

### 9.1 VSIX metadata

In `editors/visual_studio/source.extension.vsixmanifest`, use this Description:

```xml
<Description xml:space="preserve">Editor support for Serializer — State Framework: schema highlighting, editing assistance, and navigation between schemas and generated code.</Description>
```

Keep `<DisplayName>Rohit Serializer</DisplayName>` unchanged. Preserve the XML namespace, `<Identity>` values, assets, installation targets, architecture, and dependencies. Use XML-aware edits or carefully scoped text changes; do not reconstruct the whole manifest from the snippet. [S4][P2]

Retain relevant Tags and add `State Framework` or `Serialization` only as discoverability terms. Preserve the existing `MoreInfo` extension-documentation destination; link to the product website from the README/overview.

### 9.2 README and Marketplace overview

Keep the extension-specific title and add:

```markdown
**Editor support for Serializer — State Framework**
```

Opening paragraph:

> Rohit Serializer adds schema highlighting, editing assistance, and navigation between schemas and existing generated code in Visual Studio for projects using Serializer — State Framework.

Retain the platform/version support and installation instructions from current source. Add the same **About Serializer** definition and editor/runtime separation used for VS Code.

Do not copy VS Code-only snippets or CMake commands into the Visual Studio feature list. The inspected Visual Studio documentation explicitly distinguishes those capabilities. [S5]

Find the actual maintained Visual Studio Marketplace overview source. The portal description may differ from the repository README; do not assume rebuilding a VSIX automatically replaces every portal field. Prepare a separate overview Markdown payload when no version-controlled source is used, and report where it must be applied.

### 9.3 Publication wording in both extensions

The reviewed Marketplace URLs exist, while some source and listing prose says publication is pending or the source distribution has not been published. Replace **unqualified current-status statements** with version-aware wording. Do not infer that source `1.1.25` is already the published version. [S5][M1][M2]

Safe current documentation wording:

> A published extension is available on Visual Studio Marketplace. This source checkout may include changes not yet available in the published package; compare the Marketplace version with the extension changelog before relying on recently added behavior.

Use each extension's own listing link. Preserve historical changelog statements that were true when written.

## 10. vcpkg: distinguish three separate surfaces

### 10.1 Repository development manifest

The root `vcpkg.json` controls development dependencies and compression options. It is not the published `microsoft/vcpkg` port. Do not add a dependency on the project itself, rename its feature keys, or change dependency versions/baselines to “update the brand.” [S6]

Adding `description` and `homepage` to this top-level manifest is acceptable when compatible with its existing conventions:

```json
{
  "description": "Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.",
  "homepage": "https://www.singh.org.in/serializer.html"
}
```

Merge only these fields, preserving existing content. Do not invent a package version or add a `name` purely to make this development manifest look like the upstream port. Top-level project description/homepage fields are optional under the vcpkg manifest model. [P3]

### 10.2 Existing local port or registry assets

Search for a maintained overlay, port template, or release script. Update it only if present. Inspect `vcpkg-configuration.json` and packaging documentation to establish ownership.

Do not create a second registry or an overlay merely to avoid the upstream update process. If no local port source exists, prepare the upstream patch separately and document that it targets `microsoft/vcpkg`, not this repository.

### 10.3 Published upstream port

The verified upstream paths are:

```text
microsoft/vcpkg
  ports/rohit-singh-gautam-serializer/vcpkg.json
  ports/rohit-singh-gautam-serializer/portfile.cmake
```

The package identity and command remain:

```sh
vcpkg install rohit-singh-gautam-serializer
```

**Do not simply replace the old port description with the latest full feature list.** First inspect its pinned source, build options, installed headers, CMake exports, and runnable consumer behavior. The inspected port's version is `1.0.0`; its pinned revision is recorded in section 3. [V1][V2]

#### Route A: metadata-only update of the existing packaged version

When keeping the existing source revision, use wording that accurately describes only the packaged components. Suitable conservative copy, after confirming it matches the package:

```json
{
  "description": "Serialization components of Serializer — State Framework: a C++20 schema compiler and runtime with multi-language output, JSON, binary, Protobuf codecs, and optional compression.",
  "homepage": "https://www.singh.org.in/serializer.html"
}
```

Retain additional currently documented codecs/compression details if verified for that pin. Do not assert that managed history, journals, or collaboration ship merely because the product website describes them.

The reviewed pin already contains managed implementation and tests. Its package
gap is configuration/installation: the
[pinned CMake source](https://github.com/rohit-singh-gautam/Serializer/blob/f1966b6a25c7b43f0257b60b7451ce99c7162f18/CMakeLists.txt)
sets `SERIALIZER_BUILD_MANAGED` OFF by default and
the port does not override it. Do not describe this as an absence of managed
source. Before enabling it, validate generated record installation, exported
targets, dependency discovery, host generation, and installed consumers. Existing
port features are `zstd`, `lz4`, and `zlib`; the root development features have
different `compression-*` names. Keep `!uwp` and the Windows static-linkage policy
unchanged in Route A.
The pin uses `SameMinorVersion` CMake package discovery, while current source uses
`SameMajorVersion`; preserve the pin's policy in metadata-only work and verify
current package-version compatibility in a separately scoped source upgrade.

A metadata-only port revision keeps the upstream version and follows vcpkg's port-revision policy. Do not change the source pin or source hash for an unchanged source archive. [P3]

#### Route B: prepare a separately scoped package update

Use the full State Framework description only after the selected package actually contains and exposes the documented managed capabilities:

```json
{
  "description": "Serializer is a schema-driven framework that combines multi-language serialization and schema evolution with C++ application-state management, including transactional editing, undo/redo history, crash-recovery journaling, and collaborative editing.",
  "homepage": "https://www.singh.org.in/serializer.html"
}
```

Before that update can be submitted:

1. Select a maintainer-approved immutable source revision associated with the intended upstream version. Do not assume a release tag exists because the README prints a version.
2. Compute and verify the source archive SHA512; never fabricate it or reuse the old archive's hash.
3. Check CMake/build-option changes between revisions, including managed-target installation and compression dependencies/defaults. Preserve existing port feature names unless a separate compatibility change is approved.
4. Build and test the package on supported target configurations available to the maintainer. Validate a consumer using the installed CMake package, not a source-tree shortcut.
5. Confirm the compiler can generate an existing schema example and that the installed runtime targets link. For managed claims, exercise an existing managed example against the installed package, including undo/redo and journal recovery; verify collaboration packaging before advertising it.
6. Update upstream version and registry version records using the current vcpkg maintenance workflow. Reset/remove `port-version` when the upstream version changes; increment it for revisions that keep the upstream version. [P3]
7. Prepare a separate upstream patch/PR description with source revision, package capabilities, dependency changes, and actual test results.

Locate and update the applicable registry version-database and baseline entries according to the registry's current workflow. Do not guess Git tree hashes or edit version records without their corresponding port contents.

If Route B requires runtime or packaging fixes beyond metadata, report that as a separate release/packaging dependency. Complete Route A or prepare its patch without silently widening this positioning task into an unvalidated package upgrade.

### 10.4 Source-versus-package availability note

Where installation guidance would otherwise imply full feature parity, use:

> Package availability can lag the source repository. Check the port version and build options for the features you need; use the source-build guide for the documented current source capabilities.

Do not claim that the registry has changed until the authorized upstream update is merged and its published contents are verified.

## 11. Other documentation and descriptive metadata

Update whole-project descriptions in current overview material, contribution introductions, agent integration summaries, or packaging metadata when the existing wording is incomplete. Preserve component-specific descriptions, historical release notes, licensing text, technical API names, and generated navigation markers.

For CMake/CPack or assembly metadata, update existing human-readable Description/Summary fields where they are actually used. Do not invent a new packaging system or add a CPack release configuration just for a tagline.

Create or reuse a short `docs/product_positioning.md` containing the product name, descriptor, canonical definition, short About description, editor/runtime distinction, package-availability distinction, and protected identity rules. Link it from an appropriate contributor-facing location. Keep it small and do not copy the entire implementation specification into the repository.

Use this file as an editorial reference, not as a new runtime/build dependency. Do not make code compilation require Markdown parsing or a network fetch.

## 12. Capability accuracy requirements

Validate the wider description against current implementation status. The following boundaries were documented in the inspected project sources. [S7][S8]

| Capability | Required qualification |
|---|---|
| Multi-language support | Refers to generated models/codecs and supported wire-record exchange. Native managed runtimes outside C++ are not implemented. |
| History | C++ transactional application-state history, including supported linear/tree choices; not source control or IDE text history. Current retention is snapshot-based. |
| Journals | Recovery of committed work under documented I/O/durability semantics; not an unconditional no-data-loss guarantee. |
| Collaboration | Authority-coordinated editing with explicit synchronization and conflicts; not a hosted service, CRDT, or automatically conflict-free peer-to-peer engine. |
| Authentication/authorization | Application-supplied authentication and session binding, with implemented authority policy hooks; not a complete users/roles/access-control platform. |
| Schema evolution | Preserve supported-model and protocol restrictions; do not imply universal managed/view/Protobuf versioning. |
| Databases | Database-adapter proposals are not implemented integrations. |
| Optional components | Optional adoption/feature selection is not equivalent to all managed features being disabled in fresh source builds. |
| Editor tooling | Editor features do not install or replace the compiler/runtime or provide runtime collaboration inside the IDE. |
| Distribution | Source main, compiler release, schema-language version, VSIX versions, and vcpkg port version are distinct facts. |

### Feature and API boundaries from the local review

- Fresh source builds enable managed support, SIMD, and Zstandard/LZ4/zlib.
  A build option exposing managed APIs does not add IDs/history to ordinary
  schemas. Application adoption remains optional.
- `model_store` defaults to linear history, disabled action labels, and
  `store_features::all`. History mode and `none`/`journal`/`collaboration`/`all`
  feature selection are compile-time choices; disabled features omit their
  attachment state and APIs. Do not present proposed schema selectors as their API.
- C++ managed identity defaults to `uint32`; `uint64` and opt-in separate-values
  representation are supported. Changing representation or identity width needs
  saved-state migration, not an editorial substitution.
- History and journal edits retain whole-root snapshots. Disjoint collaboration
  edits can merge; conflicting local drafts are retained for explicit resolution.
  Immediate local undo still faces current authority, permission, and lease checks
  during synchronization.
- A client journal can retain pending work and exact requests. Authority sessions,
  locks, deduplication and contribution history do not survive a fresh epoch;
  avoid an exactly-once-across-restarts promise. Follow the
  [authority epoch contract](managed/collaboration_runtime.md).
- New schemas use language `1.2.0`; `1` remains exactly `1.0.0`. Payload versioning
  supports unmanaged owning models across eleven outputs/four native codecs;
  managed models, views, and Protobuf mappings reject it. Replacement conversion
  remains application code; compiler release policies cannot supply missing layouts.
- Flexible JSON is opt-in and requires regenerated reader hooks; unknown binary
  fields still reject. The compatibility checker has no flexible-reader selection
  and conservatively reports incompatible unknown-field additions/removals.
- Fixed arrays currently require C++ native codecs. Native application-side
  generic APIs are C++ only; concrete schema instances define other-language
  contracts. Typed magic,
  inferred fixed arrays, compact scalars, views, managed models, constant output,
  emission-only output and Protobuf each retain their documented combination limits.
- Constant output is opt-in (`--cpp.constant_evaluation true`); positional-only
  generation is independently selected. Borrowed emission-only models also require
  those profiles and expose no reader. Do not imply universal zero-copy or that
  ordinary runtime serialization has been replaced.
- Repeated `--config` accepts independent INIs and paired outputs from one parse.
  Shared CLI overrides affect the batch. Backend validation completes before
  output writes, but filesystem writes across files are not atomic. Keep one
  consistent generated model definition in each compiled consumer.

Do not add `real-time`, `zero-copy everywhere`, `zero overhead`, `production-proven`, or performance/compliance guarantees without specific implementation evidence and appropriate scope.

## 13. Versioning, packaging, and release preparation

Documentation-only edits do not require a schema-language or compiler major/minor version change. Do not edit historical tags or releases to make them appear to use the new wording.

For prose/descriptive-metadata corrections alone, keep compiler/runtime and
schema versions at the checkout's existing values. If implementation work expands
into a compiler/runtime fix or feature, apply `AGENTS.md` patch/minor rules.
Every extension-related change, including documentation alone, must increment
**both extension patch components** together; do not defer that increment until
publication. The pre-edit local `1.1.25` pair advances to `1.1.26` for this change,
subject to a fresh check for intervening changes. Preserve unrelated `1.5.0` work.

For a new extension package containing changed metadata, follow the repository's current release policy and choose the next appropriate unused version. The inspected project requires **the Visual Studio and VS Code extension versions to match**. Update all version-dependent filenames, scripts, tests, and lockfile root metadata that genuinely derive from that version. Do not hardcode `1.1.26` without checking what has since been released. [S3][S5]

Do not update dependency versions during branding work. Use the established lockfile-preserving installation/build process. Add a small new changelog entry, under the actual next release or an existing Unreleased section:

> Clarified Serializer's State Framework positioning and updated project, editor, and package descriptions; no schema or runtime behavior changes.

Only use the final clause if the actual diff contains no functional change. Describe a separately approved port upgrade as a package upgrade, not merely a wording change.

## 14. Validation plan

### 14.1 Static and content checks

Parse changed JSON and XML rather than relying on visual inspection. Check Markdown links, relative paths from each extension directory, and fragment destinations. Verify that the canonical sentence appears after whitespace normalization in the designated overview/About sections.

Separate newly introduced link errors from pre-existing ones, while correcting
nearby affected links. Verify local links against the actual checkout and external
facts against their primary sources; a remote `main` document may lag uncommitted
local changes. Retain verification logs/packages under ignored output paths, and
review ignored artifacts without deleting pre-existing user build outputs.

Compare protected identities against the pre-change checkout. Review JSON structural diffs to confirm that functional extension contributions, engine requirements, dependencies, and port feature keys did not change in a metadata-only patch.

Scan for stale whole-product-only definitions, but do not require zero occurrences of “schema compiler” or “serialization library”; they are valid technical terms.

A lightweight consistency test is welcome if it fits the existing test structure. Prefer checking one authoritative wording definition plus selected surfaces over duplicating long marketing paragraphs in multiple brittle snapshots.

### 14.2 VS Code package checks

The inspected manifest provides these commands; confirm they still exist before running:

```sh
cd editors/vscode
npm ci
npm test
npm run package
```

Inspect the generated VSIX contents and rendered README/metadata. Confirm the extension ID is unchanged and the description names **Serializer — State Framework**. Do not hand-edit generated archives.

On an available suitable host, run the repository's existing navigation/integration smoke tests and verify that the updated package installs as an update to the existing extension, not a separate extension. Record any tests not run and why.

### 14.3 Visual Studio package checks

On Windows with the documented prerequisites, the existing combined packaging path is:

```powershell
# From the repository root:
./editors/build.ps1
```

The documented individual route is:

```powershell
npm ci --prefix editors/vscode
./editors/visual_studio/build.ps1
```

These paths build packages; they do not constitute Marketplace publication. Inspect the generated VSIX manifest, assets, version, Identity ID, and Description. Use the repository's existing package validation and host checks. Preserve the shared-version rule. [S5]

### 14.4 vcpkg checks

Validate edited manifests with the current vcpkg schema/formatting tools. For metadata-only changes, check that the dependency graph, feature IDs, source revision, source hash, and supported-platform settings remain unchanged. For a package revision upgrade, perform the installed-consumer checks from section 10 and report each tested triplet.

### 14.5 Scope and release checks

Run `git diff --check` and review the final diff. Compiler/runtime tests are required if functional build/runtime changes were introduced; do not claim a metadata-only patch requalifies all platforms or features.

Mark each deliverable accurately as one of **source edited**, **package built**, **smoke-tested**, **external update prepared**, or **published and verified**. These states are not interchangeable.

## 15. Acceptance checklist

- [ ] The repository remains named **Serializer** and uses **State Framework** as its descriptor.
- [ ] The root README leads with the approved one-sentence definition and exposes serialization and C++ managed-state onboarding.
- [ ] Technical details and important existing links/anchors are retained.
- [ ] GitHub About, homepage, and topic changes are either applied with authorization and verified, or delivered as explicit pending updates.
- [ ] Both extension introductions, About sections, and descriptive manifests are consistent.
- [ ] Both extension display names, IDs, command/settings identities, and functional contributions remain intact.
- [ ] The C++-only managed-runtime qualification is explicit wherever multi-language serialization and state-management features are summarized together.
- [ ] Extension documentation does not imply that installing the extension installs the compiler/runtime or adds runtime collaboration to the IDE.
- [ ] Current publication statements distinguish existing listings from newer source packages.
- [ ] The root development manifest is not confused with the upstream vcpkg port.
- [ ] Any upstream port description is accurate for its pinned and tested source, not merely current main.
- [ ] Compiler, schema, editor, and port versions are not incorrectly synchronized with one another; only the required pair of editor versions is synchronized.
- [ ] No license, schema syntax, wire format, API, namespace, package identity, or build behavior changed unintentionally.
- [ ] Actual validation results and skipped checks are reported; no remote publication is implied by a local build.

## 16. Deliverables from the implementing agent

Return the local diff and a report naming changed paths, final public descriptions, protected identities checked, and actual test/package results. Include locally built package paths when applicable.

Separately provide the prepared GitHub About values, any portal-only Marketplace overview payload, and the upstream vcpkg patch or clearly scoped package-update plan. Identify external steps that remain pending without asking the owner to repeat already settled branding decisions.

Do not publish or deploy these changes as part of implementing this specification unless the owner separately authorizes that step.

## Sources and verification notes

Sources were reviewed on 8 October 2026. Branch URLs can advance; inspect the current checkout and published package versions before implementation. The requested wording is a positioning decision; the cited sources establish capability boundaries, metadata identities, and the inspection baseline.

[S1]: https://github.com/rohit-singh-gautam/Serializer/blob/main/README.md "Root project overview"
[S2]: https://api.github.com/repos/rohit-singh-gautam/Serializer "Repository About, homepage, and topics metadata"
[S3]: https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/vscode/package.json "VS Code metadata, IDs, scripts, and contributions"
[S4]: https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/visual_studio/source.extension.vsixmanifest "Visual Studio VSIX identity and metadata"
[S5]: https://github.com/rohit-singh-gautam/Serializer/blob/main/editors/visual_studio/README.md "Visual Studio behavior, packaging, and synchronized version policy"
[S6]: https://github.com/rohit-singh-gautam/Serializer/blob/main/vcpkg.json "Development dependency manifest"
[S7]: https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/feature_status.md "Implemented features and exclusions"
[S8]: https://github.com/rohit-singh-gautam/Serializer/blob/main/docs/managed/getting_started.md "Managed history, journaling, collaboration, and authorization scope"
[S9]: https://api.github.com/repos/rohit-singh-gautam/Serializer/git/trees/642301bdbcab903fa253bd169be0f3d0eaba30ce "Inspected main Git tree"
[M1]: https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.rohitserializervisualstudio "Visual Studio published listing"
[M2]: https://marketplace.visualstudio.com/items?itemName=rohitjairajsingh.serializer-language "VS Code published listing"
[V1]: https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/vcpkg.json "Upstream port manifest"
[V2]: https://github.com/microsoft/vcpkg/blob/master/ports/rohit-singh-gautam-serializer/portfile.cmake "Upstream source revision and build configuration"
[P1]: https://code.visualstudio.com/api/references/extension-manifest "Official VS Code manifest and Marketplace README documentation"
[P2]: https://learn.microsoft.com/en-us/visualstudio/extensibility/vsix-manifest-designer "Official VSIX metadata and package-manifest documentation"
[P3]: https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-json "Official manifest field and port-version semantics"
