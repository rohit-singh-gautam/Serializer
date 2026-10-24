// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <compact_history.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using graph = compact_history_example::graph;
using leaf = compact_history_example::leaf;
using section = compact_history_example::section;
using linear_store = managed::model_store<graph>;
using tree_store = managed::model_store<graph, managed::history_mode::tree>;
constexpr std::size_t leaf_count = 64;
constexpr std::size_t label_bytes = 256;
constexpr std::size_t retained_edits = 12;

// Borrow the same public payload from direct or explicitly separated generated identity storage.
const auto& payload(const auto& value) {
  if constexpr (requires { value.value; }) { return value.value; } else { return value; }
}

// Build a nested graph whose unchanged durable bytes occupy several independent compact blocks.
graph initial() {
  graph result{};
  result.title = "baseline";
  result.sections.resize(1);
  result.sections.front().title = "section";
  result.sections.front().leaves.resize(leaf_count);
  for (std::size_t index = 0; index < leaf_count; ++index) {
    result.sections.front().leaves[index].label.assign(label_bytes, 'x');
    result.sections.front().leaves[index].count = static_cast<std::int64_t>(index);
  }
  return result;
}

// Select the opt-in internal retention representation while preserving normal history admission limits.
managed::store_options options(bool compact = true) {
  managed::store_options result;
  result.compact_history = compact;
  result.max_revisions = retained_edits + 8;
  return result;
}

// Read a nested identity without relying on uint32/uint64 or direct/separated field layout.
auto first_leaf_id(const auto& store) {
  return payload(payload(*store.read()).sections.front()).leaves.front().persistent_id;
}

// Perform one checked leaf edit through generated registry/editor dispatch.
void set_count(auto& store, typename std::remove_reference_t<decltype(store)>::id_type id,
               std::int64_t value) {
  store.execute_transaction([&](auto& edit) {
    edit.registry().template edit<leaf>(id).set_count(value);
  }).throw_if_failed();
}

// Fail PMR requests deterministically; counters represent actual requested domain bytes.
class history_resource final : public std::pmr::memory_resource {
  std::size_t bytes_{};
  std::size_t calls_{};
  std::size_t failure_{std::numeric_limits<std::size_t>::max()};
  bool denied_{};
  // Reject before requesting storage from the upstream resource.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (calls_++ == failure_ || denied_) { throw std::bad_alloc{}; }
    auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    bytes_ += bytes;
    return result;
  }
  // Deallocation is always permitted, including rollback and post-flush publication.
  void do_deallocate(void* address, std::size_t bytes, std::size_t alignment) noexcept override {
    bytes_ -= bytes;
    std::pmr::new_delete_resource()->deallocate(address, bytes, alignment);
  }
  // Physical resource identity determines valid ownership transfer.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
public:
  // Observe the number of attempted allocation calls.
  std::size_t calls() const noexcept { return calls_; }
  // Observe actual outstanding domain requests after a candidate is destroyed.
  std::size_t reserved_bytes() const noexcept { return bytes_; }
  // Reject one selected subsequent allocation and leave already published state untouched.
  void fail_after(std::size_t count) noexcept { failure_ = calls_ + count; }
  // Reenable normal resource requests after a deliberately injected failure.
  void allow() noexcept { denied_ = false; failure_ = std::numeric_limits<std::size_t>::max(); }
  // Disable every future PMR allocation at the journal's acknowledged flush boundary.
  void deny() noexcept { denied_ = true; }
};

// Own an exclusively created OS-temporary directory; never claim or replace a preexisting location.
struct history_directory {
  std::filesystem::path directory;
  // Create one unique named fixture root before constructing a journal path beneath it.
  history_directory() {
    const auto id = managed::make_document_id();
    directory = std::filesystem::temp_directory_path() /
        ("serializer-compact-history-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    if (!std::filesystem::create_directory(directory)) {
      throw std::runtime_error{"Cannot create compact history fixture directory"};
    }
  }
  // Only files created under this verified, exclusively owned directory are removed after handles close.
  ~history_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }
  // Return the test-owned native appended-journal path.
  auto path() const { return directory / "state.srj"; }
};
} // namespace

// Internal compaction preserves existing Save bytes and linear navigation across all generated profiles.
TEST(managed_compact_history, default_and_compact_exports_match_exactly) {
  linear_store ordinary{initial(), {41, 42}, options(false)};
  linear_store compact{initial(), {41, 42}, options()};
  const auto id = first_leaf_id(compact);
  EXPECT_EQ(ordinary.save(), compact.save());
  for (std::size_t index = 0; index < retained_edits; ++index) {
    set_count(ordinary, id, static_cast<std::int64_t>(index + 100));
    set_count(compact, id, static_cast<std::int64_t>(index + 100));
    EXPECT_EQ(ordinary.save(), compact.save());
  }
  EXPECT_LT(compact.resident_bytes(), ordinary.resident_bytes());
  for (std::size_t index = 0; index < retained_edits; ++index) {
    ordinary.undo(); compact.undo();
    EXPECT_EQ(ordinary.save(), compact.save());
  }
  for (std::size_t index = 0; index < retained_edits; ++index) {
    ordinary.redo(); compact.redo();
    EXPECT_EQ(ordinary.save(), compact.save());
  }
  linear_store loaded{initial(), {51, 52}, options()};
  loaded.load(ordinary.save());
  EXPECT_EQ(loaded.save(), compact.save());
}

// Deleted nested payloads restore their original IDs, while new accepted lifetimes never reuse them.
TEST(managed_compact_history, nested_delete_restore_and_new_identity_nonreuse) {
  linear_store store{initial(), {41, 42}, options()};
  const auto pin = store.read();
  const auto id = first_leaf_id(store);
  const auto owner = payload(*pin).sections.front().persistent_id;
  const auto highwater = store.allocated_id();
  store.execute_transaction([&](auto& edit) {
    edit.registry().template edit<section>(owner).leaves().erase(id);
  }).throw_if_failed();
  EXPECT_EQ(payload(payload(*store.read()).sections.front()).leaves.size(), leaf_count - 1);
  store.undo();
  EXPECT_EQ(first_leaf_id(store), id);
  store.redo();
  store.execute_transaction([&](auto& edit) {
    edit.registry().template edit<section>(owner).leaves().append(leaf{});
  }).throw_if_failed();
  EXPECT_GT(payload(payload(*store.read()).sections.front()).leaves.back().persistent_id, highwater);
  EXPECT_EQ(payload(payload(*pin).sections.front()).leaves.front().persistent_id, id);
  EXPECT_EQ(payload(payload(*pin).sections.front()).leaves.size(), leaf_count);
  const auto saved = store.save();
  linear_store restored{initial(), {51, 52}, options()};
  restored.load(saved);
  EXPECT_EQ(restored.save(), saved);
  restored.undo();
  restored.undo();
  EXPECT_EQ(first_leaf_id(restored), id);
}

// Tree branches and merge ancestry export full legacy records and remain independently reconstructible.
TEST(managed_compact_history, tree_branches_merge_and_reload_preserve_ancestry) {
  tree_store store{initial(), {41, 42}, options()};
  const auto base = store.current_revision();
  const auto id = first_leaf_id(store);
  set_count(store, id, 71);
  const auto source = store.current_revision();
  store.checkout(base);
  store.execute_transaction([](auto& edit) { edit.root().set_title("target"); }).throw_if_failed();
  const auto target = store.current_revision();
  const auto merged = store.merge_revision(source);
  merged.throw_if_failed();
  EXPECT_EQ(store.revision_parents(merged.revision), (std::vector<std::uint64_t>{target, source}));
  EXPECT_EQ(payload(*store.read()).title, "target");
  EXPECT_EQ(payload(payload(payload(*store.read()).sections.front()).leaves.front()).count, 71);
  const auto saved = store.save();
  tree_store restored{initial(), {51, 52}, options()};
  restored.load(saved);
  EXPECT_EQ(restored.save(), saved);
  restored.undo();
  EXPECT_EQ(restored.current_revision(), target);
  restored.redo(merged.revision);
  EXPECT_EQ(restored.save(), saved);
  restored.checkout(source);
  EXPECT_EQ(payload(*restored.read()).title, "baseline");
}

// Reset prunes history block ownership while independently pinned root values remain immutable and charged.
TEST(managed_compact_history, pruning_and_runtime_reset_preserve_reader_pins) {
  linear_store store{initial(), {41, 42}, options()};
  const auto id = first_leaf_id(store);
  store.update_runtime([](auto& edit) {
    edit.update([](auto& value) {
      using traits = managed::model_traits<graph>;
      traits::payload(value).runtime_note.emplace("memory only");
    });
  }).throw_if_failed();
  auto pin = store.read();
  ASSERT_TRUE(payload(*pin).runtime_note.has_value());
  for (std::size_t index = 0; index < retained_edits; ++index) {
    set_count(store, id, static_cast<std::int64_t>(index + 10));
  }
  EXPECT_FALSE(payload(*store.read()).runtime_note.has_value());
  EXPECT_EQ(*payload(*pin).runtime_note, "memory only");
  const auto retained = store.resident_bytes();
  store.reset_history();
  EXPECT_LT(store.resident_bytes(), retained);
  EXPECT_THROW(store.undo(), std::out_of_range);
  const auto pinned = store.resident_bytes();
  pin.reset();
  EXPECT_LT(store.resident_bytes(), pinned);
  EXPECT_EQ(first_leaf_id(store), id);
}

// Every opt-in PMR allocation failure leaves accepted bytes, history selection and resource counts intact.
TEST(managed_compact_history, preparation_allocation_failures_roll_back_atomically) {
  std::size_t allocations{};
  {
    auto resource = std::make_shared<history_resource>();
    auto selected = options(); selected.allocation_resource = resource;
    linear_store store{initial(), {41, 42}, selected};
    const auto before = resource->calls();
    set_count(store, first_leaf_id(store), 100);
    allocations = resource->calls() - before;
  }
  ASSERT_GT(allocations, 0U);
  for (std::size_t failure = 0; failure < allocations; ++failure) {
    auto resource = std::make_shared<history_resource>();
    auto selected = options(); selected.allocation_resource = resource;
    {
      linear_store store{initial(), {41, 42}, selected};
      const auto before = store.save();
      const auto accepted = store.read();
      const auto bytes = resource->reserved_bytes();
      resource->fail_after(failure);
      const auto outcome = store.execute_transaction([&](auto& edit) {
        edit.registry().template edit<leaf>(first_leaf_id(store)).set_count(100);
      });
      resource->allow();
      EXPECT_THROW(outcome.throw_if_failed(), std::bad_alloc);
      EXPECT_EQ(store.read(), accepted);
      EXPECT_EQ(store.save(), before);
      EXPECT_EQ(resource->reserved_bytes(), bytes);
      EXPECT_THROW(store.undo(), std::out_of_range);
    }
    EXPECT_EQ(resource->reserved_bytes(), 0U);
  }
}

// Once the journal flushes, publishing prebuilt compact tables/blocks cannot request PMR storage.
TEST(managed_compact_history, durable_publication_and_recovery_need_no_postflush_allocation) {
  history_directory directory;
  auto resource = std::make_shared<history_resource>();
  auto selected = options(); selected.allocation_resource = resource;
  std::vector<std::uint8_t> accepted;
  {
    linear_store store{initial(), {41, 42}, selected};
    bool armed{};
    std::size_t calls_at_flush{};
    managed::journal_options journal;
    journal.fault_injector = [&](auto point) {
      if (armed && point == managed::journal_io_event::after_flush) {
        calls_at_flush = resource->calls();
        resource->deny();
      }
    };
    store.create_journal(directory.path(), managed::journal_storage_mode::appended, journal);
    armed = true;
    const auto outcome = store.execute_transaction([&](auto& edit) {
      edit.registry().template edit<leaf>(first_leaf_id(store)).set_count(100);
    });
    resource->allow();
    outcome.throw_if_failed();
    EXPECT_GT(calls_at_flush, 0U);
    EXPECT_EQ(resource->calls(), calls_at_flush);
    accepted = store.save();
  }
  EXPECT_EQ(resource->reserved_bytes(), 0U);
  {
    linear_store restored{initial(), {51, 52}, selected};
    restored.recover_journal(directory.path());
    EXPECT_EQ(restored.save(), accepted);
    restored.undo();
    EXPECT_EQ(payload(payload(payload(*restored.read()).sections.front()).leaves.front()).count, 0);
    restored.redo();
    EXPECT_EQ(restored.save(), accepted);
  }
  EXPECT_EQ(resource->reserved_bytes(), 0U);
}
