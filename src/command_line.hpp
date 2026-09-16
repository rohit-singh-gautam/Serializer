// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Chaturanga's commandline option descriptors and short/long lookup.
// See docs/command_line.md for upstream provenance and the adaptation scope.
#pragma once

#include <algorithm>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::cli {
struct commandline_option {
  char short_name{};
  std::string_view name{};
  std::string_view value_help{};
  std::string_view help{};
  bool repeatable{};
  bool allow_empty{};
};

using arguments = std::map<std::string, std::vector<std::string>>;

// Parse descriptors without modifying caller state; reject unknown, empty, or repeated options.
// Values beginning with '-' must use --name=value. Short option clusters are unsupported.
inline arguments parse(int argc, const char* const* argv,
                       std::span<const commandline_option> options) {
  arguments result{};
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    const bool long_name = argument.starts_with("--");
    const auto equal = argument.find('=');
    const auto name = argument.substr(0, equal);
    const auto option = std::find_if(options.begin(), options.end(),
                                     [name, long_name](const commandline_option& candidate) {
                                       return long_name ? name.substr(2) == candidate.name
                                                        : name.size() == 2 && name.front() == '-' &&
                                                              name.back() == candidate.short_name;
                                     });
    if (option == options.end()) {
      throw std::invalid_argument{"Unknown argument: " + std::string{argument}};
    }
    auto& values = result[std::string{option->name}];
    if (!option->repeatable && !values.empty()) {
      throw std::invalid_argument{"Repeated argument: --" + std::string{option->name}};
    }
    std::string_view value{};
    if (option->value_help.empty()) {
      if (equal != std::string_view::npos) {
        throw std::invalid_argument{"Flag does not accept a value: " + std::string{name}};
      }
      value = "true";
    } else if (long_name && equal != std::string_view::npos) {
      value = argument.substr(equal + 1);
    } else {
      if (equal != std::string_view::npos || index + 1 == argc ||
          std::string_view{argv[index + 1]}.starts_with('-')) {
        throw std::invalid_argument{"Missing value for argument: " + std::string{name}};
      }
      value = argv[++index];
    }
    if (value.empty() && !option->allow_empty) {
      throw std::invalid_argument{"Empty value for argument: " + std::string{name}};
    }
    values.emplace_back(value);
  }
  return result;
}

// Build help from the same descriptors used for parsing so option names cannot drift.
inline std::string usage(std::span<const commandline_option> options) {
  std::string result{"Usage: serializer --input <schema.serializer> [options]\nOptions:\n"};
  for (const auto& option : options) {
    result += "  ";
    if (option.short_name != '\0') {
      result += '-';
      result += option.short_name;
      result += ", ";
    }
    result += "--";
    result += option.name;
    if (!option.value_help.empty()) {
      result += " <";
      result += option.value_help;
      result += '>';
    }
    result += "\n      ";
    result += option.help;
    result += '\n';
  }
  return result;
}
} // namespace rohit::serializer::cli
