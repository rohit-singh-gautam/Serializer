# ZIP/OPC reference qualification, 20 October 2026

[Package contract](../serialization/xml_packages.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

This record qualifies selected C++ and Python package tasks for compiler/runtime
**2.14.0**, schema language **1.20.0**. The starting Git revision is
`bd2ee2ef9239b65335367bd8de19be377ce84262`; this checkpoint includes uncommitted
working-tree changes. It covers the documented ASCII OPC subset, not complete
OPC/Office/3MF conformance or every platform/provider combination.

## Independent content-type corpus: CP-P13 and PY-P13

Two new native cases in [package_test.cpp](../../test/package_test.cpp) and two
generated-Python cases in [python_package_test.py](../../test/python_package_test.py)
exercise **41 decisions per backend**: 38 rejected inputs and three valid controls.
The native fixture authors literal infrastructure XML through the plain ZIP writer;
it does not use Serializer's OPC metadata writer. Python authors the metadata
independently and wraps it with standard-library `zipfile`. Its consumer is the
actual freshly generated `ProfileArchive` facade.

The corpus rejects identical and conflicting duplicate Defaults and Overrides,
ASCII case aliases, percent-hex case aliases and whitespace-collapsed Override
aliases. Each duplicate case runs with both declaration orders. It also rejects
missing manifests, empty manifests, uncovered parts, missing required attributes,
empty/malformed MIME syntax, the relationship MIME type assigned to ordinary
payloads, and parameters on a reserved core-properties MIME type.

Positive controls retain extension fallback, case/escape-equivalent Override
lookup, and a per-part Override whose media type deliberately differs from its
extension Default. That precedence is valid and is not treated as a conflict.
The existing 2.14 readers pass the corpus before any production change; no runtime
repair, public API change or wire change was necessary. Native errors retain the
existing `missing_part` category for an absent manifest and `invalid_metadata`
for the other 37 invalid inputs; Python reports `invalid_metadata` for all 38.

The Windows native target rebuilds with MSVC **19.51.36260**, C++20, `/W4 /WX`,
CMake **4.4.2**, Ninja **1.13.2**, LibXml2 **2.15.3** and zlib **1.3.2**.
All **5/5 focused CTests pass in 0.50 s**: `package_test`,
`package_independent_interop`, `opc_profile_test`, `document_profile_test` and
`serializer_opc_exchange`. Fresh Python generation and all **16/16 Python package
tests pass**, with no skips, using CPython **3.14.3** and zlib **1.3.1.zlib-ng**.

Windows evidence:

- `out/build/backlog-2026-10-20/windows-package-p13-final.log`
- `out/build/backlog-2026-10-20/python-package-p13-final.log`

A separate Linux test-only overlay verifies the same frozen P13 source hashes.
The native file is rescanned, compiled and linked under the existing strict GCC
**15.2.0** Release build. All **5/5 focused CTests pass in 0.57 s**:
`package_test`, `package_independent_interop`, `opc_profile_test`,
`python_package_test` and `serializer_opc_exchange`. This profile uses CMake
**4.2.3**, CPython **3.14.4**, LibXml2 **2.15.2** and zlib **1.3.1**, with native
Zstandard **1.5.7** and LZ4 **1.10.0** enabled. The log is
`out/build/backlog-2026-10-20/linux-p13-focused.log`. The overlay's initial epoch
mtime was corrected before the reported native build, ensuring that Ninja
actually compiled the new cases. The earlier **200/200 Linux CTest** result
remains separate baseline evidence.

The compiler used for the P13 replay has SHA-256
`4c79e3a359e8335dd849569818aa0dc518b236684ad898eff6c9c8f3c5b21ef6`;
the runtime library has SHA-256
`8ff9712d2f0a77bf028977701897fc1ee06796d8354dfac7f21d0e56e541de7f`.
The P13 native test source is
`eebe251d67eb0b7abcf108c073f4e37ffaf294e616e8e6ffb45c86e743d939cb`;
the Python test source is
`e12e1b4b2eb74c24d18af53a09054dc96ca3bc8482379f0b545ac2c982af9c73`.

## Shared directory and exact-DEFLATE corpus: P03/P05 follow-up

The [independent fixture generator](../../test/resources/package_malformed_fixtures.py)
uses Python `zipfile` to create and independently validate one complete ZIP32/OPC
control. C++ and actual generated Python consume the same
[ten mutated ZIP files](../../test/resources/package_malformed_fixtures/README.md).
The six index cases alter the directory offset, directory extent, local-header
offset, local filename length, central extra-field length or directory entry
count. They must fail indexing, before payload decoding.

The four integrity cases alter a model entry's CRC, expanded size, trailing
DEFLATE data or truncated DEFLATE data. Local and central metadata agree; compressed
payload changes repair every affected entry/directory offset and compressed size.
These cases must pass ZIP indexing and fail reading the selected model entry.
Native aggregate checks additionally preserve the prior destination and verify
that no selected model callback executes. Python verifies that its complete-candidate
validation callback never receives a rejected aggregate.

The first actual generated-Python run reproduces an existing category defect for
central extra-field length overflow: `unsupported_feature` instead of
`invalid_archive`. The parser combined a truncated-range check with unsupported
disk/flag/method checks. The narrow runtime repair separates those conditions;
unsupported features retain their existing category. The baseline failure is
retained in `out/build/backlog-2026-10-20/python-package-p03-p05-baseline.log`.
The repaired runtime template has SHA-256
`b71693bfebe5b1c838c5369d6c77161f4852dda6a07e97f337fd68388104ffac`.
After rebuilding the actual compiler, all **17/17 generated Python package tests
pass**, with no skips. Three additional actual-generation probes retain
`unsupported_feature` for an encryption flag, an unsupported compression method
and a split-entry disk number: **3/3 pass**. The native package target is rescanned,
rebuilt and relinked; the same **5/5 Windows focused CTests pass in 1.18 s**.
These checks close **CP-P03/PY-P03** and **CP-P05/PY-P05** for this selected
reference corpus. They are separate from the frozen P13 and earlier full-suite
results above, and do not claim the complete ZIP malformed-input space.

Follow-up evidence:

- `out/build/backlog-2026-10-20/windows-package-p03-p05-compiler.log`
- `out/build/backlog-2026-10-20/windows-package-p03-p05-final.log`
- `out/build/backlog-2026-10-20/python-package-p03-p05-final.log`
- `out/build/backlog-2026-10-20/python-package-unsupported-features-final.log`

The separate final Linux follow-up overlays exactly the runtime template, two
tests and fourteen generator/fixture files, with every copied hash checked and
current source timestamps. Both `package_test.cpp` and `python_writer.cpp` are
rescanned and compiled; the package executable and actual compiler are relinked.
All **5/5 focused CTests pass in 0.65 s** under the Linux provider/tool profile
above. This Linux selection is `package_test`, `package_independent_interop`,
`opc_profile_test`, `python_package_test` and `serializer_opc_exchange`; the
Windows selection instead includes `document_profile_test` and runs the Python
suite separately. The retained Linux `LastTest.log` explicitly records all named
Python cases, **17 tests in 0.175 s**, and `OK`, using the rebuilt Linux compiler.
Evidence is `out/build/backlog-2026-10-20/linux-p03-focused.log`,
`linux-p03-LastTest.log` and `linux-p03-source-inventory.json`.
Its **2,153-file** source aggregate is
`e7edd2874f1c6d87708c9009a07925e98b46238df3c812ec11c6181e9e6106b5`.
Earlier full-suite and P13-only inventories remain unchanged; this is a focused
qualification of the later source delta.

The refreshed compiler has SHA-256
`975163b4cec10cab5276233ac80d242991a5d007c6ed96c3deca686de83cc1da`.
The final native test source is
`92b1301d50dd2462092eb9e3ad1a48d169ee3d61690322a1b4986df2e56cea2a`;
the Python test source is
`ba36547498908ed15ee0f6fe859c3234142d88ea6f786a5629376c06c5517fd4`.
The independent valid ZIP has SHA-256
`cb1a539e08af81df9d5e50b0da9b28c686c791e620157949c01b6d06b76683f2`.

To reproduce against an already configured matching native build, from its
compiler environment:

```text
python -B test/resources/package_malformed_fixtures.py
cmake --build out/build/opc-implementation --config Release --target serializer package_test --parallel 4
ctest --test-dir out/build/opc-implementation -C Release --output-on-failure -R "^(package_test|package_independent_interop|opc_profile_test|document_profile_test|serializer_opc_exchange)$"
python -B test/python_package_test.py --compiler out/build/opc-implementation/serializer.exe --build out/build/package-reference
```

Use the corresponding configured Linux build/compiler path on Linux. The
fixture generator intentionally rewrites the maintained fixture files; its
valid archive is independently checked before the mutations are emitted.

## Reference tasks verified in the selected profile

The full Windows wrapper passed **200/200 CTests** before these test-only additions.
The five focused native replays above repeat the relevant native and independent
interchange checks; the generated Python suite is also rerun. These results support
the following narrow task instances alongside P13:

| Task instances | Executed evidence |
| --- | --- |
| CP-P01 | `package_zip` source/producer boundaries, complete-part staging and cumulative limits. |
| CP-P02 | `package_independent_interop`: Python opens native stored ZIP and verifies exact payloads, timestamps and attributes. |
| CP-P03, PY-P03 | Shared independent directory-offset, extent, local-header, name/extra range and count mutations fail indexing. |
| CP-P04, PY-P04 | `serializer_opc_exchange`: both C++/Python producers are read by both consumers and independent `zipfile`/XML readers; the model entry is verified as DEFLATE. Python package tests independently inspect generated output too. |
| CP-P05, PY-P05 | Shared coherent ZIP fixtures reject explicit CRC/expanded-size mismatch and trailing/truncated DEFLATE before model/aggregate publication. |
| CP-P06, PY-P06 | Independent `zipfile` inspection accepts forced-small ZIP64 output and validates entry payloads. |
| CP-P07, PY-P07 | Native Python-produced ZIP64 fixtures and Python `zipfile` forced-ZIP64 inputs, including descriptor framing, decode successfully. |
| CP-P08, PY-P08 | Independent `zipfile` reads native/generated ZIP64 directories and locators; tiny forced-ZIP64 archives retain exact entries. |
| PY-P10 | Directly authored signed/unsigned 32/64-bit descriptors, including a CRC equal to the descriptor signature, pass the generated reader. |
| CP-P13, PY-P13 | The independent 41-decision content-type corpus above passes. |
| CP-P14, PY-P14 | Root/part-scoped relationships, relative targets, inert external targets, missing targets and escaping/scheme/authority rejection fixtures. |
| CP-P19 | Native input permutations produce identical canonical ZIP/OPC bytes; stored ZIP equals independently authored Python wire bytes. Metadata timestamps/attributes and OPC XML are independently inspected. |

The relevant full-run log is
`out/build/backlog-2026-10-20/windows-native-final.log`. The earlier refreshed
Python run is also retained in `windows-source-final-checks.log`. Provider-free
Windows tests separately qualify stored ZIP and explicit missing-provider behavior,
as recorded in the [release checkpoint](verification-backlog-2026-10-20.md).

## Remaining package checks

The later [lifecycle/descriptor checks](verification-package-lifecycle-2026-10-20.md)
close CP-P10 and PY-P01/P02/P11/P12/P19. The later
[typed/profile checks](verification-package-profiles-2026-10-20.md) close the
selected CP/PY-P15/P17/P18 instances. The remaining audited gaps are:

| Tasks | Remaining completion check |
| --- | --- |
| CP-P09, PY-P09 | Large-offset/count ZIP64 boundary fixtures beyond tiny forced headers. |
| CP-P11 | Mid-spool cancellation/private-file/cleanup qualification. |
| CP-P12 | Publication-transition/durability fault coverage beyond checked preparation/before-publication failures and successful replacement. |
| CP-P16, PY-P16 | Complete automatic codec/reference accounting with part/field/link exhaustion diagnostics. |
| CP-P20, PY-P20 | Complete generated fixed/default/runtime profile selection and before-decode type admission. |
| CP-P21, PY-P21 | Automatic generated payload key/reference checks; the C++ caller-authored reference index is separately checked. |

The source/sink and logical resource guarantees do not impose hard provider/RSS
bounds. Broader OPC edition/corpus, Unicode part naming, application semantics,
other backends and complete platform/release gates remain separate requirements.
Temporary generated Python modules are automatically removed; useful native build
output and final verification logs remain beneath ignored `out/build/`.
