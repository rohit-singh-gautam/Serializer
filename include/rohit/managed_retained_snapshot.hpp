// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/managed_snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed::detail {

// Runtime-only history bytes. Default storage remains an ordinary owning vector;
// compact storage retains independent immutable blocks, never a flattened cache.
class retained_snapshot {
  using bytes_type = std::vector<std::uint8_t>;
  std::variant<bytes_type, compact_snapshot> value_{};
public:
  // Empty default histories allocate no byte storage or shared block tables.
  retained_snapshot() = default;
  // Preserve the ordinary vector's owning copy/move policy when compaction is disabled.
  explicit retained_snapshot(bytes_type value) noexcept : value_{std::move(value)} {}
  // Transfer a completely prepared compact table before the durable publication boundary.
  explicit retained_snapshot(compact_snapshot value) noexcept : value_{std::move(value)} {}
  // Copy default bytes or share an immutable compact table without changing its admission charges.
  retained_snapshot(const retained_snapshot&) = default;
  // Move ownership without allocating or invoking the selected PMR resource.
  retained_snapshot(retained_snapshot&&) noexcept = default;
  // Retain a valid predecessor if copying ordinary bytes fails.
  retained_snapshot& operator=(const retained_snapshot& source) {
    if (this != &source) {
      retained_snapshot copy{source};
      swap(copy);
    }
    return *this;
  }
  // Replace unpublished or retired bytes without allocation.
  retained_snapshot& operator=(retained_snapshot&&) noexcept = default;

  // Return logical encoded length; history byte limits remain independent of deduplication.
  std::size_t size() const noexcept {
    if (const auto* bytes = std::get_if<bytes_type>(&value_)) { return bytes->size(); }
    return std::get<compact_snapshot>(value_).size();
  }
  // Only default vector capacity belongs to the model's auxiliary reservation.
  // Compact tables/blocks each own a separate shared resident charge already.
  std::size_t capacity() const noexcept {
    if (const auto* bytes = std::get_if<bytes_type>(&value_)) { return bytes->capacity(); }
    return 0;
  }
  // Borrow immutable block ownership for exact same-position reuse during preparation.
  const compact_snapshot* compact() const noexcept {
    return std::get_if<compact_snapshot>(&value_);
  }
  // Obtain independent contiguous bytes for current storage, Save or validated history navigation.
  bytes_type materialize() const {
    if (const auto* bytes = std::get_if<bytes_type>(&value_)) { return *bytes; }
    return std::get<compact_snapshot>(value_).materialize();
  }
  // Decode default bytes without copying; compact temporaries exist only during the callback.
  // The callback must return an owning value, never a borrowed reference into its input.
  template <typename Callable>
  decltype(auto) with_bytes(Callable&& callback) const {
    if (const auto* bytes = std::get_if<bytes_type>(&value_)) {
      return std::invoke(std::forward<Callable>(callback), *bytes);
    }
    const auto bytes = std::get<compact_snapshot>(value_).materialize();
    return std::invoke(std::forward<Callable>(callback), bytes);
  }
  // Exchange prepared ownership using only no-throw vector/shared-table operations.
  void swap(retained_snapshot& other) noexcept { value_.swap(other.value_); }
  // Compare exact encoded bytes for recovery ancestry checks, without merging logical identities.
  bool operator==(const retained_snapshot& other) const {
    if (size() != other.size()) { return false; }
    return with_bytes([&](const auto& left) {
      return other.with_bytes([&](const auto& right) { return left == right; });
    });
  }
};

static_assert(std::is_nothrow_swappable_v<retained_snapshot>);
static_assert(std::is_nothrow_move_constructible_v<retained_snapshot>);
static_assert(std::is_nothrow_move_assignable_v<retained_snapshot>);

} // namespace rohit::managed::detail
