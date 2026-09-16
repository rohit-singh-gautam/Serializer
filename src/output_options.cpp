#include <rohit/output_options.hpp>
#include <rohit/stream.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <random>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace rohit::serializer::writer {
namespace {
constexpr std::array profile_names{"serializer", "core",  "google",  "llvm", "gnu",
                                   "cert",       "misra", "autosar", "qt"};

// Trim configuration whitespace without interpreting backslashes in Windows paths.
std::string_view trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Accept only explicit boolean spellings so configuration typos cannot disable formatting.
bool read_bool(std::string_view value) {
  if (value == "true") {
    return true;
  }
  if (value == "false") {
    return false;
  }
  throw std::invalid_argument{"Expected true or false"};
}

// Own an atomically created temporary directory and clean it up on every exit path.
class temporary_directory {
  std::filesystem::path directory{};

public:
  // Reserve a unique directory, including when several generator processes run concurrently.
  temporary_directory() {
    constexpr unsigned maximum_attempts = 32;
    std::random_device random{};
    const auto root = std::filesystem::temp_directory_path();
    for (unsigned attempt = 0; attempt < maximum_attempts; ++attempt) {
      const auto candidate =
          root / ("serializer-format-" + std::to_string(random()) + "-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
      if (std::filesystem::create_directory(candidate)) {
        directory = candidate;
        return;
      }
    }
    throw std::runtime_error{"Cannot reserve a temporary formatter directory"};
  }
  // Remove only the uniquely owned formatter directory; cleanup must not throw.
  ~temporary_directory() {
    std::error_code ignored{};
    std::filesystem::remove_all(directory, ignored);
  }
  // Unique temporary storage cannot be shared or transferred.
  temporary_directory(const temporary_directory&) = delete;
  // Unique temporary storage cannot be assigned.
  temporary_directory& operator=(const temporary_directory&) = delete;
  // Return a file path within the owned directory.
  std::filesystem::path file(std::string_view name) const {
    return directory / name;
  }
};

// Write all text to a new temporary file and detect delayed write/close errors.
void write_text(const std::filesystem::path& file, std::string_view text) {
  std::ofstream output{};
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output.open(file, std::ios::binary | std::ios::trunc);
  output << text;
  output.close();
}

#ifdef _WIN32
// Quote one argument using the Windows C runtime rules, including trailing backslashes.
std::wstring quote_argument(const std::wstring& value) {
  std::wstring result{L"\""};
  std::size_t backslashes{};
  for (const auto character : value) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    result.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
    backslashes = 0;
    result += character;
  }
  result.append(backslashes * 2, L'\\');
  result += L'"';
  return result;
}
#endif

// Invoke the formatter with separate arguments; paths and configuration never become shell code.
void run_formatter(const std::filesystem::path& executable, const std::filesystem::path& style,
                   const std::filesystem::path& header) {
#ifdef _WIN32
  const std::array arguments{quote_argument(executable.wstring()),
                             std::wstring{L"-i"},
                             std::wstring{L"--Werror"},
                             std::wstring{L"--fail-on-incomplete-format"},
                             quote_argument(L"--style=file:" + style.wstring()),
                             quote_argument(header.wstring())};
  std::array<const wchar_t*, arguments.size() + 1> argv{};
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    argv[index] = arguments[index].c_str();
  }
  const auto status = _wspawnvp(_P_WAIT, executable.c_str(), argv.data());
  if (status == -1) {
    throw std::system_error{errno, std::generic_category(), "Cannot launch clang-format 19+"};
  }
  if (status != 0) {
    throw std::runtime_error{"clang-format failed; generated output was not written"};
  }
#else
  std::array arguments{executable.string(),
                       std::string{"-i"},
                       std::string{"--Werror"},
                       std::string{"--fail-on-incomplete-format"},
                       "--style=file:" + style.string(),
                       header.string()};
  std::array<char*, arguments.size() + 1> argv{};
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    argv[index] = arguments[index].data();
  }
  pid_t process{};
  const auto error =
      posix_spawnp(&process, executable.c_str(), nullptr, nullptr, argv.data(), environ);
  if (error != 0) {
    throw std::system_error{error, std::generic_category(), "Cannot launch clang-format 19+"};
  }
  int status{};
  while (waitpid(process, &status, 0) == -1) {
    if (errno != EINTR) {
      throw std::system_error{errno, std::generic_category(), "Cannot wait for clang-format"};
    }
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error{"clang-format failed; generated output was not written"};
  }
#endif
}
} // namespace

// Keep profile parsing and reporting tied to the same ordered table.
coding_standard parse_coding_standard(std::string_view name) {
  for (std::size_t index = 0; index < profile_names.size(); ++index) {
    if (name == profile_names[index]) {
      return static_cast<coding_standard>(index);
    }
  }
  throw std::invalid_argument{"Unknown C++ coding standard: " + std::string{name}};
}

// Reject invalid enum values supplied by library callers.
std::string_view coding_standard_name(coding_standard standard) {
  return profile_names.at(static_cast<std::size_t>(standard));
}

// Keep Java profile parsing independent from C++ presentation rules.
java_coding_standard parse_java_coding_standard(std::string_view name) {
  if (name == "serializer") { return java_coding_standard::serializer; }
  if (name == "google") { return java_coding_standard::google; }
  if (name == "oracle") { return java_coding_standard::oracle; }
  throw std::invalid_argument{"Unknown Java coding standard: " + std::string{name}};
}

// Reject invalid profile values supplied by library callers.
std::string_view java_coding_standard_name(java_coding_standard standard) {
  switch (standard) {
  case java_coding_standard::serializer: return "serializer";
  case java_coding_standard::google: return "google";
  case java_coding_standard::oracle: return "oracle";
  }
  throw std::invalid_argument{"Unknown Java coding standard"};
}

// Parse the documented small INI grammar and report the line responsible for invalid input.
output_options read_output_options(const std::filesystem::path& file) {
  std::ifstream input{file};
  if (!input) {
    throw std::runtime_error{"Cannot open output configuration: " + file.string()};
  }
  output_options result{};
  std::string section{};
  std::set<std::pair<std::string, std::string>> keys{};
  std::set<std::string> sections{};
  std::string line{};
  std::size_t line_number{};
  try {
    while (std::getline(input, line)) {
      ++line_number;
      auto text = trim(line);
      if (text.empty() || text.front() == '#' || text.front() == ';') {
        continue;
      }
      if (text.front() == '[' && text.back() == ']') {
        section = trim(text.substr(1, text.size() - 2));
        if ((section != "output" && section != "cpp" && section != "java") || !sections.insert(section).second) {
          throw std::invalid_argument{"Unknown or repeated section: " + section};
        }
        continue;
      }
      const auto separator = text.find('=');
      if (section.empty() || separator == std::string_view::npos) {
        throw std::invalid_argument{"Expected a section followed by key = value"};
      }
      const std::string key{trim(text.substr(0, separator))};
      auto value = trim(text.substr(separator + 1));
      if (!keys.emplace(section, key).second) {
        throw std::invalid_argument{"Repeated key: " + key};
      }
      if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
      }
      if (value.find('"') != std::string_view::npos) {
        throw std::invalid_argument{"Unmatched quotation mark"};
      }
      if (value.empty()) {
        throw std::invalid_argument{"Empty configuration value: " + key};
      }
      if (section == "output" && key == "language") {
        result.language = value;
        if (value != "cpp" && value != "java") {
          throw std::invalid_argument{"Unsupported output language: " + std::string{value}};
        }
      } else if (section == "java" && key == "coding_standard") {
        result.java.standard = parse_java_coding_standard(value);
      } else if (section == "java" && key == "package") {
        result.java.package_name = value;
      } else if (section == "java" && key == "naming") {
        if (value != "profile" && value != "preserve") {
          throw std::invalid_argument{"Expected naming = profile or preserve"};
        }
        result.java.rename_identifiers = value == "profile";
      } else if (section == "cpp" && key == "coding_standard") {
        result.cpp.standard = parse_coding_standard(value);
      } else if (section == "cpp" && key == "naming") {
        if (value != "profile" && value != "preserve") {
          throw std::invalid_argument{"Expected naming = profile or preserve"};
        }
        result.cpp.rename_identifiers = value == "profile";
      } else if (section == "cpp" && key == "format") {
        result.cpp.format = read_bool(value);
      } else if (section == "cpp" && (key == "format_file" || key == "clang_format")) {
        auto path = std::filesystem::path{std::u8string{value.begin(), value.end()}};
        if (path.is_relative() && (key == "format_file" || path.has_parent_path())) {
          path = std::filesystem::absolute(file).parent_path() / path;
        }
        if (key == "format_file") {
          result.cpp.format_file = path;
        } else {
          result.cpp.clang_format = path;
        }
      } else {
        throw std::invalid_argument{"Unknown output configuration key: " + section + "." + key};
      }
    }
    if (input.bad()) {
      throw std::runtime_error{"Cannot read output configuration"};
    }
    if (!result.cpp.format && !result.cpp.format_file.empty()) {
      throw std::invalid_argument{"format_file requires format = true"};
    }
  } catch (const std::exception& error) {
    throw std::invalid_argument{file.string() + ":" + std::to_string(line_number) + ": " +
                                error.what()};
  }
  return result;
}

// Broad design/security guides inherit Serializer's explicit layout; they prescribe no one layout.
std::string cpp_format_style(coding_standard standard) {
  switch (standard) {
  case coding_standard::google:
    return "BasedOnStyle: Google\nLineEnding: CRLF\n";
  case coding_standard::llvm:
    return "BasedOnStyle: LLVM\nLineEnding: CRLF\n";
  case coding_standard::gnu:
    return "BasedOnStyle: GNU\nLineEnding: CRLF\n";
  case coding_standard::qt:
    return "BasedOnStyle: LLVM\nIndentWidth: 4\nColumnLimit: 100\nUseTab: Never\n"
           "BreakBeforeBraces: Custom\nBraceWrapping:\n  AfterClass: true\n  AfterFunction: true\n"
           "PointerAlignment: Right\nReferenceAlignment: Right\nDerivePointerAlignment: false\n"
           "BreakBeforeBinaryOperators: All\nAllowShortFunctionsOnASingleLine: Empty\n"
           "AllowShortIfStatementsOnASingleLine: Never\nLineEnding: CRLF\n";
  case coding_standard::serializer:
  case coding_standard::core:
  case coding_standard::cert:
  case coding_standard::misra:
  case coding_standard::autosar:
    return "BasedOnStyle: LLVM\nIndentWidth: 2\nContinuationIndentWidth: 4\nColumnLimit: 100\n"
           "UseTab: Never\nBreakBeforeBraces: Attach\nBreakTemplateDeclarations: Yes\n"
           "PointerAlignment: Left\nReferenceAlignment: Left\nDerivePointerAlignment: false\n"
           "AllowShortFunctionsOnASingleLine: Empty\nAllowShortIfStatementsOnASingleLine: Never\n"
           "AllowShortLoopsOnASingleLine: false\nInsertBraces: true\nSortIncludes: CaseSensitive\n"
           "IncludeBlocks: Preserve\nReflowComments: false\nLineEnding: CRLF\n";
  }
  throw std::invalid_argument{"Unknown C++ coding standard"};
}

// Stage both input and style away from the destination, exposing output only after success.
std::string format_cpp(std::string_view source, const cpp_options& options) {
  if (source.empty()) {
    return {};
  }
  if (!options.format) {
    return std::string{source};
  }
  temporary_directory temporary{};
  auto style = options.format_file;
  if (style.empty()) {
    style = temporary.file(".clang-format");
    write_text(style, cpp_format_style(options.standard));
  } else {
    style = std::filesystem::absolute(style);
    if (!std::filesystem::is_regular_file(style)) {
      throw std::invalid_argument{"Cannot read C++ format_file: " + style.string()};
    }
  }
  const auto header = temporary.file("generated.hpp");
  write_text(header, source);
  run_formatter(options.clang_format, style, header);
  const auto formatted = make_stream_from_file(header);
  if (formatted.remaining_buffer() == 0 && !source.empty()) {
    throw std::runtime_error{"clang-format returned an empty header"};
  }
  return std::string{reinterpret_cast<const char*>(formatted.curr()), formatted.remaining_buffer()};
}
} // namespace rohit::serializer::writer
