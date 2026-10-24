# Current XSD export corpus — 24 October 2026

[Tooling inventory](../../qualification/xsd_tooling/README.md) · [Pending QA02](../pending.md#qualification-and-distribution)

Compiler/runtime **2.33.0**, schema language **1.23.0**, source baseline
`ca04f1af33af9927110eb5ca67bf9e7bf82c478c` plus this qualification-only fixture
update. Compiler/runtime sources, generated APIs and wire formats are unchanged.
This is a compiler-tooling slice of QA02; it does not close the broader corpus,
runtime, package or platform gates.

The current compiler already exports the fixture's `xs:int | xs:string` union.
The former corpus incorrectly expected rejection and stopped after the other
two modules' 90 decisions. A direct admission probe confirms the other seven
unsupported fixture families still reject; the bound prefix-list case now has
its current explicit external-capability diagnostic. No rejection is silently
ignored or converted into a passing runtime result.

The union now follows the complete positive-family workflow. Independent fixed
XML instances cover a whitespace-surrounded integer, preserved Unicode text,
empty string, missing value, excess value and wrong child namespace. Network-
disabled lxml validates each instance against both the original and exported
XSD. Rebind preserves the exact `ToolingEntry.value` field and durable ID `37`.
Fresh declaration import, native model generation, export/import idempotence,
reviewed bundles, protected publication and staging cleanup also execute.

All three mandatory modules pass **321/321 semantic decisions**, with a complete
inventory and no source/artifact drift. The report retains
`scope=compiler_tooling_only`, `runtime_followup_required=true` and
`runtime_backend_completion=false`; these dispositions are intentional.

Execution uses Windows 11 x64 build 26300, Python **3.14.3**, independent
lxml **6.0.2** / libxml2 **2.11.9**, xmlschema **4.2.0** and elementpath **5.1.4**.
The compiler was rebuilt with MSVC **19.51.36260.0**, C++23 and native LibXml2
**2.15.3**. Its SHA-256 is
`701f08e6b45120524c2aaa0fe767e6a56de30053a1993e74d365c59cadc8d08c`.

Run with the independent Python providers on `PYTHONPATH` and the compiler's
native provider DLL directory on `PATH`:

```text
python -B qualification/xsd_tooling.py --compiler out/qualification-2026-10-24/windows-msvc/serializer.exe --build out/qualification-2026-10-24/xsd-tooling-current
```

Choose a fresh output directory for another run. The successful report is
`out/qualification-2026-10-24/xsd-tooling-current/verification.json`, SHA-256
`1fb986c14aa5dd8bc9e5dbcd084baf7af37be961a4aad7bc0035a895da27f1c6`.
The initial inaccessible oracle deployment, stale union rejection and direct
admission probe remain separately retained under the same ignored task directory.
The pinned oracle deployment was copied into this checkout rather than changing
the shared retained cache or package versions. `.gitignore` already covers these
logs, dependencies and generated outputs.
