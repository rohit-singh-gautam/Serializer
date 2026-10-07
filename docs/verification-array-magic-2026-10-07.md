# Inferred arrays and typed magic verification — 7 October 2026

These features were introduced for compiler/runtime **1.2.0**, schema language **1.1.0**,
and synchronized editor extensions **1.1.24**. The original integer language header
continues to mean exactly `1.0.0`; existing explicit arrays and byte magic preserve
their contract.

## Editor checks

The shared TypeScript build and all **82** VS Code unit/provider tests pass.
New grammar coverage tokenizes inferred empty extents, fixed-array element types,
typed scalar/enum magic, and adjacent enum defaults with the real TextMate/Oniguruma
engine. New shared navigation coverage checks qualified enum cursor boundaries,
transitive includes, unsaved edits, and exclusion of literal/metadata tokens from
schema navigation.
Numeric separator regressions also verify that apostrophes inside C++ numeric
defaults do not open character literals or hide later namespace declarations.

The fresh **1.2.0** compiler navigation harness passes **16,881** bidirectional
checks across all eleven output languages. It generates the typed magic fixture
in every backend and the inferred-array fixture in C++, checks enum operand and
initializer cursor boundaries, and retains transitive includes, naming profiles,
managed outputs, and identical legacy/dotted-header output coverage.

Both VSIX packages were rebuilt at **1.1.24**. VS Code packaging refreshes its
canonical grammar, snippets, and compiled extension. The Visual Studio package
validation script passes, including the current manifest and shared assets.
The Visual Studio resolver regression includes typed magic and inferred-array enum
operands, boundary selections, and enum definitions in every supported output
language. The real .NET/Jint resolver passes **165** source/output checks against
the freshly generated complex/generic fixtures, covering all eleven outputs,
typed metadata, numeric separators, cursor boundaries, unsaved includes,
dimensions, and cancellation. It uses the fixtures from the fresh compiler
navigation harness above.
Extension installation and native interactive IDE checks were not requested for this change.

## Generated language codecs

The `test/typed_magic_languages.py` harness passes for **ten** generated outputs:
C++, Java, JavaScript, TypeScript, Go, C#, Rust, Python, Swift, and C. Each output
checks **15 typed headers**, **77 malformed fixtures**, and round trips in native
JSON, positional binary, integer-key binary, and string-key binary. The cases
cover scalar and enum constants, boundary values, zero/false signatures,
omissions, and validation failures.

All ten outputs produce identical bytes in each of the three binary modes.
Positional bytes are additionally pinned against an independent Python
`struct.pack` construction, rather than only compared between generated codecs.
The final result and binary hashes are retained in
`out/typed-magic-languages/verification.log`.

| Execution environment | Verified generated outputs |
| --- | --- |
| Windows | C++, Java, JavaScript, TypeScript, Go, C#, Python |
| WSL | Rust, Swift, C |
| Generation and inspection only | Kotlin; no Kotlin compiler is installed |

Kotlin constants and codecs were emitted and inspected, including boundary-value
representations. Kotlin runtime compilation and execution remain unverified.

The existing `test/magic_languages.py` harness also passes for the same ten
outputs. It verifies unchanged legacy byte magic, format omissions, UTF-8
metadata, flexible JSON exclusions, and payload-version migration. Its results
are retained in `out/magic-languages/verification.log`.

## Native core build and tests

Final native verification uses a frozen source copy in
`out/array-magic-verification/source`, with its own CMake build directory. Concurrent
compact-integer work had advanced that copy to compiler/runtime **1.3.0** and
schema language **1.2.0**; array inference and typed magic still require only **1.1.0**.
The compiler's `--version` output confirms both supported versions.

The clean MSVC 19.51 Release build passes with C++20, managed runtime and optional
compression disabled, and style/iostream examples enabled. The full CTest run
passes 45 of its 46 registered checks, including all Protobuf, package-version,
installed-consumer, include, CLI, dimension-profile, and example checks. Its sole
failure belongs to concurrently added compact generic validation. After importing
that task's parser fix into the copy and rebuilding the core target, all **354**
core tests pass. All 46 registered checks therefore pass across the suite and
the final core rerun; the other 45 checks were not repeated after that parser fix.

The core tests exercise inferred text, numeric, enum, string, class, and generic
defaults; exact cardinality; malformed/bounded initializers; per-file language
gates; typed scalar/enum headers; omissions; endian behavior; and legacy JSON
text adapters. Compatibility tests check equivalent explicit/inferred arrays and
retain diagnostics for changed values/extents and unevaluated host expressions.
Protobuf tests additionally validate typed presence, zero/false constants,
duplicates, and preserved legacy scalar occurrence behavior.

Retained evidence is in `out/array-magic-verification/configure.log`, `build.log`,
`ctest.log`, `final-core-build.log`, and `final-core-tests.log`. The frozen build
and its input copy are retained for reproducibility. Earlier
[byte-magic verification](verification-magic-2026-10-07.md) and
[fixed-array qualification](verification-dimensions-2026-10-04.md) describe their
identified snapshots.
