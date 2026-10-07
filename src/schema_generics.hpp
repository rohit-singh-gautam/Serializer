#pragma once

#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rohit::serializer::parser {

// Reuse lexical type lookup from the parser; only already visible declarations are eligible.
syntax_node* find_declared_type(const std::string& name, namespace_node* current_namespace,
                                const std::unordered_map<std::string, syntax_node*>& types);

// The parser defines this bounded-input diagnostic helper before instantiating lowering.
void resolve_type(const rohit::type_check::schema_input_buffer auto& input, type_name& type,
                  const std::unordered_map<std::string, syntax_node*>& types);

// Validate fixed scalar/enum identities after declaration and generic resolution.
void validate_magic_constant(const rohit::type_check::schema_input_buffer auto& input,
                             member& field);
// Normalize inferred char bytes and enum values once their element type is bound.
void normalize_inferred_constants(const rohit::type_check::schema_input_buffer auto& input,
                             member& field);
// Check unsigned compact constraints after lexical binding and again after specialization.
void validate_compact_member(const rohit::type_check::schema_input_buffer auto& input,
                             member& field, bool dependent);

// Lower concrete contracts once while retaining validated native C++ template definitions.
template <typename Input>
class generic_lowering {
  static constexpr std::size_t maximum_instances = 1024;
  static constexpr std::size_t maximum_expansion_depth = 32;
  static constexpr std::size_t maximum_identity_bytes = 4096;
  const Input& input;
  std::vector<std::unique_ptr<syntax_node>> output{};
  std::vector<class_node*> retained_templates{};
  std::unordered_map<std::string, syntax_node*> visible{};
  std::unordered_map<std::string, class_node*> instances{};
  std::unordered_set<std::string> occupied{};
  std::unordered_set<std::string> expanding{};
  std::unordered_set<std::string> incomplete{};

  // Produce a canonical schema identity, independent of generated names and namespace spelling.
  std::string identity(const type_name& type) const {
    if (type.kind == generic_argument_kind::dimension) {
      return "#uint64:" + std::to_string(type.dimension);
    }
    if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
      const auto& object = static_cast<const class_node&>(*type.resolved_node);
      if (!object.generic_name.empty()) {
        std::string result = object.generic_name + "<";
        for (const auto& argument : object.generic_arguments) {
          result += identity(argument) + ",";
        }
        return result + ">";
      }
    }
    return type.get_full_name();
  }

  // Evaluate without host-width arithmetic; dependent validation still checks constant subtrees.
  std::optional<std::uint64_t> evaluate(const dimension_expression& expression,
      const std::unordered_map<std::string, type_name>& bindings, bool dependent = false) const {
    using operation = dimension_expression::operation;
    const auto fail = [&](const std::string& message) {
      throw exception::bad_member_type{input, message + " at schema byte " +
          std::to_string(expression.source_offset)};
    };
    if (expression.kind == operation::literal) { return expression.value; }
    if (expression.kind == operation::parameter) {
      const auto found = bindings.find(expression.name);
      if (found == bindings.end() || found->second.kind != generic_argument_kind::dimension) {
        fail("Invalid dimension or type/value kind mismatch: " + expression.name);
      }
      if (dependent && found->second.dimension == 0) { return std::nullopt; }
      return found->second.dimension;
    }
    const auto left = evaluate(expression.operands[0], bindings, dependent);
    const auto right = evaluate(expression.operands[1], bindings, dependent);
    if (!left || !right) { return std::nullopt; }
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (expression.kind == operation::add) {
      if (*right > maximum - *left) { fail("Dimension addition overflow"); }
      return *left + *right;
    }
    if (*left != 0 && *right > maximum / *left) { fail("Dimension multiplication overflow"); }
    return *left * *right;
  }

  // Bind a numeric argument, interpreting a bare name only in a dimension parameter position.
  void bind_dimension(type_name& argument,
      const std::unordered_map<std::string, type_name>& bindings, bool dependent = false) const {
    if (argument.expression.empty()) {
      if (argument.kind == generic_argument_kind::dimension && argument.dimension != 0) { return; }
      if (argument.application) {
        throw exception::bad_member_type{input, "Type/value kind mismatch: expected dimension"};
      }
      dimension_expression expression{};
      expression.kind = dimension_expression::operation::parameter;
      expression.name = argument.name;
      expression.source_offset = argument.source_offset;
      argument.expression.push_back(std::move(expression));
    }
    const auto value = evaluate(argument.expression.front(), bindings, dependent);
    if (value && *value == 0) {
      throw exception::bad_member_type{input, "Invalid dimension: expected a positive uint64"};
    }
    argument.kind = generic_argument_kind::dimension;
    argument.dimension = value.value_or(0);
  }

  // Build placeholders for earlier parameters without inventing concrete dimensions.
  std::unordered_map<std::string, type_name> placeholders(const class_node& owner) const {
    std::unordered_map<std::string, type_name> result{};
    for (const auto& parameter : owner.generic_parameters) {
      type_name value{std::string{parameter.name}, owner.parent_namespace};
      value.kind = parameter.kind;
      result.emplace(parameter.name, std::move(value));
    }
    return result;
  }

  // Encode every identity byte so different qualified names cannot collapse onto one identifier.
  std::string instance_name(const std::string& key) const {
    if (key.size() > maximum_identity_bytes) {
      throw exception::bad_class{input, "Generic type identity exceeds 4096 bytes"};
    }
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result = "serializer_instance_";
    for (const unsigned char ch : key) {
      result += hex[ch >> 4];
      result += hex[ch & 15];
    }
    return result;
  }

  // Keep original scope objects alive while emitting split namespace blocks in dependency order.
  void emit(std::unique_ptr<syntax_node> node) {
    auto* scope = node->parent_namespace;
    while (scope) {
      auto block = std::make_unique<namespace_node>(
          object_type::namespace_type, std::string{scope->name}, scope->parent_namespace);
      block->statements.push_back(std::move(node));
      node = std::move(block);
      scope = scope->parent_namespace;
    }
    output.push_back(std::move(node));
  }

  // Bind a reference to a concrete type so later resolution cannot capture a use-site shadow.
  void bind(type_name& type, const syntax_node* node) const {
    type.name = node->get_full_name();
    type.declared_namespace = nullptr;
    type.defined_namespace = node->parent_namespace;
    type.resolved_node = node;
    type.type = node->type;
    type.arguments.clear();
    type.application = false;
  }

  // Validate generic bodies even when unused; dependent constraints are checked after substitution.
  void validate_expression(type_name& type, const class_node& owner) const {
    if (type.kind == generic_argument_kind::dimension) {
      throw exception::bad_member_type{input, "Type/value kind mismatch: expected type"};
    }
    if (std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) !=
        owner.type_parameters.end()) {
      const auto parameters = placeholders(owner);
      if (parameters.at(type.name).kind != generic_argument_kind::type) {
        throw exception::bad_member_type{input, "Type/value kind mismatch: expected type"};
      }
      if (!type.arguments.empty()) {
        throw exception::bad_member_type{input, "Type parameters cannot take type arguments"};
      }
      return;
    }
    auto* node = find_declared_type(type.name, type.declared_namespace, visible);
    const auto* object = node && node->type == object_type::class_type
                             ? static_cast<const class_node*>(node)
                             : nullptr;
    const auto arity = object ? object->type_parameters.size() : 0;
    if (object && (object->supports_managed() ||
        object->storage_modes != static_cast<std::uint8_t>(storage_mode::owning))) {
      throw exception::bad_member_type{input, "Generic fields and arguments require unmanaged owning types"};
    }
    if (object && object->get_full_name() == owner.get_full_name()) {
      throw exception::bad_member_type{input, "Recursive generic ownership: " + object->get_full_name()};
    }
    const auto required = object ? std::count_if(object->generic_parameters.begin(),
        object->generic_parameters.end(), [](const auto& parameter) {
          return parameter.default_argument.empty();
        }) : 0;
    if ((arity == 0 && type.application) || type.arguments.size() > arity || type.arguments.size() < static_cast<std::size_t>(required) ||
        (arity != 0 && !type.application) || (node && node->type == object_type::namespace_type) ||
        (!node && serializer::get_cpp_type_or_empty(type.name).empty())) {
      throw exception::bad_member_type{input, "Unknown type or incorrect generic argument count: " +
                                                  type.name};
    }
    for (std::size_t index = 0; index < type.arguments.size(); ++index) {
      auto& argument = type.arguments[index];
      if (object->generic_parameters[index].kind == generic_argument_kind::dimension) {
        bind_dimension(argument, placeholders(owner), true);
      } else {
        validate_expression(argument, owner);
      }
    }
    if (node) {
      type.name = node->get_full_name();
      type.declared_namespace = nullptr;
      type.resolved_node = node;
      type.type = node->type;
    } else {
      type.type = object_type::primitive;
      type.declared_namespace = nullptr;
    }
  }

  // Resolve arguments, enforce the finite owning profile, and share each concrete expansion.
  void resolve(type_name& type, const std::unordered_map<std::string, type_name>& bindings,
               std::size_t depth) {
    if (depth >= maximum_expansion_depth) {
      throw exception::bad_class{input, "Maximum generic expansion depth (32) exceeded"};
    }
    if (type.kind == generic_argument_kind::dimension) {
      throw exception::bad_member_type{input, "Type/value kind mismatch: expected type"};
    }
    if (type.type == object_type::primitive) { return; }
    if (const auto found = bindings.find(type.name); found != bindings.end()) {
      if (found->second.kind != generic_argument_kind::type) {
        throw exception::bad_member_type{input, "Type/value kind mismatch: expected type"};
      }
      if (!type.arguments.empty()) {
        throw exception::bad_member_type{input, "Type parameters cannot take type arguments"};
      }
      const auto enum_name = type.enum_name;
      type = found->second;
      type.enum_name = enum_name;
      return;
    }
    auto* node = find_declared_type(type.name, type.declared_namespace, visible);
    const auto* definition = node && node->type == object_type::class_type
                                 ? static_cast<const class_node*>(node)
                                 : nullptr;
    if (!definition || definition->type_parameters.empty()) {
      if (type.application) {
        throw exception::bad_member_type{input,
                                         "Type does not accept generic arguments: " + type.name};
      }
      resolve_type(input, type, visible);
      if (type.resolved_node) {
        bind(type, type.resolved_node);
      }
      type.declared_namespace = nullptr;
      return;
    }
    if (!type.application || type.arguments.size() > definition->type_parameters.size()) {
      throw exception::bad_member_type{input, "Incorrect generic argument count: " + type.name};
    }
    std::string key = definition->get_full_name() + "<";
    std::unordered_map<std::string, type_name> substitutions{};
    for (std::size_t index = 0; index < definition->generic_parameters.size(); ++index) {
      const auto& parameter = definition->generic_parameters[index];
      const bool supplied = index < type.arguments.size();
      if (!supplied) {
        if (parameter.default_argument.empty()) {
          throw exception::bad_member_type{input, "Missing generic argument: " + parameter.name};
        }
        type.arguments.push_back(parameter.default_argument.front());
      }
      auto& argument = type.arguments[index];
      if (parameter.kind == generic_argument_kind::dimension) {
        bind_dimension(argument, supplied ? bindings : substitutions);
      } else {
        resolve(argument, supplied ? bindings : substitutions, depth + 1);
      }
      if (argument.resolved_node && argument.type == object_type::class_type) {
        const auto& value = static_cast<const class_node&>(*argument.resolved_node);
        if (incomplete.contains(value.get_full_name()) || value.supports_managed() ||
            value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
          throw exception::bad_member_type{input,
                                           "Generic arguments require unmanaged owning types"};
        }
      }
      key += identity(argument) + ",";
      substitutions.emplace(parameter.name, argument);
    }
    key += ">";
    if (const auto found = instances.find(key); found != instances.end()) {
      bind(type, found->second);
      return;
    }
    if (!expanding.insert(definition->get_full_name()).second) {
      throw exception::bad_class{input,
                                 "Recursive generic ownership: " + definition->get_full_name()};
    }
    if (instances.size() + expanding.size() > maximum_instances) {
      throw exception::bad_class{input, "Maximum generic instance count (1024) exceeded"};
    }
    auto concrete =
        std::make_unique<class_node>(object_type::class_type, instance_name(key), nullptr,
                                     definition->attributes, std::vector<parent>{});
    if (!occupied.insert(concrete->name).second) {
      throw exception::bad_class{input, "Generated generic name collision: " + concrete->name};
    }
    concrete->generic_name = definition->get_full_name();
    concrete->source_path = definition->source_path;
    concrete->generic_arguments = type.arguments;
    concrete->generic_parameters = definition->generic_parameters;
    concrete->member_list = definition->member_list;
    concrete->magic_bytes = definition->magic_bytes;
    concrete->magic_field = definition->magic_field;
    concrete->magic_access = definition->magic_access;
    concrete->magic_id = definition->magic_id;
    concrete->magic_explicit_id = definition->magic_explicit_id;
    concrete->magic_omitted_formats = definition->magic_omitted_formats;
    concrete->reserved_ids = definition->reserved_ids;
    concrete->reserved_variables = definition->reserved_variables;
    concrete->reserved_names = definition->reserved_names;
    try {
      resolve_fields(*concrete, substitutions, depth + 1);
    } catch (const std::exception& error) {
      throw exception::bad_class{input, "In specialization " + key + ": " + error.what()};
    }
    auto* result = concrete.get();
    visible.emplace(result->name, result);
    instances.emplace(key, result);
    expanding.erase(definition->get_full_name());
    output.push_back(std::move(concrete));
    bind(type, result);
  }

  // Bound the minimum inline storage before backends emit nested fixed containers.
  std::uint64_t minimum_storage(const class_node& object, std::size_t depth = 0) const {
    if (depth >= maximum_expansion_depth) {
      throw exception::bad_class{input, "Fixed storage nesting resource limit (32) exceeded"};
    }
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max());
    std::uint64_t total{};
    for (const auto& field : object.member_list) {
      if (field.modifier != member::modifier_type::none && field.fixed_extent == 0) { continue; }
      const auto& type = field.type_name_list.front();
      std::uint64_t bytes = 1;
      if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
        bytes = minimum_storage(static_cast<const class_node&>(*type.resolved_node), depth + 1);
      } else if (type.name == "double" || type.name == "uint64" || type.name == "int64") {
        bytes = 8;
      } else if (type.name == "float" || type.name == "uint32" || type.name == "int32") {
        bytes = 4;
      } else if (type.name == "uint16" || type.name == "int16") {
        bytes = 2;
      }
      const auto count = field.fixed_extent ? field.fixed_extent : 1;
      if (bytes > maximum / count || bytes * count > maximum - total) {
        throw exception::bad_member_type{input, "Nested fixed storage byte product exceeds backend representability: " + object.get_full_name() + "." + field.name};
      }
      total += bytes * count;
    }
    return std::max(std::uint64_t{1}, total);
  }

  // Substitute field and map-key types, retaining wire IDs, order, names, and defaults verbatim.
  void resolve_fields(class_node& object,
                      const std::unordered_map<std::string, type_name>& bindings,
                      std::size_t depth) {
    if (object.magic_field) {
      auto& field = *object.magic_field;
      for (auto& type : field.type_name_list) {
        resolve(type, bindings, depth);
      }
      validate_magic_constant(input, field);
    }
    for (auto& field : object.member_list) {
      if (!field.extent_expression.empty()) {
        constexpr std::uint64_t maximum_fixed_elements = 65536;
        const auto extent = evaluate(field.extent_expression.front(), bindings);
        if (!extent || *extent == 0 || *extent > maximum_fixed_elements) {
          throw exception::bad_member_type{input, "Invalid or excessive fixed array extent (1..65536) at schema byte " +
              std::to_string(field.extent_expression.front().source_offset)};
        }
        if (object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
            (object.attributes & class_attributes::packed) == class_attributes::packed || field.managed) {
          throw exception::bad_member_type{input, "Fixed arrays require unpacked ordinary owning fields"};
        }
        field.fixed_extent = *extent;
      }
      for (auto& type : field.type_name_list) {
        resolve(type, bindings, depth);
        if (!object.generic_name.empty() && type.resolved_node &&
            type.type == object_type::class_type) {
          const auto& nested = static_cast<const class_node&>(*type.resolved_node);
          if (nested.supports_managed() ||
              nested.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
            throw exception::bad_member_type{input,
                                             "Generic fields require unmanaged owning types"};
          }
        }
      }
      normalize_inferred_constants(input, field);
      validate_compact_member(input, field, false);
      if (field.modifier == member::modifier_type::map) {
        type_name key{std::string{field.key}, field.type_name_list.front().declared_namespace};
        // Generic bodies were bound lexically at declaration, including qualified map keys.
        key.declared_namespace = object.generic_name.empty() ? object.parent_namespace : nullptr;
        resolve(key, bindings, depth);
        field.key = key.get_full_name();
        field.key_node = key.resolved_node;
      }
    }
    static_cast<void>(minimum_storage(object));
  }

  // Inventory names before generating anything, so later source declarations cannot collide.
  void collect(const std::vector<std::unique_ptr<syntax_node>>& nodes) {
    for (const auto& node : nodes) {
      occupied.insert(node->get_full_name());
      if (node->type == object_type::namespace_type) {
        collect(static_cast<const namespace_node&>(*node).statements);
      }
    }
  }

  // Walk declaration order, exposing definitions only after their source declaration.
  void visit(std::vector<std::unique_ptr<syntax_node>> nodes) {
    for (auto& node : nodes) {
      const auto source_path = node->source_path;
      try {
        auto* raw = node.get();
        visible.emplace(raw->get_full_name(), raw);
        if (raw->type == object_type::namespace_type) {
          auto children = std::move(static_cast<namespace_node*>(raw)->statements);
          emit(std::move(node));
          visit(std::move(children));
        } else if (raw->type == object_type::class_type) {
          auto& object = static_cast<class_node&>(*raw);
          if (!object.type_parameters.empty()) {
            if (object.supports_managed() || !object.parents.empty() ||
                object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
                (object.attributes & class_attributes::packed) == class_attributes::packed) {
              throw exception::bad_class{
                  input,
                  "Generic classes require unpacked unmanaged owning fields without inheritance"};
            }
            class_node earlier{object_type::class_type, std::string{object.name},
                               object.parent_namespace, class_attributes::none, {}};
            for (auto& parameter : object.generic_parameters) {
              try {
                if (!parameter.default_argument.empty()) {
                  auto& argument = parameter.default_argument.front();
                  if (parameter.kind == generic_argument_kind::dimension) {
                    bind_dimension(argument, placeholders(earlier), true);
                  } else {
                    validate_expression(argument, earlier);
                  }
                }
              } catch (const std::exception& error) {
                throw exception::bad_class{input, "Invalid default for " + parameter.name + ": " + error.what()};
              }
              earlier.type_parameters.push_back(parameter.name);
              earlier.generic_parameters.push_back(parameter);
            }
            if (object.magic_field) {
              auto& field = *object.magic_field;
              for (auto& type : field.type_name_list) {
                validate_expression(type, object);
              }
              validate_magic_constant(input, field);
            }
            for (auto& field : object.member_list) {
              if (!field.extent_expression.empty()) {
                const auto extent = evaluate(field.extent_expression.front(), placeholders(object), true);
                if (extent && (*extent == 0 || *extent > 65536)) {
                  throw exception::bad_member_type{input, "Invalid or excessive fixed array extent (1..65536)"};
                }
              }
              if (field.modifier == member::modifier_type::variant) {
                throw exception::bad_class{input, "Generic unions are not supported"};
              }
              for (auto& type : field.type_name_list) {
                validate_expression(type, object);
              }
              validate_compact_member(input, field, true);
              if (field.modifier == member::modifier_type::map) {
                type_name key{std::string{field.key}, object.parent_namespace};
                validate_expression(key, object);
                field.key = key.name;
                field.key_node = key.resolved_node;
              }
            }
            retained_templates.push_back(&object);
            emit(std::move(node));
            continue;
          }
          if (!object.instance_of.empty()) {
            auto& target = object.instance_of.front();
            if (!target.application) {
              throw exception::bad_class{input, "instantiate requires a generic type application"};
            }
            resolve(target, {}, 0);
            const auto& source = static_cast<const class_node&>(*target.resolved_node);
            object.attributes = source.attributes;
            object.member_list = source.member_list;
            object.magic_bytes = source.magic_bytes;
            object.magic_field = source.magic_field;
            object.magic_access = source.magic_access;
            object.magic_id = source.magic_id;
            object.magic_explicit_id = source.magic_explicit_id;
            object.magic_omitted_formats = source.magic_omitted_formats;
            object.reserved_ids = source.reserved_ids;
            object.reserved_variables = source.reserved_variables;
            object.reserved_names = source.reserved_names;
            object.instance_of.clear();
          } else {
            incomplete.insert(object.get_full_name());
            resolve_fields(object, {}, 0);
            incomplete.erase(object.get_full_name());
          }
          emit(std::move(node));
        } else {
          emit(std::move(node));
        }
      } catch (const std::exception& error) {
        if (source_path.empty()) {
          throw;
        }
        throw std::invalid_argument{source_path + ": " + error.what()};
      }
    }
  }

public:
  // Keep diagnostics associated with the compilation input; all caches are per compilation.
  explicit generic_lowering(const Input& source) : input{source} {}

  // Return concrete contracts plus validated generic declarations for native C++ emission.
  std::vector<std::unique_ptr<syntax_node>> run(std::vector<std::unique_ptr<syntax_node>> nodes) {
    collect(nodes);
    visit(std::move(nodes));
    for (auto* definition : retained_templates) {
      definition->type = object_type::generic_definition;
    }
    return std::move(output);
  }
};

// Preserve the existing AST shape and cost for schemas that do not use generic syntax.
inline bool contains_generics(const std::vector<std::unique_ptr<syntax_node>>& nodes) {
  for (const auto& node : nodes) {
    if (node->type == object_type::namespace_type) {
      if (contains_generics(static_cast<const namespace_node&>(*node).statements)) {
        return true;
      }
    } else if (node->type == object_type::class_type) {
      const auto& object = static_cast<const class_node&>(*node);
      if (!object.type_parameters.empty() || !object.instance_of.empty()) {
        return true;
      }
      for (const auto& field : object.member_list) {
        if (!field.extent_expression.empty()) { return true; }
        for (const auto& type : field.type_name_list) {
          if (type.application) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

// Lower generic syntax transactionally before publishing a parsed compilation unit.
void lower_generics(const rohit::type_check::schema_input_buffer auto& input,
                    std::vector<std::unique_ptr<syntax_node>>& nodes) {
  if (contains_generics(nodes)) {
    nodes = generic_lowering{input}.run(std::move(nodes));
  }
}

} // namespace rohit::serializer::parser
