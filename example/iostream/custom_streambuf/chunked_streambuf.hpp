#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <streambuf>
#include <string>

namespace iostream_example {
// A single-message memory transport exposing tiny input chunks and no seek operations.
// Finish writing before reading; the buffer must outlive both standard stream objects.
class chunked_streambuf : public std::streambuf {
  static constexpr std::size_t read_chunk_bytes = 257;
  std::string message;
  std::array<char, read_chunk_bytes> read_area{};
  std::size_t next_byte{};

protected:
  // Append the complete write or propagate an allocation failure to the standard stream.
  std::streamsize xsputn(const char* source, std::streamsize count) override {
    if (count <= 0) {
      return 0;
    }
    message.append(source, static_cast<std::size_t>(count));
    return count;
  }

  // Support single-character writes as well as bulk ostream::write calls.
  int_type overflow(int_type value) override {
    if (!traits_type::eq_int_type(value, traits_type::eof())) {
      message.push_back(traits_type::to_char_type(value));
    }
    return traits_type::not_eof(value);
  }

  // Expose the next bounded chunk; basic_streambuf::xsgetn combines chunks for istream::read.
  int_type underflow() override {
    if (gptr() != egptr()) {
      return traits_type::to_int_type(*gptr());
    }
    if (next_byte == message.size()) {
      return traits_type::eof();
    }
    const auto count = std::min(read_area.size(), message.size() - next_byte);
    std::copy_n(message.data() + next_byte, count, read_area.data());
    next_byte += count;
    setg(read_area.data(), read_area.data(), read_area.data() + count);
    return traits_type::to_int_type(*gptr());
  }

  // Writes already reach the owned message, so flushing has no pending work.
  int sync() override {
    return 0;
  }

public:
  // Start with empty storage and no exposed input chunk.
  chunked_streambuf() = default;
  // Streambuf get-area pointers refer to this object's read_area and cannot be copied safely.
  chunked_streambuf(const chunked_streambuf&) = delete;
  chunked_streambuf& operator=(const chunked_streambuf&) = delete;
  chunked_streambuf(chunked_streambuf&&) = delete;
  chunked_streambuf& operator=(chunked_streambuf&&) = delete;

  // Report the full message size independently of the current input position.
  std::size_t size() const {
    return message.size();
  }
};
} // namespace iostream_example
