# Integer-default recognition resource repair — 24 October 2026

Compiler/runtime **2.31.1** replaces the recursive regex used to classify known
integer defaults with a linear, constant-stack scan. Schema language **1.23.0**
and both editor packages **2.0.16** are unchanged. This is a compiler-only repair
within `SC02`; broader validation/bit-field requirements remain open.

## Reproduced defect

The retained 2.31.0 Linux GCC/libstdc++ compiler reports the expected member range
error for 1,024 decimal digits. Inputs containing **65,536** and **262,144** digits
instead terminate with **SIGSEGV (139)** in the recursive recognizer. The Windows
MSVC compiler reports the expected range error for all three controls. No output
artifact is published by these rejected models.

## Preserved contract

Only the existing optional-minus, unsuffixed decimal/hex grammar is classified.
The complete spelling is checked before conversion, preserving native suffixes,
expressions, octal spellings and pre-1.14 default handling. Very long in-range hex
values with leading zeros and negative zero remain accepted. Range errors retain
their original member/path/line/column attribution. No public API, generated name,
class/field identity or serialized format changes.

The scan uses constant stack space and linear work in the input length; this does
not introduce a global schema-memory limit or hard process-peak guarantee.

## Verification

This completes **SC02c** within the parser/resource scope above.

| Check | Executed result |
| --- | --- |
| Windows MSVC 19.51 core | 618 cases execute: 617 pass; formatter-dependent case initially skips without its environment; full-wrapper replay passes the core test with formatter configured |
| Linux GCC 15.2/libstdc++ 15 core | **615/615 pass**, including large canonical failures, long in-range hex, opaque native suffix/expression and old-header controls |
| Standalone invalid-default controls | Both hosts reject 1,024/65,536/262,144 digits with the expected member diagnostic and preserve existing output bytes |
| Integrated CLI regression | Both hosts pass **nine atomic two-output rejections**, nine native/legacy controls and generated Python defaults across all four protocols |
| Generated compatibility | **19 byte-identical files** against compiler 2.31.0: all eleven ordinary generic outputs and eight managed-reference profiles |
| Package version | Both hosts accept the earlier patch/minor requests within major 2 and reject newer/different-major requests |
| GCC installed consumers | Installed headers, capability consumers, compiler/native discovery and C++/C/Python plus named-reference examples pass |
| Full Windows release regression | Running; the source/compiler snapshot is kept unchanged until completion |

Both CLIs report compiler **2.31.1** and schema language **1.23.0**. The original
Windows core formatter omission is distinct from a product failure; the root
wrapper replay supplies the configured formatter and passes the core CTest.
Both editor implementations/listings were reviewed; this compiler-only spelling-
preserving repair requires no editor package change. Build wrappers, public usage
and wire-format metadata remain synchronized.

Baseline executables, comparison hashes, reproducible drivers and logs remain
under ignored `out/build/default-validation-overnight/`. The 262,144-digit CLI
controls complete in approximately 0.02 seconds on Windows and 0.20 seconds under
WSL in these individual runs; those timings are observations, not a throughput or
hard process-peak guarantee. The related portable backend recognizers have
separate defects and are not covered by this parser-only repair.
