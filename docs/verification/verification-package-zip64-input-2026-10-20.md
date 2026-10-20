# ZIP64 input boundaries — 2026-10-20

Compiler/runtime 2.14.0 qualifies **CP-P09 and PY-P09** on the selected Windows
and Linux profiles. The test uses actual native ZIP readers/writers and actual
emitted Python support, with independently authored wire records and Python's
standard-library `zipfile` producer/consumer.

## Executed profiles

| Profile | Tools | Final result |
| --- | --- | --- |
| Windows x64 | MSVC 19.51.36260.0, CMake 4.4.2/Ninja, Python 3.14.3 | 56 cases / 100 backend decisions passed in 18.245 s; generated package suite 30/30 passed in 0.425 s; four related native/independent/literal CTests passed in 1.84 s. |
| Linux x86-64 | GCC 15.2.0, CMake 4.2.3/Ninja, Python 3.14.4 | 56 cases / 100 backend decisions passed in 15.828 s; eight focused CTests passed in 17.14 s, including all 30 generated package tests. |

The Linux compiler was rebuilt from the recorded Linux-local source snapshot.
Its complete 2,181-file inventory has aggregate SHA-256
`af830c60236051817d4b2fe4bf3c54c1859acd74bdac656d624bf8128f792bce`
using sorted `path NUL sha256 newline` records. This focused snapshot includes the
ZIP64 changes and prior native/package qualifications; it does not qualify later
unrelated backend work in the active Windows checkout.

| Final artifact | SHA-256 |
| --- | --- |
| Windows immutable compiler | `7734e373119a20f60b39bd85aa75907346bdce19690e9ba517813c16d321b819` |
| Linux rebuilt compiler | `0eacafffb0f13aa236edd10df2137a06b8feda7abebfdbb2dd73392b819082fb` |
| Windows emitted Python fixture | `7416cbb380cabb2e52c445166952f8e04348595f1a4fef3560fa86e9201138da` |
| Linux emitted Python fixture | `2c88fbe8a376affdceed226eced8570c0be4b0dceb2878db7299c67de0e66f2f` |
| Native package implementation | `14baf08c1bb49cf5172873d0375d67284d0588edd80a30e4b6be2d6d495added` |
| Python package runtime template | `2c2c95d3d62d6ec6440847e55ae0e5fdf16eaaded8892899243232ad21963e2a` |

## Independent boundary corpus

[`package_zip64_probe.cpp`](../../test/package_zip64_probe.cpp) provides a borrowed
`std::istream` whose stream buffer synthesizes bounded reads from small stored
segments. [`package_zip64_test.py`](../../test/package_zip64_test.py) independently
authors ZIP local/central headers, ZIP64 offset extras, locator/end records and
classic end records. Zero-filled logical gaps require no corresponding file or
memory allocation. The largest logical archive exceeds 4 GiB; each offset
fixture stores at most 224 bytes and the real readers request about 66 KiB.

The executed corpus covers:

- Local-header offsets at `UINT32_MAX - 64`, `- 1`, the maximum, `+ 1`, and `+ 4096`;
  ZIP64 directory offsets immediately below, at and above the maximum; and
  classic directory offsets immediately below and exactly at the maximum.
- Optional small ZIP64 end records whose ordinary narrow fields agree; mixed
  narrow/wide disagreements; unsupported extraction versions; split counts;
  missing/truncated locators, records and offset extras; out-of-range 64-bit
  offsets/sizes; and local variable fields overlapping the directory.
- Real independent archives with 65,534, 65,535 and 65,536 empty entries, about
  6.14 MB each. The native writer also produces a real 65,536-entry archive for
  independent `zipfile` validation and both native/Python consumers. Sixteen
  independent positive producer/consumer controls run in total.
- Exact archive, cumulative I/O, metadata, entry-count and buffer limits, and
  each limit reduced by one. Backend-specific metadata/container scratch
  accounting is measured separately. Rejected archive limits cause zero reads.
- Seventeen-byte short reads, a nonzero borrowed stream base, exact logical
  offsets, unchanged central-directory order, caller ownership and successful
  EOF positioning. The public generated OPC suite additionally refuses invalid
  source ranges before reads and rejects invalid post-probe positioning before
  a nonseekable fallback can read anything.
- Early, middle and late isolated duplicate names in real 65,536-entry archives;
  both backends retain the `duplicate_name` category.

All virtual input reads have independent harness caps. Resource limits remain
logical operation limits, not a process RSS guarantee. No physical 4 GiB fixture
is created and no 4 GiB payload is materialized or decompressed.

## Repairs and measured indexing behavior

The baseline rejected a valid classic ZIP with exactly 65,535 entries, and a
classic directory at exactly `UINT32_MAX`. Those values still fit their ordinary
unsigned fields. Locator detection now selects ZIP64 records when present;
otherwise exact central-directory extents validate the full classic field range.
This follows [PKWARE APPNOTE 6.3.10, §§4.4.1.4 and 4.4.21–24](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).
The independent [CPython 3.14.3 writer](https://github.com/python/cpython/blob/v3.14.3/Lib/zipfile/__init__.py)
also switches entry-count framing above 65,535, rather than at that count.

Python now checks optional ZIP64 locators even without narrow sentinels, validates
ordinary/extended field agreement and extraction version, and rejects invalid
directory/local extents before corresponding reads. Checked seekable sources
are borrowed from their current position, avoiding whole-input spooling for
large logical offsets. Genuine nonseekable sources retain private bounded
spooling. Short reads, cancellation and source ownership remain supported.

The native reader reuses its already admitted extent vector as a sorted name
index after releasing the directory byte buffer, then clears and reuses the same
capacity for extent validation. It adds no retained name strings or scratch
reservation and preserves central-directory order, exact metadata limits and
isolated duplicate diagnostics. When an archive contains multiple independent
errors, which error is reported first may change.

| 65K-entry reader measurement | Baseline | Final |
| --- | --- | --- |
| Windows | 10.49–11.14 s | 0.055–0.062 s |
| Linux | 11.60–13.03 s | 0.041–0.046 s |

The writer's duplicate scan is unchanged. Final 65,536-entry writer measurements
were 6.326 s on Windows and 6.734 s on Linux; timings are observations from these
runs, not performance guarantees.

## Retained evidence

Ignored evidence remains under `out/build/backlog-2026-10-20/package-zip64/`:
immutable compiler/probe/DLL manifests, baseline failures, final generated
modules and wire segments, Windows/Linux JSON decisions and timings, compiler
build logs, focused CTest logs, complete Linux inventories, and hash-verified
overlay/replay inputs. `windows-verified/verification.json` and
`linux-verified/linux-verification.json` contain the final decisions;
`python-verified.log` and `linux-verified/linux-LastTest.log` record the 30-test
facade regression suites. The CMake `package_zip64_test` registration repeats
the native corpus and includes generated Python when XML support is enabled.

This record closes the selected ZIP64 input rows. It does not establish new ZIP
compression methods, arbitrary multi-disk support, physical multi-gigabyte
payload throughput, or additional language backends.
