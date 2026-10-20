// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
constexpr std::uint64_t max_fixture_bytes = 64 * package::mebibyte;
constexpr std::array<char, 8> fixture_signature{'S', 'Z', 'I', 'P', '6', '4', '\0', '\1'};

// Decode independent little-endian fixture integers without relying on host byte order.
template <typename UInt>
UInt read_integer(std::istream& input) {
  std::array<unsigned char, sizeof(UInt)> bytes{};
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  UInt value{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    value |= static_cast<UInt>(bytes[index]) << (index * 8U);
  }
  return value;
}

struct segment {
  std::uint64_t offset{};
  std::vector<char> bytes{};
};

// Model logical multi-gigabyte input with bounded independent byte segments and zero-filled gaps.
class segmented_buffer final : public std::streambuf {
  std::uint64_t size_{};
  std::uint64_t position_{};
  std::vector<segment> segments_{};
public:
  std::uint64_t read_bytes{};
  std::uint64_t read_calls{};
  std::uint64_t largest_read{};

  // Read only a small fixture description; reject extents before retaining their stored bytes.
  explicit segmented_buffer(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    input.exceptions(std::ios::failbit | std::ios::badbit);
    std::array<char, fixture_signature.size()> signature{};
    input.read(signature.data(), static_cast<std::streamsize>(signature.size()));
    if (signature != fixture_signature) { throw std::invalid_argument{"Invalid segmented fixture"}; }
    size_ = read_integer<std::uint64_t>(input);
    if (size_ > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
      throw std::invalid_argument{"Fixture is not addressable by this stream implementation"};
    }
    const auto count = read_integer<std::uint32_t>(input);
    if (count > 1024) { throw std::invalid_argument{"Too many fixture segments"}; }
    std::uint64_t retained{};
    std::uint64_t previous_end{};
    for (std::uint32_t index = 0; index < count; ++index) {
      const auto offset = read_integer<std::uint64_t>(input);
      const auto length = read_integer<std::uint64_t>(input);
      if (offset < previous_end || offset > size_ || length > size_ - offset ||
          length > max_fixture_bytes - retained) {
        throw std::invalid_argument{"Invalid fixture segment extent"};
      }
      segment item{offset, std::vector<char>(static_cast<std::size_t>(length))};
      input.read(item.bytes.data(), static_cast<std::streamsize>(length));
      segments_.push_back(std::move(item));
      previous_end = offset + length;
      retained += length;
    }
  }
  segmented_buffer(const segmented_buffer&) = delete;
  segmented_buffer& operator=(const segmented_buffer&) = delete;

  // Materialize only the requested range, never the holes between independently authored records.
  std::streamsize xsgetn(char* destination, std::streamsize requested) override {
    if (requested < 0) { return 0; }
    const auto length = std::min(static_cast<std::uint64_t>(requested), size_ - position_);
    if (length > max_fixture_bytes) { throw std::runtime_error{"Unbounded fixture read"}; }
    ++read_calls;
    read_bytes += length;
    largest_read = std::max(largest_read, length);
    std::memset(destination, 0, static_cast<std::size_t>(length));
    for (const auto& item : segments_) {
      const auto first = std::max(position_, item.offset);
      const auto last = std::min(position_ + length, item.offset + item.bytes.size());
      if (first < last) {
        std::memcpy(destination + static_cast<std::size_t>(first - position_),
            item.bytes.data() + static_cast<std::size_t>(first - item.offset),
            static_cast<std::size_t>(last - first));
      }
    }
    position_ += length;
    return static_cast<std::streamsize>(length);
  }

  // Support the same absolute and end-relative addressing used by a real seekable ZIP source.
  pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                   std::ios_base::openmode mode) override {
    if ((mode & std::ios_base::in) == 0) { return pos_type{off_type{-1}}; }
    const auto base = direction == std::ios_base::beg ? std::uint64_t{} :
        direction == std::ios_base::end ? size_ : position_;
    if ((offset < 0 && static_cast<std::uint64_t>(-(offset + 1)) + 1 > base) ||
        (offset >= 0 && static_cast<std::uint64_t>(offset) > size_ - base)) {
      return pos_type{off_type{-1}};
    }
    position_ = offset < 0 ? base - (static_cast<std::uint64_t>(-(offset + 1)) + 1)
                           : base + static_cast<std::uint64_t>(offset);
    return pos_type{static_cast<off_type>(position_)};
  }

  // Treat stream positions as absolute logical offsets; the virtual source owns no OS file handle.
  pos_type seekpos(pos_type position, std::ios_base::openmode mode) override {
    return seekoff(static_cast<off_type>(position), std::ios_base::beg, mode);
  }
};

// Emit a stable diagnostic token without depending on the numeric values of public error enums.
std::string_view error_name(package::error_code code) {
  switch (code) {
    case package::error_code::invalid_archive: return "invalid_archive";
    case package::error_code::unsupported_feature: return "unsupported_feature";
    case package::error_code::resource_limit: return "resource_limit";
    case package::error_code::integrity: return "integrity";
    case package::error_code::io: return "io";
    case package::error_code::duplicate_name: return "duplicate_name";
    default: return "other";
  }
}

// Preserve byte-level payload evidence without locale-sensitive string escaping.
std::string hexadecimal(std::span<const std::uint8_t> bytes) {
  constexpr std::string_view digits{"0123456789abcdef"};
  std::string result;
  result.reserve(bytes.size() * 2);
  for (auto value : bytes) {
    result.push_back(digits[value >> 4U]);
    result.push_back(digits[value & 15U]);
  }
  return result;
}

// Exercise the production reader against a segmented fixture and report cumulative budget use.
void inspect(int argc, char** argv) {
  if (argc != 9) { throw std::invalid_argument{"read fixture archive io metadata buffer entries name"}; }
  package::limits configured{};
  configured.max_archive_bytes = std::stoull(argv[3]);
  configured.max_io_bytes = std::stoull(argv[4]);
  configured.max_metadata_bytes = std::stoull(argv[5]);
  configured.max_buffer_bytes = std::stoull(argv[6]);
  configured.max_entries = std::stoull(argv[7]);
  auto shared = std::make_shared<package::budget>(configured);
  segmented_buffer buffer{argv[2]};
  std::istream input{&buffer};
  std::string status{"ok"};
  std::size_t count{};
  std::uint64_t header{};
  std::uint64_t data{};
  std::string payload;
  bool order_matches{true};
  try {
    package::zip_reader reader{input, shared};
    count = reader.entries().size();
    if (count > 1) {
      for (std::size_t index = 0; index < count; ++index) {
        order_matches = order_matches && reader.entries()[index].name == "part" + std::to_string(index);
      }
    }
    const auto* entry = reader.find(argv[8]);
    if (entry == nullptr) { throw std::runtime_error{"Expected fixture entry was not indexed"}; }
    header = entry->header_offset_bytes;
    data = entry->data_offset_bytes;
    payload = hexadecimal(reader.read(entry->name));
  } catch (const package::error& failure) {
    status = error_name(failure.code);
  }
  std::cout << "{\"status\":\"" << status << "\",\"entries\":" << count
            << ",\"header\":" << header << ",\"data\":" << data
            << ",\"order_matches\":" << (order_matches ? "true" : "false")
            << ",\"payload\":\"" << payload << "\",\"read_bytes\":" << buffer.read_bytes
            << ",\"read_calls\":" << buffer.read_calls << ",\"largest_read\":" << buffer.largest_read
            << ",\"io_bytes\":" << configured.max_io_bytes - shared->remaining(package::resource::io_bytes)
            << ",\"metadata_bytes\":" << configured.max_metadata_bytes - shared->remaining(package::resource::metadata_bytes)
            << "}\n";
}

// Generate real count-boundary archives with the production writer for independent consumers.
void produce(int argc, char** argv) {
  if (argc != 4) { throw std::invalid_argument{"produce count output"}; }
  const auto count = std::stoull(argv[2]);
  if (count > 65536) { throw std::invalid_argument{"Unexpected fixture entry count"}; }
  std::ofstream output{argv[3], std::ios::binary};
  output.exceptions(std::ios::failbit | std::ios::badbit);
  package::limits configured{};
  configured.max_entries = count;
  configured.max_metadata_bytes = max_fixture_bytes;
  package::zip_writer writer{output, std::make_shared<package::budget>(configured)};
  for (std::uint64_t index = 0; index < count; ++index) {
    writer.add("part" + std::to_string(index), std::span<const std::uint8_t>{});
  }
  writer.finish();
  output.close();
}
} // namespace

// Run only bounded qualification operations; malformed fixture descriptions are harness errors.
int main(int argc, char** argv) {
  try {
    if (argc < 2) { throw std::invalid_argument{"Expected read or produce"}; }
    if (std::string_view{argv[1]} == "read") { inspect(argc, argv); }
    else if (std::string_view{argv[1]} == "produce") { produce(argc, argv); }
    else { throw std::invalid_argument{"Unknown operation"}; }
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
