# Constant evaluation for positional binary serialization

[Back to the project overview](../README.md)

Status: implemented in compiler/runtime release **1.4.0**. Updated: 2026-10-07.

This document describes C++20 constant-evaluation support for Serializer's
binary_none protocol. The public APIs, default-off generator option, and independent
positional-only C++ profile are implemented. Existing runtime codecs remain the
wire-compatibility reference. Section 17 records actual generated-model qualification;
sections 12 and 14 retain the broader regression and performance acceptance contract
and do not claim that every listed benchmark or build configuration has been tested.

## 1. Purpose and required outcome

An application shall be able to describe a value once, determine its exact
`binary_none` memory requirement during constant evaluation, allocate inline
storage of that size, and serialize the value into that existing storage during
constant evaluation. The resulting bytes shall be readable by the existing
runtime `binary_none` reader for the same schema and payload revision.

Counting and writing shall use one generated field traversal and one wire
contract. Applications shall not maintain independent size formulas or duplicate
metadata solely to supply the counting pass. No runtime initialization shall be
needed for a successfully constant-evaluated byte array.

The implementation shall have zero impact on existing runtime serialization
execution/performance and memory usage, with constant-evaluation generation both
disabled and enabled. Preserve allocations, reservations, bulk copies, SIMD
validation, encoded size, and generated object layout. Ordinary runtime calls shall
not acquire a sizing pass, stored evaluation flag, cached size or extra model copy.

## 2. Scope

The required baseline is C++20 with a matching standard library. C++23 syntax may
be supported as an optional implementation detail; it shall not be necessary for
this feature. The format is explicitly `binary_none`, also known by the existing
`binary_positional` alias, with its existing little-endian fixed-width profile.

The feature consists of:

1. A constant-evaluation-capable byte-counting sink.
2. A constant-evaluation-capable stream over caller-owned writable bytes.
3. A constexpr-capable `binary_none` encoder and generated traversal.
4. Public size and existing-memory write APIs.
5. An optional immediate-function factory returning an exact-size owning array.
6. A compiler opt-in for generated constant-evaluation support, defaulting to false.
7. An independent optional C++ profile emitting only positional binary codecs.
8. Wire-parity, failure, toolchain, and runtime-regression qualification.
9. An optional borrowed emission-only model profile with a concrete output method.

Compile-time deserialization, JSON, keyed binary, Protobuf, compression, file I/O,
managed stores, and constexpr support for every standard container are outside
this initial feature. Existing runtime support for those features shall remain.
The ordinary runtime reader is the required consumer of constant-produced bytes.

## 3. C++ function selection

### 3.1 Shared constexpr implementation

`constexpr` permits a function to participate in constant evaluation; it does
not force every call to run during translation. `consteval` enforces an
immediate invocation. Neither specifier distinguishes otherwise identical
overloads, and declarations of the same function must agree on the specifier.
Use one constexpr implementation or distinct helper names/types.
See [C++20 draft, declarations](https://timsong-cpp.github.io/cppwp/n4861/dcl.constexpr).

An explicit specialization for a different stream type can have its own suitable
specifier, but that is type-based selection, not automatic selection based on
whether the caller happens to be constant-evaluated.

The preferred public size/write functions are `constexpr`. A separately named
`consteval` convenience function shall enforce compile-time output when needed.

### 3.2 Constant-evaluation and runtime byte paths

The non-execution-policy overloads of `std::copy` and `std::copy_n` are constexpr
in C++20 and can run inside an immediate function. Prefer them for compatible typed
byte/code-unit ranges in the new constant-evaluation path. This does not make every
operation surrounding a copy constexpr-valid. See
[C++20 draft, copying algorithms](https://timsong-cpp.github.io/cppwp/n4861/alg.copy).

Preserve the existing runtime implementation where necessary to meet the zero-impact
contract. In C++20 an ordinary `if (std::is_constant_evaluated())` can select the
constexpr algorithm without executing it at runtime. Do not use
`if constexpr (std::is_constant_evaluated())`; that condition is itself required
to be a constant expression and therefore selects the constant branch. See
[C++20 draft, evaluation query](https://timsong-cpp.github.io/cppwp/n4861/meta.const.eval).

The following is a language-level illustration, not an existing Serializer API:

```cpp
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Copy independent byte ranges; the caller has already checked their extents.
constexpr void copy_octets(std::uint8_t* destination,
                           const std::uint8_t* source,
                           std::size_t size) {
  if (size == 0) {
    return;
  }
  if (std::is_constant_evaluated()) {
    std::copy_n(source, size, destination);
  } else {
    std::memcpy(destination, source, size);
  }
}
```

The constant branch shall call constexpr helpers. An ordinary conditional does not
make a parameter-dependent call to an immediate helper valid. C++23 `if consteval`
supplies an immediate-function context for that use; its branches still require
well-formed code. See
[C++23 draft, selection statements](https://timsong-cpp.github.io/cppwp/n4950/stmt.if)
and the [original proposal](https://open-std.org/JTC1/SC22/WG21/docs/papers/2021/p1938r3.html).
Runtime-only work needing nonliteral local objects shall stay in ordinary helper
functions rather than directly inside a C++20 constexpr body.

### 3.3 Copy selection and the runtime guarantee

std::copy and std::copy_n specify element assignment through iterators; memcpy
specifies byte copying. The standard specifies linear assignment complexity for
the copying algorithms, not a throughput advantage over memcpy. Implementations
may lower eligible contiguous trivial-element copies to a bulk memory operation,
but that does not guarantee identical instructions, cost, or debug-check overhead.

A constexpr algorithm is a capability, not a standard guarantee that its runtime
machine code or cost matches memcpy. Do not replace every memory copy solely to
make constant evaluation possible. If one std::copy implementation is considered
for both modes, require unchanged semantics and zero runtime impact under the
supported compiler/library/build configurations before accepting that replacement.
Otherwise retain the established runtime fast path and use the standard algorithm
only in the constant-evaluation branch.

| Existing operation | Constant-evaluation design | Runtime requirement |
| --- | --- | --- |
| Independent uint8 byte range | std::copy/std::copy_n over live typed elements | Preserve current copy cost, reservations and cursor behavior |
| Text/code units to wire bytes | Typed element copy/conversion after text validation | Preserve existing text/bulk/SIMD paths |
| Fixed scalar object representation | Explicit integer byte encoding, or bit_cast to eligible bytes | Preserve current unaligned-safe memcpy path |
| Native scalar array representation | Encode eligible elements with the same wire order | Preserve current bulk copy/endian conversion |
| Potentially overlapping append | Outside the new independent-buffer API | Retain existing memmove and alias-rebasing behavior |
| SIMD register loads/stores or runtime decoding | Outside initial constexpr-output scope | Retain existing operations |

Element assignment and object-representation copying are different operations.
Copying a uint32 value into a uint8 element does not encode its four wire bytes.
Casting a scalar pointer to byte pointers remains forbidden during C++20 constant
evaluation even if std::copy performs the subsequent copying. Raw representation
copying has separate rules; see
[C++20 draft, object representation](https://timsong-cpp.github.io/cppwp/n4861/basic.types).

std::copy is not a general replacement for memmove's overlap guarantee. In
particular, its destination cannot begin inside the input range. Preserve all
previously permitted append overlap directions and source rebasing. The narrow new
fixed-memory API deliberately requires independent source/destination ranges.

No compiler option, constant-evaluation branch, or standard-algorithm preference
may relax the runtime CPU, memory, or encoded-size requirements in section 12.

## 4. Meaning of existing constant-evaluation memory

The destination is writable storage whose lifetime began within the same enclosing
constant evaluation. It is not a const-qualified array that the encoder may
modify. An application can create a local array inside a constexpr/consteval
factory, create a borrowed stream over it, write it, and return the owning array.

An immediate call cannot modify a runtime array created before that call or an
already existing mutable static buffer. Temporary dynamic storage must be
released before the evaluation ends. No pointer into factory-local or transient
allocated storage may escape in the final byte-array result. See
[C++20 draft, constant expressions](https://timsong-cpp.github.io/cppwp/n4861/expr.const).

The stream itself borrows storage; copying or destroying it does not copy or free
the destination. Its cursor is temporary state and is not part of the encoded
message. Source and destination lifetimes shall cover the write, and their memory
ranges shall not overlap in the new minimal fixed-memory API.

## 5. Public API

These declarations are in `rohit::serializer`, exposed by the installed
[constant_binary.hpp](../include/rohit/constant_binary.hpp) header in release 1.4.0.
Include `<rohit/constant_binary.hpp>` explicitly for the complete owning/runtime
API. Emission-generated headers include
[constant_binary_output.hpp](../include/rohit/constant_binary_output.hpp), which
supplies counting, exact-array factories, the concrete output protocol, and the
existing-memory overload for emission models and borrowed spans. Ordinary owning
generated headers continue to include the existing runtime header.

```cpp
namespace rohit::serializer {

// Validate and return the exact positional binary byte count without output allocation.
template <class Value>
[[nodiscard]] constexpr std::size_t binary_none_size(const Value& value);

// Serialize once into independent caller storage; return the number of bytes written.
template <class Value>
[[nodiscard]] constexpr std::size_t serialize_binary_none_to(
    std::span<std::uint8_t> destination, const Value& value);

// Build the factory value twice and return its exact-size inline binary representation.
template <auto Factory>
[[nodiscard]] consteval auto make_binary_none_bytes();

}  // namespace rohit::serializer
```

### 5.1 Size API

`binary_none_size(value)` shall validate every condition that affects whether the
selected positional message can be written. It returns the number of encoded
bytes, including magic, payload revision, compact prefixes, union discriminators,
collection counts, and selected field payloads. It shall allocate no destination
buffer and shall not read uninitialized or inactive union storage.

The size operation can run at runtime as well as during constant evaluation.
Compile-time eligibility depends on the actual value's construction, traversal,
customizations, and the compiler/library combination; a constexpr declaration
alone is not a guarantee for all values.

### 5.2 Existing-memory write API

`serialize_binary_none_to(destination, value)` starts at destination offset zero,
writes one message, and returns the committed byte count. Extra destination
capacity is permitted and shall remain untouched after that count. Passing a
subspan selects a region within a larger caller buffer. No terminator or envelope
shall be appended automatically.

This is a single writing traversal. It shall not secretly perform a sizing pass.
For insufficient capacity, each operation/batch shall fail before writing outside
its reservation; successfully written earlier bytes may remain. Whole-message
rollback is not promised. Callers needing a no-write capacity rejection can first
compare `binary_none_size(value)` with destination capacity, keeping the source
unchanged. That preflight does not make arbitrary custom callbacks transactional.

An empty positional object can write zero bytes to an empty span. The implementation
shall not dereference or perform unnecessary pointer arithmetic on a null empty
span. Neither API is `noexcept`: value validation and capacity checks can fail.

### 5.3 Immediate exact-array factory

`Factory` shall be a compile-time template argument callable without runtime
inputs. The factory shall deterministically construct the same serializable value
for the count and write invocations. It shall not retain mutable external state,
change behavior based on sink identity, or return dangling borrowed fields.

The implementation follows this two-pass algorithm:

```cpp
// Produce only owning wire bytes; temporary model allocations die in each pass.
template <auto Factory>
[[nodiscard]] consteval auto make_binary_none_bytes() {
  constexpr auto byte_count = [] {
    const auto value = Factory();
    return binary_none_size(value);
  }();

  std::array<std::uint8_t, byte_count> bytes{};
  const auto value = Factory();
  const auto written = serialize_binary_none_to(
      std::span<std::uint8_t>{bytes}, value);
  if (written != byte_count) {
    throw "binary_none count/write mismatch";
  }
  return bytes;
}
```

The factory is invoked twice by contract. The first evaluation returns only an
integer count; the second writes into the final local array. Ordinary local
`std::string` and `std::vector` members may use transient C++20 allocations,
provided the supported standard library allows their operations and releases all
storage. Do not require `constexpr auto value = Factory()` for a heap-owning model.

An ordinary function parameter is not made suitable as a template extent simply
because the function is consteval. The following is intentionally ill-formed:

```cpp
// Invalid: a normal parameter cannot determine this return array's template extent.
consteval auto incorrect_exact_array(const auto& value) {
  constexpr auto byte_count = binary_none_size(value);
  return std::array<std::uint8_t, byte_count>{};
}
```

Callers shall instead use the factory API, compute a caller-side constexpr count
from a suitable constant input, or supply fixed capacity as a template argument.
The factory approach handles models with variable-length transient storage without
forcing each text length into a generated model's template parameters.

## 6. One protocol with two output sinks

The generator uses its existing positional field-emission routine for both the
ordinary runtime traversal and the additive `serialize_constant_out` traversal.
Counting and constant writing invoke the same generated `serialize_constant_out`
method and positional codec operations. The protocol's public format
identity, key mode, byte order, omissions, and payload revision shall agree for both
sinks. A custom traversal shall not select a different layout solely because it
can distinguish the concrete counting and writing protocol types.

The additive implementation uses `detail::constant_binary_out<Sink>` for count
versus constant emission; the count and memory sinks do not derive from `rohit::stream`.
The existing runtime binary_none and output-buffer contract remain unchanged.
Applications need no model template parameters for literal lengths or format
selection. Existing schema generic parameters retain their meaning.

### 6.1 Narrow sink contract

Add a binary-output-specific concept rather than requiring both sinks to inherit
`rohit::stream` or implement JSON/text formatting and byte-pointer extraction.
The existing `output_buffer` concept and its valid runtime users shall remain
source-compatible.

The new contract shall provide these operations or equivalent named primitives:

| Operation | Required semantics |
| --- | --- |
| `position_bytes()` | Return committed byte count/cursor offset |
| `reserve_bytes(n)` | Validate the next operation's extent; never advance |
| `put_u8(value)` | Commit one byte or count one validated byte |
| `append_octets(span<const uint8_t>)` | Copy/count exactly the provided bytes |
| `append_chars(string_view)` | Copy/count exactly the code units, with no implicit prefix |
| `commit_counted_bytes(n)` | Count-only operation after semantic validation |

Text prefixing, UTF-8 validation, fixed scalar encoding, and collection framing
belong to the protocol, not to the raw sink. A count-only compile-time capability
shall allow encoding primitives to account for scalar/batch bytes without obtaining
or dereferencing a writable pointer. It is a type property, not per-object state.

### 6.2 Counting sink invariants

The counting sink owns an integer count and uses the existing aggregate stream
ceiling, `rohit::detail::maximum_buffer_bytes`, derived from ptrdiff_t maximum.
The fixed-memory stream shall use that same ceiling; a destination above it shall
be rejected before use. This total-message limit is separate from individual
30-bit field/count limits. The counter owns no byte buffer, performs no output
allocation, and never returns a fake writable pointer. It shall check every addition before overflow and check multiplication
before using a collection count times fixed element width.

`reserve_bytes(n)` shall validate a prospective extent without committing it.
Append/commit shall advance exactly once. Do not count both a reservation and its
subsequent write. Nested objects and active union payloads follow the same rules.

Bulk fixed-field operations shall validate semantic values and add their exact
wire extent without running a helper that stores to memory. The current pointer
batch path cannot be reused by returning nullptr, dummy storage, or an imagined
large buffer. The same restriction applies to scalar-array bulk operations.

### 6.3 Fixed-memory stream invariants

The fixed-memory stream borrows `span<uint8_t>` and stores a size_t cursor. It shall
not allocate, grow, own, or free storage. It shall use checked indexed access or
otherwise valid typed pointer operations, preserve capacity boundaries, and allow
zero capacity. Validation must occur before subtraction or indexing.

The constant-evaluation path shall not use reinterpret_cast, access a value through
an incompatible pointer type, convert void pointers into typed pointers, call
nonconstexpr copying/validation routines, or serialize native aggregate memory.
Copy text by code unit conversion; emit fixed-width numbers explicitly; use
`std::bit_cast` for supported floating-point representations.

At runtime, existing streams and optimized copying/batching shall retain their
current behavior. A new stream type is additive; replacing the polymorphic general
stream with a new general-purpose implementation is not required.

## 7. Wire compatibility requirements

The [existing wire-format contract](wire_format.md) is normative for byte content.
Constant evaluation is an execution mode, not a new encoding. The implementation
shall preserve all of the following:

| Element | Required positional encoding |
| --- | --- |
| bool and char | Existing one-byte representation |
| Fixed integers | Declared width, little-endian, existing signed bit interpretation |
| float/double | Existing IEC 559 width and bit representation through integer encoding |
| Enums | Existing validated compact numeric value |
| Strings | Shortest compact byte-length prefix plus unchanged valid UTF-8 bytes |
| Dynamic arrays | Compact element count followed by element encodings |
| Fixed arrays | Existing count prefix retained, followed by element encodings |
| Generated objects | Fields in the selected schema/revision order, without field keys |
| Generated unions | Existing compact zero-based alternative index plus active payload |
| Raw magic | Exact schema bytes, without added length prefix or terminator |
| Typed magic | Existing scalar/enum encoding before the payload revision |
| Payload revisions | Existing discriminator and historical/current field selection |
| Omitted fields | Existing `binary_none` omission behavior |
| Compact scalar annotations | Existing strict/lenient prefix or varint representation |

An empty unversioned object without magic emits no bytes. Messages have no added
padding, endian marker, compile-time marker, or hidden size envelope. Embedded NUL
bytes in valid text shall remain part of the encoded length. Arbitrary opaque data
shall use byte arrays, not strings subject to UTF-8 validation.

The existing 30-bit compact-prefix maximum and 1/2/3/4-byte boundaries apply to
counts, strings, and relevant discriminators. Ordinary numeric fields remain
fixed-width unless their schema explicitly selects a compact annotation. The
[compact-scalar contract](compact_integers.md) remains unchanged.

For strict UTF-8 output, both sinks shall execute equivalent validation. Counting
shall not accept a value that writing rejects just because no bytes are stored.
Existing explicit unchecked text policies may remain on the generic runtime API;
these minimal APIs are strict. Extending them with policy selection shall
require matching size/write policies and separate qualification.

## 8. Implemented constant-evaluation support matrix

Support is required only when the entire evaluated construction/traversal is
valid under C++20. Runtime support and compile-time support are separate properties.

| Value/model | Constant-evaluation support |
| --- | --- |
| bool, char, fixed signed/unsigned integers | Supported |
| float and double accepted by the current codec | Supported with bit-preserving encoding |
| Declared enums and compact scalar wrappers | Supported with current validation/policies |
| std::string and std::string_view fields | Supported with valid source lifetimes and constexpr library operations |
| std::vector and std::array fields | Supported for recursively eligible elements |
| Borrowed std::span sources | Supported by the constant-binary APIs for recursively eligible elements |
| Nested generated owning objects | Supported for recursively eligible members |
| Generated generic/dimension models | Supported when their concrete members and operations are eligible |
| Generated raw unions | Supported only for currently legal alternatives with valid initialized active lifetime |
| Magic, omissions, payload-version wrappers | Supported for eligible owning models and constexpr-capable helpers |
| std::map fields | Runtime-only in the initial C++20 profile |
| Managed models, runtime views, compression or I/O | Outside initial constant-evaluation scope |
| Arbitrary custom serializers | Opt-in only when deterministic, validation-preserving and constexpr-capable |
| Generic host std::optional/std::variant or arbitrary ranges | No new support implied by this feature |

The additive constant-binary APIs support both writable destination spans and
borrowed source spans. This does not add source-span support to every ordinary
runtime protocol or silently change an existing owning generated field.
Do not silently change generated field ownership to make a previously unsupported
constant-evaluation case appear supported.

Existing generated raw unions require trivially destructible alternatives. Merely
adding constexpr to a serializer does not make string/vector union alternatives
legal. The tag shall correspond to the initialized active member, and only that
member may be read. A separate managed-lifetime or variant-based generator design
would require its own specification and compatibility review.

Custom serializers shall not use I/O, clocks, randomness, mutable static state,
unbounded external pointers, sink-dependent layouts, or mutation of the source
between count and write. A callable can satisfy syntactic concepts and still fail
constant evaluation for a particular value; diagnostics shall not claim otherwise.

## 9. Compiler opt-in, generator changes and binary-only output

### 9.1 Explicit constant-evaluation compiler parameter

Constant-evaluation support for generated C++ models shall be opt-in through this
Serializer compiler parameter:

```sh
serializer --input payload.serializer --output payload.hpp --cpp.constant_evaluation true
```

The INI equivalent is:

```ini
[cpp]
constant_evaluation = true
```

The default is `false`. Accept exactly `true` and `false`, reject invalid/repeated
scalar settings under existing argument rules, and preserve built-in defaults,
then INI, then CLI precedence regardless of argument order. An explicit CLI false
shall override INI true. Expose the library setting as
`writer::cpp_options::constant_evaluation`, a bool defaulting to false.

The flag controls generated model capability; it is not a parameter stored in a
serialized object or checked on each runtime call. Existing default generation
shall retain its API declarations, model layout, and runtime paths. When true,
add the constexpr-capable positional traversal and compile-time capability metadata
needed by explicit size/write/factory calls. Such metadata shall require no runtime
storage or dynamic initialization. Existing runtime calls with an enabled model
shall still have exactly the baseline execution and memory behavior.

The installed constant-binary header provides the opt-in library APIs. A scalar
explicitly passed to those APIs need not have a generated-model flag; generated
models must come from appropriately enabled, eligible output for compile-time use.
An unavailable model capability shall produce a clear diagnostic when compile-time
use is requested, not a silent fallback to runtime initialization. A type marker
cannot prove that every value, lifetime, or custom operation is constant-valid.

Use one consistent generated definition of each qualified model across translation
units. Do not mix independently generated enabled and disabled definitions of the
same class, even if their member layouts match. CMake shall carry the selector
through its existing CONFIG-file route; no separate generation pipeline is needed.

### 9.2 Additive constant-evaluation traversal

With constant_evaluation = true, generated owning classes add a separate
constexpr `serialize_constant_out`(Protocol&) const method constrained to positional
binary protocols. They also add static constexpr bool serializer_constant_evaluation
set to true and a `serializer_constant_evaluation_type` alias naming that exact class.
The marker and alias require no object storage or runtime initialization.

The encoder selects the helper only for the class named by its alias, so a
handwritten derived class cannot silently inherit a traversal that ignores its
added fields. Recursively generated members must also come from eligible enabled
output. Custom serializers may instead supply a deterministic constexpr
serialize_out implementation satisfying the protocol concept.

For protocols = all, existing runtime serialize_out declarations and bodies are
unchanged. The generator does not blanket-mark the regular runtime method
constexpr or route runtime calls through the new helper. Shared compact, typed-magic,
and version helpers are constexpr-capable where needed without changing their
runtime algorithm. Other selected protocols retain their ordinary runtime codecs.

An enabled schema can still contain a runtime-only member such as std::map.
Enabling the flag does not make that construction/traversal a valid constant
expression. Failure is diagnosed when an unsupported constant call is evaluated;
its otherwise valid runtime codec remains available.

### 9.3 Independent positional-only format selector

The independent C++ protocol selector is:

```ini
[cpp]
protocols = binary_none
constant_evaluation = true
```

The command-line format selector is `--cpp.protocols binary_none`. Its default is
`all`; initial accepted values are only `all` and `binary_none`. It is independent
of the boolean activation parameter and distinct from the existing `[cpp] format`
option controlling clang-format. The library-facing cpp_options shall expose both
selectors with normal CLI-over-INI precedence.

| constant_evaluation | protocols | Generated behavior |
| --- | --- | --- |
| false | all | Existing runtime APIs and behavior |
| false | binary_none | Positional runtime APIs only; no generated constexpr capability |
| true | all | Constexpr-capable positional traversal plus existing other runtime protocols |
| true | binary_none | Positional APIs with compile-time size/write/factory capability |

Binary-only generation shall emit owning fields, required enum/union definitions,
and runtime positional readers. It shall emit the constexpr traversal only when
constant_evaluation is true. Omit generated JSON/keyed-binary entry points and
unused name/ID dispatch code where safe without changing positional bytes. Preserve
naming profiles, IDs, payload revisions and field layouts. A model need not become
a template over protocol selection.

An internal sink-templated traversal may share count/write behavior; reducing
protocol branches does not require eliminating every internal template. Schema
generic parameters retain their meaning. No .serializer syntax change is needed.

Generation rejects protocols = binary_none combined with protobuf = true,
managed models, or read-only/mutable view models. This rejection preserves those
features' requested semantics instead of silently dropping them. With protocols = all,
constant_evaluation = true can coexist with Protobuf generation; Protobuf, managed
operations, and views remain outside this constant-evaluation profile. Their otherwise
valid runtime generation is retained. Other language outputs and default C++ generation
remain compatible.

Every combination, including an enabled runtime call, remains subject to the zero
runtime execution/performance/memory-impact contract. Protocol restriction is a
separate opt-in and is not permission to regress the retained positional encoder.

### 9.4 Emission-only borrowed models

For a model used only to produce positional bytes, select the separate strict
boolean option `--cpp.emission_only true` or:

```ini
[cpp]
constant_evaluation = true
protocols = binary_none
emission_only = true
```

`emission_only` defaults to false. Enabling it requires
`constant_evaluation = true`, `protocols = binary_none`, and
`protobuf = false`; incompatible settings fail generation. The initial profile
rejects managed/view models, maps, nonempty array defaults and multidimensional
schema arrays. Those restrictions affect only explicitly selected emission output.

Emission declarations use a terminal `emission` namespace: for example,
`constant_example::payload` becomes `constant_example::emission::payload`.
References to other generated models use the same namespace, so emission and
ordinary owning headers from the same schema can coexist. The emission profile
reserves the generated name `emission` and rejects a namespace or declaration that
would collide with it. Strings become
`std::string_view`; one-dimensional arrays become `std::span<const T>`,
including fixed schema arrays. Fixed spans use dynamic extent for default construction;
the emitted active-field traversal validates the declared fixed count before encoding.
Text/count framing and every retained wire value match the owning schema contract.

Serializer tests verify that supported borrowed representations and generated
records/unions are trivially copyable. Generated headers do not repeat
`is_trivially_copyable_v` assertions for each field. Encoding traverses logical
fields and never copies the DTO object representation. Native generic type
arguments retain their supplied C++ representation and are not required to be
trivially copyable merely to emit bytes. The separate raw-union destruction
constraints remain in force.

Each emission DTO exposes one concrete output method:

```cpp
constexpr void serialize_out(rohit::serializer::binary_none_output& output) const;
```

This model method is not a template. Its generated header includes
`<rohit/constant_binary_output.hpp>` and shared binary/version/UTF-8 primitives.
That include closure omits the full runtime serializer, stream/file I/O, compression,
decoding machinery and JSON parser. The complete owning API remains available from
`<rohit/constant_binary.hpp>`. Shared primitive definitions retain their existing
namespace, wire values and runtime bodies. This reduces the emission include closure;
compile-time and memory improvements require measurements of the actual application.

There is no emitted `serialize_constant_out`,
reader, stream/static codec convenience or JSON/keyed dispatch API. Static
`serializer_emission_only`, constant-evaluation metadata and the exact-type alias
identify explicit API eligibility without object storage. Schema strings and arrays
borrow their variable payload storage, which must outlive the synchronous traversal.

The public concrete `binary_none_output` defaults to count mode; constructing it
with a writable `span<uint8_t>` selects memory mode. Its `position_bytes()`
returns committed bytes and `counting()` identifies mode. Both modes use the same
strict little-endian protocol. It is noncopyable because its internal references
borrow the enclosing writer. Count mode validates fields and counts fixed-width
batches/scalar arrays directly without a dummy output buffer or element loop.

The three public APIs recognize an exact-type emission marker and select this
concrete writer for counting/emission. Explicit runtime calls for emission DTOs or
standalone source spans use this additive path; ordinary owning runtime calls retain
existing optimized encoder delegation. No emission profile is selected implicitly.

A fixed-storage owner can safely create the borrowed model when output is requested:

```cpp
#include <rohit/constant_binary.hpp>
#include "payload.hpp" // Generated with emission_only = true from the schema in section 11.

#include <array>
#include <cstdint>

// Own construction storage; create views only while this owner's buffers remain alive.
struct payload_owner {
  static constexpr bool serializer_emission_only = true;
  static constexpr bool serializer_constant_evaluation = true;
  using serializer_constant_evaluation_type = payload_owner;

  std::array<char, 80> label{};
  std::array<std::uint8_t, 3> data{0, 128, 255};

  // Initialize inline storage without string/vector allocation.
  constexpr payload_owner() { label.fill('a'); }

  // Borrow this owner's buffers for one synchronous concrete-writer traversal.
  constexpr void serialize_out(rohit::serializer::binary_none_output& output) const {
    constant_example::emission::payload value{};
    value.sequence = 0x01020304;
    value.label = {label.data(), label.size()};
    value.data = data;
    value.serialize_out(output);
  }
};

constexpr auto owner_factory = [] { return payload_owner{}; };
constexpr auto bytes = rohit::serializer::make_binary_none_bytes<owner_factory>();
static_assert(bytes.size() == 90);
```

The owner stores arrays and recreates views inside its method; copying/returning it
never preserves a pointer into a previous owner instance. Returning a DTO whose
spans point to destroyed factory-local arrays remains invalid. A string literal or
other suitable immutable storage may also supply a borrowed field if its lifetime
covers both passes. The ordinary owning reader can decode the emitted bytes; the
emission header intentionally supplies no reader.

## 10. Validation, errors and failure behavior

Both operations shall validate strict text, enum identities, union discriminators,
compact limits/policies, selected payload revision, and arithmetic bounds before
using dependent extents. Values that are illegal for the existing
strict positional encoder remain illegal in constant evaluation. Source validity
and lifetime, source/destination independence, and correctly initialized active
union lifetime are caller preconditions. Runtime codecs cannot generally detect
dangling pointers or a valid tag attached to an inactive member; the constant
evaluator diagnoses invalid operations it evaluates.

At runtime, preserve existing exception categories for equivalent codec failures.
New helper-specific failures shall distinguish unrepresentable encoded size,
insufficient destination capacity, and count/write mismatch. Do not add noexcept
where a validation failure can occur.

During required constant evaluation, a failing validation shall make the expression
nonconstant and compilation fail. An evaluated throw or diagnostic helper can
provide that failure; do not depend on constexpr exception handling from newer
language baselines. Runtime diagnostic construction may remain in an ordinary
helper reached only outside successful constant evaluation.

Array capacity and cursor checks are independent of successful prior counting.
Counting an unchanged value is not permission for unchecked writes. The exact-array
factory shall reject a final cursor different from its separately calculated size.
A failed immediate factory shall not publish a partially initialized result.

Custom traversal is subject to documented purity/lifetime preconditions; this
feature does not promise transaction rollback for arbitrary user callbacks that
throw or change state during the second pass.

## 11. End-to-end usage

The following example uses the implemented APIs and existing schema syntax.
Generate the model with compiler/runtime release 1.4.0 and enable its constant
traversal before compiling the C++ example.

```text
serializer version 1.2.0;

namespace constant_example {
  class payload {
    public uint32 sequence;
    public string label;
    public array uint8 data;
  }
}
```

Generate the owning header through the usual
[CMake workflow](cmake_integration.md), passing a configuration with
`[cpp] constant_evaluation = true`, or use the compiler parameter shown in section 9.
Select protocols=binary_none separately when a positional-only model is wanted.
Do not manually reproduce the generated codec for qualification.

```cpp
#include <rohit/constant_binary.hpp>
#include "payload.hpp"             // Generated from the schema above.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Create a transient model; the long label exercises allocation beyond common SSO limits.
constexpr auto make_payload = [] {
  constant_example::payload value{};
  value.sequence = UINT32_C(0x01020304);
  value.label = std::string(80, 'a');
  value.data = {0, 128, 255};
  return value;
};

constexpr auto bytes =
    rohit::serializer::make_binary_none_bytes<make_payload>();

// Four scalar bytes, two length-prefix bytes, 80 text bytes, one count byte, three data bytes.
static_assert(bytes.size() == 90);
static_assert(bytes[0] == 0x04 && bytes[3] == 0x01);
static_assert(bytes[4] == 0x40 && bytes[5] == 0x50);
static_assert(bytes[86] == 3);
static_assert(bytes[87] == 0 && bytes[88] == 128 && bytes[89] == 255);
```

A runtime caller can use the same model with an explicitly chosen two-pass workflow:

```cpp
// Size first, allocate once, then serialize into that caller-owned memory.
void write_runtime_example() {
  const auto value = make_payload();
  const auto required = rohit::serializer::binary_none_size(value);
  std::vector<std::uint8_t> memory(required);
  const auto written = rohit::serializer::serialize_binary_none_to(
      std::span<std::uint8_t>{memory}, value);
  if (written != required) {
    throw "binary_none count/write mismatch";
  }
}
```

Decode the complete output using the existing runtime positional reader and the
same schema, with explicit decode limits and exact-message/trailing-byte checking
as described in [the usage guide](usage.md). No constexpr reader is required.

## 12. Zero runtime impact and compilation efficiency

The implemented separation of runtime and constant paths is described in section 15.
Section 17 identifies completed qualification. The requirements below remain the
regression contract; their presence is not a claim that every throughput, debug-mode,
allocation, stack, or code-size scenario has passed. Enabling an additional generated
helper and evaluating an explicit two-pass factory can add compilation work. No
universal zero-build-cost or compile-time speedup claim is made.

Zero runtime execution/performance and memory impact is a mandatory acceptance
condition, including when `--cpp.constant_evaluation true` is enabled. The flag
shall change generated compile-time capability only. It shall not select a slower
runtime encoder, suppress bulk/SIMD operations, or turn runtime copies into byte
loops simply because the model can also be evaluated during translation.

For equivalent existing runtime serialization calls and configuration, require:

- The same wire bytes, field order, validation, exceptions and publication behavior.
- The same model sizeof/alignment, stack/heap requirements, allocation count and
  allocated bytes, buffer capacity/growth, reservations, and retained state.
- No runtime evaluation-mode flag, additional branch/dispatch, virtual call, extra
  traversal, temporary output buffer, cached count, static table or runtime initializer.
- No extra runtime code/data emitted solely because the generator flag was enabled;
  compile-time helpers should not produce unused out-of-line runtime artifacts.
- Unchanged optimized memory-copy, alias/memmove, endian-conversion and SIMD paths.

Explicit invocation of the new size API or creation of a requested byte array is
new application work, rather than an implicit effect of enabling the flag. Enabling
it alone shall not embed an additional payload or retain the temporary model.
Existing runtime serialize calls shall remain one-pass regardless of the option.

Preserve the established runtime memcpy/memmove path when a standard algorithm
would alter runtime machine code or cost. Constant-evaluation selection must be
resolved by compilation/type specialization, with no surviving per-call mode test.
No std::copy optimization assumption is a substitute for the required evidence.

Compare flag-enabled and flag-disabled generated models with otherwise identical
schemas, values, protocol selections, compiler/library versions and build flags.
Check generated/layout differences, allocation and reservation instrumentation,
stack/code/data footprint, optimized instructions, and representative throughput.
Use supported configurations, including relevant iterator-checking/debug modes.
Assembly comparison alone or a noisy benchmark alone does not prove zero impact.
Any observed regression blocks acceptance until the implementation preserves the
baseline. Do not advertise this requirement as verified before qualification.

The explicit compile-time factory performs two field traversals and returns only
final wire bytes. It shall not allocate maximum-size scratch output for counting.
Record compiler time and constexpr evaluation steps separately for small, nested
and large models before claiming a compile-time improvement.

The final exact-array type intentionally depends on byte count; intermediate
literal/formatter fields need not become length-parameterized templates. No pointer
or ownership from transient model storage may appear in the returned bytes.

## 13. Implementation structure

1. include/rohit/constant_binary_output.hpp supplies the count/fixed-memory sinks,
   additive constexpr encoder, concrete output protocol, count/factory APIs and
   emission/span memory overload. include/rohit/constant_binary.hpp re-exports it
   and adds the existing optimized owning runtime-memory overload.
2. Count mode validates semantic values and uses checked byte arithmetic without
   acquiring output memory or passing fake pointers to runtime batch operations.
3. Constant output uses checked std::copy_n operations over typed raw pointers and
   explicit scalar encoding. Contiguous uint8 pools use one complete append; exact
   uint8 scalar fields use the one-byte operation. Owning runtime existing-memory
   output delegates to the established optimized protocol, with no hidden size pass.
4. The C++ generator emits the additive traversal only when explicitly enabled,
   and independently restricts output when protocols = binary_none is selected.
5. Small shared binary_scalar, binary_output_support and UTF-8 headers retain one
   definition of each existing primitive for owning and emission paths. The regular
   runtime headers include them without changing their codec bodies.
6. Actual owning/emission schema fixtures, runtime parity tests, and negative
   constant-expression checks qualify the two generated profiles.

The feature is additive in compiler/runtime release 1.4.0. The schema language
remains 1.2.0; payload revisions and existing wire encodings do not change merely
because constant evaluation is enabled. Broader qualification targets are retained
below, with completed evidence distinguished in section 17.

## 14. Required verification and acceptance criteria

Use the matrix below for continuing regression and full acceptance qualification
with the actual schema compiler, generated headers, and matching runtime. Section 17
records the completed subset; unlisted performance scenarios remain unverified.
Handwritten lookalike model codecs do not establish generator support.

| Area | Required verification |
| --- | --- |
| Language baseline | Successful C++20 constant size and write, with optional C++23 coverage |
| Byte identity | Static assertions against independent positional fixtures; equality with runtime output |
| Runtime reading | Existing positional reader decodes constant-produced bytes and verifies values |
| Fixed scalars | All supported integer widths, extrema/negatives, bool/char, float/double including signed zero and permitted bit patterns |
| Compact prefixes | Below/at/above transitions 0x3f/0x40, 0x3fff/0x4000 and 0x3fffff/0x400000; maximum 0x3fffffff, shortest writes and overflow rejection |
| Compact annotations | Prefix/varint boundaries, strict rejection and lenient truncation parity |
| Text | Empty, ASCII, multibyte UTF-8, embedded NUL and invalid/truncated/overlong/surrogate text |
| Collections | Empty/nonempty vectors, fixed arrays with retained count, nested text/object arrays |
| Transient allocation | Long string and nonempty vector factories; no escaping allocation or dangling view |
| Objects | Empty, nested, inherited and concrete generic/dimension models with schema-order parity |
| Unions/enums | Each legal active alternative and enum, invalid tag/value rejection, documented lifetime requirements |
| Magic/versioning | Raw/typed magic, omissions, current/historical revisions and invalid revision behavior |
| Count correctness | Count equals cursor and extent; no output allocation/fake pointer; all semantic validation retained |
| Batch behavior | Runs of 1, 16 and 17 fixed fields; correct reserve/commit accounting and atomic failed batch |
| Existing storage | Empty/exact/oversized/one-byte-short spans, nonzero subspan, surrounding sentinel protection |
| Arithmetic | Checked addition/multiplication, protocol count limits and platform size_t bounds |
| Negative translation | Invalid values, unsupported constant containers, ordinary-parameter extent, invalid storage lifetime and nonconstexpr customization |
| Compiler activation | Default false; true/false validation; CLI-over-INI overrides; enabled/disabled and all/positional matrix; consistent generated definitions |
| Binary-only generation | Actual restricted output, matching runtime reader, unchanged default generation and conflict diagnostics |
| Runtime regression | Flag enabled and disabled: existing suites, identical bytes/layouts/validation, no extra runtime mode dispatch, traversal or state |
| Performance and memory | Enabled/disabled CPU, stack/heap, allocation bytes/counts, reservations, buffer growth and code/data footprint retain baseline; compile-time cost measured separately |
| Toolchains | Supported GCC, Clang, MSVC and clang-cl C++20 compiler/standard-library combinations |

Compile-failure checks shall use isolated expected-failure translation units and
assert the relevant diagnostic class without pinning whole compiler messages.
Bounds tests shall verify the promised prefix/batch failure behavior rather than
assuming whole-message rollback. Valid-value tests shall compare semantically
independent expected bytes, not only count and write from the same implementation.

A qualification report shall identify source revision, compiler and standard
library versions, selected profile, schema-generation command, positive and negative
cases, runtime decoding results, and omitted checks. Do not infer all-toolchain
support from one successful compiler or language-only toy example.

## 15. Runtime compatibility and implementation boundaries

The ordinary `binary_out_base` constructor and output methods in
[serializer.hpp](../include/rohit/serializer.hpp) remain nonconstexpr. Its fixed-scalar,
native-array, bulk-copy, and SIMD paths retain their established runtime behavior.
[stream.hpp](../include/rohit/stream.hpp) continues to provide the existing general
runtime streams, including their pointer, alias/rebasing, and copying operations.

The new [constant_binary.hpp](../include/rohit/constant_binary.hpp) does not replace
those facilities. During constant evaluation, `serialize_binary_none_to` uses the
borrowed typed memory sink. At runtime it delegates directly to an ordinary helper
that constructs `full_stream` over the destination and calls the existing strict,
little-endian binary_none encoder for ordinary owning values. Emission-only models
and standalone source spans instead use the explicit concrete writer from section 9.4.
Neither writing path performs an implicit count pass. The explicit
`binary_none_size` operation uses the count protocol in either execution mode.

For protocols = all, enabling the generator flag adds only the static marker,
exact-type alias, and separate constant traversal. The regular generated runtime
traversal stays unchanged. Changing to the independent positional-only profile
intentionally removes JSON/keyed/protobuf capability; it is a distinct output choice,
not a change silently caused by the constant-evaluation flag.

This implementation structure protects the existing hot paths. Actual emitted-code,
layout, allocation, reservation, and throughput evidence must still be stated for its
specific tested configurations; source separation alone does not establish every
performance item in section 14.

## 16. Historical language-design probes

### 16.1 Function selection and storage lifetime

On 2026-10-07, isolated language-level probes were compiled with MSVC 19.51.36257
for x64, the MSVC standard library, and Windows SDK 10.0.26100.0. C++20 probes used
`/std:c++20 /permissive- /Zc:__cplusplus /EHsc /W4 /WX`.

The positive probe used one templated field traversal with a counting sink and a
fixed-memory sink. An NTTP factory created a transient 80-character std::string
and a vector of three uint16 values, and returned an exact 94-byte owning array.
Compile-time byte/extent checks and runtime checks passed. Ordinary
`if (std::is_constant_evaluated())` selected the correct constant/runtime paths.
An immediate helper also successfully wrote storage created inside its enclosing
immediate factory.

| Negative C++20 probe | Observed result |
| --- | --- |
| Otherwise identical constexpr/consteval declarations | Rejected, C2475 |
| Ordinary function parameter used as array template extent | Rejected, C2975 |
| Immediate write into a previously created runtime buffer | Rejected, C7595 |
| Write into previously existing mutable static storage | Rejected, C2131 |
| Parameter-dependent immediate call behind ordinary evaluation query | Rejected, C7595 |

An additional `/std:c++23preview` probe used `if consteval` with an immediate
helper receiving the surrounding function parameter; compile-time and runtime
checks passed. It is supplementary evidence and does not change the C++20 baseline.

These probes validate the language and memory-lifetime design on that compiler.
They did not use new Serializer APIs or newly generated constexpr codecs, did not
qualify all listed compiler/library combinations, and do not establish completion
of section 14. Temporary probe sources and binaries were removed; a small local
verification log remains in the ignored build directory. Required implementation
qualification must use actual generated Serializer models and independent fixtures.

### 16.2 Standard copying algorithms and runtime dispatch

Separate isolated probes used the same MSVC/compiler-library combination with
C++20 and /O2. Compile-time std::copy and std::copy_n over typed byte arrays passed
static assertions. Copying bytes obtained through scalar reinterpret_cast was
rejected during constant evaluation; bit_cast to an owning byte array followed by
std::copy passed. Runtime valid-buffer byte-equality checks also passed.

| Probe wrapper | Fixed 16-byte generated instructions | Dynamic-length generated instructions |
| --- | --- | --- |
| Direct std::copy over byte pointers | Tail-call memmove | Tail-call memmove |
| Direct memcpy | Inline movups load/store, then return | Tail-call memcpy |
| Ordinary evaluation-query dispatch: constexpr std::copy, runtime memcpy | Identical to direct memcpy wrapper | Identical to direct memcpy wrapper |

Direct std::copy_n and char-to-uint8 element-copy wrappers also used memmove in
this configuration. These observations show that one library/compiler setup does
not necessarily generate identical runtime code for the standard algorithms and
memcpy. They are not a throughput benchmark or evidence that either operation is
universally faster. The dispatch wrapper preserved the direct memcpy instructions
in this probe; actual Serializer enabled/disabled qualification is recorded separately in section 17.

These preliminary probes preceded the Serializer implementation and did not use
its generated codecs.
Temporary sources, assembly and binaries were removed; the small result log remains
in the ignored build directory.

## 17. Implemented feature qualification (2026-10-07)

The 1.4.0 implementation working tree was tested with headers emitted by the actual
schema compiler from test/resources/constant_evaluation.serializer and
constant_evaluation.ini. The configured profile was
constant_evaluation = true, protocols = all; language mode was C++20. The generated
fixtures exercise transient 80-character string and nonempty vector storage,
exact-array emission, nested/generated class unions, inheritance, fixed dimensions,
compact encodings, raw/typed magic, omissions, and payload revisions. Static assertions
include an independent 90-byte positional payload fixture. Existing runtime readers
decode emitted bytes, and runtime tests compare count/write results with the ordinary
encoder.

| Compiler and standard library | Completed feature qualification |
| --- | --- |
| MSVC 19.51.36257 x64 with MSVC 14.51 standard library and Windows SDK 10.0.26100.0 | Actual fixture generation/build; all static assertions; 8 runtime tests; 9 expected compile-failure cases |
| GCC 15.2.0 with libstdc++ on Ubuntu WSL2 | Actual fixture generation/build; all static assertions; 8 runtime tests; 9 expected compile-failure cases |
| Clang 21.1.8 with libstdc++ on Ubuntu WSL2 | Actual fixture generation/build; all static assertions; 8 runtime tests; 9 expected compile-failure cases |
| clang-cl 21.1.8 targeting x64 Windows, MSVC 14.51 headers, Windows SDK 10.0.26100.0 | Actual generated fixture compiled with /std:c++20 /W4 /WX, linked to native runtime/test libraries, and passed all static assertions and 8 runtime tests |

MSVC, GCC and Clang CTest runs each passed constant_binary_test and
constant_binary_negative. The negative translation cases cover invalid UTF-8,
invalid enum/union identities, strict compact overflow, invalid payload revision,
C++20 map construction, dangling storage, ordinary-parameter template extents,
and escaping allocation. A valid control translation is compiled before expected
failures are accepted. The clang-cl run qualified the positive fixture only;
its compile-failure matrix was not run in that configuration.

The default-off and enabled versions of the actual runtime fixture
[runtime_equivalence.serializer](../test/resources/runtime_equivalence.serializer)
were generated by the 1.4.0 compiler and compiled from the same
[runtime_equivalence_codegen.cpp](../test/runtime_equivalence_codegen.cpp) wrapper.
With MSVC 19.51.36257.0 x64, complete emitted code/data matched for Release
(/O2 /Ob2) and Debug (/Od /Ob0), after removing file/line comments and canonicalizing
local compiler label names. The comparison included emitted helpers, not only the
exported wrapper. This is evidence for those specific compiler/fixture configurations;
it is not an exhaustive throughput, allocation, debug-iterator or footprint benchmark.

These results establish the stated compiler/library combinations, not every C++20
implementation or standard-library release. They do not establish every performance,
debug-iterator, footprint, or compilation-cost scenario in section 14. Those require
separate controlled measurements; no universal runtime or build-cost conclusion is
inferred from the compiler matrix.

### 17.1 Emission-only fixture qualification

The actual compiler generated both `test/resources/emission_only.serializer`
profiles into separate output directories: default owning output and the
`emission_only.ini` concrete-output profile. The same translation unit includes
both headers. Static assertions verify the model's non-template concrete method
signature, borrowed string/span field types, trivial storage, inline-owner 90-byte
output and independent scalar-span/union fixtures. The initial four runtime tests decode
emitted output with the existing owning reader and compare its unchanged writer's bytes.
They cover nested borrowed object/string spans, fixed extents, selected class-union
lifetime, magic, historical revision selection and compact fields.

GCC 15.2/libstdc++ and Clang 21.1.8/libstdc++ each passed the actual generated
C++20 build, all static assertions,
all four runtime tests and nine expected constant-expression failures. The failures
cover fixed extent, dangling text/span storage, invalid UTF-8/enum/union/revision,
strict compact overflow and short destination capacity, following a valid control.
clang-cl 21.1.8/MSVC headers compiled the actual generated fixture with /W4 /WX,
linked the native libraries, and passed all four runtime tests/static assertions;
its emission negative matrix was not run. On 2026-10-08, MSVC 19.51's native
fixture built with /W4 /WX and passed all four runtime tests/static assertions
and all nine expected constant-expression failures after a valid control.

The native Serializer suite registered 50 CTests: 48 passed in the complete run,
and the two remaining checks passed focused reruns after correcting an invalid
schema fixture and the clang-format discovery path. The core compiler/runtime
test executable passed all 359 tests. This qualifies all 50 registered native
checks for the tested configuration; it does not establish zero include-parsing
or compile-time cost, universal runtime performance, or all-toolchain heap
instrumentation results.


### 17.2 Lean output support qualification (2026-10-08)

After the shared-header extraction and octet/native-scalar optimizations, GCC
15.2/libstdc++ and Clang 21.1.8/libstdc++ each rebuilt headers with the actual schema
compiler and passed all four profile CTests: eight owning runtime tests, five emission
runtime tests, and nine expected constexpr failures after a valid control for each
profile. The emission failure/control translation units include only their actual
generated header. The additional runtime test checks a 65-byte contiguous pool
against the existing owning writer, guarded subspans, complete-payload reservation
failure and empty-span framing. Scalar extrema compare the concrete writer with the
unchanged owning runtime writer; signed minimum and unsigned maximum also have
independent static byte assertions.

The final native MSVC 19.51 build passed the complete 50-test CTest run after the
shared-header extraction and lookup repair, including both positive profile fixtures,
both nine-case negative matrices after valid controls, and the enabled/disabled
runtime-equivalence check. The core compiler/runtime executable passed all 359
cases. The complete native CTest run finished in 63.55 seconds; this is test-suite
execution time, not a compiler-performance measurement.

clang-cl 21.1.8 targeting x64 Windows with MSVC 14.51/Windows SDK 10.0.26100.0
headers compiled both actual fixture sources and all static assertions with
/std:c++20 /W4 /WX. GTest headers were treated as external system headers. This
post-extraction check compiled objects; it did not rerun clang-cl runtime binaries
or its negative matrix.

The GCC compiler's dependency output for the real emission control contains the
generated header, constant_binary_output.hpp, binary_output_support.hpp,
binary_scalar.hpp, versioning.hpp, utf8.hpp and runtime_simd.hpp. It contains no
serializer.hpp, stream.hpp, stream_io.hpp, decode.hpp, compression.hpp or
json_text.hpp. This verifies the tested generated include closure. The retained
runtime SIMD header supplies declarations and small wrappers; it does not include
intrinsics headers. Compile-time and peak-memory results remain separate measured
acceptance criteria, not consequences inferred from the smaller include list.
