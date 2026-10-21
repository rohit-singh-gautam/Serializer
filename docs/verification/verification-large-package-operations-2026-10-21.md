# Large combined package operation qualification

21 October 2026, compiler/runtime **2.22.0**, schema language **1.21.0**.
This record completes the supported owning identity profile behind `OPC-R11`,
`XML-B05` and `XML-B21` in all eleven actual Windows SDKs. Unsupported richer
graphs, other platform profiles and opaque provider preemption remain separate.

## Independent input and output contracts

`test/package_large_operation_corpus.py` authors the input using independent
Python ZIP and XML producers. Three separately scoped registry parts each have
25,000 forward references followed by 25,000 keyed entries with unique,
deterministic 32-character labels. The fixture contains **150,003 model elements**,
**225,000 key/unique/keyref tuples**, **six relationships**, **5,708,538 model XML
bytes** and a **65,536-byte opaque asset**. The DEFLATE archive is 1,932,355 bytes,
SHA-256 `6534fdf2f5cf8d67b881bf1d179c425194a510c3517ad5538c09e806f79c02b4`.
Each owner repeats the same integer key range, requiring owner-local identity
scope rather than global deduplication. The final invalid fixture changes only
the last owner's final reference to a missing key.

Explicit preservation policies retain the opaque asset, its root relationship,
a source-scoped internal link with a selector and the exact external target
string. External targets remain inert. Independent `zipfile` verifies every
output CRC, `lxml` checks the unchanged identity XSD and exact model values, and
the oracle verifies the asset bytes and all three preserved relationships.
The valid input, intentionally invalid final owner and fixture reproducibility
were independently checked before SDK execution.

These are actual generated owning SDKs. The tests use public budgets, policies,
selected-codec adapters and Save APIs. They do not replace providers with
successful stubs or use private operation hooks to manufacture cancellation.
The generated identity engine enforces the native constraints; runtime prepared
XSD validation is not selected in this large fixture. Its independent XSD oracle
is distinct from the separately executed eleven-output prepared-provider checks.

## Completed Windows SDK profiles

| Actual output | Successful consumer assertions | Final evidence directory |
| --- | ---: | --- |
| C++ | 78 | `pending-package-large-2026-10-21/cpp-standard-marker-final/` |
| C | 229 | `pending-package-large-2026-10-21/c-final/` |
| Rust | 26 | `pending-package-large-2026-10-21/rust-final/` |
| Java | 20 | `pending-large-operation-portable-2026-10-21/java/` |
| C# | 20 | `pending-large-operation-portable-2026-10-21/csharp/` |
| Go | 20 | `pending-large-operation-portable-2026-10-21/go/` |
| JavaScript | 20 | `pending-large-operation-portable-2026-10-21/node/` |
| TypeScript | 20 | `pending-large-operation-portable-2026-10-21/node/` |
| Kotlin | 27 | `pending-large-package-mobile-2026-10-21/kotlin/` |
| Swift | 23 | `pending-large-package-mobile-2026-10-21/swift-preservation-final/` |
| Python | 78 | `pending-package-large-2026-10-21/python-fixed-large/` |

All paths above are beneath ignored `out/build/`. Assertion counts include each
consumer's own model, filesystem and recovery checks; they are not equivalent
units of benchmark work or additional CTest targets. The exact SDK versions,
argv, generated-source hashes, logs and outputs remain in the receipts.
Java uses actual JDK 26 with `--release 17`; C# uses .NET SDK 10.0.402 targeting
net8; Go is 1.26.4/windows-amd64; Node is 22.23.1 with strict TypeScript 5.9.3.
Kotlin and Swift libraries compile separately from their importing consumers.

Every completed profile checks real DEFLATE input and output, typed identities,
opaque preservation, late invalid-owner rejection, cancellation/elapsed host
time, pre-publication file preservation and fresh-call recovery. Each public
operation retains its cumulative ledger through its generated XML, compression,
ZIP, identity and preservation stages. Exact/one-tight reference, work and
logical allocation admission are exercised; the five portable profiles also
exercise preserved-byte admission. All five portable pipelines retain one caller
budget through five traversals, with **1,125,030** total reference admissions.
C and Rust retain one caller ledger through load plus Save, with **675,018**
reference admissions. A single large model load spends **225,006**.

C++ public load and Save accept limits and own their operation budgets; write
additionally accepts a caller budget. The test measures exact owning write
admissions, verifies those same exact/one-tight limits through full generated
Save, and exercises actual final pre-publication cancellation. It does not claim
an unavailable public budget argument for cross-call load/Save reuse. Kotlin and
Swift likewise retain the documented per-operation boundaries; their independent
operation counters are not presented as a five-traversal portable pipeline.

Public completion controls use the actual available backend interfaces. Kotlin's
domain callback and final `before_replace` callback arm cancellation and return;
the same operation must reject before publication. Swift's real
`requiredRelationship` preservation callback arms cancellation after typed owners
have decoded and returns normally; no aggregate may escape. Swift exposes no
replacement fault hook, so its Save check rejects during final encoding under
the same public budget, preserves the old file and then recovers. It does not
claim forced preemption of Foundation's opaque atomic write.

Go's `encoding/xml` writer emits additional physical namespace declarations.
Its public selected-codec adapter uses a finite `MaxAttributes = 5*n + 8`
profile, preserving the package-supplied validator and shared context. The
ordinary 100,000-attribute policy correctly rejects this 125,001-attribute output;
the qualification does not silently change the library default. Other large
limits are explicit finite profiles recorded in each maintained consumer and
receipt. Logical admission counters are not hard process-memory ceilings.

## Publication defect and capture provenance

The old immutable compiler SHA-256
`dea245fd49925f789d3d4444750d21501bdd726656fcf7372c046dfde4af6499`
reproduces Python's missing pre-publication callback fence on this full input:
Save returns after the callback arms cancellation and overwrites the sentinel.
The independently valid complete replacement and counterfactual are retained in
`python-dea-counterfactual/counterfactual.json`. The regular 44-test Python suite
separately reproduces both missing pre- and post-publication checks.

The two added polls in `src/python_package_runtime.inc` preserve a throwing
hook's original error. Before replacement they retain the previous file; after
replacement they report `publication_uncertain`, retain the original cause and
leave a complete new archive. The corrected large Python publication controls
pass both cancellation and real elapsed-clock cases at both boundaries. The full
**78-assertion** large run passes in **1,817.672 seconds**, including deliberate
real deadline waits; its initial complete pipeline takes **128.587 seconds**.
All six exact/one-tight resource cases, invalid-final-owner rejection, three
original-error cancellation cases, three native deadline cases and fresh-call
recovery pass. Each ordinary interruption reaches its exact measured poll
ordinal: **1**, **14,251,365** or **28,502,732**. The one shared load/Save ledger
spends **675,018 references**, **178,998,376 work units** and **562,186,296 logical
allocation bytes**.

The integrated post-fix compiler SHA-256 is
`82019ab1cc5088111bc7c562a5b29ba5899f660a5143b0120abcf9736dc3a061`.
C++'s final 78-assertion large run executes that image directly, including a
caller exception derived from `std::exception` whose shared token retains
identity across MSVC's exception-storage copies. Source-only
bridges regenerate all eleven large identity profiles: the ten non-Python
outputs are byte-identical to the executed `dea245fd` outputs, including recursive
C++ include units and JS/TS declarations. Final Python output is byte-identical
to its actually executing corrected private compiler
`9c32096a38efc06372a9e6948fbf12eadf1fcb5e8ad9f9d0a60389b9f91b4d7b`,
with generated-source SHA-256
`b7cda26c4bce179f98f59667bc7f9175a58ea2938e7937c6379df6f60da1513c`.
Bridges compare generation only; original executed receipts are unchanged.

An initial C test incorrectly assumed a two-millisecond sleep necessarily
advanced Windows' coarse `GetTickCount64`. The maintained test now waits until
its recorded actual deadline. The corrected full C replay passes all 229
assertions and eight interruption cases. No production change was needed.

## Reproduction and limits

Use a Windows MSVC developer shell, the already configured native runtime and
actual installed SDKs. Supply the existing `lxml` directory on `PYTHONPATH` and
the provider DLL directory on `PATH`. The runners install no SDKs/providers.

```powershell
$packageCompiler = 'out/build/pending-runtime-providers-completion-2026-10-21/compiler-publication-final/serializer.exe'
$packageProvider = 'out/build/ReleaseWindows/vcpkg_installed/x64-windows'
python -B test/package_large_operation_native_test.py --compiler $packageCompiler --build out/build/large-python --language python
python -B test/package_large_operation_native_test.py --compiler $packageCompiler --build out/build/large-cpp --language cpp --provider-prefix $packageProvider --runtime-build out/build/ReleaseWindows
python -B test/package_large_operation_native_test.py --compiler $packageCompiler --build out/build/large-c --language c --provider-prefix $packageProvider --cc cl
python -B test/package_large_operation_native_test.py --compiler $packageCompiler --build out/build/large-rust --language rust --cargo-home out/build/pending-qname-2026-10-20/native-namespaces/cargo-home
python -B test/package_large_operation_portable_test.py --compiler $packageCompiler --build out/build/large-portable --language java,csharp,go,javascript,typescript --node-dependencies out/build/pending-namespace-budget-2026-10-21/js-providers/node_modules
python -B test/package_large_operation_mobile_test.py --compiler $packageCompiler --build out/build/large-kotlin --language kotlin --kotlinc out/tools/kotlin-2.2.21/kotlinc/bin/kotlinc.bat --oracle-pythonpath out/python-dependencies
python -B test/package_large_operation_mobile_test.py --compiler $packageCompiler --build out/build/large-swift --language swift --oracle-pythonpath out/python-dependencies
```

The normal harness default is 25,000 entries per owner. The opt-in
`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` Python CTest uses ten per owner for a
quick regression of the same operation chain, requiring the independent `lxml`
oracle. Its actual manual smoke run passes on the final compiler; it is not
counted as a large profile or as a new target in the earlier 275-target capture.

This qualification selects Windows owning XML and standard ZIP DEFLATE. It is
not a Linux/macOS replay, a throughput/peak-memory measurement, signature
verification, a hard provider CPU/memory limit or evidence for unsupported rich
XML/projection graphs. The [prepared-provider completion checks](verification-operation-publication-2026-10-21.md)
and [large owning XML measurements](verification-large-xml-2026-10-21.md) retain
their distinct configurations and historical provenance.
