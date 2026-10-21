# Migrating to the snake_case Serializer API

## Compiler/runtime 2.22.0

Activate version-one C++ feature profiles through
`model_store::configure_capabilities`; requirement inspection alone does not
activate exclusions or mandatory authorization. Attached engines and mandatory
identity/authorization cannot be silently removed. For native history-policy
conversion, use `migration::convert_history_policy` with explicit branch selection
and permission for history loss. Payload identities/bytes, allocation high-water
marks and lease wrappers retain their contracts; see the
[executed profile conversions](docs/verification/verification-managed-capabilities-2026-10-21.md).

The new [Pack-IRI API](docs/serialization/pack_iri.md) addresses outer packages
without changing internal part/relationship resolution. Supply scheme-specific
outer equivalence in the application. It performs no file or network access.
These additions do not change schema language 1.21.0 or existing native formats.

Regenerate affected XML codecs for corrected namespace admission and cancellation
checks around prepared validators. C++ namespace limits count physical declarations;
inherited map copies retain separate tree/allocation admission. C output also
counts ordinary attributes, generated declarations and namespace URI values,
and omits redundant declarations of the predefined `xml` namespace.
Go accounts expanded attribute names and binding prefix/URI bytes consistently;
qualified attributes now use writer-owned aliases with the existing namespace
basename preference and inherited reuse, preserving their expanded identities.
Python scope copies retain work/allocation charges without duplicating
the logical XML text count. Applications must compare QName identities rather
than depend on a provider-selected prefix spelling. New checks can reject output
that previously escaped a tight resource or canceled-operation policy.

C#/Kotlin/Swift standalone XML callers can now attach cancellation or a host-clock
deadline through `Limits.WithCancellation` / `XmlLimits.withCancellation`.
The returned policy copy retains callback state and the existing ledger; use a
fresh wrapper per operation and after failure, without concurrent sharing.
Swift retains delegate failures and throws after the byte-admitted synchronous
FoundationXML call returns, avoiding unsafe parser-abort reentry. Existing
constructors and native wire representations remain unchanged.

Regenerate Python package facades for Save publication-hook checks. A hook that
arms cancellation and returns now triggers the caller's policy immediately.
Before replacement, the old file remains unchanged. After replacement,
`publication_uncertain` retains the cancellation cause and the complete new
archive; reopen that generation before deciding whether to retry. Throwing hook
exceptions retain their existing precedence.

## Compiler/runtime 2.21.0

The optional [C++ OPC core-property adapter](docs/serialization/core_properties.md)
supplies package metadata from application values. Complete-candidate managed
authorization covers inherited entities, owner-local edits and replacement/import
publication; bind the principal and policy administration in the host.
The routed PMR resource caps only allocations that pass through it, without
replacing generated payload/decoder containers. Python QName-list restrictions
retain schema-scope expanded enumeration/default/fixed identities in the supported
graphs. Regenerate affected output; native field IDs, wire bytes and schema
language 1.21.0 stay compatible.

## Compiler/runtime 2.20.1

Regenerate Java/C#/Kotlin/Swift XML output to count generated namespace
declarations, qualified attributes, default resets and type declarations against
the selected structural/text policy. Tight bounds can reject output earlier;
existing native APIs, IDs, payload bytes and XML expanded identities remain intact.
See the [original namespace-boundary checks](docs/verification/verification-namespace-budgets-2026-10-21.md).

## Compiler/runtime 2.20.0

Existing schema language 1.21.0, native APIs, enum values and saved formats remain
unchanged. Native-enum XSD export/string-space binding now passes its supported
Windows/Linux qualification. Its bounded profile uses `xs:string` with empty/preserve whitespace.
Preserve declared enum spellings and native values when rebinding;
default/fixed restrictions must select an admitted native enumerator and the
restriction must admit at least one native value. Invalid bindings reject before
generated output changes. An exported
XML restriction does not renumber enum values or migrate native data. Unsupported
token/collapsing/normalized spaces, list-lexical enum collections and
numeric/QName/list enum spaces still reject.

Directional XML comparison now accepts sequence-to-`all` widening independently
of the `all` declaration order. The reverse direction and reordered sequences
still reject where their accepted documents differ. Both contracts must bind;
association/capability preflights are separate from compatibility differences.
Export/rebind does not infer a renamed application field or merge an import.
See the [tooling inventory](qualification/xsd_tooling/README.md).

Use the rebuilt compiler for UTF-8 schema locations and
[Windows UTF-8 CLI paths](docs/serialization/xsd_tooling.md#windows-utf-8-cli-paths).
Its process manifest avoids requiring a system-locale change on qualified modern
Windows; it does not change the text/path configuration of consuming applications.

## Compiler/runtime 2.19.2

No compiler-driven schema, native API or saved-format migration is required.
This patch qualifies the existing [XSD evolution workflows](qualification/xsd_evolution/README.md).
Import changed XSDs into fresh destinations for review: surviving expanded XML
identities retain imported IDs, while renamed identities receive different native
names/IDs. Stable IDs do not preserve positional layouts after reordered or added
fields. For an authored application model, keep native names/IDs/order and use
inline XML exceptions to change XML names; defaults and behavior remain native.
An XML rename still changes the XML contract. Select its matching schema or
perform an application migration instead of assuming old XML will decode.

## Compiler/runtime 2.19.1

No schema, model/API or saved-format migration is required. This patch qualifies
existing package reproducibility policies rather than changing default wire bytes.
Use the [standalone inventory](qualification/package_reproducibility/README.md)
when verifying input permutations, independent processes, ZIP64 metadata and
provider-qualified compressed equality. Retain exact payloads/names/IDs and the
selected backend policy. Python's generic ZIP probe remains diagnostic/internal;
its public schema-bound OPC facade is unchanged. The selected C# matrix targets
.NET 8; it does not qualify later runtime/provider combinations.

## Compiler/runtime 2.19.0

Regenerate affected package output and rebuild C++ consumers against matching
headers/runtime. Schema language remains 1.21.0; native APIs, wire identities and
payload revisions retain their contracts. New C canonical encode/write/Save and
qualified Rust `CanonicalPackage` methods are opt-in. Go selects canonical fixed
role paths with additive canonical Load functions; its existing Load defaults and
public option/value layouts are unchanged. Use collision-selected helper names
reported by generated source.

For the remaining generated facades, keep the loaded extensions attached to the
edited input. Java, JS/TS, Kotlin, C# and Swift retain private moved-role origins;
constructing new extensions only from their public part/link arrays loses that
provenance. Swift now retains the original public preserved relationship context
until composition rather than eagerly rewriting it at load. Save resolves each
internal target against its original source before applying simultaneous moves.
External URI references, raw query/fragment suffixes and untouched target spelling
retain their values. C/Rust also accept inert relative external URI references;
relationship type URIs still require the existing absolute URI contract.

Only actually moved fixed singleton roles acquire provenance. C++/Python no longer
rewrite preserved target spelling for an unchanged role. Removing a moved role
cannot silently redirect its old links to another role that now occupies its path;
such links reject before publication. Remove or edit those links explicitly.
Dynamic role renames, arbitrary path edits, lexical XML and signature preservation
remain separate requirements. See the
[preservation contract](docs/serialization/xml_packages.md#fixed-singleton-path-canonicalization).

## Compiler/runtime 2.18.0

Regenerate affected C/Rust XML/package output to use shared native operation
admission and supported XSD identity package graphs. Old native models, limit
aggregates and entry points remain valid. New C descriptors and qualified Rust
operation traits are additive; preserve the generated collision-selected helper
names instead of hardcoding a default when a schema claims that name.

Go selected-codec functions receive existing public `XmlOptions` already bound to
the package ledger. JS/TS `options.codecs` uses schema role names and synchronous
typed functions with bridged `XmlLimits`. Kotlin exposes `xmlLimits(CodecContext)`
for Composer/Loader callbacks in a separate consuming module. Use the supplied
context when invoking native codecs and keep it alive only for the synchronous
call. Existing package methods select generated native codecs automatically.
Known generated default allocations are admitted before construction; opaque
factory/provider internal transient work and allocation remain outside hard peak
or preemption claims. Schema language, wire IDs and saved formats do not change.

## Compiler/runtime 2.17.1

Rebuild C++ applications against matching runtime headers. A fresh local
collaboration client can now join a valid authority baseline containing typed
managed references: validation uses the staged authority namespace while retaining
the original client application policy. Foreign references and policy failures
still reject before publishing the incoming model. Existing schema language
1.21.0, native model APIs, identities, journals and client checkpoints require no
migration. See [local synchronization](docs/managed/local_collaboration.md).

## Compiler/runtime 2.17.0

Regenerate affected package/XML output and rebuild C++ applications against matching
headers/runtime. Schema language **1.21.0** adds package policies and direct scalar
key/reference selectors. Older compatible headers retain their meaning and the
integer `version 1;` alias remains exactly 1.0.0. Native IDs, scalar encodings and
managed storage formats do not change.

Follow the [generated package contract](docs/serialization/package_application_contracts.md).
New syntax requires language 1.21.0 in its declaring file, including includes.
Fixed profiles reject mismatched physical storage; older profiles retain overridable
defaults. Domain callbacks remain available after generated reference validation.

## Compiler/runtime 2.16.0

Regenerate XML output and rebuild C++ applications with matching headers/runtime.
Schema language **1.20.0**, native wire values and managed storage formats are
unchanged. C++/Python namespace-dependent scalar children retain their own XML
scope, including child-local/shadowed prefixes and default namespace resets.
Optional/repeated QName/list and nested-union values retain expanded identities.
Keep XML-only scope metadata associated with its field/occurrence when editing
values; reordering identical lexical strings with different namespace meanings
requires corresponding metadata association.

Go/Java/C# add plain QName, QName-list and prefix-list codecs. Go
`SetXMLNamespaces`, Java `setXmlNamespaces` and C# `SetXmlNamespaces` copy
validated authored bindings and replace captured scopes on that record; nested
owning records bind independently. Namespace-dependent union/default/fixed/enumeration
combinations still fail during generation. Unicode XML mapping names/prefixes
now use the complete UTF-8 XML NCName contract, including provider-free validation.
See [executed checks and remaining gates](docs/verification/verification-qname-scopes-2026-10-20.md).

Kotlin/Swift owning codecs retain plain QName/list/prefix-list scopes. C/Rust use
their XML document companions to retain namespace state; use document decode and
encode-with-extensions APIs and respect the remaining graph capability gates.

Explicit language 1.14+ canonical unsuffixed decimal floating defaults now reject
overflow at the declared width, and enum defaults explicitly naming their declared
type reject unknown members. Original declaration locations survive included and
generic source. Underflow, numeric Boolean defaults, native expressions and legacy
spelling retain their existing handling.

C++/Python OPC edges and supported XSD key/unique/keyref tuples now share the
package reference counter. Existing limits may need to admit these previously
uncharged references; failed/repeated operations remain cumulative. Active
generated fields/constraints add diagnostic locations; deferred presentation and
provider work still have documented attribution/classification gaps. The original context overload and legacy
callback resource vocabulary remain supported. Managed runtime/storage formats
are unchanged; this batch adds eight-profile fault/lifecycle qualification. See
the [batch record](docs/verification/verification-pending-batch-2026-10-20.md).

Regenerated C# and Kotlin/JVM now include physical ZIP helpers and typed schema
OPC facades. Ordinary XML-disabled models retain provider independence; package
declarations require the existing XML capability. Use the collision-selected
helper discovery comment, one shared operation budget, explicit private spooling
and complete-aggregate validation. Owning byte-array results have platform array
limits. See the [API and staged Save contract](docs/languages/csharp_kotlin_packages.md)
and [acceptance record](docs/verification/verification-pending-50-2026-10-20.md).
Both editor packages are **2.0.9** and recognize C#/Kotlin package facades while
excluding marked compiler runtime declarations from schema navigation.

## Compiler/runtime 2.15.0

Regenerate C++/Python XML/package output and rebuild C++ consumers with matching
runtime headers and libraries. The schema language stays **1.20.0**; native
payloads, managed saves and protocol IDs require no migration.

C++ child unions can select QName/QName-list branches when the child and owning
record scopes retain the same selected member and expanded names. Child-local
or shadowed bindings that would lose identity still reject. Python accepts outer
enumeration/fixed restrictions on namespace-independent unions, comparing values
through the first matching member's primitive value space. Namespace-dependent
outer union restrictions and QName-list enumeration/fixed contracts remain gated.

Generated C++/Python package loads retain discovered fixed singleton role names in
`preserved_extensions::relocations` / `PreservedExtensions.relocations`. Pass the
loaded extensions to the generated input for Save; incoming and outgoing retained
relationships follow the authored canonical names automatically. Internal relative
targets resolve in their original source scope before rebinding, preserving query
and fragment suffixes. External URI strings and opaque payload bytes remain intact.
Manually created extension objects retain their existing behavior with an empty
relocation list. Other package backends retain their documented explicit rebinding
requirement; dynamic role renames are separate application edits.

For explicitly versioned language 1.14+ defaults, out-of-range unsuffixed decimal
and hexadecimal integer literals fail during compilation with the member's original
path/line/column. Such literals previously bypassed early portable-range validation.
Valid native expressions, suffixed/octal literals, Boolean numeric defaults and
legacy/headerless defaults retain their handling. Aggregate/generic default errors
also include their declaration location. See the
[verification record](docs/verification/verification-pending-xml-2026-10-20.md).

## Compiler/runtime 2.14.0

External-document journal recovery now rejects a foreign companion generation or
damaged/truncated first baseline frame before calling the application decoder.
Successful Save/reopen, existing callback signatures and native side-channel
formats remain compatible. Keep the main file, manifest and selected companion
together; use the [runnable JSON example](example/managed/external_document/README.md)
and [Windows/Linux qualification](docs/verification/verification-managed-document-bridge-2026-10-20.md).

Generation-only CMake consumers can now use
`find_package(Serializer 2.14 CONFIG REQUIRED COMPONENTS compiler)`. It imports
`Serializer::serializer` and loads the generation helpers without discovering
enabled runtime-provider packages. This supports `LANGUAGES NONE` consumers on
multiarch Linux without provider search-prefix or library-architecture workarounds.
Use `serializer_generate_source` or `serializer_generate_java`; the source helper
also supports C++ generation without runtime linkage.

Existing unversioned discovery and `COMPONENTS runtime` retain the full installed
targets and provider discovery. C++ `serializer_generate` and
`serializer_generate_variants` still need `Serializer::runtime`; request runtime
discovery before calling them. A later discovery can add runtime targets after
compiler-only discovery. No consumer migration is required unless selecting the
new component.

The compiler component skips CMake provider lookup; deploy the host executable's
own shared-library dependencies as before. In cross builds, select a host package
with `NO_CMAKE_FIND_ROOT_PATH` or explicit `Serializer_DIR`, or pass a host
`GENERATOR` override. See the [CMake guide](docs/guides/cmake_integration.md#compiler-only-source-generation).
Regenerate Python modules to obtain the additive `__serializer_compression__`
namespace and conditional `Compression` convenience alias. Existing schema types
named `Compression` retain their names. Ordinary model `encode`/`decode` remain
uncompressed; select a standard outer format explicitly through the new helpers.
Gzip/zlib/raw DEFLATE need stdlib `zlib`; optional Zstandard uses
`compression.zstd` on Python 3.14+, and optional LZ4 uses `lz4.frame`. A missing
provider fails before source I/O or model serialization. Generated source gains
support code even when compression is unused; existing model APIs and payload
bytes retain their behavior. See [Python compression](docs/serialization/compression.md#generated-python-api)
for byte/window bounds, cooperative cancellation and borrowed-stream ownership.

Regenerated Java files add a nested whole-message helper, normally named
`SerializerCompression`; an existing model with that name selects an unused
helper suffix. Identity, gzip, zlib and raw DEFLATE use the platform provider.
Optional runtime dependencies `com.github.luben:zstd-jni:1.5.7-16` and
`at.yawk.lz4:lz4-java:1.12.0` enable Zstandard and LZ4 frames respectively;
neither is needed for generation or compilation. Missing providers fail before
callbacks or I/O. Existing native codec calls remain uncompressed. See
[Java integration](docs/languages/java.md#whole-message-compression).

Regenerated Go packages add the collision-selected `SerializerCompression`
helper family. Identity, gzip, zlib and raw DEFLATE need only the Go standard
library on Go 1.22+; existing native codecs keep their direct paths and wire bytes.
Optional Zstandard/LZ4 providers are explicit per-operation adapters. The maintained
adapter pins klauspost/compress 1.20.3 and pierrec/lz4/v4 4.1.33 and requires Go
1.25+; it adds no dependency to the default generated core. Use explicit finish
for borrowed streams and pass the generated decoder to the fresh-value helper.
See [Go compression](docs/languages/go_compression.md).

C++ compression now stops source staging after a callback requests cancellation,
checks final external writes/native candidates, and fences swallowed same-writer
reentry. Cached input `finish()` retains the named cancellation hook: keep its
callable alive through the wrapper's last operation. Cancellation/reentry failure
fences subsequent finish and retains its nested cause; a premature finish check
with unread bytes remains recoverable. Existing uncompressed and disabled-hook
buffering paths stay direct. C++, generated Python, Java and Go also enforce a
valid zlib frame's smaller advertised history before provider expansion; this
check leaves normal 32 KiB zlib, gzip and raw-DEFLATE decoding paths unchanged.

Regenerated Go output also adds a separate collision-selected physical ZIP helper
family using Go 1.22 standard APIs. Existing compression names, native model APIs
and wire bytes remain unchanged. See the [Go ZIP contract](docs/languages/go_packages.md)
for shared budgets, explicit sources/spooling and platform-dependent Save.
Schema package declarations now also emit typed Go OPC facades. They enable
XML automatically and reject an explicit conflicting XML-disable request before
replacing existing output. Generic physical ZIP remains available independently.
Typed payloads use existing owning XML mappings; supply a complete-aggregate
validator for application references and keep cumulative native-codec limits
separate. The [OPC proof](docs/verification/verification-go-opc-packages-2026-10-20.md)
records actual Windows/Linux SDKs, quota regressions and publication faults.
Prepared compiler output files and depfiles with equal bytes now retain their
timestamps, including separate C++ children; changed files retain the prior write
contract. See the [publication proof](docs/verification/verification-generated-output-publication-2026-10-20.md).

Regenerated C# output adds a collision-selected nested `SerializerCompression`
helper using .NET 8's standard identity/gzip/zlib/raw DEFLATE providers. Select
actual `CompressionLevel` enum values and exact native codec callbacks; ordinary
model APIs stay uncompressed. Borrowed wrappers require explicit `Finish`.
Caller-selected `Provider.ZstdSharp(assembly)` and `Provider.Lz4(assembly)` add
optional managed formats. Use `EncodeOptions.WithProvider` and
`DecodeOptions.WithProvider`; original public constructors retain overload
resolution, including untyped `default`. See the [C# contract](docs/languages/csharp_compression.md).

Regenerated JavaScript ES modules and matching TypeScript declarations add an
asynchronous `SerializerCompression` container with collision-safe naming. Call
`await nodeProvider()` and pass that provider explicitly for gzip/zlib/raw DEFLATE;
ordinary browser imports and identity use no Node modules. Optional
`await zstdProvider()` selects public Node Zstandard transforms;
`lz4Provider(blockModule)` selects caller-loaded lz4js 0.2.0, fast level 0 only.
TypeScript preserves existing default provider literals and option-property types;
new option constructors infer the additional provider types. Await borrowed-wrapper
operations sequentially and finish explicitly. See the
[JS/TS contract](docs/languages/javascript_compression.md).

Regenerated Kotlin adds a collision-selected `SerializerCompression` object using
standard JDK identity/gzip/zlib/raw DEFLATE providers. The selected actual runtime
qualification targets JVM 17. Optional runtime jars lazily enable Zstandard and
LZ4; compilation requires neither. Existing positional options remain compatible,
with provider-specific default levels. Missing jars or failed Zstandard native
deployment reject before callbacks or I/O. See the
[Kotlin contract](docs/languages/kotlin_compression.md) and
[Windows/Linux provider proof](docs/verification/verification-kotlin-compression-providers-2026-10-20.md).

Regenerated Swift output adds standalone identity/gzip/zlib/raw-DEFLATE
compression with owning native callbacks and borrowed closure-based wrappers.
Core formats require no extra compression module or system library. Explicit
caller-owned providers add Zstandard/LZ4 through separate required `provider:`
initializers. Original initializer signatures, defaults and function-value types
remain compatible; default optional availability is unchanged. The reference
C shim and native libraries are application dependencies only when selected.
See the [Swift API](docs/languages/swift_compression.md),
[core SDK profiles](docs/verification/verification-swift-compression-2026-10-20.md) and
[Windows/Linux provider proof](docs/verification/verification-swift-compression-providers-2026-10-20.md).

Regenerated Rust adds a collision-selected `serializer_compression` module with
standalone identity/gzip/zlib/raw DEFLATE. Native APIs and wire bytes retain their
behavior. Supply owning callbacks with separate native limits; explicitly finish
the typed wrappers and pass mutable adapter references to retain ownership.
Additive `*_with_provider` operations and explicit wrapper constructors borrow
caller-owned optional Zstandard/LZ4 providers. Exhaustive option literals retain
their fields; the existing level default stays 6, so select Zstandard 3 or LZ4
fast 0 explicitly. Standard-trait nesting remains separate. The
[provider qualification](docs/verification/verification-rust-compression-providers-2026-10-20.md)
uses pinned opt-in Cargo features with no generated default dependencies.
See the [Rust API](docs/languages/rust_compression.md) and
[actual Windows/native Linux proof](docs/verification/verification-rust-compression-2026-10-20.md).

Regenerated C11 headers add a collision-selected `serializer_compression` prefix
with byte/typed owning helpers and borrowed callbacks. Identity is standalone;
define `SERIALIZER_COMPRESSION_ZLIB=1` consistently and link zlib to enable
gzip/zlib/raw DEFLATE. Existing model functions and native bytes are unchanged.
See the [C API](docs/languages/c_compression.md) and its
[Windows/Linux SDK profiles](docs/verification/verification-c-compression-2026-10-20.md).
Optional `SERIALIZER_COMPRESSION_ZSTD=1`/`SERIALIZER_COMPRESSION_LZ4=1`
definitions and native linkage enable Zstandard/LZ4 without adding dependencies
to the default C output. Select sufficient decode history explicitly; the
32 KiB default is preserved. See the
[provider contract](docs/languages/c_compression.md#optional-zstandard-and-lz4)
and [actual Windows/Linux proof](docs/verification/verification-c-compression-providers-2026-10-20.md).

Existing language **1.20.0** package declarations now generate asynchronous
JavaScript facades and matching TypeScript declarations. Use the
collision-selected `SerializerPackage` container, explicit Node providers for
DEFLATE/filesystem operations and the same pinned XML dependencies as generated
XML. Typed roles use XML; native model APIs and wire values retain their behavior.
See the [package API](docs/languages/javascript_packages.md) and
[selected independent/Save proof](docs/verification/verification-javascript-packages-2026-10-20.md)
before claiming broader codec, cumulative-context or application-reference coverage.

Regenerated Java also emits typed ZIP/OPC facades using only Java 17 standard
APIs. The collision-selected nested `SerializerPackage` helper supplies bounded
random/sequential sources, XML/opaque aggregate loading, preservation and staged
Save. Existing native APIs and bytes retain their behavior. Use explicit package
budgets and complete validation; preserved links must be rebound when known-role
writer names change. See the [Java API](docs/languages/java_packages.md) and
[actual qualification](docs/verification/verification-java-packages-2026-10-20.md)
for Java-array output, nested budget and runtime codec limits.

Regenerate Python, Java, Go and C# output to obtain the callback lifecycle repairs.
An active wrapper now rejects callback-driven close/disposal or same-wrapper
reentry before further borrowed I/O or successful completion. The active outer
operation retains its first failure; later inactive calls retain each backend's
existing lifecycle error contract. The JS/TS and Kotlin helpers enforce the same
publication boundary. See the [dated backend proofs](docs/verification/verification-backlog-2026-10-20.md).

For first initialization from already identified managed storage, use the new
`identified_baseline` tagged constructor with the exact generated storage type,
stored document namespace and native allocation watermark. It preserves IDs,
declared references and burned gaps, resets runtime-only fields, and establishes
one validated history baseline. Existing zero-ID constructors and native
save/journal recovery keep their contracts. See the
[typed admission guide](docs/managed/cpp_runtime.md#first-initialization-from-identified-storage)
and [197-case Windows/Linux proof](docs/verification/verification-managed-identified-baseline-2026-10-20.md).

The new [external JSON lesson](example/managed/external_document/README.md) makes
the existing 2.13 document adapter executable: bounded owning callbacks, full
Save/reopen, retained redo and burned identities. Public generated identity
placeholders remain zero; authoritative native IDs and recovery metadata live
in companions. No application codec or saved-format migration is required.

Regenerate Python package facades for byte-correct buffer-view admission, final
sink cancellation checks and reliable owned-descriptor cleanup if Save stream
construction fails. Borrowed streams stay open; prepublication failures retain
the old final bytes, and later durability failures report publication uncertainty.
See the [descriptor/spool/Save proof](docs/verification/verification-package-lifecycle-2026-10-20.md)
and [C++/Python typed rollback and preservation checks](docs/verification/verification-package-profiles-2026-10-20.md).

Rebuild the native runtime and regenerate Python package facades for the
[ZIP64 input corrections](docs/verification/verification-package-zip64-input-2026-10-20.md).
Representable classic counts/offsets at their exact maxima are accepted when no
ZIP64 locator is present; supplied ZIP64 metadata must agree. Python now borrows
checked seekable inputs from their current position and leaves them open at EOF
after successful use. Genuine nonseekable inputs retain bounded private spooling.
Native directory indexing reuses existing budgeted scratch and preserves entry
order. For archives with multiple independent errors, diagnostic precedence may
change; isolated duplicate-name categories retain their contract.

Schema language **1.20.0**, existing model APIs and serialized formats are
unchanged. Both editor source packages use **2.0.7** for navigation to generated
Java/JS/TS package facades and offset-preserving exclusion of marked Java runtime
support from declaration indexing. Older generated output retains fallback
indexing; normal language-service navigation remains separate.

## Compiler/runtime 2.13.0

Require `find_package(Serializer 2.13 CONFIG REQUIRED)` for differential sidecars
and `<rohit/managed_journal_document.hpp>`. New explicit `sidecar` journals use
container version 3; the default storage mode remains `appended` and keeps its
version-two container. Existing version-two journals recover and continue writing
their original representation until `save_journal()` explicitly publishes a new
generation. Older runtimes reject version-three containers. Upgrade every reader
before distributing new sidecars; ordinary model/save envelopes and collaboration
message protocols are unchanged.

To keep an application-format main file, provide an exact owning encoder/decoder
through `make_journal_document_codec<Root>(format_id, encode, decode)` and set
`journal_options.document_codec` on creation and recovery. Select `sidecar`
explicitly. An existing matching document is attached byte-for-byte; a mismatching
model is rejected. The document codec sees normalized persistent IDs and transient
defaults; side-channel metadata restores the native IDs and allocation state.
Format IDs must remain stable for compatible callbacks. The caller owns external
parser bounds and resource/lifetime policies; the file adapter checks physical
budgets and the exact saved-byte digest before invoking the recovery decoder.

Keep `<path>`, `<path>.journal` and its selected `.journal-<high>-<low>` companion
together. A `.journal.pending` manifest may be required after interrupted Save;
recover before manual cleanup. Full Save publishes updated document bytes and
matching metadata through the recoverable generation protocol. It preserves
unsent requests, exact retries and receive cursors; Save does not acknowledge
collaboration work. A clean document can still have a nonempty outbox.

The generic caller-adapter implementation does not close all built-in XML/OPC
profile, standard-specific crash, performance and platform qualification tasks.
Schema language **1.20.0** and editor packages **2.0.5** remain unchanged. See
[usage](docs/guides/usage.md#external-documents-with-a-differential-sidecar),
[journal formats](docs/managed/journal.md#version-three-differential-sidecars)
and [remaining work](docs/pending.md).

## Compiler/runtime 2.12.1

C++ XML floating-point parsing uses `std::numeric_limits<T>::denorm_min()` instead
of the traits deprecated in C++23. Rebuild consumers against the updated runtime
header to retain warnings-as-errors builds. Float, double and long double keep
signed-zero underflow and overflow rejection. No schema regeneration, API or wire
migration is required; schema language remains **1.20.0**.

## Compiler/runtime 2.12.0

Require `find_package(Serializer 2.12 CONFIG REQUIRED)` and regenerate owning
headers for `<rohit/xml_tokens.hpp>` and the generated C++ token traversals.
Explicit presence contracts select token codecs through the ordinary complete
XML APIs; the explicit token APIs reject unsupported rich object graphs.
Keep borrowed input/models alive through each synchronous operation, and publish
only after exact EOF. Direct writers may change XML prefixes/declaration placement.

Regenerate Python XML modules for incremental `decode_xml_chunks`, explicit nil,
all traversal, bounded key/unique/keyref validation and the separate HTML adapter.
Qualified non-C++ outputs accept concrete owning generic instances with resolved
field mappings; C++ supports checked concrete owning templates through generated
field bindings. Regenerate selected outputs for the independent scalar, identity,
lifecycle, digest, HTML and readonly XML policies described in the token guide.
Each backend retains its unsupported advanced-contract diagnostics. The CLI now
checks each requested backend independently; a Python-only supported contract
can generate without requiring C++ support. A failing multi-target request still
preserves every existing output.

For positioned C/Rust extensions, regenerate the owning document companion APIs
and keep the decoded sidecar with its native value. Initialize/free C
`Type_xml_document` through generated functions; Rust `XmlDocument<T>` owns
`value` and `extensions` separately. Their companion item/retained-byte policies
are cumulative over the document. Other generated extension policies retain
their documented owner-local contracts. Native codecs and record layouts do not
gain opaque payload fields in C/Rust. See the token guide for the extra private
byte buffers/scans and unsupported combinations.

Existing schema IDs, native JSON/binary payloads and managed bytes need no
conversion. Schema language remains **1.20.0** and editor packages remain
**2.0.5**. See [token subsets and ownership](docs/serialization/xml_tokens.md),
[the commented OPC example](example/opc/README.md), and
[executed checks](docs/verification/verification-xml-tokens-2026-10-19.md).

## Compiler/runtime 2.11.0

Require `find_package(Serializer 2.11 CONFIG REQUIRED)` for the new public
`<rohit/xml_pattern.hpp>`, `<rohit/xml_identity.hpp>`,
`<rohit/document_profile.hpp>` and `<rohit/html.hpp>` APIs. Rebuild compiler-library
clients because XML binding structures gained compiled facets, model/content
metadata and optional identity scopes. Regenerate and rebuild C++ package facades
to expose their additive owning profile metadata factory, normally named
`document_profiles()`. A package rendered as `document_profiles` uses
`document_profiles_metadata()` to preserve its constructor name.

Fixed-XSD tools now derive supported text/wrappers, retain scalar and identity
contracts, adopt chameleon namespaces, deduplicate legal cycles, associate
unambiguous anonymous records and expand supported import/export/evolution
checks. Review imported stable IDs/native ordering as before. Use
`--xml.export-xsd-bundle new-directory` to stage, validate and publish a pinned
multi-namespace bundle. The parent must exist; identical reruns succeed and
differing existing directories reject. Existing single-file export remains.

Portable regex compilation rejects unsupported dialect features; broader
existing XSD patterns retain their provider path. Identity projections use the
strict documented XPath/scalar subset and same-owner key/unique targets; nested
local identity owners and wildcard inference reject. Generated identity,
unordered/grouped content and generic/inherited/revision/projection codecs remain
gated per backend. HTML accepts the approved fragment subset separately from XML.

Existing native payloads need no conversion. Schema language **1.20.0**, editor
packages **2.0.5**, protocol values, field IDs, payload revision rules and managed
journal formats are unchanged. Review [advanced contracts](docs/serialization/xml_advanced_contracts.md)
and [verification status](docs/verification/verification-xml-advanced-contracts-2026-10-19.md)
for exact API/ownership/resource limits and executed qualification.

## Compiler/runtime 2.10.0

Require `find_package(Serializer 2.10 CONFIG REQUIRED)` for
`<rohit/xml_event_reader.hpp>` and `<rohit/xml_extensions.hpp>`. The incremental
reader uses the optional LibXml2 provider and delivers borrowed normalized events;
copy retained data and call `finish()` before publishing a decoded candidate.
It accepts UTF-8 XML 1.0, arbitrary byte splits and namespace scope, while rejecting
DTDs, user/external entities, other encodings and malformed/trailing input.
Provider-private allocations and native parser limits are separate from logical
event-state bounds. The 2.9 structural event validator remains provider-independent.

New [shared XML configuration contracts](docs/serialization/xml_shared_contracts.md)
bind explicit presence/nil, ordered mixed content, map entries, variant/type tags
and positioned extension slots. They validate shared IR and reference behavior;
their generated token codecs remain pending in every language. Selecting a new
contract produces an explicit backend capability diagnostic, rather than partial
generated support. Fixed-XSD overrides, XSD export and directional XML comparison
for these contracts reject. Existing generated codecs and native wire formats
require no payload migration or schema regeneration to keep their behavior.

Raw extension helpers require explicit foreign slots, complete namespace context,
correct positions and understood capabilities. Preserving unknown content does
not satisfy a required vocabulary. Publish the returned owning candidate only
after complete decoding and byte EOF validation. Rebuild clients of the compiler
binding/output-option structures, which gained additive members. Schema language
remains **1.20.0** and both editor packages remain **2.0.5**; this release changes
no schema syntax or editor navigation. See [executed checks](docs/verification/verification-xml-shared-contracts-2026-10-19.md).

## Compiler/runtime 2.9.0

Require `find_package(Serializer 2.9 CONFIG REQUIRED)` for the provider-independent
C++ [XML event API](docs/serialization/xml_events.md) in `<rohit/xml_events.hpp>`.
This additive API accepts normalized borrowed events and validates document
structure, cumulative limits and explicit completion while retaining only open
expanded names. Keep payload buffers alive through `accept()` and synchronous
consumption; copy any data retained afterwards. Failures are terminal and shared
operation-context charges are not rolled back.

Existing generated codecs and whole-object XML APIs retain their behavior.
No schema regeneration, payload migration or protocol-value change is required
to keep using them. The event API does not parse XML bytes: incremental UTF-8,
name and namespace processing is supplied by the 2.10 incremental reader;
generated token codecs remain per-language work. A provider must reject DTDs and user/external entities, check
trailing input through EOF before emitting `end_document`, and enforce byte
limits outside the event validator. Call `finish()` before publishing a candidate.
Schema language remains **1.20.0** and both editor packages remain **2.0.5**;
this runtime-only boundary changes no editor syntax or navigation.

## Compiler/runtime 2.8.0

XML support is now enabled by default in CMake and the repository's vcpkg
manifest. Provide libxml2, or explicitly configure
`-DSERIALIZER_WITH_XML=OFF` for a build without XML. Existing CMake caches retain
their previous value; set `-DSERIALIZER_WITH_XML=ON` to enable a previously disabled
build. To omit unused vcpkg dependencies, set
`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON` and select the compression features you
retain through `VCPKG_MANIFEST_FEATURES`.

Schema language remains **1.20.0**. Existing native schema identities and wire
formats are unchanged by this release. XML declarations and XSD association
already select XML generation automatically; ordinary schemas keep their existing
native output selection. Require `find_package(Serializer 2.8 CONFIG REQUIRED)`
for generated Python OPC profiles and the new XML contracts. Rebuild clients of
the XML operation context, ZIP entry metadata and compiler binding structures,
which gained members. ZIP entry metadata now exposes its central-directory
creator system and external attributes; OPC uses them to exclude non-file items
from logical parts while still validating their payload integrity.
Read the [backend contracts](docs/serialization/xml_packages.md) and
[remaining qualification](docs/pending.md) before assuming complete OPC,
3MF or backend parity.

## Compiler/runtime 2.7.0

Select language **1.20.0** for inline XML defaults/overrides, `using schema` and
`package` declarations. Existing 1.x schemas and ordinary native JSON/binary bytes
remain compatible; the C++ managed-ID binary policy below now defaults to compact
encoding. Require `find_package(Serializer 2.7 CONFIG REQUIRED)` for the new
compiler/runtime APIs; earlier same-major package requests remain valid.

Use [XSD import or manual binding](example/xsd/README.md) to avoid repeating
attribute/element placement in INI. Import into a new file when output differs,
then review native field identities and application edits. Keep existing explicit
IDs when migrating a handwritten model; automatically imported declarations are
not a promise of an identical native binary layout.

C++ package declarations generate synchronous borrowed-input Save and owning
typed load. Existing builder/callback APIs remain supported. Rebuild runtime and
compiler-library clients because optional XML/package context and binding
structures gained members. Unknown-extension slots and QName bindings are emitted
only for models selecting those XML contracts; ordinary native models do not
gain package storage. Other-language OPC profiles and broader XSD constructs remain
pending. See [the contract and limits](docs/serialization/xml_packages.md) and
[verification status](docs/pending.md). Both editor packages use **2.0.5**.

### C++ managed identity and collaboration

Select language **1.20.0** for `managed_ref<T>` fields and
`class ... managed class_id(decimal)`. Typed entity tokens use the existing
native document/object IDs and a stable uint64 class discriminator; they do not
replace `persistent_id` or allocate a second object identity. The default class
discriminator hashes the canonical qualified concrete schema type, independently
of field layout. Preserve a published discriminator with an explicit nonzero
`class_id` when renaming a class. Distinct emitted managed classes cannot share
an explicit discriminator; generation checks collisions. Keep the configured
native object-ID width unchanged.

C++ generated `persistent_id` values now default to canonical unsigned LEB128 in
all three native binary protocols. `[managed] id_encoding = compact_varint` is
the default; `id_encoding = fixed` or `--managed.id_encoding fixed` selects
width-based payloads. `id_type = uint32|uint64` continues
to select the native numeric type and allocation limit, independently of encoding.
JSON identities remain numeric. Typed token object IDs follow the same policy;
their document halves and uint64 class discriminator retain fixed-width encoding.
Ordinary application integer fields and `binary_integer` field tags are unchanged.

Generated managed schema identities distinguish ID-encoding policies. Changing
the policy changes binary wire bytes; select one consistently across participants
and stored data. Ordinary Load performs neither encoding autodetection nor
conversion; use the explicit offline migration tool for supported saves or raw
models.
This option adds no schema grammar or separate language-version bump; no
backward-compatibility qualification is claimed for this new default.
Follow the [persistent-ID migration guide](docs/guides/persistent_id_migration.md)
for generation commands and the requirements for converting an existing data set
without changing its identities or allocation state.

Targets must be concrete, unpacked, owning managed classes. Ordinary
`optional<managed_ref<T>>` represents absence. Concrete self/forward/cyclic
references are nonowning edges; complete candidates validate document, class and
target liveness before publication. The C++ backend explicitly rejects the
pending named-instantiation combination whose self-token still refers to an
internal generic specialization. Packed/view targets, unresolved emitted generic
targets and non-C++ managed-reference output remain unsupported.

Collaborating clients now reserve native identity ranges from the authenticated
authority before creating entities. `collaboration_transport::reserve_ids(count)`
forwards that authenticated operation; custom transports must supply it for
offline creation. Store-owned clients prefetch on synchronization and consume the
range locally. A standalone replica calls
`initialize_identity_lease(session, authority.reserve_ids(session, count))` after
synchronizing and before creation. Exhausted or unavailable capacity rejects
creation before the editor returns an ID. Failed/canceled allocations remain
consumed; retain the native checkpoint/journal and authority lease ledger during
recovery. Allocate another fresh range instead of reusing a saved old grant.
Attach the authority journal before issuing persistent ranges: reservations are
checkpointed before return, including ranges never used by an accepted edit.
An in-memory authority provides uniqueness for its live allocation context; an
older snapshot cannot remember ranges issued after that snapshot. Store-owned
client journal recovery restores consumed reservations even when their edits
were canceled and no full Save followed.

Client checkpoint format **2** preserves identical document/object IDs across
optimistic edits, authority acceptance and replicas; it has no active local/remote
ID translation table. Format-1 mapped client checkpoints reject instead of being
silently converted. Preserve/export outstanding work with the earlier release
before rejoining from an authoritative snapshot under the new contract. Unleased
native envelopes do not gain allocation-lease records. Keep transport
authentication, session permissions and durable storage configuration explicit;
copying a range value does not grant authority to publish its IDs.

## Compiler/runtime 2.6.0

Select `serializer version 1.19.0;` for inline value-reference fields such as
`public borrowed(payload) d (1);`. C++ `consumer.d = owner.b` stores a const payload
pointer without changing the owner; the application keeps that storage alive.
The explicit one-argument `bind` API is also available. These fields write exactly
the ordinary selected value. These handles own no replacement
payload and explicitly reject reference-field input; read their output with an ordinary owning
schema. They do not write or resolve document/object/path addresses. Use this
form for temporary export views, including payload records containing arrays, and
use scoped translator output to avoid copying the source buffers. Shared/weak
ownership modes remain optional. `shared(payload)` retains an independent payload;
`shared(owner.field)` aliases an inline field and retains its complete owner.

Existing language 1.18 named `reference` declarations keep their durable address
wire format and identity-taking bind/resolve APIs. They are not reinterpreted as
inline values. Choose the new field syntax explicitly when value-only output is
the intended contract. Existing owning field bytes can remain unchanged; ownership
itself is not encoded. Earlier schemas and 2.x CMake package requests stay valid.
Both editor packages use **2.0.4**; see [value references](docs/schema/value_references.md).

## Compiler/runtime 2.5.0

Regenerate new reference/translator schemas with compiler/runtime 2.5.0 and select
`serializer version 1.18.0;` in every declaring file. Existing 1.x language headers,
public generated APIs and serialized payloads remain compatible. CMake requests
for earlier 2.x packages remain supported; require 2.5 when using these APIs.
Both editor packages use version 2.0.3.

A named reference serializes its document/owner/generation/stable field path,
never its live binding or pointee. Load owners and explicitly resolve addresses;
applications retain object-ID, deletion and cross-document policies. Shared C++
references retain the storage owner. Follow the per-language lifetime and scope
rules in [field references](docs/schema/references.md). Existing conversions keep
their independent-copy behavior; opt into `translator` for synchronous borrowed
target traversal. Existing binary views and emission-only models are unchanged.


## Compiler/runtime 2.4.0

Regenerate and rebuild consumers to select `[output] protocols = binary_none`
across all languages, or use XML `text = member` and `wrapper.member = element`
mappings. Defaults remain all four native formats and XML opt-in. Existing
protocol IDs and bytes are unchanged. The new vocabulary mappings require explicit
configuration and change only that selected XML contract.

Fixed XSD inputs can use pinned local includes/imports; changing any bundle source
regenerates the codecs through depfiles. Use the
[XSD tooling guide](docs/serialization/xsd_tooling.md) for path/budget rules,
SHA-256 pins, supported nested types, export and reader compatibility checks.
Unsupported facets, identities and content models still reject.

C++ ZIP readers accept ZIP64 and can explicitly spool nonseekable input to a
bounded private file. ZIP32 output stays the default until its limits require
ZIP64 or an entry explicitly forces it. New OPC aggregate and preservation APIs
are caller-authored profiles; generated pipeline/backend parity and the managed
document-state bridge remain [pending](docs/pending.md).

Recompile compiler-library clients completely because output-option and XML-binding
structures gained members. Rebuild runtime clients using the changed compression
options and package classes. Existing named APIs, prior positional aggregate
initializers and `constexpr` compression policies remain supported. Require
`find_package(Serializer 2.4 CONFIG REQUIRED)` for these additions; earlier 2.x
requests remain compatible. Schema language **1.17.0** is unchanged.

## Compiler/runtime 2.3.1

Regenerate outputs to remove the injected Serializer support-permission banner.
Generated files preserve the copyright/license notices supplied by their input
schemas and add only the minimal generator identification and repository URL.
Inputs without license notices receive no invented license. Notices from included
schemas remain preserved, as do source markers used for editor navigation.
Serializer's upstream generated-support permission remains in `LICENSE-GENERATED`;
this change does not alter generated APIs, wire bytes or schema language 1.17.0.

## Compiler/runtime 2.3.0

Release **2.3.0** adds optional generated owning XML mappings across all language
outputs, fixed build-time XSD binding and explicit prepared runtime validation.
Build the compiler with `SERIALIZER_WITH_XML=ON` and select `[xml] enabled = true`
in generator configuration; regenerate and rebuild consumers. Unsupported XML/XSD
constructs fail explicitly. Runtime parser/provider dependencies vary by backend;
see the [usage contract](docs/serialization/xml_packages.md) and
[generated examples](example/xml/README.md).

Recompile compiler-library clients completely when upgrading: the output-option
structures gained XML settings. Older precompiled client objects must not be
linked against the new compiler library. Existing schema meaning and wire bytes
remain compatible.

C++ adds whole-object compression streams and ZIP/OPC source/sink/full-Save APIs.
These preserve existing inner codec bytes and native managed formats. They do not
implement the proposed managed dual-codec recovery bridge. General compression
and package parity, advanced XML/XSD and the bridge remain
[pending](docs/pending.md).

Use `find_package(Serializer 2.3 CONFIG REQUIRED)` when requiring these additions.
Earlier 2.x requests remain compatible. Schema language **1.17.0**, payload
revisions, existing protocol IDs and default generated codec selection are unchanged.
Both editor packages are **2.0.2**; their XML-language-service scope is unchanged.

## Compiler/runtime 2.2.0

Compiler/runtime **2.2.0** adds `model_store::discard_journal_changes()` for ordinary
journaled C++ stores. It lazily reads the last full-Save envelope under the native
writer lock, validates and restores its values and complete saved history, and
atomically replaces the journal generation. Object/revision allocation high-water
marks never decrease, abandoned edits cannot reappear through redo/recovery, and
the durable operation sequence advances once. Creation is the baseline before the
first Save. Shared or unattached stores reject this operation. Preparation failures
preserve the live state; uncertain replacement fences the writer until fresh
recovery. This operation adds no saved-envelope copy or work to ordinary edits and
changes no schema language or existing wire format.

Package discovery remains compatible with earlier 2.x releases. Use
`find_package(Serializer 2.2 CONFIG REQUIRED)` when discard is required; existing
2.0/2.1 consumers and schema language 1.17.0 remain supported.

## Compiler/runtime 2.1.1

Compiler/runtime **2.1.1** corrects the Go examples' native project workflow.
Every maintained Go consumer now keeps `go.mod`, `main.go`, `main_test.go`,
`generate.go`, a `model.serializer` entry, and a separate `internal/model` package. Put the
installed `serializer` executable on `PATH`, then run these commands from the
example directory:

```sh
go generate .
go build -o build/ .
go test ./...
```

Generation writes the ignored `internal/model/model_gen.go` and its `.go.d`
depfile. Run generation before the first build/test and after schema changes;
Go does not run it automatically. Applications import the model package through
their existing module path. No external Go dependency, CMake, Python, C++ runtime,
or cgo is required for the native workflow once the CLI is installed.
The native test packages verify independent fixtures, codec behavior, and relevant
rejection paths; the interoperability example also checks frozen positional bytes.

CMake wrappers invoke the same native workflow and can provide the compiler.
The Python qualification runners remain independent fixture and interoperability
tools: they copy the Go project and regenerate its internal package in an
isolated validation directory. The managed wire-record Go source module defaults
to history; its matrix copies select the other feature schemas independently.

Existing module identities, schemas, generated model APIs, and wire formats are
preserved. The only application source adaptation is importing the generated
model package and qualifying its symbols. Package discovery still accepts older
2.x requests such as `find_package(Serializer 2.0 CONFIG REQUIRED)`; use
`2.1.1 EXACT` to pin this patch. CMake source builds retain the 4.0 minimum,
schema language remains **1.17.0**, and editor packages remain **2.0.1**.
See [native Go projects](example/go/README.md).

Verification passed with Go **1.26.4**, Windows amd64: all twelve source modules
generated, built, and passed their native tests; `go build -o build/ .` also
passed. All twelve explicit-CLI CMake leaves passed configuration, build, and
CTest, as did the basic example with PATH discovery and an installed 2.1.0
package. Source fallback was configured but not built. The rebuilt Release CLI
reports compiler/runtime **2.1.1** and language **1.17.0**; CMake version metadata
accepted 2.0.0, 2.1.0, and exact 2.1.1 while rejecting 1.99.0 and newer 2.1.2.
Go/Python qualification passed all nine standard examples, generics, custom IDs,
all four managed wire features, and the affected conversion and magic suites;
interoperability included 48 exchanges and frozen positional bytes. Compilers
ran sequentially. The full C++ suite and eleven-SDK matrix were not rerun for
this patch.

## Compiler/runtime 2.1.0

Compiler/runtime **2.1.0** requires **CMake 4.0 or newer** for source and example
builds. Upgrade CMake before reconfiguring an existing build; the presets advertise
the same minimum. Individual examples now have their own `CMakeLists.txt`, including
every supported language's `basic`, `collections`, and other maintained consumers.
See [individual example builds](example/README.md#build-an-individual-example-with-cmake)
for source-checkout and installed-package commands and SDK prerequisites.

Every Go consumer directory retains its own `go.mod`. The runner copies that
manifest into the generated build module and invokes `go build` there. Existing
module identities remain unchanged; no external Go dependencies are added.

Versioned package discovery continues to accept older 2.x requests, including
`find_package(Serializer 2.0 CONFIG REQUIRED)`. Use `2.1.0 EXACT` to pin this release.
The schema language remains **1.17.0**; schemas, generated APIs and wire formats
retain their existing contracts. Both editor packages are **2.0.1**.

## Compiler/runtime 2.0.2

Compiler/runtime **2.0.2** fixes private arithmetic helper collisions in generated
Rust output that trigger `non_snake_case` diagnostics with `rustc 1.99` and
`-D warnings` on native Windows. Internal collision names use `_internal`
suffixes, repeated as needed to avoid authored names. Regenerate affected Rust
output through the consuming project's build rules and rebuild its consumers.
Private helper names may differ; authored schemas, public model and function
names, arithmetic semantics and serialized formats remain unchanged. The schema
language stays **1.17.0** and both editor packages stay **2.0.0**. No payload
revision or data migration is needed. See
[Rust qualification](docs/verification/verification-rust-windows-2026-10-15.md).

The shared example runner now executes conversion consumers directly without a
fixture argument. Conversion examples perform their own checks; the ordinary
codec examples retain the fixture-based four-protocol producer/consumer checks.

## Compiler/runtime 2.0.1

Compiler/runtime **2.0.1** fixes strict Clang builds of the digest runtime on
32-bit ARM by comparing the input length as an explicit 64-bit value.
The schema language remains **1.17.0** and both editor packages remain **2.0.0**.
Schemas, public APIs, generated output, digest results and serialized formats
are unchanged. Existing 2.0.0 integrations need no schema regeneration or data migration.

## Compiler/runtime 2.0.0

The compiler/runtime release is **2.0.0**; the schema language remains **1.17.0**
and both editor packages remain **1.1.48**. The concrete migration boundaries are
the default C++ artifact layout and versioned CMake package discovery. Supported
1.x schema meanings, generated model names, runtime APIs and serialized formats
retain their existing contracts.

- Update versioned installed-package requests to
  `find_package(Serializer 2.0 CONFIG REQUIRED)`, or use `2.0.0 EXACT` when pinning
  this release. The package's `SameMajorVersion` policy rejects requests for
  Serializer 1.x; removing `EXACT` does not allow crossing that major boundary.
  Existing unversioned package discovery remains available.
- Regenerate C++ output through the consuming project's build rules and rebuild
  its consumers. Separate mode is now the default: keep each entry header and
  its adjacent `<entry-output-filename>.includes/` tree together when copying or
  installing generated APIs. Preserve their relative paths. To retain one
  combined header, select `--include-mode inline`, `[output] include_mode = inline`,
  or CMake `INCLUDE_MODE inline`. Other language backends retain their existing
  combined compilation units/modules.
- Keep schema headers at their supported 1.x language contracts. New schemas may
  use `serializer version 1.17.0;`; the original `serializer version 1;` still
  means exactly `1.0.0`. Do not select `serializer version 2.0.0;` for this compiler
  release: compiler and language versions are independent. A release-number
  change alone does not require new field IDs, payload revisions or wire data.

See [generated include layout](docs/guides/usage.md#share-declarations-with-includes),
[CMake integration](docs/guides/cmake_integration.md), and
[version reporting](docs/guides/command_line.md).

## Generic owner-parameter defaults (12 October 2026)

Compiler/runtime **1.21.0** supports language **1.16.0**. Raise each declaring
file's header, including included files, when replacing a native constructor or
factory's `Frame input = Frame{}` with schema `Frame input = {}`, where `Frame` is
a type parameter of the owning class. Keep direct field bindings to preserve
nonassignable representations. Omission uses exactly the selected C++ type's
normal `{}` construction, evaluated only when an argument is omitted. Explicit
nondefault inputs remain valid without introducing a factory or class-wide
defaultability constraint. Ordinary codec/default-construction requirements
remain independent. Keep `default(factory)` where that factory deliberately
differs from normal host defaults.

Known supported owning generated record substitutions reuse the portable
`1.15.0` default contract. Open native and concrete primitive/enum/container
substitutions are C++-only and diagnose in other outputs. Host defaults may throw
or be non-constant; this feature does not add a `noexcept` or `constexpr` promise.
Independently generic input defaults cannot deduce an omitted type. Direct scalar
`{}` defaults, ordinary function defaults, partial/named record literals and
fixed-array cardinality retain their existing boundaries.

Regenerate and compile selected outputs, checking both omitted and explicitly
supplied arguments. Preserve tests for nondefault/nonassignable host values,
custom host defaults and exceptions. Existing `1.15.0` concrete record defaults
and `1.14.0` direct bindings remain unchanged. This parameter-only change adds no
fields, payload revisions, wire bytes or managed protocol metadata. See
[the construction contract](docs/schema/construction.md#generic-owner-parameter-defaults-in-language-116)
and [qualification](docs/verification/verification-generic-owner-defaults-2026-10-12.md).

## Empty class-valued parameter defaults (12 October 2026)

Compiler/runtime **1.20.0** supports language **1.15.0**. Raise the declaring
file's header, including included files, when adding `area input = {}` to a
constructor or named factory for a record with fields, or when nesting an empty
record literal in a positional parameter default. Omission creates a fresh
owning value from the class's schema field defaults and does not run parameterized
authoring constructors or their hooks. An external public zero-argument constructor
retains its host-defined behavior. Supplied values retain their ordinary binding
semantics.

Full positional defaults such as `area input = {10, 20}` remain available under
`1.14.0`; already-valid defaults for actually empty records retain their earlier
meaning. Partial/named record literals and ordinary function parameter defaults
remain unavailable. Under the `1.15.0` contract, unconstrained type parameters
require the existing explicit factory contract instead of `Value input = {}`.
Known schema record
applications such as `area<T>` use their generated owning defaults.

Regenerate and compile selected outputs. Use the portable `_default_<count>`
wrappers where the backend cannot express native trailing record defaults.
Verify fresh mutable defaults and explicitly supplied arguments in the consuming
application. No field IDs, payload revisions or wire protocols change. See
[the construction contract](docs/schema/construction.md#empty-class-valued-parameter-defaults-in-language-115).

## Owning APIs and typed defaults (12 October 2026)

Compiler/runtime **1.19.0** supports language **1.14.0**. Raise the header only in
files adopting readonly field exposure, typed recursive defaults, reciprocal
constructor dependencies, declaration-site C++ additions or arithmetic mappings.
Regenerate and compile each selected output. Earlier schemas keep their default
construction/lowering contract, public APIs, IDs, names and serialized formats.

Replace native readonly getters with `expose readonly(public) as name` only on
ordinary owning C++ storage. The generated getter borrows a const reference and
rejects temporary owners. Check naming-profile collisions; use `naming = preserve`
when a backing underscore and getter name must remain distinct. Keep existing
friend definitions for private writes and preserve owner/reference lifetime.

New constructor bindings initialize C++ members directly. Codec/default
construction stays separate from authoring. Typed nested defaults are fresh
values, but changing a released field's default still changes fresh/missing-field
application behavior and needs the application's compatibility review. Keep
custom coordinates and historical decode fixtures when introducing unit-Z values.

Reciprocal constructor types no longer need declaration order to follow their
call dependencies; by-value storage cycles remain rejected. Match external
`cpp { constexpr }` declarations with visible host definitions. An explicit
`constructor() = default;` differs from an external `constructor();` and from the
automatic codec default constructor. `constexpr` and `noexcept` are independent.

Map operators explicitly to typed readonly methods or owning constructor
expressions. Preserve strong result/quantity identities and checked errors;
a scalar helper produces a scalar operator unless it is deliberately wrapped.
No fields or wire tags are added. Run [the four examples](example/README.md#owning-record-apis)
and consult [qualification](docs/verification/verification-owning-api-2026-10-12.md).

## Explicit C++ noexcept functions (12 October 2026)

Compiler/runtime **1.18.0** supports language **1.13.0**. Raise the header only
in files declaring `noexcept(cpp)`, including included files; older contracts
remain supported. To migrate an external native declaration:

```text
serializer version 1.13.0;
class progress {
  public uint32 completed;
  public uint32 expected;
  public function bool complete() readonly noexcept(cpp);
}
```

Replace the native `cpp storage(none)` method declaration with this typed function
and retain the matching application definition `bool progress::complete() const
noexcept`. Or provide a native `cpp { ... };` function body. Regenerate selected
outputs and rebuild consumers; C++ external definitions and matching inherited
behavior contracts must preserve the exception specification. A derived matching
declaration for an explicitly qualified virtual/abstract base requires the
qualifier even without the `override` keyword.

The qualifier also supports readonly namespace functions and dispatched member
contracts. Eligible ordinary native bodies may combine it with `constexpr(cpp)`
in either order. `edit` methods and portable expressions/predicates reject it;
existing inference stays unchanged, including conservative caller guarantees
around native/external calls. Portable expression/predicate overrides of an
explicitly qualified base are currently unavailable. Conditional specifications
and arbitrary native postfix tokens are not supported.

This is an author promise: an escaping C++ exception terminates rather than
propagating to the caller. Serializer neither proves native purity nor catches
the exception. Other outputs ignore the qualifier and retain their normal failure
contracts; supply their own bodies or external attachments when selected. No
schema field, payload bytes, managed identity or runtime protocol changes. See
[behavior](docs/schema/behavior.md#explicit-c-noexcept-contracts) and
[verification status](docs/verification/verification-noexcept-2026-10-12.md).

## C++ constant evaluation and result contracts (11 October 2026)

Compiler/runtime **1.15.0** supports schema language **1.10.0** and preserves existing wire
formats. Regenerate C++ headers to obtain `constexpr` ordinary codec templates,
checked arithmetic, dotted-version constructors and union/variant name conversions.
Use constexpr-safe protocols and streams for compile-time calls; runtime calls
retain their validation and optimized byte paths. See the
[direct-codec contract](docs/serialization/constant_evaluation.md#34-direct-generated-codecs-and-value-qualifiers).

Value-returning factories, conversions, readonly typed methods and observers now
use `[[nodiscard]]`. Use the result or explicitly discard it with
`static_cast<void>(call())`, including deliberate failure probes. Proven
nonthrowing value operations gain `noexcept`; fallible validation/allocation stays
throwing, and generic observers/comparisons use their actual exception guarantees.
No ordinary model API becomes `consteval`; dynamic runtime inputs remain valid.

Raise the schema header to `1.10.0` in files declaring namespace functions or
calling declared readonly functions in expressions. Existing `1.9.0` and older
schemas keep their meaning. These behavior declarations add no wire fields.
See [function calls and Java mapping](docs/schema/behavior.md#declared-function-calls).
Boolean category/type constraints also require language `1.10.0`; existing
atomic `T: stateless` schemas keep their language `1.9.0` contract. The parser
checks concrete arguments and C++ emits equivalent `requires` clauses. See
[type constraints](docs/schema/generics.md#boolean-type-constraints).

## Derived sizes and portable conversions (11 October 2026)

Compiler/runtime **1.14.0** supports language **1.9.0**, preserving earlier
contracts. Raise the header only in files using `auto_size`, `wire_only`,
`size_scope`, `omit(all_except(...))`, declarative conversions or stateless type
constraints. Regenerate all selected outputs with the matching compiler/runtime.
See [derived sizes](docs/schema/derived_sizes.md), [conversions](docs/schema/conversions.md),
[constraints](docs/schema/generics.md#stateless-type-constraints), and
[qualification](docs/verification/verification-deferred-features-2026-10-11.md).

Adding a size field or changing its scope/width/format selection changes that
payload contract. Preserve released identities and migrate or revise historical
records explicitly; do not add a positional header to persisted data silently.
Changing an existing member to `wire_only` removes its generated storage/API and
can change native object layout. Prefer a new metadata declaration where that
change is inappropriate. This storage choice is opt-in and does not infer a raw
ABI format. Conversions and stateless constraints do not add serialized data.
Decoder `retain` remains bounded and still rejects missing or repeated headers.

## Integration enhancements (11 October 2026)

Compiler/runtime **1.13.0** supports language **1.8.0** while retaining earlier
schemas and their default wire behavior. Raise each declaring file's header only
when using portable constructors/factories, structured `default(factory)` values,
`layout(empty)`, generic inheritance, or the new checked expression operators.
Regenerate all selected outputs with the matching compiler/runtime. See
[construction](docs/schema/construction.md), [generics](docs/schema/generics.md), and
[behavior](docs/schema/behavior.md) for contracts and limitations.

New C++ type guards allow overlapping entry headers with identical definitions.
Incompatible output profiles for one qualified C++ type diagnose rather than
silently selecting whichever header was included first. Preserve the same
managed identity/naming configuration across those units.

Bounded keyed counting and guarded collection APIs do not change stored formats.
Signed-zero normalization is opt-in output behavior: default codecs preserve
existing bytes, and normalized cache/digest encodings need an explicit shared
policy. Do not silently normalize a released persisted payload. Changing a
structured default can change the meaning of a missing keyed field even though
no wire tag is added. Empty transient hints have no wire or history effect;
physical overlap remains compiler/ABI-dependent.

Native output/counting can now borrow `std::span` and `std::reference_wrapper`
directly; callers must keep referents alive and unchanged through synchronous
traversal. Eligible portable C++ construction remains `constexpr`, while
unsupported constant-evaluation shapes retain runtime construction.
[Class-specific comparison configuration](docs/schema/record_comparisons.md) is opt-in
for every backend and leaves wire bytes unchanged. Review selected fields,
NaN/unordered behavior and portable shape restrictions before enabling it.
Named generic records retain their native type identity and may override source
comparison settings. Compiler-internal C++ aliases share their template's
comparison selection; conflicting internal alias settings reject.

## Simplicity warnings and local suppression (10 October 2026)

Compiler/runtime **1.10.0** adds advisory parser warnings for initialized ordinary
fields named `magic`, supported revision scalar fields named `version` or `revision`,
and `version` declarations that repeat the default member name. Existing schemas,
including older language headers, remain accepted. The warnings suggest intent
from source or wire names; they do not rewrite declarations or alter generated
APIs, runtime behavior or serialized bytes.

Prefer `public version uint32 (1) { 1 };` over
`public version uint32 version (1) { 1 };`. This omission retains the default
member name, wire name, ID and payload bytes. Keep intentional custom names.
Do not automatically replace ordinary fields with `magic` or `version`: immutable
header validation and revision layouts can change an existing contract.

Language **1.5.0** adds `ignore(warning magic, version)` before a member's semicolon.
Use one or both rule names to keep intentional fields or explicit names without
the corresponding warning. Upgrade the header in each file using this new clause,
including dependencies. Suppressions have no wire effect, do not silence schema
errors or release-policy warnings, and need no payload-revision increase.
See [simplicity warning rules and examples](docs/schema/schema_reference.md#simplicity-warnings).

## Digest fields and creation (9 October 2026)

Compiler/runtime **1.8.0** and language **1.3.0** add digest byte fields. Use
`digest` for variable application-supplied bytes, `digest[N]` for a decimal fixed
byte count, or `digest(sha256)` and other supported names for the algorithm's
standard length. Update every schema that declares a built-in digest to language
`1.3.0`, including included files, and regenerate with a compatible compiler.
Older headers and schemas remain valid; declared types named `digest` keep their
prior bare-reference meaning. Existing unrelated wire bytes and APIs are unchanged.

Compute values explicitly in C++ with `<rohit/digest.hpp>` and
`rohit::make_digest<rohit::digest_algorithm::sha256>(input)`; application linkage
is still `Serializer::runtime`. Other outputs store/decode digests and use the
application's provider for computation. No OpenSSL dependency is introduced.
Fixed defaults are zero-filled storage, rather than the hash of an empty message.

Native binary retains the ordinary byte-array count and JSON uses numeric arrays.
Algorithm names add no payload tags; changing the algorithm requires peer
agreement even at the same byte width. Use the compatibility checker before
replacing an established field. Digest map keys, nonempty initializers, C++ views,
and Protobuf mappings are unsupported. See [the digest contract](docs/schema/digest.md)
and [why application licensing was separated](docs/project/licensing.md#why-the-license-was-separated).

## Proprietary application runtime and generated output (9 October 2026)

Compiler/runtime **1.7.0** separates the application runtime from the GPL compiler
API library. The schema language remains **1.2.0**; generated APIs and wire formats
are unchanged. Application runtime headers, compiled helpers, and managed support
now use [`LICENSE-RUNTIME`](LICENSE-RUNTIME) (0BSD). Serializer-authored generated
code has explicit any-license distribution permission in
[`LICENSE-GENERATED`](LICENSE-GENERATED), including proprietary applications.

Replace application linkage to `Serializer::serializer_lib` with
`Serializer::runtime`, including applications using pre-generated headers.
`serializer_generate` and `serializer_generate_variants` select the runtime
automatically; `Serializer::managed` also links it. Rebuild from this release or
use an installed package containing the new targets and licensing files. The
existing `Serializer::serializer_lib` compiler/parser/generator API remains
available under GPL-3.0-or-later for compatibility.

Regenerate models to propagate source-schema copyright and license comments,
including notices from included schemas. Preserve the schema owners' terms;
generation does not transfer copyright or remove restrictions in a third-party
schema. These permissions concern Serializer-authored code, and dependencies
retain their independent licenses. See [licensing](docs/project/licensing.md) for scope
and schema notice examples. Earlier package revisions need their own license
review; updating current source documentation does not update a published port.

## GoogleTest packages and Windows test launches (9 October 2026)

Repository tests require GoogleTest **1.18.0** or newer. The vcpkg manifest sets
that minimum for `gtest`, and CMake uses
`find_package(GTest 1.18.0 CONFIG REQUIRED)`. The default registry baseline for
unrelated dependencies is unchanged. GoogleTest remains a test-only dependency;
generated applications use the application runtime independently.

Upgrade an older dependency installation, clear a cached `GTest_DIR` override
with `cmake -S . -B <build-directory> -U GTest_DIR`, and reconfigure with the build's
existing toolchain/options. Rebuild the tests after package selection changes.
Windows test-directory executables now stage their transitive imported shared
DLLs beside the executable using configuration-specific target metadata, so
CTest and direct launches can find shared GoogleTest dependencies there. Empty
and static-only dependency lists need no copies. See
[test build and DLL setup](docs/guides/cmake_integration.md#windows-test-runtime-dlls)
for commands and the recorded verification scope.

## Portable fixed arrays (8 October 2026)

Compiler/runtime **1.6.0** adds JavaScript/TypeScript and Python generation for
fixed arrays without explicit initializers. Regenerate those outputs to use
`array[N] T`; existing variable-array schemas and wire formats are unchanged.
The schema language remains **1.2.0**. Owning arrays/lists start with exactly `N`
independent element defaults, and their codecs enforce cardinality in all four
native protocols. The ordinary sequence count remains encoded. TypeScript uses
`T[]` declarations, so enforce cardinality through the generated JS codec.
Explicit collection initializers, other portable backends, and direct Protobuf
fixed-array output remain unsupported. See [the backend matrix](docs/schema/generics.md#backend-support).

## Several generation configurations from one parse (8 October 2026)

Compiler/runtime **1.5.0** accepts repeated `--config` and output options to
generate independent variants of one schema from one parse. Existing single-config
commands, INI syntax, generated APIs, and serialization formats remain compatible.
The schema language stays **1.2.0**; no schema or payload-version migration is needed.

Keep existing INIs and replace separate compiler invocations with one invocation.
Pair configs and generic outputs in order, one output per config. Each config must
select exactly one language when using `--output`. Configs selecting several
languages require repeated language-specific outputs, one per config selecting
that language. Scalar CLI overrides are shared by all configs, and `--language`
replaces their language selections. See
[CLI examples](docs/guides/command_line.md#generate-several-configurations-from-one-parse).

For C++ consumers, replace separate `serializer_generate` calls for the same
schemas with `serializer_generate_variants(TARGETS ... SCHEMAS ... CONFIGS ...)`.
Targets/configs pair by order, with independent generated include directories.
The existing per-target header-generation targets and
`serializer_generated_headers` aggregate remain available. Changes to any tracked
input regenerate all variants for the affected entry schema; all variant outputs
are owned by one rule to support concurrent consumer builds. See
[CMake migration examples](docs/guides/cmake_integration.md#generate-several-configurations-from-one-parse).

Choose one consistent generated model definition within each consumer's compiled
translation units. Combining differing managed widths, representations, or naming
profiles in one binary is not made valid by batch generation. Invalid schemas,
options, and backend failures preserve all prior outputs and the shared depfile;
filesystem write failures may leave partial results.

## C++20 constant binary output (7 October 2026)

Compiler/runtime **1.4.0** adds `binary_none_size`,
`serialize_binary_none_to`, and `make_binary_none_bytes<Factory>()` in
`<rohit/constant_binary.hpp>`. Existing schemas, APIs, object fields and native
wire bytes retain their contracts. The schema language stays **1.2.0**; no schema
syntax or payload revision change is required to enable this feature.

Regenerate owning C++ models with `--cpp.constant_evaluation true` or
`[cpp] constant_evaluation = true` before using them in required constant
expressions. This adds a separate `serialize_constant_out` traversal and static
capability metadata; regular runtime serialization remains unchanged. Keep each
model's generated definition consistent across translation units. Scalars and
handwritten eligible constexpr serializers do not require a generated-model flag.

The independent `--cpp.protocols binary_none` / `[cpp] protocols = binary_none`
setting intentionally removes JSON/keyed codecs and rejects Protobuf, managed and
view combinations. Keep its default `all` when those formats remain needed.
Both selectors are default-off/default-all and use normal CLI-over-INI precedence.

Factories construct transient string/vector data twice and return an owning exact
byte array; they must be deterministic and retain no dangling storage. Decode the
result with the existing runtime positional reader. Runtime calls to the new memory
writer for owning models use the established optimized encoder; any size pass is
explicitly requested.
Maps and runtime-only operations do not gain C++20 constant support. See the
[usage example](docs/guides/usage.md#count-and-emit-constant-positional-bytes) and
[implemented contract and qualification](docs/serialization/constant_evaluation.md).

Optionally select `--cpp.emission_only true` with both required settings above.
This profile changes generated C++ ownership to borrowed string_view/span fields
under a terminal emission namespace, with only a concrete
`serialize_out(binary_none_output&)` method. It preserves wire semantics but
supplies no reader; retain separately generated owning models for runtime decoding.
Referenced buffers must outlive traversal, and fixed-array views must match their
schema extent. Maps, managed/views, nonempty array defaults and multidimensional
arrays are rejected. A fixed-buffer owner can create the borrowed DTO at output time
without heap-owning model construction. Ordinary generation is unchanged by default.

## Compact scalar encodings (7 October 2026)

Compiler/runtime **1.3.0** supports schema language **1.2.0** and editor
extensions **1.1.25**. Existing 1.0.0/1.1.0 contracts and the original integer
header alias remain supported with their existing public APIs and bytes.

Opt into `compact_prefix [strict|lenient]` or
`compact_varint [strict|lenient]` on owning uint8/16/32/64 scalar fields. Select
`serializer version 1.2.0;` in each declaring file and regenerate all affected
language outputs. Omitted policy means strict. Prefix has a 30-bit maximum;
lenient explicitly truncates to the low 30 bits. Varint uses unsigned LEB128
and preserves the full declared type range. JSON and optional C++ Protobuf
mappings remain unchanged.

Adding or changing an encoding changes native binary bytes. Coordinate peers
and explicitly decode/convert existing stored data with its original contract;
a compiler upgrade does not auto-migrate payloads. Changing only strict/lenient
preserves valid bytes but changes writer acceptance. Signed/floating formats
remain proposals. The [selected numeric contract](docs/design/compact_scalar_contract.md)
now defines their payload mathematics; this design adds no syntax, wire value or
automatic migration. See [compact fields](docs/schema/compact_integers.md).

## Inferred arrays and typed magic (7 October 2026)

Compiler/runtime release **1.2.0** supports schema language **1.1.0**. Existing
`serializer version 1;` headers remain exactly the `1.0.0` language contract;
all existing explicit arrays and legacy byte magic preserve their generated APIs
and wire bytes. Updating the compiler alone requires no schema migration.

Select `serializer version 1.1.0;` in each file that uses either new declaration,
including included files. `public array[] uint32 values {1, 2, 3};` infers fixed
extent three. `public array[] char signature {'SRLFILE'};` infers seven decoded
bytes without an implicit terminator. Empty/missing initializers reject, and
fixed arrays remain limited to 1..65,536 elements. Inferred or initialized fixed
arrays require native C++ output; portable support above covers uninitialized extents.
Replacing an explicit extent with an equal inferred extent preserves storage
and wire cardinality; changing defaults later can change the inferred contract.

Use `private magic uint32 (99) {42};` or a declared enum for typed magic.
Supported types are `char`, `bool`, signed/unsigned integer widths, finite
`float`/`double`, and enums. Writers use the chosen codec's scalar/enum mapping;
readers require, validate, and discard the constant without object storage.
Keep a durable magic value, type, and Protobuf ID fixed. Changing a legacy byte
signature to a typed value can change native and Protobuf representations and
requires coordinated producer/consumer migration.

Both editor packages advance together to **1.1.24**, adding inferred-array
highlighting/snippets and typed-magic enum navigation. See
[fixed arrays](docs/schema/generics.md), [magic](docs/schema/magic_and_omission.md), and
[schema versions](docs/guides/command_line.md).

## Compiler portability (7 October 2026)

Compiler/runtime release 1.1.1 guards the durable output adapter's standard
`no_unique_address` attribute with a compiler feature check and compiles journal
recovery checks out of journal-disabled stores. This fixes clang-cl and GCC
warnings-as-errors builds while retaining existing object layouts and runtime
behavior. Journal failures use an internal exception wrapper with an ordinary
copy constructor so Windows Clang can retain and rethrow the indeterminate error
and its original nested cause without recursive exception capture. The public
error type, recovery fence, and durable decision are unchanged.
Rebuild the runtime and consumers; schema syntax, generated APIs,
payload revisions, and serialized bytes are unchanged. The schema language
remains 1.0.0. See the [four-toolchain verification](docs/verification/verification-toolchains-2026-10-07.md)
for the tested Windows and Linux configurations.

## Default runtime features (7 October 2026)

Fresh builds enable managed runtime records and all Zstandard, LZ4, and zlib
compression backends. SIMD remains enabled. The repository vcpkg manifest acquires
compression dependencies through default features. Existing caches keep their
values: reset `SERIALIZER_BUILD_MANAGED` and `SERIALIZER_WITH_*` with CMake `-U`,
or override them explicitly. Compression calls and on-wire formats remain unchanged.

Managed records generate during normal builds, including package builds without
tests or examples, and do not require a host formatter. Default installations
export `Serializer::managed` and its complete header dependencies. Explicit
`SERIALIZER_BUILD_MANAGED=OFF` installations omit record-dependent headers.

Development tools remain optional. Managed examples are included in managed test
builds; with tests disabled, select `SERIALIZER_BUILD_MANAGED_EXAMPLES=ON`.
Reduced dependency builds can disable compression backends and set
`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON`, selecting any retained manifest features.
The separately maintained vcpkg port must be updated to a published source revision
and declare default compression dependencies before these defaults reach its users.

## Release policies (6 October 2026)

Regenerate with the current schema compiler before using `releases` or nested
`policy` expressions. Both editor packages advance to 1.1.17. The schema language
version remains 1; older generators reject these additions.

Generated codecs contain ordinary resolved compatibility bounds, so release metadata
adds no payload bytes or runtime date logic. Different reference dates can change the
accepted read/write interval without changing individual revision layouts. Pin
`--version-policy-as-of` or CMake `VERSION_POLICY_AS_OF` in reproducible builds and
compatibility checks. Rebuild consumers after regenerating changed bounds. Existing
unversioned schemas and explicit compatibility declarations retain their behavior.

The current version stays readable; optionally fail generation on expiry warnings
with `--version-policy-warnings-as-errors`. See [release policies](docs/schema/versioning.md#release-dates-and-compile-time-policies).

## Payload revisions and read policies (6 October 2026)

Regenerate models with the current compiler and rebuild consumers against the
matching runtime before using `version`, `compatibility`, lifecycle annotations,
or class-scoped `reserve`. The schema language header remains `serializer version 1;`.
Both editor packages advance together to 1.1.16. See
[payload versioning](docs/schema/versioning.md) and the
[examples in all eleven languages](example/README.md#versioning).

Replace `json_read_policy::strict` with `read_policy::strict`. Replace the former
`json_read_policy::compatible` with `read_policy::flexible` to retain unknown JSON
field skipping; `read_policy::compatible` now accepts declared historical revisions
and rejects unknown fields. The old enum has no compatibility alias. Binary aliases
accept the common policy as a fourth template argument. Other generated languages
select it through `Limits`; update positional Go limits literals and Rust limits
struct literals for the added policy field.

Existing unversioned positional layouts retain their bytes and direct codec path.
Adding a version member introduces a discriminator prefix and does not make older
unversioned bytes readable automatically: decode with the original model, convert
explicitly, and write the new versioned model. Freeze the discriminator's type,
wire name, and ID for durable formats; its default ID is the first free ID starting
at 1. Preserve historical definitions and positional order within the supported
range. `replaced` validates the relationship but leaves value conversion to the
application. Versioned views, managed models, and Protobuf mappings remain unsupported.

## Native templates and dimensions (4 October 2026)

Regenerate C++ headers to use generic declarations directly from application code,
including unused declarations. Concrete specialization identifiers remain available
as aliases to native templates; their historical type-only spellings and bytes are
preserved. Optional `instantiate` declarations still produce distinct named roots.
Keep these or concrete schema fields when another language or the schema checker
needs a concrete contract. Older generators reject dimension/default/fixed-array
syntax; use the compiler and runtime from this change together (schema version
remains 1). The supporting revision is recorded in the
[qualification report](docs/verification/verification-dimensions-2026-10-04.md); no earlier released
compiler is advertised as supporting these additions.

Fixed arrays enforce exact present-field cardinality. At that release, they required
C++ native protocols; other language and Protobuf generation rejected them explicitly.
Both editor packages advance together to 1.1.15. See [generics](docs/schema/generics.md).

## Schema generics

Regenerate with the current compiler before using `class result<T>` or
`instantiate root = result<uint32>;`; older compilers cannot parse these additions
to schema version 1. Existing non-generic schemas and their wire encodings are
unchanged. Concrete applications preserve ordinary field/codec semantics, without
automatic type tags. A changed type argument still requires compatibility review.
See [schema generics](docs/schema/generics.md) for the finite C++ bindings and other-language
concrete APIs. Editor extensions 1.1.13 recognize the new syntax.

## Independent managed features

Rebuild consumers and `Serializer::managed`: store layouts and generated private
client records have changed. `model_store` and `managed` accept a fifth argument
`store_features::none`, `journal`, `collaboration`, or `all`. Existing four-argument
code defaults to `all`; ordinary model/journal formats remain compatible. Select
`none` with linear/tree history for history-only use; combine disabled history with
`journal` or `collaboration` for the isolated features. See the
[examples](docs/managed/cpp_runtime.md#independent-store-features).

`history_mode::disabled` now also disables collaborative undo on clients and authorities.
Callers that relied on collaborative undo despite disabled native history must select
linear/tree history. Authority `undo_operation`/`redo_operation` are unavailable when
history is disabled, and incoming inverse requests are rejected. The authority's seventh
feature argument and the session facade's fifth authority argument can disable journals.

History-enabled client checkpoint format/binding is unchanged. History-free clients use
`serializer.collaboration.pending.v1`, without undo/redo stacks, historical labels or
archived completed transactions. Earlier `serializer.collaboration.client.v1` checkpoints
from history-disabled stores are not accepted by this new policy. Recover them with the
previous runtime, resolve/persist outstanding delivery under that runtime, then join a
fresh client under the new runtime. Do not discard unresolved requests to force migration.
Host-managed archives can retain an explicit `state()` pin before pending-only compaction.

## Store-owned local collaboration

For immediate local editing, include `<rohit/managed_collaboration.hpp>`, attach a
typed session with `model_store::collaborate`, bind transport, then join the authority
baseline. Keep using existing generated editors and manual/scoped/callback
transactions. Explicit/timed synchronization and separate send/receive methods operate
on the store's outbox. See the [integration guide](docs/managed/local_collaboration.md).

Client checkpoint format **1** wraps the native model envelope/journal operation with
the session, local undo/redo, pending changes, mappings and exact outstanding request.
Rebuild `Serializer::managed` for the new generated client records. Ordinary model
saves and authority wire protocols 5/6/7/8 do not change for this addition. Client
wrappers require current client readers; attach the same session to a fresh store
before loading/recovering. Existing plain model journals are not automatically converted.
The journal flushes per committed transaction; only network exchange is interval-driven.

The collaboration `main.cpp`, `locks.cpp`, and `undo.cpp` examples now author document
changes through `model_store` transactions and `undo`/`redo`. Replace handwritten
proposal/delivery helpers in store applications with `store.synchronize()`. For host
lock requests on the same session, use `store.collaboration().reserve_operation_id()`
after joining rather than a separate operation counter. Reservations use the existing
checkpoint watermark and journal machinery; no additional format migration is needed.

Offline entity creation preserves local handles using persisted mappings to authority
IDs. Use `remote_id`/`local_id` at server/UI boundaries. Application integer fields,
foreign-key references and map keys are not inferred as identity references. After an
authority epoch change, retain and reconcile uncertain drafts explicitly; durable
client history does not restore the authority's epoch-local deduplication/history.

## Collaborative change records and conditional undo

Rebuild `Serializer::managed` and regenerate all managed model headers for
`visit_collaboration_fields` and `merge_collaboration`. The model's schema ID,
payload field IDs, ID width, and saved model envelopes remain unchanged.

Collaboration messages now use protocols **5/6** for uint64 custom/model endpoints
and **7/8** for application-defined session custom/model endpoints. These replace
protocols 1/2 and 3/4 respectively; upgrade both peers together and start a fresh
authority epoch. Native keyed decoders reject unknown fields, so the added history
metadata is not wire-compatible with older collaboration readers. Existing public
record aliases, constructor shapes, and proposal/editor APIs remain available.

`accepted_change.history` records one transaction's fields and dependencies; its
outer session, operation, sequence and domain supply authorship, grouping and scope.
`replica.undo` and `replica.redo` build requests for the session's latest eligible
history tip, using a fresh operation ID. Deliver them through `submit_change` and
apply the accepted result. Conflict, denial, and journal failure preserve the stack;
exact retries return their original outcome. An indeterminate journal write fences
the authority. See the [history guide](docs/managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).

Journal operation tag **5** records a validated restoration edit, allowing recovery
of original object identities after deletion undo. Existing ordinary edits retain
tag 1. Upgrade readers before opening journals containing tag 5. Collaborative undo
stacks and field-version tombstones are epoch-local and are not restored from the
document journal. Store clients now provide immediate local writes, durable pending
operations and periodic synchronization through the integration described above.

## Earlier managed collaboration migration

The protocol numbers below describe the earlier snapshot-only release; use the
history-bearing versions above for the current runtime.

The optional C++ managed target now also generates `collaboration_records.hpp`.
Include `<rohit/managed_collaboration.hpp>` for authority/replica APIs and regenerate
managed model headers for the added `model_traits::visit_collaboration` traversal.
Existing model wire formats, schema IDs, saved history envelopes, and ordinary
store APIs are unchanged. `model_store::allocated_id()` exposes the non-reusable
allocation watermark, including candidate reservations.
The [collaboration runtime guide](docs/managed/collaboration_runtime.md) defines
host session/epoch binding, snapshot conflicts, and bounded
retry retention. Restore documents into a fresh host-fenced epoch before opening
sessions. Coordination rights and retry results are absent from document journals;
reconnecting clients reconcile acknowledged state. No new selector syntax is enabled.

Ordinary generated edits no longer require application command records or a
dispatcher. Construct an authority without a handler and author proposals using
`replica.propose(session, operation, callback[, grants])`. This default endpoint
uses collaboration protocol version 2 with a version-one `model_change` payload;
the existing custom-handler constructor retains protocol version 1 and its opaque
application payloads. Endpoint contexts reject mixing these protocols. Rebuild
`Serializer::managed` to regenerate the added coordination record. Existing
envelope field IDs and document/model formats are unchanged.

Session types are now configurable without moving the existing history/traits
template arguments. `collaboration_authority<Root>` and `collaboration_replica<Root>`
retain `std::uint64_t` sessions and protocol versions 1/2. Use
`collaboration_session<Session, SessionTraits>` to select a different type throughout
the authority, replica, records, results, policy hooks, and lock cache. String and
unsigned integer policies are supplied; custom policies define identity validity,
ordering, and a bounded canonical codec. The application owns session creation and
lifetime; no automatic allocation or numeric mapping is introduced.

Custom session types/policies use protocol 3 for application commands or protocol 4
for model proposals, with the generated `session_envelope` around existing record
layouts. Use `encode_collaboration_record`/`decode_collaboration_record<T>` for these
typed records. Peers must agree on the session policy's stable `wire_name` and codec.
Rebuild managed support for the added envelope records; existing uint64 message bytes
and public result/cache aliases are preserved. Closed IDs still cannot be reused
within an authority epoch. See the [session example](example/managed/collaboration/sessions.cpp).

Draft callbacks run locally and do not change acknowledged state. Keep the encoded
proposal for exact retries. Resynchronize after a conflict and create a new
operation; failed creation may consume authority IDs without advancing its accepted
sequence. Default proposals include a full candidate snapshot and are bounded by
`max_command_bytes` and the new `max_created_ids` option (4096 by default).
The collaboration examples now use this API; optional custom-command fixtures live
under `test/resources/`.

The optional [C++ managed interface](docs/managed/cpp_runtime.md) adds
`Serializer::managed` and bare `managed` declarations. Regenerate managed headers:
the default now puts `persistent_id` directly on the schema class, and store reads
use `snapshot->name` rather than `snapshot->value.name`. Application field IDs stay
unchanged, but the direct codec adds metadata key `0x3fffffff`/`persistent_id`.
To retain the preceding ID-free payload and storage-wrapper API/wire format, set
`[managed] separate_values = true` or `--managed.separate_values true` consistently.
Default direct saves have a different schema binding; loading old wrapper saves
requires that opt-in representation or an explicit migration. Width changes also
require migration (`[managed] id_type = uint32|uint64`). Ordinary schemas without
managed declarations remain unchanged; other-language managed backends are not
implemented. The existing explicit managed occurrence boundaries still apply.

New stores no longer require an application-supplied document namespace; omit the
second constructor argument for automatic generation. Explicit IDs remain supported.
Loading restores saved identity. Object IDs are document-local and allocated from 1
upwards, with no renumbering/reuse on undo, deletion, or failed transactions.

History mode is now selected by `model_store<Root, Mode, Labels, Traits>` at compile time.
Use `history_mode::linear` (the default), `history_mode::tree`, or
`history_mode::disabled` as the second template argument. Replace the former
`supported_mechanism::none` with `history_mode::disabled`; remove `options.mode`
assignments. Replace `reset_history(mode)` with `reset_history()` only when clearing
the current history: changing its mode is no longer supported. Different modes
are different store types, and loads must match the receiving specialization.
The `managed` alias follows the same template arguments. Wire mode values remain
unchanged; the wire format also depends on the compile-time label policy.

Linear stores now use parameterless `redo()` and no longer provide
`checkout(revision)`, `redo(revision)`, or `redo_children()`. Use a tree store when
revision-addressed navigation is required. Manual generic code should declare
`Store::outcome_type`: linear/disabled use `transaction_outcome` with status/error;
tree uses `tree_transaction_outcome` and additionally provides `revision`.
Revision counters exist only in tree storage. Object IDs are unaffected.

Labels are now disabled by default. Replace `execute_transaction(label, callback)`
with `execute_transaction(callback)` and `begin_transaction(label, outcome)` with
`begin_transaction(outcome)`, or retain names by selecting
`model_store<Root, Mode, history_labels::enabled>`. Enabled stores accept both forms
and expose `undo_label()` plus linear `redo_label()` or tree `redo_label(revision)`.
Move a custom `Traits` argument from the third position to the fourth, inserting
`history_labels::disabled` or `history_labels::enabled` before it.

Default saves use version 3 with label fields entirely omitted, for both tree and
linear/disabled history. Label-enabled linear/tree stores preserve versions 2/1.
Loads must match the label policy and history mode; no automatic migration tool
is supplied. To read existing labeled linear/tree saves, enable labels. A converter
between label policies must explicitly add/discard names while preserving all
snapshots, selection, document/object IDs, and allocation marks.
Linear saves contain ordered snapshots and a zero-based cursor, with no revision
IDs or parent links. Older version-one linear/disabled saves additionally require
validating the old chain and translating its selected revision into an index.
Disabled saves contain no history and cannot enable labels.

Linear managed history now uses a deque and evicts oldest retained states on
changed commits when `max_revisions` or `max_history_bytes` would be exceeded.
Previously reaching either limit rejected the commit. The count includes the
current state; one permits editing without undo, while zero cannot hold an enabled
baseline. Oversized individual states still fail atomically. Tree mode retains its
existing reject-on-limit behavior. New changed edits after undo still discard redo
only in linear mode. Loading over-budget histories still fails instead of evicting
imported states. `max_revisions` remains the retained-state count option for both
policies; linear entries do not carry revision IDs.

Synchronous [file journaling](docs/managed/journal.md) wraps existing managed
memory envelopes in the base without changing their versions. Version-two journal
records append individual serialized snapshots and small control operations; the
earlier full-envelope journal format is rejected. To adopt it, construct/load a
store and call `create_journal` at a new path; reopen with `recover_journal` into
an unattached store. Use `save_journal` for a full durable Save. Existing raw
memory envelopes are not journal containers, and `save()` remains a memory export.
Extend exhaustive `transaction_status` handling for `indeterminate`: a write may
have committed on disk while the live model stayed unchanged. Destroy/recover the
store before retrying. No schema regeneration, editor package change, or selector
syntax is required for this runtime API.

Optional [message compression](docs/serialization/compression.md) adds C++ overloads without
changing existing bytes or calls. Rebuild the runtime with selected optional
dependencies and regenerate owning headers for member/static options. Free
helpers work with existing headers. Readers must explicitly select the same
standard compression format; there is no automatic uncompressed fallback.
The compression overloads finish the inner decoder even for buffer inputs.
See [compression verification](docs/verification/verification-compression-2026-09-17.md).

See the dated [verification record](docs/verification/verification-2026-09-17.md) for the source
revision, configurations, passing checks, and remaining validation work.

## Finalization correctness fixes

Regenerate Rust output to correct integer-spelled `float`/`double` defaults outside
Rust's inferred integer range. Regenerate C output for the equivalent `double`
literal issue. Accepted defaults now use floating syntax; field types and wire
representations are unchanged.

Regenerate Swift output for strict standard-library UTF-8 decoding. Foundation's
Windows string conversion could discard a leading U+FEFF; the generated runtime
now preserves it as string data, including in JSON escapes and binary payloads.
Malformed UTF-8 remains rejected, and U+FEFF is not ignored before field names or
numeric tokens.

Rebuild C++ consumers and regenerate Protobuf-enabled headers for corrected
ProtoJSON duplicate-field handling, including generated union wrappers. Later
nested objects replace earlier objects; later `null` restores the Protobuf default.
Binary Protobuf still merges repeated messages, and native JSON is unchanged.
TextProto enforces decoded string storage limits before growth, including escaped
Unicode and adjacent quoted fragments. This can reject over-budget input earlier.
Temporary token/numeric scratch remains outside the logical storage budget;
configure input/work and string/token limits too.

Existing `map(char)` wire ordering is preserved: Java uses signed bytes, C++ uses
its compiler's char signedness, and other backends use unsigned bytes. Equal maps
with high-byte keys can have different binary order. Use `map(uint8)` for new
portable byte-keyed contracts; do not silently change existing schemas, since
JSON key representation also changes. See [map ordering](docs/serialization/wire_format.md#map-ordering)
and [finalization verification](docs/verification/verification-finalization-2026-09-18.md).

## Binary string validation

C++ native binary codecs now validate UTF-8 by default for strings and field names,
matching all generated language runtimes. Invalid output raises
`std::invalid_argument`; invalid input raises `bad_input_data`. Owning string
decoding validates before replacing the destination. Mapping and mutable view
setters enforce the same rule; failed string updates preserve the backing bytes.
Constant generated binary field names are validated at compile time.

Valid UTF-8, embedded NULs, lengths, scalar byte order, and collection bytes are
unchanged. Rebuild the runtime and C++ consumers against matching updated headers.
The dedicated validator uses CPU-selected SIMD with a bounded scalar fallback;
existing generated headers do not need regeneration. Existing binary
messages containing invalid UTF-8 strings must be migrated to `array uint8`:
the byte-count prefix and payload representation are the same, but the schema
and generated APIs distinguish text from arbitrary bytes. Update both endpoints'
schema and regenerate together. Optional/nullable fields are not added by this
change.

An explicit C++ owning-codec `binary_text_validation::unchecked` policy can skip
runtime string/name checks when validity is guaranteed elsewhere. It preserves
length, bounds, resource, and non-text checks and has no runtime policy branch.
This can also read old C++-only raw strings during migration, but it does not
make those strings valid for strict readers or other languages. Generated views
and JSON remain strict. See [policy usage](docs/guides/usage.md#choose-c-binary-text-validation)
and [UTF-8 verification](docs/verification/verification-utf8-2026-09-18.md).

The little-endian wire profile remains independent of native machine byte order.
See [cross-endian verification](example/interoperability/README.md) for frozen
positional fixtures and the optional s390x/QEMU producer/consumer matrix.

## Additional native output languages

JavaScript/TypeScript, Go, and C# output can be generated alongside existing C++
and Java using the same C++ compiler. Existing wire bytes and CLI defaults are
unchanged. Follow [portable language mappings](docs/languages/portable_languages.md) when
sharing schemas: use portable literal defaults and the common owning subset.
Regenerate JS and declarations together. Generate one combined entry per Go
package, and use generated constructors to apply schema defaults.

C++ generation now spells the signed 64-bit minimum as a representable signed
expression and adds an unsigned suffix to decimal `uint64` defaults. Regenerate
headers using those defaults to avoid compiler-dependent literal diagnostics.
Values and encoded bytes are unchanged.

## Optional compatibility checks and exact decoding

Schema evolution checks are a separate read-only compiler mode:
`--input current.serializer --check-against previous.serializer` requires an
explicit `--compatibility-protocol`. Retain a versioned JSON reservation policy
with `--compatibility-policy`, and select the intended reader direction with
`--compatibility-direction`. This does not add schema keywords or change field
numbers, enum ordinals, union tags, or message bytes. See
[the checker guide](docs/schema/schema_evolution.md) before adding it to CI.

Recompile C++ callers with the new runtime headers to use the optional
`rohit::serializer::deserialize_exact<Value, Protocol>(input[, limits])` helper.
It returns a fresh value only after exact-message validation; input may still be
consumed on error. Existing generated headers work unchanged. Member input APIs
and generated static `deserialize` retain their existing buffer semantics and
storage-reuse behavior. See [exact decoding](docs/guides/usage.md#decode-one-exact-message-into-a-fresh-value)
for limits, ownership, framing, and commit behavior.

## Stream concepts and implicit standard-stream adapters

Rebuild the compiler/library and regenerate C++ headers to accept custom stream
implementations and standard iostreams directly. Existing calls using native
buffers and protocol names remain valid. Concrete codecs add an optional buffer
type parameter; low-level custom codecs must satisfy their input/output concepts.
Generated signatures and compiler parser/writer entrypoints are now constrained
function templates; code taking their addresses may need an explicit specialization.
Rebuild binary consumers together because compiled parser/writer entrypoints changed.

`type_check::stream` and `write_stream` now check structural capabilities rather
than inheritance. Custom buffers preserve the pointer fast path; iostream adapters
are selected implicitly by static type. Standard memory input borrows its unread
storage. Known file streams use larger I/O batches; generic/base-class streams use
the portable fallback. Stream output can be partially written before encoding fails.
Byte-stream input is one bounded EOF-delimited message, followed by exact-message
validation. No framing, protocol markers, or wire bytes change. See
[stream concepts and adapters](docs/guides/usage.md#stream-concepts-and-implicit-adapters)
for complete contracts and limits.

Regenerate C++ headers to use `object.serialize_in<Protocol>(input, limits)`.
This overload accepts the same native/custom buffers and standard streams as the
one-argument overload, with explicit `decode_limits`. Existing one-argument calls
and the free `serialize_from<Protocol>(input, object, limits)` remain supported.
Byte streams retain EOF-delimited exact-message validation; contiguous buffer
convenience calls retain their existing behavior without an implicit `finish()`.

Regenerated owning classes also provide static
`Type::serialize<Protocol>(output, value)` and
`Type::deserialize<Protocol>(input[, limits])`. The first returns void; the
second initializes and returns a new owning value. Existing member APIs remain
available, including `serialize_in` for reusing destination storage. Input
validation, default constructors, stream selection, and wire bytes stay unchanged.
Views continue to use `map`.

`serialize` and `deserialize` are now reserved in generated owning class scope.
The generator rejects colliding class/member names before emitting invalid C++.
Resolve member collisions while preserving wire names and IDs, for example
`public string serialized_payload ("serialize", 1);` for a field whose existing
wire name is `serialize` and ID is 1.

## VS Code navigation in extension 1.1.0

Upgrade the VS Code package to 1.1.0 for declaration navigation to source schemas
and definition navigation to existing generated C++ headers. Includes and schema
class/enum references are supported; C++ type references use the installed C++
definition provider. `Serializer: Open Generated Header` no longer offers to build
missing output. Navigation only reads available files and returns no result when
a destination is missing. Explicit `Serializer: Generate Headers` remains a
separate command. Existing generated files need no regeneration; available sibling
`<header>.d` files improve source mapping, especially for included schemas and
duplicate basenames. See [navigation](docs/editors/editor_extension.md#navigate-available-schemas-and-headers).

## Unquoted schema includes and namespace scopes

Prefer `include sales/order;` with the updated compiler. It resolves directly to
`sales/order.serializer` relative to the including file. Explicit `.serializer`
includes remain accepted and are required by older compilers; retain the full
spelling for dotted filenames such as `order.v2.serializer`. Files on disk and
CLI/CMake input paths keep their extensions. Both spellings share include-once,
cycle detection, and depfile tracking. This adds no schema version or wire-format
change. Update VS Code to **1.1.3** or Visual Studio to **1.0.3** for shorthand
highlighting; VS Code also resolves shorthand during navigation.

To split a schema, move shared declarations into another versioned `.serializer`
file and add `include common;` after the entry file's version header.
Paths are unquoted and relative to the including file. Generate only the entry:
included declarations appear in its combined C++ header or Java compilation unit.
Existing field order, IDs, names, and protocols are preserved; the wire format is unchanged.

Namespace scopes are reused during declaration creation. Duplicate qualified
classes/enums and namespace/type collisions now fail in the parser, before backend
generation. Distinct namespace blocks remain ordered for C++ emission; shorthand
`namespace a::b` is represented by nested namespace nodes in the returned AST.
Java emits one container for reopened namespaces. Applications inspecting AST
namespace names directly should account for the normalized nested representation.

Library callers needing includes use `parser::parse_file(path)` and pass the
returned `statements` to the existing writers. Stream-only parsing remains available
for fragments without includes. Reconfigure consumers to pick up transitive schema
dependency tracking in both CMake helpers. Install editor extension version **1.0.2 or later**
for the updated unquoted-include highlighting. See [include usage](docs/guides/usage.md#share-declarations-with-includes).

## Build portability and quoted defaults

Rebuild with the updated runtime headers for GCC/Clang and 32-bit MSVC fixes.
Identifier hashes retain their existing native-width arithmetic. Regenerate
LLVM-profile headers to avoid the storage-donor parameter/template-type collision.
Mapped views can be passed to a little-endian `binary_none` encoder's
`serialize_out(view)` with the updated runtime; their wire representation is unchanged.

Quoted schema defaults now preserve literal spaces, escaped quotes, and braces
inside the quotes. Keep defaults such as `"schema default"` unchanged; no escape
substitution or schema-version change is needed. Empty identifier input and
unterminated quoted defaults produce schema diagnostics.

The root `make all` (Linux) and `./make.ps1 all` (Windows) wrappers configure and
build enabled targets. Use `test` instead of `all` to run CTest after the build.
See [CMake wrapper options](docs/guides/cmake_integration.md#build-this-repository).

## SIMD JSON whitespace scanning

Rebuild `Serializer::runtime` and consumers with matching updated runtime
headers to accelerate long whitespace runs in native JSON and ProtoJSON input.
Existing generated headers work without regeneration. The existing
`SERIALIZER_ENABLE_SIMD` option controls the backends; disabled or unsupported
builds retain scalar scanning. Accepted whitespace, wire bytes, budgets, and
failure positions remain unchanged. See [whitespace scanning](docs/serialization/runtime_simd.md#simd-json-whitespace-scanning)
for scope and coverage, and the [verification record](docs/verification/verification-2026-09-17.md)
for unit tests and bounded sanitizer/fuzz results. Benchmarks remain outstanding.

## Reused JSON string scan results

Recompile consumers with the updated runtime headers to reuse JSON validation
results during string copying and escaping. Regenerating schema headers is not
required. Existing API calls, wire bytes, formatting, validation, limits, and
failure behavior remain unchanged; there is no new option or trusted-string mode.
The shared ProtoJSON input and ProtoJSON/TextProto quoting helpers benefit too.
See [JSON scan reuse](docs/guides/usage.md#reduce-repeated-json-scans) for remaining scans
and coverage. See the [verification record](docs/verification/verification-2026-09-17.md) for
unit tests and bounded sanitizer/fuzz results. Benchmarks remain outstanding.

## Pre-encoded constant field names

Regenerate C++ owning headers to use compile-time name tokens for native JSON
and string-key binary output, including fixed-width binary batches. Existing
schemas, wire spellings, bytes, endian choices, formatting, and object calls
remain valid. Old headers retain their original name paths; fixed JSON map
wrapper names are optimized by the updated runtime itself. Custom protocols
without the optional `encoded_field_name<Name>()` hook receive ordinary
`std::string_view` names. Input, views, Java, and Protobuf are unchanged.

An isolated binary name now reserves its length and text together, so a failed
reservation leaves both unwritten. The following value can still fail separately.
Review [constant field names](docs/guides/usage.md#pre-encode-constant-field-names) for
compilation/storage tradeoffs and custom-protocol behavior. Generation and test
results are in the [verification record](docs/verification/verification-2026-09-17.md);
benchmarks remain outstanding.

## Generated fixed-width field batches

Regenerate C++ owning headers to group adjacent fixed-width scalars automatically.
Native binary output batches all three key modes; positional binary input shares
checks while retaining scalar failure behavior. Existing object calls, schemas,
wire bytes, and endianness are unchanged. JSON and custom protocols without the
optional batch hooks retain individual calls. On output reservation failure, the
current batch remains unwritten; earlier output remains, so the failure prefix can
differ from individual writes. See [field batching](docs/guides/usage.md#batch-generated-fixed-width-fields)
before relying on output failure prefixes or custom reservation policies.
See the [verification record](docs/verification/verification-2026-09-17.md) for generation and
test results. Benchmarks remain outstanding.

## Nested destination storage reuse

Regenerate C++ owning headers with the updated compiler to reuse buffers in
generated objects inside arrays and maps. Existing input calls and schema syntax
remain valid; native JSON/binary wire bytes are unchanged. The runtime uses typed
generated donor support internally. Older headers continue decoding through the
fresh-element fallback, while direct string/container reuse is runtime-provided.
The generator reserves `serializer_reuses_storage` and `StorageSource` for its
support declarations; conflicting generated C++ names are diagnosed. Collection
elements still use schema defaults for missing fields. Review
[reuse and failure behavior](docs/guides/usage.md#reuse-destination-storage) before relying
on retained capacity or partial updates. See the
[verification record](docs/verification/verification-2026-09-17.md) for generation and test results.

## Optional Protobuf protocols

Regenerate C++ headers with `--cpp.protobuf true` (or `[cpp] protobuf = true`) to
use `protobuf_binary`, `protojson`, or `textproto` through the existing protocol
template API. Existing protocol aliases and wire formats are unchanged. Protobuf
input uses standard defaults for absent fields, not schema member initializers;
field IDs, maps, names, and union mappings have additional restrictions. Review
[the Protobuf contract](docs/serialization/protobuf.md) before changing an application's protocol.

## Versioned schema files and compiler options

Rename `.def` and `.struct` schemas to `.serializer` and add `serializer version 1;`
as the first statement. Update schema paths in scripts and CMake. All repository
examples and fixtures use this format. Legacy file extensions and headerless files
are rejected by the executable; the library parser still accepts headerless fragments
unless called as `parser::parse(input, true)`.

Replace bare CLI keys with long options: `input` becomes `--input`, `output` becomes
`--output`, `config` becomes `--config`, and `cpp.naming` becomes `--cpp.naming`.
The CMake helpers already use the updated interface. For both languages, pass
`--language cpp,java --cpp.output account.hpp --java.output AccountSchema.java`.
Each backend's settings can be overridden independently on the command line.
`serializer --version` reports compiler release 1.1.1 and schema language version 1.0.0.
These are separate version domains; neither changes wire bytes. See
[the complete CLI contract](docs/guides/command_line.md).

The canonical language header is now `serializer version 1.0.0;`. Existing
`serializer version 1;` headers remain exact aliases for `1.0.0`, including in
transitive includes; no schema rewrite is required. This integer shorthand is
exclusive to the original version 1 and never tracks newer language releases.
Future versions require all three decimal components; `2;` will not mean `2.0.0;`.
Use `schema_language_version_text` and the `schema_language_version_major`,
`schema_language_version_minor`, and `schema_language_version_patch` constants
for complete language metadata. The original unsigned `schema_language_version`
constant remains available as the language major.

Compiler/runtime minor and patch releases preserve full compatibility. Installed
CMake packages now accept requests for earlier minor and patch releases within
the same major version; `EXACT` still requires the requested release precisely.
Only developers change the major version manually, with best-effort compatibility
and explicit migration guidance for breaking changes.

## Java output and example folders

Java generation is an opt-in backend (`--language java`) and does not change the
default C++ API or wire layout. See [Java output](docs/languages/java.md) for supported
features, Java naming, parent composition, and differences from C++ ownership.

The configuration schema moved to `example/config/config.serializer`. C++ coding-style
examples now live in `example/coding_styles/<profile>/`, each with its own
`account.serializer`, `<profile>.ini`, and `<profile>.cpp`. Java examples live in
`example/java/`, with a separate folder for the round trip and each Java profile.
Update scripts that referenced the earlier shared schema or flat example paths.

## C++ source migration

This is a breaking C++ source migration. Update callers and regenerate schema
headers with the updated `serializer` executable before compiling. Legacy
headers and symbol aliases are not provided.

## Output profiles and generated names

Generation now defaults to the repository's C++ naming and clang-format rules.
For example, a schema field `ID` becomes the C++ member `id`, `reverseListMap`
becomes `reverse_list_map`, and type `IP` becomes `ip`. JSON/string-key names and
enum/union wire spellings retain their schema values. IDs, binary layout, and
endianness are unaffected by selecting an output profile.

Use `--cpp.naming preserve` (or `[cpp] naming = preserve` in the output config) to
retain previous schema-derived C++ identifiers. Otherwise update callers along
with regenerated headers. Profiles also rename union support types, discriminator
members, conversion helpers, and view accessors; runtime-required hooks keep their
existing spelling. Name collisions and reserved C++ identifiers are diagnosed.
Literal and declared-enum defaults are supported during renaming; opaque C++
expressions require `naming = preserve` or a rewritten default.

Install clang-format 19+ for generation. Set `--cpp.clang_format` on the CLI or
`SERIALIZER_CLANG_FORMAT_EXECUTABLE` for the repository's CMake generation rules
when it is not on `PATH`. Formatting failures do not replace the destination
header. CLI errors now return nonzero and unknown arguments are rejected.

See [output configuration](docs/languages/output_configuration.md) and the
[profile examples](example/coding_styles/README.md). Generation, compilation, and
regression-test results are in the [verification record](docs/verification/verification-2026-09-17.md).

## Generated representations and byte order

Binary fixed-width integers and floating-point values now default to little-endian.
Compact prefixes retain their existing encoding. This is an intentional wire
change with no compatibility aliases or automatic fallback. Applications that
require a specific byte order can set the third `binary` template parameter and
inspect `wire_endian`; see [the wire contract](docs/serialization/wire_format.md#byte-order).

`serializer version 1;` selects schema syntax and is not emitted into messages;
it identifies neither byte order nor wire version. Keep legacy peers on explicitly
selected big-endian protocols and agree on any transition outside the payload.
Independent frozen big-endian and little-endian fixtures now cover all three
native binary key modes. A wrong byte-order choice may parse successfully with
different values, so successful decoding alone does not identify the format.

Schemas can request `view`, restrict it with `readonly`/`mutable`, and add `owning`.
One mode retains a concrete class name; multiple modes require `person<storage_mode>`.
Owning nested fields and parents select the owning specialization. Nested types
must enable the modes requested by their owners. Views map little-endian positional
binary and expose size-preserving getters/setters. See [views.md](docs/serialization/views.md).
See the [verification record](docs/verification/verification-2026-09-17.md) for generated builds,
view tests, independent wire fixtures, and bounded fuzz runs.

## Headers and build targets

| Previous | Current |
| --- | --- |
| `<rohit/stream.h>` | `<rohit/stream.hpp>` |
| `<rohit/serializer.h>` | `<rohit/serializer.hpp>` |
| `<rohit/serializercreator.h>` | `<rohit/serializer_creator.hpp>` |
| CMake target `serializerlib` | `serializer_lib` |
| Test target `CoreSerializerTest` | `core_serializer_test` |

The `serializer` executable name and its `input` / `output` arguments are
unchanged. Generated test headers now use `.hpp`. Link applications to
`Serializer::runtime` to inherit the include directory and C++20 requirement;
`serializer_lib` remains the compiler API target. Tests are enabled by default
for a standalone build and disabled by default when included as a subdirectory;
set `SERIALIZER_BUILD_TESTS` explicitly to override this.

Runtime JSON scanning and binary array conversion now call compiled helpers.
Applications that previously included runtime headers without linking Serializer
must link `Serializer::runtime`, even when using pre-generated headers or
building with `SERIALIZER_ENABLE_SIMD=OFF`. The shipped `serializer_generate`
helper already supplies this dependency. SIMD preserves wire bytes and requires
no schema or output-profile changes; see [runtime SIMD](docs/serialization/runtime_simd.md).

The minimum language mode is C++20. CMake defaults to at least that version,
requires the selected standard, and disables compiler extensions for this
project's targets. Higher `CMAKE_CXX_STANDARD` settings remain effective. Direct
header users must also enable C++20 or later; the header check uses `_MSVC_LANG`
on MSVC to accommodate its default `__cplusplus` reporting.

`byteswap` and `change_endian` now share scalar constraints: booleans, integers
without padding bits, and supported 32-bit/64-bit binary IEC 559 floating-point
representations. Other types are rejected even when source and destination byte
orders match. `change_endian` accepts only little/big byte orders; `native` is
accepted when it aliases one of those orders. Both helpers are `constexpr` and
`noexcept`. Integers forward directly to `std::byteswap` when the feature is
available, with a C++20 fallback otherwise.

## Public C++ names

Namespaces remain rooted at `rohit`. Library-owned names use `snake_case`.

| Previous | Current |
| --- | --- |
| `Stream`, `FullStream` | `stream`, `full_stream` |
| `FullStreamAutoAlloc` | `full_stream_auto_alloc` |
| `FullStreamAutoAllocLimits` | `full_stream_auto_alloc_limits` |
| `FullStreamLimitChecked` | `full_stream_limit_checked` |
| `StreamAutoFree`, `FixedBuffer` | `stream_auto_free`, `fixed_buffer` |
| `streamlimit_t` | `stream_limits` |
| `MinReadBuffer`, `MaxReadBuffer` | `min_read_buffer_bytes`, `max_read_buffer_bytes` |
| `MakeStream`, `MakeConstantStream` | `make_stream`, `make_constant_stream` |
| `MakeConstantFullStream`, `MakeStreamFromFile` | `make_constant_full_stream`, `make_stream_from_file` |
| `CurrentOffset`, `RemainingBuffer`, `Reset` | `current_offset`, `remaining_buffer`, `reset` |
| `Write`, `WriteRaw`, `Append`, `Reserve` | `write`, `write_raw`, `append`, `reserve` |
| `Hash`, `ChangeEndian` | `hash`, `change_endian` |
| `typecheck` | `type_check` |
| `SerializeType::In`, `SerializeType::Out` | `serialize_type::in`, `serialize_type::out` |
| `SerializeKeyType::{None,Integer,String}` | `serialize_key_type::{none,integer,string}` |
| Protocol member `serialize_key_type` | `key_type` |
| `JsonOut` | `json_out` |
| `binaryInBase`, `binaryOutBase` | `binary_in_base`, `binary_out_base` |
| `SerializeIn`, `SerializeOut` | `serialize_in`, `serialize_out` |
| `StructSerializeIn`, `StructSerializeOut` | `struct_serialize_in`, `struct_serialize_out` |
| `SerializeInMemberByName` | `serialize_in_member_by_name` |
| `SerializeInMemberByIdentifier` | `serialize_in_member_by_identifier` |
| `GetStream` | `get_stream` |
| `write_format::intendtext` | `write_format::indent_text` |

Apply the same word conversion to the other stream/protocol helpers and exception
types, for example `BadInputData` becomes `bad_input_data`. The protocol templates
`json`, `binary_none`, `binary_integer`, and `binary_string` retain their names.
Custom serializer protocols and handwritten serializable types must implement
the renamed methods and expose `key_type` with the new enum type.
For custom protocols, `key_type` must be a static constant expression set to
`none`, `integer`, or `string`. Generated methods select the field operations with
`if constexpr` and reject unsupported modes at compile time.

## Binary field keys

The three binary modes retain their field-key encodings, with fixed-width values
using the byte order described above:

- `binary_none` writes field values in schema order without field identifiers or
  an object terminator. Unions retain a compact alternative index, including zero.
- `binary_integer` writes compact field IDs and ends each object with ID zero.
- `binary_string` writes length-prefixed wire names and ends each object with an
  empty name. Union keys retain the `field:alternative` spelling.

Schema parsing now rejects field and parent IDs outside `1..0x3fffffff` and empty
wire names. Decimal IDs are parsed without narrowing; excessively large tokens
raise `bad_member_spec` instead of overflowing or truncating. Existing default
ID assignment and explicit valid ID/name overrides remain unchanged.

For handwritten serializers, integer-key fields reject ID zero and string-key
fields reject empty names with `std::invalid_argument`. Numeric and named field
overloads now reject incompatible key modes at compile time. Positional union
discriminators still use the numeric pair overload, where zero is valid.

`serialize_out_variable` now throws `std::out_of_range` for negative values and
values above `0x3fffffff`, before writing any bytes for that integer. This also
applies to lengths, enum values, and union indices that use the same encoding.
Failures do not roll back previously written fields or prefixes. Ordinary
fixed-width signed integer fields continue to support negative values.

## Decoder, JSON, and generator qualification changes

Generated keyed fields and union payloads now hold references in their temporary
pairs/tuples. Serialization does not copy or move the source field. Generated
enum output borrows its spelling; the existing owning `to_string` API remains.
Regenerate headers through the generator when the build pass is authorized.
Custom output protocols need `struct_serialize_out_empty()` for empty generated
objects. Named enum input can optionally provide `serialize_in_named_enum(value)`;
custom protocols without it retain the owning-string fallback.

Binary scalar input now copies bytes into aligned storage and converts byte order
through unsigned representations. String-key input borrows field names during
dispatch. Binary vectors validate their count and storage budget before one
reservation, and strings assign directly from validated input spans.

Strings, vectors, and maps now **replace** existing contents on input. Vectors
move decoded elements and reuse capacity; maps retain ordered semantics and use
the last complete duplicate-key entry. Objects remain partial updates: missing
fields retain their destination values, and repeated fields apply in order.
Failures retain earlier writes, consumed prefixes, completed fields/entries, and
budget charges. This is not whole-object atomic decoding.

Both input protocols accept `decode_limits` as a second constructor argument and
share its accounting through nested values. Defaults bound consumed bytes, string
length, collection/object counts, nesting depth, accounted storage, and work.
Protocols are noncopyable; use a reference for nesting and a new protocol per new
message budget. `finish()` opts into exact-message validation. Generated stream
convenience overloads continue to read one value without requiring end-of-input.
Input bytes must remain independent of mutable destination storage.

JSON now validates strings, escapes, Unicode, punctuation, number grammar, and
range. Mixed-case booleans, leading plus signs/zeroes, trailing commas, invalid
UTF-8, unpaired surrogates, numeric overflow, and malformed suffixes are rejected.
Output escapes names/strings. Floating output uses shortest general-format
`std::to_chars`, so text such as `3.140000` becomes `3.14`; non-finite values are
rejected. Empty objects and `std::nullptr_t` values are supported. JSON map wire
shape remains an array of `key`/`value` entries.

Schema parsing rejects duplicate IDs/names, explicit/implicit ID collisions,
duplicate enum/union alternatives, and unknown class attributes. `stable_ids`
requires explicit IDs for every member and parent in that class. Generated name
dispatch groups hash collisions and checks full equality. Numeric enum input
checks generated enum membership. Union readers validate alternatives and begin
the selected member's lifetime before decoding; raw union payloads must be
trivially destructible. Generated serializer methods are public even when a
class is empty or its last data member is private. The generator exits with a
failure status when schema parsing or writing fails.

Parser exceptions expose a structured `code()` and omit input excerpts by default.
Use `diagnostic_options` or `decode_limits::diagnostics` for bounded, escaped,
opt-in excerpts. `format::compress` means whitespace suppression, not compression.
Recompile consumers after these decoder and exception-interface changes.

See [the wire-format contract](docs/serialization/wire_format.md) for exact limits, exception
categories, lifetime rules, and schema-evolution requirements, and
[qualification tools](qualification/README.md) for the prepared validation pass.

## Stream safety and ownership

Owning `stream_auto_free` and `full_stream_auto_alloc` objects are now move-only;
`full_stream_auto_alloc_limits` remains noncopyable and supports ownership moves.
Use `std::move` to transfer ownership or a borrowed stream view to share access.
Pointer-taking owning constructors still require exclusively owned,
malloc-compatible storage. `fixed_buffer` also supports move assignment.

Forward cursor movement and dereferencing now reject exhausted buffers before
changing state; reaching the end exactly is allowed. All `full_stream` variants
check backward movement. `at_unchecked()`, direct pointer mutation, and backward
movement on a base `stream` retain caller-validated preconditions. Borrowed stream
assignment copies the complete view; assignment through a const base view only
commits a cursor sharing the same buffer end.

For a batch, validate or reserve the complete byte count once, then use the
nonvirtual `push_unchecked`, `append_unchecked`, `advance_unchecked`,
`get_curr_and_increase_unchecked`, or existing `at_unchecked` helpers. They do not
check bounds or allocate, including on an allocating stream. The caller must keep
all accesses within the validated range; growth invalidates borrowed pointers.

```cpp
// data is a byte array whose storage is independent of output's allocation.
output.reserve(data.size());
for (const auto byte : data) {
  output.push_unchecked(byte);
}
```

Byte-only `write_raw(...)` calls now reserve once for the whole argument pack.
Their bytes are captured before reservation, so byte arguments may refer to the
output allocation even if it grows. Insufficient capacity rejects the batch
before writing any of its bytes. Mixed argument packs retain per-argument appends.
Binary input uses unchecked consumption after its existing range validation;
three- and four-byte variable integers now require exactly two and three bytes
after their first byte, correcting the previous oversized remaining-byte checks.

`write(...)` now batches multiple characters, booleans, character arrays, strings,
and string views into one reservation. For example, `write('"', value, '"')`
reserves the complete quoted text. Boolean text remains `1` or `0`. Numeric
mixtures and single arguments retain their existing formatting paths. Lengths
and scalar bytes are captured first; text ranges are copied in argument order
with overlap support. The built-in allocating streams rebase internal sources
after growth. A length, source-range, or reservation failure occurs before the
batch writes any bytes.

The protected `reserve_fragments` hook handles batched source rebasing. Custom
streams that relocate storage must handle this hook as well as `reserve_append`;
the built-in allocating streams provide both. Each batch still invokes virtual
`reserve` once so custom limits remain effective. Recompile consumers after this
addition to the virtual interface.

`append_external(source, size)` skips source-alias bookkeeping for independently
owned data. The source must not overlap the stream's backing storage or become
invalid during reservation. General `append` retains rebasing and overlap support.
Integer text output and binary scalar output use the independent-source path;
integer text still reserves only its actual formatted size.

Prefix comparisons now compare bytes consistently on signed-char platforms,
including bytes above `0x7F`. Empty prefixes and literal terminators retain their
existing behavior. Pointer helpers rely on the C++20 rules for null-plus-zero and
null-pointer subtraction while retaining invalid-range checks.

Bounded allocating streams copy their supplied limits; callers no longer need to
keep the limits object alive, and later edits to that object do not alter an
existing stream. Invalid policies are rejected. Zero-capacity streams allocate
lazily, and zero-byte operations on empty storage are valid.

Bounded reservations reuse the validated cursor offset and capacity and perform
growth calculations only when necessary. Adopted storage can be larger than the
logical maximum; reservations still enforce that maximum.

File helpers now throw on failed or incomplete I/O instead of returning partial
input or silently accepting failed output. Literal comparison and explicit
literal hashing exclude the final string terminator. Direct compact-output and
allocation-failure regressions are described in the
[verification record](docs/verification/verification-2026-09-17.md), along with remaining checks.

## Parser and writer API

| Previous | Current |
| --- | --- |
| `Parser::Parse` | `parser::parse` |
| `Writer::CPP::Write` | `writer::cpp::write` |
| `Base`, `Namespace`, `Class`, `Enum` | `syntax_node`, `namespace_node`, `class_node`, `enum_node` |
| `Member`, `Parent`, `TypeName` | `member`, `parent`, `type_name` |
| `AccessType`, `ObjectType`, `ClassAtributes` | `access_type`, `object_type`, `class_attributes` |
| `AccessType::{Private,Protected,Public}` | `access_type::{private_access,protected_access,public_access}` |
| `ObjectType::{Namespace,Class,Enum}` | `object_type::{namespace_type,class_type,enum_type}` |
| `Member::{none,array,map,Union}` | `member::modifier_type::{none,array,map,variant}` |
| `Name`, `MemberList`, `statementlist`, `parentlist` | `name`, `member_list`, `statements`, `parents` |
| `modifer`, `typeNameList`, `displayName` | `modifier`, `type_name_list`, `display_name` |
| `declaredNameSpace`, `definedNameSpace` | `declared_namespace`, `defined_namespace` |

## Generated code and serialized data

Generated protocol methods now use the renamed API and two-space indentation.
Schema-defined identifiers keep their spelling: a schema member named `ID`
still generates a member named `ID`. The generator does not silently rewrite
user-defined classes, enum values, JSON keys, custom member names, or numeric IDs.
The existing fixtures deliberately retain mixed-case names to test that contract.

The API renaming alone preserves JSON output, binary layouts, field order, and
discriminator values. The later qualification changes above intentionally update
JSON formatting/parsing and internal name dispatch while preserving valid binary
wire representations. Renaming a schema member itself may change its default
wire name; retain an explicit name modifier when preserving that external contract.

```cpp
#include <rohit/serializer.hpp>
#include "person.hpp"

test::test1::person person{"Ada", 322};
rohit::full_stream_auto_alloc output{256};
person.serialize_out<rohit::serializer::json>(output);

const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
test::test1::person decoded{};
decoded.serialize_in<rohit::serializer::json>(input);
```

Use the checked-in `.clang-format` for future C++ edits. The repository's rules are
documented in [CodingStandard.md](CodingStandard.md).

## Additional native targets and language example folders

The C++ compiler now accepts Rust, Python, Swift, Kotlin, and C destinations;
regenerate from the same `.serializer` schema. Existing protocol bytes and IDs
are unchanged. Read [native target ownership and API differences](docs/languages/native_languages.md)
before adopting them, particularly C lifecycle functions and Swift `WireString`
map keys. Kotlin's optional package uses `--kotlin.package` or `[kotlin] package`.

Interoperability consumers moved from `example/interoperability/<language>` to
`example/<language>/interoperability`. Shared schemas remain in
`example/interoperability`. Every language folder now has four examples; see
[the runner and SDK requirements](example/README.md). The earlier CMake option
still runs its five-runtime subset; the full suite uses
`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES`.

## Durable file stream integration

`rohit::file_stream` in `<rohit/file_stream.hpp>` implements the existing byte-stream
concepts, so generated serialization calls require no schema regeneration. Link
`Serializer::runtime` and rebuild the runtime/consumers together. Native
journal I/O moved into this reusable stream implementation; the managed store owns
an internal storage-independent journal sink. Existing path-based journal calls,
version-two records, memory-stream APIs, and full-Save history behavior are unchanged.
`managed_journal_stream.hpp` adds bounded frame helpers for buffers and iostreams;
see [usage](docs/guides/usage.md#file-streams-and-journal-records) for explicit synchronization
and borrowed-payload lifetimes. No database backend is introduced.

## Generic optionals (10 October 2026)

Compiler/runtime **1.12.0** supports language **1.7.0** `optional<T>` using
C++ `std::optional<T>` directly, including nested and collection payloads.
Existing schemas and their native formats remain supported. Existing user
declarations named `optional` retain precedence over the built-in generic.
Use a `1.7.0` header in every file that declares the new built-in syntax.

Replacing a released scalar or a manually encoded presence/value pair with an
optional changes its wire layout. Retain old layouts and use explicit payload
revision conversion; the compatibility checker reports presence and inner-type
changes. Schema optional JSON is `[]`/`[value]`, preserving nested absence.
Legacy standalone C++ JSON optional generic arguments retain `null`/value
semantics. Direct Protobuf mappings and view/packed profiles reject the new
built-in type. See the [full contract](docs/schema/optional.md).
