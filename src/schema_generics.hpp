#pragma once

#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
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

// Expand portable type parameters once, before any language writer or compatibility check.
template <typename Input>
class generic_lowering {
  static constexpr std::size_t maximum_instances = 1024;
  static constexpr std::size_t maximum_expansion_depth = 32;
  static constexpr std::size_t maximum_identity_bytes = 4096;
  const Input& input;
  std::vector<std::unique_ptr<syntax_node>> output{};
  std::vector<std::unique_ptr<syntax_node>> definitions{};
  std::unordered_map<std::string, syntax_node*> visible{};
  std::unordered_map<std::string, class_node*> instances{};
  std::unordered_set<std::string> occupied{};
  std::unordered_set<std::string> expanding{};
  std::unordered_set<std::string> incomplete{};

  // Produce a canonical schema identity, independent of generated names and namespace spelling.
  std::string identity(const type_name& type) const {
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
  }

  // Validate generic bodies even when unused; dependent constraints are checked after substitution.
  void validate_expression(type_name& type, const class_node& owner) const {
    if (std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) !=
        owner.type_parameters.end()) {
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
    if (arity != type.arguments.size() || (node && node->type == object_type::namespace_type) ||
        (!node && serializer::get_cpp_type_or_empty(type.name).empty())) {
      throw exception::bad_member_type{input, "Unknown type or incorrect generic argument count: " +
                                                  type.name};
    }
    for (auto& argument : type.arguments) {
      validate_expression(argument, owner);
    }
    if (node) {
      type.name = node->get_full_name();
      type.declared_namespace = nullptr;
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
    if (type.type == object_type::primitive) { return; }
    if (const auto found = bindings.find(type.name); found != bindings.end()) {
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
      if (!type.arguments.empty()) {
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
    if (type.arguments.size() != definition->type_parameters.size()) {
      throw exception::bad_member_type{input, "Incorrect generic argument count: " + type.name};
    }
    std::string key = definition->get_full_name() + "<";
    for (auto& argument : type.arguments) {
      resolve(argument, bindings, depth + 1);
      if (argument.resolved_node && argument.type == object_type::class_type) {
        const auto& value = static_cast<const class_node&>(*argument.resolved_node);
        if (incomplete.contains(value.get_full_name()) || value.supports_managed() ||
            value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
          throw exception::bad_member_type{input,
                                           "Generic arguments require unmanaged owning types"};
        }
      }
      key += identity(argument) + ",";
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
    concrete->member_list = definition->member_list;
    std::unordered_map<std::string, type_name> substitutions{};
    for (std::size_t index = 0; index < type.arguments.size(); ++index) {
      substitutions.emplace(definition->type_parameters[index], type.arguments[index]);
    }
    resolve_fields(*concrete, substitutions, depth + 1);
    auto* result = concrete.get();
    visible.emplace(result->name, result);
    instances.emplace(key, result);
    expanding.erase(definition->get_full_name());
    output.push_back(std::move(concrete));
    bind(type, result);
  }

  // Substitute field and map-key types, retaining wire IDs, order, names, and defaults verbatim.
  void resolve_fields(class_node& object,
                      const std::unordered_map<std::string, type_name>& bindings,
                      std::size_t depth) {
    for (auto& field : object.member_list) {
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
      if (field.modifier == member::modifier_type::map) {
        type_name key{std::string{field.key}, field.type_name_list.front().declared_namespace};
        // Generic bodies were bound lexically at declaration, including qualified map keys.
        key.declared_namespace = object.generic_name.empty() ? object.parent_namespace : nullptr;
        resolve(key, bindings, depth);
        field.key = key.get_full_name();
        field.key_node = key.resolved_node;
      }
    }
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
            for (auto& field : object.member_list) {
              if (field.modifier == member::modifier_type::variant) {
                throw exception::bad_class{input, "Generic unions are not supported"};
              }
              for (auto& type : field.type_name_list) {
                validate_expression(type, object);
              }
              if (field.modifier == member::modifier_type::map) {
                type_name key{std::string{field.key}, object.parent_namespace};
                validate_expression(key, object);
                field.key = key.name;
              }
            }
            definitions.push_back(std::move(node));
            continue;
          }
          if (!object.instance_of.empty()) {
            auto& target = object.instance_of.front();
            if (target.arguments.empty()) {
              throw exception::bad_class{input, "instantiate requires a generic type application"};
            }
            resolve(target, {}, 0);
            const auto& source = static_cast<const class_node&>(*target.resolved_node);
            object.attributes = source.attributes;
            object.member_list = source.member_list;
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

  // Return only ordinary resolved declarations; writers never need dependent-type semantics.
  std::vector<std::unique_ptr<syntax_node>> run(std::vector<std::unique_ptr<syntax_node>> nodes) {
    collect(nodes);
    visit(std::move(nodes));
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
        for (const auto& type : field.type_name_list) {
          if (!type.arguments.empty()) {
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
