# Serializer Codex Instructions

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
