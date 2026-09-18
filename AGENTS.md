# Serializer Agent Instructions

## Integrating Serializer into an application

- When asked to integrate this repository into another C++ or Java project or update that
  project's Serializer usage, read and apply the
  [Serializer integration skill](.agents/skills/serializer-integration/SKILL.md).
  This path is relative to this `AGENTS.md`, including when the checkout is a
  nested dependency. Read the file directly if it is absent from your skill list.
- Start with the skill's schema, CMake, and codec workflow and follow its links
  for the requested features. Apply the consuming project's local instructions
  to its files; the coding rules below govern changes to Serializer itself.
- Keep the canonical skill in `.agents/skills/serializer-integration/` and its
  direct entry link near the top of `README.md`. The directory is tracked source;
  preserve it when distributing the source repository.

## File naming and coding conventions

- Follow this repository's `CodingStandard.md` for coding conventions.
- Keep instructions and public documentation specific to Serializer. Do not copy
  ideas, quotations, project names, paths, or implementation details from private
  projects into this public repository.
- Use lowercase `snake_case` for new C++ file names, types, functions, variables,
  constants, enum members, and namespaces. Use descriptive `PascalCase` or
  conventional short names for template type parameters.
- Use `.hpp` for new C++ headers, `.cpp` for implementations, and `_test.cpp`
  for test files. Preserve tool-required names and existing public include paths.
- Use two-space indentation in new C++ files and preserve surrounding indentation
  in localized edits. Put opening braces on the declaration or control statement
  line and use braces around new conditional and loop bodies.
- Follow each repository's source and test layout, language baseline, licensing,
  and generated-artifact requirements. Regenerate build output through its real
  pipeline; preserve explicitly required versioned provider artifacts.
- Migrate existing code in focused changes, updating references and verification
  together. Preserve public APIs, schema identifiers, wire values, and generated
  names unless their migration is explicitly part of the task.

## Code clarity and standard practices

- Replace magic numbers and repeated semantic strings with descriptive named
  constants. Keep each constant in the narrowest useful scope; share it when
  multiple components implement the same contract. Include units in names for
  sizes, durations, offsets, and capacities.
- Derive values from their source of truth whenever possible: use `sizeof`,
  `std::size`, container `.size()`, `std::numeric_limits`, and constant expressions
  instead of repeating type widths, array lengths, masks, or dependent limits.
  Prefer deduced array extents and template arguments when that preserves the
  intended element type. Keep externally specified wire values explicit and
  independent of platform object layout.
- Keep obvious arithmetic identities, loop origins, empty states, and direct
  comparison values inline when a constant would add no meaning. Preserve
  intentional literal data in logging examples and test cases.
- Use `constexpr` for C++ compile-time constants, `const` for immutable runtime
  values, scoped enums for closed sets of states, and unsigned fixed-width types
  where the wire contract requires them. Avoid macros for ordinary constants.
- Add a concise comment to every function explaining its purpose. Document
  preconditions, return/failure behavior, ownership, lifetime, thread-safety, and
  side effects where relevant. Keep declaration and definition comments
  complementary rather than duplicating documentation.
- Inside functions, explain non-obvious algorithms, invariants, boundary checks,
  synchronization, and reasons for unusual decisions. Keep comments accurate
  during changes; do not narrate obvious assignments or returns merely to fill
  space. A trivial function needs only its purpose comment.
- Prefer standard-library algorithms, range iteration, RAII, and value
  initialization when they make intent clearer and preserve bounds, allocation,
  exception, and performance guarantees. Include the headers used directly.
- Validate lengths before indexing or subtracting them, check arithmetic before
  overflow, and preserve transactional failure behavior. Use compile-time
  assertions for relationships that the compiler can verify.
- Keep cleanup behavior-preserving, including serialized formats, diagnostics,
  resource bounds, and public APIs. Run the relevant existing build and tests;
  report any verification that could not be completed.

- Preserve serialization hot-path efficiency during cleanup, including CPU,
  memory, and encoded size.

## Usage documentation and repository skill

- Always keep [README.md](README.md) and the
  [Serializer integration skill](.agents/skills/serializer-integration/SKILL.md)
  current. Review both for every change; update both in the same change whenever
  schema syntax, supported features, public APIs, protocol behavior, build steps,
  or recommended usage changes.
- Update their linked [usage guide](docs/usage.md), wire-format contract, migration
  notes, and skill metadata where affected. Keep detailed examples in the usage
  guide and task instructions in the skill, with working links between them.
- Document implemented behavior separately from proposals. Include relevant
  limitations and verification status; do not present deferred builds or tests
  as completed.
- Keep the skill in `.agents/skills/serializer-integration/` with Agent Skills
  `SKILL.md` frontmatter. Its hyphenated folder/name follows the skill format;
  C++ identifiers and file names continue to follow this repository's conventions.

## Extension versioning

- Always keep both "Rohit Serializer" extensions, for **Visual Studio** and
  **Visual Studio Code**, up to date with the current Serializer code, schema
  syntax, generators, naming profiles, and supported languages. Review both
  extensions' behavior and documentation whenever those change;
  update, rebuild, validate, and install the current local package when requested.
- All supported navigation must work consistently. Treat broken declaration,
  definition, include, and generated-output navigation as regressions. Verify
  qualified names, cursor/selection boundaries, transitive includes, unsaved
  edits, and every supported output language with relevant automated tests.
  Preserve normal editor navigation for unrelated symbols, and document genuine
  prerequisites or unavailable destinations instead of claiming unverified support.

- Every extension-related change must increment the affected extension's version
  in the same change, including code, grammar, snippets, icons, metadata,
  documentation, tests, and build or packaging scripts. Documentation-only and
  behavior-preserving changes are not exempt.
- Increment only the patch/revision component (the third number), for example
  `1.1.0` to `1.1.1`, then `1.1.2`. Keep the major and minor components unchanged
  unless the user explicitly requests a major or minor version increase. Apply
  this rule to both the VS Code and Visual Studio extensions.
- Changes to shared assets or tooling used by both the VS Code and Visual Studio
  extensions must increment both extension versions. Keep extension IDs stable.
- Update the affected manifests, lockfiles where applicable, changelogs, package
  output filenames, and current-version documentation together. Rebuild and
  validate affected packages before distribution; do not reuse an earlier
  version number for changed content.

## Commit comments

- When the user says exactly `show commit comment`, inspect the active Git worktree and any changed nested Git repositories or submodules before composing the response.
- Write a separate detailed Git commit message for each repository that has its own changes. Never combine changes from different repositories in one commit message.
- Put every commit message inside its own plain-text Markdown code fence so the interface displays a copy button.
- When only one repository has changes, return only its code fence with no heading, label, prefix, introduction, explanation, or trailing commentary.
- When multiple repositories have changes, place only the repository name as a Markdown heading immediately before each code fence. Do not add any other prose outside the code fences.
- Describe only changes owned by that repository. In a parent repository, mention a submodule only when the recorded submodule pointer itself is part of that repository's commit.
- Use an imperative subject line followed by a blank line and a detailed body describing the important changes and verification.
- Keep the contents of the code fence ready to paste directly into `git commit`.
- Do not add Conventional Commit prefixes or scopes such as `feat:`, `fix:`, or `chore:` unless the user explicitly requests them.

## Response preferences

- When suggesting a Git commit message, provide it in a fenced code block so it is directly copyable.
- Do not add Conventional Commit prefixes or scopes such as `feat:`, `fix:`, `chore:`, or `feat(component):` unless the user explicitly requests them.
