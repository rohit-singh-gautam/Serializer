#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer {

// Presentation profiles select layout and naming, not whole-standard compliance.
enum class coding_standard { serializer, core, google, llvm, gnu, cert, misra, autosar, qt };

struct cpp_options {
  coding_standard standard{coding_standard::serializer};
  bool rename_identifiers{true};
  bool format{true};
  std::filesystem::path clang_format{"clang-format"};
  std::filesystem::path format_file{};
  bool protobuf{false};
};

enum class java_coding_standard { serializer, google, oracle };

struct java_options {
  java_coding_standard standard{java_coding_standard::serializer};
  bool rename_identifiers{true};
  std::string package_name{};
};

struct output_options {
  // One language or a comma-separated list, validated by parse_output_languages.
  std::string language{"cpp"};
  cpp_options cpp{};
  java_options java{};
};

// Parse a nonempty comma-separated language list; reject unknown, empty, or repeated names.
std::vector<std::string> parse_output_languages(std::string_view names);

// Resolve a Java presentation profile, rejecting unknown spellings.
java_coding_standard parse_java_coding_standard(std::string_view name);
// Return the stable Java profile configuration spelling.
std::string_view java_coding_standard_name(java_coding_standard standard);

// Resolve an exact profile name; reject misspellings instead of silently choosing a default.
coding_standard parse_coding_standard(std::string_view name);
// Return the stable configuration spelling for a known profile.
std::string_view coding_standard_name(coding_standard standard);
// Read strict language sections; resolve file paths relative to the config file.
output_options read_output_options(const std::filesystem::path& file);
// Return a standalone clang-format configuration for the selected presentation profile.
std::string cpp_format_style(coding_standard standard);
// Format a temporary header without a shell; throw on launch, configuration, or format failures.
std::string format_cpp(std::string_view source, const cpp_options& options);

} // namespace rohit::serializer::writer
