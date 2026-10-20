// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/stream.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace rohit::serializer::compiler_output {
// Compare regular-file bytes in bounded scratch space without changing the output's modification time.
// Unreadable/nonregular paths fall through to the existing writer's permissions and error behavior.
// Callers must complete source/alias/output preflight first and serialize mutations to these paths.
inline bool identical_file(const std::filesystem::path& path, std::string_view contents) {
  std::error_code failure;
  if (!std::filesystem::is_regular_file(path, failure) || failure) {
    return false;
  }
  const auto bytes = std::filesystem::file_size(path, failure);
  if (failure || bytes != contents.size()) {
    return false;
  }
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }
  constexpr std::size_t comparison_chunk_bytes = 64U * 1024U;
  std::array<char, comparison_chunk_bytes> scratch{};
  std::size_t position{};
  while (position < contents.size()) {
    const auto count = std::min(scratch.size(), contents.size() - position);
    input.read(scratch.data(), static_cast<std::streamsize>(count));
    if (input.gcount() != static_cast<std::streamsize>(count) ||
        !std::equal(scratch.data(), scratch.data() + count, contents.data() + position)) {
      return false;
    }
    position += count;
  }
  return input.peek() == std::char_traits<char>::eof() && input.eof() && !input.bad();
}

// Preserve equal compiler artifacts/depfiles; changed files retain the ordinary stream write contract.
// This is not atomic batch publication: filesystem failures retain the existing partial-write behavior.
inline void write_if_different(const std::filesystem::path& path, std::string_view contents) {
  if (identical_file(path, contents)) {
    return;
  }
  rohit::full_stream_auto_alloc output{};
  output.write(contents);
  output.write_to_file_till_offset(path);
}
} // namespace rohit::serializer::compiler_output
