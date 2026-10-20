# Typed package and preservation policies, 20 October 2026

This record closes the selected **CP-P15/P17/P18** and **PY-P15/P17/P18**
profiles for compiler/runtime **2.14.0**, schema language **1.20.0**, on baseline
`bd2ee2ef9239b65335367bd8de19be377ce84262` plus uncommitted changes. The existing
implementations pass the additional checks; no production package codec or
serialized format changed.

Windows CPython **3.14.3** passes **28/28 generated package tests**, with no skips,
using actual compiler SHA-256
`1c7d8663872085f66e1c1d7c0c4bf35427015a57dc073001502b586fddc45c80`.
The command and final plain log are retained as
`out/build/backlog-2026-10-20/python-package-profiles.log`.

The subsequent Windows native rebuild passes **6/6 focused CTests in 1.37 s**,
including **44/44 native package cases** and **6/6 generated C++ profile cases**.
Its compiler SHA-256 is
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`;
the exact command log and native `LastTest.log` are retained with the
`windows-package-profiles` prefix. This extends the native profile checks below
to Windows without changing the recorded Linux run.

Linux passes **6/6 focused CTests in 1.00 s**, including **44/44 native package
cases**, **6/6 generated C++ profile cases**, **28/28 generated Python tests**,
independent ZIP/interchange checks and runtime-literal validation. This is the
existing GCC **15.2**, C++20 warnings-as-errors, CPython **3.14.4**, zlib **1.3.1**,
LibXml2 **2.15.2** full-provider profile. The unchanged C++ generator SHA-256 is
`258f9c4d011855d82bb90f26b234231cf1cc8a9b44d1f36458ddd832d0b434bf`.

The Linux-local source inventory is retained in
`out/build/backlog-2026-10-20/linux-package-profiles-source-inventory.json`,
with aggregate SHA-256
`bb52ce5358359a206cceb859a9a0613a6d4af8697145053fcab4fabb03bb2f9b`.
Changed inputs were streamed without using the Windows mount; actual C++ package
tests and schema-generated profile consumers were rebuilt. Logs, source delta
hashes and `LastTest.log` use the same `linux-package-profiles` prefix.

| Input | SHA-256 |
| --- | --- |
| `test/package_test.cpp` | `b54a5a8a6da8f28403aec4536aa318459cdf5d81017a07fc53f437b247103498` |
| `test/opc_profile_test.cpp` | `5194dbd0898978dde03aab67cf4b5f2ec9122f7a07d3576ad1b8d085fad1fd3d` |
| `test/python_package_test.py` | `1312e74ec23c9636f2c1a8a839cca0d770ec01c481eca7ed411e435c389044cb` |
| `test/resources/opc_profile.serializer` | `48f2c754d41ca1fe24a66412c6e81c85a112a6dcb8a87e4b99374d2e558fad2a` |

## Typed candidates

An additional schema package declares exactly three dynamic typed XML model
parts. Python independently authors ZIP/XML and C++ uses the plain OPC builder,
so the generated package compositor does not produce its own failing inputs.
Valid controls decode all three models. Invalid scalar values in the first,
middle or last part reject before the domain-validation callback or destination
assignment. Errors retain the exact source part and relationship ID; the prior
complete aggregate remains unchanged. C++ obtains its occurrence count from
generated profile metadata.

## Optional and required extensions

Each backend executes **21 policy decisions**: all nine part/relationship
reject/skip/preserve combinations, followed by required-part, required-link and
combined required-capability checks for each of the four successful optional
combinations. Reject policies fail; skip retains nothing for that category;
preserve retains the selected opaque bytes or scoped links. Required capability
callbacks override both skip and preserve. Existing edited-opaque-byte round
trips retain the original whole XML part and its namespace declarations.

## Revalidation after editing

Save revalidates retained relationships against the resulting package. Missing
targets and missing sources reject with `missing_part` before replacement, retain
the previous final bytes and remove staging. Python additionally removes a
previously retained opaque target while keeping its link, and verifies a valid
edit preserves the original opaque bytes through independent `zipfile` reading.
The C++ missing-source fixture points at an existing target, so it independently
qualifies source validation.

These checks cover the supported whole-part/link preservation contract, not
arbitrary XML fragment normalization, automatic retargeting of moved typed roles,
complete OPC/3MF conformance or all platforms. Automatic payload-key/reference
generation and full budget/field diagnostics remain P21 and P16 respectively.
Temporary generated modules and owned staging are removed. Useful native build
outputs and final evidence remain under ignored `out/build/`.
