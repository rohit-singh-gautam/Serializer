# Compile-time release-policy verification - 6 October 2026

This record covers the working-tree implementation of
[release policies](versioning.md#release-dates-and-compile-time-policies) on source
baseline `a800527`. It records completed checks, rather than qualification of an
immutable release. Git release-date population is outside this implementation.

## Completed checks

- The full configured Release build passed with Visual Studio 2026 on Windows x64.
  All 68 configured CTest entries passed, including all 306 core tests.
- Eleven new core tests cover every policy leaf, nested allow unions/intersections,
  inside-tree compatibility and outside-tree overrides, ordered/gapped catalogs,
  inclusive dates, month-end/leap-day arithmetic, expired-current warnings,
  warning promotion, all nine discriminator types, generic lowering, malformed
  contracts, truncated input, and tree depth/size limits.
- Policy-based generation produces byte-for-byte identical source to the equivalent
  explicit compatibility floor in all eleven backends. No release catalogs, date
  calculations, clocks, or policy objects reach generated codecs. This establishes
  no additional generated release processing; no throughput benchmark was run.
- CLI checks cover pinned and default reference dates, advancing age-based bounds,
  malformed dates, warnings and warning promotion before output replacement, and
  using the same pinned options in schema compatibility checks.
- Real Ninja builds verify all three CMake helpers forward the pinned reference date
  and warning-promotion flag, producing the expected minimum in C++, Java, and
  Python. Existing direct/transitive include and unchanged-build checks passed.
- Handwritten versioning examples passed in C++, Java, JavaScript, TypeScript, Go,
  C#, Rust, Python, Swift, Kotlin, and C using the updated shared release-policy
  schema. Each reads revision 8, migrates its replacement explicitly, and round-trips
  revision 10 through JSON, positional binary, integer-key binary, and string-key
  binary. Existing explicit discriminator fixtures also passed in each language.
- Both editor packages were rebuilt and validated at version 1.1.17. All 76 shared
  editor tests and 13,376 fresh-compiler navigation checks passed across all eleven
  output languages. The Visual Studio .NET resolver passed 89 checks, including
  selection boundaries, unsaved includes, generic/dimension references, and cancellation.

## Reproduction

With the required language SDKs and Visual Studio developer environment available,
run from the repository root:

```powershell
cmake --build out/build/make-Release --config Release --parallel 4
ctest --test-dir out/build/make-Release -C Release --output-on-failure --parallel 4
python example/run.py --compiler out/build/make-Release/Release/serializer.exe --example versioning --language all --wsl-languages rust,swift --version-policy-as-of 2026-10-06
```

Rust and Swift used installed WSL SDKs; other consumers used native Windows SDKs.
`SERIALIZER_KOTLINC` selected the installed Kotlin compiler's direct JVM entry point,
and `SERIALIZER_TSC` selected the installed TypeScript compiler. See the
[earlier versioning record](verification-versioning-2026-10-06.md#reproduction) for
the SDK setup. Generated output and rebuilt VSIX packages stay under `out/`.

| Artifact | SHA-256 |
| --- | --- |
| Tested `serializer.exe` | `41541E9F8A445CB647A3C1DC17B7C4131B9E58E3C21C89FD8A54927481B7D992` |
| Shared versioning example schema | `200CCD93A22E23710F9AE91D6A2BE5BE9C16218CB783B5FD592A5C6791346F3F` |
| VS Code 1.1.17 VSIX | `237824BD701524942BD5BBB63C54EBB689595C14A07C7696A08BB53F5DA17F1E` |
| Visual Studio 1.1.17 VSIX | `88B0F2F5291163E31A2E2414A615DF7050160CD953C9B69DC91124F83BD9EF0C` |

## Boundaries

Policies fold to a version interval, not a runtime release whitelist. Age-based
bounds change only on regeneration and rebuild; an incremental build does not
regenerate just because the current date changes. The current version remains
readable even when time leaves are exceeded; warning promotion can fail generation.

Versioned views, managed models, and Protobuf mappings remain unsupported. The
eleven-language versioning examples were rerun; the separate 1,029-case native
boundary suites were not repeated for this compiler-only change. Extension packages
were not installed, and interactive IDE-host tests were not rerun. This run does not
establish big-endian, sanitizer, every-platform, or throughput qualification.
