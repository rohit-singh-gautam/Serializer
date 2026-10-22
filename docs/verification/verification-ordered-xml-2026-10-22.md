# Portable element-only ordered XML — 22 October 2026

Compiler/runtime **2.27.0**, schema language **1.21.0**. This extends the explicit
`ordered=member` mapping from C++ to Python, JavaScript, TypeScript, Java, C#, Go,
Kotlin, Swift, Rust and C. It is a slice of `OPC-R04`, not completion of that
requirement or the complete OPC/3MF request. `OPC-R01`, `OPC-R02`, `OPC-R03`,
`OPC-R04`, `OPC-R06` and `OPC-R10` remain open in [the backlog](../pending.md).

## Contract and compatibility

The existing native array contains private or public record-variant fragments.
The XML-only metadata has an explicit element-only flag and fixed extent;
there is no text alternative, synthetic padding, model field or native wire
change. Backend fragment discovery recognizes both `mixed` and `ordered` owners.
Generated readers skip XML formatting whitespace, reject other text and unknown
expanded child names, preserve heterogeneous order, and enforce fixed arrays on
input and output. Typed child text keeps its ordinary scalar limits, independently
of the owner's zero mixed-text allowance.

Regenerate non-C++ output with compiler 2.27.0. C++'s existing mapping and native
APIs remain unchanged. The usage guide explains [explicit migration boundaries](../serialization/ordered_xml.md#compatibility-and-remaining-work):
separate arrays do not migrate automatically into one variant array. Fixed-XSD
grouped/repeated bindings, XSD export and arbitrary compositor validation remain
rejected before generated-file publication. Those require further implementation.

Both editor implementations and listing payloads were reviewed: this change adds
no schema syntax, language version, target-language name or generated output path.
The existing INI option gains backend implementations, so editor content and
extension versions are unchanged. Nothing is published or installed.

## Executed verification

The maintained `test/xml_ordered_portable_test.py` generates real source, compiles
the selected SDK, runs fourteen independent acceptance/resource decisions and
observes emitted XML with Python's independent ElementTree parser. Successful
round trips compare exact bytes in JSON, positional, integer-key and string-key
native protocols. Cases include empty content, formatting CDATA/comments/PIs,
interleaved variants, zero/exact/one-tight fragment bounds, non-XML whitespace,
unknown names/namespaces, invalid child values and exact/short/excess fixed arrays.
C additionally checks unchanged destination storage after rejected decoding.
Invalid-input assertions run immediately after decoding, before encoding, so a
writer failure cannot conceal a reader that incorrectly accepted invalid XML.

| Actual Windows consumer | Result |
| --- | --- |
| C++ maintained ordered fixture | 11/11 GTests pass |
| Shared XML configuration/binding contracts | 10/10 GTests pass |
| Python | 14/14 decisions and four native protocols pass |
| JavaScript | 14/14 decisions and four native protocols pass |
| TypeScript, strict actual compiler and Node execution | 14/14 decisions and four native protocols pass |
| Java, JDK 17 target, all warnings treated as errors | 14/14 decisions and four native protocols pass |
| C#, .NET 8, nullable checks and warnings treated as errors | 14/14 decisions and four native protocols pass |
| Go | 14/14 decisions and four native protocols pass |
| Kotlin, warnings treated as errors | 14/14 decisions and four native protocols pass |
| Swift, optimized, warnings treated as errors | 14/14 decisions and four native protocols pass |
| Rust, locked dependencies and warnings denied | 14/14 decisions and four native protocols pass |
| C, MSVC C11 and LibXml2, warnings treated as errors | 14/14 decisions and four native protocols pass |

The combined ten-SDK command completes successfully, with all 140 decisions
passing in one run. The compiler executable SHA-256 is
`14128cc027805488ff0c63ecf014d19df9b351331c6aac9dbb78bf295736cac2`, built
from the modified 2.27.0 worktree based on commit
`8a134bad57bcf5a753234434f133ba12b092717e`.

Existing mixed-content regressions also pass: 21 independent probes and three
protected helper-use failures in each of Go/Java/C#, Kotlin/Swift and C/Rust
(147 decisions and 21 helper gates across seven actual SDKs). The existing
JavaScript/TypeScript suite passes 4/4 tests, including strict TypeScript,
chunked input and edited native fragments. Python's rich XML suite passes 20/20
tests, including mixed content, lexical constraints and scoped QName unions.

`xml_ordered_generation_test.py` passes ten combined C++/other-backend requests
and three protected failures: a text alternative, a fixed-XSD override and XSD
export. Existing output files survive rejected requests. Its portable generation
control uses `motion`, because Rust reserves the older C++ fixture's `move` name.
Shared contract tests now require owning-backend admission while retaining the
XSD export rejection. The default CTest suite includes Python ordered checks;
the all-language option additionally registers the other nine SDK consumers.

The root Windows wrapper rebuilt the native targets and ran all 282 default
CTest entries. Initially 268 passed; fourteen encountered sandbox execution or
temporary-directory access errors. Retrying those fourteen outside the sandbox
with a workspace-local temporary directory passed all fourteen in 212.44 seconds.
Thus all 282 distinct entries passed across the initial run and targeted retry;
this was not one uninterrupted passing run. Installed-package consumption and
CMake package-version compatibility passed. `serializer --version` reports
compiler/runtime 2.27.0 and schema language 1.21.0. The strengthened Python ordered
consumer also passed a final targeted CTest run.
No Linux compiler/runtime run, sanitizer run, large-workload benchmark, full XSD
compositor qualification or complete OPC/3MF conformance is claimed here.

## Environment and retained outputs

Windows generation/build uses the root `./make.ps1` wrapper and CMake's real
target pipeline with the installed MSVC toolchain. C's fixture uses Ninja
Multi-Config so the existing example runner's `Release/` executable convention
is retained. Swift needed execution outside the sandbox because the sandbox
denied `swift-frontend.exe`; Cargo needed network access for the fixture's pinned
`xmlparser` dependency. Both actual SDK runs subsequently passed. Initial harness
failures involving UTF-8 byte input, a path separator, minimal Node declarations,
SDK discovery and CMake generator selection were corrected before qualification;
they are not counted as passing runs.

Task-created SDK directories are managed by temporary-directory scopes and
removed on completion. Useful native builds and final verification logs remain
under ignored `out/`, already covered by `.gitignore`: `ordered-all-sdks.log`,
`ordered-cpp-focused.log`, `ordered-generation.log`, `ordered-legacy-*.log` and
the native regression logs. The retained `out/build/opc-complete-2026-10-22/legacy-*`
directories contain useful generated consumers, provider dependencies and SDK
build outputs. Unused scratch searches, superseded isolated logs, empty SDK
temporary parents and the failed Cargo cache were removed. The dependency fetched
by the approved Cargo run remains in the ordinary user cache. No Git commit was
created. Changed-document local link paths were checked: 1,944 resolve; anchors
were not checked by that diagnostic.
