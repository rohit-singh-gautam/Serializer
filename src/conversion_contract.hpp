// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace rohit::serializer::conversion {

// Substitute a generic record's arguments into a projected member type.
inline type_name bound_type(type_name value, const type_name& record) {
  if (record.resolved_node) {
    const auto& owner = static_cast<const class_node&>(*record.resolved_node);
    for (std::size_t index = 0; index < owner.type_parameters.size(); ++index) {
      if (value.name == owner.type_parameters[index] && index < record.arguments.size()) {
        return record.arguments[index];
      }
    }
  }
  for (auto& argument : value.arguments) { argument = bound_type(std::move(argument), record); }
  return value;
}

// Resolve an owning generated record, rejecting representations with hidden mutable ownership.
inline const class_node& record(const type_name& value) {
  if (!value.resolved_node || (value.resolved_node->type != object_type::class_type &&
      value.resolved_node->type != object_type::generic_definition)) {
    throw std::invalid_argument{"Conversion target/projection requires a generated record"};
  }
  const auto& owner = static_cast<const class_node&>(*value.resolved_node);
  if (owner.reference || owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
      (owner.attributes & class_attributes::packed) == class_attributes::packed ||
      owner.supports_managed()) {
    throw std::invalid_argument{"Conversions require ordinary owning records"};
  }
  return owner;
}

// Preserve dependent base arguments while projecting or copying a generic inherited value.
inline type_name base_type(const parent& base, const type_name& owner) {
  auto result = base.type_expression.empty() ? type_name{std::string{base.name}, nullptr} :
      bound_type(base.type_expression.front(), owner);
  result.resolved_node = base.parent_class;
  result.type = base.parent_class->type;
  return result;
}

// Find an unambiguous public field, retaining composed-base navigation for portable targets.
inline const member& find_field(const class_node& owner, const std::string& name,
    std::vector<std::size_t>& path, std::size_t depth = 0) {
  if (depth >= 32) { throw std::invalid_argument{"Conversion projection depth exceeds 32"}; }
  const member* found{};
  for (const auto& field : owner.member_list) {
    if (field.wire_only) { continue; }
    if (field.name != name) { continue; }
    if (field.access != access_type::public_access) {
      throw std::invalid_argument{"Conversion projections require public fields: " + name};
    }
    return field;
  }
  std::vector<std::size_t> selected;
  for (std::size_t index = 0; index < owner.parents.size(); ++index) {
    const auto& base = owner.parents[index];
    if (base.access != access_type::public_access || !base.parent_class) { continue; }
    std::vector<std::size_t> nested;
    // A missing name is distinct from an inaccessible or ambiguous matching declaration.
    const auto contains = [&](const auto& self, const class_node& node) -> bool {
      if (std::any_of(node.member_list.begin(), node.member_list.end(),
          [&](const auto& field) { return field.name == name && !field.wire_only; })) { return true; }
      return std::any_of(node.parents.begin(), node.parents.end(), [&](const auto& parent) {
        return parent.parent_class && self(self, *parent.parent_class);
      });
    };
    if (!contains(contains, *base.parent_class)) { continue; }
    const auto& candidate = find_field(*base.parent_class, name, nested, depth + 1);
    if (found) { throw std::invalid_argument{"Ambiguous conversion source field: " + name}; }
    found = &candidate;
    selected = {index};
    selected.insert(selected.end(), nested.begin(), nested.end());
  }
  if (!found) { throw std::invalid_argument{"Unknown conversion source field: " + name}; }
  path = std::move(selected);
  return *found;
}

// Qualify independent value copying; unsupported owned shapes fail before generation.
inline void validate_copy(const type_name& value, std::set<const class_node*>& active,
    std::size_t depth = 0) {
  if (depth >= 32 || value.is_optional() || value.is_sequence() || value.is_associative() ||
      value.is_digest()) {
    throw std::invalid_argument{"Conversion copying supports scalars, strings, enums and ordinary records"};
  }
  if (!value.resolved_node || value.type == object_type::primitive || value.type == object_type::enum_type) {
    return; // Open type parameters defer the copy requirement to their concrete application.
  }
  const auto& owner = record(value);
  if (!owner.native_blocks.empty() || !active.insert(&owner).second ||
      std::any_of(owner.functions.begin(), owner.functions.end(), [](const auto& function) {
        return function.dispatch != function_declaration::dispatch_type::ordinary;
      })) {
    throw std::invalid_argument{"Conversion copying rejects opaque, polymorphic or recursive records"};
  }
  for (const auto& base : owner.parents) {
    if (base.access != access_type::public_access || !base.parent_class) {
      throw std::invalid_argument{"Conversion copying requires public generated bases"};
    }
    validate_copy(base_type(base, value), active, depth + 1);
  }
  for (const auto& field : owner.member_list) {
    if (field.wire_only) { continue; }
    if (field.access != access_type::public_access || field.modifier != member::modifier_type::none ||
        field.type_name_list.size() != 1 || field.managed) {
      throw std::invalid_argument{"Conversion record copies require public scalar/record fields"};
    }
    validate_copy(bound_type(field.type_name_list.front(), value), active, depth + 1);
  }
  active.erase(&owner);
}

// Validate a borrowed payload without imposing the independent conversion's copy restrictions.
inline void validate_borrow(const type_name& value, std::set<const class_node*>& active,
    std::size_t depth = 0) {
  if (depth >= 32) { throw std::invalid_argument{"Borrowed value nesting exceeds 32 levels"}; }
  if (value.is_optional() || value.is_sequence() || value.is_associative()) {
    for (const auto& argument : value.arguments) { validate_borrow(argument, active, depth + 1); }
    return;
  }
  if (!value.resolved_node || value.type == object_type::primitive || value.type == object_type::enum_type) { return; }
  const auto& owner = record(value);
  if (owner.reference || !owner.native_blocks.empty() || !active.insert(&owner).second) {
    throw std::invalid_argument{"Borrowed values require nonrecursive ordinary owning records"};
  }
  for (const auto& base : owner.parents) {
    if (base.access != access_type::public_access || !base.parent_class) {
      throw std::invalid_argument{"Borrowed values require public generated bases"};
    }
    validate_borrow(base_type(base, value), active, depth + 1);
  }
  for (const auto& field : owner.member_list) {
    if (field.wire_only) { continue; }
    if (field.access != access_type::public_access || field.managed ||
        field.modifier == member::modifier_type::variant) {
      throw std::invalid_argument{"Borrowed records require public ordinary fields"};
    }
    for (const auto& item : field.type_name_list) { validate_borrow(bound_type(item, value), active, depth + 1); }
  }
  active.erase(&owner);
}

// Reject target authoring effects that leaf-default projections cannot reproduce without an owner.
inline void validate_translator_defaults(const type_name& value, std::set<const class_node*>& visited) {
  if (!value.resolved_node || (value.type != object_type::class_type && value.type != object_type::generic_definition)) { return; }
  const auto& owner = record(value);
  if (!visited.insert(&owner).second) { return; }
  for (const auto& construction : owner.constructions) {
    const bool callable_without_inputs = construction.parameters.empty() || std::all_of(construction.parameters.begin(), construction.parameters.end(), [](const auto& parameter) {
      return !parameter.default_value.empty() || !parameter.default_factory.empty() || parameter.initializer.has_value();
    });
    if (construction.constructor && callable_without_inputs && !construction.defaulted) {
      throw std::invalid_argument{"Translators cannot reproduce authored zero-argument target construction; use an owning conversion"};
    }
  }
  for (const auto& base : owner.parents) { validate_translator_defaults(base_type(base, value), visited); }
  for (const auto& field : owner.member_list) {
    if (!field.default_factory.empty()) {
      throw std::invalid_argument{"Translators require literal field defaults; owning field factory effects require an owning conversion"};
    }
    if (!field.wire_only) { validate_translator_defaults(bound_type(field.type_name_list.front(), value), visited); }
  }
}

// Bind exact source/target identities after normal type resolution and generic substitution.
inline void validate(class_node& owner) {
  if (owner.conversions.empty()) { return; }
  if (owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) || owner.supports_managed() ||
      (owner.attributes & class_attributes::packed) == class_attributes::packed) {
    throw std::invalid_argument{"Conversions require an ordinary owning source record"};
  }
  std::set<std::string> names;
  for (const auto& field : owner.member_list) { names.insert(field.name); }
  for (const auto& method : owner.functions) { names.insert(method.name); }
  for (const auto& factory : owner.constructions) { names.insert(factory.constructor ? "create" : factory.name); }
  for (auto& method : owner.conversions) {
    if (!names.insert(method.name).second) { throw std::invalid_argument{"Conversion name collision: " + method.name}; }
    const auto& target = record(method.target);
    if (&target == &owner || !target.native_blocks.empty()) {
      throw std::invalid_argument{"Conversion targets must be previously declared nonopaque owning records"};
    }
    if (method.translator) {
      // Every getter must expose an inspectable value; defaults never hide unsupported ownership.
      std::set<const class_node*> active;
      if (method.collection_projection) { validate_borrow(method.target, active); }
      else { validate_copy(method.target, active); }
      std::set<const class_node*> visited;
      validate_translator_defaults(method.target, visited);
    }
    for (auto& assignment : method.assignments) {
      const auto destination = std::find_if(target.member_list.begin(), target.member_list.end(),
          [&](const auto& field) { return field.name == assignment.field; });
      if (destination == target.member_list.end() || destination->access != access_type::public_access ||
          destination->modifier != member::modifier_type::none || destination->managed || destination->wire_only) {
        throw std::invalid_argument{"Conversion targets require public own scalar/record fields: " + assignment.field};
      }
      type_name source{std::string{owner.get_full_name()}, nullptr};
      source.resolved_node = &owner; source.type = owner.type;
      assignment.source_base_paths.clear();
      for (std::size_t index = 0; index < assignment.source_path.size(); ++index) {
        std::vector<std::size_t> path;
        const auto& field = find_field(record(source), assignment.source_path[index], path);
        if (field.modifier != member::modifier_type::none || field.managed) {
          throw std::invalid_argument{"Conversion projections cannot traverse collections or managed fields"};
        }
        auto projected_owner = source;
        for (const auto base : path) {
          projected_owner = base_type(record(projected_owner).parents[base], projected_owner);
        }
        source = bound_type(field.type_name_list.front(), projected_owner);
        assignment.source_base_paths.push_back(std::move(path));
      }
      const auto expected = bound_type(destination->type_name_list.front(), method.target);
      if (source != expected) { throw std::invalid_argument{"Conversion field types must match exactly: " + assignment.field}; }
      std::set<const class_node*> active;
      if (method.translator && method.collection_projection) { validate_borrow(source, active); }
      else { validate_copy(source, active); }
    }
  }
}

} // namespace rohit::serializer::conversion
