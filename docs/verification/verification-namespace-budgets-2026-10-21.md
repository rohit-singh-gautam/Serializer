# XML namespace budgets — 21 October 2026

[Pending requirements](../pending.md) · [Independent boundary test](../../test/xml_namespace_budget_test.py)

The **2.20.1 implementation checkpoint**, schema language **1.21.0**, corrects
selected Java/C#/Kotlin/Swift direct-writer namespace admissions. The actual
Windows SDK matrix passes **8,256 structural/text admission decisions**. This is
a completed slice of `XML-B13`; the broader parent remains open.

## Implementation and independent oracle

Java and C# charge generated element and qualified-attribute declarations against
their attribute counter before emitting declaration bytes. C# readers count
namespace declaration text as prefix plus URI in both direct and tree adapters,
matching the output convention. The declaration's expanded XMLNS attribute name
does not add an unrelated namespace URI to that text admission.

Kotlin and Swift charge generated element/attribute declarations and empty-default
resets against their combined node/attribute counter. Type-name prefix creation
charges its declaration once; reuse adds no declaration admission, including
preserved scopes. These changes retain existing output bytes and native models.

The maintained boundary test generates ordinary owning models for four profiles:
unqualified names, a namespaced root/child, qualified attributes, and an explicitly
unqualified child mapping. Expat independently counts emitted elements, ordinary
attributes, physical namespace declarations and decoded UTF-8 text. It derives
each adapter's structural and expanded-name text totals independently of the
generated implementation. Every limit from zero through 128 is checked for both
encoding and decoding, separately for structure and text:

`4 profiles × 4 SDKs × 2 policies × 129 boundaries × 2 directions = 8,256`.

The Kotlin/Swift regression also passes **161 independent variant probes per
SDK**, covering element and `xsi:type` selection, empty/prefixed type namespaces,
nested attributes, ordinary native round trips and explicit resource rejection.
Real prepared-provider tests pass across **all eleven selected outputs**: four
C++ tests, twelve Python token tests including prepared `lxml`, five JS/TS
provider/type-check tests with `libxml2-wasm`, Java/C#/Kotlin native provider
consumers and C/Rust/Go/Swift callback consumers against the actual libxml2 bridge.
These cover shared schema reuse, concurrent valid/invalid validation, bounded
input and prohibited external schema/entity loading. JS worker isolates retain
separate native handles. No mock XSD engine substitutes for these checks.

## Executed commands and artifacts

From a Windows developer shell, with the existing Kotlin compiler and Python
`lxml` directory selected through `SERIALIZER_KOTLINC` and `PYTHONPATH`:

```powershell
python -B test/xml_namespace_budget_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --build out/build/pending-namespace-budget-2026-10-21/final-portable `
  --language java,csharp
python -B test/xml_namespace_budget_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --build out/build/pending-namespace-budget-2026-10-21/final-mobile `
  --language kotlin,swift
python -B test/mobile_xml_variant_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --build out/build/pending-namespace-budget-2026-10-21/variants
python -B test/portable_xml_provider_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --build out/build/pending-namespace-budget-2026-10-21/providers
python -B test/python_xml_token_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --build out/build/pending-namespace-budget-2026-10-21/python-provider
python -B test/javascript_xml_provider_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --node-dependencies out/build/pending-105-2026-10-20/q02-final/prepared-js-typescript/node_modules `
  --build out/build/pending-namespace-budget-2026-10-21/js-providers
python -B test/native_xml_provider_test.py `
  --compiler out/build/pending-namespace-budget-2026-10-21/compiler/serializer.exe `
  --bridge out/build/ReleaseWindows/test/xml_provider_bridge.dll `
  --libxml2-prefix out/build/ReleaseWindows/vcpkg_installed/x64-windows `
  --build out/build/pending-namespace-budget-2026-10-21/native-providers `
  --language go,swift,rust,c
./out/build/ReleaseWindows/test/xml_provider_test.exe
```

The actual matrix used `out/build/ReleaseWindows/serializer.exe`; the retained
immutable checkpoint is byte-identical, SHA-256
`9edd3eb526afb1bd8f8efa126fdce9c8bb6da3c85d845605e478ece920a06938`.
Generated SDK sources, independently inspected XML, complete boundary tables and
verification logs remain in the named ignored build directory. The final test
output wording was clarified after execution; its counting and admission
assertions retain the executed behavior.

## Remaining boundaries

This checkpoint does not qualify every backend, retained-tree adapter, cumulative
package/provider combination or preserved-QName graph at exact resource limits.
It does not change schema syntax, native wire identities or XML vocabulary gates.
Logical structural/text admission is separate from provider-internal memory,
process residency and asynchronous preemption. Linux replay and the later
aggregate implementation have their own verification requirements.
