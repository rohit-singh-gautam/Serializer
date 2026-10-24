// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "managed_resident.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rohit::managed::detail {

// Logical byte and pointer-count limits apply before any snapshot storage allocation.
struct compact_snapshot_limits {
  static constexpr std::size_t default_max_bytes = 64 * 1024 * 1024;
  static constexpr std::size_t default_max_chunks = 16 * 1024;
  std::size_t max_bytes{default_max_bytes};
  std::size_t max_chunks{default_max_chunks};
};

// Independently reconstructible immutable bytes; sharing never retains an earlier revision table.
// Copies/readers may cross threads when the retained PMR resource supports their destruction.
// Resident charges are conservative admission estimates, not physical heap/RSS measurements.
class compact_snapshot {
public:
  static constexpr std::size_t chunk_bytes = 4 * 1024;

private:
  // Match the runtime's conservative allowance for shared ownership and allocator bookkeeping.
  static constexpr std::size_t shared_owner_bytes = 14 * sizeof(void*);

  struct chunk {
    resident_charge charge;
    const std::size_t length;
    std::array<std::uint8_t, chunk_bytes> bytes{};

    // Retain one reservation until every independently reconstructed revision releases this block.
    chunk(std::span<const std::uint8_t> input, const std::shared_ptr<resident_ledger>& ledger)
        : charge{ledger, sizeof(chunk) + shared_owner_bytes}, length{input.size()} {
      std::ranges::copy(input, bytes.begin());
    }
  };
  using chunk_pointer = std::shared_ptr<const chunk>;
  using chunk_table = std::vector<chunk_pointer, resident_allocator<chunk_pointer>>;

  struct table {
    std::shared_ptr<resident_ledger> ledger;
    std::shared_ptr<std::pmr::memory_resource> resource;
    resident_charge charge;
    chunk_table chunks;
    const std::size_t length;

    // Admit pointer storage before populating it; all failure paths destroy only this private table.
    table(std::size_t bytes, std::size_t count, std::shared_ptr<resident_ledger> selected_ledger,
          std::shared_ptr<std::pmr::memory_resource> selected_resource)
        : ledger{std::move(selected_ledger)}, resource{std::move(selected_resource)},
          charge{ledger, estimate_table_bytes(count)},
          chunks{resident_allocator<chunk_pointer>{resource}}, length{bytes} {
      chunks.reserve(count);
      charge.resize(estimate_table_bytes(chunks.capacity()));
    }

    // Bound both container capacity multiplication and the shared-control metadata addition.
    static std::size_t estimate_table_bytes(std::size_t capacity) {
      auto result = multiply_memory_estimate(capacity, sizeof(chunk_pointer));
      add_memory_estimate(result, sizeof(table) + shared_owner_bytes);
      return result;
    }
  };

  std::shared_ptr<const table> table_{};

  // Publish only a completely prepared table; later copies and swaps need no allocation.
  explicit compact_snapshot(std::shared_ptr<const table> value) noexcept
      : table_{std::move(value)} {}

public:
  // An empty snapshot has no allocation, reservation or resource lifetime dependency.
  compact_snapshot() noexcept = default;

  // Build bounded independent bytes, reusing only exact same-position blocks in the selected domain.
  // Passing no ledger inherits the previous snapshot's ledger, or creates an unlimited standalone one.
  // The explicit resource is never selected through thread-local or process PMR defaults.
  static compact_snapshot prepare(std::span<const std::uint8_t> bytes,
      const compact_snapshot* previous = nullptr, compact_snapshot_limits limits = {},
      std::shared_ptr<resident_ledger> ledger = {},
      std::shared_ptr<std::pmr::memory_resource> resource = {}) {
    const auto count = bytes.size() / chunk_bytes + (bytes.size() % chunk_bytes != 0 ? 1 : 0);
    if (bytes.size() > limits.max_bytes || count > limits.max_chunks) {
      throw std::length_error{"Managed compact snapshot exceeds byte or chunk bound"};
    }
    if (bytes.empty()) { return {}; }
    const auto* prior = previous && previous->table_ ? previous->table_.get() : nullptr;
    if (!ledger) {
      ledger = prior ? prior->ledger :
          std::make_shared<resident_ledger>(std::numeric_limits<std::size_t>::max());
    }
    auto selected = std::allocate_shared<table>(resident_allocator<table>{resource},
        bytes.size(), count, ledger, resource);
    const bool reusable = prior && prior->ledger.get() == ledger.get() &&
        prior->resource.get() == resource.get();
    for (std::size_t index = 0; index < count; ++index) {
      const auto offset = index * chunk_bytes;
      const auto input = bytes.subspan(offset, std::min(chunk_bytes, bytes.size() - offset));
      if (reusable && index < prior->chunks.size() &&
          prior->chunks[index]->length == input.size() &&
          std::equal(input.begin(), input.end(), prior->chunks[index]->bytes.begin())) {
        selected->chunks.push_back(prior->chunks[index]);
      } else {
        selected->chunks.push_back(std::allocate_shared<chunk>(resident_allocator<chunk>{resource},
            input, ledger));
      }
    }
    return compact_snapshot{std::move(selected)};
  }

  // Return logical encoded size, independently of deduplication and resource admission estimates.
  std::size_t size() const noexcept { return table_ ? table_->length : 0; }

  // Empty values materialize to an empty exact message without retaining an empty table.
  bool empty() const noexcept { return size() == 0; }

  // Inspect retained pointer count without constructing bytes or changing ownership.
  std::size_t chunk_count() const noexcept { return table_ ? table_->chunks.size() : 0; }

  // This charge is already in the shared ledger and must not be added to auxiliary accounting again.
  std::size_t table_bytes() const noexcept { return table_ ? table_->charge.bytes() : 0; }

  // Count same-position physical blocks shared with another retained snapshot, without allocation.
  std::size_t shared_chunk_count(const compact_snapshot& other) const noexcept {
    const auto count = std::min(chunk_count(), other.chunk_count());
    std::size_t result{};
    for (std::size_t index = 0; index < count; ++index) {
      if (table_->chunks[index] == other.table_->chunks[index]) { ++result; }
    }
    return result;
  }

  // Borrow block identity/used length/admission charge for bounded diagnostics; never expose writable bytes.
  template <typename Callable>
  void visit_chunks(Callable&& callback) const {
    if (!table_) { return; }
    for (const auto& block : table_->chunks) {
      std::invoke(callback, static_cast<const void*>(block.get()), block->length, block->charge.bytes());
    }
  }

  // Reconstruct one exact contiguous message before decoding, validation or durable publication.
  // No flattened cache or previous revision dependency survives this call.
  std::vector<std::uint8_t> materialize() const {
    std::vector<std::uint8_t> result(size());
    std::size_t offset{};
    if (table_) {
      for (const auto& block : table_->chunks) {
        std::copy_n(block->bytes.data(), block->length, result.data() + offset);
        offset += block->length;
      }
    }
    return result;
  }

  // Exchange preconstructed ownership handles; used by no-allocation durable publication paths.
  void swap(compact_snapshot& other) noexcept { table_.swap(other.table_); }
};

} // namespace rohit::managed::detail
