#include <rohit/output_options.hpp>
#include <rohit/stream.hpp>

#include <algorithm>
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

// Validate source identifiers before accepting class selectors or explicit member lists.
bool source_identifier(std::string_view value) {
  const auto first = [](char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') || character == '_';
  };
  if (value.empty() || !first(value.front())) { return false; }
  return std::all_of(value.begin() + 1, value.end(), [&](char character) {
    return first(character) || (character >= '0' && character <= '9');
  });
}

// Recognize only documented shared-contract keys; their AST-dependent semantics bind later.
bool read_xml_contract_option(xml_class_options& mapping, std::string_view key,
    std::string_view value) {
  const auto identifier = [](std::string_view name) {
    if (!source_identifier(name)) {
      throw std::invalid_argument{"Expected XML contract source identifier"};
    }
  };
  if (key == "mixed" || key == "mixed.variant" || key == "mixed.text" ||
      key == "ordered" || key == "ordered.variant") {
    identifier(value);
  } else if (key.starts_with("presence.")) {
    identifier(key.substr(std::string_view{"presence."}.size()));
    if (value != "required" && value != "absent" && value != "nil" && value != "absent_or_nil") {
      throw std::invalid_argument{"XML presence must be required, absent, nil or absent_or_nil"};
    }
  } else if (key.starts_with("mixed.child.") || key.starts_with("ordered.child.")) {
    identifier(key.substr(key.starts_with("mixed.") ? std::string_view{"mixed.child."}.size() :
        std::string_view{"ordered.child."}.size()));
  } else if (key.starts_with("map.") || key.starts_with("variant.")) {
    const bool is_map = key.starts_with("map.");
    auto tail = key.substr(is_map ? std::string_view{"map."}.size() :
        std::string_view{"variant."}.size());
    const auto separator = tail.find('.');
    identifier(tail.substr(0U, separator));
    if (separator == std::string_view::npos) {
      if (!is_map && value != "element" && value != "xsi_type") {
        throw std::invalid_argument{"XML variant selection must be element or xsi_type"};
      }
    } else {
      tail.remove_prefix(separator + 1U);
      if (is_map) {
        if (tail != "key" && tail != "value" && tail != "key_placement") {
          throw std::invalid_argument{"Unknown XML map contract key"};
        }
        if (tail == "key_placement" && value != "element" && value != "attribute") {
          throw std::invalid_argument{"XML map key_placement must be element or attribute"};
        }
      } else {
        const auto part = tail.find('.');
        if (part == std::string_view::npos ||
            (tail.substr(0U, part) != "tag" && tail.substr(0U, part) != "type")) {
          throw std::invalid_argument{"Unknown XML variant contract key"};
        }
        identifier(tail.substr(part + 1U));
      }
    }
  } else if (key == "extensions.required") {
    // Values are vocabulary URI lists, resolved independently of document prefix spelling.
  } else if (key.starts_with("extension.")) {
    auto tail = key.substr(std::string_view{"extension."}.size());
    const auto separator = tail.find('.');
    if (separator == std::string_view::npos) {
      throw std::invalid_argument{"XML extension slot requires a property"};
    }
    identifier(tail.substr(0U, separator));
    tail.remove_prefix(separator + 1U);
    if (tail != "kind" && tail != "before" && tail != "namespaces" && tail != "policy" &&
        tail != "minimum" && tail != "maximum") {
      throw std::invalid_argument{"Unknown XML extension slot property"};
    }
    if (tail == "kind" && value != "attribute" && value != "element") {
      throw std::invalid_argument{"XML extension kind must be attribute or element"};
    }
    if (tail == "policy" && value != "reject" && value != "skip" && value != "preserve") {
      throw std::invalid_argument{"XML extension policy must be reject, skip or preserve"};
    }
    if (tail == "before" && value != "$end") { identifier(value); }
  } else {
    return false;
  }
  mapping.contracts.emplace(key, value);
  return true;
}

// Require a fully qualified source spelling, with no empty namespace components.
bool source_class_name(std::string_view value) {
  while (true) {
    const auto separator = value.find("::");
    if (!source_identifier(value.substr(0, separator))) { return false; }
    if (separator == std::string_view::npos) { return true; }
    value.remove_prefix(separator + std::string_view{"::"}.size());
  }
}

// Select durable declaration order or an exact, duplicate-free source member order.
std::vector<std::string> comparison_fields(std::string_view value) {
  if (value == "durable") { return {}; }
  std::vector<std::string> fields;
  std::set<std::string> seen;
  while (true) {
    const auto separator = value.find(',');
    const std::string field{trim(value.substr(0, separator))};
    if (!source_identifier(field) || !seen.insert(field).second) {
      throw std::invalid_argument{"Expected distinct source member names or fields = durable"};
    }
    fields.push_back(field);
    if (separator == std::string_view::npos) { return fields; }
    value.remove_prefix(separator + 1);
  }
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

// Resolve the common native protocol profile without silently widening its scope.
output_protocols parse_output_protocols(std::string_view name) {
  if (name == "all") {
    return cpp_protocols::all;
  }
  if (name == "binary_none") {
    return cpp_protocols::binary_none;
  }
  throw std::invalid_argument{"protocols must be all or binary_none"};
}

// Preserve the established parser and option-specific diagnostic for C++ overrides.
cpp_protocols parse_cpp_protocols(std::string_view name) {
  try {
    return parse_output_protocols(name);
  } catch (const std::invalid_argument&) {
    throw std::invalid_argument{"cpp.protocols must be all or binary_none"};
  }
}

// Configure every native codec backend without changing independent XML options.
void select_output_protocols(output_options& options, output_protocols protocols) {
  options.protocols = protocols;
  options.cpp.protocols = protocols;
  options.java.protocols = protocols;
  options.js.protocols = protocols;
  options.go.protocols = protocols;
  options.csharp.protocols = protocols;
  options.kotlin.protocols = protocols;
}

// Reject invalid protocol enum values supplied through the library API.
std::string_view cpp_protocols_name(cpp_protocols protocols) {
  switch (protocols) {
  case cpp_protocols::all:
    return "all";
  case cpp_protocols::binary_none:
    return "binary_none";
  }
  throw std::invalid_argument{"Unknown C++ protocol selection"};
}

// Keep Java profile parsing independent from C++ presentation rules.
java_coding_standard parse_java_coding_standard(std::string_view name) {
  if (name == "serializer") {
    return java_coding_standard::serializer;
  }
  if (name == "google") {
    return java_coding_standard::google;
  }
  if (name == "oracle") {
    return java_coding_standard::oracle;
  }
  throw std::invalid_argument{"Unknown Java coding standard: " + std::string{name}};
}

// Reject invalid profile values supplied by library callers.
std::string_view java_coding_standard_name(java_coding_standard standard) {
  switch (standard) {
  case java_coding_standard::serializer:
    return "serializer";
  case java_coding_standard::google:
    return "google";
  case java_coding_standard::oracle:
    return "oracle";
  }
  throw std::invalid_argument{"Unknown Java coding standard"};
}

// Preserve selection order while rejecting empty entries, duplicates, and unsupported backends.
std::vector<std::string> parse_output_languages(std::string_view names) {
  std::vector<std::string> result{};
  std::set<std::string> seen{};
  while (true) {
    const auto separator = names.find(',');
    const std::string name{trim(names.substr(0, separator))};
    if (name != "cpp" && name != "java" && name != "js" && name != "typescript" && name != "go" &&
        name != "csharp" && name != "python" && name != "rust" && name != "swift" && name != "kotlin" && name != "c") {
      throw std::invalid_argument{"Unsupported output language: " + name};
    }
    if (!seen.insert(name).second) {
      throw std::invalid_argument{"Repeated output language: " + name};
    }
    result.push_back(name);
    if (separator == std::string_view::npos) {
      return result;
    }
    names.remove_prefix(separator + 1);
  }
}

// Match only supported payload encodings so typos never change persistent bytes silently.
persistent_id_encoding parse_managed_id_encoding(std::string_view name) {
  if (name == "fixed") { return persistent_id_encoding::fixed; }
  if (name == "compact_varint") { return persistent_id_encoding::compact_varint; }
  throw std::invalid_argument{"managed.id_encoding must be fixed or compact_varint"};
}

// Resolve the include layout without silently accepting misspelled modes.
include_mode parse_include_mode(std::string_view name) {
  if (name == "separate") { return include_mode::separate; }
  if (name == "inline") { return include_mode::inline_output; }
  throw std::invalid_argument{"include_mode must be separate or inline"};
}

// Programmatic callers must receive the same typo and mode checks as the strict INI parser.
void validate_xml_contract_options(const xml_class_options& options) {
  xml_class_options checked{};
  for (const auto& [key, value] : options.contracts) {
    if (value.empty() || !read_xml_contract_option(checked, key, value)) {
      throw std::invalid_argument{"Unknown or empty XML shared contract option: " + key};
    }
  }
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
        constexpr std::string_view class_separator{".class."};
        const auto class_offset = section.find(class_separator);
        bool class_section = class_offset != std::string::npos &&
            source_class_name(std::string_view{section}.substr(class_offset + class_separator.size()));
        if (class_section && !section.starts_with("xml.class.")) {
          const auto languages = parse_output_languages(std::string_view{section}.substr(0, class_offset));
          class_section = languages.size() == 1;
        }
        if ((section != "output" && section != "cpp" && section != "java" && section != "js" &&
             section != "go" && section != "csharp" && section != "kotlin" && section != "managed" && section != "xml" &&
             !class_section && !(section.starts_with("package.") && source_class_name(std::string_view{section}.substr(8U)))) ||
            !sections.insert(section).second) {
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
      if (section.starts_with("package.")) {
        const auto separator_at = key.find('.');
        if (separator_at == std::string::npos || !source_identifier(std::string_view{key}.substr(0, separator_at))) {
          throw std::invalid_argument{"Package profile keys require role.setting"};
        }
        const auto setting = std::string_view{key}.substr(separator_at + 1U);
        if (setting != "codec" && setting != "compression" && setting != "policy" &&
            setting != "vocabularies" && setting != "requires_type" && setting != "target_mode") {
          throw std::invalid_argument{"Unknown package profile setting: " + std::string{setting}};
        }
        result.cpp.xml.package_profiles[section.substr(8U)].emplace(key, value);
      } else if (section.starts_with("xml.class.")) {
        auto& mapping = result.cpp.xml.classes[section.substr(std::string_view{"xml.class."}.size())];
        if (key == "root") { mapping.root = value; }
        else if (key == "namespace") { mapping.namespace_uri = value; }
        else if (key == "attributes") { mapping.attributes = comparison_fields(value); }
        else if (key.starts_with("name.")) { mapping.names.emplace(key.substr(5U), value); }
        else if (key == "text") {
          if (!source_identifier(value)) { throw std::invalid_argument{"Expected XML text member name"}; }
          mapping.text = value;
        }
        else if (key.starts_with("wrapper.")) { mapping.wrappers.emplace(key.substr(8U), value); }
        else if (read_xml_contract_option(mapping, key, value)) {}
        else { throw std::invalid_argument{"Unknown XML class configuration key: " + key}; }
      } else if (section == "xml") {
        if (key == "enabled") {
          result.cpp.xml.enabled = read_bool(value);
          result.cpp.xml.enabled_selection = result.cpp.xml.enabled;
        }
        else if (key == "namespace") { result.cpp.xml.namespace_uri = value; }
        else if (key == "scalars") {
          if (value != "attribute" && value != "element") {
            throw std::invalid_argument{"XML scalars must be attribute or element"};
          }
          result.cpp.xml.scalar_attributes = value == "attribute";
        }
        else if (key == "runtime_xsd") { result.cpp.xml.runtime_xsd = read_bool(value); }
        else if (key == "bundle_sha256") { result.cpp.xml.bundle_sha256 = value; }
        else if (key == "validation_scope") {
          if (value != "input" && value != "output" && value != "both") {
            throw std::invalid_argument{"XML validation_scope must be input, output or both"};
          }
          result.cpp.xml.validation_scope = value;
        }
        else if (key == "xsd") {
          result.cpp.xml.xsd = std::filesystem::path{std::string{value}};
          if (result.cpp.xml.xsd.is_relative()) {
            result.cpp.xml.xsd = std::filesystem::absolute(file).parent_path() / result.cpp.xml.xsd;
          }
        } else { throw std::invalid_argument{"Unknown XML configuration key: " + key}; }
      } else if (const auto class_offset = section.find(".class."); class_offset != std::string::npos) {
        constexpr std::string_view class_separator{".class."};
        const auto language = section.substr(0, class_offset);
        const auto name = section.substr(class_offset + class_separator.size());
        auto& comparisons = language == "cpp" ? result.cpp.class_comparisons :
            language == "java" ? result.java.class_comparisons : result.class_comparisons[language];
        if (key == "auto_size_validation") {
          if (value != "verify" && value != "retain") {
            throw std::invalid_argument{"Expected auto_size_validation = verify or retain"};
          }
          auto& sizes = language == "cpp" ? result.cpp.class_auto_sizes :
              language == "java" ? result.java.class_auto_sizes : result.class_auto_sizes[language];
          sizes[name].validation = value == "verify" ? auto_size_validation::verify :
                                                       auto_size_validation::retain;
        } else if (key == "equality") {
          auto& comparison = comparisons[name];
          comparison.equality = read_bool(value);
        } else if (key == "ordering") {
          if (value != "none" && value != "lexicographic") {
            throw std::invalid_argument{"Expected ordering = none or lexicographic"};
          }
          comparisons[name].ordering = value == "lexicographic";
        } else if (key == "fields") {
          comparisons[name].fields = comparison_fields(value);
        } else {
          throw std::invalid_argument{"Unknown class configuration key: " + key};
        }
      } else if (section == "output" && key == "language") {
        result.language = value;
        parse_output_languages(value);
      } else if (section == "output" && key == "include_mode") {
        result.includes = parse_include_mode(value);
        result.native_includes = result.includes;
      } else if (section == "output" && key == "protocols") {
        result.protocols = parse_output_protocols(value);
      } else if ((section == "js" || section == "go" || section == "csharp") && key == "naming") {
        if (value != "profile" && value != "preserve") {
          throw std::invalid_argument{"Expected naming = profile or preserve"};
        }
        auto& settings = section == "js" ? result.js : section == "go" ? result.go : result.csharp;
        settings.rename_identifiers = value == "profile";
      } else if (section == "kotlin" && key == "package") {
        result.kotlin.package_name = value;
      } else if (section == "go" && key == "package") {
        result.go.package_name = value;
      } else if (section == "csharp" && key == "namespace") {
        result.csharp.namespace_name = value;
      } else if (section == "managed" && key == "id_type") {
        if (value != "uint32" && value != "uint64") {
          throw std::invalid_argument{"managed.id_type must be uint32 or uint64"};
        }
        result.cpp.managed_id_type = value;
      } else if (section == "managed" && key == "separate_values") {
        result.cpp.managed_separate_values = read_bool(value);
      } else if (section == "managed" && key == "id_encoding") {
        result.cpp.managed_id_encoding = parse_managed_id_encoding(value);
      } else if (section == "cpp" && key == "constant_evaluation") {
        result.cpp.constant_evaluation = read_bool(value);
      } else if (section == "cpp" && key == "emission_only") {
        result.cpp.emission_only = read_bool(value);
      } else if (section == "cpp" && key == "protocols") {
        result.cpp.protocols = parse_cpp_protocols(value);
      } else if (section == "cpp" && key == "protobuf") {
        result.cpp.protobuf = read_bool(value);
      } else if (section == "java" && key == "coding_standard") {
        result.java.standard = parse_java_coding_standard(value);
      } else if (section == "java" && key == "package") {
        result.java.package_name = value;
      } else if (section == "java" && key == "output_buffer_reuse") {
        result.java.output_buffer_reuse = read_bool(value);
      } else if (section == "java" && key == "nio_output_buffer_reuse") {
        result.java.nio_output_buffer_reuse = read_bool(value);
      } else if (section == "java" && key == "stream_output") {
        result.java.stream_output = read_bool(value);
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
  result.java.xml = result.cpp.xml;
  result.js.xml = result.cpp.xml;
  result.go.xml = result.cpp.xml;
  result.csharp.xml = result.cpp.xml;
  result.kotlin.xml = result.cpp.xml;
  // Global selection is independent of INI section order; a C++ override stays local.
  const auto cpp_selection = result.cpp.protocols;
  select_output_protocols(result, result.protocols);
  if (keys.contains({"cpp", "protocols"})) { result.cpp.protocols = cpp_selection; }
  return result;
}

// Broad design/security guides inherit Serializer's explicit layout; they prescribe no one layout.
std::string cpp_format_style(coding_standard standard) {
  const std::string access_spacing =
      "EmptyLineBeforeAccessModifier: Leave\nEmptyLineAfterAccessModifier: Leave\n";
  switch (standard) {
  case coding_standard::google:
    return "BasedOnStyle: Google\nLineEnding: CRLF\n" + access_spacing;
  case coding_standard::llvm:
    return "BasedOnStyle: LLVM\nLineEnding: CRLF\n" + access_spacing;
  case coding_standard::gnu:
    return "BasedOnStyle: GNU\nLineEnding: CRLF\n" + access_spacing;
  case coding_standard::qt:
    return "BasedOnStyle: LLVM\nIndentWidth: 4\nColumnLimit: 100\nUseTab: Never\n"
           "BreakBeforeBraces: Custom\nBraceWrapping:\n  AfterClass: true\n  AfterFunction: true\n"
           "PointerAlignment: Right\nReferenceAlignment: Right\nDerivePointerAlignment: false\n"
           "BreakBeforeBinaryOperators: All\nAllowShortFunctionsOnASingleLine: Empty\n"
           "AllowShortIfStatementsOnASingleLine: Never\nLineEnding: CRLF\n" + access_spacing;
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
           "IncludeBlocks: Preserve\nReflowComments: false\nLineEnding: CRLF\n" + access_spacing;
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
