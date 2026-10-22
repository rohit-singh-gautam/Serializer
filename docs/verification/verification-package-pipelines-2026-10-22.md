# Generated package pipelines: 2026-10-22

This record closes `OPC-R06` and extends the fixed-singleton `OPC-R06a`
checkpoint. All eleven actual Windows SDKs executed the shared independently
produced package pipeline corpus. The final combined repository regression is
tracked separately; these results do not imply that later unrelated edits have
passed a fresh full regression.

## Implemented contract

Generated roles preserve their bound native model, XML root/type, MIME and
container identity while selecting XML/JSON/BINARY_NONE and stored/DEFLATE
profiles. Fixed roles reject overrides; defaults permit declared runtime
codec/compression selection. Old compression-only APIs retain their behavior.
Positional-only output rejects JSON defaults/overrides before invoking callbacks.
Selected XML callbacks remain XML-only and reject native envelopes.

Configured required `xsi:type` uses expanded identity. Root
`mc:MustUnderstand` resolves original-scope prefixes against the known plus
explicit vocabulary inventory; only this authorized root marker is consumed.
Missing/wrong types, unknown/unbound capabilities and unknown core content reject.
Internal/external/both byte roles preserve exact inert external URIs without
fetching them or writing ZIP members. External occurrences never become internal
rename provenance or relationship source roles.

Original internal physical occurrences remain associated with whole reordered
parts. Explicit rename plans rebind source scopes and internal targets together,
including runtime profile selection and Save. Case-equivalent original OPC names
identify the same source. Collisions, missing/removed and ambiguous occurrences
reject before publication. Native model fields, protocol bytes, old role/budget
layouts and schema language 1.21.0 remain compatible.

C/Rust package XML adapters retain namespace and foreign payload companions per
part. C singleton `_srl_xml_<role>` and repeated `xml_state` pointers are owned by
the aggregate and borrowed by `as_input`. Rust aggregate `xml_companions` keys
associations by role/original physical name without extending `ModelPart` or
native model layouts. Selected decoder presentation preparation constructs no
owning payload; encoders see the exact borrowed native model/sidecar. Native
JSON/BINARY_NONE serialization never inserts XML metadata.

Native traversal, temporary XML filtering/replay, preservation and publication
share the original operation and first failure. Additive native preservation
callbacks preserve existing resource enums/entrypoints; logical preservation is
admitted before owning copies. Opaque provider/factory internals retain their
published accounting/preemption boundary. These counters are not process-peak
memory guarantees.

## Actual SDK and independent archive evidence

`test/package_pipeline_test.py` authors 16 common inputs and independently
inspects eight successful archives per backend. It covers XML/JSON/BINARY_NONE,
truncation/trailing input, root type/capability controls, fixed native JSON roles,
inert external relationships, owning Load/Encode/Save and old destination
preservation. Actual SDKs: C++, C, Rust, Python, JavaScript, TypeScript, Java,
C#, Go, Kotlin and Swift. TypeScript additionally passes strict API compilation;
its archive decisions run through the generated JavaScript runtime.

Logs are `out/opc-pipeline-{cpp,python,javascript,typescript,java,csharp,kotlin,swift,rust}.log`,
`out/opc-pipeline-go-common.log` and `out/opc-pipeline-c-final.log`. The latest
C/Rust companion integration replays all 16/eight in
`out/opc-pipeline-c-companion-common-final.log` and
`out/opc-pipeline-rust-companion-common-final.log`. C positional-only generation
passes 16/six independently inspected archives in
`out/opc-pipeline-c-binary-companion-final.log`; Rust matches 16/six in
`out/opc-pipeline-binary-rust-final.log`. Latest C/Rust snapshot SHA-256:
`a48b658e655b9b367d6e6842c118be1cf6077c68c1cd75278a78c3fcb9c67833`.
Each build directory retains commands and `verification.json` archive hashes.
Java/C# exercise exact/one-tight shared encode/decode work/allocation and original
cancellation exception identity. Go's additional generated pipeline matrix runs
five SDK profiles and 92 assertions (`out/opc-pipeline-go.log`), plus 43 selected
codec assertions (`out/opc-pipeline-go-selected.log`).

The relocation corpus runs 14 independent inputs in each actual SDK. Produced
archives/assertions: C++ 21/48, Python 21/60, Java 21/140, JavaScript 21/165,
TypeScript 28/172, Go 21/107, C# 21/128, Kotlin 21/140, Swift 23/138,
Rust 25/87, C 26/375. Logs `out/opc-package-<language>.log` and later native
regressions `out/opc-pipeline-c-relocation-final.log`,
`out/opc-pipeline-rust-relocation-bridge.log` preserve the commands/results.

`test/native_package_xml_graph_test.py` executes C and Rust SDKs against seven
independently authored inputs and eight independently observed archives each
(the C consumer executes 169 additional ownership/publication assertions).
It associates singleton/repeated part labels, child-local QName aliases, opaque
foreign attributes/tails and source relationships after whole-record reordering,
canonical rebinding, case-equivalent explicit renames, selected callbacks,
runtime native JSON and Save. Decode failures preserve an existing aggregate.
Exact and one-tight work, allocation and preserved-byte quotas run in both
directions; cancellation preserves the original Rust Arc/C error result.
Selected C decoder assertions confirm no preconstructed child payload; selected
encoders check borrowed model pointer identity. Failed Save retains destination
bytes and original model/sidecar. Logs:
`out/opc-pipeline-c-xml-graphs-final.log`,
`out/opc-pipeline-rust-xml-graphs-final.log`; reports under
`out/build/opc-completion/pipeline-{c,rust}-xml-graphs-final/verification.json`.

The native fixed-profile graph test independently reads native QName/foreign
model bytes using JSON and BINARY_NONE and admits XML-selectable defaults now
that package companions compose. The shared protocol-only corpus separately
checks positional-only generation and JSON rejection; its backend results are
retained independently from default-protocol results. Earlier records which
explicitly gated C/Rust package companions describe historical checkpoints,
superseded by this implemented pipeline and actual SDK proof.


The later declaration-dispatch snapshot
`0d86b6b21bf0586ee20b6929ef9c49816b801fe6c3144fea34c84a9cc1806a84`
replays the complete C companion seven/eight matrix and C#/Go common16/eight
pipelines: `out/opc-pipeline-c-xml-graphs-declaration-final.log` and
`out/opc-pipeline-{csharp,go}-declaration-final.log`. Native/C#/Go private
declaration resource proof is recorded [separately](verification-native-declaration-operations-2026-10-22.md).

## Fresh integrated native regressions

The later normal-access Windows wrapper executes `serializer_package_c` and
`serializer_package_rust` successfully with compiler SHA-256
`045c5666d40ecbe614edb0a255a163294682d1a11e9ba3efe011045aaad2d1be`.
Each actual SDK replays 260 independent ZIP/OPC decisions and 16 shared
application fixtures. Their maintained tests now replace the former companion
rejection expectation with an additional independently authored nested-QName
package. Actual C/Rust consumers load, encode and reload it, compare all four
native protocols, and check its output QName's expanded identity independently
using ZIP/XML parsing. C additionally checks that `as_input` borrows the exact
model and companion pointers. Reports and command logs remain under
`out/build/ReleaseWindows/test/{c_package,rust_package}/`.

The Rust regression also derives one-tight XML node and attribute quotas from a
successful load and requires native origins `profile::model.points` and
`profile::point.z`. Secondary root/capability parsing keeps its real shared work
and allocation charges while leaving logical node/attribute quotas to the primary
model codec; a header-only pass cannot replace these member diagnostics with the
package role name.

The same fresh wrapper passes both native operation-policy SDKs and the
multi-output ordered-generation gate. Declaration collectors/dispatchers retain
the existing exclusion for mixed-fragment storage helpers, which have no
standalone XML contract; their owning mixed/ordered records continue to validate.
See `out/opc-final-windows-verified.log`. These named test passes are recorded
independently from completion of the remaining full-wrapper tests.
