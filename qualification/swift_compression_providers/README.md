# Explicit Swift native compression adapter

This caller-owned 0BSD qualification example adapts stable Zstandard and LZ4
frame C APIs to a generated Swift compression helper. It is not a generated
runtime dependency or a global registration API. The selected profile uses
Zstandard1.5.7 and LZ4 1.10.0. Ordinary generated Swift needs neither C module
nor native library; its default optional selectors remain unavailable.

Copy the C shim/header/module map and the adapter into your application's native
build, keeping the native libraries and their licenses/deployment requirements.
Replace `__COMPRESSION_NAME__` in `adapter.swift` with the exact generated helper
name recorded by its comment. The adapter extends that container, keeping its
implementation error class inside the same collision-safe namespace. When the
generated code is in another Swift module, import it and use the qualified helper
path. Adapt the application-owned C module name if its build requires it.

```swift
typealias C = SerializerCompression
let provider = C.nativeProvider(.zstd) // Declared by this copied reference adapter.
let packed = try C.compress(serialized,
    options: .init(format: .zstd, provider: provider, level: 3))
let bytes = try C.decompress(packed,
    options: .init(format: .zstd, provider: provider, maxWindowBytes: 8*1024*1024))
```

The original generated option initializers retain their exact signatures.
Selecting a provider uses a separate initializer with required `provider:`;
provider-specific default levels are3 for Zstandard and0 for LZ4. Zstandard's
level endpoints and LZ4's maximum are queried from the linked implementation.
This LZ4 frame adapter admits0 through the native maximum, unlike the fast-only
pure-language adapters in some other backends.

Each operation creates an exclusive native context, pledges known Zstandard
input size and emits a content checksum. Private LZ4 output uses independent
64 KiB blocks with block/content checksums. Its pending native encoder scratch
is derived from `LZ4F_compressBound`; it is separate from logical output bounds.
Context input/output spans and generated borrowed I/O are at most32 KiB. Native
LZ4 decoding may work on the admitted64 KiB through4 MiB block and retain its
history internally. Logical limits exclude provider workspace and process RSS.
Cancellation is cooperative around provider calls, without native preemption.

Generated code independently validates frame extents, dictionary absence,
advertised size/window and exact EOF before decoder-context creation. The native
providers validate compressed grammar and checksums. Only known native frame
errors map to `Failure.invalidData`; history admission has its resource category,
and unrelated native failures use the adapter's own error class. Application
errors thrown by custom contexts retain their original object/type.

A `Context` is private to one operation. Cleanup occurs once, including on failure.
Reusing or reentering one is fenced; cleanup requested by a nested failed call is
deferred until the active step returns. Context callbacks must not retain borrowed
buffers, resize output storage or share mutable codec state. Keep providers,
models and transports stable through each synchronous operation. This example
performs no synchronization and installs no global state.

The [maintained actual SDK runner](../../test/swift_compression_optional_test.py)
requires explicit Swift/C11 SDKs, native header/library locations, Python provider
oracles and immutable C++ helpers:

```text
python -B test/swift_compression_optional_test.py --compiler <serializer> --build <ignored-output> --cc <C11-compiler> --provider-include <native-include> --zstd-library <native-zstd-library> --lz4-library <native-lz4-library> --provider-runtime-directory <native-dll-directory-if-needed> --provider-path <python-lz4-directory> --cpp-helper <compression_interop> --cpp-linked-helper <compression_linked_lz4>
```

The normal fixture checks317 independent/native frames,24 independently consumed
outputs, all four ordinary native protocols, original initializer function values,
provider failure/lifetime boundaries and borrowed lifecycle. Four additional
profiles cover helper collisions, provider/error model names, binary-only output
and a program compiled without the C module or native libraries. See the
[Swift API](../../docs/languages/swift_compression.md) for generated contracts.
