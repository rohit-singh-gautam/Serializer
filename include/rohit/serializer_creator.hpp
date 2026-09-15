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
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
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
// Parse schema declarations and resolve their member types; malformed input throws.
std::vector<std::unique_ptr<syntax_node>> parse(const stream& in_stream);
#ifdef ROHIT_SERIALIZER_ENABLE_GTEST
// Parse identifier from the schema input; malformed input throws.
std::string parse_identifier(const stream& in_stream);
// Parse hierarchical identifier from the schema input; malformed input throws.
std::string parse_hierarchical_identifier(const stream& in_stream);
// Invoke the callback for each whitespace-separated identifier.
void space_separated_identifier(const stream& in_stream, std::function<void(std::string&&)> fn);
// Read a schema access keyword; reject unknown or incorrectly cased spellings.
access_type parse_access_type(const stream& in_stream);
// Read one member declaration and retain its wire name and identifier.
member parse_member(const stream& in_stream, const std::uint32_t id,
                    namespace_node* declared_namespace);
// Parse class body from the schema input; malformed input throws.
void parse_class_body(const stream& in_stream, class_node* obj, std::uint32_t& id);
#endif
} // namespace parser

namespace writer::cpp {
// Write the resolved schema as C++ declarations and serialization methods.
void write(stream& out_stream, std::vector<std::unique_ptr<syntax_node>>& statements);
} // namespace writer::cpp
} // namespace rohit::serializer
