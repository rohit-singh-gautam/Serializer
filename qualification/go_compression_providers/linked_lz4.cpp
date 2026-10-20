// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <lz4frame.h>
#include <lz4.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t maximum_input_bytes = 8U * 1024U * 1024U;
constexpr std::size_t batch_bytes = 64U * 1024U;

// Keep the selected native provider's errors separate from ordinary file failures.
std::size_t checked(std::size_t status) {
  if (LZ4F_isError(status)) {
    throw std::runtime_error{"Native LZ4 frame generation failed"};
  }
  return status;
}

// Emit one complete linked-block standard frame using the actual native LZ4 streaming API.
std::vector<char> encode(std::span<const char> input, bool checksum) {
  LZ4F_cctx* raw{};
  checked(LZ4F_createCompressionContext(&raw, LZ4F_VERSION));
  const std::unique_ptr<LZ4F_cctx, decltype(&LZ4F_freeCompressionContext)> context{
      raw, &LZ4F_freeCompressionContext};
  LZ4F_preferences_t preferences{};
  preferences.frameInfo.blockSizeID = LZ4F_max64KB;
  preferences.frameInfo.blockMode = LZ4F_blockLinked;
  preferences.frameInfo.contentChecksumFlag =
      checksum ? LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;
  preferences.frameInfo.blockChecksumFlag =
      checksum ? LZ4F_blockChecksumEnabled : LZ4F_noBlockChecksum;
  preferences.frameInfo.contentSize = input.size();
  std::vector<char> scratch(std::max<std::size_t>(
      LZ4F_HEADER_SIZE_MAX, checked(LZ4F_compressBound(batch_bytes, &preferences))));
  std::vector<char> output;
  // Append only the provider-confirmed initialized extent of the reusable scratch buffer.
  const auto append = [&](std::size_t count) {
    output.insert(output.end(), scratch.begin(), scratch.begin() + count);
  };
  append(checked(LZ4F_compressBegin(context.get(), scratch.data(), scratch.size(), &preferences)));
  while (!input.empty()) {
    const auto count = std::min(input.size(), batch_bytes);
    append(checked(LZ4F_compressUpdate(context.get(), scratch.data(), scratch.size(),
                                     input.data(), count, nullptr)));
    input = input.subspan(count);
  }
  append(checked(LZ4F_compressEnd(context.get(), scratch.data(), scratch.size(), nullptr)));
  return output;
}

// Prove a produced linked frame uses an earlier block by attempting each block without its dictionary.
bool requires_history(std::span<const char> frame) {
  constexpr std::uint32_t uncompressed_flag = 0x80000000U;
  constexpr std::uint32_t block_size_mask = 0x7fffffffU;
  constexpr unsigned content_size_flag = 8U;
  constexpr unsigned block_checksum_flag = 16U;
  if (frame.size() < 7U) {
    throw std::runtime_error{"Truncated native linked frame"};
  }
  const auto flags = static_cast<unsigned char>(frame[4]);
  std::size_t offset = 7U + ((flags & content_size_flag) ? sizeof(std::uint64_t) : 0U);
  std::array<char, batch_bytes> decoded{};
  bool required{};
  while (true) {
    if (offset > frame.size() || frame.size() - offset < sizeof(std::uint32_t)) {
      throw std::runtime_error{"Truncated native linked block"};
    }
    std::uint32_t block{};
    for (std::size_t index{}; index < sizeof(block); ++index) {
      block |= static_cast<std::uint32_t>(static_cast<unsigned char>(frame[offset + index]))
               << (index * 8U);
    }
    offset += sizeof(block);
    if (block == 0U) {
      return required;
    }
    const auto count = block & block_size_mask;
    if (count > batch_bytes || count > frame.size() - offset) {
      throw std::runtime_error{"Invalid native linked block extent"};
    }
    if ((block & uncompressed_flag) == 0U &&
        LZ4_decompress_safe(frame.data() + offset, decoded.data(), static_cast<int>(count),
                            static_cast<int>(decoded.size())) < 0) {
      required = true;
    }
    offset += count;
    if (flags & block_checksum_flag) {
      if (frame.size() - offset < sizeof(std::uint32_t)) {
        throw std::runtime_error{"Truncated native block checksum"};
      }
      offset += sizeof(std::uint32_t);
    }
  }
}
}

// Produce bounded independent C++ linked-frame fixtures with checksum policy selected by the runner.
int main(int argc, char** argv) {
  try {
    if (argc != 4 || (std::string_view{argv[3]} != "on" && std::string_view{argv[3]} != "off")) {
      throw std::invalid_argument{"Expected INPUT OUTPUT CHECKSUM"};
    }
    std::ifstream input{argv[1], std::ios::binary};
    if (!input) {
      throw std::runtime_error{"Cannot open linked-frame input"};
    }
    std::vector<char> bytes;
    bytes.reserve(batch_bytes);
    char value{};
    while (input.get(value)) {
      if (bytes.size() == maximum_input_bytes) {
        throw std::runtime_error{"Linked-frame input exceeds its bound"};
      }
      bytes.push_back(value);
    }
    if (!input.eof()) {
      throw std::runtime_error{"Cannot read linked-frame input"};
    }
    const auto frame = encode(bytes, std::string_view{argv[3]} == "on");
    std::ofstream output{argv[2], std::ios::binary};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.write(frame.data(), static_cast<std::streamsize>(frame.size()));
    output.close();
    std::cout << "{\"lz4_version\":\"" << LZ4_versionString()
              << "\",\"linked_history_required\":"
              << (requires_history(frame) ? "true" : "false") << "}\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
