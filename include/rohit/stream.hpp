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
#include <algorithm>
#include <bit>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rohit {

namespace detail {
inline constexpr std::size_t identifier_hash_seed = 100000000003ULL;
inline constexpr unsigned identifier_hash_shift_bits = 9;
inline constexpr std::size_t buffer_growth_factor = 2;
inline constexpr std::size_t default_minimum_read_buffer_bytes = 1024;
inline constexpr std::size_t default_maximum_read_buffer_bytes = 8192;
} // namespace detail

// Reverse the bytes of a supported scalar without changing its bit pattern.
template <typename T>
constexpr T byteswap(const T& val) {
#if __cpp_lib_byteswap
  return std::byteswap(val);
#else
  constexpr auto bits_per_byte = std::numeric_limits<unsigned char>::digits;
  if constexpr (sizeof(val) == 2) {
    if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
      const auto low_bytes = static_cast<std::make_unsigned_t<T>>(val) >> bits_per_byte;
      const auto high_bytes = static_cast<std::make_unsigned_t<T>>(val) << bits_per_byte;
      return static_cast<T>(low_bytes | high_bytes);
    } else {
      return (val >> bits_per_byte) | (val << bits_per_byte);
    }
  } else if constexpr (sizeof(val) == 4) {
    T result = ((val >> bits_per_byte) & 0x0000FF00) | ((val << bits_per_byte) & 0x00FF0000);
    if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
      const auto low_bytes = static_cast<std::make_unsigned_t<T>>(val) >> (3 * bits_per_byte);
      const auto high_bytes = static_cast<std::make_unsigned_t<T>>(val) << (3 * bits_per_byte);
      return static_cast<T>(low_bytes | high_bytes | result);
    } else {
      return (val >> (3 * bits_per_byte)) | (val << (3 * bits_per_byte)) | result;
    }
  } else if constexpr (sizeof(val) == 8) {
    T result = ((val >> (5 * bits_per_byte)) & 0x000000000000FF00ULL) |
               ((val >> (3 * bits_per_byte)) & 0x0000000000FF0000ULL) |
               ((val >> bits_per_byte) & 0x00000000FF000000ULL) |
               ((val << bits_per_byte) & 0x000000FF00000000ULL) |
               ((val << (3 * bits_per_byte)) & 0x0000FF0000000000ULL) |
               ((val << (5 * bits_per_byte)) & 0x00FF000000000000ULL);
    if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
      const auto low_bytes = static_cast<std::make_unsigned_t<T>>(val) >> (7 * bits_per_byte);
      const auto high_bytes = static_cast<std::make_unsigned_t<T>>(val) << (7 * bits_per_byte);
      return static_cast<T>(low_bytes | high_bytes | result);
    } else {
      return (val >> (7 * bits_per_byte)) | (val << (7 * bits_per_byte)) | result;
    }
  } else {
    static_assert(false, "Unsupported type");
  }
#endif
}

template <std::endian source, std::endian destination, typename T>
// Convert a scalar between byte orders; equal byte orders require no work.
constexpr T change_endian(const T& val) {
  if constexpr (source == destination || sizeof(val) == 1) {
    return val;
  } else {
    return byteswap(val);
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
  for (auto ch : str) {
    ret = ((ret << detail::identifier_hash_shift_bits) + ret) ^ static_cast<std::size_t>(ch);
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

// A borrowed buffer cursor. The caller owns storage and validates unchecked access.
// Const views can advance their mutable cursor while keeping the underlying bytes read-only.
class stream {
protected:
  friend class fixed_buffer;
  mutable std::uint8_t* current_data;
  std::uint8_t* end_data;

  // Reject a cursor or requested range outside the available buffer.
  void check_overflow() const {
    if (current_data >= end_data) {
      throw exception::stream_overflow_exception{};
    }
  }
  // Reject a cursor or requested range outside the available buffer.
  void check_overflow(std::size_t len) const {
    if (current_data + len > end_data) {
      throw exception::stream_overflow_exception{};
    }
  }

  // Initialize this object from the supplied storage or value state.
  stream() : current_data{nullptr}, end_data{nullptr} {}

public:
  // Initialize this object from the supplied storage or value state.
  stream(auto* begin_data, auto* end_data)
      : current_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{reinterpret_cast<std::uint8_t*>(end_data)} {}
  // Initialize this object from the supplied storage or value state.
  stream(auto* begin_data, std::size_t size)
      : current_data{reinterpret_cast<std::uint8_t*>(begin_data)}, end_data{current_data + size} {}
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

  // Compare the relevant values without modifying either operand.
  template <std::size_t Size>
  bool operator==(const auto (&data)[Size + 1]) const {
    if (remaining_buffer() < Size) {
      return false;
    }
    return std::equal(std::begin(data), std::end(data), current_data);
  }

  /*! IMPORTANT: text must not be null terminated */
  bool operator==(const std::string_view& text) const {
    if (remaining_buffer() < text.size()) {
      return false;
    }
    return std::equal(std::begin(text), std::end(text), current_data);
  }

  // Assign the documented view or value state from the source object.
  stream& operator=(const stream& stream) {
    current_data = stream.current_data;
    return *this;
  }
  // Assign the documented view or value state from the source object.
  const stream& operator=(const stream& stream) const {
    current_data = stream.current_data;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual stream operator+(std::size_t len) {
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual const stream operator+(std::size_t len) const {
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual stream& operator+=(std::size_t len) {
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  virtual const stream& operator+=(std::size_t len) const {
    current_data += len;
    return *this;
  }

  // Access the referenced value; the caller must provide a valid cursor or pointer.
  inline std::uint8_t& operator*() {
    return *current_data;
  }
  // Access the referenced value; the caller must provide a valid cursor or pointer.
  inline std::uint8_t operator*() const {
    return *current_data;
  }
  // Access the current byte; the caller must ensure the cursor is in bounds.
  inline std::uint8_t& at_unchecked() {
    return *current_data;
  }
  // Access the current byte; the caller must ensure the cursor is in bounds.
  inline std::uint8_t at_unchecked() const {
    return *current_data;
  }
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline stream& operator++() {
    ++current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  virtual inline stream& operator--() {
    --current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline const stream& operator++() const {
    ++current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  virtual inline const stream& operator--() const {
    --current_data;
    return *this;
  }
  // Write one byte and advance the cursor through the stream movement policy.
  inline void push(const auto ch) {
    operator*() = static_cast<std::uint8_t>(ch);
    operator++();
  }

  // Return the current byte pointer and advance by the requested byte count.
  virtual inline std::uint8_t* get_curr_and_increase(const std::size_t len) {
    auto temp = current_data;
    current_data += len;
    return temp;
  }
  // Return the current byte pointer and advance by the requested byte count.
  virtual inline const std::uint8_t* get_curr_and_increase(const std::size_t len) const {
    auto temp = current_data;
    current_data += len;
    return temp;
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
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Move the cursor back one byte using the stream movement policy.
  virtual inline std::uint8_t* operator--(int) {
    std::uint8_t* temp = current_data;
    --current_data;
    return temp;
  };
  // Advance the cursor by one byte using the stream movement policy.
  virtual inline const std::uint8_t* operator++(int) const {
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Move the cursor back one byte using the stream movement policy.
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

  // Report the number of bytes between the cursor and the buffer end.
  std::size_t remaining_buffer() const {
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
    const std::size_t len =
        reinterpret_cast<const char*>(end_data) - reinterpret_cast<const char*>(begin_data);
    reserve(len);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::string_view& source) {
    reserve(source.size());
    current_data = std::copy(std::begin(source), std::end(source), current_data);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::string& source) {
    reserve(source.size());
    current_data = std::copy(std::begin(source), std::end(source), current_data);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const stream& source) {
    reserve(source.remaining_buffer());
    current_data = std::copy(source.curr(), source.end(), current_data);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const auto* begin, const auto* end) {
    reserve(begin, end);
    current_data = std::copy(reinterpret_cast<const std::uint8_t*>(begin),
                             reinterpret_cast<const std::uint8_t*>(end), current_data);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const auto* begin, std::size_t size) {
    reserve(size);
    current_data = std::copy(reinterpret_cast<const std::uint8_t*>(begin),
                             reinterpret_cast<const std::uint8_t*>(begin) + size, current_data);
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const char value) {
    reserve(value);
    *current_data++ = value;
  }
  // Append raw bytes and advance the cursor; capacity failures follow the stream policy.
  inline void append(const std::uint8_t value) {
    reserve(value);
    *current_data++ = value;
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

  // Emit C++ raw for the parsed schema.
  template <typename... ValueType>
  inline void write_raw(const ValueType&... value) {
    ((append(value)), ...);
  }

  // Append a supported value as text, excluding a string literal terminator.
  template <typename ValueType>
  inline void append_string(const ValueType& value) {
    if constexpr (std::is_same_v<ValueType, char>) {
      append(value);
    } else if constexpr (std::is_integral_v<ValueType>) {
      // Allow one extra decimal digit, a sign, and the existing spare byte.
      constexpr std::size_t decimal_format_extra_bytes = 3;
      char buffer[std::numeric_limits<ValueType>::digits10 + decimal_format_extra_bytes]{};
      auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
      append(buffer, result.ptr);
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
        static_assert(false, "Unsupported type");
      }
    } else if constexpr (std::is_same_v<ValueType, std::string>) {
      append(value);
    } else if constexpr (std::is_same_v<ValueType, std::string_view>) {
      append(value);
    } else {
      static_assert(false, "Unsupported type");
    }
  }

  // Append each supported argument as text in argument order.
  template <typename... ValueType>
  inline void write(const ValueType&... value) {
    ((append_string(value)), ...);
  }
}; // class stream

// A borrowed cursor that also retains the buffer start for offsets and rewinding.
class full_stream : public stream {
protected:
  friend class fixed_buffer;
  std::uint8_t* begin_data;

  // Reject moving the cursor before the start of the buffer.
  void check_underflow() const {
    if (current_data == begin_data) {
      throw exception::stream_underflow_exception{};
    }
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
        begin_data{reinterpret_cast<std::uint8_t*>(begin_data)} {}
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
    current_data = stream.current_data;
    return *this;
  }

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
    return static_cast<std::size_t>(current_data - begin_data);
  }
  // Report the complete buffer capacity in bytes.
  auto capacity() const {
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

  // Emit C++ to file till offset for the parsed schema.
  void write_to_file_till_offset(const std::filesystem::path& path) {
    std::ofstream file_stream{path, std::ios::binary};
    file_stream.write(reinterpret_cast<const char*>(begin_data), current_offset());
    file_stream.close();
  }
  // Emit C++ to file complete for the parsed schema.
  void write_to_file_complete(const std::filesystem::path& path) {
    std::ofstream file_stream{path, std::ios::binary};
    file_stream.write(reinterpret_cast<const char*>(begin_data), capacity());
    file_stream.close();
  }
};

class stream_auto_free : public full_stream {
public:
  using full_stream::full_stream;
  // Release resources owned by this object.
  ~stream_auto_free() {
    free(begin_data);
  }
};

class full_stream_limit_checked : public full_stream {
public:
  using full_stream::full_stream;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Move the cursor back one byte using the stream movement policy.
  inline stream& operator--() override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  inline const stream& operator--() const override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  inline const std::uint8_t* operator--(int) const override {
    check_underflow();
    const std::uint8_t* temp = current_data;
    --current_data;
    return temp;
  };

  stream operator+(std::size_t len) override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  const stream operator+(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  stream& operator+=(std::size_t len) override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  const stream& operator+=(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline stream& operator++() override {
    ++current_data;
    check_overflow();
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline const stream& operator++() const override {
    ++current_data;
    check_overflow();
    return *this;
  }
  // Return the current byte pointer and advance by the requested byte count.
  inline const std::uint8_t* get_curr_and_increase(const std::size_t len) const override {
    auto temp = current_data;
    check_overflow();
    current_data += len;
    return temp;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
};

class full_stream_auto_alloc : public full_stream {
  // Grow owned storage when needed; existing views may be invalidated.
  inline void check_resize() {
    if (current_data == end_data) {
      auto cursor_offset_bytes = current_offset();
      auto new_capacity_bytes = capacity() * detail::buffer_growth_factor;
      begin_data = reinterpret_cast<std::uint8_t*>(
          realloc(reinterpret_cast<void*>(begin_data), new_capacity_bytes));
      end_data = begin_data + new_capacity_bytes;
      current_data = begin_data + cursor_offset_bytes;
    }
  }

  // Grow owned storage when needed; existing views may be invalidated.
  inline void check_resize(const std::size_t len) {
    if (current_data + len > end_data) {
      auto cursor_offset_bytes = current_offset();
      auto new_capacity_bytes = capacity() * detail::buffer_growth_factor;
      while (cursor_offset_bytes + len > new_capacity_bytes) {
        new_capacity_bytes += capacity();
      }
      begin_data = reinterpret_cast<std::uint8_t*>(
          realloc(reinterpret_cast<void*>(begin_data), new_capacity_bytes));
      if (begin_data == nullptr) {
        throw exception::memory_allocation_exception{};
      }
      end_data = begin_data + new_capacity_bytes;
      current_data = begin_data + cursor_offset_bytes;
    }
  }

public:
  using full_stream::full_stream;
  // Initialize this object from the supplied storage or value state.
  full_stream_auto_alloc(const std::size_t size)
      : full_stream{reinterpret_cast<std::uint8_t*>(malloc(size)), size} {
    if (begin_data == nullptr) {
      throw exception::memory_allocation_exception{};
    }
  }
  // Initialize this object from the supplied storage or value state.
  full_stream_auto_alloc() : full_stream{} {}
  // Release resources owned by this object.
  ~full_stream_auto_alloc() {
    free(begin_data);
  }

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Move the cursor back one byte using the stream movement policy.
  inline stream& operator--() override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  inline const stream& operator--() const override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline stream& operator++() override {
    check_resize();
    ++current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline const stream& operator++() const override {
    check_overflow();
    ++current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline std::uint8_t* operator++(int) override {
    check_resize();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Advance the cursor by one byte using the stream movement policy.
  inline const std::uint8_t* operator++(int) const override {
    check_overflow();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Advance the cursor using the stream bounds or allocation policy.
  inline stream operator+(std::size_t len) override {
    check_resize(len);
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline const stream operator+(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline stream& operator+=(std::size_t len) override {
    check_resize(len);
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline const stream& operator+=(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Ensure capacity for a write using the stream allocation or bounds policy.
  inline void reserve(const std::size_t len) override {
    check_resize(len);
  }

  // Return the current byte pointer and advance by the requested byte count.
  inline std::uint8_t* get_curr_and_increase(const std::size_t len) override {
    check_resize(len);
    auto temp = current_data;
    current_data += len;
    return temp;
  }
  // Return the current byte pointer and advance by the requested byte count.
  inline const std::uint8_t* get_curr_and_increase(const std::size_t len) const override {
    auto temp = current_data;
    current_data += len;
    check_overflow();
    return temp;
  }

  // Transfer the old allocation to the caller and start a fresh buffer.
  auto return_old_and_alloc(const std::size_t size) {
    full_stream stream{begin_data, end_data, current_data};
    begin_data = reinterpret_cast<std::uint8_t*>(malloc(size));
    if (begin_data == nullptr) {
      throw exception::memory_allocation_exception{};
    }
    current_data = begin_data;
    end_data = begin_data + size;
    return stream;
  }
};

struct stream_limits {
  std::size_t min_read_buffer_bytes{detail::default_minimum_read_buffer_bytes};
  std::size_t max_read_buffer_bytes{detail::default_maximum_read_buffer_bytes};
};

class full_stream_auto_alloc_limits : public full_stream {
  const stream_limits* limits{nullptr};

  // Grow owned storage when needed; existing views may be invalidated.
  inline void check_resize() {
    if (current_data == end_data) {
      auto cursor_offset_bytes = current_offset();
      auto new_capacity_bytes = capacity() * detail::buffer_growth_factor;
      if (new_capacity_bytes > limits->max_read_buffer_bytes) {
        throw exception::stream_overflow_exception{};
      }
      begin_data = reinterpret_cast<std::uint8_t*>(
          realloc(reinterpret_cast<void*>(begin_data), new_capacity_bytes));
      if (begin_data == nullptr) {
        throw exception::memory_allocation_exception{};
      }
      end_data = begin_data + new_capacity_bytes;
      current_data = begin_data + cursor_offset_bytes;
    }
  }

  // Grow owned storage when needed; existing views may be invalidated.
  inline void check_resize(const std::size_t len) {
    if (current_data + len > end_data) {
      auto cursor_offset_bytes = current_offset();
      auto new_capacity_bytes = capacity() * detail::buffer_growth_factor;
      while (cursor_offset_bytes + len > new_capacity_bytes) {
        new_capacity_bytes += capacity();
      }
      if (new_capacity_bytes > limits->max_read_buffer_bytes) {
        throw exception::stream_overflow_exception{};
      }
      begin_data = reinterpret_cast<std::uint8_t*>(
          realloc(reinterpret_cast<void*>(begin_data), new_capacity_bytes));
      if (begin_data == nullptr) {
        throw exception::memory_allocation_exception{};
      }
      end_data = begin_data + new_capacity_bytes;
      current_data = begin_data + cursor_offset_bytes;
    }
  }

  // Grow owned storage to the configured minimum while preserving the cursor offset.
  inline void set_min_buffer() {
    auto current_buffer_bytes = static_cast<std::size_t>(end_data - begin_data);
    if (current_buffer_bytes < limits->min_read_buffer_bytes) {
      auto cursor_offset_bytes = current_offset();
      begin_data = reinterpret_cast<std::uint8_t*>(
          realloc(reinterpret_cast<void*>(begin_data), limits->min_read_buffer_bytes));
      if (begin_data == nullptr) {
        throw exception::memory_allocation_exception{};
      }
      end_data = begin_data + limits->min_read_buffer_bytes;
      current_data = begin_data + cursor_offset_bytes;
    }
  }

public:
  using full_stream::full_stream;
  // Initialize this object from the supplied storage or value state.
  full_stream_auto_alloc_limits(const stream_limits* limits)
      : full_stream{reinterpret_cast<std::uint8_t*>(malloc(limits->min_read_buffer_bytes)),
                    limits->min_read_buffer_bytes},
        limits{limits} {
    if (begin_data == nullptr) {
      throw exception::memory_allocation_exception{};
    }
  }
  // Initialize this object from the supplied storage or value state.
  full_stream_auto_alloc_limits() : full_stream{} {}
  // Release resources owned by this object.
  ~full_stream_auto_alloc_limits() {
    free(begin_data);
  }
  // Initialize this object from the supplied storage or value state.
  full_stream_auto_alloc_limits(const full_stream_auto_alloc_limits&) = delete;
  // Assign the documented view or value state from the source object.
  full_stream_auto_alloc_limits& operator=(const full_stream_auto_alloc_limits&) = delete;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif
  // Move the cursor back one byte using the stream movement policy.
  inline stream& operator--() override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Move the cursor back one byte using the stream movement policy.
  inline const stream& operator--() const override {
    check_underflow();
    --current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline stream& operator++() override {
    check_resize();
    ++current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline const stream& operator++() const override {
    check_overflow();
    ++current_data;
    return *this;
  }
  // Advance the cursor by one byte using the stream movement policy.
  inline std::uint8_t* operator++(int) override {
    check_resize();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Advance the cursor by one byte using the stream movement policy.
  inline const std::uint8_t* operator++(int) const override {
    check_overflow();
    std::uint8_t* temp = current_data;
    ++current_data;
    return temp;
  };
  // Advance the cursor using the stream bounds or allocation policy.
  inline stream operator+(std::size_t len) override {
    check_resize(len);
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline const stream operator+(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline stream& operator+=(std::size_t len) override {
    check_resize(len);
    current_data += len;
    return *this;
  }
  // Advance the cursor using the stream bounds or allocation policy.
  inline const stream& operator+=(std::size_t len) const override {
    current_data += len;
    check_overflow();
    return *this;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // Ensure capacity for a write using the stream allocation or bounds policy.
  inline void reserve(const std::size_t len) override {
    check_resize(len);
  }

  // Return the current byte pointer and advance by the requested byte count.
  inline std::uint8_t* get_curr_and_increase(const std::size_t len) override {
    check_resize(len);
    auto temp = current_data;
    current_data += len;
    return temp;
  }
  // Return the current byte pointer and advance by the requested byte count.
  inline const std::uint8_t* get_curr_and_increase(const std::size_t len) const override {
    auto temp = current_data;
    current_data += len;
    check_overflow();
    return temp;
  }

  // Transfer the old allocation to the caller and start a fresh buffer.
  auto return_old_and_alloc() {
    full_stream stream{begin_data, end_data, current_data};
    begin_data = reinterpret_cast<std::uint8_t*>(malloc(limits->min_read_buffer_bytes));
    if (begin_data == nullptr) {
      throw exception::memory_allocation_exception{};
    }
    current_data = begin_data;
    end_data = begin_data + limits->min_read_buffer_bytes;
    return stream;
  }
};

class fixed_buffer {
  std::uint8_t* begin_data;
  std::uint8_t* end_data;

public:
  // Initialize this object from the supplied storage or value state.
  fixed_buffer(auto* begin_data, auto* end_data)
      : begin_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{reinterpret_cast<std::uint8_t*>(end_data)} {}
  // Initialize this object from the supplied storage or value state.
  fixed_buffer(auto* begin_data, std::size_t size)
      : begin_data{reinterpret_cast<std::uint8_t*>(begin_data)},
        end_data{reinterpret_cast<std::uint8_t*>(begin_data) + size} {}
  // Initialize this object from the supplied storage or value state.
  fixed_buffer(fixed_buffer&& buffer) : begin_data{buffer.begin_data}, end_data{buffer.end_data} {
    buffer.begin_data = buffer.end_data = nullptr;
  }
  // Initialize this object from the supplied storage or value state.
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
// Load a regular file into owned storage; throw when the file cannot be opened.
inline const stream_auto_free make_stream_from_file(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    throw std::invalid_argument{"Not a valid file"};
  }

  std::ifstream file_stream{path, std::ios::binary | std::ios::ate};
  if (!file_stream.is_open()) {
    throw std::runtime_error{"Unable to open file"};
  }

  file_stream.seekg(0, std::ios::end);
  std::size_t size = file_stream.tellg();
  file_stream.seekg(0, std::ios::beg);

  auto buffer = reinterpret_cast<char*>(malloc(size));
  file_stream.read(buffer, size);
  size = file_stream.gcount();

  file_stream.close();
  return stream_auto_free{buffer, size};
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
