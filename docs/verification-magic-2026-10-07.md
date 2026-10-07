# Magic and format omission verification — 7 October 2026

This records checks of the working-tree [magic and omission implementation](magic_and_omission.md).
It describes completed checks, rather than qualification of an immutable release.

## Completed checks

- The retained `test/magic_languages.py` suite regenerated actual bindings and
  compiled handwritten consumers for C++, Java, JavaScript, TypeScript, Go, C#,
  Rust, Python, Swift, Kotlin, and C. All eleven consumers passed.
- Every consumer writes a versioned record with an eight-byte `SRL\0UTF8`
  signature and a separate record whose first JSON field is excluded. The 88
  resulting artifacts agree across all eleven producers and four native formats:
  binary bytes match exactly, and JSON values match independently specified data.
- Checks cover raw embedded NUL bytes before the revision, JSON's fixed `magic`
  value, native header validation, per-format payload omissions, and restoration
  of omitted fields to their declared defaults. JSON omission of the first field
  does not leave a leading comma. The suite retains the existing version migration
  and four-protocol checks in each maintained example consumer.
- All eleven flexible JSON readers accept unrelated additive nested fields while
  rejecting supplied excluded magic (matching, wrong, or duplicate) and excluded
  ordinary fields.
- Each consumer also writes and validates Unicode JSON magic `UTF8é🚀`. Its schema
  uses protected metadata, exercising non-public static declaration generation.
  Generated C# owning classes are sealed, so non-public metadata follows their
  existing private-field convention.
- Native Windows consumers used MSVC C++/C with `/W4 /WX`, Java's release-17
  compiler with warnings as errors, TypeScript strict checking, Go, .NET 8 with
  warnings as errors, Python, and the installed Kotlin JVM compiler with `-Werror`.
  Installed WSL SDKs compiled Rust with `-D warnings` and Swift with warnings as
  errors. Kotlin's compiler printed JVM deprecation notices; compilation succeeded.
- All 78 shared editor tests passed. The Serializer VS Code and Visual Studio
  packages were rebuilt and validated at the synchronized version 1.1.19.

The companion C++ core suite additionally covers exact static char-array types,
every magic-prefix truncation, malformed headers, required JSON identity, and
explicit format exclusions.

## Reproduction

With the required SDKs available, run from the Serializer repository root:

```powershell
python -X utf8 test/magic_languages.py --compiler <build>/Release/serializer.exe --build <build>/magic-languages --language all --wsl-languages rust,swift
npm test --prefix editors/vscode
powershell.exe -NoProfile -ExecutionPolicy Bypass -File editors/visual_studio/build.ps1
```

On Windows, enter a Visual Studio developer environment for C/C++ and set
`SERIALIZER_TSC` and `SERIALIZER_KOTLINC` when those installed SDK launchers are
outside `PATH`. The Kotlin installation used its direct
`org.jetbrains.kotlin.cli.jvm.K2JVMCompiler` entry point. Generated bindings,
consumer copies, and produced artifacts stay in the build directory.

Enabling `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` registers the retained suite as
the `serializer_magic_languages` CTest entry. Protocol-specific Protobuf checks
are separate from this native-language suite.

## Boundaries

The editor packages were not installed, and interactive IDE-host tests were not
rerun. This run does not establish device, minimum-SDK, big-endian, sanitizer, or
throughput qualification. Application-specific pipeline qualification remains
the responsibility of each consuming project.
