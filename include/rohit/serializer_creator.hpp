//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#pragma once
#include <rohit/output_options.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Gramar
// STRUCTFILE: statementlist
// namespace: "namespace" space hirerchical_identifier space '{' space statementlist space '}'
// statementlist: statement | statement space statementlist
// statement: namespace | class
// class: classheader classbody | classextendedheader space classbody
// classextendedheader: classheader space ':' space classparentlist
// classheader: "class" space identifier space classattributelist
// classparent: accesstypeidentifier space  hirerchical_identifier
// classparentlist: classparent | classparent space ',' space classparentlist
// classattributelist: identifer | identifier space classattributelist
// classbody: '{' space memberlist space '}'
// memberlist: member | member space memberlist
// member: accesstypeidentifier space typeidentifier space identifier space ';'
// hirerchical_identifier: identifier | identifier "::" hirerchical_identifier
// accesstypeidentifier: "private" | "protected" | "public"
// typeidentifier: identifier
// space: onespace | onespace space
// onespace: ' ' | '\t' | '\n' | '\r'

namespace rohit::serializer {
namespace exception {
class bad_identifier : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_member_spec : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_access_type : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_object_type : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_class_member : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_member_type : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_class : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

class bad_namespace : public rohit::exception::base_parser {
public:
  using rohit::exception::base_parser::base_parser;
};

} // namespace exception

// Convert ASCII uppercase letters to lowercase without changing other bytes.
void to_lower_in_place(std::string& value);

enum class access_type { error, private_access, protected_access, public_access };

enum class object_type { unresolved, namespace_type, class_type, enum_type, primitive };

enum class class_attributes : std::uint8_t { none = 0x00, packed = 0x01, stable_ids = 0x02 };

struct namespace_node;

// Return the qualified name of the supplied non-null namespace node.
std::string get_full_name_for_namespace(const namespace_node* namespace_ptr);

struct syntax_node {
  object_type type;
  std::string name;
  namespace_node* parent_namespace{nullptr};
  // Initialize this object from the supplied storage or value state.
  syntax_node(object_type type, std::string&& name, namespace_node* parent_namespace)
      : type{type}, name{std::move(name)}, parent_namespace{parent_namespace} {}
  // Initialize this object from the supplied storage or value state.
  syntax_node(syntax_node&& base)
      : type{base.type}, name{std::move(base.name)}, parent_namespace{base.parent_namespace} {}
  // Release resources owned by this object.
  virtual ~syntax_node() = default;
  // Initialize this object from the supplied storage or value state.
  syntax_node(const syntax_node& base) = delete;
  // Assign the documented view or value state from the source object.
  syntax_node& operator=(const syntax_node&) = delete;
  // Resolve this syntax node name relative to its containing namespace.
  std::string get_full_name() const {
    std::string full_name{};
    if (parent_namespace) {
      full_name = get_full_name_for_namespace(parent_namespace);
      full_name += "::";
    }
    full_name += name;
    return full_name;
  }
};

struct namespace_node : public syntax_node {
  // Ordered source-block contents; children of reopened blocks share the first namespace scope.
  std::vector<std::unique_ptr<syntax_node>> statements{};
  // Initialize this object from the supplied storage or value state.
  namespace_node(object_type type, std::string&& name, namespace_node* parent_namespace)
      : syntax_node{type, std::move(name), parent_namespace} {}
};

struct type_name {
  // Initialize this object from the supplied storage or value state.
  type_name(std::string&& name, namespace_node* declared_namespace)
      : name{std::move(name)}, enum_name{}, declared_namespace{declared_namespace} {}
  // Initialize this object from the supplied storage or value state.
  type_name(std::string&& name, std::string&& enum_name, namespace_node* declared_namespace)
      : name{std::move(name)}, enum_name{std::move(enum_name)},
        declared_namespace{declared_namespace} {}
  // Initialize this object from the supplied storage or value state.
  type_name(const type_name& rhs)
      : name{rhs.name}, enum_name{rhs.enum_name}, declared_namespace{rhs.declared_namespace},
        defined_namespace{rhs.defined_namespace}, type{rhs.type}, resolved_node{rhs.resolved_node} {}
  // Assign the documented view or value state from the source object.
  type_name& operator=(const type_name& rhs) {
    name = rhs.name;
    enum_name = rhs.enum_name;
    declared_namespace = rhs.declared_namespace;
    defined_namespace = rhs.defined_namespace;
    type = rhs.type;
    resolved_node = rhs.resolved_node;
    return *this;
  }

  std::string name;
  std::string enum_name;
  namespace_node* declared_namespace;
  namespace_node* defined_namespace{};
  object_type type{object_type::unresolved};
  const syntax_node* resolved_node{};

  // Resolve this syntax node name relative to its containing namespace.
  std::string get_full_name() const {
    if (resolved_node) { return resolved_node->get_full_name(); }
    std::string full_name{};
    if (defined_namespace) {
      full_name = get_full_name_for_namespace(defined_namespace);
      full_name += "::";
    }
    full_name += name;
    return full_name;
  }

  // Compare the relevant values without modifying either operand.
  bool operator==(const type_name& rhs) const {
    return name == rhs.name && enum_name == rhs.enum_name &&
           declared_namespace == rhs.declared_namespace;
  }
};

struct member {
  enum class modifier_type { none, array, map, variant };
  access_type access;
  modifier_type modifier;
  std::vector<type_name> type_name_list;
  std::string name;
  std::string display_name;
  std::uint32_t id;
  std::string key; // Optional parameter
  std::string default_value;
  bool explicit_id{false};

  const syntax_node* key_node{};

  // Compare the relevant values without modifying either operand.
  bool operator==(const member& rhs) const {
    return access == rhs.access && modifier == rhs.modifier &&
           type_name_list == rhs.type_name_list && name == rhs.name;
  }
};

struct class_node;

struct parent {
  access_type access{};
  std::string name{};
  std::string display_name{};
  std::uint32_t id{};
  namespace_node* current_namespace{};
  class_node* parent_class{nullptr}; // This will be filled in later
  bool explicit_id{false};
};

// A resolved class schema with the requested C++ storage representations.
struct class_node : public syntax_node {
  class_attributes attributes{};
  std::uint8_t storage_modes{static_cast<std::uint8_t>(storage_mode::owning)};
  std::vector<parent> parents;
  std::vector<member> member_list{};
  // Initialize this object from the supplied storage or value state.
  class_node(object_type type, std::string&& name, namespace_node* parent_namespace,
             class_attributes attributes, std::vector<parent>&& parents)
      : syntax_node{type, std::move(name), parent_namespace}, attributes{attributes},
        parents{std::move(parents)} {}
  // Initialize this object from the supplied storage or value state.
  class_node(class_node&& rhs)
      : syntax_node{std::move(rhs)}, attributes{rhs.attributes}, storage_modes{rhs.storage_modes},
        parents{std::move(rhs.parents)},
        member_list{std::move(rhs.member_list)} {}
  // Report whether this schema requests a particular generated representation.
  bool has_mode(storage_mode mode) const {
    return (storage_modes & static_cast<std::uint8_t>(mode)) != 0;
  }
  // A single representation is emitted as a concrete class, with no mode template.
  bool multiple_modes() const {
    return (storage_modes & (storage_modes - 1)) != 0;
  }
  // Initialize this object from the supplied storage or value state.
  class_node(const class_node&) = delete;
  // Assign the documented view or value state from the source object.
  class_node& operator=(const class_node&) = delete;
};

struct enum_node : public syntax_node {
  // Initialize this object from the supplied storage or value state.
  enum_node(object_type type, std::string&& name, namespace_node* parent_namespace,
            std::vector<std::string>&& enum_name_list)
      : syntax_node{type, std::move(name), parent_namespace},
        enum_name_list{std::move(enum_name_list)} {}

  std::vector<std::string> enum_name_list{};
};

// Combine the supplied attribute flags into the left operand.
class_attributes& operator|=(class_attributes& lhs, const class_attributes& rhs);
class_attributes operator&(const class_attributes& lhs, const class_attributes& rhs);

// Look up a schema primitive and return an empty string for unknown types.
const std::string& get_cpp_type_or_empty(const std::string& type);
// Return the mapped C++ type, preserving user-defined type names.
const std::string& get_cpp_type(const std::string& type);

namespace parser {
// Own a resolved entry schema and its included declarations; dependencies are canonical paths.
struct parsed_schema {
  std::vector<std::unique_ptr<syntax_node>> statements{};
  std::vector<std::filesystem::path> dependencies{};
};

// Load versioned files with unquoted, file-relative includes and include-once semantics.
// Includes precede declarations; cycles, duplicate types, and unresolved references throw.
// All declarations are owned by the result; no source buffers must outlive this call.
parsed_schema parse_file(const std::filesystem::path& path);

namespace detail {
// Bridge the compiled parser through byte spans while preserving consumed input on failure.
std::vector<std::unique_ptr<syntax_node>> parse_bytes(std::span<const std::uint8_t> bytes,
                                                      std::size_t& consumed, bool require_version);
// Expose individual parser operations to the test-only wrappers below.
std::string identifier_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed);
// Parse one qualified identifier through the compiled schema scanner.
std::string hierarchical_identifier_bytes(std::span<const std::uint8_t> bytes,
                                          std::size_t& consumed);
// Visit each whitespace-separated identifier through the compiled schema scanner.
void identifiers_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed,
                       std::function<void(std::string&&)> callback);
// Read one schema access specifier.
access_type access_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed);
// Read one schema member declaration.
member member_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed, std::uint32_t id,
                    namespace_node* declared_namespace);
// Read the members in a schema class body.
void class_body_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed,
                      class_node* object, std::uint32_t& id);

// Commit the compiled parser's consumed byte count on both success and failure.
template <rohit::type_check::input_buffer Input>
struct input_progress {
  const Input& input;
  const std::size_t& consumed;
  // Advance only the range that the compiled parser has already checked.
  ~input_progress() {
    input.get_curr_and_increase_unchecked(consumed);
  }
};

// Borrow a custom cursor's bytes and propagate progress without copying schema storage.
template <rohit::type_check::input_buffer Input, typename Parse>
decltype(auto) invoke(const Input& input, Parse&& parse) {
  std::size_t consumed{};
  const input_progress<Input> progress{input, consumed};
  return parse(std::span<const std::uint8_t>{input.curr(), input.remaining_buffer()}, consumed);
}
} // namespace detail

// Parse declarations from a custom buffer or one bounded EOF-delimited byte source.
// Includes require parse_file so relative paths have an explicit source directory.
template <rohit::type_check::input_stream Input>
std::vector<std::unique_ptr<syntax_node>> parse(Input&& input, bool require_version = false) {
  if constexpr (rohit::type_check::input_buffer<Input>) {
    return detail::invoke(input, [require_version](auto bytes, std::size_t& consumed) {
      return detail::parse_bytes(bytes, consumed, require_version);
    });
  } else if constexpr (rohit::detail::memory_input_stream<Input>) {
    const auto view = borrow_stream_bytes(input, decode_limits{}.max_input_bytes);
    return parse(view, require_version);
  } else {
    auto buffer = read_stream_bytes(input, decode_limits{}.max_input_bytes);
    const auto view = make_constant_full_stream(buffer.begin(), buffer.current_offset());
    return parse(view, require_version);
  }
}

#ifdef ROHIT_SERIALIZER_ENABLE_GTEST
// Parse an identifier and preserve consumed input on failure.
inline std::string parse_identifier(const rohit::type_check::input_buffer auto& input) {
  return detail::invoke(input, detail::identifier_bytes);
}
// Parse a hierarchical identifier through the public cursor concept.
inline std::string
parse_hierarchical_identifier(const rohit::type_check::input_buffer auto& input) {
  return detail::invoke(input, detail::hierarchical_identifier_bytes);
}
// Invoke the callback for each whitespace-separated identifier.
inline void space_separated_identifier(const rohit::type_check::input_buffer auto& input,
                                       std::function<void(std::string&&)> callback) {
  detail::invoke(input, [&callback](auto bytes, std::size_t& consumed) {
    detail::identifiers_bytes(bytes, consumed, std::move(callback));
  });
}
// Read one access keyword, preserving parser diagnostics and cursor progress.
inline access_type parse_access_type(const rohit::type_check::input_buffer auto& input) {
  return detail::invoke(input, detail::access_bytes);
}
// Parse one schema member from an independent input buffer.
inline member parse_member(const rohit::type_check::input_buffer auto& input, std::uint32_t id,
                           namespace_node* declared_namespace) {
  return detail::invoke(input, [=](auto bytes, std::size_t& consumed) {
    return detail::member_bytes(bytes, consumed, id, declared_namespace);
  });
}
// Parse a class body and commit the checked consumed range even on failure.
inline void parse_class_body(const rohit::type_check::input_buffer auto& input, class_node* object,
                             std::uint32_t& id) {
  detail::invoke(input, [&](auto bytes, std::size_t& consumed) {
    detail::class_body_bytes(bytes, consumed, object, id);
  });
}
#endif
} // namespace parser

namespace writer::cpp {
// Validate names and return a completely generated and formatted C++ header.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     const cpp_options& options = {});
// Append validated generated text to any concept-conforming buffer or byte sink.
inline void write(rohit::type_check::output_stream auto& output,
                  const std::vector<std::unique_ptr<syntax_node>>& statements,
                  const cpp_options& options = {}) {
  const auto text = generate(statements, options);
  if constexpr (rohit::type_check::output_buffer<std::remove_reference_t<decltype(output)>>) {
    output.write(text);
  } else {
    write_stream_bytes(output, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
  }
}
} // namespace writer::cpp
namespace writer::java {
// Validate and return one self-contained Java 17 source file.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view outer_class, const java_options& options = {});
// Append validated generated Java text through the same output stream concept.
inline void write(rohit::type_check::output_stream auto& output,
                  const std::vector<std::unique_ptr<syntax_node>>& statements,
                  std::string_view outer_class, const java_options& options = {}) {
  const auto text = generate(statements, outer_class, options);
  if constexpr (rohit::type_check::output_buffer<std::remove_reference_t<decltype(output)>>) {
    output.write(text);
  } else {
    write_stream_bytes(output, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
  }
}
} // namespace writer::java
namespace writer::portable {
// Generate standalone JS, Go, C#, or TypeScript declarations from a resolved schema.
// unit_name supplies the C# outer class; unsupported features fail before publication.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view language, std::string_view unit_name,
                     const portable_options& options = {});
} // namespace writer::portable
} // namespace rohit::serializer
