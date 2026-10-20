# Compiler, package and managed-reference batch — 20 October 2026

[Pending requirements](../pending.md) · [QName scope work](verification-qname-scopes-2026-10-20.md)

Compiler/runtime **2.16.0**, schema language **1.20.0**. This batch groups
independent work for one integrated regression. Broader parent requirements stay
open wherever the recorded implementation does not meet their entire contract.

## Managed-reference failure and lifecycle qualification

Five additional tests execute in each of the eight existing C++ profiles:
direct/separated storage, fixed/compact identities, and uint32/uint64 IDs.
All eight binaries passed **23 GTests each, 184 total**, including **40 new
profile-test executions**. No managed runtime, schema, public API or storage
format changes were needed.

The added matrix covers:

- Six external mapping failures: omitted, empty, foreign, dangling, wrong exact
  target type and mapper exceptions.
- Five attachment ownership failures: dropped, duplicated or substituted owned
  roots, and exceptions before/after insertion.
- Zero/duplicate saved source IDs and a candidate allocation failure with exact
  resource balance and no consumed identity.
- Snapshot admission failure after identity reservation, atomic rollback,
  permanently burned IDs and successful Save/load recovery.
- Cyclic deletion with complete-candidate validation, aggregate-edge cleanup,
  pinned historical registries, Undo/Redo, persisted history and no ID reuse.

Assertions compare the original published state/history before and after failure,
then exercise recovery. These checks qualify selected `MR01`/`MR02` slices;
broader preview, collaboration, journal and acceptance fault matrices remain open.
Logs: `mr-direct32-build.log`, `mr-profile-build.log`, `mr-profile-test.log` under
ignored `out/build/pending-qname-2026-10-20/`.

## Compiler default validation

Explicit language **1.14.0+** canonical unsuffixed decimal floating defaults now
diagnose overflow at the declared `float`/`double` width. Defaults explicitly
qualifying the declared enum type reject unknown members. Errors retain the
original member path/line/column through includes and generic substitution.
See [owning defaults](../schema/owning_api.md).

The actual compiler passed six atomic multi-output rejection cases, nine
native/legacy controls, and generated Python default round trips across all four
native protocols. Numeric Boolean defaults, opaque native expressions,
suffixed/octal/hexadecimal floating spellings, underflow, unrelated enum
qualifiers and legacy headers retain their previous handling. This adds no
expression evaluator, language syntax, wire values or bit-field contract.
The rebuilt native parser/include/default tests and integrated regression are
recorded below when complete.

## Package accounting and diagnostic scope

Generated C++/Python package codecs automatically charge OPC relationships and
supported XSD key/unique/keyref tuples against one cumulative reference ledger.
Complete tuples count before retention, including repeated equal keyref requests;
incomplete optional tuples do not count. Three-part identity fixtures qualify
exact admission, middle-part rollback and shared load/Save accounting. Declarative
application-specific reference generation remains a separate `P21` requirement.

Linked static member/constraint names retain locations at instrumented native
validation and reference admission without constructing heap paths on ordinary
XML calls. Namespace snapshot admission forwards the active Python field context.
Reference-index failures retain the field/key and optional relationship ID.
Original overloads, aggregate initializer order, legacy callback resource events
and numeric requested/remaining-capacity semantics remain supported. Custom caller
exception subtypes survive; Python also preserves the exact exception instance.

The refreshed native core/package/OPC/operation targets passed **4/4 CTest cases**.
The current compiler passed **37/37 Python package**, **6/6 identity** and
**17/17 rich XML** tests. The raw runtime-literal size guard also passed after
splitting adjacent C++ literals without changing their concatenated generated text.

The parent `CP-P16`/`PY-P16` gates remain open for deferred presentation and
provider work. Prepared start-tag/attribute escaping and retained-tree emission
can fail after their native field scope ends; part/link remain available, but
per-field attribution is absent. Provider-local admission can also report a
codec error before a shared resource callback runs. Explicit boundary tests record
these limitations instead of claiming complete diagnostics.

The C# compression harness disables persistent build servers/node reuse and uses
a stable SDK working directory with disposable projects in the OS temporary
area. Workspace project indexing can otherwise retain directory handles after
all assertions pass. Three consecutive replays passed **974 input decisions**,
**16 independent output checks**, all five builds and project cleanup each time.
Cleanup failures remain errors and no correctness assertion was removed.

Go publication tests now distinguish a genuine injected panic from a Windows
rename access/sharing failure before reaching the hook. A bounded retry admits
only that pre-hook OS failure and first verifies unchanged complete old bytes and
no staging leak. Every selected fault/panic boundary still must execute and retain
its original error identity. The complete package runner passed, followed by
**500 complete publication-matrix replays, 5,500 subtests**. The stress run
observed the OS denial and exercised the bounded recovery path.

## Integrated verification

The combined Windows wrapper built all native targets and ran **214 CTest
cases**: **211 passed** in that complete run. The three remaining cases were the
Go pre-hook Windows rename failure, C# project-directory cleanup, and one managed
collaboration Save reporting an indeterminate filesystem outcome. After the
harness repairs, the complete Go runner/stress matrix above passed, C# passed
three times, and the unchanged managed collaboration binary passed three times.
All 214 cases are therefore accounted for, including these explicit replays;
this is not a claim of one failure-free full invocation.

Logs include `batch-integrated-final.log`, `batch-repair-focused.log`,
`go-save-harness-final.log`, `go-save-repaired-stress.log`,
`windows-harness-replay.log` and `csharp-os-temp-replay.log` beneath ignored
`out/build/pending-qname-2026-10-20/`. Compiler/package version compatibility
cases passed in the complete run. No Linux/provider-free/editor-host/publication
result is implied. The newly selected
[50-item batch](verification-pending-50-2026-10-20.md) requires its own integration
and verification. Useful generated outputs, caches and logs are retained; final
cleanup is recorded when that active work completes. No Git commit was created.
