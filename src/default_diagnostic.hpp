// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <string>
#include <string_view>

namespace rohit::serializer::writer {

// Add an authored field location only on failure; synthetic ASTs retain the original diagnostic.
inline std::string default_diagnostic(const member& field, std::string_view message) {
  if (field.source_path.empty() || field.source_line == 0 || field.source_column == 0) {
    return std::string{message};
  }
  return field.source_path + ":" + std::to_string(field.source_line) + ":" +
      std::to_string(field.source_column) + ": member " + field.name + ": " + std::string{message};
}

} // namespace rohit::serializer::writer
