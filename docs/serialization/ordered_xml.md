# Ordered element-only XML

Compiler/runtime **2.27.0**, schema language **1.21.0**. This additive mapping
preserves heterogeneous child order in one native collection in all eleven
owning outputs. C++ support began in 2.25.0. It does not close
[OPC-R04](../pending.md#opc-and-3mf-requirements): fixed-XSD grouped/repeated
bindings and XSD export remain explicitly unsupported.

## Schema and mapping

Use an ordinary fragment record with one owning variant of at least two ordinary
record alternatives. Select its array explicitly
in the generator configuration:

```serializer
serializer version 1.21.0;
namespace jobs {
  class motion { public uint32 x(1) xml(attribute); }
  class pause { public uint32 milliseconds(1) xml(attribute); }
  class step { public variant(motion = motion, pause = pause) value(1); }
  class job { public array<step> steps(1); }
}
```

```ini
[xml]
enabled=true
[xml.class.jobs::job]
ordered=steps
# Optional selectors; defaults are the variant member "value" and record names.
# ordered.variant=value
# ordered.child.motion=motion
# ordered.child.pause=pause
```

An expanded child override uses `{namespace-uri}local-name`, as in existing XML
contracts. The owner may also have ordinary attributes; separate element/text
fields cannot share this content mapping. `ordered` and `mixed` are mutually
exclusive. String alternatives, nested/optional fragment arrays, borrowed models
and shared fragment misuse reject during generation. The helper fragment has no
standalone XML codec; use its owner's mapping.

Generate and compile through the normal [CMake workflow](../guides/usage.md).
Author order with the native collection and generated variant helpers. For C++:

```cpp
jobs::job value{};
using step = jobs::step;
value.steps.emplace_back().emplace_value<step::e_value::motion>().x = 10U;
value.steps.emplace_back().emplace_value<step::e_value::pause>().milliseconds = 50U;
value.steps.emplace_back().emplace_value<step::e_value::motion>().x = 20U;
const auto document = rohit::serializer::xml::serialize_tokens(value);
```

```xml
<job><motion x="10"/><pause milliseconds="50"/><motion x="20"/></job>
```

Insertion, deletion and reordering affect the same collection that XML traverses.
Decode skips formatting XML whitespace between children, rejects non-whitespace
text and unknown children, and retains every accepted typed occurrence in order.
Child records keep their ordinary scalar/attribute validation. Fixed arrays must
contain exactly their declared number of children; no text or payload padding is
invented. Existing `mixed=...` retains its significant text and padding contract.
The variant may remain private for XML load/save; its native editing helpers
retain the declared visibility and require application-authored access when private.

## Storage, performance and failure behavior

Each occurrence uses the backend's existing native variant representation.
C++ stores the active payload; other outputs retain their ordinary discriminator
and payload storage, including inactive fields where their native layout uses
them. This mapping adds no model fields, text alternative, order trace, pointer
registry or wire metadata. Output borrows payloads, traverses the collection in order and
dispatches on the selected alternative; it does not construct a document tree.
Nested owning records retain the same contract. C++ selects the token path
through ordinary `xml::serialize` and `xml::deserialize` entry points as well;
other outputs use their existing dedicated XML methods and providers.

For C++, variant size includes the largest alternative, its discriminator and
platform padding. Other outputs retain their backend's existing storage costs.
There is no guarantee of less memory than separate arrays for every set of
payload types. Decode uses normal bounded collection growth; output
buffers and scalar formatting can allocate. No CPU benchmark or hard process
memory ceiling is claimed.

The existing mixed-content policy bounds ordered occurrences:
`xml::limits::max_mixed_fragments` in C++, `Limits(xml_mixed=XmlMixedLimits(...))`
in Python, `XmlLimits({mixed: new XmlMixedLimits(...)})` in JavaScript/TypeScript,
and the ordinary mixed-limit overloads in Java, C#, Go, Kotlin, Swift, Rust and C.
Formatting whitespace does not occupy a fragment. Text-fragment storage is absent,
so `max_mixed_text_bytes=0` remains usable; nested scalar text retains its ordinary
limits. Output byte limits and operation callbacks apply normally. Failed owning
decode returns no partial model, and failed owning output returns no XML string.
Streaming sinks retain their existing partial-write contract.

## Compatibility and remaining work

The feature requires an explicit configuration entry and adds no schema syntax.
Existing field mappings, mixed content, public native storage and native encodings
are unchanged. Do not automatically replace two existing arrays with this model:
that would change the application's native API and wire layout and requires an
intentional schema migration. Existing separate-array schemas remain unchanged.

Fixed XSD binding and XSD export/compatibility tooling currently
reject this mapping before publishing generated files. Repeated XSD group
boundaries and arbitrary compositor validation remain
in OPC-R04. Supporting unconstrained ordered elements does not implement those
grammar checks. The [original C++ verification](../verification/verification-ordered-xml-2026-10-21.md)
and [portable verification](../verification/verification-ordered-xml-2026-10-22.md)
state the actual executed scopes. Existing schemas require regeneration to gain
the new non-C++ XML entry points; native names, field IDs, alternative order,
visibility and protocol mappings retain their contracts. Use portable identifiers
when selecting multiple outputs; for example, Rust reserves `move`.
