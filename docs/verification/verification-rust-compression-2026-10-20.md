# Rust compression — 2026-10-20

Compiler/runtime 2.14.0 qualifies **RS-Z01–RS-Z08** using the self-contained
Rust 2021 standard-library profile. Identity, gzip, zlib and raw DEFLATE are
implemented; this original default profile reports Zstandard/LZ4 unavailable
before callbacks or borrowed I/O. The subsequent
[provider proof](verification-rust-compression-providers-2026-10-20.md) adds
explicit caller-selected adapters and reruns the unchanged core suite.
No Cargo dependency, compression provider, native compression FFI or C++ runtime
is required by the generated helper.

## Actual execution

| Generation and execution | Tools | Result |
| --- | --- | --- |
| Windows x64 generation and SDK | Strict MSVC 19.51 compiler build; Rust 1.99.0 | 974 independent input decisions, 64 independent standard-frame output checks, three generated profiles and two exclusive-borrow compile-fail controls passed. |
| Windows generation, native Linux SDK | The same immutable Windows compiler; Rust 1.93.1 in private native WSL storage | The complete initial suite passed, without mounted-drive execution or downloaded crates. |
| Native Linux generation and SDK | Strict GCC 15.2 compiler rebuild; Rust 1.93.1 | The complete final suite passed. All three generated profile files were byte-for-byte identical to Windows output. |

Every SDK compile used `--edition=2021 -D warnings -O`; assertions remain active.
[`rust_compression_test.py`](../../test/rust_compression_test.py) invokes the actual
compiler and SDK, with maintained
[`compression_rust_main.rs`](../../test/resources/compression_rust_main.rs).
Native Linux compilation uses the root `make configure` workflow and the real
`serializer` CMake target. Linux's installed Rust SDK is sufficient; Cargo is
not installed or needed for this qualification.

The three generated profiles are ordinary full-protocol output, a collision
schema retaining `SerializerCompression` and `SerializerCompression2` while
selecting `serializer_compression_3`, and native `binary_none`-only output.
Disabled native protocols still reject independently of compression framing.

## Framing, bounds and lifecycle

The independent Python producers cover stored/fixed/dynamic DEFLATE, Huffman
alphabet/repeat/history boundaries, gzip optional fields/header CRC, zlib
advertised history/dictionaries, mandatory checksums, truncation at each selected
boundary, trailing/concatenated data and exact versus reduced byte limits.
Generated Rust's two-pass decoder validates grammar, extent, history and actual
expanded count before reserving expanded storage.

The 64 independently decoded outputs comprise:

- Four native protocols × four outer formats, preserving the original native
  bytes through both owning callbacks and borrowed `Input.decode`.
- Four formats × levels 0/1/6/9 on ordinary input.
- Four formats × levels 0/1/6/9 on overlapping repeats and 32 KiB history input.

Python's independent `zlib` decoder checks exact frame EOF and unchanged payloads.
Identity bytes compare directly. The fixed/LZ77 effort scale is intentionally
distinct from zlib's compression-level heuristics.

Additional actual controls cover provider/level/window admission before native,
cancellation and source/sink calls; zero input/output limits; 7-byte source reads;
11-byte sink writes; true source EOF and an overflow probe; zero-write progress;
oversized I/O reports; source/sink errors; unread/empty input finish; explicit
output finish; abandoned close; late native-candidate cancellation; cancellation
after staging; retained error identity and inactive first-cause behavior.

Safe direct callback reentry/close attempts fail the Rust borrow checker for
both wrappers. Expected source/sink panic tripwires are caught by the test caller;
subsequent wrapper operations remain inactive and perform no further transport
work. The helper does not catch application panics or override interior-mutability
containers' own borrow rules. Callbacks must respect the active exclusive borrow.

Constructors take their generic adapter by value. Tests pass mutable references
to retain transport ownership. Supplying an owned `File` intentionally moves it
into the wrapper; its eventual destruction may close that owned file. Wrapper
`close`/`flush` themselves touch only private state and never finish/flush the
borrowed transport. Wrappers expose compression-typed methods, without claiming
`std::io::Read`/`Write` trait implementations or implicit standard-trait nesting.

## Preservation and provenance

The original generated Rust runtime/model file is an exact prefix of final
output; the new module is appended. Its baseline SHA-256 is
`ccfa73c8a58bc4a453cb9e04fa83c5125760ae3cc65c5a5513d63da2b6db1eca`.
The complete normal profile is 69,547 bytes with SHA-256
`98dbc1634024a0795bbafd60d966f7d9d417d6c70dc333177ec459c7a2d2d461`.
Existing schemas, public native methods, field identities and wire values are
preserved.

| Final input/artifact | SHA-256 |
| --- | --- |
| Immutable Windows compiler | `6b15711d82acb2bac35c3a4e6e39efee091a679c7c6d77653ecd29447e69d052` |
| Native Linux rebuilt compiler | `3f26d3d8f496ea7f5765fcdd1bb906f880eaa460935229c1555fec21479a8178` |
| Rust emitter | `fcfc48250a068932d2fd4ece040a09f9b31be5aa19c735b0f57ce2ccdf023ea4` |
| Rust runtime template | `ea0b21dfb9617276b9fabfe0db305a244dcc63f7bee3800956a73f12dc06a1fa` |
| Maintained Python runner | `7d9951d901f7860ad4fb5353b76dad7f998ea0ed837d99da189b5011dfe8af66` |
| Final Rust fixture driver | `84a64a350a66b409471bfe0eb6aac0299e72d79b80545f4d432aa5aa8312a243` |

The final Linux source inventory contains 2,185 files with aggregate SHA-256
`8b8a61feeeded12dd1123de93d85f06b0bb0ba64bf77032fd594753cf13c3bd2`,
using sorted `path NUL sha256 newline` records. It captures the six-file Rust
emitter/runtime/runner/shared-fixture overlay on the previously qualified native
source snapshot. It does not qualify later unrelated C/JavaScript backend work
in the active Windows checkout.

Ignored evidence is retained under
`out/build/backlog-2026-10-20/rust-compression/`: final Windows and native Linux
JSON reports, actual generated sources/output frames, SDK logs and borrow-checker
diagnostics, native compiler build logs, baseline output, complete Linux source
inventories and hash-verified replay inputs. Runtime literal and Python AST/link
checks pass. Logical caps exclude allocator spare capacity, fixed workspace,
native callback allocations and process RSS. Broader cumulative contexts,
optional providers, standard-trait adapters and performance gates remain open;
this record does not close RS-Z09–RS-Z11.

## Optional-provider follow-up

Actual compiler SHA-256
`cdaa775d0b98f5b2b0b640f9add6ff81224723cc6a82f021a46be16a65d0b337`
passes the unchanged core runner on Windows Rust **1.99.0** and native Linux
Rust **1.93.1** after the additive provider APIs. Each passes **974/974** inputs,
**64/64** outputs, all **three** profiles and both expected callback-borrow
rejections, with no Cargo or optional dependencies. Both compare the exact
original **31,535-byte** native prefix against the earlier actual output.
The complete normal source is now **97,906 bytes**; original hashes and source
sizes above retain their historical scope. The separate
[provider record](verification-rust-compression-providers-2026-10-20.md)
qualifies selected **RS-Z09/RS-Z10** profiles on Windows and native Linux;
broader **RS-Z11** remains open.
