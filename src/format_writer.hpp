#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer {

enum class protocol_name_style { lower_snake, upper_snake, pascal };

// Build selection only for explicit omissions supported by the native target.
inline std::string omitted_condition(const std::vector<std::string>& formats,
                                     std::string_view expression, std::string_view prefix,
                                     protocol_name_style style) {
  std::string result;
  for (const auto& format : formats) {
    if (format != "json" && format != "binary_none" && format != "binary_integer" &&
        format != "binary_string") {
      continue;
    }
    std::string symbol;
    bool upper = style == protocol_name_style::pascal;
    for (const auto byte : format) {
      if (style == protocol_name_style::pascal && byte == '_') {
        upper = true;
        continue;
      }
      symbol += style == protocol_name_style::upper_snake || upper
                    ? static_cast<char>(std::toupper(static_cast<unsigned char>(byte)))
                    : byte;
      upper = false;
    }
    result += (result.empty() ? "" : " || ") + std::string{expression} + " == " +
              std::string{prefix} + symbol;
  }
  return result;
}

} // namespace rohit::serializer::writer
