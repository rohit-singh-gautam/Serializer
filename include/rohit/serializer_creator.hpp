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
#include <rohit/digest.hpp>
#include <rohit/output_options.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

// Select an optional unsigned scalar encoding without changing the host value type.
enum class compact_encoding { none, prefix, varint };

enum class object_type { unresolved, namespace_type, class_type, enum_type, primitive, instantiation,
                         generic_definition, native_code };

enum class class_attributes : std::uint8_t {
  none = 0x00, packed = 0x01, stable_ids = 0x02, managed = 0x04
};

struct namespace_node;

// Return the qualified name of the supplied non-null namespace node.
std::string get_full_name_for_namespace(const namespace_node* namespace_ptr);

struct syntax_node {
  object_type type;
  std::string name;
  namespace_node* parent_namespace{nullptr};
  std::string source_path{};
  // Shared leading source notices belong to the compilation unit, not its wire identities.
  std::shared_ptr<const std::vector<std::string>> source_notices{};
  // Initialize this object from the supplied storage or value state.
  syntax_node(object_type type, std::string&& name, namespace_node* parent_namespace)
      : type{type}, name{std::move(name)}, parent_namespace{parent_namespace} {}
  // Initialize this object from the supplied storage or value state.
  syntax_node(syntax_node&& base)
      : type{base.type}, name{std::move(base.name)}, parent_namespace{base.parent_namespace},
        source_path{std::move(base.source_path)}, source_notices{std::move(base.source_notices)} {}
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

// Bounded constant-expression syntax; offsets refer to the original schema input.
struct dimension_expression {
  enum class operation { literal, parameter, add, multiply };
  operation kind{operation::literal};
  std::uint64_t value{};
  std::string name{};
  std::vector<dimension_expression> operands{};
  std::size_t source_offset{};
  // Compare syntax independently of where equivalent expressions were written.
  bool operator==(const dimension_expression& rhs) const {
    return kind == rhs.kind && value == rhs.value && name == rhs.name && operands == rhs.operands;
  }
};

enum class generic_argument_kind { type, dimension };

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
        defined_namespace{rhs.defined_namespace}, type{rhs.type}, resolved_node{rhs.resolved_node},
        arguments{rhs.arguments}, kind{rhs.kind}, expression{rhs.expression},
        dimension{rhs.dimension}, application{rhs.application}, source_offset{rhs.source_offset},
        digest{rhs.digest}, digest_extent{rhs.digest_extent},
        digest_language_supported{rhs.digest_language_supported} {}
  // Assign the documented view or value state from the source object.
  type_name& operator=(const type_name& rhs) {
    name = rhs.name;
    enum_name = rhs.enum_name;
    declared_namespace = rhs.declared_namespace;
    defined_namespace = rhs.defined_namespace;
    type = rhs.type;
    resolved_node = rhs.resolved_node;
    arguments = rhs.arguments;
    kind = rhs.kind;
    expression = rhs.expression;
    dimension = rhs.dimension;
    application = rhs.application;
    source_offset = rhs.source_offset;
    digest = rhs.digest;
    digest_extent = rhs.digest_extent;
    digest_language_supported = rhs.digest_language_supported;
    return *this;
  }

  std::string name;
  std::string enum_name;
  namespace_node* declared_namespace;
  namespace_node* defined_namespace{};
  object_type type{object_type::unresolved};
  const syntax_node* resolved_node{};
  std::vector<type_name> arguments{};
  generic_argument_kind kind{generic_argument_kind::type};
  std::vector<dimension_expression> expression{};
  std::uint64_t dimension{};
  bool application{false};
  std::size_t source_offset{};
  rohit::digest_algorithm digest{rohit::digest_algorithm::none};
  std::uint32_t digest_extent{}; // Zero denotes caller-provided variable-length bytes.
  bool digest_language_supported{true}; // The source file's selected contract survives includes.

  // Identify byte-container primitives without confusing a user declaration named digest.
  bool is_digest() const noexcept {
    return type == object_type::primitive && name == "digest";
  }

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
           declared_namespace == rhs.declared_namespace && arguments == rhs.arguments &&
           kind == rhs.kind && expression == rhs.expression && dimension == rhs.dimension &&
           application == rhs.application && digest == rhs.digest && digest_extent == rhs.digest_extent;
  }
};

// Parameter names remain separately available for existing type-only clients.
struct generic_parameter {
  generic_argument_kind kind{generic_argument_kind::type};
  std::string name{};
  std::vector<type_name> default_argument{};
};

// Compile-time release metadata is owned by the schema AST, never the payload runtime.
struct version_release {
  std::string version{};
  std::string date{};
  // Compare declared release identities and dates without evaluating a policy.
  bool operator==(const version_release&) const = default;
};

enum class version_policy_kind {
  any,
  all,
  max_age,
  keep_last,
  released_since,
  expires_on,
  compatibility
};

// One acceptance expression; groups combine children and leaves carry a checked argument.
struct version_policy {
  version_policy_kind kind{version_policy_kind::all};
  std::string argument{};
  std::vector<version_policy> children{};
  // Preserve policy structure when comparing or cloning compiler metadata.
  bool operator==(const version_policy&) const = default;
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
  bool managed{false};
  bool fixed_name{false}; // Compiler-owned metadata retains its runtime ABI spelling.
  std::vector<dimension_expression> extent_expression{};
  std::uint64_t fixed_extent{}; // Zero denotes a variable-length collection.
  bool version{false};
  std::string magic_bytes{}; // Temporary parser result; class metadata never enters member_list.
  std::vector<std::string> omitted_formats{};
  // Resolve output exclusion during generation rather than storing runtime format state.
  bool omits(std::string_view format) const {
    return transient || std::find(omitted_formats.begin(), omitted_formats.end(), format) != omitted_formats.end();
  }
  bool obsolete{false}; // Bare obsolete deprecates the API without changing its wire lifetime.
  std::string created_version{};
  std::string obsolete_version{};
  std::string replaced_member{};
  std::string compatibility_version{};
  std::vector<version_release> releases{};
  std::optional<version_policy> policy{};
  std::string resolved_compatibility_version{};
  bool inferred_extent{false}; // array[] derives storage from its explicit initializer.
  bool magic{false}; // Typed immutable schema identity, distinct from ordinary object fields.
  compact_encoding compact{compact_encoding::none};
  bool compact_strict{true}; // Prefix overflow is checked unless truncation is explicit.
  bool owning_variant{false}; // The variant keyword selects owning storage without changing union wire data.
  bool transient{false}; // Memory-only schema field; excluded from all durable traversal.

  // Compare the relevant values without modifying either operand.
  bool operator==(const member& rhs) const {
    return access == rhs.access && modifier == rhs.modifier &&
           type_name_list == rhs.type_name_list && name == rhs.name && managed == rhs.managed &&
           extent_expression == rhs.extent_expression && fixed_extent == rhs.fixed_extent &&
           inferred_extent == rhs.inferred_extent && owning_variant == rhs.owning_variant &&
           transient == rhs.transient &&
           version == rhs.version && magic == rhs.magic &&
           compact == rhs.compact && compact_strict == rhs.compact_strict &&
           obsolete == rhs.obsolete &&
           omitted_formats == rhs.omitted_formats &&
           created_version == rhs.created_version && obsolete_version == rhs.obsolete_version &&
           replaced_member == rhs.replaced_member &&
           compatibility_version == rhs.compatibility_version && releases == rhs.releases &&
           policy == rhs.policy &&
           resolved_compatibility_version == rhs.resolved_compatibility_version;
  }
};

// One opaque target-language payload; source offsets identify its original schema span.
struct native_code_block {
  std::string language{};
  std::string text{};
  access_type access{access_type::private_access};
  std::size_t source_offset{};
  std::size_t source_end{};
  std::size_t source_line{1};
  std::size_t source_column{1};
  std::string source_path{};
};

// A typed, expression-only computation; durable field identities are not involved.
struct behavior_expression {
  enum class operation { literal, symbol, pi, to_double, positive, negative, add, subtract, multiply };
  operation kind{operation::literal};
  std::string value{};
  std::vector<behavior_expression> operands{};
  std::size_t source_offset{};
  std::size_t source_end{};
  std::size_t source_line{1};
  std::size_t source_column{1};
  std::string source_path{};
};

// A named argument with the schema's owned value type.
struct function_parameter {
  type_name type{std::string{"double"}, nullptr};
  std::string name{};
};

// Schema-declared behavior is an API contract, not serialized executable state.
struct function_declaration {
  enum class effect_type { readonly, edit };
  enum class dispatch_type { ordinary, virtual_method, abstract_method, override_method };
  access_type access{access_type::public_access};
  type_name return_type{std::string{"void"}, nullptr};
  std::string name{};
  std::vector<function_parameter> parameters{};
  effect_type effect{effect_type::readonly};
  dispatch_type dispatch{dispatch_type::ordinary};
  std::vector<native_code_block> bodies{};
  std::optional<behavior_expression> expression{};
  std::size_t source_offset{};
  std::size_t source_end{};
  std::size_t source_line{1};
  std::size_t source_column{1};
  std::string source_path{};
};

// Keep declaration order separate from the field-only persistence traversal.
struct class_body_item {
  enum class kind_type { field, function, native_code, magic };
  kind_type kind{kind_type::field};
  std::size_t index{};
};

// File-level target hooks are emitted before generated namespaces and declarations.
struct native_code_node : syntax_node {
  native_code_block code{};
  // Keep opaque code outside schema type lookup and wire binding.
  native_code_node(native_code_block block, namespace_node* space)
      : syntax_node{object_type::native_code, std::string{}, space}, code{std::move(block)} {}
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
  bool managed{false}; // Activate identity on this base occurrence, not on every use of its type.
};

// A resolved class schema with the requested C++ storage representations.
struct class_node : public syntax_node {
  class_attributes attributes{};
  std::uint8_t storage_modes{static_cast<std::uint8_t>(storage_mode::owning)};
  std::vector<parent> parents;
  std::vector<member> member_list{};
  std::vector<function_declaration> functions{};
  std::vector<native_code_block> native_blocks{};
  std::vector<class_body_item> body_order{};
  std::string magic_bytes{}; // Exact raw binary prefix; no terminator, length, or field identity.
  std::optional<member> magic_field{}; // Typed scalar or enum identity; no per-object storage.
  access_type magic_access{access_type::private_access};
  std::uint32_t magic_id{1};
  bool magic_explicit_id{false};
  std::vector<std::string> magic_omitted_formats{};
  // Report either legacy fixed bytes or an explicitly typed immutable identity.
  bool has_magic() const {
    return !magic_bytes.empty() || magic_field.has_value();
  }
  // Resolve fixed-header exclusions from compiler metadata.
  bool magic_omits(std::string_view format) const {
    return std::find(magic_omitted_formats.begin(), magic_omitted_formats.end(), format) != magic_omitted_formats.end();
  }
  std::vector<std::string> type_parameters{};
  std::vector<generic_parameter> generic_parameters{};
  std::vector<type_name> instance_of{};
  // Concrete instances retain their schema identity for C++ template aliases and tooling.
  std::string generic_name{};
  std::vector<type_name> generic_arguments{};
  std::vector<std::uint32_t> reserved_ids{};
  std::vector<std::string> reserved_variables{};
  std::vector<std::string> reserved_names{};
  // Initialize this object from the supplied storage or value state.
  class_node(object_type type, std::string&& name, namespace_node* parent_namespace,
             class_attributes attributes, std::vector<parent>&& parents)
      : syntax_node{type, std::move(name), parent_namespace}, attributes{attributes},
        parents{std::move(parents)} {}
  // Initialize this object from the supplied storage or value state.
  class_node(class_node&& rhs)
      : syntax_node{std::move(rhs)}, attributes{rhs.attributes}, storage_modes{rhs.storage_modes},
        parents{std::move(rhs.parents)}, member_list{std::move(rhs.member_list)},
        functions{std::move(rhs.functions)}, native_blocks{std::move(rhs.native_blocks)},
        body_order{std::move(rhs.body_order)},
        magic_bytes{std::move(rhs.magic_bytes)}, magic_field{std::move(rhs.magic_field)},
        magic_access{rhs.magic_access},
        magic_id{rhs.magic_id}, magic_explicit_id{rhs.magic_explicit_id},
        magic_omitted_formats{std::move(rhs.magic_omitted_formats)},
        type_parameters{std::move(rhs.type_parameters)},
        generic_parameters{std::move(rhs.generic_parameters)},
        instance_of{std::move(rhs.instance_of)}, generic_name{std::move(rhs.generic_name)},
        generic_arguments{std::move(rhs.generic_arguments)},
        reserved_ids{std::move(rhs.reserved_ids)},
        reserved_variables{std::move(rhs.reserved_variables)},
        reserved_names{std::move(rhs.reserved_names)} {}
  // Return the sole version discriminator, or null for an unchanged unversioned contract.
  const member* version_member() const {
    for (const auto& field : member_list) {
      if (field.version) {
        return &field;
      }
    }
    return nullptr;
  }
  // Report whether this schema requests a particular generated representation.
  bool has_mode(storage_mode mode) const {
    return (storage_modes & static_cast<std::uint8_t>(mode)) != 0;
  }
  // A single representation is emitted as a concrete class, with no mode template.
  bool multiple_modes() const {
    return (storage_modes & (storage_modes - 1)) != 0;
  }
  // Eligibility comes from this declaration, never from an incoming managed use.
  bool supports_managed() const {
    const auto managed_flag = static_cast<std::uint8_t>(class_attributes::managed);
    if ((static_cast<std::uint8_t>(attributes) & managed_flag) != 0) {
      return true;
    }
    for (const auto& base : parents) {
      if (base.managed) { return true; }
    }
    for (const auto& field : member_list) {
      if (field.managed) { return true; }
    }
    return false;
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

// Refuse fixed cardinality on backends without an exact-length storage/codec mapping.
inline void require_variable_arrays(const std::vector<std::unique_ptr<syntax_node>>& statements,
                                    std::string_view backend) {
  for (const auto& node : statements) {
    if (node->type == object_type::namespace_type) {
      require_variable_arrays(static_cast<const namespace_node&>(*node).statements, backend);
    } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
      for (const auto& field : static_cast<const class_node&>(*node).member_list) {
        if (!field.extent_expression.empty()) {
          throw std::invalid_argument{std::string{backend} + ": fixed arrays are unsupported: " +
              node->source_path + ":" + node->get_full_name() + "." + field.name +
              " at schema byte " + std::to_string(field.extent_expression.front().source_offset)};
        }
      }
    }
  }
}

// Refuse managed declarations on backends without a managed runtime instead of losing semantics.
inline void require_unmanaged_backend(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  for (const auto& node : statements) {
    if (node->type == object_type::namespace_type) {
      require_unmanaged_backend(static_cast<const namespace_node*>(node.get())->statements);
    } else if (node->type == object_type::class_type &&
               static_cast<const class_node*>(node.get())->supports_managed()) {
      throw std::invalid_argument{"Managed generation is currently supported only by the C++ backend"};
    }
  }
}

// Combine the supplied attribute flags into the left operand.
class_attributes& operator|=(class_attributes& lhs, const class_attributes& rhs);
class_attributes operator&(const class_attributes& lhs, const class_attributes& rhs);

// Look up a schema primitive and return an empty string for unknown types.
const std::string& get_cpp_type_or_empty(const std::string& type);
// Return the mapped C++ type, preserving user-defined type names.
const std::string& get_cpp_type(const std::string& type);

namespace parser {
// Evaluate release policies once per parse; an empty reference date selects today's UTC date.
// The warning callback also receives located simplicity advisories after schema validation.
// Callbacks are synchronous, borrow their text only during the call, and may throw to abort parsing.
struct parse_options {
  std::string version_policy_as_of{};
  bool version_policy_warnings_as_errors{false};
  std::function<void(std::string_view)> warning{};
  std::function<void(std::string_view)> information{};
};

// Return today's UTC calendar date for callers sharing one date across multiple entry schemas.
std::string version_policy_reference_date();

// Own a resolved entry schema and its included declarations; dependencies are canonical paths.
struct parsed_schema {
  std::vector<std::unique_ptr<syntax_node>> statements{};
  std::vector<std::filesystem::path> dependencies{};
  // Retain file-level notices even when the complete schema has no declarations.
  std::shared_ptr<const std::vector<std::string>> source_notices{};
};

// Load versioned files with unquoted, file-relative includes and include-once semantics.
// Includes precede declarations; cycles, duplicate types, and unresolved references throw.
// All declarations are owned by the result; no source buffers must outlive this call.
parsed_schema parse_file(const std::filesystem::path& path, const parse_options& options = {});

namespace detail {
// Bridge the compiled parser through byte spans while preserving consumed input on failure.
std::vector<std::unique_ptr<syntax_node>> parse_bytes(std::span<const std::uint8_t> bytes,
                                                      std::size_t& consumed, bool require_version,
                                                      const parse_options& options);
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
std::vector<std::unique_ptr<syntax_node>> parse(Input&& input, bool require_version = false,
                                                const parse_options& options = {}) {
  if constexpr (rohit::type_check::input_buffer<Input>) {
    return detail::invoke(input, [require_version, &options](auto bytes, std::size_t& consumed) {
      return detail::parse_bytes(bytes, consumed, require_version, options);
    });
  } else if constexpr (rohit::detail::memory_input_stream<Input>) {
    const auto view = borrow_stream_bytes(input, decode_limits{}.max_input_bytes);
    return parse(view, require_version, options);
  } else {
    auto buffer = read_stream_bytes(input, decode_limits{}.max_input_bytes);
    const auto view = make_constant_full_stream(buffer.begin(), buffer.current_offset());
    return parse(view, require_version, options);
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
// Preserve file-level source notices, including those from declaration-free schemas.
std::string generate_schema(const parser::parsed_schema& schema, const cpp_options& options = {});
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
// Generate a parsed file while preserving notices even when it declares no models.
std::string generate_schema(const parser::parsed_schema& schema, std::string_view outer_class,
                            const java_options& options = {});
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
// Generate standalone JS, Go, C#, Rust, Python, Swift, Kotlin, C, or TypeScript declarations.
// unit_name supplies the C# outer class; unsupported features fail before publication.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view language, std::string_view unit_name,
                     const portable_options& options = {});
// Preserve complete file-level notices, including declaration-free schemas and their includes.
std::string generate_schema(const parser::parsed_schema& schema, std::string_view language,
                            std::string_view unit_name, const portable_options& options = {});
} // namespace writer::portable
} // namespace rohit::serializer
