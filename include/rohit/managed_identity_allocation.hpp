// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>
#include <utility>
#include <rohit/managed_records.hpp>
#include <limits>
#include <stdexcept>

namespace rohit::managed {
// Snapshot a native allocation cursor without allocating or copying model/history payloads.
// A lease belongs to the surrounding document/session; copying this value grants no authority.
struct identity_allocation_context {
  std::uint64_t watermark{};
  std::uint64_t first{};
  std::uint64_t last{};
  std::uint64_t consumed{};

  // Compare an exact preview base, including consumed reservations rather than only model state.
  bool operator==(const identity_allocation_context&) const = default;

  // Check native-width bounds and the absence of partially initialized range metadata.
  void validate(std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max()) const {
    if (watermark > maximum || (first == 0 && (last != 0 || consumed != 0)) ||
        (first != 0 && (last < first || last > watermark || consumed < first - 1U ||
                        consumed > last))) {
      throw std::invalid_argument{"Invalid native identity allocation context"};
    }
  }

  // Test capacity without exposing another ID or advancing accepted allocation state.
  [[nodiscard]] bool available() const noexcept {
    return first != 0 && consumed < last;
  }

  // Reserve an isolated value; its owner must admit it through configured persistence before exposure.
  [[nodiscard]] std::uint64_t take() {
    validate();
    if (!available()) {
      throw std::overflow_error{"Native collaboration identity lease is exhausted"};
    }
    return ++consumed;
  }
};

namespace detail {
// Native authority checkpoint framing is shared by normal persistence and explicit migration tools.
inline constexpr std::array<std::uint8_t, 8> identity_allocation_checkpoint_magic{
    'S', 'E', 'R', 'I', 'D', 'L', 'S', 1};
// Authority-owned durable lease records; no range is returned twice to a fresh client join.
// The caller persists a prepared record and its burned watermark before calling publish().
class identity_lease_book {
  std::vector<records::identity_lease> entries_{};
public:
  // Retain validated records restored from the same document's native checkpoint.
  explicit identity_lease_book(std::vector<records::identity_lease> entries = {})
      : entries_{std::move(entries)} {}

  // Inspect exact serialized records without granting permission to change their ownership.
  [[nodiscard]] const auto& entries() const noexcept { return entries_; }

  // Check disjoint ranges, native-width bounds and every consumed cursor before publication.
  void validate(std::uint64_t watermark, std::uint64_t maximum, std::size_t maximum_entries) const {
    if (entries_.size() > maximum_entries) {
      throw std::length_error{"Native identity lease ledger exceeds its record budget"};
    }
    std::uint64_t previous{};
    for (const auto& entry : entries_) {
      identity_allocation_context context{watermark, entry.first, entry.last, entry.committed};
      context.validate(maximum);
      if (entry.first <= previous || entry.session_format.empty() || entry.session.empty()) {
        throw std::invalid_argument{"Overlapping or unowned native identity lease"};
      }
      previous = entry.last;
    }
  }

  // Prepare a fresh range without mutating the ledger or exposing a live entity identity.
  [[nodiscard]] records::identity_lease prepare(std::string_view format,
      std::span<const std::uint8_t> session, std::uint64_t watermark, std::uint64_t count,
      std::uint64_t maximum, std::size_t maximum_entries) {
    if (entries_.size() >= maximum_entries || count == 0 || watermark > maximum ||
        count > maximum - watermark || format.empty() || session.empty()) {
      throw std::length_error{"Native identity lease capacity is unavailable"};
    }
    // Reserving vector capacity precedes the durable decision; publish performs no allocation.
    entries_.reserve(entries_.size() + 1U);
    records::identity_lease result;
    result.session_format = format;
    result.session.assign(session.begin(), session.end());
    result.first = watermark + 1U;
    result.last = watermark + count;
    result.committed = watermark;
    return result;
  }

  // Install only after the reservation is durable; prepared capacity makes this nonallocating.
  void publish(records::identity_lease entry) noexcept { entries_.push_back(std::move(entry)); }

  // Find the authenticated owner's range containing an explicitly issued native identity.
  [[nodiscard]] records::identity_lease* find(std::string_view format,
      std::span<const std::uint8_t> session, std::uint64_t id) noexcept {
    const auto found = std::lower_bound(entries_.begin(), entries_.end(), id,
        [](const auto& entry, std::uint64_t value) { return entry.last < value; });
    if (found == entries_.end() || id < found->first || found->session_format != format ||
        !std::ranges::equal(found->session, session)) { return nullptr; }
    return &*found;
  }

  // Locate one journal cursor update by its immutable range identity.
  [[nodiscard]] records::identity_lease* find(std::uint64_t first) noexcept {
    const auto found = std::lower_bound(entries_.begin(), entries_.end(), first,
        [](const auto& entry, std::uint64_t value) { return entry.first < value; });
    return found != entries_.end() && found->first == first ? &*found : nullptr;
  }
};
} // namespace detail
} // namespace rohit::managed
