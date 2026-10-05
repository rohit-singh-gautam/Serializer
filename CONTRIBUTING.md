# Contributing to Serializer

Contributions to Serializer's compiler, runtimes, generators, examples, and
documentation are welcome. Please follow the [code of conduct](CODE_OF_CONDUCT.md)
in issues, pull requests, and other project discussions.

## Report a problem or propose a change

Search [existing issues](https://github.com/rohit-singh-gautam/Serializer/issues)
before opening a report. Use the bug report or feature request template and keep
each issue focused on one problem. For substantial API, schema, or wire-format
changes, discuss the proposed behavior in an issue before implementing it.

For bugs, include the Serializer revision, operating system, toolchain, output
language, generator configuration, and protocol. A minimal `.serializer` schema,
consumer, exact commands, and expected versus actual output make a report easier
to reproduce. Remove credentials, personal data, and private application details.

Report suspected vulnerabilities privately using the [security policy](SECURITY.md),
instead of putting exploit details in a public issue or pull request. For barriers
to using the project, see the [accessibility statement](ACCESSIBILITY.md).

## Set up a development checkout

Fork the repository, clone your fork, and create a branch for your change.
The default development build requires CMake 3.28+, a C++20 compiler and standard
library, clang-format 19+, and GoogleTest. Follow the
[build requirements](docs/cmake_integration.md#build-this-repository) to install
dependencies and select a toolchain.

From the repository root, the existing test wrappers configure and build the
enabled CMake targets before running CTest:

```sh
# Linux with GNU Make
make test
```

```powershell
# Windows with PowerShell
./make.ps1 test
```

See the [build and generation guide](docs/build_and_generation.md#build-and-test)
for direct CMake commands, presets, and optional build features. Use the
[qualification guide](qualification/README.md) for interoperability, fuzzing,
sanitizers, and performance checks relevant to your change.

## Make a focused change

- Follow [CodingStandard.md](CodingStandard.md) and [AGENTS.md](AGENTS.md).
  New C++ identifiers and files use lowercase `snake_case`, `.hpp`/`.cpp`, and
  two-space indentation. Preserve existing public include paths and APIs unless
  their migration is explicitly part of the change.
- Preserve schema identifiers, field IDs, wire values, diagnostics, resource
  limits, and transactional failure behavior during cleanup. Assess CPU cost,
  allocations, and encoded size when changing serialization hot paths.
- Edit generator inputs or implementation and regenerate through the real build
  pipeline. Do not fix generated output by hand or commit incidental build output.
  Preserve explicitly versioned provider artifacts required by the repository.
- Add regression coverage for changed behavior and relevant malformed or boundary
  input. For generator changes, generate, compile, and exercise the affected
  language output. State which platforms and optional features you checked.
- Review the [README](README.md) and
  [integration skill](.agents/skills/serializer-integration/SKILL.md) for every
  change. Update both when syntax, supported features, public APIs, protocols,
  build steps, or recommended usage changes. Update affected usage, wire-format,
  and migration documentation too. Distinguish implemented behavior from proposals.
- Follow the synchronized versioning and verification requirements in
  [AGENTS.md](AGENTS.md#extension-versioning) for any editor extension changes.

## Submit a pull request

Open a pull request against `main` and fill in its template. Explain the problem,
the resulting behavior, related issues, and any compatibility or performance
impact. Keep unrelated cleanup in a separate change.

List the exact checks you ran and their results. For code changes, run the
relevant build and tests; identify anything you could not verify and why.
Documentation-only changes need a link, consistency, and whitespace review rather
than a compiler build. Run `git diff --check` before submitting.

Use a descriptive imperative commit subject, followed by details where useful.
Conventional Commit prefixes are not required. Respond to review comments and
rerun affected checks after revisions. Contributions are made under the
repository's [license](LICENSE).
