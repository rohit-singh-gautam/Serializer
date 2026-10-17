# XML and package work still pending

Release **2.3.0** implementation checkpoint, **2026-10-17**.
Read the [implemented APIs](serialization/xml_packages.md),
[examples](../example/xml/README.md) and [complete requirements](design/xml_support.md).
Backend verification is recorded separately; generated code alone is not qualification.
See the [executed checks](verification/verification-xml-packages-2026-10-17.md).

**Coverage rule:** general formats/features must support every generated language.
A missing backend remains pending. Only inherently language-specific features,
such as C++ constant evaluation and borrowed native views, may differ by language.

```text
ordinary owning model
        |
        +-- generated XML: all outputs
        |       |
        |       +-- SDK qualification
        |
        +-- compression / ZIP / OPC
                |
                +-- C++ implementation
                +-- other backend parity
```

| Area | Remaining requirement |
| --- | --- |
| XML performance | Tree-free token codecs; measured CPU, memory, encoded and generated-code size. Current mapping is static; several backends retain bounded trees. |
| XML vocabulary | Text with attributes, wrappers, `xsi:nil`, ordered mixed content, maps/variants and explicit unknown-extension policies. |
| XSD compilation | Bundled imports/includes, custom simple types/facets, nested schema record types, richer content models and identity constraints. Unsupported constructs currently fail generation. |
| XSD tooling | XSD export/import subset, compatibility diagnostics and pinned bundle identity. Changing a fixed schema still requires regeneration. |
| Runtime XSD | Qualify prepared providers, limits and concurrency on every backend. Some outputs use an explicit caller-prepared validator interface. No automatic remote schema loading. |
| Model coverage | Generics, inheritance, managed/view/revision/magic/digest mappings require explicit XML contracts. |
| Compression parity | Whole-object stream wrappers and identical framing/error/limit contracts for every output; optional Zstandard/LZ4 providers where absent. |
| Format selection | Extend the existing C++ `binary_none`-only output selection to other backends; XML stays independently opt-in. General format selection is not a language-specific exception. |
| Package parity | ZIP/OPC source/sink APIs, metadata, shared budgets and full-Save guarantees across outputs. Current package implementation is C++. |
| ZIP extensions | ZIP64, nonseekable input/spooling and tighter peak-memory/cancellation guarantees. Unsupported features must keep failing explicitly. |
| Typed profiles | Generated compression/package pipeline configuration, fixed defaults with optional runtime selection, vocabulary profiles, expected-type dispatch and application reference checks before publication. |
| Typed package loading | One budget across codecs/references, transactional aggregate loads and diagnostics identifying part/field/reference failures. |
| Preservation | Explicit optional reject/skip/preserve policies; opaque bytes plus relationship/namespace context. No signature or lexical-preservation claim. |
| Reproducibility | Canonical ordering/metadata policies and independent byte-equality qualification beyond stable same-input writer output. |
| HTML | Separate HTML syntax/adapter. XML-form XHTML is limited by the implemented XML vocabulary; mixed-content XHTML is not supported. |

The document-state bridge remains a separate substantial item:

```text
editable application state
       |
       +-- full Save --> XML / ZIP / OPC file
       |
       +-- binary_none side channels
             history / journal / collaboration
```

It must preserve object identities, allocation marks, retained history and branch
cursors; bind side channels to the exact saved baseline/layout; and recover a
coherent old/new generation after interrupted publication. Stale/mismatched state
must fail before decoding. Final document files must contain no Serializer
history, active journal or session/outbox state. Existing native managed formats
retain their compatibility contract; they are not already this adapter.

Compression is for **one complete serialized object**. Backend byte chunks are
an implementation detail. Incremental record callbacks and incremental object
serialization are not requirements of this work.

Before marking the complete feature finished: run every SDK, XML-disabled and
minimal-provider builds, independent XML/XSD/ZIP checks, malformed-input/resource
tests, cross-language exchanges, Linux/Windows checks and performance measurements.
No completion dates or unexecuted verification are implied by this list.
