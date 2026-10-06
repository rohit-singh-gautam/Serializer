#pragma once

#include <rohit/serializer_creator.hpp>
#include <rohit/versioning.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::schema_version {

// Strip insignificant schema whitespace around one literal.
inline std::string trim(std::string_view value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  return std::string{value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1)};
}

// Identify the closed set of discriminator types, independent of generated language names.
inline bool supported_type(std::string_view name) {
  return name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64" ||
         name == "float" || name == "double" || name == "version2" || name == "version3" ||
         name == "version4";
}

// Preserve full integer precision; floating revisions use their declared wire precision.
struct literal {
  std::vector<std::uint64_t> components{};
  double floating{};
  bool is_floating{};
  // Compare only literals already validated against the same discriminator type.
  bool operator<(const literal& other) const {
    return is_floating ? floating < other.floating : components < other.components;
  }
  // Compare semantic values rather than insignificant literal spellings.
  bool operator==(const literal& other) const {
    return is_floating ? floating == other.floating : components == other.components;
  }
};

// Parse a bounded, nonnegative version literal; reject unsupported types and overflow.
inline literal parse(std::string_view type, std::string_view input) {
  const auto text = trim(input);
  if (!supported_type(type) || text.empty() || text.front() == '-') {
    throw std::invalid_argument{"Invalid version type or literal"};
  }
  // Canonical decimal spelling prevents target languages from interpreting leading zeros as octal.
  static const std::regex numeric_literal{R"((0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?)"};
  if (!type.starts_with("version") && !std::regex_match(text, numeric_literal)) {
    throw std::invalid_argument{"Version requires a canonical decimal literal"};
  }
  literal result{};
  if (type == "float" || type == "double") {
    result.is_floating = true;
    float single{};
    const auto parsed =
        type == "float" ? std::from_chars(text.data(), text.data() + text.size(), single)
                        : std::from_chars(text.data(), text.data() + text.size(), result.floating);
    if (type == "float") {
      result.floating = single;
    }
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        !std::isfinite(result.floating)) {
      throw std::invalid_argument{"Invalid floating version literal"};
    }
  } else if (type.starts_with("version")) {
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
      throw std::invalid_argument{"Dotted versions require a quoted canonical literal"};
    }
    const auto add = [&](const auto& value) {
      for (const auto component : value.components) {
        result.components.push_back(component);
      }
    };
    const auto body = std::string_view{text}.substr(1, text.size() - 2);
    if (type == "version2") {
      add(version2::parse(body));
    } else if (type == "version3") {
      add(version3::parse(body));
    } else {
      add(version4::parse(body));
    }
  } else {
    std::uint64_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    const auto maximum = type == "uint8"    ? std::numeric_limits<std::uint8_t>::max()
                         : type == "uint16" ? std::numeric_limits<std::uint16_t>::max()
                         : type == "uint32" ? std::numeric_limits<std::uint32_t>::max()
                                            : std::numeric_limits<std::uint64_t>::max();
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > maximum) {
      throw std::invalid_argument{"Version literal exceeds its declared type"};
    }
    result.components.push_back(value);
  }
  return result;
}

// Emit a checked, typed C++ constant without narrowing large unsigned or floating literals.
inline std::string cpp_literal(const member& version, std::string_view text) {
  const auto& type = version.type_name_list.front().name;
  const auto value = parse(type, text);
  const auto cpp = get_cpp_type(type);
  if (type.starts_with("version")) {
    std::string parts{};
    for (const auto component : value.components) {
      if (!parts.empty()) {
        parts += ", ";
      }
      parts += std::to_string(component);
    }
    return "::" + cpp + "{" + parts + "}";
  }
  auto number = trim(text);
  if (value.is_floating && number.find_first_of(".eE") == std::string::npos) {
    number += ".0";
  }
  return "static_cast<" + cpp + ">(" + number + (value.is_floating ? "" : "ULL") + ")";
}

// The compatibility floor defaults to the current revision, making historical support opt-in.
inline std::string minimum(const member& version) {
  return version.compatibility_version.empty() ? version.default_value
                                               : version.compatibility_version;
}

// Decide whether a retained field belongs to one historical revision.
inline bool active(const member& field, const member& version, std::string_view revision) {
  const auto& type = version.type_name_list.front().name;
  const auto value = parse(type, revision);
  return (field.created_version.empty() || !(value < parse(type, field.created_version))) &&
         (field.obsolete_version.empty() || value < parse(type, field.obsolete_version));
}

// Collect only layout transitions in the supported range, avoiding one decoder per number.
inline std::vector<std::string> transitions(const class_node& object) {
  const auto& version = *object.version_member();
  const auto& type = version.type_name_list.front().name;
  const auto lower = parse(type, minimum(version));
  const auto upper = parse(type, version.default_value);
  std::vector<std::string> result{minimum(version)};
  for (const auto& field : object.member_list) {
    for (const auto& boundary : {field.created_version, field.obsolete_version}) {
      if (boundary.empty()) {
        continue;
      }
      const auto value = parse(type, boundary);
      if (!(value < lower) && !(upper < value)) {
        result.push_back(boundary);
      }
    }
  }
  std::sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
    return parse(type, left) < parse(type, right);
  });
  result.erase(std::unique(result.begin(), result.end(),
                           [&](const auto& left, const auto& right) {
                             return parse(type, left) == parse(type, right);
                           }),
               result.end());
  return result;
}
} // namespace rohit::serializer::schema_version
