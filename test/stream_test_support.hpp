#pragma once

#include <rohit/stream.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace stream_test {
// An independent input implementation needs no inheritance or mutable byte access.
class input_cursor {
  const std::uint8_t* bytes;
  const std::size_t size;
  mutable std::size_t offset{};

public:
  // Borrow one exact message; the owner must outlive all decoder access.
  explicit input_cursor(std::string_view text)
      : bytes{reinterpret_cast<const std::uint8_t*>(text.data())}, size{text.size()} {}
  // Expose the unread bytes without changing their lifetime.
  const std::uint8_t* curr() const {
    return bytes + offset;
  }
  // Report the number of bytes available for bounds checks.
  std::size_t remaining_buffer() const {
    return size - offset;
  }
  // Report the end of this message.
  bool full() const {
    return offset == size;
  }
  // Consume a range whose length the decoder has already checked.
  const std::uint8_t* get_curr_and_increase_unchecked(std::size_t count) const noexcept {
    const auto* result = curr();
    offset += count;
    return result;
  }
};

// Composition exercises structural output constraints without a rohit::stream base class.
class output_buffer {
  rohit::full_stream_auto_alloc storage{};

public:
  // Expose unused capacity for the diagnostic contract.
  const std::uint8_t* curr() const {
    return storage.curr();
  }
  // Report unused capacity for the diagnostic contract.
  std::size_t remaining_buffer() const {
    return storage.remaining_buffer();
  }
  // Preserve the allocation and failure behavior of the composed storage policy.
  void reserve(std::size_t size) {
    storage.reserve(size);
  }
  // Claim a writable range after the caller has reserved it.
  std::uint8_t* get_curr_and_increase_unchecked(std::size_t size) noexcept {
    return storage.get_curr_and_increase_unchecked(size);
  }
  // Forward supported byte sources while preserving alias handling.
  template <typename... Args>
  void append(Args&&... args) {
    storage.append(std::forward<Args>(args)...);
  }
  // Copy bytes that are independent of output storage.
  void append_external(const void* data, std::size_t size) {
    storage.append_external(data, size);
  }
  // Preserve transformation batching and source lifetime across growth.
  template <typename Transform>
  void append_transformed(const void* data, std::size_t size, std::size_t output_size,
                          Transform&& transform) {
    storage.append_transformed(data, size, output_size, std::forward<Transform>(transform));
  }
  // Format integral values through the existing locale-independent formatter.
  template <typename T>
  void append_string(const T& value) {
    storage.append_string(value);
  }
  // Append text fragments with the same batching policy as native storage.
  template <typename... Args>
  void write(const Args&... args) {
    storage.write(args...);
  }
  // Append a complete compact integer byte batch.
  template <typename... Args>
  void write_raw(const Args&... args) {
    storage.write_raw(args...);
  }
  // Copy only bytes actually written by the codec.
  std::string bytes() const {
    return {reinterpret_cast<const char*>(storage.begin()), storage.current_offset()};
  }
};

} // namespace stream_test
