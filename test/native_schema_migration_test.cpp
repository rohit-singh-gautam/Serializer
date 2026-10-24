// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <native_schema_new.hpp>
#include <native_schema_old.hpp>
#include <native_schema_retyped.hpp>
#include <rohit/native_database.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using old_root = native_schema_old::document;
using new_root = native_schema_new::workspace;
using retyped_root = native_schema_retyped::workspace;
using old_traits = managed::model_traits<old_root>;
using new_traits = managed::model_traits<new_root>;
using old_database = managed::native_database<old_root>;
using new_database = managed::native_database<new_root>;
constexpr managed::document_id document_namespace{81, 93};
constexpr std::uint64_t source_epoch = 47;
constexpr std::array<std::uint8_t, 3> request{4, 7, 1};

// Isolate crash-safe generation artifacts in one uniquely owned temporary directory.
class migration_directory {
  std::filesystem::path directory_{};
public:
  // Create a fresh directory so a stale file cannot accidentally satisfy an assertion.
  migration_directory() {
    const auto id = managed::make_document_id();
    directory_ = std::filesystem::temp_directory_path() /
        ("serializer-native-schema-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    if (!std::filesystem::create_directory(directory_)) {
      throw std::runtime_error{"Cannot create schema migration test directory"};
    }
  }
  // Remove only this fixture's directory, after its database owners have been destroyed.
  ~migration_directory() {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }
  // Return the named current generation within the fixture's owned directory.
  std::filesystem::path current() const { return directory_ / "current.sdb"; }
  // Return a create-only archive distinct from the current generation.
  std::filesystem::path archive() const { return directory_ / "original.sdb"; }
};

// Read exact durable bytes to prove rejected conversions perform no writes.
std::vector<char> read_bytes(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot read schema migration fixture"}; }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Construct the independently generated old layout before its identities are assigned.
old_root initial() {
  old_root value{};
  value.name = "Original";
  value.items.resize(2);
  value.items[0].amount = 100;
  value.items[0].memo = "First";
  value.items[1].amount = 200;
  value.items[1].memo = "Second";
  return value;
}

// Map renamed fields, newly introduced values and typed references while preserving each live ID.
template <typename DestinationRoot = new_root>
auto convert(const old_traits::storage_type& source) {
  using destination_traits = managed::model_traits<DestinationRoot>;
  using destination_line = std::conditional_t<std::same_as<DestinationRoot, new_root>,
      native_schema_new::line, native_schema_retyped::line>;
  typename destination_traits::storage_type result{};
  result.persistent_id = source.persistent_id;
  const auto& before = old_traits::payload(source);
  auto& after = destination_traits::payload(result);
  after.title = before.name;
  after.schema_revision = 2;
  after.lines.resize(before.items.size());
  for (std::size_t index = 0; index < before.items.size(); ++index) {
    const auto& previous = before.items[index];
    auto& next = after.lines[index];
    next.persistent_id = previous.persistent_id;
    const auto& old_value = managed::model_traits<native_schema_old::item>::payload(previous);
    auto& new_value = managed::model_traits<destination_line>::payload(next);
    new_value.amount_minor_units = old_value.amount;
    new_value.note = old_value.memo;
    new_value.currency = "USD";
  }
  if (before.selected) {
    // Mint the new exact C++ token through checked destination membership, never a private ctor.
    const auto pin = std::make_shared<const typename destination_traits::storage_type>(result);
    const managed::registry_view<DestinationRoot> registry{pin, {}, {}, before.selected->document()};
    after.selected = registry.template reference<destination_line>(before.selected->id());
  }
  return result;
}

// Bind a reference after IDs exist and retain one durable operation for archive retry checks.
void accept_source_edit(old_database& source) {
  const auto& payload = old_traits::payload(*source.read());
  const auto selected = source.snapshot().registry().template reference<native_schema_old::item>(
      payload.items.back().persistent_id);
  auto prepared = source.prepare({7, 1}, source_epoch, 0, request, [&](auto& transaction) {
    transaction.update([&](auto& candidate) {
      old_traits::payload(candidate).selected = selected;
    });
  });
  source.accept(std::move(prepared));
}
} // namespace

// Different generated namespaces, class names and payload layouts retain stable class/instance IDs.
TEST(native_schema_migration, renamed_layout_references_reopen_and_archive_retry) {
  migration_directory directory;
  std::optional<old_database::snapshot_type> pinned;
  old_traits::id_type selected_id{};
  {
    old_database source{directory.current(), initial(), document_namespace, source_epoch};
    accept_source_edit(source);
    pinned.emplace(source.snapshot());
    selected_id = old_traits::payload(*pinned->read()).selected->id();
    const auto source_bytes = read_bytes(directory.current());
    auto destination = source.migrate<new_root>(source_epoch + 1, directory.archive(),
        [](const auto& value) { return convert(value); });
    EXPECT_TRUE(source.retired());
    EXPECT_EQ(destination->document(), document_namespace);
    EXPECT_EQ(destination->epoch(), source_epoch + 1);
    EXPECT_EQ(destination->revision(), 0U);
    EXPECT_EQ(destination->allocated_id(), pinned->allocated_id());
    EXPECT_EQ(read_bytes(directory.archive()), source_bytes);
    const auto& payload = new_traits::payload(*destination->read());
    EXPECT_EQ(payload.title, "Original");
    EXPECT_EQ(payload.schema_revision, 2U);
    EXPECT_EQ(payload.lines.size(), 2U);
    ASSERT_TRUE(payload.selected);
    EXPECT_EQ(payload.selected->document(), document_namespace);
    EXPECT_EQ(payload.selected->id(), selected_id);
    const auto snapshot = destination->snapshot();
    const auto& registry = snapshot.registry();
    EXPECT_NE(registry.type(selected_id), pinned->registry().type(selected_id));
    EXPECT_EQ(registry.class_id(selected_id), pinned->registry().class_id(selected_id));
    const auto selected = registry.resolve(*payload.selected);
    const auto& selected_value = selected.value();
    EXPECT_EQ(selected_value.amount_minor_units, 200U);
    EXPECT_EQ(selected_value.note, "Second");
    EXPECT_EQ(selected_value.currency, "USD");
    EXPECT_THROW(destination->prepare({7, 1}, source_epoch, 0, request, [](auto&) {}),
        std::invalid_argument);
  }
  EXPECT_THROW(old_database(directory.current()), std::invalid_argument);
  new_database reopened{directory.current()};
  EXPECT_EQ(new_traits::payload(*reopened.read()).selected->id(), selected_id);
  EXPECT_EQ(new_traits::payload(*reopened.read()).schema_revision, 2U);
  old_database archive{directory.archive()};
  bool called{};
  auto retry = archive.prepare({7, 1}, source_epoch, 0, request, [&](auto&) { called = true; });
  EXPECT_EQ(archive.accept(std::move(retry)).revision, 1U);
  EXPECT_FALSE(called);
  EXPECT_EQ(old_traits::payload(*pinned->read()).items.size(), 2U);
}

// Reusing an instance ID for another explicit class is forbidden even during an explicit conversion.
TEST(native_schema_migration, retyped_child_rejects_before_archive_or_current_write) {
  migration_directory directory;
  old_database source{directory.current(), initial(), document_namespace, source_epoch};
  accept_source_edit(source);
  const auto before = source.read();
  const auto bytes = read_bytes(directory.current());
  EXPECT_THROW(source.migrate<retyped_root>(source_epoch + 1, directory.archive(),
      [](const auto& value) { return convert<retyped_root>(value); }), std::invalid_argument);
  EXPECT_EQ(source.read(), before);
  EXPECT_EQ(read_bytes(directory.current()), bytes);
  EXPECT_FALSE(std::filesystem::exists(directory.archive()));
  EXPECT_FALSE(source.retired());
  EXPECT_FALSE(source.needs_recovery());
}

// Destination validation and entity removal fail transactionally before publishing new bytes.
TEST(native_schema_migration, missing_identity_or_rejected_new_invariant_keeps_source_writable) {
  migration_directory directory;
  old_database source{directory.current(), initial(), document_namespace, source_epoch};
  const auto bytes = read_bytes(directory.current());
  EXPECT_THROW(source.migrate<new_root>(source_epoch + 1, directory.archive(), [](const auto& value) {
    auto result = convert(value);
    new_traits::payload(result).lines.erase(new_traits::payload(result).lines.begin());
    return result;
  }), std::invalid_argument);
  EXPECT_THROW(source.migrate<new_root>(source_epoch + 1, directory.archive(),
      [](const auto& value) { return convert(value); }, [](const auto&) {
        throw std::runtime_error{"Destination application invariant"};
      }), std::runtime_error);
  EXPECT_EQ(read_bytes(directory.current()), bytes);
  EXPECT_FALSE(std::filesystem::exists(directory.archive()));
  EXPECT_FALSE(source.retired());
  accept_source_edit(source);
  EXPECT_EQ(source.revision(), 1U);
}
