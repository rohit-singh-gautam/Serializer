// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "payload_allocation.hpp"

#include <rohit/managed.hpp>
#include <rohit/managed_allocation.hpp>
#include <rohit/native_database.hpp>
#include <rohit/payload_decode.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <filesystem>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace serializer = rohit::serializer;
namespace managed = rohit::managed;
using payload_document = payload_graph::document;
using resource_type = managed::bounded_memory_resource;
constexpr std::size_t domain_bytes = 512 * 1024;
constexpr std::size_t text_bytes = 1024;

// Own one create-only native test directory and remove only its explicitly named artifacts.
struct payload_database_directory {
  std::filesystem::path directory;
  // Refuse an existing child rather than claiming ownership of unrelated files.
  payload_database_directory() {
    const auto identity = managed::make_document_id();
    directory = std::filesystem::temp_directory_path() / ("serializer-payload-database-" +
        std::to_string(identity.high) + "-" + std::to_string(identity.low));
    if (!std::filesystem::create_directory(directory)) { throw std::runtime_error{"Cannot create payload database fixture"}; }
  }
  // Native writer handles have ended before these exact owned paths are removed.
  ~payload_database_directory() {
    std::error_code ignored;
    std::filesystem::remove(directory / "state.sdb", ignored);
    std::filesystem::remove(directory / "state.sdb.writer.lock", ignored);
    std::filesystem::remove(directory, ignored);
  }
  // Select the fixture's only authoritative file.
  auto path() const { return directory / "state.sdb"; }
};
static_assert(std::same_as<decltype(payload_graph::child::label), serializer::payload_string>);
static_assert(std::same_as<decltype(payload_graph::child::notes),
    serializer::payload_vector<serializer::payload_string>>);

// Construct a graph exercising every supported owning wrapper under one retained domain.
payload_document sample(const std::shared_ptr<std::pmr::memory_resource>& resource) {
  serializer::payload_allocation_scope scope{resource};
  payload_document value{};
  value.children.emplace_back();
  value.children.back().label.assign(text_bytes, 'c');
  value.children.back().notes.emplace_back(text_bytes, 'n');
  value.children.back().lookup.try_emplace(serializer::payload_string(text_bytes, 'k'), text_bytes, 'v');
  value.children.back().subtitle.emplace(text_bytes, 's');
  value.ordinary.label.assign(text_bytes, 'o');
  value.selected.emplace();
  value.selected->label.assign(text_bytes, 'p');
  for (auto& text : value.fixed) { text.assign(text_bytes, 'f'); }
  value.by_number.try_emplace(7);
  value.by_number.at(7).label.assign(text_bytes, 'm');
  return value;
}

// Encode through the real generated traversal without including output-buffer bytes in the payload domain.
template <template <serializer::serialize_type> class Protocol>
std::string encode(const payload_document& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Decode from independent bytes into a fresh complete candidate under a different allocation domain.
template <template <serializer::serialize_type> class Protocol>
void exchange(const payload_document& value, const std::shared_ptr<std::pmr::memory_resource>& resource) {
  const auto bytes = encode<Protocol>(value);
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  const auto decoded = serializer::deserialize_payload_exact<payload_document, Protocol>(input, resource);
  EXPECT_TRUE(serializer::payload_resources_match(decoded, resource));
  EXPECT_EQ(encode<Protocol>(decoded), bytes);
  ASSERT_EQ(decoded.children.size(), value.children.size());
  EXPECT_EQ(decoded.children.front().lookup.begin()->first.size(), text_bytes);
  EXPECT_EQ(decoded.by_number.at(7).label.size(), text_bytes);
}
} // namespace

// Actual generated records propagate allocation ownership through strings, arrays, maps and optionals.
TEST(payload_allocation_generated, nested_graph_decode_and_scoped_clone_preserve_native_bytes) {
  auto first = std::make_shared<resource_type>(domain_bytes);
  auto second = std::make_shared<resource_type>(domain_bytes);
  {
    auto value = sample(first);
    EXPECT_TRUE(serializer::payload_resources_match(value, first));
    exchange<serializer::binary_none>(value, second);
    exchange<serializer::binary_integer>(value, second);
    const auto copy = serializer::clone_payload(value, second);
    EXPECT_TRUE(serializer::payload_resources_match(copy, second));
    EXPECT_EQ(encode<serializer::binary_integer>(copy), encode<serializer::binary_integer>(value));
    // Container-owned construction propagates its retained domain even outside the original scope.
    value.children.emplace_back();
    value.children.back().label.assign(text_bytes, 'x');
    value.by_number.try_emplace(8);
    value.by_number.at(8).label.assign(text_bytes, 'y');
    EXPECT_TRUE(serializer::payload_resources_match(value, first));
  }
  EXPECT_EQ(first->reserved_bytes(), 0U);
  EXPECT_EQ(second->reserved_bytes(), 0U);
}

// Complete-candidate validation detects independently moved graphs that retain another allocator domain.
TEST(payload_allocation_generated, foreign_wrapper_moves_require_an_explicit_clone) {
  auto first = std::make_shared<resource_type>(domain_bytes);
  auto second = std::make_shared<resource_type>(domain_bytes);
  auto value = sample(first);
  std::optional<payload_graph::child> external;
  {
    serializer::payload_allocation_scope scope{second};
    external.emplace();
    external->label.assign(text_bytes, 'e');
  }
  value.selected.reset();
  value.selected.emplace(std::move(*external));
  EXPECT_FALSE(serializer::payload_resources_match(value, first));
  const auto normalized = serializer::clone_payload(value, first);
  EXPECT_TRUE(serializer::payload_resources_match(normalized, first));
}

// A failed exact decode returns every temporary request while preserving the original published source.
TEST(payload_allocation_generated, decode_growth_obeys_a_real_hard_payload_bound) {
  auto source_resource = std::make_shared<resource_type>(domain_bytes);
  auto tiny_resource = std::make_shared<resource_type>(128);
  const auto value = sample(source_resource);
  const auto bytes = encode<serializer::binary_integer>(value);
  const auto source_reserved = source_resource->reserved_bytes();
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  EXPECT_THROW((void)(serializer::deserialize_payload_exact<payload_document, serializer::binary_integer>(
      input, tiny_resource)), std::bad_alloc);
  EXPECT_EQ(tiny_resource->reserved_bytes(), 0U);
  EXPECT_LE(tiny_resource->peak_reserved_bytes(), tiny_resource->maximum_bytes());
  EXPECT_EQ(source_resource->reserved_bytes(), source_reserved);
  EXPECT_EQ(encode<serializer::binary_integer>(value), bytes);
}

// Managed construction, callbacks and history must select the store's domain rather than only its control block.
TEST(payload_allocation_generated, managed_store_rebinds_and_restores_allocator_aware_payloads) {
  auto source_resource = std::make_shared<resource_type>(domain_bytes);
  auto store_resource = std::make_shared<resource_type>(domain_bytes);
  auto initial = sample(source_resource);
  managed::store_options options;
  options.allocation_resource = store_resource;
  managed::model_store<payload_document> store{std::move(initial), {701, 702}, options};
  EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
  const auto before = encode<serializer::binary_integer>(*store.read());
  const auto outcome = store.execute_transaction([&](auto& edit) {
    edit.update([&](auto& value) {
      value.selected.reset();
      value.selected.emplace();
      value.selected->label.assign(text_bytes, 'u');
      EXPECT_TRUE(serializer::payload_resources_match(value, store_resource));
    });
  });
  outcome.throw_if_failed();
  EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
  store.undo();
  EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
  EXPECT_EQ(encode<serializer::binary_integer>(*store.read()), before);
  store.redo();
  EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
  EXPECT_EQ(store.read()->selected->label.front(), 'u');
}

// Complete callback, runtime copying/reset and history scopes override an unrelated outer domain.
TEST(payload_allocation_generated, runtime_optional_and_durable_reset_use_the_store_resource) {
  auto source_resource = std::make_shared<resource_type>(domain_bytes);
  auto store_resource = std::make_shared<resource_type>(domain_bytes);
  auto foreign_resource = std::make_shared<resource_type>(domain_bytes);
  {
    serializer::payload_allocation_scope foreign_scope{foreign_resource};
    managed::store_options options;
    options.allocation_resource = store_resource;
    managed::model_store<payload_document> store{sample(source_resource), {801, 802}, options};
    store.update_runtime([&](auto& edit) {
      // Construction precedes edit.update, exercising the complete runtime callback binding.
      payload_graph::child local{};
      local.label.assign(text_bytes, 'r');
      EXPECT_TRUE(serializer::payload_resources_match(local, store_resource));
      edit.update([&](auto& value) { value.runtime_selected.emplace(std::move(local)); });
    }).throw_if_failed();
    const auto runtime_pin = store.read();
    ASSERT_TRUE(runtime_pin->runtime_selected.has_value());
    EXPECT_TRUE(serializer::payload_resources_match(*runtime_pin, store_resource));
    const auto detached = store.clone_value();
    EXPECT_TRUE(serializer::payload_resources_match(detached, store_resource));
    EXPECT_TRUE(detached.runtime_selected.has_value());
    store.update_runtime([&](auto& edit) {
      edit.update([&](auto& value) {
        ASSERT_TRUE(value.runtime_selected.has_value());
        EXPECT_TRUE(serializer::payload_resources_match(value, store_resource));
      });
    }).throw_if_failed();
    store.execute_transaction([&](auto& edit) {
      // The next persistent action begins from decoded/reset runtime defaults.
      payload_graph::child local{};
      local.label.assign(text_bytes, 'd');
      EXPECT_TRUE(serializer::payload_resources_match(local, store_resource));
      edit.update([&](auto& value) {
        EXPECT_FALSE(value.runtime_selected.has_value());
        value.ordinary = std::move(local);
      });
    }).throw_if_failed();
    EXPECT_FALSE(store.read()->runtime_selected.has_value());
    EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
    store.undo();
    EXPECT_FALSE(store.read()->runtime_selected.has_value());
    EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
    store.redo();
    EXPECT_FALSE(store.read()->runtime_selected.has_value());
    EXPECT_TRUE(serializer::payload_resources_match(*store.read(), store_resource));
    EXPECT_EQ(runtime_pin->runtime_selected->label.front(), 'r');
    EXPECT_EQ(foreign_resource->reserved_bytes(), 0U);
  }
  EXPECT_EQ(source_resource->reserved_bytes(), 0U);
  EXPECT_EQ(store_resource->reserved_bytes(), 0U);
  EXPECT_EQ(foreign_resource->reserved_bytes(), 0U);
}

// Identified preparation/recovery must not allocate temporary payload copies in an ambient foreign scope.
TEST(payload_allocation_generated, native_identified_preparation_and_recovery_use_the_store_domain) {
  auto source_resource = std::make_shared<resource_type>(domain_bytes);
  auto store_resource = std::make_shared<resource_type>(domain_bytes);
  auto foreign_resource = std::make_shared<resource_type>(domain_bytes);
  payload_database_directory directory;
  managed::database_options options;
  options.model.allocation_resource = store_resource;
  constexpr std::uint64_t epoch = 17;
  constexpr std::array<std::uint8_t, 1> request{1};
  {
    serializer::payload_allocation_scope foreign_scope{foreign_resource};
    managed::native_database<payload_document> database{directory.path(), sample(source_resource), {901, 902}, epoch, options};
    const auto initial_pin = database.read();
    auto prepared = database.prepare({3, 4}, epoch, 0, request, [&](auto& edit) {
      payload_graph::child local{};
      local.label.assign(text_bytes, 'b');
      EXPECT_TRUE(serializer::payload_resources_match(local, store_resource));
      edit.update([&](auto& value) { value.ordinary = std::move(local); });
    });
    EXPECT_TRUE(serializer::payload_resources_match(*prepared.before(), store_resource));
    EXPECT_TRUE(serializer::payload_resources_match(*prepared.after(), store_resource));
    EXPECT_EQ(foreign_resource->peak_reserved_bytes(), 0U);
    static_cast<void>(database.accept(std::move(prepared)));
    EXPECT_EQ(database.read()->ordinary.label.front(), 'b');
    EXPECT_EQ(initial_pin->ordinary.label.front(), 'o');
  }
  {
    serializer::payload_allocation_scope foreign_scope{foreign_resource};
    managed::native_database<payload_document> reopened{directory.path(), options};
    EXPECT_EQ(reopened.revision(), 1U);
    EXPECT_EQ(reopened.read()->ordinary.label.front(), 'b');
    EXPECT_TRUE(serializer::payload_resources_match(*reopened.read(), store_resource));
  }
  EXPECT_EQ(source_resource->reserved_bytes(), 0U);
  EXPECT_EQ(store_resource->reserved_bytes(), 0U);
  EXPECT_EQ(foreign_resource->reserved_bytes(), 0U);
  EXPECT_EQ(foreign_resource->peak_reserved_bytes(), 0U);
}
