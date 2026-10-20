// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <model.hpp>
#include <rohit/managed.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace legacy_fixture {
namespace managed = rohit::managed;
using root = legacy_migration::document;
using entry = legacy_migration::entry;
using traits = managed::model_traits<root>;
using entry_traits = managed::model_traits<entry>;
using storage = traits::storage_type;
using id_type = traits::id_type;

inline constexpr managed::document_id document_id{41U, 42U};
inline constexpr std::uint64_t ordinary_initial = 0x0102030405060708ULL;
inline constexpr std::uint64_t ordinary_updated = 900U;
inline constexpr std::uint64_t first_amount = 128U;
inline constexpr std::uint64_t second_amount = 16384U;
inline constexpr std::uint64_t appended_amount = 65536U;
inline constexpr std::uint64_t branch_amount = 777U;

template <managed::history_mode Mode, managed::history_labels Labels>
using store = managed::model_store<root, Mode, Labels, traits, managed::store_features::none>;

// Report an authored invariant independently of optional compiler assertion switches.
inline void require(bool value, std::string_view message) {
  if (!value) { throw std::runtime_error{std::string{message}}; }
}

// Keep fixture I/O separate from both old and current migration implementations.
inline void write(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  std::ofstream output{path, std::ios::binary};
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  output.close();
}

// Read only qualification-owned files and reject failed source access.
inline std::vector<std::uint8_t> read(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot read qualification fixture"}; }
  std::vector<std::uint8_t> result{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
  if (input.bad()) { throw std::runtime_error{"Cannot finish reading qualification fixture"}; }
  return result;
}

// Use the selected generation's public ordinary codec without migration policy overrides.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::vector<std::uint8_t> encode(const storage& value) {
  rohit::full_stream_auto_alloc output{};
  rohit::serializer::serialize_to<Protocol>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require exact fresh-value decoding with the current generated identity policy.
template <typename Value, template <rohit::serializer::serialize_type> typename Protocol>
Value decode(std::span<const std::uint8_t> bytes) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<Value, Protocol>(input);
}

// Start every old native store with an independently identified owned graph.
inline root initial() {
  root value{};
  value.entries.resize(2);
  value.entries[0].amount = first_amount;
  value.entries[1].amount = second_amount;
  value.ordinary = ordinary_initial;
  return value;
}

// Exercise both label policies through their existing generated transaction entry points.
template <managed::history_labels Labels, typename Store, typename Function>
void transact(Store& value, std::string label, Function operation) {
  if constexpr (Labels == managed::history_labels::enabled) {
    value.execute_transaction(std::move(label), std::move(operation)).throw_if_failed();
  } else {
    value.execute_transaction(std::move(operation)).throw_if_failed();
  }
}

// Generate retained redo states and two tree children without manually assigning store IDs.
template <managed::history_mode Mode, managed::history_labels Labels>
void populate(store<Mode, Labels>& value) {
  transact<Labels>(value, "Retained update", [](auto& edit) { edit.root().set_ordinary(ordinary_updated); });
  transact<Labels>(value, "Retained append", [](auto& edit) {
    entry added{};
    added.amount = appended_amount;
    edit.root().entries().append(std::move(added));
  });
  if constexpr (Mode != managed::history_mode::disabled) { value.undo(); }
  if constexpr (Mode == managed::history_mode::tree) {
    transact<Labels>(value, "Retained branch", [](auto& edit) {
      entry added{};
      added.amount = branch_amount;
      edit.root().entries().append(std::move(added));
    });
    value.undo();
  }
}

// Check numeric identity, ordinary values and nested payloads independently of encoded byte counts.
inline void check_raw(const storage& value, bool boundary) {
  const auto& payload = traits::payload(value);
  require(payload.entries.size() == 2, "Raw owned membership changed");
  require(value.persistent_id == (boundary ? id_type{127U} : id_type{1U}), "Raw root identity changed");
  require(payload.entries[0].persistent_id == (boundary ? id_type{128U} : id_type{2U}), "Raw first identity changed");
  require(payload.entries[1].persistent_id == (boundary ? std::numeric_limits<id_type>::max() : id_type{3U}), "Raw high identity changed");
  require(payload.ordinary == ordinary_updated, "Ordinary integer changed");
  require(entry_traits::payload(payload.entries[0]).amount == first_amount, "First ordinary child value changed");
  require(entry_traits::payload(payload.entries[1]).amount == second_amount, "Second ordinary child value changed");
}

// Retained snapshots and undo branches must keep the old allocator watermark even when IDs are absent.
template <managed::history_mode Mode, managed::history_labels Labels>
void check_store(const store<Mode, Labels>& value) {
  const auto& current = *value.read();
  const auto& payload = traits::payload(current);
  require(value.document().high == document_id.high && value.document().low == document_id.low, "Document namespace changed");
  require(value.allocated_id() == (Mode == managed::history_mode::tree ? 5U : 4U), "Allocation high-water mark changed");
  require(current.persistent_id == 1U, "Managed root identity changed");
  require(payload.ordinary == ordinary_updated, "Managed ordinary value changed");
  require(payload.entries.size() == (Mode == managed::history_mode::disabled ? 3U : 2U), "Managed current membership changed");
  require(payload.entries[0].persistent_id == 2U && payload.entries[1].persistent_id == 3U, "Managed child identities changed");
  if constexpr (Mode == managed::history_mode::linear) { require(value.history_cursor() == 1U, "Linear cursor changed"); }
  if constexpr (Mode == managed::history_mode::disabled) {
    require(payload.entries[2].persistent_id == 4U, "Disabled-history appended identity changed");
  }
}
} // namespace legacy_fixture
