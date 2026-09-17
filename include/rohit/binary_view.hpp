#pragma once

#include <rohit/serializer.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rohit::serializer::detail {

// Validate positional bytes using the same limits and scalar rules as owning decoding.
template <rohit::type_check::input_buffer Stream = stream>
class basic_view_scanner : public binary_none<serialize_type::in, Stream> {
  using base = binary_none<serialize_type::in, Stream>;
  using base::check_collection;
  using base::limits;
  const std::size_t initial_size;
public:
  using base::get_stream;
  using base::serialize_in_variable;
  // Share one decoding session throughout the complete nested message scan.
  basic_view_scanner(const Stream& input, decode_limits input_limits)
      : base{input, input_limits}, initial_size{input.remaining_buffer()} {}
  // Return a byte offset without subtracting potentially null pointers.
  std::size_t position() const {
    return initial_size - get_stream().remaining_buffer();
  }
  // Bound traversal of borrowed elements; no owning collection allocation is charged.
  std::size_t collection_size() {
    const auto count = serialize_in_variable();
    check_collection(count, 0);
    return count;
  }
  // Reject an invalid discriminator using the session's bounded diagnostics.
  [[noreturn]] void bad_union() const {
    throw exception::bad_input_data{get_stream(), "Invalid union discriminator", limits.diagnostics};
  }
};

using view_scanner = basic_view_scanner<>;

// Read a compact prefix from bytes already validated by a view scanner.
inline std::pair<std::uint32_t, std::size_t> view_compact(std::span<const std::uint8_t> bytes) {
  constexpr auto length_tag_shift = std::bit_width(constants::variable_payload_mask);
  const auto size = static_cast<std::size_t>(bytes[0] >> length_tag_shift) + 1;
  std::uint32_t value = bytes[0] & constants::variable_payload_mask;
  for (std::size_t index = 1; index < size; ++index) {
    value = (value << constants::wire_byte_bits) | bytes[index];
  }
  return {value, size};
}

// Access a single validated scalar without reflection, alignment assumptions, or allocation.
template <typename Value>
struct scalar_view_codec {
  static constexpr std::size_t fixed_size = std::is_enum_v<Value> ? 0 :
      (std::is_same_v<Value, bool> ? 1 : sizeof(Value));
  // Validate and consume one scalar in the shared mapping session.
  static void scan(view_scanner& scanner) {
    Value value{};
    scanner.serialize_in(value);
  }
  // Copy only the scalar bits and convert the fixed little-endian wire representation.
  static Value read(std::span<const std::uint8_t> bytes, const decode_limits&) {
    if constexpr (std::is_enum_v<Value>) {
      return static_cast<Value>(view_compact(bytes).first);
    } else if constexpr (std::is_same_v<Value, bool>) {
      return bytes[0] != 0;
    } else if constexpr (std::is_same_v<Value, char>) {
      return static_cast<char>(bytes[0]);
    } else if constexpr (std::is_floating_point_v<Value>) {
      using wire_type = std::conditional_t<sizeof(Value) == sizeof(std::uint32_t),
                                            std::uint32_t, std::uint64_t>;
      wire_type bits;
      std::memcpy(&bits, bytes.data(), sizeof(bits));
      return std::bit_cast<Value>(change_endian<std::endian::little, std::endian::native>(bits));
    } else {
      std::make_unsigned_t<Value> bits;
      std::memcpy(&bits, bytes.data(), sizeof(bits));
      return std::bit_cast<Value>(change_endian<std::endian::little, std::endian::native>(bits));
    }
  }
  // Encode before touching the destination; reject compact-width changes atomically.
  static void write(std::span<std::uint8_t> bytes, Value value) {
    if constexpr (std::is_enum_v<Value>) {
      std::array<std::uint8_t, sizeof(std::uint32_t)> scratch{};
      full_stream output{scratch.data(), scratch.size()};
      binary_none<serialize_type::out> encoder{output};
      encoder.serialize_out(value);
      if (output.current_offset() != bytes.size()) {
        throw std::length_error{"View update changes the encoded field size"};
      }
      std::memcpy(bytes.data(), scratch.data(), bytes.size());
    } else if constexpr (std::is_same_v<Value, bool> || std::is_same_v<Value, char>) {
      bytes[0] = static_cast<std::uint8_t>(value);
    } else if constexpr (std::is_floating_point_v<Value>) {
      using wire_type = std::conditional_t<sizeof(Value) == sizeof(std::uint32_t),
                                            std::uint32_t, std::uint64_t>;
      const auto encoded = change_endian<std::endian::native, std::endian::little>(
          std::bit_cast<wire_type>(value));
      std::memcpy(bytes.data(), &encoded, sizeof(encoded));
    } else {
      const auto encoded = change_endian<std::endian::native, std::endian::little>(
          static_cast<std::make_unsigned_t<Value>>(value));
      std::memcpy(bytes.data(), &encoded, sizeof(encoded));
    }
  }
};

struct string_view_codec {
  static constexpr std::size_t fixed_size = 0;
  // Validate a length-prefixed string without allocating its contents.
  static void scan(view_scanner& scanner) {
    scanner.serialize_in_name();
  }
  // Borrow the validated string payload; the original buffer retains ownership.
  static std::string_view read(std::span<const std::uint8_t> bytes, const decode_limits&) {
    const auto [size, prefix] = view_compact(bytes);
    return size == 0 ? std::string_view{} :
        std::string_view{reinterpret_cast<const char*>(bytes.data() + prefix), size};
  }
  // Preserve the prefix and extent, including an accepted overlong length prefix.
  static void write(std::span<std::uint8_t> bytes, std::string_view value) {
    const auto [size, prefix] = view_compact(bytes);
    if (value.size() != size) {
      throw std::length_error{"A view string replacement must have the same byte length"};
    }
    if (size != 0) {
      std::memmove(bytes.data() + prefix, value.data(), size);
    }
  }
};

template <typename View>
struct object_view_codec {
  static constexpr std::size_t fixed_size = 0;
  // Traverse a nested schema with the parent's budgets and depth accounting.
  static void scan(view_scanner& scanner) {
    View::serializer_scan(scanner, nullptr);
  }
  // Return a nested borrowed object, validating its independent offset cache.
  template <typename Byte>
  static View read(std::span<Byte> bytes, const decode_limits& limits) {
    return View::map(bytes, limits);
  }
};

template <typename Element> struct array_view_codec;
template <typename Key, typename Value> struct map_view_codec;
template <typename... Alternatives> struct variant_view_codec;

template <typename Element, typename Byte> class binary_array_view;
template <typename Key, typename Value, typename Byte> class binary_map_view;

// Keep independent positions and cached entry boundaries without owning or allocating message data.
template <typename Byte, typename... Codecs>
class binary_collection_iterator {
  static constexpr auto component_count = sizeof...(Codecs);
  static_assert(component_count == 1 || component_count == 2);
  static constexpr bool is_map_entry = component_count == 2;
  using first_codec = std::tuple_element_t<0, std::tuple<Codecs...>>;
  using last_codec = std::tuple_element_t<component_count - 1, std::tuple<Codecs...>>;
  using first_byte = std::conditional_t<is_map_entry, const std::uint8_t, Byte>;
  using first_value = decltype(first_codec::read(std::declval<std::span<first_byte>>(),
                                                 std::declval<const decode_limits&>()));
  using last_value = decltype(last_codec::read(std::declval<std::span<Byte>>(),
                                               std::declval<const decode_limits&>()));
  using entry_offsets = std::array<std::size_t, component_count + 1>;
  std::span<Byte> storage{};
  decode_limits limits{};
  std::size_t index{};
  std::size_t count{};
  entry_offsets offsets{};

  template <typename, typename>
  friend class binary_array_view;
  template <typename, typename, typename>
  friend class binary_map_view;

  // Discover only the current entry; mapping has already validated the whole collection and limits.
  entry_offsets locate(std::size_t start) const {
    entry_offsets result{};
    result[0] = start;
    std::size_t component{};
    if constexpr (((Codecs::fixed_size != 0) && ...)) {
      auto next = start;
      ((next += Codecs::fixed_size, result[++component] = next), ...);
    } else {
      const auto remaining = storage.subspan(start);
      const auto input = make_constant_stream(remaining.data(), remaining.size());
      view_scanner scanner{input, limits};
      // Retain the collection's nesting level during this bounded entry scan.
      auto nesting = scanner.enter_object();
      ((Codecs::scan(scanner), result[++component] = start + scanner.position()), ...);
    }
    return result;
  }

  // Only validated collection views may construct iterators; end construction never scans entries.
  binary_collection_iterator(std::span<Byte> bytes, const decode_limits& input_limits, bool at_end)
      : storage{bytes}, limits{input_limits} {
    const auto [element_count, prefix] = view_compact(storage);
    count = element_count;
    if (at_end || count == 0) {
      index = count;
      offsets.fill(storage.size());
    } else {
      offsets = locate(prefix);
    }
  }

  // Reject dereferencing, advancing, or modifying an end or default-constructed iterator.
  void require_element() const {
    if (index == count) {
      throw std::out_of_range{"View collection iterator at end"};
    }
  }

  // Borrow one cached component; callers must first require a dereferenceable iterator.
  template <std::size_t Index>
  std::span<Byte> component_bytes() const {
    return storage.subspan(offsets[Index], offsets[Index + 1] - offsets[Index]);
  }

public:
  using value_type =
      std::conditional_t<is_map_entry, std::pair<first_value, last_value>, first_value>;
  using difference_type = std::ptrdiff_t;
  using reference = value_type;
  using pointer = void;
  using iterator_concept = std::forward_iterator_tag;
  // Legacy forward iterators require references; these iterators return values or borrowed views.
  using iterator_category = std::input_iterator_tag;

  // Create an empty singular iterator; copies retain independent positions and shared storage.
  binary_collection_iterator() = default;

  // Return a scalar/string/nested view, or a pair containing an immutable key and its value.
  value_type operator*() const {
    require_element();
    // Read values on demand so size-preserving edits through another alias remain observable.
    if constexpr (!is_map_entry) {
      return first_codec::read(component_bytes<0>(), limits);
    } else {
      return {first_codec::read(std::span<const std::uint8_t>{component_bytes<0>()}, limits),
              last_codec::read(component_bytes<1>(), limits)};
    }
  }

  // Advance from cached boundaries; failed scans leave this iterator's position unchanged.
  binary_collection_iterator& operator++() {
    require_element();
    if (index + 1 < count) {
      offsets = locate(offsets.back());
    } else {
      offsets.fill(storage.size());
    }
    ++index;
    return *this;
  }

  // Return the previous independent position, then advance this iterator.
  binary_collection_iterator operator++(int) {
    auto previous = *this;
    ++*this;
    return previous;
  }

  // Compare positions in the same encoded collection, including zero-byte object elements.
  bool operator==(const binary_collection_iterator& other) const noexcept {
    return storage.data() == other.storage.data() && storage.size() == other.storage.size() &&
           index == other.index;
  }

  // Replace the current scalar/string array element without rescanning earlier elements.
  template <typename Replacement>
  void set(const Replacement& value) const
      requires (!is_map_entry && !std::is_const_v<Byte> &&
                requires(std::span<Byte> bytes) { first_codec::write(bytes, value); }) {
    require_element();
    first_codec::write(component_bytes<0>(), value);
  }

  // Replace only the current map value, preserving key identity and encoded entry extents.
  template <typename Replacement>
  void set_value(const Replacement& value) const
      requires (is_map_entry && !std::is_const_v<Byte> &&
                requires(std::span<Byte> bytes) { last_codec::write(bytes, value); }) {
    require_element();
    last_codec::write(component_bytes<1>(), value);
  }
};

// A borrowed sequence supports sequential traversal and checked indexed access.
template <typename Element, typename Byte>
class binary_array_view {
  std::span<Byte> storage;
  decode_limits limits;
  friend struct array_view_codec<Element>;
  // Only the validated codec may create a sequence over existing bytes.
  binary_array_view(std::span<Byte> bytes, const decode_limits& input_limits)
      : storage{bytes}, limits{input_limits} {}
  // Locate an element using bounded traversal, retaining the original writable span type.
  std::span<Byte> element_bytes(std::size_t index) const {
    if constexpr (Element::fixed_size != 0) {
      const auto [count, prefix] = view_compact(storage);
      if (index >= count) { throw std::out_of_range{"View array index"}; }
      return storage.subspan(prefix + index * Element::fixed_size, Element::fixed_size);
    } else {
      const auto input = make_constant_stream(storage.data(), storage.size());
      view_scanner scanner{input, limits};
      auto nesting = scanner.enter_object();
      const auto count = scanner.collection_size();
      if (index >= count) { throw std::out_of_range{"View array index"}; }
      for (std::size_t current = 0; current < index; ++current) { Element::scan(scanner); }
      const auto start = scanner.position();
      Element::scan(scanner);
      return storage.subspan(start, scanner.position() - start);
    }
  }
public:
  using iterator = binary_collection_iterator<Byte, Element>;
  // Start an independent traversal; its storage and limits outlive a temporary collection wrapper.
  iterator begin() const {
    return iterator{storage, limits, false};
  }
  // Return the past-the-end position without traversing the collection.
  iterator end() const {
    return iterator{storage, limits, true};
  }
  // Return the validated element count, with no traversal or allocation.
  std::size_t size() const { return view_compact(storage).first; }
  // Read a scalar/string or return a nested view of an existing element.
  auto at(std::size_t index) const { return Element::read(element_bytes(index), limits); }
  // Replace a scalar/string element while preserving the enclosing layout.
  template <typename Value>
  void set(std::size_t index, const Value& value)
      requires (!std::is_const_v<Byte> && requires(std::span<std::uint8_t> bytes) {
        Element::write(bytes, value);
      }) {
    Element::write(element_bytes(index), value);
  }
};

template <typename Element>
struct array_view_codec {
  static constexpr std::size_t fixed_size = 0;
  // Validate every element once when mapping the containing message.
  static void scan(view_scanner& scanner) {
    auto nesting = scanner.enter_object();
    const auto count = scanner.collection_size();
    for (std::size_t index = 0; index < count; ++index) { Element::scan(scanner); }
  }
  // Construct a lightweight sequence over already validated bytes.
  template <typename Byte>
  static auto read(std::span<Byte> bytes, const decode_limits& limits) {
    return binary_array_view<Element, Byte>{bytes, limits};
  }
};

// Map views expose encoded entries in wire order; keys stay read-only to preserve identity.
template <typename Key, typename Value, typename Byte>
class binary_map_view {
  std::span<Byte> storage;
  decode_limits limits;
  friend struct map_view_codec<Key, Value>;
  // Only the validated codec may construct a map view.
  binary_map_view(std::span<Byte> bytes, const decode_limits& input_limits)
      : storage{bytes}, limits{input_limits} {}
  // Locate both halves of a wire entry with the original mapping limits.
  std::pair<std::span<Byte>, std::span<Byte>> entry_bytes(std::size_t index) const {
    if constexpr (Key::fixed_size != 0 && Value::fixed_size != 0) {
      const auto [count, prefix] = view_compact(storage);
      if (index >= count) { throw std::out_of_range{"View map index"}; }
      const auto start = prefix + index * (Key::fixed_size + Value::fixed_size);
      return {storage.subspan(start, Key::fixed_size),
              storage.subspan(start + Key::fixed_size, Value::fixed_size)};
    }
    const auto input = make_constant_stream(storage.data(), storage.size());
    view_scanner scanner{input, limits};
    auto nesting = scanner.enter_object();
    const auto count = scanner.collection_size();
    if (index >= count) { throw std::out_of_range{"View map index"}; }
    for (std::size_t current = 0; current < index; ++current) {
      Key::scan(scanner);
      Value::scan(scanner);
    }
    const auto start = scanner.position();
    Key::scan(scanner);
    const auto middle = scanner.position();
    Value::scan(scanner);
    return {storage.subspan(start, middle - start),
            storage.subspan(middle, scanner.position() - middle)};
  }
public:
  using iterator = binary_collection_iterator<Byte, Key, Value>;
  // Traverse pairs in wire order; key views remain read-only even when values are mutable.
  iterator begin() const {
    return iterator{storage, limits, false};
  }
  // Return the past-the-end position without traversing the collection.
  iterator end() const {
    return iterator{storage, limits, true};
  }
  // Return the encoded entry count; duplicate keys remain visible in wire order.
  std::size_t size() const { return view_compact(storage).first; }
  // Borrow or read a key with immutable access even from a mutable map view.
  auto key_at(std::size_t index) const {
    return Key::read(std::span<const std::uint8_t>{entry_bytes(index).first}, limits);
  }
  // Borrow or read an entry's value; nested mutable values can be edited through their views.
  auto value_at(std::size_t index) const { return Value::read(entry_bytes(index).second, limits); }
  // Replace a scalar/string value without changing keys, entry count, or byte extents.
  template <typename Replacement>
  void set_value(std::size_t index, const Replacement& value)
      requires (!std::is_const_v<Byte> && requires(std::span<std::uint8_t> bytes) {
        Value::write(bytes, value);
      }) {
    Value::write(entry_bytes(index).second, value);
  }
};

template <typename Key, typename Value>
struct map_view_codec {
  static constexpr std::size_t fixed_size = 0;
  // Validate keys and values with a single shared decoding budget.
  static void scan(view_scanner& scanner) {
    auto nesting = scanner.enter_object();
    const auto count = scanner.collection_size();
    for (std::size_t index = 0; index < count; ++index) {
      Key::scan(scanner);
      Value::scan(scanner);
    }
  }
  // Borrow the validated entry sequence without constructing a std::map.
  template <typename Byte>
  static auto read(std::span<Byte> bytes, const decode_limits& limits) {
    return binary_map_view<Key, Value, Byte>{bytes, limits};
  }
};

template <typename Byte, typename... Alternatives>
class binary_variant_view {
  std::span<Byte> storage;
  decode_limits limits;
  friend struct variant_view_codec<Alternatives...>;
  // Only the validated codec may create an active-alternative view.
  binary_variant_view(std::span<Byte> bytes, const decode_limits& input_limits)
      : storage{bytes}, limits{input_limits} {}
  // Require the requested alternative before exposing its payload.
  std::span<Byte> payload(std::size_t requested) const {
    const auto [active, prefix] = view_compact(storage);
    if (active != requested) { throw std::invalid_argument{"Inactive view union alternative"}; }
    return storage.subspan(prefix);
  }
public:
  // Return the active zero-based wire discriminator.
  std::size_t index() const { return view_compact(storage).first; }
  // Access the active alternative as a scalar/string or a nested borrowed view.
  template <std::size_t Index>
  auto get() const {
    using codec = std::tuple_element_t<Index, std::tuple<Alternatives...>>;
    return codec::read(payload(Index), limits);
  }
  // Update an existing scalar/string alternative; switching alternatives requires rebuilding.
  template <std::size_t Index, typename Value>
  void set(const Value& value)
      requires (!std::is_const_v<Byte> && requires(std::span<std::uint8_t> bytes) {
        std::tuple_element_t<Index, std::tuple<Alternatives...>>::write(bytes, value);
      }) {
    using codec = std::tuple_element_t<Index, std::tuple<Alternatives...>>;
    codec::write(payload(Index), value);
  }
};

template <typename... Alternatives>
struct variant_view_codec {
  static constexpr std::size_t fixed_size = 0;
  // Dispatch validation to the selected schema alternative with no dynamic metadata.
  template <std::size_t Index = 0>
  static void scan_alternative(view_scanner& scanner, std::size_t active) {
    if constexpr (Index == sizeof...(Alternatives)) {
      scanner.bad_union();
    } else {
      if (active == Index) {
        std::tuple_element_t<Index, std::tuple<Alternatives...>>::scan(scanner);
      } else {
        scan_alternative<Index + 1>(scanner, active);
      }
    }
  }
  // Validate the discriminator and payload using the containing message's budgets.
  static void scan(view_scanner& scanner) {
    auto nesting = scanner.enter_object();
    scan_alternative(scanner, scanner.serialize_in_variable());
  }
  // Borrow the existing alternative without constructing native union storage.
  template <typename Byte>
  static auto read(std::span<Byte> bytes, const decode_limits& limits) {
    return binary_variant_view<Byte, Alternatives...>{bytes, limits};
  }
};

// Common storage for generated positional views; offsets are per-object, never runtime reflection.
template <typename Byte, std::size_t FieldCount>
class binary_view_base {
  std::span<Byte> storage;
  std::array<std::size_t, FieldCount + 1> offsets{};
protected:
  decode_limits view_limits;
  // Validate an exact message and build offsets before a mapped object becomes observable.
  binary_view_base(std::span<Byte> bytes, decode_limits limits,
                   void (*scan)(view_scanner&, std::size_t*))
      : storage{bytes}, view_limits{limits} {
    const auto input = make_constant_stream(bytes.data(), bytes.size());
    view_scanner scanner{input, limits};
    scan(scanner, offsets.data());
    scanner.finish();
  }
  // Select a generated constant field slot without a key search or repeated full scan.
  template <std::size_t Index>
  std::span<Byte> field_bytes() const {
    static_assert(Index < FieldCount);
    return storage.subspan(offsets[Index], offsets[Index + 1] - offsets[Index]);
  }
public:
  static constexpr std::endian wire_endian = std::endian::little;
  static constexpr serialize_key_type key_type = serialize_key_type::none;
  // Borrow the exact serialized message; raw mutation must not invalidate mapped layouts.
  std::span<const std::uint8_t> serialized_bytes() const { return storage; }
  // Copy the existing positional message into a stream using its normal reservation policy.
  void serialize_out(rohit::type_check::output_stream auto& output) const {
    if constexpr (rohit::type_check::output_buffer<std::remove_reference_t<decltype(output)>>) {
      output.append(storage.data(), storage.size());
    } else {
      write_stream_bytes(output, storage.data(), storage.size());
    }
  }
  // Append to a positional protocol; keyed and JSON protocols intentionally have no overload.
  template <rohit::type_check::output_buffer Stream>
  void serialize_out(
      binary_out_base<serialize_key_type::none, std::endian::little, Stream>& protocol) const {
    serialize_out(protocol.get_stream());
  }
  // Match the runtime's explicit template probe without enabling incompatible protocols.
  template <typename Protocol>
    requires requires(Protocol& protocol) {
      requires Protocol::key_type == serialize_key_type::none;
      requires Protocol::wire_endian == std::endian::little;
      requires rohit::type_check::output_buffer<
          std::remove_reference_t<decltype(protocol.get_stream())>>;
    }
  void serialize_out(Protocol& protocol) const {
    serialize_out(protocol.get_stream());
  }
  // Support the existing explicit-protocol convenience syntax for positional binary only.
  template <template <serialize_type> class Protocol>
  void serialize_out(rohit::type_check::output_stream auto& output) const {
    static_assert(std::is_same_v<Protocol<serialize_type::out>, binary_none<serialize_type::out>>,
                  "Binary views store only binary_none positional messages");
    serialize_out(output);
  }
};

} // namespace rohit::serializer::detail
