# Opt-in C++ ordered element-only XML — 21 October 2026

Compiler/runtime **2.25.0**, schema language **1.21.0**. This implements the
explicit non-XSD C++ mapping documented in [the usage contract](../serialization/ordered_xml.md).
It leaves OPC-R04 open: fixed-XSD repeated/grouped binding, schema tooling and
other generated backends still reject the new mapping before publication.

## Implemented behavior

`ordered=member` selects one ordinary owning array of fragment records, each
with one owning variant containing at least two ordinary record alternatives.
`ordered.variant` and `ordered.child.branch` provide sparse selectors. Native
field IDs, variant order, payload visibility, stored model fields and encodings
are unchanged. Public variant editing helpers retain public access; private
payloads keep their original private editing API and use XML-specific bridges
for owner load/save. No hidden order trace or text alternative is introduced.

The collection itself owns the ordering contract. Output borrows active payloads
and emits them in that order; vector insertion, deletion and reordering have
direct meaning. Input ignores formatting whitespace between children and rejects
other text, unknown children and malformed child payloads. Fixed arrays require
exact cardinality rather than synthetic padding. Existing mixed content retains
significant text, its string branch and its earlier padding behavior.

## Executed checks

The actual Windows compiler executable SHA-256 is
`3f266233d04488d74157b6139b475d8d8b4c07cc458b666890278039e1361f9c`.
The compiler was rebuilt through `./make.ps1 configure` and the root CMake target
pipeline with the installed MSVC toolchain and C++20. Its CLI reports compiler
2.25.0 and schema language 1.21.0.

| Check | Result |
| --- | --- |
| Root Windows `./make.ps1 test` | Full native build and 281/281 CTests pass in one uninterrupted run; CTest time 868.60 seconds, including installed-package and same-major package-version checks |
| Maintained generated `xml_ordered_test.cpp`, Windows | 11/11 GTests pass with warnings as errors |
| WSL Linux runtime, Windows-generated headers | 11/11 GTests pass with GCC/C++20 and warnings as errors; the runtime was configured through root `make configure` and built through CMake |
| Shared content/binding contracts | 28/28 GTests pass, including element-only observation, configuration exclusions and protected backend gates |
| Maintained `xml_ordered_generation_test.py` | 13 atomic rejection cases pass: ten other backends in combined C++ requests, an unwanted text alternative, fixed XSD and XSD export; prior generated files remain byte-identical |
| Existing C++ mixed/ordinary output | The retained pre-change 73,561-byte mixed header and 51,691-byte ordinary separate-array header are exactly identical after regeneration |
| Existing Go/Java/C# mixed-content suite | 21 independently checked probes per actual SDK, 63/63 pass, plus three protected helper-misuse cases per SDK |

The native control is a separate XML-disabled generation with identical native
field names/IDs and alternative order. C++ checks equal object sizes and exact
bytes in all four native protocols before and after XML encode/decode. This
proves parity for that fixture rather than a universal ABI guarantee.

Other native cases cover authored edits, private payload load/save, independent
expanded child names, nested direct output with a zero tree budget, four input
chunk sizes, independently authored repeated-choice XSD validation, rejection of
unexpected text/attributes/namespaces, fixed cardinality and exact/one-tight
output/fragment bounds. A 25,000-item case retains the original vector address,
interrupts through a caller-owned exception token without replacing the previous
result, then successfully exports and decodes every original value on a fresh
operation. These checks do not establish zero allocations, a process memory
ceiling or a measured CPU improvement.

The existing C++ mixed header's SHA-256 is
`18de409b7970fe912c271ab76b1c5d5a744c12df6ca856a771b5c588932c5767`.
The ordinary separate-array header's SHA-256 is
`e0dca3a2ec7827c46f8895fcdc3fbda0547c5340d3dd693669a224672b2bb5d3`.
An initial Go/Java/C# text comparison used older captures from before the final
2.24 QName changes; it was not a valid before/after baseline for this change.
Their maintained actual SDK suite was rerun instead and passes as recorded above.

## Compatibility limits and retained evidence

No existing model is rewritten into the new representation. Replacing two old
arrays with one variant array remains an explicit schema/API/wire migration.
The new option does not alter legacy `mixed` interpretation or activate on
ordinary fields. Both editors were reviewed: schema syntax/version, generated
native names and output paths are unchanged, so no extension release is required.
Schema language version remains 1.21.0; compiler package compatibility continues
within major version 2.

The Linux check executes the maintained C++ consumer against the current Linux
runtime build and actual XML/zlib providers, with Zstandard/LZ4 disabled. It uses
the current Windows compiler's generated headers and the retained GoogleTest
1.18.0 libraries. It is not a Linux compiler rebuild or full Linux regression.

The first host build could not launch optional `clang-format` for the native
control; its CMake generation now explicitly disables formatting, matching the
XML fixture. An initial test tried to use private native editing helpers from
outside their class. The final tests author through a public variant control and
separately verify private owner load/save; generator visibility was preserved.
Neither failed attempt counts as a passing check.

Useful build outputs, the legacy comparison, SDK consumers and final verification
logs are intentionally retained under ignored `out/build/opc-r04-2026-10-21/`.
The final records are `regression.log`, `focused.log`, `linux.log`,
`generation.log`, `legacy-cpp-identical.log`, `legacy-mixed-sdk.log` and
`doc-links.json` (2,240 local documentation links checked, none missing).
Existing `.gitignore` covers them. The earlier explanatory R01/R04 examples are
also preserved. No live Marketplace publication or Git commit is performed.
