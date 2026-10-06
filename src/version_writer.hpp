#pragma once

#include "schema_version.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace rohit::serializer::writer {

// Target spellings are isolated from schema identity and C++ generator naming profiles.
enum class version_language {
  java,
  javascript,
  typescript,
  go,
  csharp,
  python,
  rust,
  swift,
  kotlin,
  c
};

// Emit a checked revision literal in its target language without losing uint64 precision.
inline std::string version_literal(version_language language, const member& version,
                                   std::string_view text) {
  const auto& type = version.type_name_list.front().name;
  const auto value = schema_version::parse(type, text);
  auto result = schema_version::trim(text);
  if (type.starts_with("version")) {
    return result;
  }
  if (language == version_language::java && type.starts_with("uint")) {
    return "Long.parseUnsignedLong(\"" + result + "\")";
  }
  if (type == "float") {
    if (language == version_language::java) {
      return result + "F";
    }
    if (language == version_language::c) {
      return result + (result.find_first_of(".eE") == std::string::npos ? ".0f" : "f");
    }
    if (language == version_language::python) {
      return "_FLOAT.unpack(_FLOAT.pack(" + result + "))[0]";
    }
    if (language == version_language::javascript || language == version_language::typescript) {
      return "Math.fround(" + result + ")";
    }
  }
  if ((language == version_language::javascript || language == version_language::typescript) &&
      type == "uint64") {
    return result + "n";
  }
  if (language == version_language::csharp) {
    if (type == "uint64") {
      return result + "UL";
    }
    if (type == "float") {
      return result + "F";
    }
  }
  if (language == version_language::kotlin) {
    if (type == "uint8") {
      return "(" + result + "u).toUByte()";
    }
    if (type == "uint16") {
      return "(" + result + "u).toUShort()";
    }
    if (type == "double" && result.find_first_of(".eE") == std::string::npos) {
      return result + ".0";
    }
    if (type == "uint64") {
      return result + "uL";
    }
    if (type.starts_with("uint")) {
      return result + "u";
    }
    if (type == "float") {
      return result + "f";
    }
  }
  if (language == version_language::rust && value.is_floating &&
      result.find_first_of(".eE") == std::string::npos) {
    result += ".0";
  }
  if ((language == version_language::c || language == version_language::java) && type == "double" &&
      result.find_first_of(".eE") == std::string::npos) {
    return result + ".0";
  }
  if (language == version_language::c && type == "uint64") {
    return result + "ULL";
  }
  return result;
}

// Compare dotted components numerically and Java unsigned fields without signed reinterpretation.
inline std::string version_compare(version_language language, const member& version,
                                   std::string_view value, std::string_view boundary) {
  const auto& type = version.type_name_list.front().name;
  const auto constant = version_literal(language, version, boundary);
  const auto source = std::string{value};
  if (type.starts_with("version")) {
    const auto count = std::string{type.back()};
    switch (language) {
    case version_language::python:
      return "_version_compare(" + source + ", " + constant + ", " + count + ")";
    case version_language::rust:
      return "srl_version_compare(&" + source + ", " + constant + ", " + count + ")?";
    case version_language::swift:
      return "srlVersionCompare(" + source + ", " + constant + ", " + count + ")";
    case version_language::c:
      return "srl_version_compare(&" + source + ", " + constant + ", " + count + ")";
    case version_language::csharp:
      return "SrlVersionCompare(" + source + ", " + constant + ", " + count + ")";
    default:
      return "srlVersionCompare(" + source + ", " + constant + ", " + count + ")";
    }
  }
  if (language == version_language::java && type.starts_with("uint")) {
    const auto converted = type == "uint8"    ? "Byte.toUnsignedLong(" + source + ")"
                           : type == "uint16" ? "Short.toUnsignedLong(" + source + ")"
                           : type == "uint32" ? "Integer.toUnsignedLong(" + source + ")"
                                              : source;
    return "Long.compareUnsigned(" + converted + ", " + constant + ")";
  }
  // Callers use less/equal directly for scalar targets; no tri-state temporary is needed.
  return {};
}

// Produce a target expression for the strict lower-bound comparison.
inline std::string version_less(version_language language, const member& version,
                                std::string_view value, std::string_view boundary) {
  const auto comparison = version_compare(language, version, value, boundary);
  if (!comparison.empty()) {
    return "(" + comparison + " < 0)";
  }
  return "(" + std::string{value} + " < " + version_literal(language, version, boundary) + ")";
}

// Produce a target expression for an inclusive lower bound.
inline std::string version_at_least(version_language language, const member& version,
                                    std::string_view value, std::string_view boundary) {
  const auto comparison = version_compare(language, version, value, boundary);
  if (!comparison.empty()) {
    return "(" + comparison + " >= 0)";
  }
  return "(" + std::string{value} + " >= " + version_literal(language, version, boundary) + ")";
}

// Produce a target expression for matching the current revision.
inline std::string version_equal(version_language language, const member& version,
                                 std::string_view value, std::string_view boundary) {
  const auto comparison = version_compare(language, version, value, boundary);
  if (!comparison.empty()) {
    return "(" + comparison + " == 0)";
  }
  return "(" + std::string{value} + " == " + version_literal(language, version, boundary) + ")";
}

// Emit one lifecycle predicate; unaffected fields remain unconditional in generated source.
inline std::string version_active(version_language language, const member& field,
                                  const member& version, std::string_view value) {
  std::string result{};
  if (!field.created_version.empty()) {
    result = version_at_least(language, version, value, field.created_version);
  }
  if (!field.obsolete_version.empty()) {
    if (!result.empty()) {
      result += language == version_language::python ? " and " : " && ";
    }
    result += version_less(language, version, value, field.obsolete_version);
  }
  return result;
}

// Find a field's stack-presence slot; parent identities occupy the preceding slots.
inline std::size_t version_slot(const class_node& object, const member& field) {
  const auto found = std::find_if(object.member_list.begin(), object.member_list.end(),
                                  [&](const auto& candidate) { return candidate.id == field.id; });
  return object.parents.size() + static_cast<std::size_t>(found - object.member_list.begin());
}
} // namespace rohit::serializer::writer
