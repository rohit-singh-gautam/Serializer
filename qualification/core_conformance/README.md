# Official Core 1.4 corpus replay

The [current record](../../docs/verification/verification-core-conformance-2026-10-21.md)
qualifies the published Core 1.4/XML XSD bundle against the independent official
Core-only Suite3 corpus. The
[manifest](../../test/resources/3mf_core_1_4/official_suite3_manifest.json) pins every
archive and model byte hash. Corpus acquisition is explicit; qualification never
downloads a schema, follows external XML entities or silently skips a missing SDK.

Use an existing built Serializer compiler/runtime with XML enabled, Python 3,
.NET SDK 10, CMake 4 and a C++23 toolchain. The .NET streaming `XmlSchemaSet` oracle
uses the unchanged finite Core occurrence bounds. Secondary `xmlschema` 4.2.0
checks the 111 model parts no larger than 8192 bytes. These providers are independent
of Serializer's importer, generated codecs and libxml2 grammar projection.

Fetch only the pinned official Core cases into ignored storage:

```text
git clone --filter=blob:none --sparse https://github.com/3MFConsortium/test_suites.git out/build/core-conformance/official
git -C out/build/core-conformance/official fetch --depth 1 origin f483d3beee069e7adbac125d1f7baa36b5b3e5bf
git -C out/build/core-conformance/official sparse-checkout set --no-cone /suite3_core/positive_test_cases/ /suite3_core/negative_test_cases/
git -C out/build/core-conformance/official checkout --detach f483d3beee069e7adbac125d1f7baa36b5b3e5bf
python -m pip install --target out/build/core-conformance/python-dependencies xmlschema==4.2.0 elementpath==5.1.4
dotnet build qualification/core_conformance/oracle.csproj -c Release --artifacts-path out/build/core-conformance/dotnet
```

Run `qualification/core_conformance.py` with common arguments `--official`,
`--build` and `--python-dependencies` naming the checkout above, a fresh ignored
replay directory and the dependency directory. Its phases are:

1. `--phase oracle --oracle-dll <built oracle.dll>` validates all 175 original model
   streams from all 175 archives, including models above the consumer byte policy.
   Discovery honors declared model MIME types, including arbitrary/no extensions;
   identifiable `.model` payloads in package-invalid negatives are also inspected.
   It writes `oracle.json` with hashes, diagnostics and secondary-provider agreement.
2. `--phase generate --compiler <serializer>` imports the actual local Core/XML
   bundle and attempts all eleven backend bindings. C++ and Python are admitted;
   the other nine must reject precisely for unsupported rich XML shapes. Generated
   C++ formatting is explicitly disabled; schema and codec validation remain active.
3. `--phase python` runs the actual generated Python model on every original part
   within the explicit common 16 MiB input policy, edits metadata when present,
   writes the graph and decodes it again. Resource rejection is recorded separately
   from schema rejection. This complete Python workload can take several minutes.
4. Configure this directory with CMake using `CMAKE_PREFIX_PATH` for an installed
   Serializer package and `SERIALIZER_CORE_GENERATED_DIRECTORY` for
   `<replay>/generated-cpp`. Build `core_conformance_probe`, then run
   `--phase cpp --cpp-probe <executable>`. This also compares every native persistent
   field through binary payload equality before/after the edited XML round trip.
5. For each of `--output-language cpp` and `python`, run
   `--phase validate --oracle-dll <built oracle.dll>`. The independent .NET oracle
   must accept every actually emitted model. The native disposition checker rejects
   acceptance of invalid originals or rejection unrelated to declared resource limits.

The SDK's artifact layout places the oracle under
`<artifacts>/bin/oracle/release/oracle.dll`. Windows launch parents suppress error
dialogs. Ensure compiler/runtime dependency DLLs are discoverable on `PATH`, or use
a correctly installed compiler/runtime. Root repository builds use `./make.ps1`
on Windows and `make` on Linux; the CMake project here is a separate qualification
consumer of the installed package.

This is Core XML codec qualification. It does not claim that accepting XSD-valid
XML accepts its geometry, object references, required extensions, OPC graph or
business rules. Those checks have separate contracts. Application geometry/domain
validation is [host-owned](../../docs/project/application_responsibilities.md).
