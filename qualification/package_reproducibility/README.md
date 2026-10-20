# Package reproducibility qualification

This inventory checks existing writer policies in all eleven outputs. It uses
literal payloads and authored relationships, six permutations of unordered input
sets, two fresh producer processes, stored/DEFLATE payload profiles and public
ZIP64 forcing. Missing SDKs fail the full run. A `--languages` subset is explicitly
diagnostic and cannot report full qualification success.

`manifest.json` fixes the input bytes and each backend's existing policy.
`oracle.py` independently parses local headers, central directories, signed
descriptors, ZIP64 extras/end records and CRCs; it also verifies complete raw
DEFLATE streams, uncompressed payload bytes, XML declarations, relationship
ordering, raw targets and closure. It imports no generated reader or writer.
Its authored positive and negative controls run before every matrix and through
the guarded `serializer_package_reproducibility_oracle` CTest.

The producer protocol is:

```text
producer FIXTURE_DIRECTORY PROFILE PERMUTATION OUTPUT [FORCE_ZIP64]
producer --provider
```

Profiles are `zip-stored`, `zip-deflate`, `opc-stored` and `opc-deflate`;
permutations are 0 through 5; the optional ZIP64 flag is 0 or 1. The profile
overrides fixture payload compression. OPC infrastructure keeps the backend's
existing compression policy, so `opc-stored` can still contain DEFLATE metadata.
C++ forces its public per-payload entry options; other writers use their existing
whole-operation controls. The manifest checks their distinct valid ZIP64 layouts.

Fixtures are UTF-8/LF headerless TSV under `zip/` and `opc/`. Parts contain name,
media type, storage and hexadecimal payload columns. Relationships contain source,
ID, type, target and mode columns. Text columns contain no literal tabs/newlines;
empty hexadecimal payload cells are valid. Adapters permute inputs without
sorting them; the actual writer must implement its declared canonical ordering.

Build native targets with the root Windows/Linux wrappers, with XML, zlib and
tests enabled. The build includes `package_reproducibility_cpp` and its generated
`test/package_reproducibility/package_relocation.hpp`. Then run the standalone
driver from the repository root, selecting the installed SDK paths:

```powershell
python -B qualification/package_reproducibility.py `
  --compiler out/build/ReleaseWindows/serializer.exe `
  --build out/build/package-reproducibility/windows `
  --cpp-producer out/build/ReleaseWindows/test/package_reproducibility_cpp.exe `
  --cpp-generated out/build/ReleaseWindows/test/package_reproducibility/package_relocation.hpp `
  --cpp-provider-library out/build/ReleaseWindows/test/z.dll `
  --provider-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows `
  --node-dependencies editors/vscode/node_modules `
  --typescript-cli editors/vscode/node_modules/typescript/bin/tsc `
  --kotlinc C:/tools/kotlin/kotlinc/bin/kotlinc.bat `
  --dotnet dotnet --swiftc swiftc
```

Use a configured MSVC developer shell on Windows. C/Rust/Go and JVM/Node tools
can be selected with `--cc`, `--cargo`, `--go`, `--java`, `--javac` and `--node`.
For dynamically linked C++ zlib, pass the actual loaded library with
`--cpp-provider-library` (the deployed `z.dll` in this Windows example, or the
resolved linked `libz.so` on Linux) so its bytes join the provider fingerprint.
Rust uses the maintained locked dependencies offline. Kotlin requires its full
compiler distribution. JS/TS require the repository-locked XML packages and
TypeScript compiler. C# selects the installed .NET SDK **8.0.426**, targets .NET 8
and disables persistent build servers; later runtime/provider behavior is outside
this fixed inventory. Swift compiles with optimization and warnings as errors.

Every producer reports its actual compression implementation/version/options.
The driver pins provider/tool/source/binary artifacts, generated libraries and
consumer images, and verifies they remain unchanged. Complete archive bytes must
match across every permutation and repeated process within the same configured
scope. Use `--compare-to PATH/verification.json` on a second full host run to
compare metadata/uncompressed projections, require stored ZIP byte equality, and
qualify compressed equality only when complete provider fingerprints or explicit
self-contained implementation identities agree. Observed equality with different
provider pins is recorded without making a general byte-equality promise.

Python's OPC adapter uses a public generated schema facade and full Save. Python
currently has no public generic ZIP writer: its plain ZIP adapter is explicitly a
diagnostic call into the generated physical helper. This qualification does not
introduce that as a public application API. XML payloads remain opaque bytes;
the matrix does not establish lexical XML normalization, signatures or C14N.

CPU/RSS performance, large-archive stress and broad OPC/3MF conformance retain
their separate backlog gates. See the [verification index](../../docs/verification/README.md)
and [pending requirements](../../docs/pending.md).
