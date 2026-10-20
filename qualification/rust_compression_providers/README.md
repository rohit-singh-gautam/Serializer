# Explicit Rust compression provider adapters

This maintained qualification library adapts pinned **zstd 0.14.0** and
**lz4_flex 0.14.0** to the generated Rust compression `Provider` contract.
It is a local, unpublished 0BSD example. Ordinary generated source and default
compression constructors remain dependency-free; linking the library does not
change default availability or select a provider implicitly.

The Cargo features `zstd` and `lz4` are independent and disabled by default.
Zstandard supports its provider-advertised level interval; this LZ4 adapter
supports the explicit fast level **0**. It does not implement LZ4 HC levels.
The maintained lock pins registry dependencies. The qualification runner checks
each consumer's resolved registry packages against that lock and uses an explicit
offline Cargo home without a network fallback.

Select a caller-owned module name and the actual generated helper path:

```rust
mod schema;
use schema::serializer_compression as compression;
serializer_compression_providers::zstd_provider!(zstd_adapter, crate::schema::serializer_compression);

// Borrow one explicit provider for this operation; its lifetime covers every codec call.
fn compress_message(bytes: &[u8]) -> compression::Result<Vec<u8>> {
    compression::compress_with_provider(bytes, compression::EncodeOptions {
        format: compression::Format::Zstd,
        level: 3,
        ..Default::default()
    }, &zstd_adapter::Zstd)
}
```

`lz4_provider!(module, helper_path)` similarly defines `module::Lz4`.
Helper collisions may change the generated helper module name; use the exact
name recorded in the generated file's comment. Provider contexts are private to
each operation, borrowed wrapper/provider lifetimes are enforced by safe Rust,
and provider calls operate on bounded slices supplied by the generated runtime.

Run the maintained actual-SDK qualification explicitly:

```text
python -B test/rust_compression_optional_test.py --compiler <serializer> --build <ignored-build-directory> --cargo-home <offline-cargo-home> --provider-path <python-lz4-directory> --cpp-helper <compression_interop> --cpp-linked-helper <compression_linked_lz4>
```

The runner requires actual Rust/Cargo SDKs, Python's standard Zstandard provider,
the selected Python LZ4 provider and both native interoperability executables.
It verifies independent input framing/bounds, output bytes/checksums, native
interchange, callback/error/lifecycle behavior, malformed provider progress,
compile-failing borrow controls, preserved option literals, generated model
collisions and all four Cargo feature deployments. Missing prerequisites fail;
the optional provider runner is not part of dependency-free default CTest.

See the [Rust compression guide](../../docs/languages/rust_compression.md) for
the generated API and current qualification scope.
