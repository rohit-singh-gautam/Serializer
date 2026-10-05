#pragma once

#include <rohit/managed.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

enum class collaboration_history_action : std::uint32_t { edit = 1, undo = 2, redo = 3 };

namespace detail {

using collaboration_field_address = std::pair<std::uint64_t, std::uint32_t>;
using collaboration_fields = std::map<collaboration_field_address, std::vector<std::uint8_t>>;
using collaboration_field_versions = std::map<collaboration_field_address, std::uint64_t>;
using collaboration_entity_versions = std::map<std::uint64_t, std::uint64_t>;

// Encode fields, scalars and containers through the generic codec rather than a record-only helper.
inline std::vector<std::uint8_t> encode_collaboration_value(const auto& value,
                                                            std::size_t max_bytes) {
  if (max_bytes == 0) {
    throw std::length_error{"Collaboration field encoding budget is zero"};
  }
  const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, max_bytes), max_bytes};
  full_stream_auto_alloc_limits stream{&limits};
  serializer::binary_integer<serializer::serialize_type::out, full_stream_auto_alloc_limits>
      encoder{stream};
  encoder.struct_serialize_out(value);
  return {stream.begin(), stream.begin() + stream.current_offset()};
}

// Missing entries are unchanged baseline contributions; retained tombstones preserve ABA detection.
inline std::uint64_t collaboration_version(const auto& versions, const auto& key) {
  const auto found = versions.find(key);
  return found == versions.end() ? 0 : found->second;
}

// Encode ownership independently of child payload so edits to different children remain independent.
template <typename Value>
auto collaboration_membership(const Value& value) {
  if constexpr (requires { value.persistent_id; }) {
    return value.persistent_id;
  } else if constexpr (requires { typename Value::mapped_type; }) {
    std::map<typename Value::key_type, std::uint64_t> members;
    for (const auto& [key, child] : value) {
      members.emplace(key, child.persistent_id);
    }
    return members;
  } else {
    std::vector<std::uint64_t> members;
    members.reserve(value.size());
    for (const auto& child : value) {
      members.push_back(child.persistent_id);
    }
    return members;
  }
}

// Visit single children, map values, or ordered elements without retaining borrowed references.
template <typename Value, typename Visitor>
void collaboration_children(Value& value, Visitor&& visitor) {
  if constexpr (requires { value.persistent_id; }) {
    visitor(value);
  } else if constexpr (requires { typename Value::mapped_type; }) {
    for (auto& [key, child] : value) {
      static_cast<void>(key);
      visitor(child);
    }
  } else {
    for (auto& child : value) {
      visitor(child);
    }
  }
}

// Bound encoded fields independently of snapshots; collection fields contain only ownership edges.
class collaboration_field_collector {
  std::size_t remaining_;
  std::size_t max_fields_;

public:
  collaboration_fields fields{};

  // A traversal is temporary and never adds mutable aliases to a history entry.
  collaboration_field_collector(std::size_t max_bytes, std::size_t max_fields)
      : remaining_{max_bytes}, max_fields_{max_fields} {}

  // Capture the entire ordinary field as one atomic value under its stable schema field ID.
  void value(std::uint64_t entity, std::uint32_t field, const auto& input) {
    if (fields.size() >= max_fields_) {
      throw std::length_error{"Collaboration field budget exceeded"};
    }
    auto bytes = encode_collaboration_value(input, remaining_);
    remaining_ -= bytes.size();
    if (!fields.emplace(collaboration_field_address{entity, field}, std::move(bytes)).second) {
      throw std::invalid_argument{"Duplicate collaboration field address"};
    }
  }

  // Record membership once and recursively collect each independently identified child's fields.
  template <typename ChildTraits>
  void owned(std::uint64_t entity, std::uint32_t field, const auto& input) {
    value(entity, field, collaboration_membership(input));
    collaboration_children(
        input, [&](const auto& child) { ChildTraits::visit_collaboration_fields(child, *this); });
  }
};

// A failed reverse is a conflict, not a partially applied edit or a best-effort restoration.
class collaboration_history_conflict : public std::runtime_error {
public:
  // Distinguish lost dependencies from malformed input and resource failures.
  collaboration_history_conflict() : std::runtime_error{"Collaborative history conflict"} {}
};

// Merge a trusted historical inverse into a private candidate after revision checks succeed.
class collaboration_history_merger {
  std::size_t max_bytes_;

  // Compare exact codec values, including floating-point bit patterns and container contents.
  bool same(const auto& left, const auto& right) const {
    return encode_collaboration_value(left, max_bytes_) ==
           encode_collaboration_value(right, max_bytes_);
  }

public:
  // Reuse the store snapshot budget for every bounded scratch encoding.
  explicit collaboration_history_merger(std::size_t max_bytes) : max_bytes_{max_bytes} {}

  // Reverse changed ordinary fields only; external version checks also catch equal-value ABA edits.
  void value(std::uint64_t entity, std::uint32_t field, auto& current, const auto& expected,
             const auto& desired) {
    static_cast<void>(entity);
    static_cast<void>(field);
    if (same(expected, desired)) {
      return;
    }
    if (!same(current, expected)) {
      throw collaboration_history_conflict{};
    }
    current = desired;
  }

  // Preserve current payloads of retained children while reversing membership and nested fields.
  template <typename ChildTraits, typename Value>
  void owned(std::uint64_t entity, std::uint32_t field, Value& current, const Value& expected,
             const Value& desired) {
    static_cast<void>(entity);
    static_cast<void>(field);
    if (same(expected, desired)) {
      return;
    }
    using child_type = typename ChildTraits::storage_type;
    std::map<std::uint64_t, const child_type*> old_children;
    std::map<std::uint64_t, const child_type*> current_children;
    collaboration_children(
        expected, [&](const auto& child) { old_children.emplace(child.persistent_id, &child); });
    collaboration_children(
        current, [&](const auto& child) { current_children.emplace(child.persistent_id, &child); });
    const auto old_members = collaboration_membership(expected);
    const auto desired_members = collaboration_membership(desired);
    if (old_members != desired_members) {
      if (collaboration_membership(current) != old_members) {
        throw collaboration_history_conflict{};
      }
      auto replacement = desired;
      collaboration_children(replacement, [&](auto& child) {
        const auto old = old_children.find(child.persistent_id);
        const auto live = current_children.find(child.persistent_id);
        if (old != old_children.end() && live != current_children.end()) {
          auto merged = *live->second;
          ChildTraits::merge_collaboration(merged, *old->second, child, *this);
          child = std::move(merged);
        }
      });
      current = std::move(replacement);
    } else {
      std::map<std::uint64_t, child_type*> live_children;
      collaboration_children(
          current, [&](auto& child) { live_children.emplace(child.persistent_id, &child); });
      collaboration_children(desired, [&](const auto& child) {
        const auto old = old_children.find(child.persistent_id);
        if (old == old_children.end() || same(*old->second, child)) {
          return;
        }
        const auto live = live_children.find(child.persistent_id);
        if (live == live_children.end()) {
          throw collaboration_history_conflict{};
        }
        ChildTraits::merge_collaboration(*live->second, *old->second, child, *this);
      });
    }
  }
};

} // namespace detail
} // namespace rohit::managed
