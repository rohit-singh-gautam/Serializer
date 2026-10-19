// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/managed_identity_allocation.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

namespace {
constexpr auto native_maximum = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t maximum_leases{16U};
} // namespace

// An isolated preview advances only its copied cursor and cannot manufacture extra lease capacity.
TEST(managed_identity_allocation, cursor_copy_isolated_exact_base_and_exhaustion) {
  rohit::managed::identity_allocation_context live{100U, 51U, 100U, 98U};
  auto preview = live;
  EXPECT_EQ(preview.take(), 99U);
  EXPECT_EQ(preview.take(), 100U);
  EXPECT_FALSE(preview.available());
  EXPECT_THROW((void)preview.take(), std::overflow_error);
  EXPECT_EQ(live.consumed, 98U);
  EXPECT_NE(preview, live);
  EXPECT_NO_THROW(live.validate(native_maximum));
}

// Prepared ranges use the real native watermark and remain disjoint for concurrent participants.
TEST(managed_identity_allocation, range_ownership_native_bounds_and_durable_publication_order) {
  rohit::managed::detail::identity_lease_book book;
  constexpr std::array<std::uint8_t, 1> first_session{1U};
  constexpr std::array<std::uint8_t, 1> second_session{2U};
  auto first = book.prepare("test.session", first_session, 10U, 8U,
                            native_maximum, maximum_leases);
  EXPECT_TRUE(book.entries().empty());
  EXPECT_EQ(first.first, 11U);
  EXPECT_EQ(first.last, 18U);
  book.publish(std::move(first));
  auto second = book.prepare("test.session", second_session, 18U, 8U,
                             native_maximum, maximum_leases);
  EXPECT_EQ(second.first, 19U);
  EXPECT_EQ(second.last, 26U);
  book.publish(std::move(second));
  EXPECT_NO_THROW(book.validate(26U, native_maximum, maximum_leases));
  EXPECT_NE(book.find("test.session", first_session, 11U), nullptr);
  EXPECT_EQ(book.find("test.session", second_session, 11U), nullptr);
  EXPECT_EQ(book.find("wrong.session", first_session, 11U), nullptr);
  EXPECT_EQ(book.find("test.session", first_session, 27U), nullptr);
  EXPECT_THROW((void)book.prepare("test.session", first_session, native_maximum, 1U,
                                  native_maximum, maximum_leases), std::length_error);
}

// A checkpoint rejects overlapping owners and cursors outside their recorded native ranges.
TEST(managed_identity_allocation, malformed_contexts_and_overlapping_ledger_reject) {
  EXPECT_THROW((rohit::managed::identity_allocation_context{8U, 5U, 9U, 4U}.validate()),
               std::invalid_argument);
  EXPECT_THROW((rohit::managed::identity_allocation_context{8U, 0U, 0U, 1U}.validate()),
               std::invalid_argument);
  rohit::managed::records::identity_lease first;
  first.session_format = "test.session";
  first.session = {1U};
  first.first = 1U;
  first.last = 8U;
  first.committed = 0U;
  auto second = first;
  second.first = 8U;
  second.last = 9U;
  second.committed = 7U;
  const rohit::managed::detail::identity_lease_book book{{first, second}};
  EXPECT_THROW(book.validate(9U, native_maximum, maximum_leases), std::invalid_argument);
}
