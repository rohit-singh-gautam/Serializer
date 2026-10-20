// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/compression.hpp>
#include <rohit/compression_stream.hpp>
#include <rohit/stream_io.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace compression = codec::compression;
namespace fs = std::filesystem;
using bytes = std::vector<std::uint8_t>;
constexpr std::array protocols{"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"};
constexpr std::array formats{"NONE", "GZIP", "ZLIB", "DEFLATE", "ZSTD", "LZ4"};

// Read only bounded fixture files and keep their storage owned throughout decoding.
bytes read_file(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot open compression fixture"};
  }
  const auto storage = rohit::read_stream_bytes(input, 8 * compression::mebibyte);
  return {storage.begin(), storage.begin() + storage.current_offset()};
}

// Publish a complete successful fixture result to its isolated qualification directory.
void write_file(const fs::path& path, const bytes& value) {
  std::ofstream output{path, std::ios::binary};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  rohit::write_stream_bytes(output, value.data(), value.size());
  output.close();
}

// Select the same mandatory core provider profile in every language adapter.
std::pair<compression::format, compression::encode_options> selection(std::string_view name) {
  if (name == "NONE") {
    return {compression::format::none, compression::none_options{}};
  }
  if (name == "GZIP") {
    return {compression::format::gzip, compression::gzip_options{}};
  }
  if (name == "ZLIB") {
    return {compression::format::zlib, compression::zlib_options{}};
  }
  if (name == "DEFLATE") {
    return {compression::format::deflate, compression::deflate_options{}};
  }
  if (name == "ZSTD") {
    return {compression::format::zstd, compression::zstd_options{}};
  }
  if (name == "LZ4") {
    return {compression::format::lz4, compression::lz4_options{}};
  }
  throw std::invalid_argument{"Unknown shared compression format"};
}

// Encode a real owning generated object through an existing selected native codec.
template <template <codec::serialize_type> class Protocol>
bytes encode_object(const account_record& value) {
  rohit::full_stream_auto_alloc output;
  codec::serialize_to<Protocol>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require exact native EOF before returning an independently owned generated value.
template <template <codec::serialize_type> class Protocol>
account_record decode_object(const bytes& value) {
  const auto input = rohit::make_constant_full_stream(value.data(), value.size());
  return codec::deserialize_exact<account_record, Protocol>(input);
}

// Dispatch stable protocol spellings without changing their native wire definitions.
bytes encode_object(const account_record& value, std::string_view protocol) {
  if (protocol == "JSON") {
    return encode_object<codec::json>(value);
  }
  if (protocol == "BINARY_NONE") {
    return encode_object<codec::binary_none>(value);
  }
  if (protocol == "BINARY_INTEGER") {
    return encode_object<codec::binary_integer>(value);
  }
  if (protocol == "BINARY_STRING") {
    return encode_object<codec::binary_string>(value);
  }
  throw std::invalid_argument{"Unknown shared native protocol"};
}

// Decode the selected native representation with the same complete-object boundary.
account_record decode_object(const bytes& value, std::string_view protocol) {
  if (protocol == "JSON") {
    return decode_object<codec::json>(value);
  }
  if (protocol == "BINARY_NONE") {
    return decode_object<codec::binary_none>(value);
  }
  if (protocol == "BINARY_INTEGER") {
    return decode_object<codec::binary_integer>(value);
  }
  if (protocol == "BINARY_STRING") {
    return decode_object<codec::binary_string>(value);
  }
  throw std::invalid_argument{"Unknown shared native protocol"};
}

// Split the runner's bounded tab-separated jobs while preserving every required column.
std::vector<std::string> columns(const std::string& line) {
  std::vector<std::string> result;
  std::size_t begin{};
  while (true) {
    const auto end = line.find('\t', begin);
    result.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos) {
      return result;
    }
    begin = end + 1;
  }
}

// Normalize stable compression categories for the common language-independent oracle.
std::string_view category(compression::error_code code) {
  switch (code) {
  case compression::error_code::unavailable:
    return "unavailable";
  case compression::error_code::invalid_options:
    return "invalid_options";
  case compression::error_code::invalid_data:
    return "invalid_data";
  case compression::error_code::resource_limit:
    return "resource_limit";
  }
  throw std::runtime_error{"Unknown compression error category"};
}

} // namespace

bool* late_cancellation{};

// Decode through the ordinary JSON implementation and simulate cancellation after its owning callback.
template <codec::serialize_type Direction>
class late_json : public codec::json<Direction> {
  using base = codec::json<Direction>;

public:
  using base::base;
  // Retain the native JSON wire contract while exposing the final-publication cancellation boundary.
  template <typename Value>
  void serialize_in(Value& value) {
    base::serialize_in(value);
    *late_cancellation = true;
  }
};

// Fresh owning candidates must not escape when cancellation arrives inside native decoding.
void candidate_controls() {
  account_record value{};
  value.account_id = 42;
  value.label = "candidate";
  const auto plain = encode_object<codec::json>(value);
  for (const auto name : formats) {
    const auto frame = compression::compress(plain, selection(name).second);
    bool stopped{};
    late_cancellation = &stopped;
    int original{};
    auto check = [&] {
      if (stopped) {
        throw &original;
      }
    };
    compression::decode_options options{.format = selection(name).first, .check_cancelled = check};
    const auto input = rohit::make_constant_full_stream(frame.data(), frame.size());
    bool rejected{};
    try {
      static_cast<void>(codec::deserialize_exact<account_record, late_json>(input, {}, options));
    } catch (int* failure) {
      if (failure != &original) {
        throw std::runtime_error{"Original candidate cancellation changed"};
      }
      rejected = true;
    }
    late_cancellation = nullptr;
    if (!rejected) {
      throw std::runtime_error{std::string{name} +
                               ": owning candidate published after native cancellation"};
    }
  }
}

// Swallowed nested operations must fence the active encoder before any subsequent borrowed output.
void reentry_controls() {
  class reentrant_sink final : public compression::output_sink {
  public:
    compression::stream_encoder* encoder{};
    std::exception_ptr nested;
    bool accepted{};
    std::size_t calls{};
    // Reenter only identity finalization so the before-control cannot corrupt native provider state.
    void append(std::span<const std::uint8_t>) override {
      ++calls;
      try {
        encoder->finish();
        accepted = true;
      } catch (...) {
        nested = std::current_exception();
      }
    }
  } destination;
  compression::stream_encoder encoder{destination, compression::none_options{}};
  destination.encoder = &encoder;
  const bytes plain{1, 2, 3};
  std::exception_ptr outer;
  try {
    encoder.write(plain);
  } catch (...) {
    outer = std::current_exception();
  }
  if (destination.accepted || !destination.nested || !outer || !encoder.failed()) {
    throw std::runtime_error{
        "NONE: swallowed sink callback reentry did not fence active encoder with first exception"};
  }
  // MSVC may copy exception objects for exception_ptr; observable category/message remain portable.
  std::string nested_message, outer_message;
  try {
    std::rethrow_exception(destination.nested);
  } catch (const std::logic_error& failure) {
    nested_message = failure.what();
  }
  try {
    std::rethrow_exception(outer);
  } catch (const std::logic_error& failure) {
    outer_message = failure.what();
  }
  if (nested_message.empty() || nested_message != outer_message) {
    throw std::runtime_error{"Nested reentry failure category or provenance changed"};
  }
  const auto calls = destination.calls;
  try {
    encoder.finish();
  } catch (const std::logic_error&) {
  }
  if (destination.calls != calls) {
    throw std::runtime_error{"Reentrant encoder retried borrowed sink"};
  }
}

// A cancellation raised by one borrowed read must stop before another source callback.
void source_controls() {
  class source_buffer final : public std::streambuf {
    const bytes& input;
    bool& stopped;
    std::size_t offset{};

  public:
    std::size_t calls{};
    // Borrow the fixed input and cancellation state for one input wrapper construction.
    source_buffer(const bytes& input, bool& stopped) : input{input}, stopped{stopped} {}
    // Return full requested batches and cancel on the first callback, exposing extra reads reliably.
    std::streamsize xsgetn(char* destination, std::streamsize requested) override {
      const auto count = std::min(input.size() - offset, static_cast<std::size_t>(requested));
      std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(offset), count, destination);
      offset += count;
      ++calls;
      stopped = true;
      return static_cast<std::streamsize>(count);
    }
  };
  bytes plain(131073);
  std::uint32_t state{107};
  for (auto& value : plain) {
    state = state * 1664525u + 1013904223u;
    value = static_cast<std::uint8_t>(state >> 24);
  }
  for (const auto name : formats) {
    const auto frame = compression::compress(plain, selection(name).second);
    bool stopped{};
    int original{};
    source_buffer buffer{frame, stopped};
    std::istream source{&buffer};
    auto check = [&] {
      if (stopped) {
        throw &original;
      }
    };
    const compression::decode_options options{.format = selection(name).first,
                                              .check_cancelled = check};
    bool rejected{};
    try {
      compression::input_stream candidate{source, options};
    } catch (int* failure) {
      if (failure != &original) {
        throw std::runtime_error{"Source cancellation changed"};
      }
      rejected = true;
    }
    if (!rejected || buffer.calls != 1) {
      throw std::runtime_error{std::string{name} + ": first-read cancellation used " +
                               std::to_string(buffer.calls) + " borrowed source callbacks"};
    }
  }
}

// Cached input still observes the borrowed policy when explicit complete-message finalization occurs.
void input_finish_controls() {
  const bytes plain{1, 2, 3};
  for (const auto name : formats) {
    const auto frame = compression::compress(plain, selection(name).second);
    std::istringstream source{
        std::string{reinterpret_cast<const char*>(frame.data()), frame.size()}};
    bool stopped{};
    int original{};
    auto check = [&] {
      if (stopped) {
        throw &original;
      }
    };
    const compression::decode_options options{.format = selection(name).first,
                                              .check_cancelled = check};
    compression::input_stream candidate{source, options};
    candidate.get_curr_and_increase_unchecked(candidate.remaining_buffer());
    stopped = true;
    bool rejected{};
    try {
      candidate.finish();
    } catch (int* failure) {
      if (failure != &original) {
        throw std::runtime_error{"Input finish cancellation changed"};
      }
      rejected = true;
    }
    if (!rejected) {
      throw std::runtime_error{std::string{name} + ": cached input finish accepted cancellation"};
    }
  }
}

// Require cancellation raised by the final borrowed sink callback to fence the active operation.
void cancellation_controls() {
  class callback_sink final : public compression::output_sink {
    bool& stopped;
    const std::size_t stop_at;

  public:
    std::size_t calls{};
    // Borrow cancellation state solely for this qualification operation.
    callback_sink(bool& stopped, std::size_t stop_at) : stopped{stopped}, stop_at{stop_at} {}
    // Simulate cancellation arriving from the last external write callback.
    void append(std::span<const std::uint8_t>) override {
      ++calls;
      if (calls == stop_at) {
        stopped = true;
      }
    }
  };
  const bytes plain(70001, 65);
  for (const auto name : formats) {
    bool stopped{};
    callback_sink baseline{stopped, 0};
    compression::stream_encoder first{baseline, selection(name).second};
    first.write(plain);
    first.finish();
    callback_sink destination{stopped, baseline.calls};
    int original{};
    auto check = [&] {
      if (stopped) {
        throw &original;
      }
    };
    compression::stream_encoder candidate{
        destination, selection(name).second, {plain.size(), 8 * compression::mebibyte, check}};
    bool rejected{};
    try {
      candidate.write(plain);
      candidate.finish();
    } catch (int* failure) {
      if (failure != &original) {
        throw std::runtime_error{"Original cancellation changed"};
      }
      rejected = true;
    }
    if (!rejected) {
      throw std::runtime_error{std::string{name} +
                               ": active encoder accepted cancellation from final sink callback"};
    }
    const auto calls = destination.calls;
    try {
      candidate.finish();
    } catch (const std::logic_error&) {
    }
    if (!candidate.failed() || destination.calls != calls) {
      throw std::runtime_error{"Failed encoder retried borrowed output"};
    }
  }
}

// Require every selected language's frame through exact native owning-model decoding.
int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      throw std::invalid_argument{"Expected mode and corpus directory"};
    }
    const fs::path root{argv[2]};
    if (std::string_view{argv[1]} == "controls") {
      cancellation_controls();
      std::cout << "{\"controls\":6}\n";
      return 0;
    }
    if (std::string_view{argv[1]} == "candidate-controls") {
      candidate_controls();
      std::cout << "{\"controls\":6}\n";
      return 0;
    }
    if (std::string_view{argv[1]} == "reentry-controls") {
      reentry_controls();
      std::cout << "{\"controls\":1}\n";
      return 0;
    }
    if (std::string_view{argv[1]} == "source-controls") {
      source_controls();
      std::cout << "{\"controls\":6}\n";
      return 0;
    }
    if (std::string_view{argv[1]} == "input-finish-controls") {
      input_finish_controls();
      std::cout << "{\"controls\":6}\n";
      return 0;
    }
    for (const auto name : formats) {
      if (!compression::available(selection(name).first)) {
        throw std::runtime_error{"Missing C++ provider"};
      }
    }
    if (std::string_view{argv[1]} == "produce") {
      const bytes input{'{', '"', 'a', 'c', 'c', 'o',  'u',  'n', 't',  '_',  'i',  'd',  '"', ':',
                        '4', '2', ',', '"', 'l', 'a',  'b',  'e', 'l',  '"',  ':',  '"',  's', 'h',
                        'a', 'r', 'e', 'd', ' ', 0xce, 0xbb, ' ', 0xf0, 0x9f, 0x99, 0x82, '"', '}'};
      const auto value = decode_object<codec::json>(input);
      for (const auto protocol : protocols) {
        for (const auto name : formats) {
          const auto stem = std::string{"cpp-"} + protocol + "-" + name;
          const auto plain = encode_object(value, protocol);
          const auto options = selection(name).second;
          const auto frame = compression::compress(plain, options);
          if (compression::compress(plain, options, {plain.size(), frame.size()}) != frame) {
            throw std::runtime_error{"Exact encoding bounds changed native bytes"};
          }
          for (const auto bounds : std::array{std::pair{plain.size() - 1, frame.size()},
                                              std::pair{plain.size(), frame.size() - 1}}) {
            bool rejected{};
            try {
              static_cast<void>(
                  compression::compress(plain, options, {bounds.first, bounds.second}));
            } catch (const compression::error& failure) {
              if (failure.code != compression::error_code::resource_limit) {
                throw;
              }
              rejected = true;
            }
            if (!rejected) {
              throw std::runtime_error{"One-tight encoding bound accepted"};
            }
          }
          write_file(root / (stem + ".frame"), frame);
          write_file(root / (stem + ".plain"), plain);
        }
      }
      std::cout << "{\"produced\":24,\"encode_bounds\":72}\n";
      return 0;
    }
    std::ifstream cases{root / "cases.tsv"}, jobs{root / "exchange.tsv"};
    if (!cases || !jobs) {
      throw std::runtime_error{"Missing shared manifests"};
    }
    std::size_t decisions{}, exchanges{};
    std::string line;
    while (std::getline(cases, line)) {
      const auto row = columns(line);
      if (row.size() != 8) {
        throw std::runtime_error{"Incomplete case"};
      }
      std::string observed{"ok"};
      try {
        const compression::decode_options options{
            .format = selection(row[1]).first,
            .max_compressed_bytes = static_cast<std::size_t>(std::stoull(row[5])),
            .max_decompressed_bytes = static_cast<std::size_t>(std::stoull(row[6])),
            .max_window_bytes = static_cast<std::size_t>(std::stoull(row[7]))};
        if (const auto plain = compression::decompress(read_file(root / row[2]), options);
            row[4] == "ok" && plain != read_file(root / row[3])) {
          throw std::runtime_error{row[0] + ": plain differs"};
        }
      } catch (const compression::error& failure) {
        observed = category(failure.code);
      }
      if (observed != row[4]) {
        throw std::runtime_error{row[0] + ": " + observed + " != " + row[4]};
      }
      ++decisions;
    }
    for (; std::getline(jobs, line); ++exchanges) {
      const auto row = columns(line);
      if (row.size() != 5) {
        throw std::runtime_error{"Incomplete exchange"};
      }
      const auto plain =
          compression::decompress(read_file(root / row[3]), {.format = selection(row[2]).first});
      const auto value = decode_object(plain, row[1]);
      if (value.account_id != 42 || value.label != "shared \xce\xbb \xf0\x9f\x99\x82" ||
          plain != read_file(root / row[4])) {
        throw std::runtime_error{row[0] + ": model differs"};
      }
    }
    std::cout << "{\"decisions\":" << decisions << ",\"exchanges\":" << exchanges << "}\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
