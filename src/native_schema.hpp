#pragma once

#include <rohit/serializer_creator.hpp>
#include "xml_schema.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::native {

// Shared, validated schema metadata for the additional native generators.
struct schema {
  std::vector<const syntax_node*> nodes{};
  std::vector<const package_node*> packages{};
  std::vector<native_code_block> preambles{};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, std::size_t> keys{};
  std::vector<std::string> ordered_keys{"key", "value"};
  std::map<std::string, class_comparison_options> class_comparisons{};
  std::map<std::string, class_auto_size_options> class_auto_sizes{};
  xml_options xml{};
  output_protocols protocols{output_protocols::all};
  std::map<std::string, std::size_t> source_files{};
  // Mark declaration boundaries only while planning a multi-file native bundle.
  [[nodiscard]] std::string file_marker(const syntax_node* node) const {
    if (source_files.empty()) { return {}; }
    const auto source = node ? (node->output_source_path.empty() ? node->source_path : node->output_source_path) : std::string{};
    const auto found = source_files.find(source);
    const auto index = found == source_files.end() ? source_files.size() - 1 : found->second;
    return "\n\x1e" + std::to_string(index) + "\x1f\n";
  }
  std::vector<std::shared_ptr<const xml_binding::declaration_inventory>> xml_inventories{};
  // Append each private validation AST once after XML options have been configured.
  void collect_xml_validators();
  // Find the shared declaration dispatcher for authored or private validator classes.
  [[nodiscard]] const xml_binding::declaration_inventory* inventory_for(const class_node* source) const;

  // Resolve flattened names and validate portable owning shapes and defaults.
  explicit schema(const std::vector<std::unique_ptr<syntax_node>>& statements);
  // Add every declaration in source order, retaining the full namespace prefix.
  void collect(const std::vector<std::unique_ptr<syntax_node>>& statements,
               const std::string& prefix = {});
  // Intern a wire spelling once for pre-encoded field tables.
  void add_key(const std::string& value);
};

// Convert a schema identifier to native PascalCase or snake_case presentation.
std::string pascal(std::string_view value);
std::string snake(std::string_view value);
// Quote a UTF-8 source literal without changing its contents.
std::string quote(std::string_view value);
// Normalize and validate a portable default, retaining strings as quoted literals.
std::string literal(const member& value);
// Return fixed scalar byte width, or zero for variable-length values.
int width(const type_name& value);
// Resolve the scalar metadata of a map key.
type_name map_key(const member& value);
// Reject language keywords and collisions with generated/runtime identifiers.
void validate_identifier(std::string_view value, std::string_view reserved);

// Generate each standalone native compilation unit; all implementation remains C++.
std::string python(const schema& value);
std::string rust(const schema& value);
std::string swift(const schema& value);
std::string kotlin(const schema& value, std::string_view package);
std::string c(const schema& value);

} // namespace rohit::serializer::writer::native
