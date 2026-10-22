# Ordered XSD groups — 22 October 2026

Compiler/runtime **2.28.0**, schema language **1.21.0**. This completes the
`OPC-R04` ordered owning-storage contract: explicit record-variant arrays preserve
heterogeneous order, multiple typed choices and nested/repeated sequence/choice
groups in all eleven generated owning outputs. Existing separate arrays, variant
identities and native encodings retain their contracts; migration to ordered
storage is explicit. This does not close the other OPC/3MF parent requirements.

## Implementation

The pinned XSD supplies expanded child identities, complex payload types and
occurrence grammar. The compiler builds a bounded epsilon automaton and
determinizes reachable states. Generated readers and writers advance a scalar
cursor for each typed child and require an accepting final state before returning
output or publishing an owning candidate. Native backends use generated switches;
Python and JavaScript use static transition metadata. No per-event matching tree,
backtracking state or copied native payload is needed. Formatting whitespace
does not advance the cursor or consume a mixed-text allowance.

Compilation bounds cover depth, state count, transition cells and work; extremely
large finite bounds fail before generated-file publication. Fixed native arrays
also enforce their independent exact storage cardinality. Existing flat arrays
remain rejected for heterogeneous grouped content because they cannot retain
cross-array order. Owning record alternatives are required; wildcard/all storage,
nil alternatives and text branches retain their separate mapping contracts.
Importing these new ordered native layouts is a separate declaration-authoring
capability; this feature binds explicitly authored storage.

XSD export omits private fragment helpers, emits ordinary payload types and
retains the original sequence/choice tree and occurrence intervals. Unbound
ordered mappings export repeated choices, including exact fixed-array extents.
Fixed storage bound to an XSD requires an exactly matching grammar length for
export. Compatibility uses bounded language inclusion in reader direction,
independently of native branch numbering; narrowed languages fail before output
publication.

## Executed checks

- All ten actual non-C++ Windows SDKs pass **34** independently classified input
  decisions each: Python, JavaScript, strict TypeScript, Java, C#, Go, Kotlin,
  Swift, Rust and C. The matrix includes intact/reversed branch pairs, missing
  and excess groups, broken pairing, wrong namespaces, invalid child values,
  fixed storage and one-tight fragment admission.
- Every successful grouped consumer removes the final native fragment and
  requires its writer to reject the incomplete group. Successful ordinary XML
  round trips preserve all four native protocol byte streams.
- An independent lxml XSD provider classifies the corpus against the original
  schema and compares the exported schema's language on the same documents.
- C++ passes **511** complete binary child traces through an independent native
  XSD provider and generated token decoding. Successful outputs preserve expanded
  names, payload text and order; incomplete edited output rejects without replacing
  the caller's previous result. Exact/one-tight resource and fixed-array checks
  are maintained in the same executable.
- The shared XSD contract suite passes **23** GTests, including **1,023** short
  traces comparing nullable/unbounded deterministic matching with the separate
  particle matcher. State/work exhaustion and an overflowing-scale occurrence
  request reject through bounded generation.
- CLI checks pass combined generation for ten backend pairs, ordered XSD export,
  compatible exported-schema comparison and protected narrowing rejection.
  Existing malformed storage, incompatible XSD and unsupported mixed export
  failures continue to preserve old output files.

The maintained sources are `xml_ordered_portable_test.py`,
`xml_ordered_xsd_test.cpp`, `xml_xsd_contract_test.cpp` and
`xml_ordered_generation_test.py`. Actual SDK compilation treats warnings as errors
where the existing runner supports that policy. The final strengthened matrix is
recorded in ignored `out/opc-2-28-grouped.log`; earlier development checks are in
`out/opc-grouped-all.log`. Useful native builds remain under ignored `out/build/`.
The matrix uses installed Windows SDKs and the pinned existing example providers;
no Linux, sanitizer or large-workload performance qualification is claimed by this
record.

## Release verification

The final root `./make.ps1 test` rebuild with MSVC x64, RelWithDebInfo and four
parallel CTests passes **283/283** default entries in **482.61 seconds**, without
targeted retries. This includes installed-package consumption and same-major
package discovery for older 2.27 releases. The CLI reports compiler/runtime
**2.28.0**, schema language **1.21.0**. The compiler executable SHA-256 is
`9f4dbe18c015288b5486a8330e4483d972d820fdc1d4cb3b10f002381467e6b0`, built
from the modified worktree based on commit
`6dea61fd4f4378dbb98865074e1e61921628eb14`. The complete final log is
`out/opc-2-28-regression.log`. An earlier development run passed 283/283 in
500.43 seconds; it predates the final release-version update.

Both editor implementations, READMEs, changelogs and Marketplace listing payloads
were reviewed. Schema syntax, language version, generated output paths and
navigation boundaries remain unchanged; no editor content/version change or
extension installation/publication is part of this update. `.gitignore` already
covers all task-created output under `out/`. Unused inspections and failed
task-local cache output were removed; useful consumers and final logs are retained.
