# Native foreign-content graphs — 22 October 2026

Compiler/runtime **2.28.0**, schema language **1.21.0**. Native C/Rust document
companions now traverse wrapped and nil records, lifecycle metadata, typed
mixed/ordered children, maps and element/`xsi:type` variants. Static ownership
descriptors follow XML wrappers independently of native storage. Foreign payloads
remain in XML-only companions; native fields and all four native protocols retain
their existing layout and identities.

The expanded actual-SDK matrix passes **90 decisions per consumer** in
`out/opc-stable-map-graphs.log`. It covers namespace-valued text and qualified
Unicode attributes, child-local scopes, independent nested foreign attributes,
ordinary positioned child slots, nil markers, revision/magic metadata, maps and
both variant selectors. Disallowed foreign namespaces, unknown core children and
missing required positioned children reject. Consumers release input bytes before
replay, compare native encodings, enforce exact/one-tight preservation budgets and
recover with a fresh policy. C also checks transactional destination preservation.

Variant ownership dispatch resolves the instance's expanded `xsi:type`. On output,
private writer type aliases are rebound into the retained scope without overriding
aliases used by opaque content or QName-valued native strings. Conflicting default
namespace combinations reject instead of silently changing expanded identities.

The maintained `native_xml_extension_graph_test.py` includes mixed/end-slot
cardinality, late known-child/text, wrong type and reverse-map controls in those
180 passing decisions. Map companion paths follow complete canonical native keys
when a writer reorders entries. String, Unicode/empty, enum, Boolean and signed/
unsigned numeric key shapes are verified independently from input entry order.

`xml_ordered_extensions_test.py` additionally passes **18 decisions in each of
eight actual SDKs**: Python, JavaScript, strict TypeScript, Java, C#, Go, Kotlin
and Swift (`out/opc-ordered-suffix-sdk.log`). Required opaque suffixes have their
own occurrence quotas and do not consume fixed/dynamic known-fragment limits.
Missing, early, excess and late-text controls reject. C++'s generated
`xml_ordered_extensions_test` adds two passing GTests for dynamic/fixed ordered
storage and actual mixed text; its original output string survives failure.

This advances `OPC-R02` and the graph portion of `OPC-R01`. Positioned foreign
content interleaved within mixed/all storage, fixed-XSD wildcard adaptation and
the remaining backend combinations still require implementation and qualification.
The original request remains open until its parent requirements are completed.
