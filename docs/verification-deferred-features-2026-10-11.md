# Derived wire metadata and portable conversion qualification

Compiler/runtime 1.14.0 and schema language 1.9.0 add derived native binary
record sizes, complement omission lists, explicit portable record conversions
and stateless generic layout constraints. The work starts from upstream
revision `caf1420`. Earlier compatible schemas retain their meaning and default
encoded bytes.

## Selected capabilities

| Capability | Selection |
|---|---|
| Derived record size | Used: fixed uint32/uint64 fields, total containing-record bytes including the size field, source-preserving encoding and bounded decoding |
| Existing native binary protocols | Used: positional, integer-key and string-key framing, including nested generated records |
| Complement omission | Used: comma-separated protocol lists resolve through existing omission metadata |
| Class-specific output configuration | Used: decoder size verification or retention, with unchanged wire meaning |
| Size storage/scope | Used: wire-only metadata and record/payload scopes, including actual consumed key widths |
| Compile-time C++ images | Used: owning and borrowed positional output with nested derived headers and runtime byte parity |
| Portable record conversion | Used: named explicit projections, exact types, fresh target defaults and ownership-preserving copies |
| Generic layout | Used: stateless constraints checked for defaults, concrete instances and forwarded parameters; active state remains ineligible |
| Existing generation pipeline and editor support | Used: real CMake compiler/runtime build, regenerated SDK output, grammar/navigation and both editor packages |
| New buffer ownership framework | Not applicable: existing streams, bounded counters and generated lifecycle APIs provide the required ownership |
| Application dependency adoption | Not applicable: this is an upstream-only change |

## Verification

The following checks have completed against generated 1.14.0 output:

| Check | Result |
|---|---|
| Windows native regression | All 108 CTest entries passed through `./make.ps1 test -Jobs 4`, including installed-package and version-compatibility checks |
| Native core tests | 492 passed, including annotation validation, compatibility, conversion ownership and stateless constraints |
| Derived-size SDKs | All eleven backends pass both `verify` and `retain` (22 language/policy combinations) |
| Conversion SDKs | All eleven pass exact/public projections, target defaults, generic targets and independent copies |
| Stateless SDKs | All eleven pass default/forwarded owning APIs, active transient preservation and four native protocols |
| Compile-time C++ output | MSVC, GCC and Clang pass static byte assertions, ordinary bounded decode, runtime byte parity and output bounds; GCC and Clang use strict C++20 warnings |
| Count-only size bounds | Allocation-free tests accept the uint32 limit and reject one byte above it, including payload-scope header exclusion |
| Editor unit tests | 107 passed |
| Generated editor navigation | 34,105 checks across all eleven outputs and C++ profiles |
| Real .NET navigation | 555 checks passed |
| Editor packaging | Both 1.1.36 packages rebuilt and validated against current shared grammar |
| Documentation links | 797 local and repository links checked; no missing targets or heading anchors |
| Public version metadata | `serializer --version` reports compiler 1.14.0 and schema language 1.9.0 |

Derived-size consumers check independent nested spans, uint32/uint64 fields,
stored-source preservation, magic, disjoint protocol selections, wire-only
storage absence, complete header exclusion under payload scope, and accepted
noncanonical compact key widths. Both policies reject missing/duplicate headers,
truncation, trailing bytes and ordinary resource-limit failures. Verification
also rejects wrong size values; retention preserves/discards them according to
storage. C conversion consumers inject allocation failures and verify rollback.

The count-only size-width guard also passes the native C++ SDK checks and the
GCC/Clang constant-evaluation probes. The complete Windows regression log is
`out/build/deferred-features-native-complete.log`; final SDK, compiler and editor
qualification logs remain beside it. Compilation uses the installed MSVC and
Windows SDK through the repository wrapper. One existing journal subtest skips
its sidecar-specific assertion in appended-storage mode; the sidecar case passes.

Run the SDK suites with the built compiler and runtime:

```powershell
python test/auto_size_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/auto-size-qualification --wsl-languages c,rust,swift
python test/conversion_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/conversion-qualification --wsl-languages c,rust,swift
python test/stateless_languages.py --compiler out/build/make-Release/Release/serializer.exe --cpp-library out/build/make-Release/Release/serializer_runtime.lib --build out/stateless-qualification --wsl-languages c,rust,swift
```

For individual size policies, use `--policy verify` or `--policy retain`.
Windows uses MSVC, installed language SDKs and WSL GCC/Rust/Swift. Local
TypeScript and Kotlin overrides select their installed compiler entry points.
Useful generated modules, executables, packages and final qualification logs
remain under ignored `out/build` and `out/extensions`.

## Supported limits

Native size headers support fixed uint32/uint64 fields in unpacked unmanaged
owning records. JSON and Protobuf size derivation, view/managed/lifecycle fields
and compact size scalars are diagnosed. Compile-time output remains positional
only. No raw ABI object dump is introduced. Conversion shapes and generic
constraints follow [conversions](conversions.md), [derived sizes](derived_sizes.md)
and [generics](generics.md#stateless-type-constraints); no portable managed engine
or arbitrary expression conversion is implied. These checks do not measure
performance or qualify every target architecture.
