// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::comparison {

using configuration = std::map<std::string, class_comparison_options>;

// Keep source-order bases and fields separate from each backend's native access spelling.
struct selection {
  std::vector<const parent*> bases{};
  std::vector<const member*> fields{};
};

// Bind concrete settings first, then inherit settings from a named instance's generic source.
inline const class_comparison_options* find(const class_node& owner,
                                           const configuration& options) {
  if (const auto found = options.find(owner.get_full_name()); found != options.end()) {
    return &found->second;
  }
  if (!owner.generic_name.empty()) {
    if (const auto found = options.find(owner.generic_name); found != options.end()) {
      return &found->second;
    }
  }
  if (!owner.named_generic_source.empty()) {
    if (const auto found = options.find(owner.named_generic_source); found != options.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

// Select durable authored values by default; explicit lists select only the named own fields.
inline selection parts(const class_node& owner, const class_comparison_options& options) {
  selection result;
  if (options.fields.empty()) {
    for (const auto& base : owner.parents) { result.bases.push_back(&base); }
    for (const auto& field : owner.member_list) {
      if (!field.transient && !field.fixed_name && !field.wire_only) { result.fields.push_back(&field); }
    }
    return result;
  }
  std::set<std::string> seen;
  for (const auto& name : options.fields) {
    const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(),
        [&](const member& field) { return field.name == name && !field.fixed_name && !field.wire_only; });
    if (found == owner.member_list.end() || !seen.insert(name).second) {
      throw std::invalid_argument{"Unknown or repeated comparison field: " +
                                  owner.get_full_name() + "::" + name};
    }
    result.fields.push_back(&*found);
  }
  return result;
}

// Compare API spellings after the supported profiles' underscore and case transformations.
inline std::string api_spelling(std::string_view value) {
  std::string result;
  for (auto character : value) {
    if (character == '_') { continue; }
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character + ('a' - 'A'));
    }
    result += character;
  }
  return result;
}

// Reject portable API collisions rather than replacing an authored method or field.
inline void validate_api_names(const class_node& owner, const class_comparison_options& options) {
  const auto collision = [&](std::string_view name) {
    const auto spelling = api_spelling(name);
    return (options.equality && spelling == "equalsvalue") ||
           (options.ordering && spelling == "comparevalue");
  };
  for (const auto& field : owner.member_list) {
    if (collision(field.name)) {
      throw std::invalid_argument{"Generated comparison API collision: " + field.name};
    }
  }
  for (const auto& method : owner.functions) {
    if (collision(method.name)) {
      throw std::invalid_argument{"Generated comparison API collision: " + method.name};
    }
  }
  for (const auto& factory : owner.constructions) {
    if (collision(factory.name)) {
      throw std::invalid_argument{"Generated comparison API collision: " + factory.name};
    }
  }
  for (const auto& method : owner.conversions) {
    if (collision(method.name)) {
      throw std::invalid_argument{"Generated comparison API collision: " + method.name};
    }
  }
}

// Require nested records to expose the same opted-in portable comparison operations.
inline void validate_dependency(const class_node& owner, const class_node& dependency,
    const class_comparison_options& options, const configuration& settings) {
  const auto* nested = find(dependency, settings);
  if (!nested || (options.equality && !nested->equality) ||
      (options.ordering && !nested->ordering)) {
    throw std::invalid_argument{"Comparison for " + owner.get_full_name() +
        " requires matching comparison configuration for " + dependency.get_full_name()};
  }
}

// Validate the portable scalar/record profile without silently comparing native container identity.
inline void validate_portable_field(const class_node& owner, const member& field,
    const class_comparison_options& options, const configuration& settings) {
  if (field.modifier != member::modifier_type::none || field.type_name_list.size() != 1) {
    throw std::invalid_argument{"Portable comparison requires scalar or generated record fields: " +
                                owner.get_full_name() + "::" + field.name};
  }
  const auto& type = field.type_name_list.front();
  if (type.resolved_node && (type.resolved_node->type == object_type::class_type ||
                            type.resolved_node->type == object_type::generic_definition)) {
    validate_dependency(owner, static_cast<const class_node&>(*type.resolved_node), options, settings);
    return;
  }
  if (type.type == object_type::enum_type) { return; }
  if (owner.type == object_type::generic_definition &&
      std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) !=
          owner.type_parameters.end()) { return; }
  constexpr std::string_view scalar_names[]{"bool", "char", "int8", "uint8", "int16", "uint16",
      "int32", "uint32", "int64", "uint64", "float", "double", "string"};
  if (type.type != object_type::primitive || type.application ||
      std::find(std::begin(scalar_names), std::end(scalar_names), type.name) ==
          std::end(scalar_names)) {
    throw std::invalid_argument{"Unsupported portable comparison type: " + type.get_full_name()};
  }
}

// Validate one selected owning record before emitting any comparison declarations.
inline void validate_owner(const class_node& owner, const class_comparison_options& options,
                           const configuration& settings, bool portable) {
  const auto selected = parts(owner, options);
  if (!options.equality && !options.ordering) { return; }
  if (owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
      (owner.attributes & class_attributes::packed) != class_attributes::none) {
    throw std::invalid_argument{"Generated comparison requires an unpacked owning record: " +
                                owner.get_full_name()};
  }
  for (const auto* field : selected.fields) {
    if (field->bit_width != 0) {
      throw std::invalid_argument{"Generated comparison does not support bit-field addresses: " +
                                  owner.get_full_name() + "::" + field->name};
    }
    if (field->modifier == member::modifier_type::variant) {
      throw std::invalid_argument{"Generated comparison does not support union/variant fields: " +
                                  owner.get_full_name() + "::" + field->name};
    }
    if (portable) { validate_portable_field(owner, *field, options, settings); }
  }
  if (portable) {
    validate_api_names(owner, options);
    for (const auto* base : selected.bases) {
      if (!base->parent_class) { throw std::invalid_argument{"Unresolved comparison base"}; }
      validate_dependency(owner, *base->parent_class, options, settings);
    }
  }
}

// Gather source-qualified declarations, including generic sources and their concrete instances.
inline void collect(const std::vector<std::unique_ptr<syntax_node>>& statements,
                    std::map<std::string, const class_node*>& owners) {
  for (const auto& statement : statements) {
    if (statement->type == object_type::namespace_type) {
      collect(static_cast<const namespace_node&>(*statement).statements, owners);
    } else if (statement->type == object_type::class_type ||
               statement->type == object_type::generic_definition) {
      const auto& owner = static_cast<const class_node&>(*statement);
      owners.emplace(owner.get_full_name(), &owner);
    }
  }
}

// Reject unknown selectors and validate inherited generic settings in every concrete instance.
inline void validate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     const configuration& settings, bool portable = false) {
  if (settings.empty()) { return; }
  std::map<std::string, const class_node*> owners;
  collect(statements, owners);
  for (const auto& [name, options] : settings) {
    const auto found = owners.find(name);
    if (found == owners.end()) {
      throw std::invalid_argument{"Unknown comparison class: " + name};
    }
    validate_owner(*found->second, options, settings, portable);
  }
  for (const auto& [name, owner] : owners) {
    static_cast<void>(name);
    if (const auto* options = find(*owner, settings)) {
      validate_owner(*owner, *options, settings, portable);
    }
  }
}

} // namespace rohit::serializer::writer::comparison
