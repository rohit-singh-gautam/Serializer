# C compression verification, 20 October 2026

This record qualifies **C-Z01–C-Z08** for compiler/runtime **2.14.0**, schema
language **1.20.0**, baseline `0928053389fcf237e53f682442510ae19ef5eb5c` plus
working-tree changes. The generated C11 helpers supply identity and optional
zlib-backed gzip/zlib/raw DEFLATE. Zstandard/LZ4 are disabled in this selected
core profile. Later independent compile-time providers are qualified in the
[optional C provider proof](verification-c-compression-providers-2026-10-20.md).
After that integration, all 974 inputs, 48 independent outputs and five core
profiles passed again on both SDKs against immutable compiler `97148980…`;
the later record contains its exact hash and retained replay logs.
The [C API guide](../languages/c_compression.md) defines ownership and policies.

## Actual generated SDK profiles

The real Windows compiler emits every tested header. Windows executes native
MSVC C11; a second run streams the exact emitted source and independent fixtures
to private Linux-local temporary storage and executes the native C SDK there.
The latter does not require Windows-mounted I/O or claim a Linux C++ compiler
rebuild for this slice.

| Profile | SDK/provider | Executed result |
| --- | --- | --- |
| Windows x64 | MSVC **19.51.36260**, `/std:c11 /W4 /WX /O2`; zlib **1.3.2** | **974/974** independent input decisions, **48/48** independent output checks, **5/5** generated compile/run profiles |
| Linux x86-64 | GCC **15.2.0**, `-std=c11 -Wall -Wextra -Wpedantic -Werror -O2`; zlib **1.3.1** | Same **974/48/5** inventory passes |

The five SDK profiles are normal, provider-free, preserved model-name collisions,
enum-constant collisions and binary-only output. Provider-free compilation links
no zlib library and checks identity plus explicit unavailable-provider behavior.
Normal output is **84,341 bytes**, SHA-256
`d399dea3248c377f4a6a6e325ab074819b6a17b4e03cc000749415ff68860cb8`,
identical across the Windows-generation runs.

Independent Python zlib **1.3.1.zlib-ng** verifies exact output EOF and bytes:
**16** actual native-model outputs cover all four protocols and outer formats;
**32** empty/multichunk controls cover levels 0/1/6/9 across all four formats.
The **974** independent input decisions reuse the maintained Java corpus and
seventeen separately authored grammar/history/window fixtures. Exact and
one-tight input/output policies are checked for accepted inputs.

Additional executed assertions cover:

- Typed fresh native decode, exact inner consumption, unchanged old destinations
  on suffix/codec failure and preserved native `srl_status` diagnostics.
- Empty output, exact/one-tight compression caps, seven-byte borrowed reads,
  bounded complete sink writes and one-byte source overflow probing.
- Source/sink failures, zero/oversized transport results, explicit finish,
  unread expanded input, cached decoded reads and abandoned close.
- Same-wrapper callback reentry, callback-driven close with deferred cleanup,
  final sink cancellation, invalid-argument fencing and retained first status.
- Explicit provider admission, standalone identity and preserved helper/model
  and enum namespaces.

The grammar pass validates dynamic trees, actual expansion, available/declared
history and exact frame extent before expanded allocation. Native zlib performs
framing/checksum completion. Provider state is finalized on success or failure.
An independent source review found no additional concrete ownership, arithmetic,
framing or lifecycle defect in the final implementation.

## Provenance and reproduction

The immutable actual compiler SHA-256 is
`7eecf615f7a7a0c260c77776d9ddec49f53fa360684306fabb8fce054762b5c1`.
Frozen inputs are:

| Source | SHA-256 |
| --- | --- |
| `src/c_writer.cpp` | `9fa62ec1c5cb904cab6bc11961cf3bca2111ecadb4333cc1e252536040b0bb33` |
| `src/c_compression_runtime.inc` | `78fce5c7f721507bc270327a69d7a5e2e6920251f5a31f8f7bed954fc7220780` |
| `src/c_compression_validation_runtime.inc` | `876a3cc3db36e3a481696efb51fd1f40987c8bf657d3ca5f66d32c7861bc568f` |
| `src/c_compression_stream_runtime.inc` | `da72b8187e3c849c956c16c67765eb71e08f384266eb8640c4e7a2eef9549b94` |
| `test/c_compression_test.py` | `ecd7a5f27b2fc3b00f0d3c433c5b50b7d20288b82f9b14132f7b199a89c7b422` |

The [maintained runner](../../test/c_compression_test.py) accepts `--compiler`,
`--build`, `--cc` and optional explicit zlib include/library paths. On Windows,
run it in the Visual Studio developer environment. `--wsl` streams generated
inputs to native `/usr/bin/cc` and returns outputs for the independent oracle;
it records each exact input archive hash and automatically removes temporary
native projects. CTest registers `serializer_compression_c` when the C SDK,
Python and configured native zlib dependency are available.

Final evidence is retained under ignored `out/build/backlog-2026-10-20/`:
`c-compression-{windows,linux}-final.log`, their matching `verification.json`
directories, `windows-c-symbol-compiler.log`, and
`compiler-c-final-7eecf615/` with compiler/DLLs and source hashes. Earlier setup
and intermediate profiles are separate from this final run.

Logical byte/history limits exclude native model allocations, provider workspace,
allocator overhead and total RSS. Native encoding allocates its ordinary bytes
before their compressed-input size check. Cooperative cancellation does not
preempt blocking native/application calls. These profiles do not qualify other
C SDKs/platforms, optional Zstandard/LZ4, hard peak-memory/CPU limits or the broader
shared-operation **C-Z11** contract.
