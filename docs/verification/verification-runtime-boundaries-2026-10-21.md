# Eleven-output XML namespace and provider boundaries

This development checkpoint extends the earlier
[four-output namespace checks](verification-namespace-budgets-2026-10-21.md).
It qualifies actual generated SDKs against an independent inventory of physical
XML declarations. It does not relabel earlier compiler captures as a later
release build.

## Independent admission matrix

The maintained `test/xml_namespace_budget_test.py` and
`test/xml_namespace_budget_consumers.py` generate and compile C++, C, Rust,
Python, JavaScript, TypeScript, Java, C#, Go, Kotlin and Swift consumers. Expat
independently inventories the emitted XML before deriving admission boundaries.
Six profiles cover ordinary names, a default namespace, qualified attributes,
an empty-default reset, repeated qualified attributes and the predefined `xml`
namespace. Every quota from zero through 128 is tested for encode and decode.

All **35,604 ordinary admission decisions pass**: structural and text limits in
eleven outputs, plus C++'s separate namespace limit. C++ token readers at chunk
sizes of one and 65,536 bytes add **9,288 passing decisions**, for **44,892** total.
Tree and token readers use the same physical-declaration convention; inherited
scope copies retain their separate allocation and work costs.

The oracle respects each public adapter's documented accounting convention:

| Outputs | Structural and text convention |
| --- | --- |
| C++ | Elements and ordinary attributes have separate counters; namespace declarations have their own limit; XML text values have their own byte limit. |
| C, Kotlin, Swift | Elements, ordinary attributes and physical namespace declarations share the structural counter. C additionally bounds each individual value. |
| Java, C#, Python | Text accounting includes braced expanded names, values and declaration prefix/URI bytes. |
| Go, Rust, JS, TS, Kotlin, Swift | Text accounting includes namespace URI plus local name, values and declaration prefix/URI bytes. |

The resulting fixes charge declarations when emitted or read, rather than
charging every inherited scope as another physical declaration. Go explicitly
tracks generated attribute aliases and reuses inherited bindings, preserving
ordinary output spelling. C avoids a redundant predefined `xml` declaration
that libxml2 hides on input. C++ retained trees and generated roots now admit
physical declarations through the same limits. Native codecs and expanded XML
identities retain their existing contracts.

## Capture provenance

The following selected captures form the aggregate; failed or superseded rows
are not counted as passes.

| Capture under `out/build/pending-runtime-providers-completion-2026-10-21/` | Selected outputs | Decisions | Compiler SHA-256 |
| --- | --- | ---: | --- |
| `matrix-fixed/summary.json` | C++, Rust, Python, JS, TS | 17,028 | `312fe92f4d28063ff1a3edd36fcc22bfdf9808a814e4710c6c0dddadf6d87310` |
| `portable-fixed/summary.json` | Java, C#, Kotlin | 9,288 | Same `312fe92f…` image |
| `remaining-fixed/summary.json` | C, Go, Swift | 9,288 | `fdd015a516f50d11f6cbd88140053fbc718ce5a337b7a612d40b2a813f0c5d04` |
| `matrix-fixed/cpp-token-summary.json` | C++ at both token chunk sizes | 9,288 | Same `312fe92f…` image |

`namespace-aggregate.json` retains the selected per-profile results. The earlier
C predefined-namespace failure remains in the original matrix report and is
superseded by the corrected C capture. This provenance is intentionally separate
from the final integrated release checks.

## Real prepared providers

The `fdd015a5…` compiler passes the actual Java/C#/Kotlin prepared-provider suite
and C/Rust/Go/Swift suite using the installed libxml2 callback bridge. These
exercise bounded input, prohibited remote schema/entity loading, concurrent
valid/invalid reuse, closed state and native-codec controls. Go's 403 callbacks
include rejection after a validator completes and signals cancellation. Kotlin
and Swift execute the corresponding owning-output cancellation controls.

Python's actual `lxml` completion tests and C's final-poll destination rollback
have a separate [publication record](verification-operation-publication-2026-10-21.md).
JavaScript/TypeScript use the real prepared `libxml2-wasm` engine and independent
worker handles; their exact replay and readonly completion checks belong to that
record and the integrated checkpoint. A callback-shaped stub is not evidence of
an XSD engine's correctness.

The freshly linked 2.22.0 C++ prepared-provider executable passes **4/4 tests**
against the current native runtime: schema validity, concurrent reuse,
schema/entity/instance-hint no-fetch and limits/cancellation before publication.
`cpp-provider-final.log` under the aggregate build directory records this replay.
The compiler image at this checkpoint has SHA-256
`2154768e2a80d6bdfb75ce54e241dd2ece660082f6bfe871c3772bdbac90059d`.
Together these ordinary supported-profile checks close `XML-B13`. Later public
C#/Kotlin/Swift cancellation policies also pass external-library checks in the
publication record. Large combined-operation contracts remain pending.

Logical resource admission does not bound opaque provider heap peaks or forcibly
interrupt foreign provider code. Cancellation and deadline checks surround
supported provider calls and run at the documented internal polling points.
Applications needing hard preemption or process-wide memory ceilings must impose
those deployment limits externally. This matrix does not establish arbitrary
rich-XSD backend parity or qualification of every combined operation graph.
