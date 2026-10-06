# Payload versioning verification - 6 October 2026

This record covers the working-tree implementation of [payload versioning](versioning.md)
on source baseline `fbc7718c0cf47bbe93a5bef7521f0924a3685080`. It describes
completed checks, rather than qualification of an immutable release.

Release dates and compile-time policies were implemented after this run. See the
[subsequent policy verification](verification-release-policies-2026-10-06.md) for
that implementation and editor version 1.1.17.

## Completed checks

- The full configured Release build passed with Visual Studio 2026 on Windows x64.
  All 68 configured CTest entries passed, including all 295 core tests.
- Nine versioning tests cover independent positional fixtures for revisions 8, 9,
  and 10, historical keyed binary, JSON policy/lifecycle validation, dotted ordering,
  malformed discriminators and schema contracts, and default ID allocation.
  Schema compatibility tests also cover retained historical layouts and embedded
  reservations.
- Handwritten examples passed in C++, Java, JavaScript, TypeScript, Go, C#, Rust,
  Python, Swift, Kotlin, and C. Each reads historical revision 8, migrates its retained
  replacement explicitly, and round-trips revision 10 through JSON, positional
  binary, integer-key binary, and string-key binary.
- Each example also checks independent positional bytes and all four protocols for
  uint16/32/64, float/double, and version2/3/4. Cases include binary32 `0.1`, maximum
  uint64, custom discriminator identity, and protected/private access.
- Existing native boundary suites passed all 1,029 cases each in Python, Rust,
  Swift, Kotlin, and C. These are codec regressions, not 1,029 distinct versioning cases.
- Unversioned fixed-field output retained the five checked positional input branches
  and all 18 batching calls against the prior generator build. Independent byte
  fixtures also passed. No throughput benchmark was run for this change.
- Embedded runtime string limits passed after byte-preserving splits of the Go
  and JavaScript runtime literals.
- Both editor packages were rebuilt and validated at version 1.1.16. All 74 shared
  editor tests and 13,376 fresh-compiler navigation checks passed across all eleven
  output languages, including the new versioned schema. Visual Studio's actual
  .NET resolver passed 89 checks, including selection boundaries, unsaved includes,
  generic/dimension references, and cancellation.

## Reproduction

Run from the repository root with the required SDKs available:

```powershell
cmake --build out/build/make-Release --config Release --parallel 4
ctest --test-dir out/build/make-Release -C Release --output-on-failure --parallel 4
python example/run.py --compiler out/build/make-Release/Release/serializer.exe --example versioning --language all --wsl-languages rust,swift
```

Rust and Swift used installed WSL SDKs; the other consumers used native Windows
SDKs. Kotlin used the installed compiler's direct JVM entry point because the local
launcher could not load its preloader. `SERIALIZER_KOTLINC` and `SERIALIZER_TSC`
selected those installed tools. Generated output stays under `out/`.

| Artifact | SHA-256 |
| --- | --- |
| Tested `serializer.exe` | `965C7E41F9C2247B4170A00984B7D41E7ACA706E6D8CDD61838B6364CD887D7F` |
| Shared versioning example schema | `3CE9FD8231618BA1306AFE5F2047C16AC130BDF478AC2EAD83DFA4F679EA2466` |
| C++ versioning fixture schema | `AE84CDA411C74FC88C1DA4D3BC61EF6F2DE8DB853F10408D477524137E40FF81` |

## Boundaries

Versioned views, managed models, and Protobuf mappings remain unsupported and
produce diagnostics. Version dates and time-based expiry policies are proposals,
not implemented schema syntax. Retained definitions describe old bytes; replacement
conversion remains application code.

The extension packages were not installed for this change, and interactive IDE-host
tests were not rerun. This run does not establish big-endian, sanitizer, every-platform,
or throughput qualification for versioning. See [the feature contract](versioning.md)
and [editor coverage](editor_navigation.md#navigation-coverage-matrix).
