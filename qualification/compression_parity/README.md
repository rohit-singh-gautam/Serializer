# Selected shared compression parity

The [manifest](manifest.json) declares eleven mandatory SDK profiles and ten
runtime implementations. TypeScript checks the actual generated declarations
with strict TypeScript and runs the same JavaScript implementation. Each profile
produces and consumes all six formats (`NONE`, `GZIP`, `ZLIB`, `DEFLATE`, `ZSTD`,
`LZ4`) across JSON and all three native binary protocols.
The [dated execution record](../../docs/verification/verification-compression-parity-2026-10-20.md)
identifies the actual SDK/provider profiles and distinguishes completed checks
from any required repair follow-up.

Run the [driver](../compression_parity_qualification.py) from the repository root:

```sh
python -B qualification/compression_parity_qualification.py \
  --compiler /path/to/immutable/serializer \
  --config out/compression-sdk-config.json \
  --build out/compression-parity \
  --output out/compression-parity.json
```

Copy [the configuration example](sdk_config.example.json) into ignored output and
replace every placeholder with an explicitly selected tool, installed runtime or
provider path. The example uses Windows tool names. Import the native developer
environment first; Swift also needs its matching `SDKROOT` and runtime/toolchain
directories on `PATH`. The tested SDKs and provider artifacts belong in the dated
proof, rather than in machine-specific tracked paths.

The driver requires every requested SDK and provider. It generates actual owning
models, compiles the adapters, validates producer output independently, runs every
consumer and replays the maintained backend lifecycle/deployment runners. Missing
tools, missing providers, absent jobs, unexpected errors and partial inventories
fail. `--prepare-only`, `--languages` and `--skip-lifecycle` are diagnostic modes;
they cannot write a successful full qualification report.

The [C++ runtime refresh driver](../compression_parity_cpp_refresh.py) handles
the dated isolated native Zstandard repair without rerunning unchanged SDK
builds. It first seals the completed baseline with `--seal-only`. A normal
refresh requires exactly the known C++ window failure, all ten other consumers,
all 17 successful SDK suites, unchanged baseline files/provider hashes, and an
isolated `src/compression.cpp` compiler-input delta. It regenerates and compares
the other SDK sources, rebuilds C++, requires all 24 C++ producer frames to remain
byte-identical, and repeats 1,400 C++ decisions, 264 model exchanges and 25
callback controls. Any additional failure or drift requires the complete driver.
The resulting report distinguishes the original SDK compiler from the refreshed
native runtime; it never claims that reused suites ran on the later artifact.

The declared decisions are:

- 264 producer frames and 2,904 owning-model exchanges: every producer to every
  consumer, with all four protocols and six formats. Independent Python providers
  require one complete frame and exact original native bytes.
- 792 encoding-bound decisions: each selected producer admits its exact serialized
  input/compressed output sizes and rejects either one-too-tight bound. C++ applies
  these compression bounds to the ordinary generated native bytes; its generated
  owning serialization and transactional publication have separate native tests.
- One identical corpus for every consumer: the existing 974 core and 317 optional
  inputs, exact/one-tight byte caps, conservative history budgets and 64 additional
  advertised-history decisions. The latter use fixed, dynamic and stored blocks,
  all seven smaller zlib windows, repeat symbols 16/17/18, degenerate valid trees
  and independently rejected malformed inputs.
- 25 focused C++ callback/publication controls and 17 maintained SDK runners for
  cancellation, first failures, borrowed ownership, same-wrapper reentry, provider
  absence/deployment, native protocol admission and safe borrow rejection.

The owning model has stable fields `account_id` and `label`; every consumer checks
the numeric value and Unicode label after its generated exact decoder succeeds.
Fixture producers are shared with the maintained tests. The independent Python
oracle needs CPython's standard Zstandard API and the explicitly selected
python-lz4 package. Java/Kotlin jars, managed .NET assemblies, Go modules, Rust
crates, the caller-owned JavaScript block module and Swift native libraries are
explicit dependencies of this qualification. Default generated runtimes keep
their documented optional-provider admission behavior.

Use prepopulated provider caches. Go runs with `GOPROXY=off`/`GOSUMDB=off`, Rust
uses its maintained lock with `--offline --locked`, and .NET uses empty package
sources. The driver installs nothing and gives each SDK its own build directory.
It writes command logs, generated-source hashes, provider/runtime hashes, corpus
inventories and a final JSON report under the requested ignored output.

This inventory is separate from the XML S35 manifest. It qualifies selected
whole-message compression implementations, framing, limits and operation
lifecycle. It does not claim a platform/SDK matrix, nested XML/OPC shared contexts,
throughput, physical heap/RSS limits, or preemption inside caller/native code.
