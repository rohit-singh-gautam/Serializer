# C/Rust OPC infrastructure encodings — 21 October 2026

The generated owning C and Rust profiles now accept BOM-marked UTF-16 content-type
and Relationships parts in both byte orders. An optional encoding declaration
must identify `UTF-16`, matching the existing C++ infrastructure policy. Ordinary
generated model XML remains UTF-8; output infrastructure remains deterministic
UTF-8. This closes this specific encoding gap in `OPC-R08`, independently of
Relationships markup compatibility and the broader edition/corpus requirements.

The normative encoding restriction is ECMA-376 Part2 fifth edition section6.2.5(a),
printed page13, in the [official standard bundle](https://ecma-international.org/wp-content/uploads/ECMA-376-2_5th_edition_december_2021.zip).
It limits the defined infrastructure XML and its declaration labels to UTF-8 or
UTF-16. [XML1.0 section4.3.3](https://www.w3.org/TR/xml/#charencoding) separately
distinguishes UTF-16BE/LE from its mandatory UTF-16 scheme. Endian-labelled
declarations therefore reject under the OPC infrastructure rule; the corpus does
not claim that every such document is intrinsically malformed outside OPC.

C validates declaration code units without alignment assumptions and lets libxml2
decode the original bytes. Rust validates every scalar before reserving the exact
UTF-8 conversion size. Its declaration adjustment affects only the encoding
pseudo-attribute inside the initial XML declaration. Encoding-like text in comments
and ordinary payload text is untouched. DTD/entity and external loading policies
remain enforced by the existing checked metadata reader.

The independent [encoding corpus](../../test/package_utf16_corpus.py) adds 27
cases shared by the actual C and Rust SDK harnesses:

- Eight positive variants cover UTF-16LE/BE manifests, Relationships, both together,
  omitted declarations and supplementary Unicode scalars.
- Nineteen negative variants cover conflicting/unsupported declaration labels,
  missing BOMs, odd byte counts, isolated surrogates, UTF-32, XML1.1 and DTDs.
- Independent Python XML parsing verifies the original roots of positive UTF-16
  parts. ZIP fixtures are authored independently with valid physical records and
  CRCs; malformed XML is not hidden behind a malformed ZIP failure.

Both SDK suites pass **122 independent cases and 16 application cases**. MSVC
19.51 compiles generated C11 with `/W4 /WX`; Rust uses Cargo 1.99.0 with locked
offline `roxmltree`/`xmlparser` providers. Input metadata accounting equals the
independently computed original encoded-byte total. Exact metadata, work and
allocation budgets succeed; reducing any by one rejects. C additionally verifies
that these failures preserve an existing candidate's owning pointer. Generated
allocation-failure rollback, native wire/profile/reference behavior and canonical
save checks in the existing harnesses continue to pass.

The executed compiler SHA-256 is
`34e67ca28294018920cc54cd26b4ae6f67ed1f42ef67c4ed7c9aac8ef846cf7b`.
Builds use the root Windows wrapper and CMake pipeline; native processes inherit
headless error handling. Commands, source hashes, accepted case manifests and
final results are retained in ignored build directories:
`out/build/pending-xml-completion-2026-10-21/c-package-utf16/` and
`rust-package-utf16/`, including each `verification.json` and `acceptance.log`.
The frozen compiler is in `compiler-utf16/`. These useful outputs are retained
for replay; unused failed dependency-copy scratch was removed.

No Linux, sanitizer, complete OPC/3MF conformance or provider process-memory claim
is made by this encoding replay. Namespace-sensitive generated model graphs and
metadata markup compatibility retain their separate qualification boundaries.
