#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer {

// Presentation profiles select layout and naming, not whole-standard compliance.
enum class coding_standard { serializer, core, google, llvm, gnu, cert, misra, autosar, qt };

// Preserve the established C++ protocol-selection type and binary API.
enum class cpp_protocols { all, binary_none };
// Select native codec entry points independently of XML and constant evaluation.
using output_protocols = cpp_protocols;

// Select whether filesystem generation emits one C++ header per source schema.
enum class include_mode { separate, inline_output };

// Select generated persistent-ID payload bytes independently of their in-memory width.
enum class persistent_id_encoding { fixed, compact_varint };

// Opt in to comparison APIs for one source-qualified generated owning record.
struct class_comparison_options {
  bool equality{false};
  bool ordering{false};
  // An empty list selects durable fields and base subobjects in declaration order.
  std::vector<std::string> fields{};
};

// Select whether a decoded derived size must match the consumed record bytes.
enum class auto_size_validation { verify, retain };

// Wire selection remains schema-owned; this option changes decoder verification only.
struct class_auto_size_options {
  auto_size_validation validation{auto_size_validation::verify};
};

// Fixed XML mapping authored in generator configuration rather than schema grammar.
struct xml_class_options {
  std::string root{};
  std::string namespace_uri{};
  std::vector<std::string> attributes{};
  std::map<std::string, std::string> names{};
  std::string text{};
  std::map<std::string, std::string> wrappers{};
  // Shared XML contracts are bound before backend capability checks, never silently ignored.
  std::map<std::string, std::string> contracts{};
};

// Check the closed shared-contract key/value grammar for programmatic and INI clients alike.
void validate_xml_contract_options(const xml_class_options& options);

// XML generation is opt-in and does not alter other codec layouts or wire identities.
struct xml_options {
  bool enabled{false};
  bool runtime_xsd{false};
  std::string validation_scope{"input"};
  std::filesystem::path xsd{};
  std::map<std::string, xml_class_options> classes{};
  std::string bundle_sha256{};
  // Defaults apply only to XML generation; explicit class/field bindings take precedence.
  std::string namespace_uri{};
  bool scalar_attributes{false};
  // Preserve an explicit opt-out when schema-authored metadata would otherwise enable XML.
  std::optional<bool> enabled_selection{};
  // Package-qualified role profiles stay separate from native model and XML field mappings.
  std::map<std::string, std::map<std::string, std::string>> package_profiles{};
};

struct cpp_options {
  coding_standard standard{coding_standard::serializer};
  bool rename_identifiers{true};
  bool format{true};
  std::filesystem::path clang_format{"clang-format"};
  std::filesystem::path format_file{};
  bool protobuf{false};
  std::string managed_id_type{"uint32"};
  bool managed_separate_values{false};
  bool constant_evaluation{false};
  cpp_protocols protocols{cpp_protocols::all};
  bool emission_only{false};
  std::map<std::string, class_comparison_options> class_comparisons{};
  std::map<std::string, class_auto_size_options> class_auto_sizes{};
  xml_options xml{};
  persistent_id_encoding managed_id_encoding{persistent_id_encoding::compact_varint};
};

enum class java_coding_standard { serializer, google, oracle };

struct java_options {
  java_coding_standard standard{java_coding_standard::serializer};
  bool rename_identifiers{true};
  std::string package_name{};
  std::map<std::string, class_comparison_options> class_comparisons{};
  std::map<std::string, class_auto_size_options> class_auto_sizes{};
  xml_options xml{};
  output_protocols protocols{output_protocols::all};
  bool output_buffer_reuse{false}; // Opt in to caller-owned output-window encode overloads.
  bool nio_output_buffer_reuse{false}; // Opt in to caller-owned heap/direct ByteBuffer windows.
};

// Native naming and module configuration for portable owning backends.
struct portable_options {
  bool rename_identifiers{true};
  std::string package_name{"generated"};
  std::string namespace_name{"SerializerGenerated"};
  std::map<std::string, class_comparison_options> class_comparisons{};
  std::map<std::string, class_auto_size_options> class_auto_sizes{};
  xml_options xml{};
  output_protocols protocols{output_protocols::all};
};

struct output_options {
  // One language or a comma-separated list, validated by parse_output_languages.
  std::string language{"cpp"};
  cpp_options cpp{};
  java_options java{};
  portable_options js{};
  portable_options go{};
  portable_options csharp{};
  portable_options kotlin{true, "", ""};
  // Other portable sections remain independent even when their naming profile is shared.
  std::map<std::string, std::map<std::string, class_comparison_options>> class_comparisons{};
  std::map<std::string, std::map<std::string, class_auto_size_options>> class_auto_sizes{};
  include_mode includes{include_mode::separate};
  output_protocols protocols{output_protocols::all};
  include_mode native_includes{include_mode::inline_output}; // Preserve established native CLI defaults.
};

// Apply a common native codec selection to all output backends; XML remains independent.
void select_output_protocols(output_options& options, output_protocols protocols);

// Parse an exact native protocol profile, rejecting unsupported selections.
output_protocols parse_output_protocols(std::string_view name);

// Parse a nonempty comma-separated language list; reject unknown, empty, or repeated names.
std::vector<std::string> parse_output_languages(std::string_view names);

// Resolve the exact include layout spelling; reject unknown modes.
include_mode parse_include_mode(std::string_view name);

// Resolve an exact persistent-ID payload policy; unknown spellings are errors.
persistent_id_encoding parse_managed_id_encoding(std::string_view name);

// Parse the exact generated C++ protocol selection, rejecting unknown names.
cpp_protocols parse_cpp_protocols(std::string_view name);
// Return the stable spelling of a valid C++ protocol selection.
std::string_view cpp_protocols_name(cpp_protocols protocols);

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
