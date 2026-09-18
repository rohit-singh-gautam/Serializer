# Serializer examples

Each example lives in its own folder with its inputs and consumer sources.
Generated output stays in the build tree.

- [Configuration schema](config/README.md): a small C++ schema with inheritance.
- [C++ coding styles](coding_styles/README.md): nine independently selectable profiles.
- [Iostream examples](iostream/README.md): seven memory, file, buffered, and custom
  stream examples sharing a large 52-class telemetry schema.
- [Compression examples](compression/README.md): Zstandard, LZ4, gzip,
  zlib, raw DEFLATE, uncompressed output, and a custom backend with explicit limits.
- [Java examples](java/README.md): a runnable pure Java round trip and three style profiles.
- [Schema includes](includes/README.md): three paired C++/Java examples for shared
  types, reopened namespaces, and repeated/diamond includes.
- [Five-language interoperability](interoperability/README.md): one shared schema,
  independent C++/Java/JS/Go/C# producers and consumers, all 25 language pairs,
  four protocols, a browser example, and optional throughput measurements.
