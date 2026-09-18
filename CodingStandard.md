# Serializer Coding Standard

## Scope

This document defines coding conventions for Serializer. Build commands,
directory layouts, language versions, and task instructions belong in this
repository's `AGENTS.md` and README.

- Keep this document and `AGENTS.md` consistent when changing conventions.
- Keep public code and documentation grounded in Serializer and public sources.
  Do not import private-project ideas, quotations, names, paths, or implementation
  details into this repository.
- This standard supersedes older local style guidance, including Serializer's
  former PascalCase functions and camelCase variables. Explicit user instructions
  and required language, framework, or external-interface contracts take priority.
- Apply the standard to new code and substantial edits. Migrate existing code
  in focused, reviewable changes with affected references and tests updated
  together. Adopting this document does not mean all existing code already complies.
- Preserve existing public symbols, include paths, generated identifiers, schema
  spelling, and wire values until a task explicitly includes their migration.

## File names and layout

- Use descriptive lowercase `snake_case` for new C++ source files, headers, and
  source directories: `decoder_io.cpp`, `fixed_string.hpp`, and
  `serializer_parser_test.cpp`. Avoid spaces, case-only distinctions, and vague
  names such as `misc` or `new_helpers`.
- Use `.hpp` for new C++ headers and `.cpp` for implementations. Use `.h` and `.c`
  for C interfaces and implementations. Existing C++ `.h` headers remain valid
  compatibility paths until an intentional migration updates their consumers.
- Give a header and implementation the same base name when they represent the
  same component. Split large implementations by responsibility when useful.
- Follow the adopting repository's established source, include, test, and fixture
  directories. Use `<component>_test.cpp` for new C++ test files. Preserve input
  formats and required extensions such as `.serializer`.
- Keep generated build output in the build directory. Modify the generator or
  its inputs and regenerate through the real build pipeline.
- Preserve tool-required and established document names such as `CMakeLists.txt`,
  `CMakePresets.json`, `README.md`, `AGENTS.md`, and `CodingStandard.md`. Other
  languages and documentation collections follow their framework or local naming
  requirements rather than adopting C++ extensions or spelling mechanically.
- For an intentional rename, update includes, build rules, dependencies, tests,
  documentation, scripts, and generator references in the same change. Check
  exact path casing for portability to case-sensitive filesystems.

## C++ identifier naming

Java source and generated Java use Java-specific presentation profiles rather than
the C++ identifier rules below. Use UpperCamelCase types, lowerCamelCase fields and
methods, and UPPER_SNAKE_CASE constants in maintained Java code. See
[Java profiles](docs/java.md#java-coding-styles) for generated layout options.

Use names consistent with Serializer's public API and generated protocol methods.

| Entity | Convention | Examples |
| --- | --- | --- |
| Classes, structs, aliases, concepts | `snake_case` | `full_stream`, `json_out` |
| Functions and methods | `snake_case`, preferably a verb phrase | `parse_identifier`, `serialize_out` |
| Enums and enum members | `snake_case` | `serialize_type::out` |
| Variables, parameters, data members | `snake_case` | `input_stream`, `start_offset` |
| Constants | Descriptive `snake_case` | `buffer_growth_factor`, `identifier_hash_seed` |
| Namespaces | Lowercase `snake_case` | `rohit::serializer`, `type_check` |
| Template type parameters | Descriptive `PascalCase` or conventional short names | `ValueType`, `UInt`, `T` |
| Template value parameters | Follow the established generic parameter family | `N`, `Name` |
| Required macros | Project-prefixed `UPPER_SNAKE_CASE` | `SERIALIZER_BUILD_TESTS` |

- Prefer C++ constants and functions over introducing ordinary macros.
- Preserve standard and external-interface names such as `begin`, `end`,
  `value_type`, operators, and inherited method names.
- Choose names that express purpose. Prefer `is_`, `has_`, `can_`, or `should_`
  for boolean predicates. Include units for sizes, durations, and offsets.
- Avoid Hungarian type prefixes and redundant category prefixes on new names.
  Use semantic distinctions such as `input_` and `output_` where useful.
- Avoid reserved identifiers, double underscores, and new leading-underscore
  project names. Short indices and generic type names are acceptable in small,
  obvious scopes.

## Formatting

Use two spaces for new C++ files; preserve the surrounding indentation during
localized edits to existing files.

Apply the same two-space block indentation to `.serializer` source. Put each
namespace, class, and enum body on separate lines, indent nested declarations,
and put each member or enum value on its own line. Keep opening braces on the
declaration line and closing braces on their own lines. Inline field-default
braces remain part of the member declaration. Preserve schema identifiers,
IDs, quoted literal contents, and intentional malformed test input.

- Use spaces, not tabs. Place opening braces on the declaration or control
  statement line and closing braces on their own line. Use `} else {`.
- Use braces for new conditional and loop bodies. Existing concise one-line
  functions may retain their local style when their purpose is clear.
- Use spaces after control keywords, around binary operators, and after commas.
  Attach pointer and reference symbols to the type in new declarations:
  `const stream& input_stream` and `base* parent`.
- Aim for lines within 100 columns; wrap at logical boundaries. Preserve literal
  data or required generated text when wrapping would obscure or alter it.
- Separate logical sections with a single blank line. Remove trailing whitespace
  in edited lines, end files with a newline, and preserve existing line endings
  and license notices. Do not copy licensing terms from another repository.
- Keep mechanical formatting separate from behavior changes when practical.

## Constants, bounds, and performance

- Replace magic numbers and repeated semantic strings with descriptive named
  constants in the narrowest useful scope. Share constants when components
  implement the same contract; include units in their names.
- Derive dependent values from their source of truth using `sizeof`, `std::size`,
  container `.size()`, `std::numeric_limits`, and constant expressions. Prefer
  deduced array extents and template arguments when the intended type is preserved.
- Keep externally specified wire values explicit and independent of platform
  layout. Do not serialize native object layout as a substitute for a wire format.
- Keep obvious arithmetic identities, loop origins, empty states, and direct
  comparison values inline when naming adds no meaning. Preserve intentional
  literal data in logging examples and tests.
- Use `constexpr` for compile-time constants, `const` for immutable runtime
  values, scoped enums for closed state sets, and unsigned fixed-width types
  where required by a wire contract. Avoid macros for ordinary constants.
- Validate lengths before indexing or subtraction and check arithmetic before
  overflow. Use compile-time assertions for relationships the compiler can verify.
  Preserve transactional failure behavior and documented resource bounds.
- Preserve serialization hot-path efficiency, including CPU, memory, and encoded
  size. Assess affected hot paths when changing allocations, copies,
  synchronization, formatting, or I/O.

## Functions and documentation

- Give each function one clear responsibility. Prefer guard clauses and meaningful
  helpers to deep nesting; use judgment instead of arbitrary function-length limits.
- Add a concise purpose comment to every function. Document relevant preconditions,
  return and failure behavior, ownership, lifetime, thread-safety, and side effects.
  Keep declaration and definition comments complementary rather than duplicated.
- Explain non-obvious algorithms, invariants, boundary checks, synchronization,
  and unusual decisions inside functions. Keep comments accurate; avoid narrating
  obvious assignments or returns. A trivial function needs only its purpose comment.
- Use `const` for unchanged values and const-qualified methods for operations that
  do not modify observable state. Pass small values by value and expensive read-only
  objects by const reference where appropriate. Do not use `const` to prevent an
  intended move. Top-level `const` on by-value declarations is unnecessary.
- Use `auto` when the initializer makes the type clear or avoids repetition.
  Keep explicit types where widths, conversions, ownership, or public contracts
  matter. Use deduced returns and constrained `auto` parameters when a deduced
  or generic interface is intended, not as a blanket rule.
- Use `constexpr` where compile-time evaluation is meaningful. Add `noexcept`
  only when the implementation supports that guarantee, and `[[nodiscard]]` when
  ignoring a result would likely lose required error handling or useful work.
- Keep related stateless functions in a namespace. Use classes for state,
  ownership, invariants, or cohesive abstractions.

## Types, ownership, and headers

- Keep classes focused and members private unless public access is part of the
  intended interface. Use structs for plain data with intentionally public members.
- Prefer standard-library algorithms, range iteration, RAII, and value
  initialization when they clarify intent and preserve bounds, allocation,
  exception, and performance guarantees.
- Prefer value semantics and automatic resource management. Use smart pointers
  for ownership when dynamic allocation is needed. Document the lifetime of
  borrowed pointers, references, spans, and string views.
- Prefer the rule of zero: let members manage resources and special member
  functions. When custom ownership requires them, define or delete copy and move
  operations consistently. Mark overriding virtual functions `override`.
- Keep the existing header protection style. Use `#pragma once` for new
  headers unless the component's portability requirements call for guards.
- Include directly used dependencies and remove duplicates. Prefer C++ headers
  such as `<cstring>` over C compatibility headers in C++ implementations.
- Include the corresponding component header first when one exists, then related
  project headers, third-party headers, and standard headers. Group and sort
  includes where ordering has no semantic requirement.
- Do not put `using namespace` directives in headers. Keep implementation helpers
  private unless templates or compile-time use require header definitions.

## Verification and adoption

- Generated C++ defaults to this standard through the `serializer` output profile.
  Keep its built-in formatter settings aligned with `.clang-format`. Alternative
  output profiles and their example consumers follow the documented target rules;
  generator/runtime implementation code continues to follow this repository's rules.
  See [output configuration](docs/output_configuration.md). Keep wire names and IDs
  independent of target-language naming, and validate naming collisions before emission.

- Follow each repository's declared language baseline and supported platforms.
  Preserve serialized formats, diagnostics, resource limits, and public APIs during
  cleanup. Avoid disabling warnings to make a style change pass.
- Run relevant existing builds and tests for code changes. Add meaningful coverage
  for changed behavior and affected malformed or boundary input. For generator
  changes, regenerate, compile, and exercise actual generated output.
- Check renamed paths and symbols across build files, consumers, fixtures, and
  documentation. Do not silently change emitted keys or protocol identifiers
  when renaming an implementation symbol.
- Review documentation-only changes for consistency and whitespace; a build is
  not required. Report verification that could not be completed.
- Review this standard and `AGENTS.md` together after a convention change.
  Documentation changes do not automatically migrate existing code.
