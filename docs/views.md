# Generated owning objects and binary views

Examples here use the default Serializer naming profile. Other
[C++ output profiles](output_configuration.md) rename schema types and field
accessors, while `map`, storage-mode values, and inherited runtime APIs keep
their established spellings. The profile does not change the mapped wire layout.

The default C++ suite, including generated view tests, passes on Linux x64 with
GCC and Clang and Windows x64 with MSVC. Native macOS/Android/ARM Linux validation
and performance measurements remain pending.

## Select representations in the schema

```text
serializer version 1;

namespace example {
  class person view owning {
    public string name;
    public uint32 age;
    public array uint16 scores;
  }
}
```

The attributes after a class name form an unordered set:

- No representation attributes, or `owning` alone: generate only an owning class.
- `view`: generate read-only and mutable views.
- `view readonly`: generate only a read-only view.
- `view mutable`: generate only a mutable view.
- `view readonly mutable`: generate both views, just like `view`.
- Add `owning` to any view selection to generate the owning representation too.
- `readonly` and `mutable` require `view`. Repeated attributes are idempotent.
- `stable_ids` remains independent. `packed` with `view` is rejected because
  mapped objects contain pointers and offsets rather than native wire-layout fields.

One mode generates a concrete `class person`. Multiple modes generate a class
template with explicit specializations for the requested `storage_mode` values:
`owning`, `read_only_view`, and `mutable_view`. There is no default template mode.
An unsupported mode has no definition and cannot be constructed.

## Encode, map, read, and update

For the schema above, the following C++ code uses all three representations:

```cpp
#include "person.hpp"
#include <bit>
#include <cstdint>
#include <span>

using rohit::serializer::storage_mode;
using owning_person = example::person<storage_mode::owning>;
using readonly_person = example::person<storage_mode::read_only_view>;
using mutable_person = example::person<storage_mode::mutable_view>;

// Encode an owning value, then edit the existing positional message through a view.
void update_person() {
  owning_person value{};
  value.name = "Rohit";
  value.age = 30;
  value.scores = {100, 200};

  rohit::full_stream_auto_alloc output{128};
  value.serialize_out<rohit::serializer::binary_none>(output);
  auto bytes = std::span<std::uint8_t>{output.begin(), output.current_offset()};

  auto editor = mutable_person::map(bytes);
  editor.set_age(31);
  editor.set_name("Alice"); // Same five-byte payload.
  editor.get_scores().set(1, std::uint16_t{250});

  auto reader = readonly_person::map(std::span<const std::uint8_t>{bytes});
  const auto age = reader.get_age();        // uint32_t, by value.
  const auto name = reader.get_name();      // std::string_view, borrowing bytes.
  const auto score = reader.get_scores().at(1);
  static_cast<void>(age);
  static_cast<void>(name);
  static_cast<void>(score);
  static_assert(readonly_person::wire_endian == std::endian::little);

  // The edited bytes are already serialized. Copy them only when another buffer is needed.
  rohit::full_stream_auto_alloc destination{128};
  editor.serialize_out(destination);
}
```

For `class person view readonly`, use `example::person::map(bytes)` directly.
Its generated C++ class has no mode template parameter and no setters.
Mutable views accept `std::span<std::uint8_t>`; read-only views accept
`std::span<const std::uint8_t>`. A read-only span cannot create a mutable view.

Owning classes retain their ordinary fields and all existing codec choices.
View-only classes have no owning fields and no default constructor or builder.
Map a complete encoded message, produced by an owning representation or another
application that follows the same wire contract.

## Wire protocol and byte order

Views currently map exactly one format: **little-endian positional binary**.
Their static constants are `wire_endian == std::endian::little` and
`key_type == serialize_key_type::none`. The fixed-width byte order matches the
default `binary_none`, `binary_integer`, and `binary_string` codecs. Compact
prefixes retain their separately defined encoding.

View output accepts a stream, a little-endian positional output protocol, or
`serialize_out<binary_none>(stream)`. It copies the existing message bytes;
it does not convert them to JSON, keyed binary, or another byte order.
Mapping JSON, keyed binary, or big-endian buffers is unsupported. Because messages
contain no protocol/endian marker, mapping cannot reliably detect a wrong choice.
Agree on the format outside the message; see [byte order](wire_format.md#byte-order).

Accessors load numeric values by copying scalar bits and converting byte order
where necessary. They never expose an unaligned native integer/float reference.
Strings borrow payload bytes; no complete owning object is constructed.

## Access and mutation contract

| Field | Getter result | Mutable update |
| --- | --- | --- |
| Integer, float, bool, char | Value | `set_field(value)` |
| Enum | Validated enum value | `set_field(value)` if compact width is unchanged |
| String | `std::string_view` | `set_field(text)` with identical byte length |
| Class | Nested view | Use nested field setters |
| Array | Borrowed sequence with `size()`, `at(index)`, and `begin()`/`end()` | `set(index, value)` or iterator `set(value)` for scalar/string elements; otherwise use nested views |
| Map | Encoded entries with `size()`, `key_at(index)`, `value_at(index)`, and `begin()`/`end()` | `set_value(index, value)` or iterator `set_value(value)` for scalar/string values; otherwise use nested views |
| Union | Active alternative with `index()` and `get<Index>()` | `set<Index>(value)` for the active scalar/string alternative |

Unions also expose `e_field` and `get_field_type()` for symbolic discriminator
names. Selecting an inactive union alternative throws `std::invalid_argument`.
Invalid enum values are rejected. A size-changing scalar/string replacement
throws `std::length_error` before changing bytes. Nested object replacement,
array/map insertion, and switching union alternatives require rebuilding the
message; no resizing setter is generated.

Map keys are read-only, including keys that are nested views. Map views expose
encoded entries in wire order, including duplicates. They do not construct or
normalize a `std::map`; an owning decode applies its usual duplicate-key rules.
Editing one encoded duplicate only changes that entry.

Field accessors retain the schema's public/protected/private access. Parent
objects are exposed through `get_base_ID()`, using the parent's field ID, with
the declared parent access. Views use composition for these parent buffers.

## Sequential collection traversal

Array and map views provide C++20 forward iterators. Prefer a range-based loop when
visiting every entry, especially for strings, compact enums, and nested objects:

```cpp
std::uint64_t total{};
for (const auto score : reader.get_scores()) {
  total += score;
}

auto scores = editor.get_scores();
for (auto iterator = scores.begin(); iterator != scores.end(); ++iterator) {
  iterator.set(std::uint16_t{250});
}
```

For a `map(uint32) string` field named `labels`, each dereference returns a
key/value pair. The following updates every matching value, including duplicates:

```cpp
auto labels = editor.get_labels();
for (auto iterator = labels.begin(); iterator != labels.end(); ++iterator) {
  const auto [key, text] = *iterator;
  if (key == 1 && text == "one") {
    iterator.set_value("ONE");
  }
}

for (const auto [key, text] : labels) {
  // Read each encoded entry in wire order.
  static_cast<void>(key);
  static_cast<void>(text);
}
```

- Array dereference returns a scalar, borrowed string, or nested view by value.
  Map dereference returns a pair with read-only key access and a value using the
  collection's mutability. Use `auto child` when editing a returned nested view.
  Scalar/string updates use the iterator setters; assigning a dereferenced copy
  does not update the encoded value.
- Copies advance independently. Prefix/postfix increment and equality comparison
  support multiple passes and C++20 range algorithms. `end()` does not scan entries.
  Dereferencing, incrementing, or setting an end/default iterator throws
  `std::out_of_range`. Iterators do not provide random-access arithmetic.
- Iterators keep spans, limits, and current boundaries inline without heap allocation.
  Fixed-width entries advance by their encoded width. Variable-width entries scan
  only the next entry; a complete pass discovers each entry's boundaries once.
  Nested object dereferences still validate that subobject to construct its offsets.
  Repeated indexed access to variable-width entries retains its existing scan cost.
- The complete message is validated under the shared decode limits during mapping.
  Subsequent entry scans stay bounded by the mapped bytes and retain the original
  limit policy. No additional schema attribute or generation option is required.
- An iterator can outlive a temporary collection wrapper because it retains its
  own span and limits. The buffer owner must still outlive the iterator and every
  borrowed result. Size-preserving setters keep iterators valid, and subsequent
  dereferences observe changes through other aliases. Buffer relocation or layout
  changes invalidate them. Constness of a wrapper does not remove the mutability
  granted by its byte span; use a read-only mapping for read-only access.

`test/binary_view_iterator_test.cpp` prepares iterator/range concept checks, scan-count
checks for linear traversal, independent copies, unaligned input and empty collections,
zero-byte objects, variable compact widths, duplicate keys, nested views, mapping
limits, and mutation failures. Compilation and execution remain deferred.

## Nested types and application boundaries

Each nested class and parent must enable every representation required by its
containing class. For example, a mutable parent view requires a mutable child
view; an owning parent requires an owning child. Map-key classes require a
read-only view even when the map is mutable. Unsupported combinations are schema
errors rather than silent conversions or additional generated modes.

Declare referenced classes/enums before their users. The generator resolves
qualified names, enclosing namespaces, global names, parents, and map keys before
selecting nested specializations. Owning raw unions retain their existing
trivially-destructible-alternative requirement; view-only unions have no native
raw union storage.

Different applications can choose different representations while agreeing on
the same field order, types, and byte order. Within a linked C++ program, use
consistent definitions for each qualified generated name. Switching a schema
from one mode to multiple modes changes `person` to `person<mode>` at call sites.

## Lifetime, validation, and cost

- `map(bytes, limits)` validates the complete message, enforces decode limits,
  rejects trailing data, and records each field's byte offsets before returning.
  An empty schema maps an empty span. No heap allocation is needed for mapping;
  the view stores a span, inline offsets, and its limits.
- Mapping shares depth/work/input budgets throughout the nested message. It does
  not charge owning string/container allocations because it creates none.
- Scalar and string field getters use generated constant slots, without runtime
  reflection or key lookup. Fixed-width array elements and fixed-width map
  entries use direct indexed offsets. Variable-width entries require bounded
  traversal of preceding entries on each indexed access; sequential iterators
  continue from the current entry's boundary. A nested object getter
  validates that subobject to construct its own offset cache.
- The owner must keep the buffer alive and at the same address. Do not resize,
  reset, replace, or directly alter its layout while any view, string view, or
  collection view or iterator is live. Generated setters preserve layout, so existing views
  remain usable. Overlapping string replacements are supported.
- Read-only means that this view cannot write. Another mutable alias can update
  the observed values. Synchronization between threads is the caller's responsibility.
- `serialized_bytes()` returns a read-only span of the exact message. Appending
  a mapped view through a little-endian `binary_none` encoder's
  `serialize_out(view)` copies these same bytes using the stream's reservation policy.
  JSON, keyed binary, and big-endian output remain unsupported for mapped views.
  Appending a view into its own growable owner can invalidate that view if storage moves;
  create a new mapping before accessing it again.
- Single-mode classes avoid a class-mode template. Multi-mode headers contain
  concrete specializations for each selected mode, all of which must be parsed
  and checked when included. Shared codec helpers still use templates. Selecting
  fewer modes reduces emitted source; it does not establish a measured compile-time
  or runtime improvement. Owning-only headers do not include the view helper header.

For integration steps, see [usage.md](usage.md) and the
[repository integration skill](../.agents/skills/serializer-integration/SKILL.md).
