// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/managed_snapshot.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace detail = rohit::managed::detail;
using snapshot = detail::compact_snapshot;
constexpr auto block_bytes = snapshot::chunk_bytes;

// Track actual PMR request lifetimes and permit deterministic failure at every preparation allocation.
class snapshot_resource final : public std::pmr::memory_resource {
  std::size_t outstanding_{};
  std::size_t calls_{};
  std::size_t fail_at_{std::numeric_limits<std::size_t>::max()};

  // Reject the chosen request before allocating; successful requests retain their exact byte charge.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (calls_++ == fail_at_) { throw std::bad_alloc{}; }
    auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    outstanding_ += bytes;
    return result;
  }
  // Release through the exact allocator domain even after every snapshot creator has returned.
  void do_deallocate(void* address, std::size_t bytes, std::size_t alignment) noexcept override {
    outstanding_ -= bytes;
    std::pmr::new_delete_resource()->deallocate(address, bytes, alignment);
  }
  // Distinct resource instances do not exchange ownership of their allocations.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

public:
  // Observe real outstanding requested bytes, separately from conservative admission charges.
  std::size_t outstanding_bytes() const noexcept { return outstanding_; }
  // Count requests to drive the complete allocation failure campaign.
  std::size_t allocation_calls() const noexcept { return calls_; }
  // Fail one subsequent zero-based request while leaving existing allocations untouched.
  void fail_after(std::size_t successful_requests) noexcept { fail_at_ = calls_ + successful_requests; }
};

// Construct a caller-selected resident admission domain without importing model store templates.
auto ledger(std::size_t maximum = std::numeric_limits<std::size_t>::max()) {
  return std::make_shared<detail::resident_ledger>(maximum);
}

// Produce distinct positional blocks, with explicit tail lengths and no generated schema assumptions.
std::vector<std::uint8_t> message(std::size_t bytes) {
  std::vector<std::uint8_t> result(bytes);
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = static_cast<std::uint8_t>(index / block_bytes + index % 251);
  }
  return result;
}
} // namespace

// Every boundary reconstructs exactly, including empty messages and a final partial block.
TEST(managed_snapshot, exact_materialization_and_empty_bounds) {
  for (const auto bytes : std::array{std::size_t{0}, std::size_t{1}, block_bytes - 1,
                                    block_bytes, block_bytes + 1, 3 * block_bytes + 7}) {
    const auto input = message(bytes);
    const auto value = snapshot::prepare(input);
    EXPECT_EQ(value.size(), bytes);
    EXPECT_EQ(value.empty(), input.empty());
    EXPECT_EQ(value.materialize(), input);
    EXPECT_EQ(value.chunk_count(), bytes / block_bytes + (bytes % block_bytes != 0 ? 1 : 0));
  }
  auto accounting = ledger(0);
  auto resource = std::make_shared<snapshot_resource>();
  const auto empty = snapshot::prepare({}, nullptr, {0, 0}, accounting, resource);
  EXPECT_EQ(empty.table_bytes(), 0U);
  EXPECT_EQ(resource->allocation_calls(), 0U);
  EXPECT_EQ(accounting->bytes(), 0U);
}

// Byte/count admission rejects before resource allocation or modifications to prior immutable bytes.
TEST(managed_snapshot, bounds_precede_all_allocations) {
  auto resource = std::make_shared<snapshot_resource>();
  auto accounting = ledger();
  const auto input = message(block_bytes + 1);
  EXPECT_THROW(snapshot::prepare(input, nullptr, {block_bytes, 2}, accounting, resource),
               std::length_error);
  EXPECT_THROW(snapshot::prepare(input, nullptr, {input.size(), 1}, accounting, resource),
               std::length_error);
  EXPECT_EQ(resource->allocation_calls(), 0U);
  EXPECT_EQ(accounting->bytes(), 0U);
}

// A one-byte update owns one new block; all untouched positions share exact immutable storage.
TEST(managed_snapshot, sharing_requires_exact_bytes_and_tail_length) {
  auto accounting = ledger();
  auto resource = std::make_shared<snapshot_resource>();
  auto input = message(2 * block_bytes + 7);
  const auto before = snapshot::prepare(input, nullptr, {}, accounting, resource);
  const auto equal = snapshot::prepare(input, &before, {}, accounting, resource);
  EXPECT_EQ(equal.shared_chunk_count(before), 3U);
  input[block_bytes + 11] ^= 1;
  const auto after = snapshot::prepare(input, &before, {}, accounting, resource);
  EXPECT_EQ(after.shared_chunk_count(before), 2U);
  EXPECT_EQ(after.materialize(), input);
  input.pop_back();
  const auto shorter = snapshot::prepare(input, &after, {}, accounting, resource);
  EXPECT_EQ(shorter.shared_chunk_count(after), 2U);
  EXPECT_EQ(shorter.materialize(), input);
}

// Equal bytes from another store's ledger or resource cannot bypass that store's resource admission.
TEST(managed_snapshot, different_domains_never_share_blocks) {
  const auto input = message(block_bytes + 3);
  auto first_ledger = ledger();
  auto second_ledger = ledger();
  auto first_resource = std::make_shared<snapshot_resource>();
  auto second_resource = std::make_shared<snapshot_resource>();
  const auto before = snapshot::prepare(input, nullptr, {}, first_ledger, first_resource);
  const auto other_ledger = snapshot::prepare(input, &before, {}, second_ledger, first_resource);
  const auto other_resource = snapshot::prepare(input, &before, {}, first_ledger, second_resource);
  EXPECT_EQ(other_ledger.shared_chunk_count(before), 0U);
  EXPECT_EQ(other_resource.shared_chunk_count(before), 0U);
  EXPECT_EQ(other_ledger.materialize(), input);
  EXPECT_EQ(other_resource.materialize(), input);
}

// A child table never retains its parent: obsolete table and replaced blocks release immediately.
TEST(managed_snapshot, pruning_releases_parent_tables_and_unshared_blocks) {
  auto accounting = ledger();
  auto resource = std::make_shared<snapshot_resource>();
  auto input = message(2 * block_bytes);
  auto before = snapshot::prepare(input, nullptr, {}, accounting, resource);
  auto copied = before;
  const auto original_charge = accounting->bytes();
  EXPECT_EQ(copied.table_bytes(), before.table_bytes());
  EXPECT_EQ(accounting->bytes(), original_charge);
  input.front() ^= 1;
  auto after = snapshot::prepare(input, &before, {}, accounting, resource);
  std::size_t chunk_charge{};
  before.visit_chunks([&](const void*, std::size_t length, std::size_t charge) {
    EXPECT_EQ(length, block_bytes);
    chunk_charge = charge;
  });
  const auto both_charge = accounting->bytes();
  const auto before_table = before.table_bytes();
  before = {};
  EXPECT_EQ(accounting->bytes(), both_charge);
  copied = {};
  EXPECT_EQ(accounting->bytes(), both_charge - before_table - chunk_charge);
  EXPECT_EQ(after.materialize(), input);
  after = {};
  EXPECT_EQ(accounting->bytes(), 0U);
  EXPECT_EQ(resource->outstanding_bytes(), 0U);
}

// Arbitrary encoded IDs/deletion bytes survive grow/shrink and old immutable revisions remain exact.
TEST(managed_snapshot, independent_states_preserve_replacement_and_deleted_bytes) {
  auto accounting = ledger();
  const auto baseline_bytes = message(3 * block_bytes + 13);
  auto baseline = snapshot::prepare(baseline_bytes, nullptr, {}, accounting);
  auto deleted_bytes = baseline_bytes;
  deleted_bytes.erase(deleted_bytes.begin() + 17, deleted_bytes.begin() + block_bytes + 39);
  auto deleted = snapshot::prepare(deleted_bytes, &baseline, {}, accounting);
  auto replacement_bytes = deleted_bytes;
  replacement_bytes.insert(replacement_bytes.begin() + 7, block_bytes + 1, 0xff);
  auto replacement = snapshot::prepare(replacement_bytes, &deleted, {}, accounting);
  EXPECT_EQ(baseline.materialize(), baseline_bytes);
  EXPECT_EQ(deleted.materialize(), deleted_bytes);
  baseline = {};
  deleted = {};
  EXPECT_EQ(replacement.materialize(), replacement_bytes);
  replacement = {};
  EXPECT_EQ(accounting->bytes(), 0U);
}

// Failure at every actual PMR allocation destroys only prospective blocks and returns all charges.
TEST(managed_snapshot, allocation_failures_preserve_the_previous_snapshot) {
  const auto input = message(2 * block_bytes + 1);
  std::size_t requests{};
  {
    auto resource = std::make_shared<snapshot_resource>();
    const auto prepared = snapshot::prepare(input, nullptr, {}, ledger(), resource);
    requests = resource->allocation_calls();
    EXPECT_GT(requests, 0U);
  }
  for (std::size_t failure = 0; failure < requests; ++failure) {
    auto resource = std::make_shared<snapshot_resource>();
    auto accounting = ledger();
    const auto before = snapshot::prepare(input, nullptr, {}, accounting, resource);
    const auto before_charge = accounting->bytes();
    const auto before_bytes = resource->outstanding_bytes();
    auto changed = input;
    for (std::size_t index = 0; index < changed.size(); index += block_bytes) { changed[index] ^= 1; }
    resource->fail_after(failure);
    EXPECT_THROW(snapshot::prepare(changed, &before, {}, accounting, resource), std::bad_alloc);
    EXPECT_EQ(before.materialize(), input);
    EXPECT_EQ(accounting->bytes(), before_charge);
    EXPECT_EQ(resource->outstanding_bytes(), before_bytes);
  }
}

// Ledger failure also releases private table/control/pointer allocations through their selected resource.
TEST(managed_snapshot, resident_admission_failure_rolls_back_private_storage) {
  auto resource = std::make_shared<snapshot_resource>();
  auto accounting = ledger(1);
  const auto input = message(1);
  EXPECT_THROW(snapshot::prepare(input, nullptr, {}, accounting, resource), std::length_error);
  EXPECT_EQ(accounting->bytes(), 0U);
  EXPECT_EQ(resource->outstanding_bytes(), 0U);
}

// Copies, swaps and final release use preconstructed handles even while the resource denies allocation.
TEST(managed_snapshot, prepared_handle_publication_requires_no_allocation) {
  static_assert(std::is_nothrow_copy_constructible_v<snapshot>);
  static_assert(std::is_nothrow_move_constructible_v<snapshot>);
  static_assert(std::is_nothrow_copy_assignable_v<snapshot>);
  static_assert(std::is_nothrow_move_assignable_v<snapshot>);
  static_assert(std::is_nothrow_swappable_v<snapshot>);
  static_assert(noexcept(std::declval<snapshot&>().swap(std::declval<snapshot&>())));
  auto resource = std::make_shared<snapshot_resource>();
  auto accounting = ledger();
  const auto input = message(block_bytes + 1);
  auto prepared = snapshot::prepare(input, nullptr, {}, accounting, resource);
  const auto requests = resource->allocation_calls();
  resource->fail_after(0);
  auto pin = prepared;
  snapshot selected;
  selected.swap(prepared);
  EXPECT_TRUE(prepared.empty());
  EXPECT_EQ(selected.materialize(), input);
  pin = {};
  selected = {};
  EXPECT_EQ(resource->allocation_calls(), requests);
  EXPECT_EQ(resource->outstanding_bytes(), 0U);
  EXPECT_EQ(accounting->bytes(), 0U);
}

// Shared allocator owners survive host handles; immutable tables may be finally released on another thread.
TEST(managed_snapshot, resource_lifetime_follows_shared_blocks) {
  std::weak_ptr<std::pmr::memory_resource> observer;
  auto accounting = ledger();
  snapshot retained;
  {
    auto resource = std::make_shared<snapshot_resource>();
    observer = resource;
    retained = snapshot::prepare(message(block_bytes + 1), nullptr, {}, accounting, resource);
  }
  EXPECT_FALSE(observer.expired());
  std::thread release{[value = std::move(retained)]() mutable { value = {}; }};
  release.join();
  EXPECT_TRUE(observer.expired());
  EXPECT_EQ(accounting->bytes(), 0U);
}
