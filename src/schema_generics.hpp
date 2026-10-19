#pragma once

#include <rohit/serializer_creator.hpp>
#include "conversion_contract.hpp"
#include "stateless_constraint.hpp"
#include "callable_guard.hpp"

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
// Validate digest defaults and unsupported storage profiles after substitutions.
void validate_digest_member(const rohit::type_check::schema_input_buffer auto& input,
                            const class_node& owner, const member& field);

// Check the owning profile and defaults for recursive built-in generic value types.
void validate_optional_member(const rohit::type_check::schema_input_buffer auto& input,
                              const class_node& owner, const member& field);
// Validate overlap hints in concrete records and explicitly constrained template definitions.
void validate_empty_layout(const rohit::type_check::schema_input_buffer auto& input,
                          const class_node& owner, const member& field);

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
  std::string application_source_path{};
  // Named specializations copy signatures before the deferred callable pass substitutes them.
  std::unordered_map<const class_node*, std::unordered_map<std::string, type_name>> named_bindings{};

  // Produce a canonical schema identity, independent of generated names and namespace spelling.
  std::string identity(const type_name& type) const {
    if (type.is_optional() || type.is_sequence() || type.is_associative() || type.is_managed_reference()) {
      std::string result = type.name + "<";
      for (const auto& argument : type.arguments) {
        result += identity(argument) + ",";
      }
      return result + ">";
    }
    if (type.is_digest()) {
      return "digest:" + std::to_string(static_cast<unsigned>(type.digest)) + ":" +
             std::to_string(type.digest_extent);
    }
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
      block->source_path = node->source_path;
      block->output_source_path = node->output_source_path;
      block->source_notices = node->source_notices;
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

  // Recognize only unshadowed generic syntax and keep each included file's language contract.
  bool bind_builtin(type_name& type, const syntax_node* declaration) const {
    if ((declaration && !type.is_optional() && !type.is_sequence() && !type.is_associative() &&
         !type.is_expected() && !type.is_managed_reference()) ||
        !type.application ||
        (type.name != "optional" && type.name != "array" && type.name != "map" &&
         type.name != "expected" && type.name != "managed_ref")) {
      return false;
    }
    if (type.name == "expected" && !type.expected_language_supported) {
      throw exception::bad_member_type{input, "Expected callable results require serializer version 1.17.0"};
    }
    if (type.name == "managed_ref" && !type.managed_reference_language_supported) {
      throw exception::bad_member_type{input, "Typed managed references require serializer version 1.20.0 or newer"};
    }
    if (type.name != "expected" && type.name != "managed_ref" && !type.optional_language_supported) {
      throw exception::bad_member_type{input,
          "Built-in optional and collection type arguments require serializer version 1.7.0 or newer"};
    }
    const std::size_t required_arguments = type.name == "map" || type.name == "expected" ? 2 : 1;
    if (type.arguments.size() != required_arguments) {
      throw exception::bad_member_type{input, "Incorrect built-in generic argument count: " + type.name};
    }
    type.type = object_type::primitive;
    type.declared_namespace = nullptr;
    return true;
  }

  // Preserve owning C++ record keys while portable backends enforce their narrower key contract.
  void validate_builtin_key(const type_name& type, bool dependent) const {
    if (!type.is_associative()) { return; }
    const auto& key = type.arguments.front();
    if (dependent && key.type == object_type::unresolved) { return; }
    if (key.is_optional() || key.is_sequence() || key.is_associative() || key.is_digest() ||
        key.is_managed_reference() ||
        key.name == "float" || key.name == "double") {
      throw exception::bad_member_type{input, "Generic map keys require a scalar, enum, or owning record"};
    }
    if ((key.type == object_type::class_type || key.type == object_type::generic_definition) &&
        !type.record_key_language_supported) {
      throw exception::bad_member_type{input, "Generic record map keys require serializer version 1.11.0"};
    }
  }

  // Check concrete empty records or a forwarded parameter carrying the same proof.
  void validate_stateless_argument(const type_name& argument, const class_node* owner = nullptr) const {
    if (owner && !argument.application && !argument.resolved_node) {
      const auto parameter = std::find_if(owner->generic_parameters.begin(), owner->generic_parameters.end(),
          [&](const auto& value) { return value.name == argument.name; });
      if (parameter != owner->generic_parameters.end() && parameter->stateless) { return; }
    }
    if (!argument.resolved_node || argument.resolved_node->type != object_type::class_type ||
        !is_stateless_record(static_cast<const class_node&>(*argument.resolved_node))) {
      throw exception::bad_member_type{input,
          "Stateless argument requires a generated empty owning record or constrained type parameter: " +
          argument.name};
    }
  }

  // Bind exact-type leaves in declaration scope, including earlier generic parameters.
  void validate_constraint(generic_constraint& constraint, const class_node& owner) const {
    if (constraint.type) { validate_expression(*constraint.type, owner); }
    for (auto& operand : constraint.operands) { validate_constraint(operand, owner); }
  }

  // Identify type identities whose truth value must wait for a concrete specialization.
  bool dependent_constraint_type(const type_name& type, const class_node* owner) const {
    if (!owner) { return false; }
    if (std::find(owner->type_parameters.begin(), owner->type_parameters.end(), type.name) !=
        owner->type_parameters.end()) { return true; }
    return std::any_of(type.arguments.begin(), type.arguments.end(), [&](const auto& argument) {
      return dependent_constraint_type(argument, owner);
    });
  }

  // Evaluate Boolean constraints with three-valued logic while dependent arguments remain unknown.
  std::optional<bool> evaluate_constraint(const generic_constraint& constraint, const type_name& argument,
                                          const class_node* owner = nullptr) const {
    using operation = generic_constraint::operation;
    if (constraint.kind == operation::logical_not) {
      const auto result = evaluate_constraint(constraint.operands.at(0), argument, owner);
      return result ? std::optional<bool>{!*result} : std::nullopt;
    }
    if (constraint.kind == operation::logical_and || constraint.kind == operation::logical_or) {
      const auto left = evaluate_constraint(constraint.operands.at(0), argument, owner);
      const auto right = evaluate_constraint(constraint.operands.at(1), argument, owner);
      if (constraint.kind == operation::logical_and) {
        if ((left && !*left) || (right && !*right)) { return false; }
        return left && right ? std::optional<bool>{*left && *right} : std::nullopt;
      }
      if ((left && *left) || (right && *right)) { return true; }
      return left && right ? std::optional<bool>{*left || *right} : std::nullopt;
    }
    if (dependent_constraint_type(argument, owner)) {
      if (constraint.kind == operation::stateless) {
        const auto parameter = std::find_if(owner->generic_parameters.begin(), owner->generic_parameters.end(),
            [&](const auto& value) { return value.name == argument.name; });
        if (parameter != owner->generic_parameters.end() && parameter->stateless) { return true; }
      }
      return std::nullopt;
    }
    if (constraint.kind == operation::exact_type) {
      if (dependent_constraint_type(*constraint.type, owner)) { return std::nullopt; }
      return identity(argument) == identity(*constraint.type);
    }
    if (constraint.kind == operation::stateless) {
      return argument.resolved_node && argument.resolved_node->type == object_type::class_type &&
          is_stateless_record(static_cast<const class_node&>(*argument.resolved_node));
    }
    if (constraint.kind == operation::factory) {
      if (!argument.resolved_node || (argument.resolved_node->type != object_type::class_type &&
          argument.resolved_node->type != object_type::generic_definition)) { return false; }
      const auto& record = static_cast<const class_node&>(*argument.resolved_node);
      return record.storage_modes == static_cast<std::uint8_t>(storage_mode::owning) &&
          !record.supports_managed() && std::any_of(record.constructions.begin(), record.constructions.end(),
              [&](const auto& factory) { return !factory.constructor &&
                  factory.name == constraint.factory_name && factory.parameters.empty() &&
                  factory.access == access_type::public_access; });
    }
    if (argument.type != object_type::primitive || argument.application) { return false; }
    if (constraint.kind == operation::floating_point) {
      return argument.name == "float" || argument.name == "double";
    }
    return argument.name == "bool" || argument.name == "char" || argument.name == "int8" ||
        argument.name == "uint8" || argument.name == "int16" || argument.name == "uint16" ||
        argument.name == "int32" || argument.name == "uint32" || argument.name == "int64" ||
        argument.name == "uint64";
  }

  // Reject known failures while allowing category constraints to defer dependent forwarding.
  void validate_constraint_argument(const generic_constraint& constraint, const type_name& argument,
                                    const class_node* owner = nullptr) const {
    const auto result = evaluate_constraint(constraint, argument, owner);
    if (result && !*result) {
      throw exception::bad_member_type{input, "Generic argument does not satisfy its type constraint: " + argument.get_full_name()};
    }
  }

  // Substitute earlier arguments in exact-type leaves before checking a concrete application.
  void resolve_constraint(generic_constraint& constraint,
      const std::unordered_map<std::string, type_name>& bindings, std::size_t depth) {
    if (constraint.type) { resolve(*constraint.type, bindings, depth); }
    for (auto& operand : constraint.operands) { resolve_constraint(operand, bindings, depth); }
  }

  // Forward already bound argument identities into dependent exact-type leaves without instantiation.
  void substitute_constraint(generic_constraint& constraint,
      const std::unordered_map<std::string, type_name>& bindings) const {
    const auto substitute = [&](const auto& self, type_name& type) -> void {
      if (const auto found = bindings.find(type.name); found != bindings.end() && !type.application) {
        type = found->second;
      } else {
        for (auto& argument : type.arguments) { self(self, argument); }
      }
    };
    if (constraint.type) { substitute(substitute, *constraint.type); }
    for (auto& operand : constraint.operands) { substitute_constraint(operand, bindings); }
  }

  // Bind declaration-scope references and require proof for constrained forwarded arguments.
  void validate_expression(type_name& type, const class_node& owner) const {
    if (type.common_numeric) {
      throw exception::bad_member_type{input, "Common numeric policies cannot be stored types, generic arguments, constraints, or defaults"};
    }
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
    if (type.is_digest()) {
      if (type.application) {
        throw exception::bad_member_type{input, "Digest types do not accept generic arguments"};
      }
      type.declared_namespace = nullptr;
      return;
    }
    auto* node = find_declared_type(type.name, type.declared_namespace, visible);
    if (bind_builtin(type, node)) {
      if (type.is_expected()) {
        throw exception::bad_member_type{input, "Expected is supported only as a direct native C++ callable result"};
      }
      // Entity tokens do not own the target. Bind them after the complete declaration inventory.
      if (type.is_managed_reference()) { return; }
      for (auto& argument : type.arguments) {
        validate_expression(argument, owner);
      }
      validate_builtin_key(type, true);
      return;
    }
    const auto* object = node && node->type == object_type::class_type
                             ? static_cast<const class_node*>(node)
                             : nullptr;
    const auto arity = object ? object->type_parameters.size() : 0;
    if (object && (object->storage_modes != static_cast<std::uint8_t>(storage_mode::owning))) {
      throw exception::bad_member_type{input, "Generic fields and arguments require owning types"};
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
    std::unordered_map<std::string, type_name> constrained_arguments;
    for (std::size_t index = 0; index < type.arguments.size(); ++index) {
      auto& argument = type.arguments[index];
      if (object->generic_parameters[index].kind == generic_argument_kind::dimension) {
        bind_dimension(argument, placeholders(owner), true);
      } else {
        validate_expression(argument, owner);
        if (object->generic_parameters[index].stateless) {
          validate_stateless_argument(argument, &owner);
        }
        if (object->generic_parameters[index].constraint) {
          auto constraint = *object->generic_parameters[index].constraint;
          substitute_constraint(constraint, constrained_arguments);
          validate_constraint_argument(constraint, argument, &owner);
        }
      }
      constrained_arguments.emplace(object->generic_parameters[index].name, argument);
    }
    if (node) {
      type.name = node->get_full_name();
      type.declared_namespace = nullptr;
      type.resolved_node = node;
      type.type = node->type;
    } else {
      if (type.name == "digest" && !type.digest_language_supported) {
        throw exception::bad_member_type{input, "Digest types require serializer version 1.3.0 or newer"};
      }
      type.type = object_type::primitive;
      type.declared_namespace = nullptr;
    }
  }

  // Resolve arguments, enforce the finite owning profile, and share each concrete expansion.
  void resolve(type_name& type, const std::unordered_map<std::string, type_name>& bindings,
               std::size_t depth) {
    if (type.common_numeric) {
      for (auto& argument : type.arguments) {
        if (const auto found = bindings.find(argument.name); found != bindings.end() && !argument.application) {
          argument = found->second;
        }
      }
      return;
    }
    if (depth >= maximum_expansion_depth) {
      throw exception::bad_class{input, "Maximum generic expansion depth (32) exceeded"};
    }
    if (type.kind == generic_argument_kind::dimension) {
      throw exception::bad_member_type{input, "Type/value kind mismatch: expected type"};
    }
    if (type.type == object_type::primitive &&
        !type.is_optional() && !type.is_sequence() && !type.is_associative() &&
        !type.is_expected() && !type.is_managed_reference()) { return; }
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
    if (bind_builtin(type, node)) {
      if (type.is_expected()) {
        throw exception::bad_member_type{input, "Expected is supported only as a direct native C++ callable result"};
      }
      if (type.is_managed_reference()) {
        // Substitution is lexical; target layout and forward declarations remain deferred.
        const auto substitute_dimension = [&](const auto& self, dimension_expression& expression) -> void {
          if (expression.kind == dimension_expression::operation::parameter) {
            const auto found = bindings.find(expression.name);
            if (found != bindings.end() && found->second.kind == generic_argument_kind::dimension) {
              expression.kind = dimension_expression::operation::literal;
              expression.value = found->second.dimension;
              expression.name.clear();
            }
          }
          for (auto& operand : expression.operands) { self(self, operand); }
        };
        const auto substitute = [&](const auto& self, type_name& target) -> void {
          const auto found = bindings.find(target.name);
          if (found != bindings.end() && !target.application && target.expression.empty()) {
            target = found->second;
          }
          for (auto& expression : target.expression) { substitute_dimension(substitute_dimension, expression); }
          for (auto& argument : target.arguments) { self(self, argument); }
        };
        substitute(substitute, type.arguments.front());
        return;
      }
      for (auto& argument : type.arguments) {
        if (argument.common_numeric) {
          throw exception::bad_member_type{input, "Common numeric policies cannot be container arguments"};
        }
        resolve(argument, bindings, depth + 1);
        if (argument.resolved_node && argument.type == object_type::class_type) {
          const auto& object = static_cast<const class_node&>(*argument.resolved_node);
          if (incomplete.contains(object.get_full_name()) ||
              object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
            throw exception::bad_member_type{input, "Built-in generic arguments require finite owning types"};
          }
        }
      }
      validate_builtin_key(type, false);
      return;
    }
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
        if (argument.common_numeric) {
          throw exception::bad_member_type{input, "Common numeric policies cannot be record generic arguments"};
        }
        resolve(argument, supplied ? bindings : substitutions, depth + 1);
        if (parameter.stateless) { validate_stateless_argument(argument); }
        if (parameter.constraint) {
          auto constraint = *parameter.constraint;
          resolve_constraint(constraint, substitutions, depth + 1);
          validate_constraint_argument(constraint, argument);
        }
      }
      if (argument.resolved_node && argument.type == object_type::class_type) {
        const auto& value = static_cast<const class_node&>(*argument.resolved_node);
        if (incomplete.contains(value.get_full_name()) ||
            value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
          throw exception::bad_member_type{input,
                                           "Generic arguments require owning types"};
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
                                     definition->attributes, std::vector<parent>{definition->parents});
    if (!occupied.insert(concrete->name).second) {
      throw exception::bad_class{input, "Generated generic name collision: " + concrete->name};
    }
    concrete->generic_name = definition->get_full_name();
    concrete->source_path = definition->source_path;
    concrete->output_source_path = application_source_path;
    concrete->source_notices = definition->source_notices;
    concrete->generic_arguments = type.arguments;
    concrete->generic_parameters = definition->generic_parameters;
    concrete->member_list = definition->member_list;
    concrete->xml = definition->xml;
    concrete->explicit_class_id = definition->explicit_class_id;
    concrete->functions = definition->functions;
    concrete->constructions = definition->constructions;
    concrete->conversions = definition->conversions;
    concrete->arithmetic = definition->arithmetic;
    concrete->native_blocks = definition->native_blocks;
    concrete->body_order = definition->body_order;
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
    for (const auto& base : object.parents) {
      const auto bytes = minimum_storage(*base.parent_class, depth + 1);
      if (bytes > maximum - total) {
        throw exception::bad_class{input, "Inherited fixed storage exceeds backend representability"};
      }
      total += bytes;
    }
    for (const auto& field : object.member_list) {
      if (field.modifier != member::modifier_type::none && field.fixed_extent == 0) { continue; }
      const auto& type = field.type_name_list.front();
      std::uint64_t bytes = 1;
      if (type.is_digest() && type.digest_extent != 0) {
        bytes = type.digest_extent;
      } else if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
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

  // Resolve generated bases in declaration scope and preserve their occurrence wire identities.
  void resolve_parents(class_node& object,
                       const std::unordered_map<std::string, type_name>& bindings,
                       std::size_t depth) {
    std::unordered_set<const syntax_node*> bases{};
    for (auto& base : object.parents) {
      if (base.type_expression.empty()) {
        base.type_expression.emplace_back(std::string{base.name}, base.current_namespace);
      }
      auto& type = base.type_expression.front();
      resolve(type, bindings, depth);
      if (!type.resolved_node || type.type != object_type::class_type ||
          type.resolved_node == &object || !bases.insert(type.resolved_node).second) {
        throw exception::bad_class{input, "Parent must name a distinct previously declared class"};
      }
      base.parent_class = const_cast<class_node*>(static_cast<const class_node*>(type.resolved_node));
      if (!object.generic_name.empty() &&
          (base.parent_class->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
           (base.parent_class->attributes & class_attributes::packed) == class_attributes::packed)) {
        throw exception::bad_class{input, "Generic bases require unpacked owning generated classes"};
      }
      base.name = type.name;
      base.current_namespace = nullptr;
    }
  }

  // Build a lexical input scope without treating API record arguments as recursive field storage.
  class_node callable_scope(const class_node& owner,
      const std::vector<generic_parameter>& input_types) const {
    class_node scope{object_type::class_type, std::string{}, owner.parent_namespace,
        class_attributes::none, {}};
    scope.type_parameters = owner.type_parameters;
    scope.generic_parameters = owner.generic_parameters;
    for (const auto& parameter : input_types) {
      if (parameter.name == owner.name ||
          std::find(scope.type_parameters.begin(), scope.type_parameters.end(), parameter.name) !=
              scope.type_parameters.end()) {
        throw exception::bad_member_type{input, "Callable input types cannot shadow their owner or other input types"};
      }
      if (parameter.constraint) {
        auto constraint = *parameter.constraint;
        validate_constraint(constraint, scope);
      }
      scope.type_parameters.push_back(parameter.name);
      scope.generic_parameters.push_back(parameter);
    }
    return scope;
  }

  // Recognize dependent input leaves before concrete lowering attempts to instantiate their record.
  bool contains_callable_input(const type_name& type,
      const std::vector<generic_parameter>& input_types) const {
    if (!type.application && std::any_of(input_types.begin(), input_types.end(),
        [&](const auto& parameter) { return parameter.name == type.name; })) { return true; }
    return std::any_of(type.arguments.begin(), type.arguments.end(),
        [&](const auto& argument) { return contains_callable_input(argument, input_types); });
  }

  // Validate nested independent inputs against the same class and input constraints as C++ templates.
  void validate_callable_type(type_name& type, const class_node& owner,
      const std::vector<generic_parameter>& input_types, bool result = false) const {
    const auto scope = callable_scope(owner, input_types);
    if (bind_builtin(type, find_declared_type(type.name, type.declared_namespace, visible)) && type.is_expected()) {
      if (!result || type.arguments.back().name == "void") {
        throw exception::bad_member_type{input, "Expected requires a nonvoid error type and a direct native C++ callable result"};
      }
      for (auto& argument : type.arguments) {
        if (argument.name != "void") { validate_callable_type(argument, owner, input_types); }
      }
      return;
    }
    if (type.common_numeric) {
      for (auto& argument : type.arguments) { validate_expression(argument, scope); }
      return;
    }
    validate_expression(type, scope);
  }

  // Substitute class arguments while retaining method-dependent record applications for C++ deduction.
  void resolve_callable_type(type_name& type, const class_node& owner,
      const std::vector<generic_parameter>& input_types,
      const std::unordered_map<std::string, type_name>& bindings, std::size_t depth, bool result = false) {
    const auto substitute = [&](const auto& self, type_name& value) -> void {
      if (!value.application) {
        if (const auto found = bindings.find(value.name); found != bindings.end()) {
          value = found->second;
          return;
        }
      }
      for (auto& argument : value.arguments) { self(self, argument); }
    };
    substitute(substitute, type);
    if (bind_builtin(type, find_declared_type(type.name, type.declared_namespace, visible)) && type.is_expected()) {
      if (!result || type.arguments.back().name == "void") {
        throw exception::bad_member_type{input, "Expected requires a nonvoid error type and a direct native C++ callable result"};
      }
      for (auto& argument : type.arguments) {
        if (argument.name != "void") {
          resolve_callable_type(argument, owner, input_types, bindings, depth + 1);
        }
      }
      return;
    }
    // A callable may return/take its exact owner without introducing recursive inline storage.
    if (type.application && !owner.generic_name.empty() &&
        type.arguments.size() == owner.generic_arguments.size()) {
      const auto* declaration = find_declared_type(type.name, type.declared_namespace, visible);
      const bool same_owner = type.name == owner.generic_name ||
          (declaration && declaration->get_full_name() == owner.generic_name);
      if (same_owner && std::equal(type.arguments.begin(), type.arguments.end(),
          owner.generic_arguments.begin(), [&](const auto& left, const auto& right) {
            return identity(left) == identity(right);
          })) {
        bind(type, &owner);
        return;
      }
    }
    if (contains_callable_input(type, input_types)) {
      validate_callable_type(type, owner, input_types, result);
    } else {
      resolve(type, bindings, depth);
    }
  }

  // Validate open expression types in their class/input scope without inventing concrete bindings.
  void validate_callable_expression(behavior_expression& expression, const class_node& owner,
      const std::vector<generic_parameter>& input_types) const {
    if (expression.resolved_type) {
      validate_callable_type(*expression.resolved_type, owner, input_types);
    }
    for (auto& operand : expression.operands) {
      validate_callable_expression(operand, owner, input_types);
    }
  }

  // Rebind expression types in callable scope while retaining independent input leaves for C++.
  void resolve_callable_expression(behavior_expression& expression, const class_node& owner,
      const std::vector<generic_parameter>& input_types,
      const std::unordered_map<std::string, type_name>& bindings, std::size_t depth) {
    if (expression.resolved_type) {
      resolve_callable_type(*expression.resolved_type, owner, input_types, bindings, depth);
      if (expression.kind == behavior_expression::operation::checked_cast) {
        expression.value = expression.resolved_type->name;
      }
    }
    for (auto& operand : expression.operands) {
      resolve_callable_expression(operand, owner, input_types, bindings, depth);
    }
  }

  // Keep API array shapes exact while deferring only dimensions owned by an open class template.
  void resolve_callable_extent(const std::vector<dimension_expression>& expressions,
      std::uint64_t& fixed_extent, const class_node& owner,
      const std::unordered_map<std::string, type_name>& bindings, bool dependent) const {
    if (expressions.empty()) { return; }
    constexpr std::uint64_t maximum_fixed_elements = 65536;
    const auto extent = evaluate(expressions.front(), dependent ? placeholders(owner) : bindings, dependent);
    if ((!extent && !dependent) || (extent && (!*extent || *extent > maximum_fixed_elements))) {
      throw exception::bad_member_type{input, "Invalid or excessive callable fixed array extent (1..65536)"};
    }
    if (extent) { fixed_extent = *extent; }
  }

  // Substitute bases, fields and map keys without changing wire IDs, order, names or defaults.
  void resolve_fields(class_node& object,
                      const std::unordered_map<std::string, type_name>& bindings,
                      std::size_t depth) {
    resolve_parents(object, bindings, depth);
    if (object.magic_field) {
      auto& field = *object.magic_field;
      for (auto& type : field.type_name_list) {
        resolve(type, bindings, depth);
      }
      validate_magic_constant(input, field);
    }
    for (auto& field : object.member_list) {
      constexpr std::uint64_t maximum_fixed_elements = 65536;
      if (!field.extent_expression.empty()) {
        const auto extent = evaluate(field.extent_expression.front(), bindings);
        if (!extent || *extent == 0 || *extent > maximum_fixed_elements) {
          throw exception::bad_member_type{input, "Invalid or excessive fixed array extent (1..65536) at schema byte " +
              std::to_string(field.extent_expression.front().source_offset)};
        }
        if (object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
            (object.attributes & class_attributes::packed) == class_attributes::packed) {
          throw exception::bad_member_type{input, "Fixed arrays require unpacked owning fields"};
        }
        field.fixed_extent = *extent;
      }
      if (field.array_exposure) {
        // Named concrete owners copy this resolved metadata before discarding their source parameter scope.
        for (auto& dimension : field.array_exposure->dimensions) {
          const auto extent = evaluate(dimension, bindings);
          if (!extent || !*extent || *extent > maximum_fixed_elements) {
            throw exception::bad_member_type{input, "Invalid or unresolved exposed array dimension"};
          }
          dimension.kind = dimension_expression::operation::literal;
          dimension.value = *extent;
          dimension.name.clear();
          dimension.operands.clear();
        }
      }
      for (auto& type : field.type_name_list) {
        resolve(type, bindings, depth);
        if (!object.generic_name.empty() && type.resolved_node &&
            type.type == object_type::class_type) {
          const auto& nested = static_cast<const class_node&>(*type.resolved_node);
          if (nested.storage_modes != static_cast<std::uint8_t>(storage_mode::owning)) {
            throw exception::bad_member_type{input,
                                             "Generic fields require owning types"};
          }
        }
      }
      normalize_inferred_constants(input, field);
      validate_compact_member(input, field, false);
      validate_digest_member(input, object, field);
      validate_optional_member(input, object, field);
      if (field.modifier == member::modifier_type::map) {
        auto key = field.key_type.value_or(type_name{std::string{field.key}, field.type_name_list.front().declared_namespace});
        // Generic bodies were bound lexically at declaration, including qualified map keys.
        key.declared_namespace = object.generic_name.empty() ? object.parent_namespace : nullptr;
        resolve(key, bindings, depth);
        if (key.is_digest()) {
          throw exception::bad_member_type{input, "Digest types cannot be map keys"};
        }
        field.key = key.get_full_name();
        field.key_node = key.resolved_node;
        field.key_type = key;
      }
    }
    for (auto& method : object.functions) {
      if (method.guard) { callable_availability::bind(*method.guard, object, bindings); }
      if (method.return_type.name != "void") {
        resolve_callable_type(method.return_type, object, method.generic_parameters, bindings, depth, true);
      }
      resolve_callable_extent(method.return_extent_expression, method.return_fixed_extent, object, bindings, false);
      for (auto& parameter : method.parameters) {
        resolve_callable_type(parameter.type, object, method.generic_parameters, bindings, depth);
        resolve_callable_extent(parameter.extent_expression, parameter.fixed_extent, object, bindings, false);
      }
      if (method.expression) {
        resolve_callable_expression(*method.expression, object, method.generic_parameters, bindings, depth);
      }
    }
    for (auto& construction : object.constructions) {
      if (construction.guard) { callable_availability::bind(*construction.guard, object, bindings); }
      if (construction.constructor && construction.forward_signature_enabled) { continue; }
      for (auto& parameter : construction.parameters) {
        if (parameter.type.application && contains_callable_input(parameter.type, construction.generic_parameters) &&
            !construction.overload_contract_enabled) {
          throw exception::bad_member_type{input, "Nested construction input types require serializer version 1.12.0"};
        }
        resolve_callable_type(parameter.type, object, construction.generic_parameters, bindings, depth);
        if (!parameter.extent_expression.empty()) {
          constexpr std::uint64_t maximum_fixed_elements = 65536;
          const auto extent = evaluate(parameter.extent_expression.front(), bindings);
          if (!extent || *extent == 0 || *extent > maximum_fixed_elements) {
            throw exception::bad_member_type{input, "Construction array extent must be in 1..65536"};
          }
          parameter.fixed_extent = *extent;
        }
      }
      for (auto& assignment : construction.assignments) {
        for (auto& element : assignment.elements) {
          if (element.expression) {
            resolve_callable_expression(*element.expression, object, construction.generic_parameters, bindings, depth);
          }
        }
        if (assignment.expression) {
          resolve_callable_expression(*assignment.expression, object, construction.generic_parameters, bindings, depth);
        }
      }
    }
    for (auto& method : object.conversions) { resolve(method.target, bindings, depth); }
    conversion::validate(object);
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
      application_source_path = source_path;
      try {
        auto* raw = node.get();
        if (raw->type != object_type::function_type) { visible.emplace(raw->get_full_name(), raw); }
        if (raw->type == object_type::namespace_type) {
          auto children = std::move(static_cast<namespace_node*>(raw)->statements);
          emit(std::move(node));
          visit(std::move(children));
        } else if (raw->type == object_type::class_type) {
          auto& object = static_cast<class_node&>(*raw);
          if (!object.type_parameters.empty()) {
            if (object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
                (object.attributes & class_attributes::packed) == class_attributes::packed) {
              throw exception::bad_class{
                  input,
                  "Generic classes require unpacked owning fields"};
            }
            class_node earlier{object_type::class_type, std::string{object.name},
                               object.parent_namespace, class_attributes::none, {}};
            for (auto& parameter : object.generic_parameters) {
              try {
                if (parameter.constraint) { validate_constraint(*parameter.constraint, earlier); }
                if (!parameter.default_argument.empty()) {
                  auto& argument = parameter.default_argument.front();
                  if (parameter.kind == generic_argument_kind::dimension) {
                    bind_dimension(argument, placeholders(earlier), true);
                  } else {
                    validate_expression(argument, earlier);
                    if (parameter.stateless) { validate_stateless_argument(argument, &earlier); }
                    if (parameter.constraint) { validate_constraint_argument(*parameter.constraint, argument, &earlier); }
                  }
                }
              } catch (const std::exception& error) {
                throw exception::bad_class{input, "Invalid default for " + parameter.name + ": " + error.what()};
              }
              earlier.type_parameters.push_back(parameter.name);
              earlier.generic_parameters.push_back(parameter);
            }
            for (auto& base : object.parents) {
              if (base.type_expression.empty()) {
                base.type_expression.emplace_back(std::string{base.name}, base.current_namespace);
              }
              auto& type = base.type_expression.front();
              validate_expression(type, object);
              if (!type.resolved_node || type.type != object_type::class_type) {
                throw exception::bad_class{input, "Generic parent must name a generated class"};
              }
              base.parent_class = const_cast<class_node*>(static_cast<const class_node*>(type.resolved_node));
              if ((base.parent_class->attributes & class_attributes::packed) == class_attributes::packed) {
                throw exception::bad_class{input, "Generic bases require unpacked owning generated classes"};
              }
              base.name = type.name;
              base.current_namespace = nullptr;
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
              if (field.modifier == member::modifier_type::variant && !field.owning_variant) {
                throw exception::bad_class{input, "Generic unions are not supported"};
              }
              for (auto& type : field.type_name_list) {
                validate_expression(type, object);
              }
              validate_compact_member(input, field, true);
              validate_digest_member(input, object, field);
              validate_optional_member(input, object, field);
              validate_empty_layout(input, object, field);
              if (field.modifier == member::modifier_type::map) {
                auto key = field.key_type.value_or(type_name{std::string{field.key}, object.parent_namespace});
                validate_expression(key, object);
                if (key.is_digest()) {
                  throw exception::bad_member_type{input, "Digest types cannot be map keys"};
                }
                field.key = key.name;
                field.key_node = key.resolved_node;
                field.key_type = key;
              }
            }
            for (auto& method : object.functions) {
              if (method.guard) { callable_availability::bind(*method.guard, object); }
              if (method.return_type.name != "void") {
                validate_callable_type(method.return_type, object, method.generic_parameters, true);
              }
              resolve_callable_extent(method.return_extent_expression, method.return_fixed_extent, object, {}, true);
              for (auto& parameter : method.parameters) {
                validate_callable_type(parameter.type, object, method.generic_parameters);
                resolve_callable_extent(parameter.extent_expression, parameter.fixed_extent, object, {}, true);
              }
              // Explicit expression type applications share callable scope, including the owner.
              if (method.expression) {
                validate_callable_expression(*method.expression, object, method.generic_parameters);
              }
            }
            for (auto& construction : object.constructions) {
              if (construction.guard) { callable_availability::bind(*construction.guard, object); }
              if (construction.constructor && construction.forward_signature_enabled) { continue; }
              for (auto& parameter : construction.parameters) {
                if (parameter.type.application && contains_callable_input(parameter.type, construction.generic_parameters) &&
                    !construction.overload_contract_enabled) {
                  throw exception::bad_member_type{input, "Nested construction input types require serializer version 1.12.0"};
                }
                validate_callable_type(parameter.type, object, construction.generic_parameters);
              }
            }
            for (auto& method : object.conversions) { validate_expression(method.target, object); }
            conversion::validate(object);
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
            if (!target.resolved_node || target.type != object_type::class_type) {
              throw exception::bad_class{input, "instantiate requires a generated generic class"};
            }
            const auto& source = static_cast<const class_node&>(*target.resolved_node);
            object.attributes = source.attributes;
            object.parents = source.parents;
            object.member_list = source.member_list;
            object.xml = source.xml;
            object.explicit_class_id = source.explicit_class_id;
            object.functions = source.functions;
            object.constructions = source.constructions;
            object.conversions = source.conversions;
            object.arithmetic = source.arithmetic;
            object.native_blocks = source.native_blocks;
            object.body_order = source.body_order;
            object.magic_bytes = source.magic_bytes;
            object.magic_field = source.magic_field;
            object.magic_access = source.magic_access;
            object.magic_id = source.magic_id;
            object.magic_explicit_id = source.magic_explicit_id;
            object.magic_omitted_formats = source.magic_omitted_formats;
            object.reserved_ids = source.reserved_ids;
            object.reserved_variables = source.reserved_variables;
            object.reserved_names = source.reserved_names;
            object.named_generic_source = source.generic_name;
            object.named_generic_arguments = source.generic_arguments;
            auto& bindings = named_bindings[&object];
            for (std::size_t index{}; index < source.generic_arguments.size(); ++index) {
              bindings.emplace(source.generic_parameters[index].name, source.generic_arguments[index]);
            }
            object.instance_of.clear();
          } else {
            incomplete.insert(object.get_full_name());
            resolve_fields(object, {}, 0);
            incomplete.erase(object.get_full_name());
          }
          emit(std::move(node));
        } else if (raw->type == object_type::function_type) {
          auto& function = static_cast<function_node&>(*raw).function;
          class_node scope{object_type::class_type, std::string{}, raw->parent_namespace, class_attributes::none, {}};
          if (function.return_type.name != "void") {
            resolve_callable_type(function.return_type, scope, function.generic_parameters, {}, 0, true);
          }
          resolve_callable_extent(function.return_extent_expression, function.return_fixed_extent, scope, {}, false);
          for (auto& parameter : function.parameters) {
            resolve_callable_type(parameter.type, scope, function.generic_parameters, {}, 0);
            resolve_callable_extent(parameter.extent_expression, parameter.fixed_extent, scope, {}, false);
          }
          if (function.expression) {
            resolve_callable_expression(*function.expression, scope, function.generic_parameters, {}, 0);
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

  // Resolve callable-only constructor dependencies after finite layouts and declarations are known.
  void resolve_deferred_constructors(syntax_node& node) {
    if (node.type == object_type::namespace_type) {
      for (auto& child : static_cast<namespace_node&>(node).statements) {
        resolve_deferred_constructors(*child);
      }
      return;
    }
    if (node.type != object_type::class_type) { return; }
    auto& owner = static_cast<class_node&>(node);
    application_source_path = owner.output_source_path.empty()
        ? owner.source_path : owner.output_source_path;
    auto bindings = named_bindings.contains(&owner) ? named_bindings.at(&owner)
        : std::unordered_map<std::string, type_name>{};
    for (std::size_t argument{}; argument < owner.generic_arguments.size(); ++argument) {
      bindings.emplace(owner.generic_parameters[argument].name, owner.generic_arguments[argument]);
    }
    for (auto& construction : owner.constructions) {
      if (construction.guard) { callable_availability::bind(*construction.guard, owner, bindings); }
      if (!construction.constructor || !construction.forward_signature_enabled) { continue; }
      for (auto& parameter : construction.parameters) {
        if (!owner.type_parameters.empty()) {
          validate_callable_type(parameter.type, owner, construction.generic_parameters);
        } else {
          resolve_callable_type(parameter.type, owner, construction.generic_parameters, bindings, 0);
        }
        if (!parameter.extent_expression.empty()) {
          constexpr std::uint64_t maximum_fixed_elements = 65536;
          const auto extent = evaluate(parameter.extent_expression.front(),
              owner.type_parameters.empty() ? bindings : placeholders(owner),
              !owner.type_parameters.empty());
          if (extent && (*extent == 0 || *extent > maximum_fixed_elements)) {
            throw exception::bad_member_type{input, "Construction array extent must be in 1..65536"};
          }
          if (extent) { parameter.fixed_extent = *extent; }
        }
      }
      for (auto& assignment : construction.assignments) {
        for (auto& element : assignment.elements) {
          if (!element.expression) { continue; }
          if (!owner.type_parameters.empty()) {
            validate_callable_expression(*element.expression, owner, construction.generic_parameters);
          } else {
            resolve_callable_expression(*element.expression, owner, construction.generic_parameters, bindings, 0);
          }
        }
        if (assignment.expression) {
          if (!owner.type_parameters.empty()) {
            // Deferred open constructors retain T just like their signatures and method bodies.
            validate_callable_expression(*assignment.expression, owner, construction.generic_parameters);
          } else {
            resolve_callable_expression(*assignment.expression, owner,
                construction.generic_parameters, bindings, 0);
          }
        }
      }
    }
  }

  // Resolve phantom entity edges only after finite ownership and all declarations are known.
  void resolve_managed_references(syntax_node& node) {
    if (node.type == object_type::namespace_type) {
      for (auto& child : static_cast<namespace_node&>(node).statements) {
        resolve_managed_references(*child);
      }
      return;
    }
    if (node.type != object_type::class_type && node.type != object_type::function_type) { return; }
    const auto bind_tokens = [&](const auto& self, type_name& type,
        const std::vector<generic_parameter>& parameters) -> void {
      if (type.is_managed_reference()) {
        auto& target = type.arguments.front();
        const auto dependent_dimension = [&](const auto& inspect, const dimension_expression& expression) -> bool {
          return (expression.kind == dimension_expression::operation::parameter &&
              std::any_of(parameters.begin(), parameters.end(),
                  [&](const auto& parameter) { return parameter.name == expression.name; })) ||
              std::any_of(expression.operands.begin(), expression.operands.end(),
                  [&](const auto& operand) { return inspect(inspect, operand); });
        };
        const auto dependent = [&](const auto& inspect, const type_name& value) -> bool {
          return (!value.application && !value.resolved_node && value.type == object_type::unresolved &&
              std::any_of(parameters.begin(), parameters.end(),
              [&](const auto& parameter) { return parameter.name == value.name; })) ||
              std::any_of(value.expression.begin(), value.expression.end(),
                  [&](const auto& expression) { return dependent_dimension(dependent_dimension, expression); }) ||
              std::any_of(value.arguments.begin(), value.arguments.end(),
                  [&](const auto& argument) { return inspect(inspect, argument); });
        };
        // Retained open C++ templates are checked through each concrete specialization.
        if (dependent(dependent, target)) { return; }
        resolve(target, {}, 0);
        if (!target.resolved_node || target.type != object_type::class_type) {
          throw exception::bad_member_type{input, "managed_ref requires a concrete managed class target"};
        }
        const auto& object = static_cast<const class_node&>(*target.resolved_node);
        if (!object.type_parameters.empty() || !object.supports_managed() ||
            object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
            (object.attributes & class_attributes::packed) != class_attributes::none) {
          throw exception::bad_member_type{input, "managed_ref requires a concrete unpacked owning managed class target"};
        }
        return;
      }
      for (auto& argument : type.arguments) { self(self, argument, parameters); }
    };
    const auto bind_callable = [&](function_declaration& function,
        const std::vector<generic_parameter>& owner_parameters) {
      auto parameters = owner_parameters;
      parameters.insert(parameters.end(), function.generic_parameters.begin(), function.generic_parameters.end());
      bind_tokens(bind_tokens, function.return_type, parameters);
      for (auto& parameter : function.parameters) { bind_tokens(bind_tokens, parameter.type, parameters); }
    };
    if (node.type == object_type::function_type) {
      bind_callable(static_cast<function_node&>(node).function, {});
      return;
    }
    auto& owner = static_cast<class_node&>(node);
    for (auto& field : owner.member_list) {
      for (auto& type : field.type_name_list) {
        if (type.contains_managed_reference() &&
            (owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
             (owner.attributes & class_attributes::packed) != class_attributes::none ||
             field.managed || !field.default_value.empty() || !field.default_factory.empty())) {
          throw exception::bad_member_type{input,
              "managed_ref fields require ordinary unpacked owning storage and no schema value default"};
        }
        bind_tokens(bind_tokens, type, owner.generic_parameters);
      }
      if (field.key_type) { bind_tokens(bind_tokens, *field.key_type, owner.generic_parameters); }
    }
    for (auto& function : owner.functions) { bind_callable(function, owner.generic_parameters); }
    for (auto& construction : owner.constructions) {
      auto parameters = owner.generic_parameters;
      parameters.insert(parameters.end(), construction.generic_parameters.begin(), construction.generic_parameters.end());
      for (auto& parameter : construction.parameters) { bind_tokens(bind_tokens, parameter.type, parameters); }
    }
  }

public:
  // Keep diagnostics associated with the compilation input; all caches are per compilation.
  explicit generic_lowering(const Input& source) : input{source} {}

  // Return concrete contracts plus validated generic declarations for native C++ emission.
  std::vector<std::unique_ptr<syntax_node>> run(std::vector<std::unique_ptr<syntax_node>> nodes) {
    collect(nodes);
    visit(std::move(nodes));
    // Callable dependencies do not own storage. Resolve them only after every finite field
    // layout and template declaration is known, allowing reciprocal constructor signatures.
    // New concrete signature types may append output; their fields are checked before reuse.
    for (std::size_t index{}; index < output.size(); ++index) {
      resolve_deferred_constructors(*output[index]);
      resolve_managed_references(*output[index]);
    }
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
    } else if (node->type == object_type::function_type) {
      const auto& function = static_cast<const function_node&>(*node).function;
      if (!function.generic_parameters.empty()) { return true; }
      if (function.return_type.application) { return true; }
      if (!function.return_extent_expression.empty()) { return true; }
      for (const auto& parameter : function.parameters) {
        if (parameter.type.application || !parameter.extent_expression.empty()) { return true; }
      }
    } else if (node->type == object_type::class_type) {
      const auto& object = static_cast<const class_node&>(*node);
      if (!object.type_parameters.empty() || !object.instance_of.empty()) {
        return true;
      }
      for (const auto& base : object.parents) {
        if (!base.type_expression.empty() && base.type_expression.front().application) { return true; }
      }
      for (const auto& field : object.member_list) {
        if (!field.extent_expression.empty()) { return true; }
        for (const auto& type : field.type_name_list) {
          if (type.application) {
            return true;
          }
        }
      }
      for (const auto& method : object.functions) {
        if (!method.generic_parameters.empty()) { return true; }
        if (method.return_type.application) { return true; }
        if (!method.return_extent_expression.empty()) { return true; }
        for (const auto& parameter : method.parameters) {
          if (parameter.type.application || !parameter.extent_expression.empty()) { return true; }
        }
      }
      for (const auto& construction : object.constructions) {
        if (!construction.generic_parameters.empty()) { return true; }
        for (const auto& parameter : construction.parameters) {
          if (!parameter.extent_expression.empty()) { return true; }
          if (parameter.type.application) { return true; }
        }
      }
      for (const auto& method : object.conversions) {
        if (method.target.application) { return true; }
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
