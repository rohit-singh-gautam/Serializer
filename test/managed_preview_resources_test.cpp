// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_entity_references.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace managed = rohit::managed;
using root = entity_graph::document;
using node = entity_graph::node;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using store_type = managed::model_store<root>;
using id_type = store_type::id_type;
using token = managed::managed_ref<node, id_type>;
constexpr managed::document_id preview_document{1101U, 1102U};
constexpr std::size_t full_reservation_capacity = 64U;
constexpr std::size_t reservation_growth_count = full_reservation_capacity + 1U;

// An empty child collection keeps catalog admission independent of the reservation-count boundary.
root initial() {
  return {};
}

// Fail only the documented scoped allocation hook, accounting for every still-live holder.
class failing_resource final : public std::pmr::memory_resource {
public:
  bool reject{};
  std::size_t attempts{}, live_bytes{};
private:
  // Reject before obtaining storage so failed attempts cannot leave a resource-owned allocation.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    ++attempts;
    if (reject) {
      throw std::bad_alloc{};
    }
    auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    live_bytes += bytes;
    return result;
  }
  // Release through the same allocator and preserve an independent ownership balance.
  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    live_bytes -= bytes;
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
  }
  // Each deterministic fault resource has its own lifetime and failure switch.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
};

// Retain caller identity across implementation-permitted copies of an exception during rethrow.
struct validation_failure final : std::runtime_error {
  std::shared_ptr<const int> identity;
  // Keep an independent identity token alongside the original application diagnostic.
  explicit validation_failure(std::shared_ptr<const int> token)
      : std::runtime_error{"application rejected preview"}, identity{std::move(token)} {}
};

// Count actual configured native snapshot bytes without assuming an ID width or encoding policy.
std::size_t snapshot_bytes(const auto& value) {
  rohit::full_stream_auto_alloc output{};
  rohit::serializer::serialize_to<rohit::serializer::binary_integer>(output, value);
  return output.current_offset();
}

// Add one typed draft entity and retain its already-reserved identity for acceptance assertions.
id_type add_draft(auto& proposal) {
  id_type created{};
  proposal.working().execute_transaction([&](auto& edit) {
    node value{};
    value.label = std::string(128U, 'x');
    created = edit.root().nodes().append(std::move(value));
  }).throw_if_failed();
  return created;
}

// Measure admitted reservation growth while the candidate remains open, then discard its values.
std::size_t reservation_growth_bytes() {
  store_type store{initial(), preview_document};
  auto proposal = store.preview();
  managed::transaction_outcome outcome;
  auto edit = proposal.working().begin_transaction(outcome);
  for (std::size_t index = 0; index < reservation_growth_count; ++index) {
    static_cast<void>(edit.create_id());
  }
  const auto required = store.resident_bytes();
  edit.revert();
  return required;
}

// Exercise one complete accept and distinguish earlier setup limits from its final admission limit.
bool accepts_with_resident_cap(std::size_t maximum, bool verify) {
  managed::store_options options{};
  options.max_resident_bytes = maximum;
  std::size_t stage{};
  try {
    store_type store{initial(), preview_document, options};
    stage = 1U;
    const auto before = store.read();
    const auto state = store.state();
    auto proposal = store.preview();
    stage = 2U;
    const auto created = add_draft(proposal);
    stage = 3U;
    const auto resident = store.resident_bytes();
    const auto burned = store.allocated_id();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    const auto result = store.accept_preview(proposal);
    if (result.status == managed::transaction_status::committed) {
      if (verify) {
        EXPECT_FALSE(proposal.active());
        EXPECT_EQ(publications, 1U);
        EXPECT_EQ(store.registry().template find<node>(created).value().label.size(), 128U);
        EXPECT_EQ(store.allocated_id(), burned);
      }
      return true;
    }
    if (verify) {
      EXPECT_THROW(result.throw_if_failed(), std::length_error);
      EXPECT_TRUE(proposal.active());
      EXPECT_EQ(store.read(), before);
      EXPECT_EQ(store.state(), state);
      EXPECT_EQ(store.history_cursor(), 0U);
      EXPECT_EQ(store.allocated_id(), burned);
      EXPECT_EQ(store.resident_bytes(), resident);
      EXPECT_EQ(publications, 0U);
    }
    result.throw_if_failed();
    throw std::logic_error{"Resident admission control did not commit or fail"};
  } catch (const std::length_error& error) {
    if (std::string_view{error.what()} != "Managed resident admission budget exceeded") {
      throw;
    }
    if (verify) {
      EXPECT_EQ(stage, 3U) << "One-tight cap must reach the acceptance boundary";
    }
    return false;
  }
}
} // namespace

// A failed preview control-block allocation leaves the source and all accounting untouched.
TEST(managed_preview_resources, construction_allocation_failure_is_atomic_and_recoverable) {
  auto resource = std::make_shared<failing_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  {
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    const auto allocated = store.allocated_id();
    const auto resident = store.resident_bytes();
    const auto live_bytes = resource->live_bytes;
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    resource->reject = true;
    EXPECT_THROW(static_cast<void>(store.preview()), std::bad_alloc);
    resource->reject = false;
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(store.allocated_id(), allocated);
    EXPECT_EQ(store.resident_bytes(), resident);
    EXPECT_EQ(resource->live_bytes, live_bytes);
    EXPECT_EQ(publications, 0U);
    auto proposal = store.preview();
    EXPECT_TRUE(proposal.active());
    proposal.cancel();
    EXPECT_EQ(store.resident_bytes(), resident);
    EXPECT_EQ(resource->live_bytes, live_bytes);
  }
  EXPECT_EQ(resource->live_bytes, 0U);
}

// The measured preview metadata charge admits exactly and rolls back completely one byte below.
TEST(managed_preview_resources, construction_resident_exact_and_one_tight) {
  std::size_t required{};
  {
    store_type store{initial(), preview_document};
    auto proposal = store.preview();
    required = store.resident_bytes();
  }
  ASSERT_GT(required, 0U);
  for (const bool tight : {false, true}) {
    SCOPED_TRACE(tight);
    managed::store_options options{};
    options.max_resident_bytes = required - (tight ? 1U : 0U);
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    const auto resident = store.resident_bytes();
    const auto allocated = store.allocated_id();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    if (tight) {
      EXPECT_THROW(static_cast<void>(store.preview()), std::length_error);
    } else {
      auto proposal = store.preview();
      EXPECT_EQ(store.resident_bytes(), required);
      proposal.cancel();
    }
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(store.allocated_id(), allocated);
    EXPECT_EQ(store.resident_bytes(), resident);
    EXPECT_EQ(publications, 0U);
  }
}

// Reservation count grows through multiple capacities; its exact maximum rejects before exposure.
TEST(managed_preview_resources, reservation_collection_exact_one_tight_and_nonreuse) {
  // Both bounds admit the generated root's own fields before exercising reservation metadata.
  constexpr std::size_t requested = 8U;
  for (const auto maximum : {requested - 1U, requested}) {
    SCOPED_TRACE(maximum);
    managed::store_options options{};
    options.decode.max_collection_elements = maximum;
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    const auto allocated = store.allocated_id();
    const auto resident = store.resident_bytes();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    auto proposal = store.preview();
    for (std::size_t index = 0; index < requested; ++index) {
      id_type exposed{};
      const auto result = proposal.working().execute_transaction([&](auto& edit) {
        exposed = edit.create_id();
      });
      if (index < maximum) {
        result.throw_if_failed();
        EXPECT_EQ(exposed, allocated + index + 1U);
      } else {
        EXPECT_THROW(result.throw_if_failed(), std::length_error);
        EXPECT_EQ(exposed, 0U);
        EXPECT_EQ(store.allocated_id(), allocated + maximum);
      }
    }
    EXPECT_EQ(store.allocated_id(), allocated + maximum);
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(publications, 0U);
    proposal.cancel();
    EXPECT_EQ(store.resident_bytes(), resident);
    const auto burned = store.allocated_id();
    const auto next = store.reserve_preview_identity(store.allocation_context());
    EXPECT_GT(next.first, burned);
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(publications, 0U);
  }
}

// Growth admission precedes the next durable reservation even when earlier IDs are already burned.
TEST(managed_preview_resources, reservation_growth_resident_exact_and_one_tight) {
  const auto required = reservation_growth_bytes();
  ASSERT_GT(required, 0U);
  for (const bool tight : {false, true}) {
    SCOPED_TRACE(tight);
    managed::store_options options{};
    options.max_resident_bytes = required - (tight ? 1U : 0U);
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    const auto resident = store.resident_bytes();
    const auto allocated = store.allocated_id();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    auto proposal = store.preview();
    {
      const auto preview_charge = store.resident_bytes();
      managed::transaction_outcome outcome;
      auto edit = proposal.working().begin_transaction(outcome);
      const auto candidate_charge = store.resident_bytes() - preview_charge;
      for (std::size_t index = 0; index < full_reservation_capacity; ++index) {
        static_cast<void>(edit.create_id());
      }
      const auto previous_charge = store.resident_bytes();
      id_type exposed{};
      if (tight) {
        EXPECT_THROW(exposed = edit.create_id(), std::length_error);
        EXPECT_EQ(exposed, 0U);
        // Poisoning releases the private candidate immediately; reservation metadata stays unchanged.
        EXPECT_EQ(store.resident_bytes(), previous_charge - candidate_charge);
      } else {
        exposed = edit.create_id();
        EXPECT_EQ(exposed, allocated + reservation_growth_count);
        EXPECT_EQ(store.resident_bytes(), required);
        edit.revert();
      }
    }
    EXPECT_EQ(store.allocated_id(), allocated +
        (tight ? full_reservation_capacity : reservation_growth_count));
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(publications, 0U);
    proposal.cancel();
    EXPECT_EQ(store.resident_bytes(), resident);
    const auto burned = store.allocated_id();
    EXPECT_GT(store.reserve_preview_identity(store.allocation_context()).first, burned);
  }
}

// Failed draft values cannot turn a copied typed token into a later live entity after cancellation.
TEST(managed_preview_resources, failed_draft_token_never_aliases_live_entities) {
  store_type store{initial(), preview_document};
  const auto before = store.read();
  const auto state = store.state();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  auto proposal = store.preview();
  const auto draft_before = proposal.working().read();
  token copied;
  id_type failed_id{};
  const auto result = proposal.working().execute_transaction([&](auto& edit) {
    failed_id = edit.root().nodes().append(node{});
    copied = edit.registry().template reference<node>(failed_id);
    throw std::runtime_error{"discard draft values"};
  });
  EXPECT_THROW(result.throw_if_failed(), std::runtime_error);
  EXPECT_EQ(proposal.working().read(), draft_before);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.history_cursor(), 0U);
  EXPECT_EQ(publications, 0U);
  EXPECT_EQ(store.allocated_id(), failed_id);
  EXPECT_THROW(store.registry().resolve(copied), managed::registry_error);
  proposal.cancel();
  id_type accepted{};
  store.execute_transaction([&](auto& edit) {
    accepted = edit.root().nodes().append(node{});
  }).throw_if_failed();
  EXPECT_GT(accepted, failed_id);
  EXPECT_EQ(publications, 1U);
  EXPECT_THROW(store.registry().resolve(copied), managed::registry_error);
  store_type recovered{initial()};
  recovered.load(store.save());
  EXPECT_EQ(recovered.allocated_id(), store.allocated_id());
  EXPECT_THROW(recovered.registry().resolve(copied), managed::registry_error);
}

// Automatic snapshot admission discards draft values while retaining every identity already exposed.
TEST(managed_preview_resources, draft_snapshot_limit_preserves_live_state_and_burned_token) {
  std::size_t required{};
  {
    store_type baseline{initial(), preview_document};
    required = snapshot_bytes(*baseline.read());
  }
  managed::store_options options{};
  options.max_snapshot_bytes = required;
  store_type store{initial(), preview_document, options};
  const auto before = store.read();
  const auto state = store.state();
  const auto resident = store.resident_bytes();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  auto proposal = store.preview();
  const auto draft_before = proposal.working().read();
  token copied;
  id_type failed_id{};
  const auto result = proposal.working().execute_transaction([&](auto& edit) {
    failed_id = edit.root().nodes().append(node{});
    copied = edit.registry().template reference<node>(failed_id);
  });
  EXPECT_THROW(result.throw_if_failed(), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(proposal.working().read(), draft_before);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.history_cursor(), 0U);
  EXPECT_EQ(store.allocated_id(), failed_id);
  EXPECT_EQ(publications, 0U);
  EXPECT_THROW(store.registry().resolve(copied), managed::registry_error);
  proposal.cancel();
  EXPECT_EQ(store.resident_bytes(), resident);
  EXPECT_GT(store.reserve_preview_identity(store.allocation_context()).first, failed_id);
  store_type recovered{initial()};
  recovered.load(store.save());
  EXPECT_EQ(recovered.allocated_id(), store.allocated_id());
  EXPECT_THROW(recovered.registry().resolve(copied), managed::registry_error);
}

// Accepting a valid draft still needs a separate live candidate and releases it on allocation failure.
TEST(managed_preview_resources, acceptance_allocation_failure_retains_retryable_exact_ids) {
  auto resource = std::make_shared<failing_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  {
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    auto proposal = store.preview();
    const auto created = add_draft(proposal);
    const auto burned = store.allocated_id();
    const auto resident = store.resident_bytes();
    const auto live_bytes = resource->live_bytes;
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    resource->reject = true;
    const auto result = store.accept_preview(proposal);
    resource->reject = false;
    EXPECT_THROW(result.throw_if_failed(), std::bad_alloc);
    EXPECT_TRUE(proposal.active());
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(store.allocated_id(), burned);
    EXPECT_EQ(store.resident_bytes(), resident);
    EXPECT_EQ(resource->live_bytes, live_bytes);
    EXPECT_EQ(publications, 0U);
    store.accept_preview(proposal).throw_if_failed();
    EXPECT_FALSE(proposal.active());
    EXPECT_EQ(publications, 1U);
    EXPECT_EQ(store.registry().template find<node>(created).id(), created);
    store.undo();
    EXPECT_FALSE(store.registry().contains(created));
    store.redo();
    EXPECT_TRUE(store.registry().contains(created));
  }
  EXPECT_EQ(resource->live_bytes, 0U);
}

// Application rejection preserves the original exception and leaves the exact proposal available.
TEST(managed_preview_resources, acceptance_validation_preserves_exception_and_proposal) {
  const auto identity = std::make_shared<const int>(7);
  const auto sentinel = std::make_exception_ptr(validation_failure{identity});
  bool reject{};
  store_type store{initial(), preview_document, {}, [&](const auto&) {
    if (reject) {
      std::rethrow_exception(sentinel);
    }
  }};
  const auto before = store.read();
  const auto state = store.state();
  auto proposal = store.preview();
  const auto created = add_draft(proposal);
  const auto resident = store.resident_bytes();
  const auto burned = store.allocated_id();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  reject = true;
  const auto result = store.accept_preview(proposal);
  reject = false;
  try {
    result.throw_if_failed();
    FAIL() << "Application sentinel was swallowed";
  } catch (const validation_failure& error) {
    EXPECT_EQ(error.identity, identity);
    EXPECT_STREQ(error.what(), "application rejected preview");
  }
  EXPECT_TRUE(proposal.active());
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.history_cursor(), 0U);
  EXPECT_EQ(store.allocated_id(), burned);
  EXPECT_EQ(store.resident_bytes(), resident);
  EXPECT_EQ(publications, 0U);
  store.accept_preview(proposal).throw_if_failed();
  EXPECT_EQ(store.registry().template find<node>(created).id(), created);
  EXPECT_EQ(publications, 1U);
}

// A history-free draft can exceed the live history cap; only exact admission may publish it.
TEST(managed_preview_resources, acceptance_history_exact_and_one_tight) {
  std::size_t required{};
  {
    store_type store{initial(), preview_document};
    auto proposal = store.preview();
    static_cast<void>(add_draft(proposal));
    required = snapshot_bytes(*proposal.working().read());
  }
  ASSERT_GT(required, 0U);
  for (const bool tight : {false, true}) {
    SCOPED_TRACE(tight);
    managed::store_options options{};
    options.max_history_bytes = required - (tight ? 1U : 0U);
    store_type store{initial(), preview_document, options};
    const auto before = store.read();
    const auto state = store.state();
    auto proposal = store.preview();
    const auto created = add_draft(proposal);
    const auto burned = store.allocated_id();
    const auto resident = store.resident_bytes();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    const auto result = store.accept_preview(proposal);
    if (tight) {
      EXPECT_THROW(result.throw_if_failed(), std::length_error);
      EXPECT_TRUE(proposal.active());
      EXPECT_EQ(store.read(), before);
      EXPECT_EQ(store.state(), state);
      EXPECT_EQ(store.history_cursor(), 0U);
      EXPECT_EQ(store.resident_bytes(), resident);
      EXPECT_EQ(publications, 0U);
    } else {
      result.throw_if_failed();
      EXPECT_FALSE(proposal.active());
      EXPECT_EQ(store.registry().template find<node>(created).id(), created);
      EXPECT_EQ(publications, 1U);
    }
    EXPECT_EQ(store.allocated_id(), burned);
  }
}

// Locate the whole-operation admitted minimum, then prove exact success and one-byte rejection.
TEST(managed_preview_resources, acceptance_resident_exact_and_one_tight) {
  std::size_t upper = 1U;
  while (!accepts_with_resident_cap(upper, false)) {
    ASSERT_LE(upper, std::numeric_limits<std::size_t>::max() / 2U);
    upper *= 2U;
  }
  std::size_t lower{};
  while (upper - lower > 1U) {
    const auto middle = lower + (upper - lower) / 2U;
    if (accepts_with_resident_cap(middle, false)) {
      upper = middle;
    } else {
      lower = middle;
    }
  }
  EXPECT_TRUE(accepts_with_resident_cap(upper, true));
  EXPECT_FALSE(accepts_with_resident_cap(upper - 1U, true));
}
