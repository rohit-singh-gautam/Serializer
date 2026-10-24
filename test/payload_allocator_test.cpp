// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/payload_allocator.hpp>
#include <rohit/payload_decode.hpp>
#include <rohit/managed_allocation.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace serializer = rohit::serializer;
namespace managed = rohit::managed;
constexpr std::size_t domain_bytes = 256 * 1024;
constexpr std::size_t text_bytes = 1024;

// Represent the same implicit memberwise resource capture used by allocation-profile generated records.
struct payload_record {
  serializer::payload_allocation_binding binding{};
  serializer::payload_string label{};
  serializer::payload_vector<serializer::payload_string> labels{};
  // Borrow the domain ownership for allocator-aware runtime operations.
  const auto& serializer_payload_resource() const noexcept { return binding.resource(); }
  // Validate every retained owning container before a candidate can be published.
  bool serializer_payload_resources_match(const std::shared_ptr<std::pmr::memory_resource>& resource) const {
    return binding.resource().get() == resource.get() &&
        serializer::payload_resources_match(label, resource) &&
        serializer::payload_resources_match(labels, resource);
  }
};
} // namespace

// Nested standard construction propagates one explicit domain into strings and map keys/values.
TEST(payload_allocator, nested_containers_and_rebound_nodes_retain_the_domain) {
  auto resource = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  serializer::payload_vector<serializer::payload_string> values;
  serializer::payload_map<serializer::payload_string, serializer::payload_string> mapping;
  {
    serializer::payload_allocation_scope scope{resource};
    serializer::payload_vector<serializer::payload_string> scoped_values;
    scoped_values.emplace_back(text_bytes, 'x');
    serializer::payload_map<serializer::payload_string, serializer::payload_string> scoped_mapping;
    scoped_mapping.try_emplace(serializer::payload_string(text_bytes, 'k'), text_bytes, 'v');
    EXPECT_TRUE(serializer::payload_resources_match(scoped_values, resource));
    EXPECT_TRUE(serializer::payload_resources_match(scoped_mapping, resource));
    // Destination assignment retains its original domain; explicit scoped construction is required.
    values = std::move(scoped_values);
    mapping = std::move(scoped_mapping);
  }
  EXPECT_FALSE(serializer::payload_resources_match(values, resource));
  EXPECT_FALSE(serializer::payload_resources_match(mapping, resource));
  EXPECT_EQ(resource->reserved_bytes(), 0U);
}

// An owning value keeps the resource alive after both the binding scope and the host owner disappear.
TEST(payload_allocator, shared_resource_lifetime_follows_payload_storage) {
  std::optional<serializer::payload_vector<serializer::payload_string>> values;
  std::weak_ptr<std::pmr::memory_resource> observer;
  {
    auto resource = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
    observer = resource;
    serializer::payload_allocation_scope scope{resource};
    values.emplace();
    values->emplace_back(text_bytes, 'x');
  }
  EXPECT_FALSE(observer.expired());
  values.reset();
  EXPECT_TRUE(observer.expired());
}

// Scope restoration and copy selection are independent of the process PMR default resource.
TEST(payload_allocator, nested_scopes_restore_and_scoped_copies_rebind_records) {
  auto source = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  auto destination = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  std::optional<payload_record> record;
  auto* const process_default = std::pmr::get_default_resource();
  {
    serializer::payload_allocation_scope source_scope{source};
    record.emplace();
    record->label.assign(text_bytes, 'x');
    record->labels.emplace_back(text_bytes, 'y');
    {
      serializer::payload_allocation_scope destination_scope{destination};
      const auto copy = serializer::clone_payload(*record, destination);
      EXPECT_TRUE(serializer::payload_resources_match(copy, destination));
      EXPECT_TRUE(serializer::payload_resources_match(*record, source));
    }
    const serializer::payload_string restored(text_bytes, 'z');
    EXPECT_EQ(restored.get_allocator().resource().get(), source.get());
  }
  const auto retained = *record;
  EXPECT_TRUE(serializer::payload_resources_match(retained, source));
  EXPECT_EQ(std::pmr::get_default_resource(), process_default);
  EXPECT_EQ(destination->reserved_bytes(), 0U);
}

// Real outstanding allocation requests, including temporary growth, cannot exceed the resource's hard bound.
TEST(payload_allocator, failed_growth_and_overflow_preserve_bound_and_release_reservations) {
  auto resource = std::make_shared<managed::bounded_memory_resource>(4096);
  {
    serializer::payload_allocation_scope scope{resource};
    serializer::payload_vector<std::uint64_t> values;
    values.resize(128);
    const auto old_size = values.size();
    const auto old_capacity = values.capacity();
    EXPECT_THROW(values.resize(4096), std::bad_alloc);
    EXPECT_EQ(values.size(), old_size);
    EXPECT_EQ(values.capacity(), old_capacity);
    EXPECT_LE(resource->reserved_bytes(), resource->maximum_bytes());
    EXPECT_LE(resource->peak_reserved_bytes(), resource->maximum_bytes());
    serializer::payload_allocator<std::uint64_t> allocator{resource};
    EXPECT_THROW((void)allocator.allocate(std::numeric_limits<std::size_t>::max()), std::bad_array_new_length);
  }
  EXPECT_EQ(resource->reserved_bytes(), 0U);
}

// Independent thread-local bindings share no mutable allocator-selection state.
TEST(payload_allocator, parallel_domains_and_cross_thread_destruction_keep_ownership) {
  auto first = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  auto second = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  std::array<std::optional<serializer::payload_string>, 2> results;
  std::atomic<bool> valid{true};
  const auto construct = [&](std::size_t index, const auto& resource) {
    serializer::payload_allocation_scope scope{resource};
    results[index].emplace(text_bytes, 'x');
    if (results[index]->get_allocator().resource().get() != resource.get()) { valid = false; }
  };
  std::thread left{construct, 0, first};
  std::thread right{construct, 1, second};
  left.join();
  right.join();
  EXPECT_TRUE(valid.load());
  EXPECT_GT(first->reserved_bytes(), 0U);
  EXPECT_GT(second->reserved_bytes(), 0U);
  results = {};
  EXPECT_EQ(first->reserved_bytes(), 0U);
  EXPECT_EQ(second->reserved_bytes(), 0U);
}

// Real native decoder temporaries and node/key construction remain within the selected domain.
TEST(payload_allocator, positional_decoder_owns_nested_strings_and_map_keys) {
  const std::string vector_bytes{"\x02\x03one\x03two", 9};
  const std::string map_bytes{"\x02\x03one\x05" "first\x03two\x04last"};
  auto resource = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  {
    serializer::payload_allocation_scope scope{resource};
    auto vector_input = rohit::make_constant_full_stream(vector_bytes.data(), vector_bytes.size());
    const auto values = serializer::deserialize_payload_exact<
        serializer::payload_vector<serializer::payload_string>, serializer::binary_none>(vector_input, resource);
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0], "one");
    EXPECT_EQ(values[1], "two");
    EXPECT_TRUE(serializer::payload_resources_match(values, resource));
    auto map_input = rohit::make_constant_full_stream(map_bytes.data(), map_bytes.size());
    const auto mapping = serializer::deserialize_payload_exact<
        serializer::payload_map<serializer::payload_string, serializer::payload_string>, serializer::binary_none>(map_input, resource);
    ASSERT_EQ(mapping.size(), 2U);
    EXPECT_EQ(mapping.begin()->second, "first");
    EXPECT_TRUE(serializer::payload_resources_match(mapping, resource));
  }
  EXPECT_EQ(resource->reserved_bytes(), 0U);
}

// A hard allocator failure destroys the unpublished exact-decode candidate and restores every charge.
TEST(payload_allocator, failed_exact_decode_releases_all_candidate_allocations) {
  constexpr std::size_t tiny_domain_bytes = 64;
  const std::string bytes = std::string{"\x01\x41\x00", 3} + std::string(256, 'x');
  auto resource = std::make_shared<managed::bounded_memory_resource>(tiny_domain_bytes);
  serializer::payload_allocation_scope scope{resource};
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  EXPECT_THROW((void)(serializer::deserialize_payload_exact<
      serializer::payload_vector<serializer::payload_string>, serializer::binary_none>(input, resource)), std::bad_alloc);
  EXPECT_EQ(resource->reserved_bytes(), 0U);
  EXPECT_LE(resource->peak_reserved_bytes(), tiny_domain_bytes);
}

// Reusable nonempty map donors retain the destination allocator even under another outer scope.
TEST(payload_allocator, reusable_map_nodes_preserve_their_original_resource) {
  auto first = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  auto second = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  {
    serializer::payload_map<std::uint32_t, std::uint32_t> values{
        std::less<std::uint32_t>{}, serializer::payload_allocator<std::pair<const std::uint32_t, std::uint32_t>>{first}};
    values.emplace(1, 2);
    const std::string bytes{"\x01\x07\0\0\0\x09\0\0\0", 9};
    const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
    serializer::payload_allocation_scope scope{second};
    serializer::binary_none<serializer::serialize_type::in> reader{input};
    reader.serialize_in(values);
    reader.finish();
    ASSERT_EQ(values.size(), 1U);
    EXPECT_EQ(values.at(7), 9U);
    EXPECT_TRUE(serializer::payload_resources_match(values, first));
    EXPECT_EQ(second->reserved_bytes(), 0U);
  }
  EXPECT_EQ(first->reserved_bytes(), 0U);
}

// Moving allocator-aware maps must not strand implementation sentinels in a moved-from null domain.
TEST(payload_allocator, moved_from_allocators_and_map_sentinels_retain_the_resource) {
  auto resource = std::make_shared<managed::bounded_memory_resource>(domain_bytes);
  {
    serializer::payload_allocation_scope scope{resource};
    serializer::payload_allocator<std::uint32_t> source;
    auto target = std::move(source);
    EXPECT_EQ(source.resource().get(), resource.get());
    EXPECT_EQ(target.resource().get(), resource.get());
    serializer::payload_map<serializer::payload_string, serializer::payload_string> first;
    first.try_emplace(serializer::payload_string(text_bytes, 'k'), text_bytes, 'v');
    auto second = std::move(first);
    EXPECT_TRUE(serializer::payload_resources_match(first, resource));
    EXPECT_TRUE(serializer::payload_resources_match(second, resource));
    first.try_emplace(serializer::payload_string(text_bytes, 'n'), text_bytes, 'm');
  }
  EXPECT_EQ(resource->reserved_bytes(), 0U);
}
