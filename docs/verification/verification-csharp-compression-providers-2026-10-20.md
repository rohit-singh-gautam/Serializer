# C# optional compression providers, 20 October 2026

[C# compression API](../languages/csharp_compression.md) ·
[Core SDK proof](verification-csharp-compression-2026-10-20.md) ·
[Compression contract](../serialization/compression.md)

This record qualifies **CS-Z09/CS-Z10** for the selected managed-provider profile
in compiler/runtime **2.14.0**, schema language **1.20.0**, on
baseline `0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes.
Actual Windows SDK output runs on Windows and a private native Linux runtime.
Broader **CS-Z11** shared-operation parity, other platforms, trimming/AOT and
general performance gates remain separate.

## Explicit providers and compatibility

The generated source compiles without optional references or NuGet packages.
Applications select an assembly explicitly with `Provider.ZstdSharp(assembly)`
or `Provider.Lz4(assembly)`, then use `EncodeOptions.WithProvider` and
`DecodeOptions.WithProvider`. Capability handles are immutable; each operation
owns its private codec contexts. There is no global provider registration or
assembly probing. The caller retains ownership of the assembly/load context.

The original public constructors and their overload resolution remain intact,
including `new EncodeOptions(Format.None, default)` and the corresponding decode
call. Static factories avoid an ambiguity that a second public constructor with
a provider argument would introduce. The maintained SDK profiles compile both
legacy calls. Original constructors and `Available(format)` retain the default
profile: optional formats fail before callbacks or I/O even if an optional
assembly is loaded elsewhere. Use `Available(format, provider)` for an explicit
selection. Ordinary model APIs, protocols and uncompressed wire paths remain
unchanged; the additional helper code increases generated-source size.

| Provider | Pinned artifact | Selected assembly SHA-256 |
| --- | --- | --- |
| Zstandard | `ZstdSharp.Port` **0.8.8**, `lib/net8.0/ZstdSharp.dll` | `6844a1af1027745ee68118e9e23ad46335a2aa8755826f568e364a48bc784930` |
| LZ4 | `K4os.Compression.LZ4` **1.3.8**, `lib/net6.0/K4os.Compression.LZ4.dll` | `e282aadc2353b864fe95cd5fb525a1a093d99a30a5bf2530b7ab831d56179816` |

The [official ZstdSharp project](https://github.com/oleg-st/ZstdSharp) provides
managed streaming contexts, pledged sizes, checksums and window parameters.
The [official K4os project](https://github.com/MiloszKrajewski/K4os.Compression.LZ4)
provides safe block APIs with an explicit prior-history dictionary. These selected
assemblies need no native compression DLL or additional runtime package.
Their NuGet archive SHA-256 values are respectively
`f6797192deebf4535f0601c2273e750da0c02062e30a3e4a3c4011b9ac22403a` and
`3a64f72703b8aa99190cbed7aa216a642cb1c928fae2cf6dfa65dc3754fe2320`.

Zstandard defaults to level 3 and admits its full advertised integer range,
**−131072 through 22**. LZ4 defaults to fast level 0 and accepts its defined
levels **0 and 3–12**. BCL `CompressionLevel` options retain their original
meaning. Missing capabilities, mismatched provider selections and invalid levels
reject before cancellation, codec or transport callbacks.

## Actual emitted-code execution

The [maintained optional runner](../../test/csharp_compression_optional_test.py)
invokes immutable compiler SHA-256
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`.
It compiles actual emitted source with **SDK 8.0.426**, **net8.0**, C# **12**,
nullable checks and warnings as errors, then runs on **Microsoft.NETCore.App
8.0.32**. Package sources are cleared; optional assemblies are loaded at runtime.
The complete maintained invocation exits zero:

| Check | Actual result |
| --- | --- |
| Independent input decisions | **317/317**: 136 Zstandard and 181 LZ4 |
| Independent output checks | **24/24** Python-provider decisions preserve complete bytes and exact EOF |
| Native interoperability | **6** actual C++ producer fixtures and **48** C++ whole-message/stream consumers pass |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callbacks and inner-suffix rejection |
| SDK profiles | **9/9**, each with zero warnings and errors |
| Deployments | **4/4**: both absent, Zstandard only, LZ4 only and both selected |
| Lifecycle | Bounds, borrowed ownership, finish/dispose, callback close/reentry, first-error identity and cancellation pass |

The nine SDK profiles cover normal generation, preserved helper-name collisions,
models named after the new provider types, binary-only generation, checked integer
arithmetic, and each of `Provider`, `OptionalCheck`, `ZstdApi` and `Lz4Api` as the
outer output class. Existing helper-name models remain usable with a suffixed
helper. The normal generated source is **104,742 bytes**, SHA-256
`09b9cc6108d5b175cbdf8b7dd805c4552ce4c7ba041030a8a3833d7ccab7d7a4`.

Independent inputs include checked/unchecked standard frames, empty and multichunk
payloads, truncation, trailing/concatenated input, skippable/legacy frames,
dictionaries, size/checksum failures and exact/one-tight input, output and window
caps. Zstandard covers content-size forms, descriptor bits, conservative
non-power-of-two windows and full level admission. LZ4 covers all block maxima,
independent/linked raw or compressed blocks and eleven history-prefix boundaries.
The native producers include linked LZ4 frames.

The runner reuses actual owning-model and borrowed-wrapper controls for both
formats, including every observed provider cancellation boundary. Active calls
retain the original exception object, even when reflection/provider cleanup might
otherwise wrap it. Late decoder cancellation withholds the fresh candidate.
Independent outputs use levels 0/1/6/9 for Zstandard and 0/3/6/12 for LZ4, with
empty and multichunk controls. The independent Python **3.14.3** providers are
stdlib Zstandard **1.5.7** and python-lz4 **4.4.5** with liblz4 **1.9.4**.
Native C++ helpers use MSVC Release, Zstandard **1.5.7** and LZ4 **1.10.0**;
their executables and app-local DLLs are copied privately before replay.

The unchanged [core runner](../../test/csharp_compression_test.py) also exits zero
against the same compiler without optional references: **974/974** independent
inputs, **16/16** independent outputs and **5/5** strict SDK profiles. Its default
optional-provider absence checks continue to pass.

## Native Linux runtime replay

The exact Windows-generated sources, SDK-built portable IL, independent input
fixtures and provider assemblies were hash-checked and streamed into an isolated
native Linux directory. Microsoft.NETCore.App **8.0.32**, **linux-x64**, passes
**317/317** input decisions, the normal and eight additional profiles, and all
four deployment cases. The **24** Linux-produced frames pass independent Python
checks and **48** C++ whole-message/stream consumers after returning to Windows.

Only the runtime is extracted into that private directory, using the official
[manual binary deployment workflow](https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual).
Its **31,316,637-byte** archive is verified against Microsoft's pinned release
metadata, SHA-512
`639a17d84e4d52fecd62c7d4e528e7bc0ae3c9860de2530cb41a2ee7196b69ea18271d8795849180742770e25b4f913df292a5a236fa197cf5d3cd0ec5ba665f`.
No global installation, Windows mount access or shared native cache is used.
The native runtime reports no installed SDK. This proves Linux execution of
Windows-compiled IL; Linux C# compilation and a Linux replay of the separate core
974-input suite are not claimed.

The 922-file input inventory aggregate is
`a916860ffcf00f10e98f7b75a7836f27213d93d282841d0a4f4b05dfd7420a5b`.
Logs, command arguments, input hashes, runtime metadata and returned frames remain
under `out/build/backlog-2026-10-20/csharp-optional-linux-final/`; the private
Linux directory is removed after returning its evidence. The matching ignored
`csharp_optional_linux_replay.py` retains the reproducible streaming workflow.

## Framing, bounds and ownership

Optional outputs always carry a content checksum; readers accept standard checked
and unchecked input. Zstandard pledges the known input size. Before provider
expansion, generated validation checks one standard frame, dictionary absence,
content size, block extents, checksum location and exact EOF. The window ceiling
rounds down to a power of two, with a 1 KiB minimum, and is also applied to the
provider context.

LZ4 independently validates descriptor/block/content checksums, size, exact EOF,
token expansion and back-reference history. Expanded bounds are checked before
allocation. Linked blocks use the provider's explicit dictionary API with at
most 64 KiB of prior output; external dictionaries remain unsupported. Input
block maxima of 64 KiB through 4 MiB must fit the separate window budget. Output
uses independent 64 KiB blocks.

Zstandard provider buffers and borrowed transport batches are at most 32 KiB.
LZ4 block calls use the admitted block maximum plus retained history. Polling is
cooperative before/after bounded calls, during validation and after final native
or sink callbacks; it cannot preempt provider/application code. Complete messages
are staged. Logical byte/window bounds exclude provider workspace, allocator
spare capacity, caller codec objects, total managed heap and process RSS.
Borrowed transports remain caller-owned; failure may leave an external prefix.

## Provenance and reproduction

| Source | SHA-256 |
| --- | --- |
| Core C# compression template | `4c2816e7d434ea24a8b13e91076be190e79bde11a1882d50db3c18c2cd1ce81c` |
| Optional capability template | `9e525f5231c0ec4cb8237b1e59d577616fcb7ce97d137564a445e2c64036c4a5` |
| Zstandard template | `5d3563d305dfda967eaf24a512cc0e814dfce391fa19491bb58d9d247603b937` |
| LZ4 template | `cb0099430d6b0e4cdacdb2f3cde488e37651b5d1536d30b4c52bdb533d12b135` |
| Optional maintained runner | `cb418ff4428ba1c6d62d1cc0865f7e5abd5455056846ec74746be6aca305776d` |

The global runtime-literal check and scoped whitespace check pass. Optional
template bodies are separate compiler strings to preserve MSVC token and joined
string limits. Provider packages, locked versions, final SDK projects, independent
fixtures, outputs and logs remain under ignored task output; no global dependency
installation is required.

```powershell
python -B test/csharp_compression_optional_test.py `
  --compiler <matching-serializer.exe> --build out/csharp-optional `
  --zstd-dll <ZstdSharp.dll> --lz4-dll <K4os.Compression.LZ4.dll> `
  --provider-path out/python-compression-provider `
  --cpp-helper <compression_interop.exe> `
  --cpp-linked-helper <compression_linked_lz4.exe>
```

Retained evidence is `out/build/backlog-2026-10-20/csharp-optional-final.log`,
`csharp-optional-final/verification.json`, `csharp-core-provider-update.log` and
`csharp-core-provider-update/verification.json` beneath that same task directory,
plus `csharp-optional-linux-final.log` and
`csharp-optional-linux-final/linux_verification.json` for the native runtime replay.
The original core performance observations describe their earlier source snapshot;
this replay does not establish a new performance baseline or an aggregate native
CTest result.
