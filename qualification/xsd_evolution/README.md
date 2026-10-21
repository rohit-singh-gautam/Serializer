# XSD authoring and evolution qualification

This inventory checks existing one-source XSD authoring workflows across all
eleven generated outputs. It combines staged declaration imports and exact
ambiguity/publication checks with actual native/XML consumers of two authored
model versions. No parallel XML INI mapping inventory is used.

The authored fixtures preserve `evolution::order` and its declaration order:
`id` (native ID 37, default 7), `customer` (ID 11, default `"native default"`),
and optional `note` (ID 83, initially absent). The readonly `has_id(expected)`
callable retains its behavior. Both schemas associate `order.xsd` using
`using schema "order.xsd";`. Version 1 derives the XML sequence `customer, note`;
version 2 derives `note, client` and uses the inline exception
`xml(name("client"))` on the existing native `customer` member. The native field
names, IDs, order and types remain unchanged.

Adding a field through fresh automatic imports is a separate workflow exercised
by `test/xsd_import_cli_test.py`. Its stages import `customer`, then insert optional
`note`, then rename XML `customer` to `client`. Existing expanded identities retain
their independently checked hash-derived IDs. A renamed identity receives a new
ID; declaration order follows the XSD. These fresh imports do not prove positional
native-byte compatibility or infer an application-level rename. Existing outputs,
dependency files and authored edits are protected: changed imports must use a new
path for review. Ambiguous shared roots require explicit root selection; ambiguous
local-bundle associations require namespace/root disambiguation.

`manifest.json` independently fixes the authored native fields, XML sequences and
five native byte strings. `oracle.py` compares all three emitted native protocols
(`binary_none`, `binary_integer`, `binary_string`) to literal bytes, and checks
default/XML-projection native output separately. XML is checked both by expanded
names/order/values and by an independent, network-disabled lxml XMLSchema provider.
The oracle imports no generated codec. Its controls include two valid documents,
eight XML mutations and one corrupted output for each of the three protocols.

The external consumer protocol is:

```text
consumer VARIANT emit OUTPUT_DIRECTORY
consumer VARIANT check-xml INPUT_XML OUTPUT_NATIVE
consumer VARIANT check-native INPUT_NATIVE OUTPUT_XML
```

`VARIANT` is `v1` or `v2`. `emit` checks fresh defaults and the readonly callable,
then authors id 7, customer `R&D` and note `memo`. It writes `authored.xml`,
`authored.bin`, `authored-integer.bin`, `authored-string.bin`, `default.bin` and
`xml-projection.bin`. Both authored XML versions retain `memo`, so both projection
files equal `authored.bin`. `check-native` consumes `binary_none`; tagged-protocol
output is checked by the literal oracle rather than the exchange reader.
Candidates are validated before output publication, and invalid XML must exit
unsuccessfully without creating the requested output.

Build the compiler and native tests through the root Windows/Linux wrappers with
XML enabled. This includes strict C++20 targets `xsd_evolution_cpp_v1` and
`xsd_evolution_cpp_v2` and their generated `order.hpp` files. Select installed SDKs
and a fresh ignored evidence directory, then run from the repository root:

```powershell
python -B qualification/xsd_evolution.py `
  --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/xsd-evolution/windows `
  --cpp-v1-producer out/build/ReleaseWindows/test/xsd_evolution_cpp_v1.exe `
  --cpp-v2-producer out/build/ReleaseWindows/test/xsd_evolution_cpp_v2.exe `
  --cpp-v1-generated out/build/ReleaseWindows/test/xsd_evolution_v1/order.hpp `
  --cpp-v2-generated out/build/ReleaseWindows/test/xsd_evolution_v2/order.hpp `
  --provider-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows `
  --node-dependencies editors/vscode/node_modules `
  --typescript-cli editors/vscode/node_modules/typescript/bin/tsc `
  --kotlinc C:/tools/kotlin/kotlinc/bin/kotlinc.bat `
  --dotnet dotnet --swiftc swiftc --jobs 3 --interop-jobs 6
```

Use an MSVC developer shell on Windows. On Linux, use the corresponding ELF
compiler/consumer paths and omit `.exe`; `--provider-prefix` is for Windows C's
selected libxml2 deployment. Select other installed tools with `--cc`, `--cargo`,
`--go`, `--java`, `--javac` and `--node`. Rust uses the maintained locked Cargo
dependencies offline. Kotlin requires its full compiler distribution; JS/TS use
the repository-locked parser packages and TypeScript compiler. C# selects .NET SDK
8.0.426 targeting .NET 8. The independent oracle requires lxml; missing tools or
providers fail rather than skip.

The default full run builds 22 consumers, emits 132 files and performs 484 native
exchanges across both versions, 242 XML exchanges within matching versions and
44 wrong-version/namespace rejections. A subset such as `--languages java kotlin`
is explicitly diagnostic. `--compare-to OTHER_HOST/verification.json` requires two
complete, clean source-identical inventories; it compares all native emission
hashes and the checked XML projections. Admission requires all five exact native
hashes and the manifest's expected XML projection. XML byte equality is recorded
separately.

Source/compiler, generated library, consumer image, SDK and XML-provider artifacts
are pinned and checked for stability. Conflicting captures of one artifact fail.
Actual commands, diagnostics, decisions and hashes are retained in
`verification.json`, including setup failures. CMake registers the independent
oracle and guard controls separately from the actual eleven-SDK driver.

This inventory does not introduce automatic rename inference, merge authored
schemas, migrate positional native data or broaden rejected XSD/document graphs.
Broader association/import/export/local-bundle coverage remains tracked in
`OPC-R12`; see the [dated verification record](../../docs/verification/verification-xsd-evolution-2026-10-21.md)
and [pending requirements](../../docs/pending.md).
