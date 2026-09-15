//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#pragma once

// MSVC reports its selected language mode through _MSVC_LANG unless /Zc:__cplusplus is enabled.
#if defined(_MSVC_LANG)
#if _MSVC_LANG < 202002L
#error "Serializer requires C++20 or later"
#endif
#elif __cplusplus < 202002L
#error "Serializer requires C++20 or later"
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace rohit {

namespace detail {
inline constexpr std::size_t identifier_hash_seed = 100000000003ULL;
inline constexpr unsigned identifier_hash_shift_bits = 9;
inline constexpr std::size_t buffer_growth_factor = 2;
inline constexpr std::size_t default_minimum_read_buffer_bytes = 1024;
inline constexpr std::size_t default_maximum_read_buffer_bytes = 8192;
inline constexpr std::size_t maximum_buffer_bytes =
    static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
template <typename>
inline constexpr bool unsupported_type = false;

// Describe one text argument without owning or copying its character sequence.
struct write_fragment {
  const std::uint8_t* data{};
  std::size_t size{};
  std::size_t source_offset{};
  std::uint8_t byte{};
  bool is_byte{};
  bool aliased{};
};

// These text arguments have a known encoded length without numeric formatting.
template <typename T>
concept text_fragment_value =
    std::same_as<T, char> || std::same_as<T, bool> || std::same_as<T, std::string> ||
    std::same_as<T, std::string_view> ||
    (std::is_array_v<T> && std::same_as<std::remove_extent_t<T>, char>);

// Integral byte swapping requires every object bit to participate in its value representation.
template <typename T>
concept endian_integer =
    std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> &&
    (std::numeric_limits<T>::digits + (std::is_signed_v<T> ? 1 : 0) ==
     sizeof(T) * std::numeric_limits<unsigned char>::digits);

// Limit floating-point conversion to the supported binary IEC 559 storage widths.
template <typename T>
concept endian_floating_point =
    std::floating_point<T> && std::numeric_limits<T>::is_iec559 &&
    std::numeric_limits<T>::radix == 2 &&
    (sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t));

// Booleans are logical no-ops; other scalars must have a supported byte representation.
template <typename T>
concept endian_scalar = std::same_as<std::remove_cv_t<T>, bool> || endian_integer<T> ||
                        endian_floating_point<T>;
} // namespace detail

// Reverse scalar bytes; floating-point values use an integer representation and bool is unchanged.
template <detail::endian_scalar T>
constexpr T byteswap(const T& val) noexcept {
  if constexpr (std::same_as<std::remove_cv_t<T>, bool>) {
    return val;
  } else if constexpr (detail::endian_integer<T>) {
#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
    return std::byteswap(val);
#else
    // C++20 fallback: reverse actual object bytes without assumptions about native byte order.
    auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
    std::reverse(bytes.begin(), bytes.end());
    return std::bit_cast<T>(bytes);
#endif
  } else {
    if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
      return std::bit_cast<T>(rohit::byteswap(std::bit_cast<std::uint32_t>(val)));
    } else if constexpr (sizeof(T) == sizeof(std::uint64_t)) {
      return std::bit_cast<T>(rohit::byteswap(std::bit_cast<std::uint64_t>(val)));
    }
  }
}

// Convert supported scalars between little and big endian; reject unsupported orders at compile time.
template <std::endian source, std::endian destination, detail::endian_scalar T>
constexpr T change_endian(const T& val) noexcept {
  static_assert(source == std::endian::little || source == std::endian::big,
                "Source byte order must be little or big endian; mixed native order is unsupported");
  static_assert(destination == std::endian::little || destination == std::endian::big,
                "Destination byte order must be little or big endian; mixed native order is unsupported");
  if constexpr (source == destination || sizeof(T) == 1 ||
                std::same_as<std::remove_cv_t<T>, bool>) {
    return val;
  } else {
    return rohit::byteswap(val);
  }
}

namespace exception {
class stream_overflow_exception : public std::exception {
public:
  // Return the exception message; the pointer remains valid for this exception lifetime.
  const char* what() const noexcept override {
    return "Stream Overflow";
  }
}; // class stream_overflow_exception

class stream_underflow_exception : public std::exception {
public:
  // Return the exception message; the pointer remains valid for this exception lifetime.
  const char* what() const noexcept override {
    return "Stream Underflow";
  }
}; // class stream_underflow_exception

class memory_allocation_exception : public std::exception {
public:
  // Return the exception message; the pointer remains valid for this exception lifetime.
  const char* what() const noexcept override {
    return "Stream unable to allocate memory";
  }
}; // class memory_allocation_exception

} // namespace exception

// Hash the supplied character sequence with the established identifier hash contract.
template <std::size_t size>
consteval std::size_t hash(const char (&str)[size]) {
  std::size_t ret = detail::identifier_hash_seed;
  const auto length = str[size - 1] == '\0' ? size - 1 : size;
  for (std::size_t index = 0; index < length; ++index) {
    ret = ((ret << detail::identifier_hash_shift_bits) + ret) ^ static_cast<std::size_t>(str[index]);
  }
  return ret;
}

// Hash the supplied character sequence with the established identifier hash contract.
constexpr std::size_t hash(const char* str) {
  std::size_t ret = detail::identifier_hash_seed;
  while (*str) {
    ret = ((ret << detail::identifier_hash_shift_bits) + ret) ^ static_cast<std::size_t>(*str++);
  }
  return ret;
}

// Hash the supplied character sequence with the established identifier hash contract.
constexpr std::size_t hash(const std::string& str) {
  std::size_t ret = detail::identifier_hash_seed;
  for (auto ch : str) {
    ret = ((ret << detail::identifier_hash_shift_bits) + ret) ^ static_cast<std::size_t>(ch);
  }
  return ret;
}

// Hash the supplied character sequence with the established identifier hash contract.
constexpr std::size_t hash(const std::string_view& str) {
  std::size_t ret = detail::identifier_hash_seed;
  for (auto ch : str) {
    ret = ((ret << detail::identifier_hash_shift_bits) + ret) ^ static_cast<std::size_t>(ch);
  }
  return ret;
}

// A borrowed buffer cursor with checked forward operators and explicit unchecked batch helpers.
// Unchecked operations require a previously validated or reserved range and never grow storage.
// Const views can advance their mutable cursor while keeping the underlying bytes read-only.
class stream {
protected:
  friend class fixed_buffer;
  mutable std::uint8_t* current_data;
  std::uint8_t* end_data;

  // Reject a cursor or requested range outside the available buffer.
  void check_overflow() const {
    check_overflow(1);
  }
  // Reject a cursor or requested range outside the available buffer.
  void check_overflow(std::size_t len) const {
    if (len > remaining_buffer()) {
      throw exception::stream_overflow_exception{};
    }
  }

  // Advance a validated range; C++20 also permits adding zero to a null empty cursor.
  void advance_cursor(std::size_t len) const noexcept {
    current_data += len;
  }

  // Reserve an append range; owning streams also rebase sources that alias their storage.
  virtual void reserve_append(const std::uint8_t*& source, std::size_t size) {
    static_cast<void>(source);
    reserve(size);
  }

  // Reserve a text batch; allocating streams also rebase any internal source pointers.
  virtual void reserve_fragments(std::span<detail::write_fragment> fragments, std::size_t size) {
    static_cast<void>(fragments);
    reserve(size);
  }

  // Capture a scalar byte or borrow a text range before reservation can move its storage.
  template <detail::text_fragment_value T>
  static detail::write_fragment make_write_fragment(const T& value) {
    if constexpr (std::same_as<T, char>) {
      return {.size = sizeof(std::uint8_t),
              .byte = static_cast<std::uint8_t>(value),
              .is_byte = true};
    } else if constexpr (std::same_as<T, bool>) {
      return {.size = sizeof(std::uint8_t),
              .byte = static_cast<std::uint8_t>(value ? '1' : '0'),
              .is_byte = true};
    } else if constexpr (std::is_array_v<T>) {
      constexpr auto extent = std::extent_v<T>;
      const auto size = value[extent - 1] == '\0' ? extent - 1 : extent;
      return {.data = reinterpret_cast<const std::uint8_t*>(value), .size = size};
    } else {
      return {.data = reinterpret_cast<const std::uint8_t*>(value.data()), .size = value.size()};
    }
  }

  // Initialize this object from the supplied storage or value state.
  stream() : current_data{nullptr}, end_data{nullptr} {}

public:
  // Initialize this object from the supplied storage or value state.
  stream(auto* begin_data, auto* end_data)
      : current_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{reinterpret_cast<std::uint8_t*>(end_data)} {
    if ((current_data == nullptr) != (this->end_data == nullptr) ||
        std::less<const std::uint8_t*>{}(this->end_data, current_data)) {
      throw std::invalid_argument{"Invalid stream buffer range"};
    }
  }
  // Initialize this object from the supplied storage or value state.
  stream(auto* begin_data, std::size_t size)
      : current_data{reinterpret_cast<std::uint8_t*>(begin_data)}, end_data{current_data} {
    if (size > detail::maximum_buffer_bytes || (size != 0 && current_data == nullptr)) {
      throw std::invalid_argument{"Invalid stream buffer size"};
    }
    if (size != 0) {
      end_data += size;
    }
  }
  // Initialize this object from the supplied storage or value state.
  stream(stream&& stream) : current_data{stream.current_data}, end_data{stream.end_data} {
    stream.current_data = stream.end_data = nullptr;
  }
  // Initialize this object from the supplied storage or value state.
  stream(std::string& string)
      : current_data{reinterpret_cast<std::uint8_t*>(string.data())},
        end_data{current_data + string.size()} {}
  // Initialize this object from the supplied storage or value state.
  stream(const stream& stream) : current_data{stream.current_data}, end_data{stream.end_data} {}
  // Release resources owned by this object.
  virtual ~stream() = default;

  // Borrow the remaining bytes as an independent cursor view.
  stream get_simple_stream() {
    return stream{current_data, end_data};
  }
  // Borrow the remaining bytes as an independent cursor view.
  const stream get_simple_stream() const {
    return stream{current_data, end_data};
  }
  // Borrow the remaining bytes through a read-only cursor view.
  const stream get_simple_const_stream() const {
    return stream{current_data, end_data};
  }

  // Compare prefix bytes regardless of char signedness, excluding only a final terminator.
  template <std::size_t Size>
  bool operator==(const char (&data)[Size]) const {
    const auto length = data[Size - 1] == '\0' ? Size - 1 : Size;
    if (remaining_buffer() < length) {
      return false;
    }
    return length == 0 || std::memcmp(data, current_data, length) == 0;
  }

  // Compare prefix bytes regardless of char signedness; string_view supplies the exact length.
  bool operator==(const std::string_view& text) const {
    if (remaining_buffer() < text.size()) {
      return false;
    }
    return text.empty() || std::memcmp(text.data(), current_data, text.size()) == 0;
  }

  // Assign the documented view or value state from the source object.
  stream& operator=(const stream& stream) {
    current_data = stream.current_data;
    end_data = stream.end_data;
    return *this;
  }
  // Commit a cursor from a view of the same storage end; do not rebind a const view.
  const stream& operator=(const stream& stream) const {
    if (end_data != stream.end_data) {
      throw std::invalid_argument{"Cursor assignment requires the same buffer end"};
    }
    current_data = stream.current_data;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual stream operator+(std::size_t len) {
    check_overflow(len);
    advance_cursor(len);
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual const stream operator+(std::size_t len) const {
    check_overflow(len);
    advance_cursor(len);
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual stream& operator+=(std::size_t len) {
    check_overflow(len);
    advance_cursor(len);
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual const stream& operator+=(std::size_t len) const {
    check_overflow(len);
    advance_cursor(len);
    return *this;
  }

  // Access the current byte, rejecting an exhausted cursor.
  inline std::uint8_t& operator*() {
    check_overflow();
    return *current_data;
  }
  // Read the current byte, rejecting an exhausted cursor.
  inline std::uint8_t operator*() const {
    check_overflow();
    return *current_data;
  }
  // Access the current byte; the caller must ensure the cursor is in bounds.
  inline std::uint8_t& at_unchecked() noexcept {
    return *current_data;
  }
  // Access the current byte; the caller must ensure the cursor is in bounds.
  inline std::uint8_t at_unchecked() const noexcept {
    return *current_data;
  }
  // Write and consume one previously reserved byte without checking capacity or growing storage.
  inline void push_unchecked(std::uint8_t value) noexcept {
    *current_data++ = value;
  }
  // Copy a valid source into previously reserved storage; allow overlap and never allocate.
  inline void append_unchecked(const auto* source, std::size_t size) noexcept {
    if (size != 0) {
      std::memmove(current_data, source, size);
      current_data += size;
    }
  }
  // Advance within a previously validated range; zero-byte movement is a no-op.
  inline void advance_unchecked(std::size_t len) const noexcept {
    advance_cursor(len);
  }
  // Return and consume a previously reserved range; later allocation invalidates the pointer.
  inline std::uint8_t* get_curr_and_increase_unchecked(std::size_t len) noexcept {
    auto* previous = current_data;
    advance_cursor(len);
    return previous;
  }
  // Return and consume a previously validated input range without another capacity check.
  inline const std::uint8_t* get_curr_and_increase_unchecked(std::size_t len) const noexcept {
    const auto* previous = current_data;
    advance_cursor(len);
    return previous;
  }
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline stream& operator++() {
    check_overflow();
    ++current_data;
    return *this;
  }
  // Move back one byte; a base stream has no start bound, so the caller must validate it.
  virtual inline stream& operator--() {
    --current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline const stream& operator++() const {
    check_overflow();
    ++current_data;
    return *this;
  }
  // Move back one byte; a base stream has no start bound, so the caller must validate it.
  virtual inline const stream& operator--() const {
    --current_data;
    return *this;
  }
  // Write one byte and advance the cursor through the stream reservation policy.
  inline void push(const auto ch) {
    append(static_cast<std::uint8_t>(ch));
  }

  // Return the current byte pointer and advance after validating the requested range.
  virtual inline std::uint8_t* get_curr_and_increase(const std::size_t len) {
    check_overflow(len);
    return get_curr_and_increase_unchecked(len);
  }
  // Return the current byte pointer and advance after validating the requested range.
  virtual inline const std::uint8_t* get_curr_and_increase(const std::size_t len) const {
    check_overflow(len);
    return get_curr_and_increase_unchecked(len);
  }

  // Measure consumed bytes from a pointer within this buffer.
  std::size_t get_size_from(const auto* start) const {
    return static_cast<std::size_t>(current_data - reinterpret_cast<const std::uint8_t*>(start));
  }

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline std::uint8_t* operator++(int) {
    check_overflow();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Move back one byte; a base stream has no start bound, so the caller must validate it.
  virtual inline std::uint8_t* operator--(int) {
    std::uint8_t* temp = current_data;
    --current_data;
    return temp;
  };
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline const std::uint8_t* operator++(int) const {
    check_overflow();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Move back one byte; a base stream has no start bound, so the caller must validate it.
  virtual inline const std::uint8_t* operator--(int) const {
    std::uint8_t* temp = current_data;
    --current_data;
    return temp;
  };
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Expose the buffer end; the returned pointer does not own storage.
  const std::uint8_t* end() const {
    return end_data;
  }
  // Expose the current cursor; the returned pointer does not own storage.
  const std::uint8_t* curr() const {
    return current_data;
  }

  // Expose the buffer end; the returned pointer does not own storage.
  auto& end() {
    return end_data;
  }
  // Expose the current cursor; the returned pointer does not own storage.
  auto& curr() {
    return current_data;
  }

  // Report remaining bytes; nonnull pointers must belong to the same live buffer.
  std::size_t remaining_buffer() const {
    if ((current_data == nullptr) != (end_data == nullptr) ||
        std::less<const std::uint8_t*>{}(end_data, current_data)) {
      throw exception::stream_overflow_exception{};
    }
    // C++20 defines subtraction of two null pointers as zero for an empty stream.
    return static_cast<std::size_t>(end_data - current_data);
  }

  // Test whether the cursor has reached the buffer end.
  bool full() const {
    return current_data == end_data;
  }

  // Test whether the remaining buffer can hold the requested value or byte count.
  template <typename T>
  bool check_capacity() const {
    return remaining_buffer() >= sizeof(T);
  }
  // Test whether the remaining buffer can hold the requested value or byte count.
  bool check_capacity(const std::size_t size) const {
    return remaining_buffer() >= size;
  }

  // Replace the cursor with a pointer into this buffer; ownership is unchanged.
  void update_curr(std::uint8_t* new_cursor) const {
    this->current_data = new_cursor;
  }

  // Borrow the current byte pointer and remaining byte count.
  auto get_raw_current_buffer() {
    return std::make_pair(current_data, remaining_buffer());
  }

  // Ensure capacity for a write using the stream allocation or bounds policy.
  virtual inline void reserve(const std::size_t len) {
    check_overflow(len);
  }
  // Ensure capacity for a write using the stream allocation or bounds policy.
  inline void reserve(const auto* begin_data, const auto* end_data) {
    if (begin_data == end_data) {
      reserve(0);
      return;
    }
    if (begin_data == nullptr || end_data == nullptr ||
        std::less<const void*>{}(end_data, begin_data)) {
      throw std::invalid_argument{"Invalid append range"};
    }
    const std::size_t len =
        reinterpret_cast<const char*>(end_data) - reinterpret_cast<const char*>(begin_data);
    reserve(len);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::string_view& source) {
    append(source.data(), source.size());
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::string& source) {
    append(source.data(), source.size());
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const stream& source) {
    append(source.curr(), source.remaining_buffer());
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const auto* begin, const auto* end) {
    if (begin == end) {
      return;
    }
    if (begin == nullptr || end == nullptr || std::less<const void*>{}(end, begin)) {
      throw std::invalid_argument{"Invalid append range"};
    }
    append(begin, static_cast<std::size_t>(reinterpret_cast<const std::uint8_t*>(end) -
                                         reinterpret_cast<const std::uint8_t*>(begin)));
  }
  // Append a valid source range, allowing overlap and rebasing aliases in owned streams.
  inline void append(const auto* begin, std::size_t size) {
    if (size == 0) {
      return;
    }
    if (begin == nullptr || size > detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid append source"};
    }
    const auto* source = reinterpret_cast<const std::uint8_t*>(begin);
    reserve_append(source, size);
    append_unchecked(source, size);
  }
  // Append an independent source in one reservation; it must not overlap this stream's storage.
  inline void append_external(const auto* source, std::size_t size) {
    if (size == 0) {
      return;
    }
    if (source == nullptr || size > detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid append source"};
    }
    reserve(size);
    std::memcpy(current_data, source, size);
    advance_cursor(size);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const char value) {
    reserve(sizeof(value));
    push_unchecked(static_cast<std::uint8_t>(value));
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::uint8_t value) {
    reserve(sizeof(value));
    push_unchecked(value);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  template <std::size_t size>
  inline void append(const char (&value)[size]) {
    if constexpr (size >= 1) {
      auto new_size = value[size - 1] ? size : size - 1;
      if (new_size) {
        append(value, value + new_size);
      }
    }
  }

  // Append raw arguments, reserving once for a batch consisting entirely of individual bytes.
  template <typename... ValueType>
  inline void write_raw(const ValueType&... value) {
    if constexpr (sizeof...(ValueType) != 0 &&
                  ((std::is_same_v<ValueType, char> ||
                    std::is_same_v<ValueType, std::uint8_t>) && ...)) {
      // Snapshot byte arguments before reservation can relocate aliased storage.
      const std::array<std::uint8_t, sizeof...(ValueType)> bytes{
          static_cast<std::uint8_t>(value)...};
      reserve(bytes.size());
      current_data = std::copy(bytes.begin(), bytes.end(), current_data);
    } else {
      ((append(value)), ...);
    }
  }

  // Append a supported value as text, excluding a string literal terminator.
  template <typename ValueType>
  inline void append_string(const ValueType& value) {
    if constexpr (std::is_same_v<ValueType, char>) {
      append(value);
    } else if constexpr (std::is_same_v<ValueType, bool>) {
      append(value ? '1' : '0');
    } else if constexpr (std::is_integral_v<ValueType>) {
      // Allow one extra decimal digit, a sign, and the existing spare byte.
      constexpr std::size_t decimal_format_extra_bytes = 3;
      char buffer[std::numeric_limits<ValueType>::digits10 + decimal_format_extra_bytes];
      const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
      if (result.ec != std::errc{}) {
        throw std::runtime_error{"Unable to format integer"};
      }
      append_external(buffer, static_cast<std::size_t>(result.ptr - std::begin(buffer)));
    } else if constexpr (std::is_array_v<ValueType>) {
      if constexpr (sizeof(value[0]) == 1) {
        constexpr auto array_size = std::extent_v<ValueType>;
        if constexpr (array_size >= 1) {
          if (value[array_size - 1] == '\0') {
            append(std::begin(value), array_size - 1);
          } else {
            append(std::begin(value), std::end(value));
          }
        }
      } else {
        static_assert(detail::unsupported_type<ValueType>, "Unsupported type");
      }
    } else if constexpr (std::is_same_v<ValueType, std::string>) {
      append(value);
    } else if constexpr (std::is_same_v<ValueType, std::string_view>) {
      append(value);
    } else {
      static_assert(detail::unsupported_type<ValueType>, "Unsupported type");
    }
  }

  // Reserve known-length text batches once; numeric mixtures retain per-argument formatting.
  template <typename... ValueType>
  inline void write(const ValueType&... value) {
    if constexpr (sizeof...(ValueType) > 1 && (std::same_as<ValueType, char> && ...)) {
      write_raw(value...);
    } else if constexpr (sizeof...(ValueType) > 1 &&
                         (detail::text_fragment_value<ValueType> && ...)) {
      std::array fragments{make_write_fragment(value)...};
      std::size_t total_bytes{};
      for (const auto& fragment : fragments) {
        if (fragment.size > detail::maximum_buffer_bytes - total_bytes) {
          throw exception::stream_overflow_exception{};
        }
        if (!fragment.is_byte && fragment.size != 0 && fragment.data == nullptr) {
          throw std::invalid_argument{"Invalid text source"};
        }
        total_bytes += fragment.size;
      }
      if (total_bytes == 0) {
        return;
      }
      reserve_fragments(fragments, total_bytes);
      for (const auto& fragment : fragments) {
        if (fragment.is_byte) {
          push_unchecked(fragment.byte);
        } else {
          append_unchecked(fragment.data, fragment.size);
        }
      }
    } else {
      ((append_string(value)), ...);
    }
  }
}; // class stream

// A borrowed cursor that also retains the buffer start for offsets and checked rewinding.
class full_stream : public stream {
protected:
  friend class fixed_buffer;
  std::uint8_t* begin_data;

  // Reject moving the cursor before the start of the buffer.
  void check_underflow() const {
    if (current_data == nullptr || begin_data == nullptr ||
        !std::less<const std::uint8_t*>{}(begin_data, current_data)) {
      throw exception::stream_underflow_exception{};
    }
  }

  // Allocate malloc-compatible storage; zero bytes produce an empty, non-owning pointer.
  static std::uint8_t* allocate_storage(std::size_t size) {
    if (size > detail::maximum_buffer_bytes) {
      throw exception::stream_overflow_exception{};
    }
    if (size == 0) {
      return nullptr;
    }
    auto* allocation = static_cast<std::uint8_t*>(std::malloc(size));
    if (allocation == nullptr) {
      throw exception::memory_allocation_exception{};
    }
    return allocation;
  }

  // Grow a validated range transactionally; require offset <= old < required <= maximum
  // and minimum <= maximum <= maximum_buffer_bytes.
  void grow_storage_to(std::size_t required_capacity, std::size_t offset,
                       std::size_t old_capacity, std::size_t minimum_capacity,
                       std::size_t maximum_capacity) {
    const auto doubled_capacity = old_capacity > maximum_capacity / detail::buffer_growth_factor
                                      ? maximum_capacity
                                      : old_capacity * detail::buffer_growth_factor;
    const auto new_capacity = std::max({required_capacity, minimum_capacity, doubled_capacity});
    auto* allocation = static_cast<std::uint8_t*>(std::realloc(begin_data, new_capacity));
    if (allocation == nullptr) {
      throw exception::memory_allocation_exception{};
    }
    begin_data = allocation;
    current_data = allocation + offset;
    end_data = allocation + new_capacity;
  }

  // Validate a growth request and reserve enough storage without repeating range validation.
  void grow_storage(std::size_t len, std::size_t minimum_capacity, std::size_t maximum_capacity) {
    const auto offset = current_offset();
    if (maximum_capacity > detail::maximum_buffer_bytes || minimum_capacity > maximum_capacity ||
        offset > maximum_capacity || len > maximum_capacity - offset) {
      throw exception::stream_overflow_exception{};
    }
    const auto required_capacity = offset + len;
    // current_offset() has validated the complete begin/cursor/end ordering.
    const auto old_capacity = static_cast<std::size_t>(end_data - begin_data);
    if (required_capacity > old_capacity) {
      grow_storage_to(required_capacity, offset, old_capacity, minimum_capacity, maximum_capacity);
    }
  }

  // Rebase an aliased append source after successful growth; external sources retain their address.
  template <typename Reserve>
  void reserve_rebased_append(const std::uint8_t*& source, std::size_t size, Reserve reserve_bytes) {
    const auto less = std::less<const std::uint8_t*>{};
    const bool aliased = begin_data != nullptr && !less(source, begin_data) && less(source, end_data);
    std::size_t source_offset{};
    if (aliased) {
      if (size > static_cast<std::size_t>(end_data - source)) {
        throw exception::stream_overflow_exception{};
      }
      source_offset = static_cast<std::size_t>(source - begin_data);
    }
    reserve_bytes();
    if (aliased) {
      source = begin_data + source_offset;
    }
  }

  // Reserve a batch once and restore internal sources after growth; copying stays in argument order.
  void reserve_rebased_fragments(std::span<detail::write_fragment> fragments, std::size_t size) {
    const auto less = std::less<const std::uint8_t*>{};
    for (auto& fragment : fragments) {
      if (fragment.is_byte || fragment.size == 0) {
        continue;
      }
      fragment.aliased = begin_data != nullptr && !less(fragment.data, begin_data) &&
                         less(fragment.data, end_data);
      if (fragment.aliased) {
        if (fragment.size > static_cast<std::size_t>(end_data - fragment.data)) {
          throw exception::stream_overflow_exception{};
        }
        fragment.source_offset = static_cast<std::size_t>(fragment.data - begin_data);
      }
    }
    reserve(size);
    for (auto& fragment : fragments) {
      if (fragment.aliased) {
        fragment.data = begin_data + fragment.source_offset;
      }
    }
  }

  // Write a complete byte range and report open, write, and close failures to the caller.
  void write_file(const std::filesystem::path& path, std::size_t size) const {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
      throw std::length_error{"Output file exceeds the stream size limit"};
    }
    std::ofstream file_stream;
    file_stream.exceptions(std::ios::failbit | std::ios::badbit);
    file_stream.open(path, std::ios::binary);
    if (size != 0) {
      file_stream.write(reinterpret_cast<const char*>(begin_data), static_cast<std::streamsize>(size));
    }
    file_stream.close();
  }

  // Initialize this object from the supplied storage or value state.
  full_stream() : stream{}, begin_data{nullptr} {}

public:
  // Initialize this object from the supplied storage or value state.
  full_stream(auto* begin_data, auto* end_data)
      : stream{reinterpret_cast<std::uint8_t*>(begin_data),
               reinterpret_cast<std::uint8_t*>(end_data)},
        begin_data{reinterpret_cast<std::uint8_t*>(begin_data)} {}
  // Initialize this object from the supplied storage or value state.
  full_stream(auto* begin_data, auto* end_data, auto* current_data)
      : stream{reinterpret_cast<std::uint8_t*>(current_data),
               reinterpret_cast<std::uint8_t*>(end_data)},
        begin_data{reinterpret_cast<std::uint8_t*>(begin_data)} {
    if ((this->begin_data == nullptr) != (this->end_data == nullptr) ||
        std::less<const std::uint8_t*>{}(this->current_data, this->begin_data)) {
      throw std::invalid_argument{"Invalid full stream cursor"};
    }
  }
  // Initialize this object from the supplied storage or value state.
  full_stream(auto* begin_data, std::size_t size)
      : stream{reinterpret_cast<std::uint8_t*>(begin_data), size},
        begin_data{reinterpret_cast<std::uint8_t*>(begin_data)} {}
  // Initialize this object from the supplied storage or value state.
  full_stream(full_stream&& source) : stream{std::move(source)}, begin_data{source.begin_data} {
    source.begin_data = nullptr;
  }
  // Initialize this object from the supplied storage or value state.
  full_stream(const full_stream& source) : stream{source}, begin_data{source.begin_data} {}

  // Assign the documented view or value state from the source object.
  full_stream& operator=(const full_stream& stream) {
    begin_data = stream.begin_data;
    current_data = stream.current_data;
    end_data = stream.end_data;
    return *this;
  }

#if defined(__GNUC__)
// Preserve the established cursor operator return types.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Move back one byte, preserving the cursor if it is already at the buffer start.
  stream& operator--() override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Move a read cursor back one byte without crossing the buffer start.
  const stream& operator--() const override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Return the old cursor and move back one byte within the buffer.
  std::uint8_t* operator--(int) override {
    check_underflow();
    return current_data--;
  }
  // Return the old read cursor and move back one byte within the buffer.
  const std::uint8_t* operator--(int) const override {
    check_underflow();
    return current_data--;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Expose the buffer start; the returned pointer does not own storage.
  const auto begin() const {
    return begin_data;
  }
  // Expose the buffer start; the returned pointer does not own storage.
  auto begin() {
    return begin_data;
  }

  // Report the cursor offset in bytes from the buffer start.
  auto current_offset() const {
    if ((begin_data == nullptr) != (current_data == nullptr) ||
        std::less<const std::uint8_t*>{}(current_data, begin_data)) {
      throw exception::stream_underflow_exception{};
    }
    check_overflow(0);
    return static_cast<std::size_t>(current_data - begin_data);
  }
  // Report the complete buffer capacity in bytes.
  auto capacity() const {
    if ((begin_data == nullptr) != (end_data == nullptr) ||
        std::less<const std::uint8_t*>{}(end_data, begin_data)) {
      throw exception::stream_overflow_exception{};
    }
    return static_cast<std::size_t>(end_data - begin_data);
  }

  // Release the buffer pointer to the caller and clear this view.
  std::uint8_t* move() {
    auto temp = begin_data;
    begin_data = end_data = current_data = nullptr;
    return temp;
  }

  // Test whether this stream has a backing buffer.
  bool is_null() const {
    return begin_data == nullptr;
  }
  // Borrow the buffer start and its full capacity in bytes.
  auto get_raw_full_buffer() {
    return std::make_pair(begin_data, capacity());
  }
  // Rewind the cursor without changing stored bytes or capacity.
  void reset() const {
    current_data = begin_data;
  }
  // Test whether the cursor is at the buffer start.
  bool is_empty() const {
    return current_data == begin_data;
  }

  // Write consumed bytes to a file, reporting I/O failures.
  void write_to_file_till_offset(const std::filesystem::path& path) {
    write_file(path, current_offset());
  }
  // Write the full buffer to a file; the caller must initialize all capacity bytes.
  void write_to_file_complete(const std::filesystem::path& path) {
    write_file(path, capacity());
  }
};

class stream_auto_free : public full_stream {
public:
  using full_stream::full_stream;
  // Owned storage cannot be shared by copying.
  stream_auto_free(const stream_auto_free&) = delete;
  // Owned storage cannot be shared by assignment.
  stream_auto_free& operator=(const stream_auto_free&) = delete;
  // Transfer ownership and leave the source empty.
  stream_auto_free(stream_auto_free&& source) noexcept : full_stream{std::move(source)} {}
  // Release current storage and take ownership from the source.
  stream_auto_free& operator=(stream_auto_free&& source) noexcept {
    if (this != &source) {
      std::free(begin_data);
      full_stream::operator=(source);
      source.begin_data = source.current_data = source.end_data = nullptr;
    }
    return *this;
  }
  // Release resources owned by this object.
  ~stream_auto_free() override {
    free(begin_data);
  }
};

// Compatibility type: full_stream now checks forward movement, byte access, and rewinding.
class full_stream_limit_checked : public full_stream {
public:
  using full_stream::full_stream;
};

// Own malloc-compatible storage; borrowed pointers are invalidated by growth or ownership transfer.
class full_stream_auto_alloc : public full_stream {
  // Reserve enough space before moving or writing; a failure leaves allocation and cursor intact.
  void check_resize(std::size_t len = 1) {
    if (len > remaining_buffer()) {
      const auto minimum_capacity =
          begin_data == end_data ? detail::default_minimum_read_buffer_bytes : 0;
      grow_storage(len, minimum_capacity, detail::maximum_buffer_bytes);
    }
  }

protected:
  // Preserve append sources inside the buffer when realloc moves the allocation.
  void reserve_append(const std::uint8_t*& source, std::size_t size) override {
    reserve_rebased_append(source, size, [this, size] { reserve(size); });
  }

  // Preserve every source in a mixed text batch across one policy-aware reservation.
  void reserve_fragments(std::span<detail::write_fragment> fragments, std::size_t size) override {
    reserve_rebased_fragments(fragments, size);
  }

public:
  // Pointer constructors adopt malloc-compatible storage, including an optional existing cursor.
  using full_stream::full_stream;
  using full_stream::get_curr_and_increase;
  using full_stream::operator+;
  using full_stream::operator+=;
  using full_stream::operator++;
  using full_stream::reserve;

  // Allocate the requested capacity; zero capacity is a valid lazily allocated stream.
  full_stream_auto_alloc(std::size_t size) : full_stream{allocate_storage(size), size} {}
  // Start empty and allocate on the first nonempty write.
  full_stream_auto_alloc() : full_stream{} {}
  // Release the owned buffer.
  ~full_stream_auto_alloc() override {
    std::free(begin_data);
  }
  // Owned storage cannot be shared by copying.
  full_stream_auto_alloc(const full_stream_auto_alloc&) = delete;
  // Owned storage cannot be shared by assignment.
  full_stream_auto_alloc& operator=(const full_stream_auto_alloc&) = delete;
  // Transfer ownership and leave the source empty and reusable.
  full_stream_auto_alloc(full_stream_auto_alloc&& source) noexcept
      : full_stream{std::move(source)} {}
  // Release current storage and take ownership from the source.
  full_stream_auto_alloc& operator=(full_stream_auto_alloc&& source) noexcept {
    if (this != &source) {
      std::free(begin_data);
      full_stream::operator=(source);
      source.begin_data = source.current_data = source.end_data = nullptr;
    }
    return *this;
  }

#if defined(__GNUC__)
// Preserve the established cursor operator return types.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Advance one byte, growing before changing the cursor if required.
  stream& operator++() override {
    check_resize();
    ++current_data;
    return *this;
  }
  // Return a writable byte and advance, growing before returning its address.
  std::uint8_t* operator++(int) override {
    check_resize();
    return current_data++;
  }
  // Advance the cursor and return a borrowed view of the remaining buffer.
  stream operator+(std::size_t len) override {
    check_resize(len);
    advance_cursor(len);
    return *this;
  }
  // Advance only after successful reservation; zero-byte movement is a no-op.
  stream& operator+=(std::size_t len) override {
    check_resize(len);
    advance_cursor(len);
    return *this;
  }
  // Reserve capacity without advancing the cursor.
  void reserve(std::size_t len) override {
    check_resize(len);
  }
  // Return a writable range and advance only after successful reservation.
  std::uint8_t* get_curr_and_increase(std::size_t len) override {
    check_resize(len);
    auto* previous = current_data;
    advance_cursor(len);
    return previous;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Hand malloc/free ownership of the old buffer to the caller after allocating its replacement.
  full_stream return_old_and_alloc(std::size_t size) {
    full_stream previous{begin_data, end_data, current_data};
    auto* replacement = allocate_storage(size);
    begin_data = current_data = replacement;
    end_data = size == 0 ? replacement : replacement + size;
    return previous;
  }
};

struct stream_limits {
  std::size_t min_read_buffer_bytes{detail::default_minimum_read_buffer_bytes};
  std::size_t max_read_buffer_bytes{detail::default_maximum_read_buffer_bytes};
};

// Own bounded malloc-compatible storage and a snapshot of its allocation policy.
class full_stream_auto_alloc_limits : public full_stream {
  stream_limits limits{};

  // Validate and copy the policy so caller lifetime or later edits cannot invalidate it.
  static stream_limits copy_limits(const stream_limits* source) {
    if (source == nullptr || source->min_read_buffer_bytes > source->max_read_buffer_bytes ||
        source->max_read_buffer_bytes > detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid stream allocation limits"};
    }
    return *source;
  }

  // Enforce the maximum on the requested range and clamp geometric growth to that maximum.
  void check_resize(std::size_t len = 1) {
    const auto offset = current_offset();
    // Physical spare capacity can exceed the logical maximum in an adopted allocation.
    if (offset > limits.max_read_buffer_bytes || len > limits.max_read_buffer_bytes - offset) {
      throw exception::stream_overflow_exception{};
    }
    const auto old_capacity = static_cast<std::size_t>(end_data - begin_data);
    const auto required_capacity = offset + len;
    if (required_capacity > old_capacity) {
      // Limits were validated when copied, and current_offset() validated the storage range.
      grow_storage_to(required_capacity, offset, old_capacity, limits.min_read_buffer_bytes,
                      limits.max_read_buffer_bytes);
    }
  }

protected:
  // Preserve append sources inside the buffer when realloc moves the allocation.
  void reserve_append(const std::uint8_t*& source, std::size_t size) override {
    reserve_rebased_append(source, size, [this, size] { reserve(size); });
  }

  // Preserve every source in a mixed text batch across one bounded reservation.
  void reserve_fragments(std::span<detail::write_fragment> fragments, std::size_t size) override {
    reserve_rebased_fragments(fragments, size);
  }

public:
  // Pointer constructors adopt malloc-compatible storage with the default allocation limits.
  using full_stream::full_stream;
  using full_stream::get_curr_and_increase;
  using full_stream::operator+;
  using full_stream::operator+=;
  using full_stream::operator++;
  using full_stream::reserve;

  // Copy a valid policy and allocate its minimum capacity; a zero minimum is permitted.
  full_stream_auto_alloc_limits(const stream_limits* source)
      : full_stream{}, limits{copy_limits(source)} {
    begin_data = current_data = allocate_storage(limits.min_read_buffer_bytes);
    end_data = limits.min_read_buffer_bytes == 0
                   ? begin_data
                   : begin_data + limits.min_read_buffer_bytes;
  }
  // Start empty with the default limits, allocating on the first nonempty write.
  full_stream_auto_alloc_limits() : full_stream{} {}
  // Release the owned buffer.
  ~full_stream_auto_alloc_limits() override {
    std::free(begin_data);
  }
  // Owned storage cannot be shared by copying.
  full_stream_auto_alloc_limits(const full_stream_auto_alloc_limits&) = delete;
  // Owned storage cannot be shared by assignment.
  full_stream_auto_alloc_limits& operator=(const full_stream_auto_alloc_limits&) = delete;
  // Transfer storage and its policy; leave the source empty and reusable.
  full_stream_auto_alloc_limits(full_stream_auto_alloc_limits&& source) noexcept
      : full_stream{std::move(source)}, limits{source.limits} {}
  // Release current storage and take ownership and allocation policy from the source.
  full_stream_auto_alloc_limits& operator=(full_stream_auto_alloc_limits&& source) noexcept {
    if (this != &source) {
      std::free(begin_data);
      full_stream::operator=(source);
      limits = source.limits;
      source.begin_data = source.current_data = source.end_data = nullptr;
    }
    return *this;
  }

#if defined(__GNUC__)
// Preserve the established cursor operator return types.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Advance one byte only after any required bounded growth succeeds.
  stream& operator++() override {
    check_resize();
    ++current_data;
    return *this;
  }
  // Return a writable byte and advance after enforcing the allocation limit.
  std::uint8_t* operator++(int) override {
    check_resize();
    return current_data++;
  }
  // Advance the cursor and return a borrowed view without exceeding the allocation limit.
  stream operator+(std::size_t len) override {
    check_resize(len);
    advance_cursor(len);
    return *this;
  }
  // Advance only after successful reservation; zero-byte movement is a no-op.
  stream& operator+=(std::size_t len) override {
    check_resize(len);
    advance_cursor(len);
    return *this;
  }
  // Reserve capacity without advancing the cursor.
  void reserve(std::size_t len) override {
    check_resize(len);
  }
  // Return a writable range and advance only after successful reservation.
  std::uint8_t* get_curr_and_increase(std::size_t len) override {
    check_resize(len);
    auto* previous = current_data;
    advance_cursor(len);
    return previous;
  }

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Hand malloc/free ownership of the old buffer to the caller after allocating its replacement.
  full_stream return_old_and_alloc() {
    full_stream previous{begin_data, end_data, current_data};
    auto* replacement = allocate_storage(limits.min_read_buffer_bytes);
    begin_data = current_data = replacement;
    end_data = limits.min_read_buffer_bytes == 0
                   ? replacement
                   : replacement + limits.min_read_buffer_bytes;
    return previous;
  }
};

class fixed_buffer {
  std::uint8_t* begin_data;
  std::uint8_t* end_data;

public:
  // Adopt a valid range allocated with malloc; the caller gives up ownership on success.
  fixed_buffer(auto* begin_data, auto* end_data)
      : begin_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{reinterpret_cast<std::uint8_t*>(end_data)} {
    static_cast<void>(stream{this->begin_data, this->end_data});
  }
  // Adopt malloc-compatible storage of the given size, including a null empty buffer.
  fixed_buffer(auto* begin_data, std::size_t size)
      : begin_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{this->begin_data} {
    stream view{this->begin_data, size};
    end_data = view.end();
  }
  // Initialize this object from the supplied storage or value state.
  fixed_buffer(fixed_buffer&& buffer) : begin_data{buffer.begin_data}, end_data{buffer.end_data} {
    buffer.begin_data = buffer.end_data = nullptr;
  }
  // Adopt consumed bytes; source storage must be malloc-compatible and exclusively owned.
  fixed_buffer(full_stream&& stream)
      : begin_data{stream.begin_data}, end_data{stream.current_data} {
    stream.begin_data = stream.current_data = stream.end_data = nullptr;
  }
  // Initialize this object from the supplied storage or value state.
  fixed_buffer(const fixed_buffer&) = delete;
  // Release resources owned by this object.
  ~fixed_buffer() {
    if (begin_data) {
      free(begin_data);
    }
  }

  // Assign the documented view or value state from the source object.
  fixed_buffer& operator=(const fixed_buffer&) = delete;

  // Release current storage and take ownership of the source buffer.
  fixed_buffer& operator=(fixed_buffer&& source) noexcept {
    if (this != &source) {
      std::free(begin_data);
      begin_data = std::exchange(source.begin_data, nullptr);
      end_data = std::exchange(source.end_data, nullptr);
    }
    return *this;
  }

  // Expose the buffer start; the returned pointer does not own storage.
  auto begin() {
    return begin_data;
  }
  // Expose the buffer start; the returned pointer does not own storage.
  const auto begin() const {
    return begin_data;
  }

  // Expose a cursor-compatible view of this fixed buffer.
  auto curr() {
    return begin_data;
  }
  // Expose the current cursor; the returned pointer does not own storage.
  const auto curr() const {
    return begin_data;
  }

  // Expose the buffer end; the returned pointer does not own storage.
  auto end() {
    return end_data;
  }
  // Expose the buffer end; the returned pointer does not own storage.
  const auto end() const {
    return end_data;
  }

  // Report the complete buffer capacity in bytes.
  auto capacity() const {
    return static_cast<std::size_t>(end_data - begin_data);
  }
};

namespace type_check {

template <typename T>
concept stream = std::is_base_of_v<rohit::stream, T>;

template <typename T>
concept write_stream =
    std::is_base_of_v<rohit::stream, T> || std::is_base_of_v<rohit::fixed_buffer, T>;

} // namespace type_check

// Borrow mutable storage as a stream; the caller retains ownership.
inline stream make_stream(auto* begin, auto* end) {
  return stream{begin, end};
}
// Borrow mutable storage as a stream; the caller retains ownership.
inline stream make_stream(auto* begin, std::size_t size) {
  return stream{begin, size};
}
// Borrow mutable storage as a stream; the caller retains ownership.
inline stream make_stream(std::string& string) {
  return stream{string};
}

// Borrow storage for reading; its lifetime must exceed the stream lifetime.
template <typename ChT>
inline const stream make_constant_stream(const ChT* begin, const ChT* end) {
  return stream{const_cast<ChT*>(begin), const_cast<ChT*>(end)};
}
// Borrow storage for reading; its lifetime must exceed the stream lifetime.
template <typename ChT>
inline const stream make_constant_stream(const ChT* begin, std::size_t size) {
  return stream{const_cast<ChT*>(begin), size};
}
// Borrow storage for reading; its lifetime must exceed the stream lifetime.
inline const stream make_constant_stream(const std::string& string) {
  return stream{const_cast<char*>(string.data()), string.size()};
}

template <typename ChT>
// Borrow a complete buffer for reading while retaining its starting offset.
inline const full_stream make_constant_full_stream(const ChT* begin, const ChT* end) {
  return full_stream{const_cast<ChT*>(begin), const_cast<ChT*>(end)};
}
template <typename ChT>
// Borrow a complete buffer for reading while retaining its starting offset.
inline const full_stream make_constant_full_stream(const ChT* begin, std::size_t size) {
  return full_stream{const_cast<ChT*>(begin), size};
}
// Borrow a complete buffer for reading while retaining its starting offset.
inline const full_stream make_constant_full_stream(const std::string& string) {
  return full_stream{const_cast<char*>(string.data()), string.size()};
}
template <typename ChT>
// Borrow a complete buffer for reading while retaining its starting offset.
inline const full_stream make_constant_full_stream(const ChT* begin, const ChT* end,
                                                   const ChT* curr) {
  return full_stream{const_cast<ChT*>(begin), const_cast<ChT*>(end), const_cast<ChT*>(curr)};
}
// Load a complete regular file into owned storage; reject size, allocation, seek, and read failures.
inline const stream_auto_free make_stream_from_file(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    throw std::invalid_argument{"Not a valid file"};
  }

  std::ifstream file_stream{path, std::ios::binary | std::ios::ate};
  if (!file_stream.is_open()) {
    throw std::runtime_error{"Unable to open file"};
  }

  const auto file_size = file_stream.tellg();
  if (file_size < std::streampos{0}) {
    throw std::runtime_error{"Unable to determine input file size"};
  }
  const auto size_bytes = static_cast<std::uintmax_t>(static_cast<std::streamoff>(file_size));
  if (size_bytes > detail::maximum_buffer_bytes ||
      size_bytes > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::length_error{"Input file exceeds the stream size limit"};
  }
  file_stream.seekg(0, std::ios::beg);
  if (!file_stream) {
    throw std::runtime_error{"Unable to seek input file"};
  }

  const auto size = static_cast<std::size_t>(size_bytes);
  auto* buffer = size == 0 ? nullptr : static_cast<std::uint8_t*>(std::malloc(size));
  if (size != 0 && buffer == nullptr) {
    throw exception::memory_allocation_exception{};
  }
  stream_auto_free result{buffer, size};
  if (size != 0) {
    const auto read_size = static_cast<std::streamsize>(size);
    file_stream.read(reinterpret_cast<char*>(buffer), read_size);
    if (!file_stream || file_stream.gcount() != read_size) {
      throw std::runtime_error{"Unable to read complete input file"};
    }
  }
  return result;
}

namespace exception {
class base_parser : public std::exception {
protected:
  const std::string message;

  // Build a diagnostic with bounded context around the failing cursor.
  static const auto create_what_string(const stream& stream, const std::string& error_text) {
    constexpr std::size_t preceding_context_bytes = 160;
    constexpr std::size_t preceding_context_threshold_bytes = 168;
    constexpr std::size_t following_context_bytes = 80;
    constexpr unsigned char first_printable_character = 32;
    std::string message{"Error: "};

    const full_stream* fullstream = dynamic_cast<const full_stream*>(&stream);

    if (fullstream) {
      message += " - Location: ";
      if (fullstream->current_offset() >= preceding_context_threshold_bytes) {
        std::string_view second{reinterpret_cast<const char*>(fullstream->curr()) -
                                    preceding_context_bytes,
                                preceding_context_bytes};
        for (auto& current_ch : second) {
          if ((current_ch >= first_printable_character) || current_ch == '\n' ||
              current_ch == '\r' || current_ch == '\t') {
            message.push_back(current_ch);
          } else {
            message.push_back('#');
          }
        }
      } else {
        std::string_view initial{reinterpret_cast<const char*>(fullstream->begin()),
                                 fullstream->current_offset()};
        for (auto& current_ch : initial) {
          if ((current_ch >= first_printable_character) || current_ch == '\n' ||
              current_ch == '\r' || current_ch == '\t') {
            message.push_back(current_ch);
          } else {
            message.push_back('#');
          }
        }
      }
      message += " <-- failed here with error ";
      message += error_text;
      message += " --| ";
    } else {
      message += "Failed with error: ";
      message += error_text;
      message += " --- ";
    }

    std::string_view last{
        reinterpret_cast<const char*>(stream.curr()),
        // Report the number of bytes between the cursor and the buffer end.
        std::min<std::size_t>(following_context_bytes, stream.remaining_buffer())};
    for (auto& current_ch : last) {
      if ((current_ch >= first_printable_character) || current_ch == '\n' || current_ch == '\r' ||
          current_ch == '\t') {
        message.push_back(current_ch);
      } else {
        message.push_back('#');
      }
    }
    if (stream.remaining_buffer() > following_context_bytes) {
      message += " ... more ";
      message += std::to_string(stream.remaining_buffer() - following_context_bytes);
      message += " characters.";
    }

    return message;
  }

public:
  // Initialize this object from the supplied storage or value state.
  base_parser(const stream& stream, const std::string& error_text)
      : message{create_what_string(stream, error_text)} {}
  // Initialize this object from the supplied storage or value state.
  base_parser(const stream& stream) : message{create_what_string(stream, {})} {}

  // Return the exception message; the pointer remains valid for this exception lifetime.
  const char* what() const noexcept override {
    return message.c_str();
  }
};

} // namespace exception

} // namespace rohit
