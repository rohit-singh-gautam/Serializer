#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <string>
#include <string_view>
#include <type_traits>

namespace rohit::type_check {

// A stable readable byte range; cursor movement must not invalidate borrowed pointers.
template <typename T>
concept buffer_view = requires(const std::remove_reference_t<T>& value) {
  requires std::is_pointer_v<decltype(value.curr())>;
  { value.curr() } -> std::convertible_to<const std::uint8_t*>;
  { value.remaining_buffer() } -> std::same_as<std::size_t>;
};

// An input cursor consumes checked ranges through const access without changing their bytes.
template <typename T>
concept input_buffer =
    buffer_view<T> && requires(const std::remove_reference_t<T>& value, std::size_t size) {
      { value.full() } -> std::same_as<bool>;
      {
        value.get_curr_and_increase_unchecked(size)
      } noexcept -> std::convertible_to<const std::uint8_t*>;
    };

// Schema scanning additionally needs checked byte access and direct cursor advancement.
template <typename T>
concept schema_input_buffer =
    input_buffer<T> && requires(const std::remove_reference_t<T>& value, std::size_t size) {
      { *value } -> std::convertible_to<std::uint8_t>;
      ++value;
      value.advance_unchecked(size);
      value += size;
      { value == std::string_view{} } -> std::convertible_to<bool>;
      { value == "serializer" } -> std::convertible_to<bool>;
    };

// A serializer output supplies atomic reservations, alias-safe appends, and text/byte batches.
// A successful reservation keeps its writable range valid until the next reservation.
template <typename T>
concept output_buffer =
    buffer_view<T> &&
    requires(T& value, const void* data, const char* text, std::size_t size, std::string_view name,
             const std::string& string,
             void (*transform)(std::uint8_t*, const std::uint8_t*, std::size_t) noexcept) {
      value.reserve(size);
      { value.get_curr_and_increase_unchecked(size) } noexcept -> std::same_as<std::uint8_t*>;
      value.append(data, size);
      value.append(name);
      value.append(string);
      value.append("text");
      value.append('x');
      value.append(std::uint8_t{});
      value.append_external(data, size);
      value.append_external(text, size);
      value.append_transformed(data, size, size, transform);
      value.append_string(std::int64_t{});
      value.append_string(std::uint64_t{});
      value.write('x');
      value.write(name);
      value.write("text");
      value.write(name, ':', string);
      value.write_raw(std::uint8_t{});
      value.write_raw(std::uint8_t{}, std::uint8_t{});
      value.write_raw(std::uint8_t{}, std::uint8_t{}, std::uint8_t{});
      value.write_raw(std::uint8_t{}, std::uint8_t{}, std::uint8_t{}, std::uint8_t{});
    };

// Standard-style byte sources report partial reads and distinguish EOF from I/O errors.
template <typename T>
concept byte_input_stream = requires(T& value, char* bytes, std::streamsize size) {
  value.read(bytes, size);
  { value.gcount() } -> std::convertible_to<std::streamsize>;
  { value.peek() } -> std::convertible_to<std::char_traits<char>::int_type>;
  { value.eof() } -> std::convertible_to<bool>;
  { value.fail() } -> std::convertible_to<bool>;
  { value.bad() } -> std::convertible_to<bool>;
};

// Standard-style byte sinks write the complete requested range or report failure.
template <typename T>
concept byte_output_stream = requires(T& value, const char* bytes, std::streamsize size) {
  value.write(bytes, size);
  { value.fail() } -> std::convertible_to<bool>;
};

template <typename T>
concept input_stream = input_buffer<T> || byte_input_stream<T>;

template <typename T>
concept output_stream = output_buffer<T> || byte_output_stream<T>;

// Optional persistence capabilities; ordinary buffers and iostreams need not provide them.
// sync() must commit preceding writes to the backend's documented durable boundary or throw.
template <typename T>
concept durable_output_stream = output_stream<T> && requires(T& value) {
  { value.sync() } -> std::same_as<void>;
};

// Absolute positioning is separate from byte transport and need not exist on sequential streams.
template <typename T>
concept seekable_stream =
    (input_stream<T> || output_stream<T>) && requires(T& value, std::uint64_t offset_bytes) {
      { value.seek(offset_bytes) } -> std::same_as<void>;
    };

// Recovery may remove a validated incomplete tail only on an explicitly truncatable destination.
template <typename T>
concept truncatable_stream = output_stream<T> && requires(T& value, std::uint64_t size_bytes) {
  { value.truncate(size_bytes) } -> std::same_as<void>;
};

// A stable external size permits bounds checks before recovery allocates its input buffer.
template <typename T>
concept sized_stream = requires(const T& value) {
  { value.size() } -> std::convertible_to<std::uint64_t>;
};

// Compatibility names now describe capabilities, without requiring a base class.
template <typename T>
concept stream = input_buffer<T> && output_buffer<T>;

template <typename T>
concept write_stream = buffer_view<T> || requires(const std::remove_reference_t<T>& value) {
  { value.curr() } -> std::convertible_to<const std::uint8_t*>;
  { value.end() } -> std::convertible_to<const std::uint8_t*>;
  { value.capacity() } -> std::same_as<std::size_t>;
};

} // namespace rohit::type_check
