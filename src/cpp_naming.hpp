#pragma once

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::cpp {

// Resolve target identifiers independently of the immutable schema and its wire spellings.
class naming {
  const cpp_options& options;

protected:
  // Bind one generation's naming policy; no global state is shared between writers.
  explicit naming(const cpp_options& options) : options{options} {}
  // Select type spelling for declarations and all resolved references.
  std::string type_name(std::string_view name) const;
  // Select class data member spelling, including Google's trailing underscore.
  std::string field_name(std::string_view name) const;
  // Select names for generated parameters and locals without the class-field suffix.
  std::string local_name(std::string_view name) const;
  // Select scoped enum values independently of union storage member spelling.
  std::string enum_name(std::string_view name) const;
  // Select ordinary function spelling; required runtime and ADL hooks keep their fixed names.
  std::string function_name(std::string_view name) const;
  // Keep namespaces in snake_case for every profile that renames identifiers.
  std::string namespace_name(std::string_view name) const;
  // Return a fully qualified, renamed type without changing parser resolution metadata.
  std::string full_type_name(const syntax_node* node) const;
  // Convert a literal or a declared enum initializer; reject opaque expressions during renaming.
  std::string default_value(const member& field) const;
  // Detect keywords and names collapsed together by the selected naming convention.
  void validate_names(const std::vector<std::unique_ptr<syntax_node>>& statements) const;
};
} // namespace rohit::serializer::writer::cpp
