# State enhancement qualification — 2026-10-10

This record qualifies the uncommitted `state-framework-enhancements` branch over
`f9a0730fc845c87f7c361d5f64462761510cc27f`, compiler/runtime 1.11.0 and schema
language 1.6.0. [Examples and remaining work](state_enhancements.md) define the
support boundary; this is not a claim that every proposed combination is implemented.

| Check | Result |
|---|---|
| Windows CTest regression | All 97 checks passed across the full run and the four corrected-check retests |
| Core parser/generator regressions | All 435 tests passed in the final core check |
| Managed document models | Direct layout: 19 tests; separate values: 5 tests; public example passed |
| Managed runtime, journal and collaboration | Store/direct/collaboration suites passed; subprocess crash recovery passed |
| Installed package | Every installed public header compiled independently through exported targets; linked capability probe passed |
| Portable behavior | 11/11 real language SDKs passed |
| Native/external behavior | 11/11 SDKs passed, including required-definition rejection probes |
| Document collections | 11/11 backends passed with independent defaults, exact extents and protocol round trips |
| C++ source maps | All 9 formatting profiles passed with exact decoded schema paths and final physical line positions |
| Linux C++ | GCC compiler/runtime build and runtime-state example passed with strict warnings |
| Editors | 95 tests passed; VS Code and Visual Studio 1.1.32 packages built and validated |
| Skills | Canonical and consumer skill validation passed |

The full Windows run initially passed 93/97 checks. The four failures were resolved:
use declaration-before-use in a legacy grammar test fixture, run raw-MSVC negative
probes inside the Visual C++ developer environment, and make installed exported
headers precede older same-name dependency headers. The final retest passed 4/4.
The installed-package test uses only exported targets; it does not override include
paths to conceal package ordering. New runtime headers use coherent sibling includes.

C++ qualification uses exceptions and RTTI. Build dependencies came from vcpkg.
See [behavior qualification](verification-behavior-2026-10-10.md) for SDK commands,
strict compiler options, native attachment checks and source-map evidence.

## Reproduce the final Windows regression

Use a Visual C++ developer PowerShell, with `SERIALIZER_TEST_CLANG_FORMAT` pointing
to clang-format. Configure/build through the synchronized repository entry points,
then run:

```powershell
ctest --test-dir out/build/make-Release -C Release --output-on-failure --parallel 4
```

Ignored logs are `out/state-framework-full-test.log` and
`out/state-framework-final-retest.log`; editor and GCC evidence is also retained in
`out/`. Generated artifacts and SDK build products remain outside source directories.

## Source identity

SHA-256 of the 117 changed/new implementation, schema, test, example and
editor files, sorted by relative path, each followed by NUL, contents with CRLF
normalized to LF, then NUL:

`12aa2abec956d59f38d42a630e23c67d67373d5b9ba65e3bb1b937d2a7a5f387`

Documentation and skill files are excluded to avoid a self-referential fingerprint.
This fingerprint identifies the qualified working-tree implementation, not a release
commit. No consumer dependency gitlinks were changed.
