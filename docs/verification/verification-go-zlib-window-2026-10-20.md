# Go zlib advertised-window repair — 2026-10-20

Compiler/runtime **2.14.0**, schema language **1.20.0**, now rejects a DEFLATE
back-reference beyond a valid zlib header's advertised window. This is a repair
to the existing generated Go compression helpers; public APIs and output wires
are unchanged.

## Independent reproduction and repair

The actual pre-repair compiler
`309eb574e5cc10ff6a0d2b65e6f069da5b2add267fb831a657ce60139bc4e14f`
accepted **15 invalid frames**: fixed and dynamic DEFLATE at one byte beyond
each advertised window from 256 through 16,384 bytes, plus a stored-to-fixed
257-byte reference under a 256-byte window. The same maintained test also
demonstrated missing cancellation during late traversal: stop 5 returned success
after four polls. Stop 2 passed; stop 20 was not reached in that failed baseline.
The retained actual before-control is
`out/build/backlog-2026-10-20/go-window-before.log`.

For a valid dictionary-free RFC 1950 header advertising less than 32 KiB, a
fixed-scratch grammar/count pass now checks stored, fixed and dynamic blocks,
canonical Huffman codes, history distances, output bounds and the exact compressed
extent before the standard decoder expands bytes. It polls cancellation during
bounded traversal and preserves the callback's original error. The standard
provider still checks Adler-32. Gzip, raw DEFLATE and ordinary 32 KiB zlib streams
retain their existing decode paths. The selected Go provider still requires a
32 KiB history budget even for a smaller advertised logical window.

The shared producer [compression_window_fixtures.py](../../test/compression_window_fixtures.py)
authors **44 distinct frames and 64 policy rows**. It includes exact and
one-beyond history distances, fixed/dynamic/stored transitions, unused-distance
trees, native-encoder dynamic streams using code-length repeat symbols 16, 17
and 18, and independently rejected malformed controls. The Go runner consumes
the 44 unique frames; its existing exact and one-tight checks cover the duplicate
resource-policy rows.

## Actual generated SDK result

The maintained command was:

    python -B test/go_compression_test.py --compiler <actual-compiler> --build <fresh-output>

| Selected profile | Executed result |
| --- | --- |
| Windows amd64, Go 1.26.4, module language `go 1.22` | **126 input decisions**, **16 independently decoded output decisions**, and all **nine SDK test groups** passed. Ordinary, collision-selected, binary-only and XML-independent generated modules compiled and ran. The ordinary test package completed in 6.634 seconds. |
| Native Linux amd64, Go 1.26.0, module language `go 1.22` | The same **126 input decisions**, **16 output decisions**, **nine groups** and three additional generated profiles passed. The ordinary test package completed in 0.435 seconds. |

The actual tested compiler SHA-256 is
`9938073386c2a00641d69a3410690322f5b5fd396f9fc9371f185e1f10b65206`.
Its 2,349-file capture has source aggregate
`303fc92b9f2932d0b4c5d4572ab27b5be81b304a4c8735d48a0979b994e0c223`.
The four generated modules were 152,492, 157,695, 151,658 and 268,134 bytes.
These are observed source sizes, not memory or performance guarantees.

The native Linux compiler was rebuilt through the actual strict GCC/CMake
pipeline, then copied before this private SDK replay. Its SHA-256 is
`3b39d98189350b0feebdf2ce59cae51b1aa05a8c6c037d7ff86ce12eb2a1ee78`;
the native source aggregate is
`363b689147918fb584c724987a3686301d29a23e695e8256a37dfa764effc5bf`.
The replay verified all three source hashes below and the compiler hash before
and after execution. The four observed generated-source sizes match Windows.
It used private Linux-local scratch, the copied compiler and offline Go, without
configuring or mutating the active full-regression cache.

Retained evidence under `out/build/backlog-2026-10-20/` is
`go-window-windows-final.log`, `go-window-windows-final/verification.json` and
`go-window-final-audit.json`. The audit verifies that the actual compiler capture
contains the executed sources:

| Source | SHA-256 |
| --- | --- |
| `src/go_compression_runtime.inc` | `543333bd6d12ab9ee9e885e1cb86e7ed4f8d070b60971e37619899669c44bddd` |
| `test/go_compression_test.py` | `554e766c5b1c62278234edafeebdb27a0ef1af82eea8c61d5cc278b181c72bb4` |
| `test/compression_window_fixtures.py` | `0363f3b735a4525d8ed2cb9d35f871d5bd93d2186bf10483fd50b1f568fd3b34` |

Linux evidence is retained beside these files under `go-window-linux-final/`:
`execution.json`, `native-capture.json`, `runner.log` and `sdk/verification.json`.
The final audit links their hashes to the matching Windows decisions and source
hashes. Package test durations exclude SDK cold-cache compilation and are not
comparable performance measurements.

This selected replay does not establish an actual Go 1.22 SDK run, hard process
memory bounds, general throughput, or every caller-supplied optional provider.
The earlier [core](verification-go-compression-2026-10-20.md) and
[optional-provider](verification-go-compression-providers-2026-10-20.md) records
retain their distinct executed scopes.
