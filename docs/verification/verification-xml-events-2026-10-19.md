# Normalized XML event checks, 2026-10-19

Working source: compiler/runtime **2.9.0**, schema language **1.20.0**, based on
`1c73fb00763355df5e18281633b7de643c6936f6` with the accompanying uncommitted S01
changes. This record qualifies the provider-independent C++
[event contract](../serialization/xml_events.md). It does not qualify an XML
byte tokenizer or generated direct token codecs. S02 and per-language work
remain in [the backlog](../pending.md#shared-compiler-and-contract-tasks).

## Executed checks

| Check | Result |
| --- | --- |
| Windows focused event target | Refreshed final callback-failure fixtures pass 15/15 cases with XML enabled. Logs: `out/build/opc-implementation/s01-event-refresh.log` and `s01-event-cases.log`. |
| Windows full native | Root `make.ps1 test` exits 0: 172/172 CTests pass in 243.56 seconds, including the event target, compiler/package-version checks and installed-package consumer. `serializer_installed_package` passes in 99.14 seconds. Log: `out/build/opc-implementation/s01-full.log`. |
| Windows providers disabled | Fresh XML/zlib/Zstandard/LZ4-OFF build compiles the final callback-failure fixtures; event target passes 15/15 cases and event/package-version CTests pass 2/2 in 0.71 seconds. Both XML-enabled and provider-OFF compilers report 2.9.0 / language 1.20.0. Logs: `out/build/s01-minimal/event-cases.log` and `test.log`. |
| CMake package-version compatibility | The generated version-file test accepts exact 2.9.0 and earlier 2.8.0/2.8.1 requests; rejects newer patch/minor and different-major requests. This checks version-file policy, rather than installed historical ABI compatibility. |
| Linux focused event target | Fresh isolated snapshot builds the runtime/event target and passes 2/2 event/package-version CTests in 0.02 seconds, including all 15 event cases. The strict GCC C++20 documentation example compiles, links and runs. Logs: `out/build/s01-linux/`. |
| Documentation example | `validate_example()` extracted directly from the event contract compiles and links with `/std:c++20 /W4 /WX /MD` against the all-provider-OFF runtime, then runs successfully. Log: `out/build/s01-minimal/doc-example.log`. |
| Documentation links | Added Markdown links and both complete new guides pass local checks: 21 destinations and 3 fragment anchors, with no issues. `git diff --check` passes. Log: `out/build/s01-minimal/documentation-links.log`. Unrelated historical backlog links are outside this audit. |
| Editor impact | Review finds no changed schema syntax, generated layout, supported language or navigation behavior. Both extension versions remain 2.0.5; no packages are changed or republished. |

The event fixtures cover golden nested/empty traces, expanded attribute identity,
reuse of borrowed producer buffers, malformed shapes and document boundaries,
duplicate attributes/prefixes, explicit completion and rejection of trailing
events. Exact resource boundaries include total events, per-event payload,
cumulative document counts/text/names/declarations, depth, open-name storage and
duplicate-check scratch. Shared-context checks cover work/allocation admissions,
failure without refunded charges, callback exceptions, cancellation and deadline
failure during events and finalization.

The Windows full build uses MSVC 19.51, C++20, Ninja and `/W4 /WX`, with XML,
managed runtime, zlib, Zstandard and LZ4 enabled. Its command is:

```powershell
./make.ps1 test -BuildDirectory out/build/opc-implementation -Jobs 3
```

Final Windows logs are retained under the ignored
`out/build/opc-implementation/s01-full.log` and `out/build/s01-minimal/`.
Linux uses the fresh isolated snapshot
`/var/tmp/serializer-s01-20261019-ucmrrX` under Ubuntu 26.04.1 LTS x86_64
through WSL2, GCC 15.2.0, CMake 4.2.3 and
Ninja 1.13.2, with libxml2 2.15.2 and zlib 1.3.1 enabled. Zstandard, LZ4 and
the managed runtime are disabled. Tests/install support are enabled in Ninja
Release. The root Makefile configures the build using an existing isolated
GoogleTest prefix; the focused commands are:

```sh
cmake --build out/build/native --target xml_event_test --parallel 4
ctest --test-dir out/build/native -R '^(xml_event_test|serializer_package_version)$' --output-on-failure
```

This builds the runtime and event target; the compiler/CLI target and full Linux
suite are not run. The documentation example uses
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror` and links the runtime archive,
libxml2 and zlib. Exact configuration/commands, qualification metadata and matching
final source hashes remain under `out/build/s01-linux/`. The useful isolated
source snapshot/native build and final logs are retained. Setup scripts and
temporary documentation probe source/executable were removed after path checks.
No provider installation or network access was used. Final focused Windows
checks include the refreshed callback-failure fixtures with XML enabled and
with all optional providers disabled.

Windows documentation probe source, object and executable were removed; its
final log remains. The minimal configure log is retained under
`out/build/s01-minimal/`. Useful Windows build outputs and verification logs are
retained; no documentation-only scratch scripts were created. The existing
`.gitignore` covers these `out/` builds, logs and caches, so no new ignore rule is
needed for S01.

## Scope and limits

The validator owns only open expanded names and short-lived duplicate-check
indexes. Its logical state bound excludes allocator bookkeeping, unused
container/string capacity and byte-provider buffers. It is not a hard process
heap or CPU quota. Metadata counts are checked before scanning their payload
strings; duplicate detection uses sorted indexes rather than pairwise scans.
The work ledger charges descriptors and payload bytes, not every sort comparison.

S01 validates normalized events, not XML bytes. UTF-8 and XML name/character
grammar, namespace scope/resolution, split-byte/token behavior, DTD/entity
rejection and provider EOF checks remain S02/provider responsibilities. A byte
provider must check trailing input before emitting `end_document`; a consumer
must call `finish()` before publishing a candidate. No generated mixed-content
mapping or backend streaming-parity claim follows from the event representation.

No throughput or process peak-memory measurement, independent XML corpus run,
new generated-language token consumer or editor-host/package check is claimed
for this slice. Existing whole-object/generated XML behavior, native wire
formats, schema language and editor versions are unchanged.
