#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rohit::serializer::writer {

// Presentation profiles select layout and naming, not whole-standard compliance.
enum class coding_standard { serializer, core, google, llvm, gnu, cert, misra, autosar, qt };

struct cpp_options {
  coding_standard standard{coding_standard::serializer};
  bool rename_identifiers{true};
  bool format{true};
  std::filesystem::path clang_format{"clang-format"};
  std::filesystem::path format_file{};
};

struct output_options {
  std::string language{"cpp"};
  cpp_options cpp{};
};

// Resolve an exact profile name; reject misspellings instead of silently choosing a default.
coding_standard parse_coding_standard(std::string_view name);
// Return the stable configuration spelling for a known profile.
std::string_view coding_standard_name(coding_standard standard);
// Read strict INI sections [output] and [cpp]; resolve file paths relative to the config file.
output_options read_output_options(const std::filesystem::path& file);
// Return a standalone clang-format configuration for the selected presentation profile.
std::string cpp_format_style(coding_standard standard);
// Format a temporary header without a shell; throw on launch, configuration, or format failures.
std::string format_cpp(std::string_view source, const cpp_options& options);

} // namespace rohit::serializer::writer
