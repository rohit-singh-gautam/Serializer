# Managed language qualification — 2026-10-24

Compiler/runtime **2.34.0**, schema language **1.24.0**, Windows x64 Release.
The native compiler/core build and generated SDK checks ran sequentially with
one build worker. The C++ filesystem tests used ordinary Windows filesystem
semantics outside sandbox path virtualization.

## Executed results

| Check | Result |
| --- | --- |
| `core_serializer_test` | **639/639 completed**, zero failures, skips, disabled cases or errors; configured clang-format gate enabled; 12.04 seconds in CTest |
| `python_managed_engine_runtime_test` | **22/22 passed**, including real writer-process termination, complete-generation recovery flushing, immutable pins, identity/history/authority contracts and post-flush interruption |
| `python_managed_engine_generated_test` | **133 checks passed** against actual generated SDKs in four identity profiles |
| `managed_token_codecs_test` | **5,310 checks passed**, including all four C++ identity exchange profiles |
| `managed_token_java_test` | **4,953 checks passed** against actual generated Java SDKs; four C++ exchange profiles and 64 byte-identical corpus round trips; 19.47 seconds |

All five CTest targets passed. The four language SDK targets took 28.57 seconds
in the final serial CTest run. Java profile counts, in test order, were 1,244,
1,223, 1,270 and 1,216.

The four identity profiles are `uint32`/`uint64`, each with `fixed` and
`compact_varint` encoding. Token tests exchange positional binary, integer-key
binary, string-key binary and JSON. Generated native Python engine qualification
uses its default canonical integer-key binary codec; other engine protocol
selection profiles are not qualified by this run.

Tools: MSVC **19.51.36260.0** in the CMake compiler metadata, C++23; CMake
**4.4.2**; Python **3.14.3**; Microsoft OpenJDK/Javac **17.0.18+8-LTS**.
Actual Java SDK compilation uses `--release 17 -Xlint:all -Werror`.

## Qualified behavior and limits

The [Python native engine](../managed/python_native_runtime.md) uses generated
record metadata for ownership, immutable reads, isolated callback edits, monotonic
logical IDs, bounded linear history, checked typed references, journal recovery
and exact authority/client retry outcomes. Native SDK checks cover ordinary
ancestor boundaries, managed grandchildren, new references, deleted/redone states,
escaped draft isolation, reopen and durable accepted/conflict receipts. Scalars
include double defaults, float, bool, negative int64, enum and `digest[32]`.
Invalid scalar updates preserve the accepted snapshot.

Recovery validates complete schema/identity/history/receipt semantics before tail
repair and flushes/fsyncs even a complete recovered generation before exposing
accepted outcomes. Fixed frame headers and payloads have independent checksums;
file and record-count admission bounds precede writes. Uncertain writes and
post-flush publication interruption fence mutation until reopen.

[Python and Java token-only modes](../managed/token_codecs.md) qualify strict
four-field native token envelopes, unsigned boundaries, exact class tags,
malformed/truncated input, optional/array/map wrappers, configuration precedence
and destination preservation on rejected generation. Tokens supply no liveness
or native engine by themselves. The Java array-preflight integer classification
bug found during qualification is fixed; actual SDK compilation and exchange now
pass. Python engine encoding validates and normalizes declared shapes before
calling ordinary native codecs, including integer defaults for floating fields.

These results do not close the ten-output `MG01` requirement or broader `MR03`
selection support. The documented Python native profile rejects unsupported
metadata/graph mechanisms explicitly. Native engine compaction/rebaseline,
native-format migration, power-failure directory persistence and non-Windows
qualification remain outside this evidence. Other non-C++ native engines remain
required. This record also does not qualify the independent C++ compact-history
helper or integration; their executed checks and limitations have a
[separate record](verification-compact-history-2026-10-24.md).

## Subsequent root-identity regression checks

After the initial checkpoint above, three added runtime cases reproduced
replacement of the document root through authority input, semantic journal
recovery and a client receipt. The fix compares the accepted root identity before
reserving IDs or clearing pending client state. A complete checksummed invalid
generation rejects without repairing its trailing bytes.

The final serial rerun passes **25/25 runtime tests** and **141 actual generated
SDK checks** across all four identity profiles, in **5.15 seconds** for the two
CTest targets. Logs are retained at
`out/build/managed-xml-integration/python-root-regressions-passing.log` and
`out/build/managed-xml-integration/python-root-generated-passing.log`. Earlier
failing logs remain available. These results precede merge `dacd81e`; they do
not by themselves qualify the combined compiler/backend worktree.

## Combined compiler/backend replay

After merge `dacd81e`, the single-worker Windows build compiles the combined
parser, native bundle support, Java streaming/packed emitter and both managed
engines. The full core suite passes **647/647 cases** with zero failures,
errors, disabled cases or skips, in **5.998 seconds**; the configured formatter
gate executes. The initial run has one failing Java negative test because two
recursive owning schemas now reject in the incoming parser before reaching the
writer. The corrected expectation checks that specific parser exception; the
original failure log and JSON remain available.

All **nine serial integration targets pass in 181.25 seconds**:

- Python runtime **25 tests**, generated native engine **141 checks**, and
  token exchange **5,310 checks** with four independent C++ identity profiles.
- Java runtime **188 checks**, six actual separate-JVM crash/reopen boundaries,
  and separate-JVM writer exclusion through ordinary and hard-link paths.
- Java generated native engine **528 checks**, 66 each under eight profiles:
  uint32/uint64, fixed/compact IDs, and positional/integer-key canonical binary.
  This uses actual generated Java source and codecs, including descending-map
  normalization and unsigned identity boundaries.
- Java token exchange **4,953 checks** with four independent C++ profiles;
  caller-owned array/NIO buffers **454,826 assertions**; stream/channel and
  packed-buffer controls; and native Python dependency-file output.

Evidence: `out/build/java-managed-engine-merged-core.json`, matching core logs,
and `out/build/java-managed-engine-merged-sdk-first-run.log`. The new
[Java engine guide](../managed/java_native_runtime.md) records the qualified
profile and its limitations. The generated Java profile does not provide default
initialization of required zero reference envelopes, inheritance, generic
definitions, indexes, caches, XML or package graphs. Eight further non-C++ native
engines remain required by MG01. The separate included-file Python managed
combination receives its own regression check after this checkpoint.

The final included-file correction passes **281 Python generated SDK checks**,
covering each identity profile in inline and included separate layouts. Core
647/647, Python token exchange and ordinary native-file output are rerun after
that one-line source-map propagation fix. See the
[dedicated Python record](verification-python-managed-runtime-2026-10-24.md)
and `python-managed-separate-core.json`, `python-managed-separate-controls.log`
and `python-managed-separate-sdk-first-run.log` under ignored `out/build`.

## Reproduction and retained evidence

From the configured Visual Studio Developer PowerShell and dependency setup in
the root README:

```powershell
./make.ps1 configure -BuildDirectory out/build/ReleaseWindows -Jobs 1
cmake --build out/build/ReleaseWindows --config Release --parallel 1 --target serializer core_serializer_test
ctest --test-dir out/build/ReleaseWindows --build-config Release --parallel 1 --output-on-failure -R '^(core_serializer_test|python_managed_engine_runtime_test|python_managed_engine_generated_test|managed_token_codecs_test|managed_token_java_test)$'
```

The four C++ token exchange executables must already be built. Set
`SERIALIZER_TEST_CLANG_FORMAT` to the configured
`SERIALIZER_CLANG_FORMAT_EXECUTABLE` to include the formatter gate. Set
`GTEST_OUTPUT=json:<absolute-output-path>` for machine-readable core evidence.

Useful local evidence is intentionally retained under ignored `out/build/`:
`managed-language-core.json`, `managed-language-core.log`,
`managed-language-sdk.log`, `managed-language-versions.log`,
`managed-java-python-build.log` and `managed-java-python-build-rerun.log`.
The `-first-run` logs/JSON preserve the initial failures separately from the final
passing evidence. The ignored `managed-language-checks.ps1` records the exact
local test runner. Generated SDKs, class files, wire corpus and small unique
native-journal fixtures remain under `out/build/ReleaseWindows/test/` for review.
