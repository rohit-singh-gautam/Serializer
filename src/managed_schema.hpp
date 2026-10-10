#pragma once

#include <rohit/serializer_creator.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace rohit::serializer::writer {

// Hash the reachable wire schema independently of language naming and source whitespace.
// Callers validate acyclic owning models first; the cache belongs to one generation pass.
inline std::uint64_t managed_schema_hash(const class_node* source,
                                         std::map<const class_node*, std::uint64_t>& cache) {
  if (const auto found = cache.find(source); found != cache.end()) {
    return found->second;
  }
  constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
  constexpr std::uint64_t fnv_prime = 1099511628211ULL;
  auto hash = fnv_offset_basis;
  const auto add = [&](std::string_view text) {
    for (const auto value : text) {
      hash = (hash ^ static_cast<unsigned char>(value)) * fnv_prime;
    }
    hash = (hash ^ 0xffu) * fnv_prime;
  };
  add(source->get_full_name());
  for (const auto& base : source->parents) {
    add("base"); add(std::to_string(base.id)); add(base.display_name);
    add(base.managed ? "managed" : "value");
    add(std::to_string(managed_schema_hash(base.parent_class, cache)));
  }
  for (const auto& field : source->member_list) {
    if (field.transient) { continue; }
    add(std::to_string(field.id));
    add(field.display_name);
    add(field.default_value);
    add(std::to_string(static_cast<unsigned>(field.modifier)));
    if (field.fixed_extent != 0) {
      add("fixed_extent");
      add(std::to_string(field.fixed_extent));
    }
    add(field.managed ? "managed" : "value");
    add(field.key);
    if (field.version) { add("version"); add(field.compatibility_version); }
    if (!field.created_version.empty()) { add("created"); add(field.created_version); }
    if (!field.obsolete_version.empty()) { add("obsolete"); add(field.obsolete_version); }
    for (const auto& format : field.omitted_formats) { add("omit"); add(format); }
    if (field.key_node && field.key_node->type == object_type::enum_type) {
      for (const auto& name : static_cast<const enum_node*>(field.key_node)->enum_name_list) {
        add(name);
      }
    }
    for (const auto& type : field.type_name_list) {
      add(type.get_full_name());
      if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
        add(std::to_string(
            managed_schema_hash(static_cast<const class_node*>(type.resolved_node), cache)));
      } else if (type.resolved_node && type.resolved_node->type == object_type::enum_type) {
        for (const auto& name : static_cast<const enum_node*>(type.resolved_node)->enum_name_list) {
          add(name);
        }
      }
    }
  }
  cache.emplace(source, hash);
  return hash;
}

} // namespace rohit::serializer::writer
