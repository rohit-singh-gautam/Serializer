# Portable literal recognition resource repair — 24 October 2026

Compiler/runtime **2.31.2** repairs recursive backend literal/identifier
recognition. Schema language **1.23.0**, all public APIs, generated identities and
editor packages **2.0.16** remain unchanged. This is a separate `SC02` resource
slice following the [integer-parser repair](verification-integer-defaults-2026-10-24.md).

## Reproduced controls and preserved scope

The retained Linux GCC/libstdc++ 2.31.0 compiler terminates with **SIGSEGV (139)**
for seven isolated controls: Python string/floating/legacy-integer/identifier and
Java string/floating/legacy-integer. String and identifier inputs contain 32,768
bytes; numeric controls contain 65,536 digits. The same Java identifier control
already succeeds and remains unchanged.

Shared iterative recognizers preserve the exact portable quoted-string escape
set, optional-minus canonical decimal grammar and each identifier alphabet.
Backend conversions still enforce their existing widths and underflow policy.
Native expressions, hex/octal defaults and legacy declaration provenance retain
their previous acceptance/rejection behavior. Typed aggregate binding uses the
same portable literal function before backend selection, including C++.

The change bounds recognizer stack use and makes its work linear in input length.
It does not establish a global schema-memory bound, SDK token-size expansion or
hard process-peak guarantee. In particular, MSVC 19.51 rejects the retained 2,048-
and 32,768-fraction-digit SDK controls with C1064. The common SDK workload uses
1,024 string bytes and 128 fraction digits; Python/Java/Node have a separately
executed 32,768-byte/digit profile. C's strict string-literal limits are not widened.

## Verification

This completes **SC02d** within the compiler/backend resource scope above.

| Check | Executed result |
| --- | --- |
| MSVC 19.51 core / constructors | **624/624** core and **25/25** constructor tests pass, including actual generator and typed-aggregate controls |
| GCC 15.2/libstdc++ 15 core / constructors | **621/621** core and **25/25** constructor tests pass through the fixed final driver |
| Grammar and large-input controls | Three cases compare five prior regex contracts over bounded spellings, exercise 262,144-byte inputs and retain quoted-byte/escape behavior |
| Former backend crash controls | All eight Windows/Linux controls return their expected success/error; accepted outputs match the baseline, errors preserve existing files |
| Reduced-stack Linux | All eight backend controls complete under a **512 KiB stack** with their expected outcome |
| Atomic CLI / Python runtime | Both hosts pass nine located scalar rejections, one large invalid-string backend rejection, nine native/legacy controls and actual Python defaults across four protocols |
| Common actual SDK matrix | **All eleven Windows SDKs pass**, baseline and repaired output, independently checked defaults and all four protocols |
| Larger actual SDK profile | Windows Python/Java/JavaScript/TypeScript and Linux Python pass 32,768-byte strings/fraction digits across four protocols |
| Generated artifact comparison | **41 byte-identical comparisons**: 12 common, five larger-profile, 12 large all-output Windows and 12 large all-output Linux files, including paired TypeScript runtime modules |
| Release metadata | Both CLIs report **2.31.2 / language 1.23.0**; both generated package-version contracts pass older-patch/minor and future/different-major controls |

The first added TypeScript test incorrectly expected runtime initializers in a
declaration file; the corrected case checks declarations and the paired
JavaScript runtime separately. The original Windows failure is retained. The
initial Linux scratch driver was edited while Bash was running and stopped after
building; the frozen final replay executes all core and constructor cases.
WSL lacks `javac`; Linux Java generation is byte-identical to the Windows JDK 17
fixture, but no Linux Java SDK execution is claimed. The Windows SDK inventory is
Python **3.14.3**, Microsoft OpenJDK **17.0.18+8**, Node **22.23.1**, TypeScript
**7.0.2**, Go **1.26.4**, Rust **1.99.0**, .NET SDK **10.0.402** targeting net8.0,
Kotlin **2.2.21**, Swift **6.4** and MSVC **19.51** for C/C++. The retained
`windows-sdk-versions.log` and SDK commands identify those installed toolchains.
Linux uses Ubuntu GCC **15.2.0-16ubuntu1**, libstdc++ 15 and Python **3.14.4**.

The unchanged 2.31.1 source passed the full **314-test** Windows suite before this
patch. Version 2.31.2 uses the focused gates above; that earlier full run is not
presented as a full 2.31.2 run. Both editors/listings were reviewed; generated
bytes, syntax and navigation contracts do not require a package change.
Broader SC02 validation/bit fields and cross-platform/provider qualification stay
open.

Reproducible baseline binaries, controls, SDK setup failures/corrections and logs
remain under ignored `out/build/portable-literals-overnight/`. Use the maintained
[compiler literal qualifier](../../qualification/compiler_literals.py) for fresh
generation, independent default values and four-protocol SDK checks.
