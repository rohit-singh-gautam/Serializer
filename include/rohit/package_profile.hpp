// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/package.hpp>

#include <concepts>
#include <cstddef>
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
};

// A typed key index validates duplicate, forward, missing and wrong-type payload references.
// Scope strings are explicit vocabulary identities; they are not URI-normalized. Target types
// are the selected C++ model types. No model pointers are retained, so candidate vector growth
// cannot invalidate the index. Object lookup/assignment remains the application's responsibility.
template <typename Key>
  requires (std::integral<Key> || std::same_as<Key, std::string>)
class reference_index {
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

  // Charge key/location storage before copying it into a retained validation index.
  void charge(const std::string& scope, const Key& key, const payload_location& location,
              std::size_t fixed_bytes) {
    budget_.consume(resource::references, 1, location.part);
    budget_.consume(resource::work_units, 1, location.part);
    budget_.consume(resource::metadata_bytes, fixed_bytes, location.part);
    budget_.consume(resource::metadata_bytes, scope.size(), location.part);
    budget_.consume(resource::metadata_bytes, location.part.size(), location.part);
    budget_.consume(resource::metadata_bytes, location.field_path.size(), location.part);
    if constexpr (std::same_as<Key, std::string>) {
      budget_.consume(resource::metadata_bytes, key.size(), location.part);
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
                  0, 0, 0, location.field_path, key_text(selected.second)};
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
      budget_.consume(resource::work_units, 1, item.location.part);
      const auto found = targets_.find(item.key);
      if (found == targets_.end() || found->second.type != item.expected) {
        throw error{error_code::invalid_reference,
                    found == targets_.end() ? "Missing payload reference target"
                                            : "Payload reference target has an unexpected type",
                    item.location.part, 0, 0, 0, item.location.field_path, key_text(item.key.second)};
      }
    }
  }
};

} // namespace rohit::serializer::package
