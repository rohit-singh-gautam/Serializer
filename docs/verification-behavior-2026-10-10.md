# Behavior verification — 2026-10-10

This record covers uncommitted behavior enhancements over baseline
`f9a0730fc845c87f7c361d5f64462761510cc27f`. It qualifies schema functions,
native fragments, external definitions, behavior interfaces and portable
expressions. Managed runtime/history qualification is tracked separately.

## Real SDK execution

Both runners passed **11/11 languages**: C++, Java, JavaScript, TypeScript,
Go, C#, Rust, Python, Swift, Kotlin and C. C/Rust/Swift ran in WSL; the other
SDKs ran on Windows. TypeScript checked declarations with strict typing and
executed the generated JavaScript implementation.

```powershell
python test/behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all --wsl-languages c,rust,swift
python test/native_behavior_languages.py --compiler out/build/make-Release/Release/serializer.exe --language all --wsl-languages c,rust,swift
```

C++ used C++20, optimization and warnings as errors, with exceptions enabled.
C used C11, optimization and strict warnings. Java used release 17 and
`-Xlint:all -Werror`; C# used .NET 8 and warnings as errors; TypeScript used
`--strict`; Rust, Swift and Kotlin also treated warnings as errors. Go used
`go build`; JavaScript and Python executed their generated consumers directly.
The maintained [runner](../example/run.py) contains the exact commands.

| Qualification | Passed checks |
|---|---|
| Portable arithmetic | Grouping, unary signs, binary arithmetic, `math.pi`, exact signed/unsigned 32-bit conversion, boundary values, nonfinite input and overflow |
| Dynamic numeric types | Boolean/fractional/out-of-range integer rejection; Python binary64 conversion even when fields contain Python integers |
| Native behavior | Target bodies, external definitions, edit receivers, virtual/override bodies and separate abstract behavior adapters |
| Native hooks | File preambles, C++ ordered access/friend/type declarations, Kotlin imports; target-specific hooks used only where supported |
| Missing C++ definition | An otherwise-unused external method failed linking when the generated qualification hook was invoked |
| Static required implementation | C, C#, Go, Rust and Swift rejected a removed external definition; Java/Kotlin rejected incomplete typed behavior implementations |
| Dynamic required implementation | JavaScript/Python rejected missing callable attachments before invocation |

Fixtures are [portable](../test/resources/document_behavior.serializer),
[native](../test/resources/document_native_behavior.serializer) and
[Go native](../test/resources/document_native_behavior_go.serializer).
Application implementations remain outside generated files. Negative SDK tests
inspect diagnostics for the missing method, rather than accepting any failure.

## Complete-unit formatting and mappings

A refreshed compiler with the final metadata-escaping fix passed a real CLI
probe with **clang-format 21.1.6** for all nine C++ profiles. JSON-unescaping
restored the canonical schema filename, including spaces, exactly. Every
`generated_line` matched the next physical output line. Native literal tokens,
friend/access order and generated reset directives were preserved.

The probe used `document metrics with spaces.serializer`. Its four final anchor
positions were:

| Profile | Final generated lines |
|---|---|
| serializer, core, llvm, cert, misra, autosar | 29, 42, 51, 59 |
| google | 28, 41, 50, 58 |
| gnu, qt | 29, 43, 52, 60 |

The [core regression](../test/behavior_test.cpp) also covers formatting twice,
JSON decoding of a path with spaces, literal placeholder preservation and
mapping spans independent of warning emission. The SDK matrices preceded the
last mapping-only change; runtime/emitter semantics were unchanged by it.

## Source fingerprint

For the listed behavior files, sorted relative path, NUL, UTF-8 contents with
CRLF normalized to LF, then NUL were hashed together using SHA-256:

`7c82d12c4dda7f234b1fbe0012f9bba19a1a4d9e3bfcb7a96f1c85d7bf1cef77`

```text
include/rohit/serializer_creator.hpp
src/behavior_parser.hpp
src/behavior_writer.hpp
src/c_writer.cpp
src/cpp_naming.cpp
src/cpp_naming.hpp
src/cpp_writer.cpp
src/java_writer.cpp
src/mobile_writer.cpp
src/parser.cpp
src/portable_writer.cpp
src/python_writer.cpp
src/rust_writer.cpp
src/schema_generics.hpp
test/behavior_languages.py
test/behavior_test.cpp
test/native_behavior_languages.py
test/resources/document_behavior.serializer
test/resources/document_native_behavior.serializer
test/resources/document_native_behavior_go.serializer
```

This fingerprint identifies the behavior-specific source slice, not the entire
release. SDK output and formatting probes remain in ignored `out/` directories;
no generated artifacts were added to source control.
