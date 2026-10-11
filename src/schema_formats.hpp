// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace rohit::serializer::schema_format {

// Keep omission complements tied to the complete existing wire-format vocabulary.
inline constexpr std::array names{
    std::string_view{"json"}, std::string_view{"binary_none"},
    std::string_view{"binary_integer"}, std::string_view{"binary_string"},
    std::string_view{"protobuf"}, std::string_view{"protojson"},
    std::string_view{"textproto"}};

// Normalize public aliases without changing previously accepted canonical spellings.
inline std::string canonical(std::string value) {
  if (value == "binary_positional") { return "binary_none"; }
  if (value == "protobuf_binary") { return "protobuf"; }
  return value;
}

// Check one canonical protocol name before schema metadata reaches a backend.
inline bool supported(std::string_view value) {
  return std::find(names.begin(), names.end(), value) != names.end();
}

} // namespace rohit::serializer::schema_format
