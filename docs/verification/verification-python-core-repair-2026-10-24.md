# Python Core routing qualification — 24 October 2026

[Migration](../../migration.md#python-core-xml-routing-repair-in-2331) ·
[Pending qualification](../pending.md#qualification-and-distribution) ·
[Official corpus workflow](../../qualification/core_conformance/README.md)

Compiler/runtime **2.33.1**, schema language **1.23.0**, backports the independent
Core owning-tree repair from main `60468b0248fbcc096969ded0d936e1a008a6a22e` in
focused commit `d830c60`. It does not merge main as a whole. The separate namespace
token feature from that commit regresses readonly/scoped projection and is
excluded; [QA-B01](verification-qualification-bugs-2026-10-24.md#qa-b01-main-namespace-token-projection-metadata)
tracks its repair. No speculative admission bypass is used.

## Failure, repair and actual public APIs

The immutable 2.33.0 compiler emits the Core model but its public Python XML API
rejects every one of the **166 originals within the common input policy** with
`This XML mapping requires a token-compatible owning graph`. The other nine
originals are outside the explicit 16 MiB policy. Even the independent local
739-byte minimal fixture reproduces the guard failure. The earlier 2.21.0 corpus
record qualified this supported whole-document graph.

The focused repair emits complete legacy-graph admission and permits the existing
bounded owning-tree adapter only when every child contract implements it.
Token-only nil/presence, maps, variants, mixed content, identity, inheritance and
lifecycle requirements cannot enter a weaker tree decoder. Direct chunked input
retains its token capability gate. Native fields, public native APIs, schema
headers and JSON/binary wire identities are unchanged.

The final Windows MSVC 19.51.36260.0 Release C++23 compiler image has SHA-256
`638013f749c6c909b69a6566c49bca4792e594f0399d73cd91fe8df9fe8d2cf8`.
`serializer --version` reports 2.33.1/language 1.23.0. Actual installed CMake
discovery accepts exact 2.33.1 and compatible 2.33.0/2.32.0, rejects future
2.33.2, and compiles/runs a consumer checking the public version constants.

## Executed Windows qualification

The final source passes **8 projection, 20 rich XML, 13 token and 3 Core tests**,
plus the complete **24 selected Python XML/package CTests** in 94.92 seconds.
All ten unaffected XML generators reproduce the frozen 2.33.0 bytes exactly.
The rich fixture refresh preserves native pattern success without lxml, requires
lxml for explicit prepared schema validation, checks the current QName-union
rejection category, and independently validates the already implemented QName-list
XSD export. These are corrected expectations, not relaxed validation.

The final official Core run uses all **175 pinned archives/model parts**, revision
`f483d3beee069e7adbac125d1f7baa36b5b3e5bf`. Independent unchanged-XSD
`.NET XmlSchemaSet` validates all original streams; **169 are XSD-valid and six
are invalid**. The separate xmlschema 4.2.0 oracle agrees on all **111 small
models**. Upstream positive/negative labels are not substituted for XSD validity.

| Actual owning consumer | Edited XML roundtrips | Declared rejections | Outside 16 MiB profile | Independently valid emitted models |
| --- | ---: | ---: | ---: | ---: |
| C++ | 156 | 10 | 9 | 156 |
| Python | 156 | 10 | 9 | 156 |

The ten declared rejections distinguish six invalid originals from four cumulative
resource-limit cases. C++ also checks persistent native fields through binary
equality before/after editing XML. Python's final dispositions exactly match the
earlier full-main prototype, but the committed narrow repair additionally retains
passing projection behavior. The final Python corpus phase takes **908.407 seconds**
on the shared host; this is not an isolated throughput threshold.

Generation still admits only C++/Python for this complete rich Core graph.
C#/Java/Kotlin/Go reject the finite native-counter boundary; C/Rust/Swift/JS/TS
reject unsupported rich shapes. The maintained runner now checks each current
diagnostic exactly instead of expecting the old common reason. These nine
admission decisions do not claim rich-Core runtime support in those SDKs.

## Evidence and scope

Raw logs and actual binaries remain beneath ignored
`out/qualification-2026-10-24/`: immutable 2.33.0 artifacts, the separately
archived full-main prototype, final `windows-patched-msvc/`,
`windows-final-validation.json`, direct `final-*-regression.log` files and
`official-core-final/`. Python dispositions SHA-256 is
`f3b31a56abd7cf7582b1f533f3132a24b0d8258452fd3773279a6412675991d0`;
its independent output-oracle SHA-256 is
`dd0f06ed7cdc36643be1cd554b660d691569392565d1646bc9cabefe34d1908b`.

Native Linux repaired-compiler results and current Python measurement/parity
supplements are recorded in the [final release qualification record](verification-qualification-distribution-2026-10-24.md). Core XSD
admission does not validate geometry, application references, required vocabulary
understanding or arbitrary package/domain rules. Other pending XML/package
parents remain unchanged. Both editor packages remain compatible at 2.0.19;
public syntax/output layout did not change.
