# Rust optional compression providers, 20 October 2026

[Rust compression API](../languages/rust_compression.md) ·
[Core proof](verification-rust-compression-2026-10-20.md) ·
[Adapter crate](../../qualification/rust_compression_providers/README.md)

This record qualifies **RS-Z09/RS-Z10** for the selected explicit-provider
profiles in compiler/runtime **2.14.0**, schema language **1.20.0**, on baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes. Actual
Windows-generated Rust is compiled and executed on Windows and native Linux.
The selected LZ4 encoder exposes fast level 0; broader **RS-Z11** contexts,
other providers/platforms, standard-trait nesting and performance gates remain
separate.

## Explicit capability and compatibility

The generated helper remains a standalone standard-library module. Its original
functions and constructors keep Zstandard/LZ4 unavailable even when an adapter
crate is linked. Existing exhaustive `EncodeOptions` and `DecodeOptions` literals
retain exactly their original fields. New `*_with_provider` functions and
`Output::with_provider`/`Input::with_provider` explicitly borrow a `Provider`.
No global registry or fallback is installed.

The object-safe trait admits format/level/window policies, then creates private
`Encoder`/`Decoder` contexts. Contexts receive bounded slices and return
`Progress { bytes_read, bytes_written, finished }`; generated code validates
counts, progress and exact completion before using returned extents. Application
cancellation/transport callbacks remain outside the provider. Active operations
preserve their original shared `Arc` error; inactive wrappers retain that same
handle as their cause. Provider and transport lifetimes remain checked by Rust.

The maintained adapter crate uses hygienic `zstd_provider!`/`lz4_provider!`
macros with a caller-selected generated helper path, including collision suffixes.
Its default Cargo features are empty. Explicit features select these pins:

| Adapter | Actual dependencies | Encoding profile |
| --- | --- | --- |
| Zstandard | `zstd` **0.14.0**, `zstd-safe` **8.2.0**, `zstd-sys` **2.1.2+zstd.1.5.7** | Full advertised **−131072 through 22**; checksummed, pledged-size standard frames |
| LZ4 | `lz4_flex` **0.14.0**, with safe encode/decode and checked-decode features | Fast **level 0**, safe independent block encoding and explicit-dictionary block decoding |

The [official Zstandard Rust bindings](https://github.com/gyscos/zstd-rs) supply
safe native contexts; the selected feature compiles bundled Zstandard with a
native C toolchain. The [official LZ4 implementation](https://github.com/PSeitz/lz4_flex)
supplies safe pure-Rust blocks. Generated code owns the LZ4 frame/checksum engine;
it does not use the provider's frame `Read` wrapper, whose legal empty raw block
can otherwise look like EOF. Linked history uses the explicit block dictionary
API rather than a synthesized prefix.

The options default remains level 6. Applications select ordinary Zstandard
level 3 or explicit LZ4 level 0; unsupported LZ4 integers reject before effects.
The selected adapter does not claim high-compression level support.

## Actual Windows compiler and SDK

The [maintained runner](../../test/rust_compression_optional_test.py) invokes
immutable compiler SHA-256
`cdaa775d0b98f5b2b0b640f9add6ff81224723cc6a82f021a46be16a65d0b337`,
built with strict MSVC **19.51**. The snapshot includes 2,306 source files with
inventory aggregate
`0664bc8400f40d1f9d4f2a0553458c2d143ab348beb9fddc1c4edcb7ba0969ca`.
Actual **Rust 1.99.0** and **Cargo 1.99.0** compile Rust 2021 with
`--release --offline --locked`, `-D warnings` and release overflow checks enabled.
The complete maintained invocation exits zero:

| Check | Actual result |
| --- | --- |
| Independent input decisions | **317/317**: 136 Zstandard and 181 LZ4 |
| Independent standard-frame outputs | **18/18** Python decisions preserve exact payload and EOF |
| Native interoperability | **6** C++ producers and **36** C++ whole-message/stream consumers pass |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callbacks and inner-suffix rejection |
| Generated profiles | **5/5**: normal, helper-name collision, new-type-name models, binary-only and deployment |
| Feature deployments | **4/4**: absent, Zstandard only, LZ4 only and both |
| Borrow safety | **6/6** expected compile failures; original exhaustive options/default constructors still compile and run with `rustc` alone |

The 18 outputs comprise eight generated model/protocol frames, eight empty or
multichunk Zstandard controls at minimum/−1/3/maximum levels, and two empty or
multichunk LZ4 fast controls. Independent Python **3.14.3** uses stdlib Zstandard
**1.5.7** and python-lz4 **4.4.5** with liblz4 **1.9.4**. The immutable C++ helper
copies use MSVC Release, Zstandard **1.5.7** and LZ4 **1.10.0**, including linked
LZ4 producers.

Inputs cover standard checked/unchecked frames, empty/multichunk data,
truncation, suffixes, concatenation, skippable/legacy framing, dictionaries,
checksum/size failures and exact/one-tight input, expanded and window bounds.
Zstandard covers descriptor/content-size forms, full level admission and
conservative non-power-of-two windows. LZ4 covers all block maxima, linked and
independent raw/compressed blocks, empty raw blocks and eleven history-prefix
boundaries.

Maintained controls exercise every observed cancellation boundary: **33 encode
and 32 decode** for Zstandard; **292 encode and 555 decode** for LZ4. They require
exact original error identity, late native-candidate withholding, cached-input
finish checks, cancellation after the final sink write, invalid provider progress,
unexpected context-error provenance and inactive first-cause fencing.

Six actual compile-failure controls reject output/input callback reentry, provider
destruction while either wrapper borrows it, provider mutation and an insufficient
provider scope. Separate exhaustive option literals prove that no required public
field was added. All default optional absence checks remain active.

The old immutable `3533d50e` compiler fails the maintained before-control because
it lacks the newly added provider APIs. This demonstrates feature absence; it is
not presented as a framing regression in that older implementation.

## Native Linux compilation and execution

The actual Windows-emitted sources, maintained harnesses, locked adapter crate,
independent fixtures and offline crate archives are hash-checked and streamed to
private native Linux storage. **Rust 1.93.1**, **Cargo 1.93.1** and Ubuntu
**GCC 15.2.0** compile and run the same checked release profiles without network
access or Windows mount execution.

The native Linux run passes **317/317** input decisions, all **five** profiles,
all **four** feature deployments, all **six** borrow rejections and the original
dependency-free constructor/option controls. Its **18** produced frames pass
independent Python checks and **36** Windows C++ consumers after returning.
Only Cargo dependency paths are relocated; generated Rust and maintained harness
source bytes remain unchanged. Consumer registry versions/checksums equal the
maintained adapter lock on both hosts.

The installed Linux Rust/GCC tools are preserved. Only the official
[Cargo component archive](https://forge.rust-lang.org/infra/other-installation-methods.html)
is unpacked task-locally, without running an installer: **10,782,872 bytes**,
SHA-256 `dbf12e8fd2a245a6bdd9f6c8df267cb2c25b0c56866ee91d8321337df0a00ddb`,
verified against the pinned official 1.93.1 channel metadata. The fresh Cargo
home and target directory remain inside the private task root; it is removed
after returning the evidence. No shared native build cache or global dependency
installation is used.

The 708-file streamed input inventory aggregate is
`90c6f0cae584290c27be0bad095d4f739f6588303c631641b23dd44cc25d3e92`.
This qualifies native Linux compilation and execution of actual Windows-emitted
Rust. It does not claim a Linux Serializer compiler rebuild for this snapshot.

## Core preservation and resource contract

The unchanged [core runner](../../test/rust_compression_test.py) also passes
against this compiler on **Windows Rust 1.99.0** and **native Linux Rust 1.93.1**,
without Cargo or optional crates: each passes **974/974** inputs, **64/64**
independent outputs, **three** profiles and **two** callback-borrow rejections.
Both enforce the exact original **31,535-byte** native model/runtime prefix,
SHA-256 `ccfa73c8a58bc4a453cb9e04fa83c5125760ae3cc65c5a5513d63da2b6db1eca`.

The complete normal source is **97,906 bytes**, SHA-256
`e342588455a72c305e3f54e5b987c2b54efb698fd763ca5296bffdde45c590ca`.
The new helper body increases generated source size while preserving ordinary
native APIs and bytes. A separate actual-output navigation check passes **28**
decisions for models named `Provider`, `Encoder`, `Decoder` and `Progress`;
indented helper declarations do not steal root model navigation. No editor
payload change is needed.

Generated Zstandard preflight independently checks standard frame extent,
dictionary absence, content-size/history bounds and checksum position before
provider expansion. History rounds down to a power of two with a 1 KiB minimum.
The provider receives the same ceiling, clamped to its native supported maximum.
Input/output context slices are at most **32 KiB**, with a bounded output overflow
probe. Generated XXHash64 checks the content digest after decoding.

Generated LZ4 preflight checks descriptor/block checksums, token expansion,
available history, exact size and EOF before expanded allocation. Input blocks
are bounded by their admitted **64 KiB–4 MiB** maximum plus at most **64 KiB** of
explicit prior history. Output uses independent **64 KiB** blocks with checked
content and **128 KiB** of fixed encoder scratch, satisfying the safe provider's
padded output-buffer requirement. XXHash32 validates content independently.
Readers accept checked and unchecked standard frames; optional outputs always
carry content checksums.

Complete messages are staged. Logical caps exclude allocator spare capacity,
provider workspace, caller native objects, total heap and process RSS.
Cancellation is cooperative between bounded calls and cannot preempt provider or
application code. Passing mutable references retains caller transport ownership;
passing owned adapters preserves the original ownership-transfer semantics.
There is no implicit standard `Read`/`Write` nesting or general shared XML/native
operation-budget propagation.

## Provenance and reproduction

| Source | SHA-256 |
| --- | --- |
| Core runtime template | `45f51b8f5a2346633fb5bd80db0010683f3eb3a3f892b8b04c30df7420787c7b` |
| Provider runtime template | `72c1719d6f8954ed03b8bacf10e79a1cb45bb3c6dc0f4e63c051c4aad82500c8` |
| Rust emitter | `43784265f43db1f47d06e9e1ac519c66404a56bbee8877808f487dba63bd5224` |
| Maintained optional runner | `c0cd706121e250494471ed9befa64628c8206e3200480ec2bb2cc7d6a03d49a9` |
| Adapter manifest | `cda5ac1170101b11782da708770a5a0f74b9f269087c0844a1dba4013b32398c` |
| Adapter lock | `99e4f4420711340026cf64fe03b6c8cdd3fe89e9b4463ba62e120fec87471449` |
| Adapter library | `fd3cc5d46accdab732b9c476a77f1ad744a547ca99a0b706420954fa30756a8b` |

Compiler strings stay separate: **39,030-byte** core and **27,269-byte** provider
bodies, each split below MSVC's **16,380-byte** raw-token ceiling. Global literal
and scoped whitespace checks pass.

```powershell
python -B test/rust_compression_optional_test.py `
  --compiler <matching-serializer.exe> --build out/rust-optional `
  --cargo-home <prepared-offline-cargo-home> `
  --provider-path out/python-compression-provider `
  --cpp-helper <compression_interop.exe> `
  --cpp-linked-helper <compression_linked_lz4.exe>
```

Missing SDKs, dependencies or native helpers fail explicitly; the runner performs
no online fallback. Optional qualification is explicit rather than an automatic
Cargo download in the dependency-free core CTest.

Retained evidence beneath `out/build/backlog-2026-10-20/` includes
`rust-optional-before.log`, `rust-optional-final.log`,
`rust-optional-final/verification.json`, `rust-optional-linux-final.log`,
`rust-optional-linux-final/{linux_verification.json,source_inventory.json,commands.json}`,
both `rust-core-provider-update` Windows/Linux reports and logs, and
`rust-optional-navigation.log`. The ignored `rust_optional_linux_replay.py`
retains the offline streaming workflow. These selected SDK checks are separate
from aggregate native CTest results for other source snapshots.
