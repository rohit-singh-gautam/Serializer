#pragma once

#include <rohit/serializer_creator.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::native {

// Shared, validated schema metadata for the additional native generators.
struct schema {
  std::vector<const syntax_node*> nodes{};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, std::size_t> keys{};
  std::vector<std::string> ordered_keys{"key", "value"};

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
