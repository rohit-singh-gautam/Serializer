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
  for (const auto& field : source->member_list) {
    add(std::to_string(field.id));
    add(field.display_name);
    add(field.default_value);
    add(std::to_string(static_cast<unsigned>(field.modifier)));
    add(field.managed ? "managed" : "value");
    add(field.key);
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
