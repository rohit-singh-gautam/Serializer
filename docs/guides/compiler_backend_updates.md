# Compiler and backend contracts in 2.34.0

Compiler/runtime **2.34.0** supports schema language **1.24.0**. The legacy `1`
header still means exactly `1.0.0`. Older declared language contracts retain their
default validation, native-expression acceptance and generic-call restrictions.
See the [qualification record](../verification/verification-compiler-backends-2026-10-24.md)
for executed checks and remaining limitations.

## Scalar defaults and native bit fields

Explicit language 1.24 scalar field defaults use portable typed literals: checked
decimal integers and finite decimal floating values, Boolean literals, ASCII
characters, supported quoted UTF-8 strings and declared enum constants. Native
C++ expressions remain available under older declared contracts; raising a
schema header requires replacing such expressions with portable literals.
Errors retain the declaring member's original location through included schemas
and concrete generic binding. Structured defaults retain their existing rules.

```text
serializer version 1.24.0;
class flags stable_ids {
  public uint8 selected (1) bits(3) {5};
  public uint32 counter (2) bits(24);
}
```

`bits(width)` selects **C++ unsigned bit-field object storage**. Width is 1 through
the declared uint8/16/32/64 width. It does not change field IDs, wire scalar widths,
byte order, JSON numbers or protocol framing. C++ ABI packing and object size are
implementation dependent. Direct C++ assignments have native bit-field conversion
semantics; generated decoders reject out-of-range input before assigning the
selected field. Ordinary in-place decoding can still modify earlier fields;
`deserialize_exact` retains fresh-value publication semantics.

The supported profile is concrete ordinary unpacked owning records and the four native
codecs. Managed/view storage, callables/conversions, compact fields, collection
bit fields, typed magic, versions, derived sizes and XML/Protobuf/constant-evaluation/emission-only
output and generated address-based comparisons have no bit-field mapping. Unsupported output selection rejects before
publication. Field-address APIs cannot take a bit-field address. Changing a bit
width changes the accepted value domain and is reported by compatibility checks.

## Independent generic calls and construction defaults

Language 1.24 schema expressions can call C++ independently generic readonly
functions and methods. Deduction uses exact typed arguments, checks consistent
bindings and declared constraints, and substitutes the selected return type.
Dependent owner constraints remain enforced by the emitted C++ `requires` clauses.
It does not guess integer widths from untyped literals. Existing exact
nongeneric overloads take precedence; ambiguous or unmatched calls reject.
Older declared language versions retain their unsupported-call diagnostic.

```text
serializer version 1.24.0;
function Input identity<Input: integral && !bool>(Input input) readonly expression(input);
function uint64 preserve(uint64 input) readonly expression(identity(input));
class value {
  public uint64 number;
  public constructor<Input: integral && !bool>(Input input = {}) {
    number = checked<uint64>(input);
  }
}
```

Independent constructor/factory input defaults accept fresh value initialization,
checked scalar defaults and proven factory defaults under their existing typed
construction rules. C++ callers supply a type when no argument can deduce it:
`value::create<std::uint64_t>()`. Ordinary default construction used by decoding
retains schema field defaults. Independently generic APIs remain C++ APIs;
concrete owning generic models retain their documented other-backend mappings.
Fixed-array Protobuf mapping retains exact repeated-field cardinality and its
[format-specific default rules](../serialization/protobuf.md#mapping-contract).

## Native separate sources

Use `--include-mode separate`, or `[output] include_mode = separate`, for C,
Rust, Python, Swift and Kotlin dependency layouts. Their CLI default remains
inline, preserving existing SDK build commands. The library
`writer::portable::generate_files` returns every planned artifact; the existing
single-string APIs always retain inline generation. Generation, collision checks
and formatting/validation finish before any output is published.

C/Rust retain one entry header/module. Its relative `#include`/`include!` links
assemble compiler-owned declaration fragments in their original order and
lexical scope; fragments are not independent public entry points. Python emits
a private sibling package containing one shared runtime and dependency modules;
keep that directory beside the entry module. The entry supports standalone and
containing-package imports. One-time linking resolves forward
calls without reflection in codec field access. Swift/Kotlin emit companion
sources under `<entry filename>.includes`; compile the entry and all companions
together in the same module/package. Generated cross-file helpers use internal
visibility. Select companions from the current `generate_files` result or depfile;
the CLI preserves older files rather than deleting them during regeneration.
Authored field access remains unchanged. Native preambles must use
visibility appropriate to the selected target's multi-file module rules.

Depfiles list every generated artifact and input. Source and sibling-output
comments bind editor navigation to the exact schema instead of a basename.
Move/copy the complete bundle together. Do not mix independently generated
overlapping model definitions in one module. Inline generation remains available
for build systems that accept only one target source file.

## Java streams, channels and packed buffers

Set `java.stream_output = true` in INI or
`--java.stream_output true` on the CLI. Owning and readonly projection encoders
gain `encode(Protocol, OutputStream)` and
`encode(Protocol, WritableByteChannel)`, returning the written byte count and
propagating `IOException`. The option is independent of array/NIO reuse options.

A count-only preflight validates the model and records fixed-size backpatches;
a second traversal writes through an 8 KiB chunk. Keep the model unchanged and
exclusively synchronized throughout both traversals. No whole-message byte array
is staged. Metadata grows with the number of size patches; existing per-field
UTF-8 conversions and codec temporaries retain their allocation behavior.
The JVM integer output-size bound remains. An I/O failure can leave a written
prefix. The encoder never closes or flushes the borrowed sink. Channels must
make progress: short writes retry, while a zero/negative write throws instead
of spinning. Nonblocking scheduling and retries belong to the caller.

Java `packed` accepts fixed-width primitive scalar records without parents,
magic, versions, callables, collections or protocol-specific omissions. It adds
`PACKED_SIZE_BYTES`, `pack(ByteBuffer)` and `unpack(ByteBuffer)`. Packing writes
the existing little-endian positional wire layout directly to heap/direct
windows without padding; success advances position, failure preserves it and
can change bytes inside the original window. Unpacking copies exactly one record
into fresh owning fields and advances only after successful decoding. Existing
four-protocol owning codecs remain available. This specifies dense **output**,
not JVM heap object layout or a new serialized format. No JNI provider is needed.

## Borrowed and mapped views

Mapped C++ views retain their
[positional little-endian, borrowed lifetime and bounded access contract](../serialization/views.md).
Real Windows/POSIX shared-file mapping checks exercise unaligned messages,
read-only aliases observing size-preserving edits, persisted bytes and rejection
before crossing the mapped window. Allocation/timing probes compare generated
view mapping/traversal against owning exact decoding of the same workload;
opening and closing the operating-system mapping are outside the timed region.
Their measurements describe that workload and host, not a universal performance
ratio or process peak. The owner must outlive every view, iterator and borrowed
string; file mappings are not automatically owned by generated classes.
Other formats, endianness and unqualified platforms retain explicit boundaries.

## OPC signature provider

`<rohit/opc_signatures.hpp>` adds `package::signatures::available`, `verify`,
`sign` and `remove`. The provider uses the **Windows Packaging API** and the
existing XML/OPC runtime. Other platforms reject with `unavailable`.

The selected profile is RSA-SHA256, SHA256 part digests, inclusive C14N for
signature/whole relationship parts, and byte-exact ordinary part digests.
Relationship selectors, custom references, other signature algorithms,
timestamps and certificate-chain/revocation services are outside this profile.
Signature XML rejects DTDs/entities before provider access; custom references
reject before cryptographic validation. External package links remain inert.

`verify` requires a caller-pinned DER certificate, validates its current validity
interval and every package signature with that key, and requires the requested
coverage **in each signature**. Embedded certificates never establish trust.
The result reports actual signed parts and relationship sources; the root source
is `/`. Require the application payload and relevant relationship sources
explicitly. Successful selected-part verification does not authenticate unsigned
parts or establish an application's document semantics.

`sign` imports one private-key certificate from an in-memory PFX without
persisting keys, requires explicit coverage, rejects already-signed input and
returns owning package bytes plus its public certificate. No API writes files
or changes trust stores. Verify an authenticated package before consuming it;
use `remove` explicitly before edits, then sign and verify the new bytes.
Editing signed content while retaining old signatures invalidates them.

ZIP/OPC preflight checks integrity and configured expansion/input limits. The bounded
owning provider emits ZIP32 packages, rather than OS-specific ZIP64 sentinel headers.
Provider
allocations are not routed through Serializer's allocation hooks; output admission
occurs before returning owning bytes and does not establish a hard process peak.
The provider owns only per-operation COM objects; inputs are borrowed synchronously.
Generic OPC preservation and generated package facades do not verify signatures
implicitly. The [Microsoft validation contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/api/msopc/nf-msopc-iopcdigitalsignaturemanager-validate)
separates content validation from signer trust; Serializer supplies the explicit
pinned-key policy above.
