// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <rohit/binary_output_support.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::auto_size {

using configuration = std::map<std::string, class_auto_size_options>;

// Select the exact owner's setting before falling back to its authored generic source.
inline const class_auto_size_options* find(const class_node& owner, const configuration& settings) {
  for (const auto& name : {owner.get_full_name(), owner.generic_name, owner.named_generic_source}) {
    if (name.empty()) { continue; }
    const auto found = settings.find(name);
    if (found != settings.end()) { return &found->second; }
  }
  return nullptr;
}

// Default decoding verifies the declaration against the actual independently bounded record span.
inline bool verifies(const class_node& owner, const configuration& settings) {
  const auto* selected = find(owner, settings);
  return !selected || selected->validation == auto_size_validation::verify;
}

// Return each size-bearing own field; inherited records retain their independent byte spans.
inline std::vector<const member*> fields(const class_node& owner) {
  std::vector<const member*> result{};
  for (const auto& value : owner.member_list) {
    if (!value.auto_size_formats.empty()) { result.push_back(&value); }
  }
  return result;
}

// Return the single active field for this containing record and canonical native protocol.
inline const member* field(const class_node& owner, std::string_view format) {
  const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(),
      [&](const auto& value) { return value.auto_sizes(format); });
  return found == owner.member_list.end() ? nullptr : &*found;
}

// Count the selected field's entire native key and fixed scalar without counting outer record framing.
inline std::size_t field_bytes(const member& value, std::string_view format) {
  const auto scalar_bytes = value.type_name_list.front().name == "uint32" ? sizeof(std::uint32_t) : sizeof(std::uint64_t);
  if (format == "binary_integer") { return scalar_bytes + detail::fixed_prefix_bytes(value.id); }
  if (format == "binary_string") {
    return scalar_bytes + detail::fixed_prefix_bytes(static_cast<std::uint32_t>(value.display_name.size())) + value.display_name.size();
  }
  return scalar_bytes;
}

// Refuse unsupported layouts and ambiguous/omitted declarations before emitting any codec.
inline void validate_owner(const class_node& owner) {
  const auto selected = fields(owner);
  if (selected.empty()) { return; }
  if (owner.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
      owner.supports_managed() ||
      (owner.attributes & class_attributes::packed) == class_attributes::packed) {
    throw std::invalid_argument{"auto_size requires an unpacked unmanaged owning record: " + owner.get_full_name()};
  }
  std::map<std::string, const member*> protocols{};
  for (const auto* value : selected) {
    if (value->modifier != member::modifier_type::none || value->type_name_list.size() != 1 ||
        (value->type_name_list.front().name != "uint32" && value->type_name_list.front().name != "uint64") ||
        value->transient || value->managed || value->version || value->magic ||
        value->compact != compact_encoding::none || value->obsolete ||
        !value->created_version.empty() || !value->obsolete_version.empty() ||
        !value->replaced_member.empty()) {
      throw std::invalid_argument{"auto_size requires a fixed uint32 or uint64 scalar: " + owner.get_full_name() + "." + value->name};
    }
    for (const auto& format : value->auto_size_formats) {
      if (format != "binary_none" && format != "binary_integer" && format != "binary_string") {
        throw std::invalid_argument{"auto_size supports only native binary formats: " + format};
      }
      if (value->omits(format)) {
        throw std::invalid_argument{"auto_size field is omitted from its selected format: " + owner.get_full_name() + "." + value->name};
      }
      if (!protocols.emplace(format, value).second) {
        throw std::invalid_argument{"Repeated auto_size field for " + format + ": " + owner.get_full_name()};
      }
    }
    if (value->wire_only) {
      for (const auto format : {"json", "binary_none", "binary_integer", "binary_string",
                               "protobuf", "protojson", "textproto"}) {
        if (!value->auto_sizes(format) && !value->omits(format)) {
          throw std::invalid_argument{"wire_only must omit unselected formats: " + owner.get_full_name() + "." + value->name};
        }
      }
    }
  }
}

// Check every owning declaration, including open generic metadata, without changing existing records.
inline void validate(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  for (const auto& node : statements) {
    if (node->type == object_type::namespace_type) {
      validate(static_cast<const namespace_node&>(*node).statements);
    } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
      validate_owner(static_cast<const class_node&>(*node));
    }
  }
}

// Resolve class settings against authored identities before generation can silently ignore a typo.
inline void collect(const std::vector<std::unique_ptr<syntax_node>>& statements,
                    std::map<std::string, const class_node*>& owners) {
  for (const auto& node : statements) {
    if (node->type == object_type::namespace_type) {
      collect(static_cast<const namespace_node&>(*node).statements, owners);
    } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
      const auto& owner = static_cast<const class_node&>(*node);
      owners.emplace(owner.get_full_name(), &owner);
    }
  }
}

// Require configured owners to declare derived metadata; no setting changes ordinary record APIs.
inline void validate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     const configuration& settings) {
  validate(statements);
  std::map<std::string, const class_node*> owners;
  collect(statements, owners);
  for (const auto& [name, options] : settings) {
    static_cast<void>(options);
    const auto found = owners.find(name);
    if (found == owners.end()) { throw std::invalid_argument{"Unknown auto_size class: " + name}; }
    if (fields(*found->second).empty()) {
      throw std::invalid_argument{"auto_size_validation requires a class with auto_size fields: " + name};
    }
  }
}

} // namespace rohit::serializer::writer::auto_size
