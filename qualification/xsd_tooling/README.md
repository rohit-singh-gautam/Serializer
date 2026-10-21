# XSD export, comparison and local-bundle qualification

This inventory exercises the compiler's existing XSD tooling and the 2.20.0
native-enum export/binding addition. It keeps compiler contract checks separate
from generated runtime qualification. The
[dated verification record](../../docs/verification/verification-xsd-tooling-2026-10-21.md)
identifies the executed versions, profiles and results.

Run from the repository root with a built compiler, Python and the independent
lxml XMLSchema provider. Use a fresh directory under ignored `out/build`:

```powershell
python -B qualification/xsd_tooling.py `
  --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/xsd-tooling/windows
```

Use the corresponding ELF compiler path on Linux. Build the compiler and native
tests through the root `make` or `./make.ps1` wrappers. Missing required tools or
XML providers fail rather than skip. The three default modules are mandatory for
a complete compiler-tooling result. Selecting `--modules comparison_cases`, for
example, produces diagnostic evidence and cannot satisfy full admission.

## Compiler contract inventory

`bundle_limits.py` contains 45 actual CLI decisions. It checks the local-only
allowlist, namespace/import/include associations, UTF-8 paths and dependency-file
escaping, canonical containment and aliases, cycles, chameleon instances, and
exact/one-over document, reference-depth and aggregate-byte bounds. Rejected
preflight operations retain prior outputs and dependency files without staging
residue. The bundle SHA-256 identity is independently reconstructed from its
documented byte framing.

`comparison_cases.py` contains 45 decisions: ten compatible contracts, 21 actual
compatibility differences, six unavailable associations and eight explicit
backend capability rejections. Both schemas must associate with the same native
model before directional comparison. Occurrence widening/tightening, sequence
and `all` membership/order, root/member names, attribute placement, defaults,
fixed values, scalar facets and required attributes are checked independently.
Sequence-to-`all` widening does not depend on `all` declaration order. Reverse
`all`-to-sequence and reordered sequences retain their rejection controls. An
unavailable association or generated-code capability is not reported as a
successful compatibility comparison.

The comparison fixtures retain explicit native field IDs, declaration order,
defaults and readonly behavior within each model profile. Real generated C++ and
Python outputs are checked; Python execution and literal native bytes exercise
all three native protocols. The singleton-`all` model and repeated-storage model
remain separate profiles, so their different particle capabilities do not imply
a native-layout migration.

`export_cases.py` contains eight rich fixture families: string text/facets,
wrappers, cross-namespace records, singleton `all`, chameleon schemas, cyclic
includes, native enums and native-enum default/fixed/text constraints. It checks
real exports, independent XSD validation, rebinds, reviewed fresh imports,
native model syntax and publication failures. Unsupported combinations must
reject without replacing existing output. These rich cases establish compiler,
AST and independent-XSD contracts; they do not assert every rich graph executes
in all eleven generated runtimes.

The new native-enum profile exports the exact authored enum wire names into an
`xs:string` enumeration. Binding permits empty or `preserve` whitespace behavior;
supported compiled string facets can narrow the admitted labels. Native values,
names, declaration order, constructor defaults and readonly behavior remain
unchanged. XML defaults and fixed values must select an admitted enumerator.
The compiled restriction must admit at least one native enumerator. Unknown or
facet-excluded defaults/fixed values and empty native admission reject during
generation, before protected output or dependency files change.
Ordinary, optional and repeated enum element members are distinct from an XML
lexical list. Text requires one nonoptional scalar; optional/repeated text remains
unsupported. Token/collapse/normalized whitespace, enum lexical lists and
numeric/QName/list binding spaces are outside this profile and reject explicitly.
Authored export/rebind retains native enum types/defaults/methods. Fresh imports
of the exported XSD produce constrained strings with fresh imported names/IDs;
they do not reconstruct the authored enum or application behavior.

The [XSD tooling contract](../../docs/serialization/xsd_tooling.md) documents
directionality, bundle identity and supported scopes. Its
[Windows UTF-8 CLI section](../../docs/serialization/xsd_tooling.md#windows-utf-8-cli-paths)
describes the compiler's process manifest and qualified operating-system scope.

## Ordinary eleven-SDK export/rebind follow-up

A complete tooling run also exports the two ordinary evolution models into
`BUILD/sdk_schemas/v1/order.xsd` and `BUILD/sdk_schemas/v2/order.xsd`. Each directory
contains a byte-identical copy of its original `order.serializer`. The preparation
step compares the original/exported contracts in both directions, compares the
generated Python native methods and executes defaults, readonly behavior, all
three literal native protocols and independent XML validation.

The tooling report deliberately records `scope: compiler_tooling_only`,
`runtime_followup_required: true` and `runtime_backend_completion: false`.
Runtime completion requires a separate actual SDK matrix:

```powershell
$schemaDirectory = (Resolve-Path out/build/xsd-tooling/windows/sdk_schemas).Path
./make.ps1 configure -BuildDirectory out/build/xsd-tooling/rebound-windows `
  -CMakeArgs '-GNinja', "-DSERIALIZER_XSD_TOOLING_SCHEMA_DIRECTORY=$schemaDirectory"
cmake --build out/build/xsd-tooling/rebound-windows --config Release `
  --target xsd_tooling_cpp_v1 xsd_tooling_cpp_v2 --parallel 4

python -B qualification/xsd_evolution.py `
  --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/xsd-tooling/runtime-windows `
  --schema-directory out/build/xsd-tooling/windows/sdk_schemas `
  --cpp-v1-producer out/build/xsd-tooling/rebound-windows/test/xsd_tooling_cpp_v1.exe `
  --cpp-v2-producer out/build/xsd-tooling/rebound-windows/test/xsd_tooling_cpp_v2.exe `
  --cpp-v1-generated out/build/xsd-tooling/rebound-windows/test/xsd_tooling_v1/order.hpp `
  --cpp-v2-generated out/build/xsd-tooling/rebound-windows/test/xsd_tooling_v2/order.hpp
```

Supply the installed SDK/provider options described by the
[evolution inventory](../xsd_evolution/README.md). Build the two C++ consumers
separately against the corresponding exported `sdk_schemas` inputs; original-XSD
consumer binaries do not prove the rebind. The example selects a Ninja build in
an MSVC developer shell. On Linux, configure through `make configure` with
`CMAKE_ARGS=-DSERIALIZER_XSD_TOOLING_SCHEMA_DIRECTORY=ABSOLUTE_SDK_SCHEMAS_PATH`,
build the same two targets and select their ELF paths without `.exe`.
The remaining adapters use the same
`--schema-directory` and compile their actual external consumers. The full matrix
has 22 emissions, 484 native exchanges, 242 same-version XML exchanges and 44
invalid XML failures per host. Tagged native outputs are checked by an independent
literal oracle; exchange readers use `binary_none`.

This follow-up preserves the authored IDs 37/11/83, native names/order, defaults
and callable behavior while retaining each version's XML mapping. Fresh imports
are a separate workflow: renamed expanded identities receive new hash-derived
IDs, and imported declaration order follows the XSD. Neither tooling phase
infers application renames, merges authored edits or promises positional native
compatibility for automatic reimports.

## Evidence admission

`verification.json` retains exact commands and diagnostics, unique decision IDs,
module summaries, generated artifacts, exported SDK schemas and source/compiler
and provider fingerprints. The full inventory rejects missing, empty, duplicate
or subset module evidence, changed source/compiler inputs, changed artifacts and
conflicting captures. Required XML providers and their loaded dependencies are
pinned before and after execution. Failed setup also produces a failed report.

Keep formal reports immutable. A later guard-only audit or selected replay must
identify its own source snapshot and preserve the distinction from the original
actual compiler/SDK runs. Broader unsupported XML graphs and application-level
migration remain subject to their separate requirements in
[the backlog](../../docs/pending.md).
