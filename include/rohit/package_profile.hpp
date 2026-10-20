// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/package.hpp>

#include <concepts>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace rohit::serializer::package {

struct payload_location {
  std::string part;
  std::string field_path;
  std::string relationship_id{};
};

// A typed key index validates duplicate, forward, missing and wrong-type payload references.
// Scope strings are explicit vocabulary identities; they are not URI-normalized. Target types
// are the selected C++ model types. No model pointers are retained, so candidate vector growth
// cannot invalidate the index. Object lookup/assignment remains the application's responsibility.
template <typename Key>
  requires (std::integral<Key> || std::same_as<Key, std::string>)
class reference_index {
  using key_buffer = std::array<char, std::numeric_limits<Key>::digits10 + 4U>;
  using scoped_key = std::pair<std::string, Key>;
  struct target {
    std::type_index type;
    payload_location location;
  };
  struct request {
    scoped_key key;
    std::type_index expected;
    payload_location location;
  };
  budget& budget_;
  std::map<scoped_key, target> targets_{};
  std::vector<request> requests_{};

  // Render only a caller-declared key for structured reference diagnostics.
  [[nodiscard]] static std::string key_text(const Key& key) {
    if constexpr (std::same_as<Key, std::string>) {
      return key;
    } else {
      return std::to_string(key);
    }
  }

  // Borrow string keys or render fixed-width integers on the stack before a diagnostic charge.
  [[nodiscard]] static std::string_view key_view(const Key& key, key_buffer& storage) {
    if constexpr (std::same_as<Key, std::string>) { return key; }
    else {
      using Integer = std::conditional_t<std::same_as<Key, bool>, unsigned char, Key>;
      const auto converted = std::to_chars(storage.data(), storage.data() + storage.size(),
          static_cast<Integer>(key));
      return {storage.data(), static_cast<std::size_t>(converted.ptr - storage.data())};
    }
  }

  // Charge key/location storage before copying it into a retained validation index.
  void charge(const std::string& scope, const Key& key, const payload_location& location,
              std::size_t fixed_bytes) {
    key_buffer text{};
    const budget_location declared{location.part, location.field_path,
        key_view(key, text), location.relationship_id};
    budget_.consume_at(resource::references, 1, declared);
    budget_.consume_at(resource::work_units, 1, declared);
    budget_.consume_at(resource::metadata_bytes, fixed_bytes, declared);
    budget_.consume_at(resource::metadata_bytes, scope.size(), declared);
    budget_.consume_at(resource::metadata_bytes, location.part.size(), declared);
    budget_.consume_at(resource::metadata_bytes, location.field_path.size(), declared);
    budget_.consume_at(resource::metadata_bytes, location.relationship_id.size(), declared);
    if constexpr (std::same_as<Key, std::string>) {
      budget_.consume_at(resource::metadata_bytes, key.size(), declared);
    } else {
      static_cast<void>(key);
    }
  }

public:
  // Borrow the same budget as archive/codec validation; the index must not outlive it.
  explicit reference_index(budget& shared) : budget_{shared} {}

  // Register a typed payload identity; a duplicate key within one scope always fails.
  template <typename Target>
  void add_target(const std::string& scope, const Key& key, const payload_location& location = {}) {
    charge(scope, key, location, sizeof(scoped_key) + sizeof(target));
    scoped_key selected{scope, key};
    if (targets_.contains(selected)) {
      throw error{error_code::invalid_reference, "Duplicate payload key", location.part,
                  0, 0, 0, location.field_path, key_text(selected.second), location.relationship_id};
    }
    targets_.emplace(std::move(selected), target{typeid(Target), location});
  }

  // Retain a forward reference for validation after every typed part has decoded.
  template <typename Target>
  void require_target(const std::string& scope, const Key& key, const payload_location& location = {}) {
    charge(scope, key, location, sizeof(request));
    requests_.push_back({{scope, key}, typeid(Target), location});
  }

  // Validate all references before aggregate publication, preserving source field/key diagnostics.
  void validate() const {
    for (const auto& item : requests_) {
      key_buffer text{};
      budget_.consume_at(resource::work_units, 1, budget_location{item.location.part,
          item.location.field_path, key_view(item.key.second, text), item.location.relationship_id});
      const auto found = targets_.find(item.key);
      if (found == targets_.end() || found->second.type != item.expected) {
        throw error{error_code::invalid_reference,
                    found == targets_.end() ? "Missing payload reference target"
                                            : "Payload reference target has an unexpected type",
                    item.location.part, 0, 0, 0, item.location.field_path, key_text(item.key.second),
                    item.location.relationship_id};
      }
    }
  }
};

} // namespace rohit::serializer::package
