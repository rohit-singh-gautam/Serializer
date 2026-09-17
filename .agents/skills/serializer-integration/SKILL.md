---
name: serializer-integration
description: "Integrate Serializer into C++ or Java applications from a provided repository or existing dependency. Use for .serializer schemas, CMake generation, language-specific coding profiles, owning classes or C++ binary views, stable_ids, stream concepts and iostream adapters, JSON or binary codecs, C++ Protobuf binary/ProtoJSON/TextProto protocols, and schema migration."
---

# Serializer Integration

Implement the user's requested Serializer integration using the version present
in their project. Preserve established wire IDs, names, protocol choices, and
application ownership requirements.

This is the canonical integration entry point linked from the repository's README
and AGENTS.md. Apply it when loaded as a skill or read directly from a supplied
checkout; direct use does not require copying the skill into the consuming project.

## Locate the library and the requested work

Find the application's existing Serializer dependency, schemas, generated-header
build rules, and callers before adding another copy. In this bundled layout,
the Serializer repository root is three directories above this skill. If the
skill has been copied or installed elsewhere, locate the matching checkout from
the application's dependency/build configuration; resolve the following files
there instead of relying on the relative links.

- Read [README.md](../../../README.md) for supported syntax and feature status.
- Use [docs/usage.md](../../../docs/usage.md) for the schema, CMake, and codec examples.
- Use [docs/java.md](../../../docs/java.md) for pure Java 17+ generation, profiles,
  type mappings, supported schema features, limits, and protocol compatibility.
- Use [docs/cmake_integration.md](../../../docs/cmake_integration.md) for the shipped
  generation helper, source dependencies, installed packages, and shared schemas.
- Use [docs/views.md](../../../docs/views.md) for view generation, mapping, and mutation.
- Use [docs/output_configuration.md](../../../docs/output_configuration.md) for
  language sections, coding profiles, naming, formatter configuration, and examples.
- Use [docs/intellisense.md](../../../docs/intellisense.md) for generated include
  errors, editor configuration, and generating headers without compiling consumers.
- Use [docs/editor_extension.md](../../../docs/editor_extension.md) for the optional
  VS Code extension, local VSIX installation, and CMake header assistance.
- Use [Visual Studio extension](../../../editors/visual_studio/README.md) for the
  separate Visual Studio 2022/2026 x64 grammar package and its build/install steps.
- Read [docs/wire_format.md](../../../docs/wire_format.md) when choosing protocols,
  limits, failure handling, or compatibility behavior.
- Read [migration.md](../../../migration.md) when updating older headers or APIs.

Use local instructions from the repository being edited. When a path or message
contract cannot be determined from the project, ask for that missing information.
Do not turn an integration request into a library redesign or implement a proposed
feature as a prerequisite without the user's request.

## Author or evolve the schema

- Use `.serializer` files beginning with `serializer version 1;`, before declarations
  (leading comments are allowed). Rename older `.def`/`.struct` inputs and update
  build references. The compiler requires this header; library fragment parsing
  remains available through `parser::parse(input)`. Schema language version 1 is
  independent of compiler release 1.0.0 and wire protocols. Use `serializer --version`
  to inspect both; see [CLI and versioning](../../../docs/command_line.md).
- Use `class`, `enum`, and `namespace`. Every member needs an explicit access
  modifier and a trailing semicolon. Classes/enums have no trailing semicolon.
- Share schema declarations with `include common.serializer;` after the version
  header and before declarations. Paths are unquoted, relative to the including
  file, and use forward slashes; `./` and `../` work, while spaces, backslashes,
  absolute paths, and quoted paths are rejected. Every dependency needs its own
  version header. Files load once per entry compilation; cycles are errors.
- Reopened namespaces reuse their scope during parsing. Duplicate qualified
  classes/enums and namespace/type conflicts are errors; same leaf names in
  different namespaces are valid. Referenced types still precede their use.
- Generate only the entry schema for a combined model. Included declarations are
  emitted into that entry's C++ header or Java compilation unit; overlapping C++
  entry graphs can produce duplicate definitions when their headers are included
  together. Both CMake helpers track transitive inputs using depfiles. Library
  callers use `parser::parse_file(path)` and pass its `statements` to the writers.
  See [include usage](../../../docs/usage.md#share-declarations-with-includes) and
  [paired C++/Java examples](../../../example/includes/README.md).
- In owning representations, map `array T` to `std::vector<T>` and `map(K) T` to `std::map<K, T>`.
- Keep literal spaces in quoted defaults, such as `public string label { "schema default" };`.
  The parser preserves quoted whitespace, escapes, and braces without rewriting the literal.
  Preserve wire names and defaults. Check the output naming policy before writing
  callers; `naming = preserve` retains existing schema-defined C++ identifiers.
- Put class attributes after the name, before a parent list. `stable_ids` requires
  explicit IDs on every member and parent; it does not assign IDs or compare old
  schemas. For new evolving integer-key contracts, prefer this check.
- When enabling `stable_ids` on an existing class, determine the previously emitted
  IDs first and write those exact numbers. Implicit slots count parents followed
  by members in declaration order; an explicit override does not shift later
  implicit slots. Parent/member IDs share the containing class's ID space.
- Keep IDs unique in `1..0x3fffffff`. Preserve wire names with metadata such as
  `public string name ("fullname", 3);` when renaming a C++ field. Do not reuse old
  IDs/names or renumber enum/union alternatives in an existing contract.
- Keyed readers reject unknown fields. Explicit IDs do not make old readers accept
  new fields. Positional binary still requires matching declaration order/types.
  Resolve incompatible changes through the application's versioning contract.
- Check supported types before choosing raw unions: generated alternatives must
  be trivially destructible. Ordinary generated enums use names in JSON/string-key
  fields; binary collection enum elements use numeric values.
- Select only the requested representations: no keywords or `owning` means an
  owning class; `view` means both views; `view readonly` or `view mutable` restricts
  the views. Adding `owning` retains owning storage too. Attributes are unordered;
  `readonly`/`mutable` require `view`. One mode generates a plain class; multiple
  modes require `storage_mode::owning`, `read_only_view`, or `mutable_view`.
- Nested classes/parents must enable each mode their owner needs; map keys require
  read-only nested views. Declare referenced types before use. `packed` cannot be
  combined with `view`. `stable_ids` is independent of mapping, which is positional.
- `inplace` and `simd` are not implemented keywords. Views and little-endian
  defaults are implemented in source but their generation/build/tests remain
  deferred for the current implementation step; do not claim qualification.

For a new schema, a minimal explicit-ID example is:

```text
serializer version 1;

class person stable_ids {
  public string name (1);
  public uint32 age (2);
}
```

Adding `public string country;` is rejected; `public string country (3);` assigns
an unused ID. Existing explicit IDs work even without `stable_ids`.

## Integrate generated headers

Use the consuming project's existing dependency mechanism. When using CMake,
`add_subdirectory` and installed `find_package(Serializer CONFIG REQUIRED)` expose
`Serializer::serializer_lib`, `Serializer::serializer`, and `serializer_generate`.
Use `serializer_generate(TARGET app SCHEMAS schemas/person.serializer)` after creating the
consumer. It supplies generation dependencies, the generated include directory,
runtime linkage, and the C++20 minimum. CMake 3.28+ is required; keep a newer C++
mode if the application already uses one. Source dependencies build the generator
as needed; installed packages use their installed executable. `SERIALIZER_INSTALL`
controls installation and defaults off when Serializer is embedded.

For this repository's own build, run `make all` / `make test` on Linux or
`./make.ps1 all` / `./make.ps1 test` on Windows. Both wrappers default to Release,
build all enabled CMake targets, and use the `VCPKG_ROOT` toolchain when set.
The test target builds before running CTest. They require the same compiler,
GoogleTest, and clang-format dependencies as direct CMake; Java, benchmarks, and
fuzzers remain opt-in. See [wrapper options](../../../docs/cmake_integration.md#build-this-repository)
for configurations, separate build directories, and CMake overrides.

The repository's Windows presets explicitly use Ninja and x64 architecture with
`strategy: external`. Visual Studio supplies the compiler environment; command-line
preset builds require an x64 developer environment and Ninja on `PATH`, plus
`VCPKG_ROOT`. Do not pass `-A x64` to Ninja or change the architecture strategy to
`set`. Clear an older CMake cache before reconfiguring after a generator/platform
change. See [Visual Studio folder builds](../../../docs/cmake_integration.md#visual-studio-folder-builds).

If the default build reports missing clang-format, install a host clang-format
19+ and rerun configuration; vcpkg's GoogleTest dependency does not supply it.
On Ubuntu/Debian with the package available, use `sudo apt install clang-format-19`.
For other installation paths, pass `-DSERIALIZER_CLANG_FORMAT_EXECUTABLE=...`
through `CMAKE_ARGS` or PowerShell's `-CMakeArgs`. Do not disable tests merely to
hide a missing formatter when a full build is requested. See
[formatter setup](../../../docs/cmake_integration.md#formatter-setup).

For dependency or vcpkg CI checks, review the complete
[build requirements](../../../docs/cmake_integration.md#build-this-repository)
and the actual port's options and host dependencies. Package builds should disable
all development targets; they then need no GoogleTest, formatter, JDK, Protobuf
runtime, or sanitizers. Consumer schema generation has separate host-tool needs,
especially when cross-compiling. See
[vcpkg package builds](../../../docs/cmake_integration.md#vcpkg-package-builds)
for configuration, installation checks, and Windows static linkage. Do not infer
other platforms' verification from a successful local package build.

Use a revision with the explicit stream-offset return type, warning-clean pointer
accessors, and native-width identifier hash seed for GCC, Clang, and 32-bit MSVC
builds. Regenerate LLVM-profile headers after the storage-donor naming fix; only
generated local names change, with public APIs and wire data preserved.
Updated runtime headers support `binary_none` encoder `serialize_out(view)`
dispatch for mapped views; keyed/JSON protocols remain unsupported for views.
The default C++ CTest targets have passed on Linux x64 (GCC and Clang) and Windows
x64 (MSVC), with separate Windows x86 package/consumer checks. Native macOS,
Android, and ARM Linux verification remains pending; see the README for versions.

Schema scanning and runtime codecs enable SIMD by default through `SERIALIZER_ENABLE_SIMD`.
Supported x64 builds use SSE2 and select isolated AVX2 backends after CPU/OS checks;
short spans and other architectures retain scalar processing. Set this CMake option
to `OFF` before adding the source dependency when explicit SIMD must be disabled.
Installed generators/libraries retain their build-time choice. Runtime helpers
accelerate compact/formatted JSON strings and C++ fixed-width binary numeric arrays
on input and output in every key mode; matching-endian arrays and binary views use bulk copies.
Native JSON and ProtoJSON also scan long whitespace runs with bounded SSE2/AVX2
helpers, retaining scalar short gaps/tails and exactly space, tab, LF, and CR.
Input/work budgets and ProtoJSON message bounds constrain scanning; consumed bytes
are charged once per run with existing failure positions. Rebuild the runtime
library and consumers with matching headers; no schema regeneration is needed.
See [JSON whitespace scanning](../../../docs/runtime_simd.md#simd-json-whitespace-scanning).
Bulk array input validates the complete payload before destination changes and
retains allocation, input, nesting, collection, and work limits. Work exhaustion
uses the scalar path to preserve partial results and failure positions. Boolean
and enum arrays retain per-element validation. Input must not overlap decoded storage.
Keep scalar handling for single values, variable-length values, and view setters.
Link `Serializer::serializer_lib` for pre-generated headers too. Disabling SIMD
retains bulk array reads/writes and direct JSON escaping. See
[runtime SIMD](../../../docs/runtime_simd.md) for scope, aliasing, and limitations.
Wire bytes and schema syntax stay unchanged; do not add a `simd` keyword or promise
a measured speedup. SIMD builds, boundary tests, and benchmarks remain deferred.

Call the helper once per target with all its schemas. For a shared generated API,
use an interface library and link consumers to it so one target owns generation.
Keep outputs in the build tree. Add custom format files selected indirectly by
configs to `DEPENDS`, or use `FORMAT_FILE` to select and track them. Cross builds
need a host-runnable executable passed through `GENERATOR`. Do not hand-edit
generated headers. Consult the CMake guide for options and public-header installation.

For IntelliSense include errors, check both the real generated header's existence
and the source target's include directories. In VS Code, use CMake Tools as the
C/C++ configuration provider; different profile headers share filenames and must
retain per-target include paths. A normal build generates headers automatically.
For generation without compiling consumers, build `<target>_serializer_headers`
or the aggregate `serializer_generated_headers` target. Configuration alone does
not create headers. No custom VS Code task or Serializer editor extension is
required, and `.vscode/*` remains ignored. Editor provider settings may be user-level.
The optional Rohit Serializer extension (`rohitjairajsingh.serializer-language`)
in `editors/vscode` highlights `.serializer`, offers
32×32 language icons for Explorer/editor tabs where the file icon theme permits them,
versioned snippets, and invokes the same targets through CMake Tools. Run the root
`install_extension.ps1` with Node.js 22+, npm, and the VS Code CLI to build and
install it; `-SkipBuild` installs an existing VSIX. This installs the editor
extension only; application dependencies remain managed by the consumer. Extension
version 1.1.1 is independent of compiler and schema versions. Configure
the consumer first, then use `Serializer: Generate Headers` or `Serializer:
Diagnose Missing Header`. Set `serializer.headersTarget` for one consumer; keep
profile include paths separate. Its IntelliSense command explicitly updates the
selected folder's C/C++ provider. Generation saves dirty schema/INI/CMake inputs
in that folder and requires workspace trust. Declaration/definition navigation
and `Serializer: Open Generated Header` only read available files; never invoke
generation or configuration to satisfy navigation, or offer generation for a
missing destination. Declaration opens the originating schema, while definition
opens existing generated C++ output. Follow schema includes to their actual entry
header and use existing `<header>.d` dependencies when present; expose ambiguous
legacy basename/profile matches as choices. An already active CMake model can
narrow lookup but navigation must also work without it and in Restricted Mode.
C++ type references use the installed C++ definition provider. Use `Serializer:
Go to Schema Declaration` when other providers add C++ declaration locations.
For file-level navigation, right-click a `.serializer` file in Explorer or its
editor tab and choose `Serializer: Go to Implementation`. It uses the clicked
file's URI and the same available-output lookup without building or prompting.
See [navigation and limits](../../../docs/editor_extension.md#navigate-available-schemas-and-headers).
Semantic schema diagnostics remain unavailable.
For Visual Studio 2022/2026 on Windows x64, use `editors/visual_studio` instead.
Run its `build.ps1` with Visual Studio's MSBuild to package and validate the VSIX,
then install it using Visual Studio's VSIX Installer. It shares the canonical
grammar and editing configuration, without the VS Code CMake commands or snippets.
Continue to use the consumer's CMake targets for generation and include paths.
Native Visual Studio installation and interactive editing checks remain pending.
These targets still build the generator when needed and perform real generation:
honor any instruction to defer configuration, generation, or builds. Install/package
consumption has been smoke-tested on Windows for compiler 0.1.0, including versioned
package discovery and both generation helpers. The extension's Windows VS Code
host smoke test covers header generation, opening, and build failure handling;
live C/C++ IntelliSense reparsing and other editor platforms remain unverified.

Keep output settings in a generator INI config, with `[output] language = cpp`
and a `[cpp]` section. Java uses its own section as described below. C++ `coding_standard` values
are `serializer`, `core`, `google`, `llvm`, `gnu`, `cert`, `misra`, `autosar`, and
`qt`. These are presentation profiles, not whole-guide compliance guarantees.
See the [profile examples](../../../example/coding_styles/README.md).

Default `naming = profile` renames schema-derived C++ types, fields, enum values,
union helpers, and view accessors. Wire names/IDs and runtime-required hooks keep
their original spelling. Resolve generated naming collisions; do not silently
change wire names to fix a C++ naming problem. Enum defaults referring to declared
values are translated; opaque C++ default expressions require `naming = preserve`
or an explicitly chosen literal/default change.

Formatting requires clang-format 19+ at generation time. Repository test/example
builds detect a suitable host tool through CMake's program search and, on Windows,
standard LLVM and all Visual Studio installations reported by `vswhere`. Discovery
runs again for a fresh build cache; installing LLVM or Visual Studio's C++ Clang
tools is still required if no suitable formatter exists. For reproducibility, pin its version and pass
`--cpp.clang_format` or set `SERIALIZER_CLANG_FORMAT_EXECUTABLE` for this repository's
CMake rules; the shipped helper also accepts `CLANG_FORMAT`. `format_file` can
replace layout rules. Config paths are relative to
the config; CLI paths are relative to the working directory. CLI overrides take
precedence over config values. Add configs and custom format files to generation
dependencies. Use `format = false` only when another pipeline formats the emitted
source. The test build includes all profile examples; with tests disabled,
`SERIALIZER_BUILD_STYLE_EXAMPLES=ON` enables them independently.

The CLI uses short/long options (`-i`/`--input`, `-o`/`--output`, `-c`/`--config`,
`-l`/`--language`); bare argument names are no longer supported:

```sh
serializer --input schemas/person.serializer --output person.hpp
```

Repeat `--language cpp --language java` or use `--language cpp,java` to generate
both backends. Supply `--cpp.output person.hpp --java.output PersonSchema.java`
instead of `--output` for multiple languages. Prefix backend overrides with `--`,
such as `--cpp.coding_standard google --java.coding_standard oracle` and
`--java.package app.models`. All backend config settings have CLI overrides.
`--java.package=` and `--cpp.format_file=` clear configured optional values.

Use the actual built executable path when it is not on `PATH`. Regenerate through
the real build pipeline when execution is part of the task; preserve an explicit
user instruction to defer generation/builds/tests and report what remains unverified.

## Integrate pure Java output

- Select `[output] language = java`, with `[java] coding_standard = serializer`,
  `google`, or `oracle`. All use conventional Java type/field/enum naming; Oracle
  uses four-space indentation and the other two use two spaces. They are limited
  presentation profiles, not full compliance tools. `naming = preserve` keeps
  valid schema identifiers. `package` optionally selects a Java package.
- Run `serializer --input account.serializer --output AccountSchema.java --config java.ini`.
  The output filename supplies the public outer class. Schema namespaces become
  static nested namespace containers. Each generated file includes its own codec
  helpers and needs only Java 17+, with no JNI, reflection, or third-party runtime.
- Compile with `javac --release 17 -encoding UTF-8 -d classes AccountSchema.java`.
  For CMake source/installed dependencies, `serializer_generate_java(TARGET name
  SCHEMA file OUTPUT Schema.java CONFIG java.ini)` creates a generation target;
  `serializer_generated_java` builds all registered Java outputs. Attach Java
  compilation in the consumer. Track custom dependencies with `DEPENDS` and use
  a host `GENERATOR` for cross builds. Java examples are opt-in through
  `SERIALIZER_BUILD_JAVA_EXAMPLES`; every example has its own input folder.
- Generate owning schemas only. Reject view/packed requests rather than promising
  Java buffer views. Parents use `base0`, `base1`, ... composition, not subtyping.
  Arrays use Java arrays; maps use ordered maps and box primitive keys/values.
  Unsigned scalars retain their bits in Java signed primitives. Preserve generated
  map ordering. Object/floating-point keys and opaque C++ defaults are unsupported.
- Call `value.encode(Schema.Protocol.BINARY_INTEGER)` and
  `Schema.Namespace.Type.decode(bytes, protocol[, limits])`. All four protocols
  are implemented; decode validates an exact message before returning a fresh
  object. JSON uses UTF-8, and binary scalars are little-endian. Java strings
  reject invalid UTF-8 byte sequences; no arbitrary byte-string compatibility is
  promised. See the Java guide before mapping unusual defaults or union payloads.
- Use the generated `Limits` to bound message bytes, string bytes, cumulative
  collection entries, and nesting. Unknown fields and malformed values fail with
  `IllegalArgumentException`. Do not share mutable input/object storage across
  concurrent codec calls. No explicit SIMD or native acceleration is implemented.
- Keep Java runtime/build verification distinct from C++ source-only status notes.
  With tests and Java examples enabled, CTest exercises compiled Java and two-way
  interoperability, including exact binary bytes. Do not claim benchmark results.

## Implement the C++ codec calls

- For Protobuf binary, ProtoJSON, or TextProto, read
  [the Protobuf guide](../../../docs/protobuf.md). Enable `[cpp] protobuf = true`
  or `--cpp.protobuf true`, regenerate, and select `protobuf_binary`, `protojson`,
  or `textproto` with the existing compile-time protocol-template calls. No
  external Protobuf runtime or `.proto` export is required. These codecs currently
  support C++ owning objects only; Java retains its four established protocols.
  Match the documented schema mapping with the peer, including union wrapper
  messages, field-number limits, map keys, and the absence of a `bytes` schema type.
  Input uses Protobuf defaults and commits a replacement object only after exact
  decoding succeeds. Unknown binary fields are skipped and discarded; unknown
  named fields/enums fail. Do not promise preservation of field/union presence,
  unknown fields, well-known-type mappings, or arbitrary Protobuf schemas.

- Select `json`, `binary_none`, `binary_integer`, or `binary_string` according to
  the agreed message contract. Do not silently change protocols to improve size
  or speed. `format::compress` means JSON whitespace suppression.
- Binary fixed-width scalars default to little-endian. Inspect `Protocol::wire_endian`;
  an explicit third `binary` template argument selects another byte order when
  required. Compact prefixes keep their own encoding. No endian/protocol marker,
  auto-detection, or compatibility aliases are provided; agree on the format with
  the peer. JSON is unaffected by numeric byte order.
- Regenerate C++ headers for structural stream concepts and implicit standard-stream
  adapters. No `rohit::stream` inheritance is required: use `input_buffer` or
  `output_buffer` for the contiguous fast path, or standard-style byte-stream
  concepts for external I/O. Generated calls accept standard streams directly.
  Memory input (`istringstream`/`stringstream`) borrows the unread suffix; known
  file streams use 64 KiB batches and erased/custom byte streams use 8 KiB batches.
  Standard memory output uses generic buffered writes. No measured speedup is claimed.
- For complete stream-specific consumers, use the seven
  [iostream example folders](../../../example/iostream/README.md). They share a
  52-class, 645-field schema and verify large messages through memory, file,
  explicitly buffered, erased, and custom non-seekable streams. Enable
  `SERIALIZER_BUILD_IOSTREAM_EXAMPLES=ON` to build them independently without
  GoogleTest, then run CTest with `-L serializer_iostream`. The standard test build
  includes them too. Keep their concrete stream types where specialization matters.
- Regenerate owning C++ classes to use matching static entrypoints:
  `Type::serialize<Protocol>(stream, value)` writes a const borrowed value and
  returns void; `Type::deserialize<Protocol>(stream[, limits])` returns a newly
  initialized owning value marked `[[nodiscard]]`. Defaults and stream adaptation
  match the existing member APIs. Failures throw without returning a partial
  object, but may consume input. Keep `serialize_in` for destination storage reuse.
  The static factory retains buffer-input semantics without an implicit `finish()`;
  byte streams still receive exact-message validation. Views retain `map`.
  Both new names are reserved in owning class scope; preserve wire names and IDs
  with metadata when resolving schema member collisions.
- Byte-stream input means one bounded EOF-delimited message and includes `finish()`
  validation; it is not incremental parsing and adds no wire framing. Use a bounded
  source or exact-size buffer for framed traffic. Regenerated owning classes expose
  `destination.serialize_in<Protocol>(input, limits)` for explicit limits; the
  one-argument overload retains default behavior. The free
  `serialize_from<Protocol>(input, destination, limits)` remains an alternative.
  Low-level codec templates accept a concrete buffer type; existing buffer calls
  remain valid. Keep memory-stream
  storage stable while decoding. Never imply that concepts prove lifetime or
  alias safety. See [stream contracts](../../../docs/usage.md#stream-concepts-and-implicit-adapters).
- Open binary files in binary mode. Adapters borrow streams, retain exception masks,
  and leave explicit flushing/closing to the caller. I/O failures can consume input
  or write an output prefix; decode errors may partially update destinations.
  Encoded staging storage is separate from decoder allocation accounting. Custom
  protocols retain their constructors unless they explicitly provide compatible
  `stream_type` and `rebind_stream` hooks; inherited hooks do not replace derived
  protocol behavior. Schema parser/writer entrypoints use the same stream concepts.
- Encode into an appropriate stream. Reuse `full_stream_auto_alloc` capacity when
  useful, resetting only after readers of the previous message have finished.
- Construct input views with the actual message length, such as
  `make_constant_full_stream(output.begin(), output.current_offset())`, never the
  spare allocation capacity. Keep input storage alive and independent of output
  destinations or any mutation that could invalidate it.
- For explicit limits and whole-message validation, construct the input protocol
  with `(input, decode_limits)`, call `decoder.serialize_in(destination)`, and then
  `decoder.finish()`. Convenience object input calls omit the final exact-message
  check for contiguous buffers; byte streams always receive that check.
  Share decoder sessions by reference; they are noncopyable.
- Choose limits from the application's message sizes and workload. Storage/work
  accounting is cumulative per decoder and is not an exact process-memory limit.
- Strings/vectors/maps replace contents. Missing keyed fields retain destination
  values; duplicate fields apply in order and maps keep the last complete duplicate
  entry. Decode failure can leave partial updates and consumed input.
- Reuse an owning destination across messages when useful: native C++ JSON and all
  three binary key modes recycle eligible nested buffers and map nodes automatically.
  Regenerate owning headers for typed storage donation inside generated collection
  elements and parents; no schema option or explicit donor argument is needed.
  Incoming elements still start from fresh schema defaults, including missing
  keyed fields. Shorter/empty collections discard removed elements; custom or
  throwing-assignment types retain fresh decoding. Reused storage remains subject
  to logical resource accounting and may increase temporary retained memory.
  Java and Protobuf codecs keep their existing replacement paths. Consult
  [destination reuse](../../../docs/usage.md#reuse-destination-storage) for scope
  and limitations; generation, builds, focused test execution, and performance
  measurements for this optimization remain deferred.
- When the application requires atomic replacement, decode/finish a temporary
  object before committing it. Keep structured parse error handling at the message
  boundary and leave payload excerpts disabled unless the task needs them.
- Regenerate owning headers for automatic fixed-width field batching: native
  binary output groups 2 through 16 consecutive scalar fields per reservation;
  positional binary input shares range/budget checks and falls back to scalar
  reads on a failed check or invalid Boolean. Keyed input keeps individual
  dispatch. Fields retain their wire order, IDs/names, endian, and work charges.
  Variable-length fields, enums, unions, parents, and objects bound runs; nested
  serializers group their own fields. JSON/custom protocols without batch hooks,
  Java, and Protobuf retain existing paths. No schema option or SIMD setting is
  required. Failed output reservations leave the current batch unwritten, with
  previous output intact. See [field batching](../../../docs/usage.md#batch-generated-fixed-width-fields)
  for custom-stream policies and prepared tests. Generation/build/test execution
  and performance measurements remain deferred.

- Regenerate owning headers for pre-encoded constant field names in native C++
  JSON and string-key binary output, including fixed-width batches. Schema wire
  spellings and bytes stay unchanged; JSON formatting and dynamic-string validation
  remain active. No schema option is needed. Custom protocols without the static
  `encoded_field_name<Name>()` hook receive ordinary `std::string_view` names.
  Constant arrays add compiler work/read-only data, not per-object storage; no
  measured speedup is claimed. Isolated binary names reserve their length and text
  together, leaving both unwritten on rejection; values may fail separately.
  Input, views, Java, and Protobuf keep their existing paths. See
  [constant field names](../../../docs/usage.md#pre-encode-constant-field-names)
  for details and prepared tests; generation/build/test execution and benchmarks
  remain deferred.

- Use the updated runtime headers for JSON string scan reuse; schema regeneration
  and new options are unnecessary. Validated plain strings copy directly, while
  escaped input/output reuse boundaries to skip rescanning plain prefixes/suffixes.
  Full validation, resource charges, destination reuse, and stream alias/failure
  behavior remain unchanged. Input still must not overlap decoded storage. Shared
  ProtoJSON input and ProtoJSON/TextProto quoted output benefit; separate parsers
  and other codecs retain their existing algorithms. See
  [JSON scan reuse](../../../docs/usage.md#reduce-repeated-json-scans) for remaining
  passes and prepared coverage. Builds/tests/sanitizers/benchmarks remain deferred;
  do not claim measured performance gains.

## Map generated views

Use `View::map(span, limits)` with the exact little-endian `binary_none` message.
Mapping validates the full message and builds inline field offsets. Getters return
scalars, borrowed strings, or nested views. Mutable mappings require writable
spans; read-only mappings never expose setters. Buffer owners must outlive every
view and must not relocate or change the layout while views remain live.

Use setters only for existing scalar/string fields or collection elements with
unchanged encoded sizes. Do not promise insertion, resizing, union switching,
or JSON/keyed/big-endian mapping. Map keys remain read-only; map entries retain
wire order and duplicates. Prefer range-based loops or `begin()`/`end()` for sequential
array/map access: iterators cache entry boundaries, while variable-width indexed
access traverses preceding entries each time. Arrays yield values or nested views;
maps yield key/value pairs. Use iterator `set(value)` for mutable scalar/string array
elements and `set_value(value)` for mutable map values; dereference returns a value,
not a writable scalar reference. Nested object values expose their existing setters.
Copies advance independently and retain their own span/limits, so temporary collection
wrappers are safe while the underlying buffer remains alive and unchanged in layout.
Nested field access still constructs a validated view. Refer to the view guide for
parent accessors, union access, and complete examples.

## Verify and document the result

Use the task's permitted validation scope. For an integration, exercise a generated
record through the selected protocol with an exact-size input, destination reuse,
and relevant malformed/limited input. For schema migration, include the relevant
old/new schema behavior. Consult [qualification](../../../qualification/README.md)
for library-wide tests, fuzzing, or benchmarks only when that work is in scope.

For fuzz qualification, enable `SERIALIZER_BUILD_FUZZERS=ON` with a Clang GNU-style
driver providing libFuzzer, ASan, and UBSan. It builds an isolated instrumented
runtime, including SIMD translation units, without changing the normal library or
generator. Build the four fuzz executables and `fuzz_corpus_generator`, then run
`ctest --test-dir <build> -L serializer_fuzz --output-on-failure` to generate and
replay deterministic seeds. Follow [the fuzzing workflow](../../../qualification/README.md#fuzzing)
for control-byte encoding, separate mutation/artifact directories, fixed seeds,
and SIMD ON/OFF runs. Seed replay is not a mutation campaign or a security claim.

Report files changed, how to generate/build/use the result, and what was actually
validated. When editing Serializer itself, keep its README and this skill current
together with affected linked guides. When editing a consumer, update that
application's relevant usage docs without modifying an upstream checkout merely
to restate an unchanged library contract. Keep public examples specific to
Serializer and public sources.
