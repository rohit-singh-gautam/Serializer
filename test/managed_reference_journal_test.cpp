// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <managed_entity_references.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

namespace {
namespace managed = rohit::managed;
namespace serializer = rohit::serializer;
using root = entity_graph::document;
using node = entity_graph::node;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using store_type = managed::model_store<root>;
using id_type = store_type::id_type;
using event = managed::journal_io_event;
using mode = managed::journal_storage_mode;
constexpr managed::document_id journal_test_document{401U, 402U};
constexpr id_type root_id{19U}, first_id{7U}, second_id{93U}, other_id{31U};
constexpr std::uint64_t initial_watermark{130U};
constexpr std::array append_points{event::before_append, event::after_header,
    event::after_payload, event::after_commit, event::after_flush};

// Decode exact typed envelopes independently of live registry membership.
template <typename Plain>
managed::managed_ref<Plain, id_type> reference(id_type id, managed::document_id owner = journal_test_document) {
  const auto text = std::string{"{\"document_high\":"} + std::to_string(owner.high) +
      ",\"document_low\":" + std::to_string(owner.low) + ",\"class_id\":" +
      std::to_string(managed::model_traits<Plain>::class_id) + ",\"object_id\":" +
      std::to_string(id) + "}";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return serializer::deserialize_exact<managed::managed_ref<Plain, id_type>, serializer::json>(input);
}

// Sparse IDs and a forward/cyclic optional graph distinguish allocator metadata from payload integers.
store_type::storage_type baseline(managed::document_id owner = journal_test_document) {
  root value{};
  value.nodes.resize(2U);
  value.ordinary_id = 9001U;
  auto storage = traits::make_storage(std::move(value));
  storage.persistent_id = root_id;
  auto& payload = traits::payload(storage);
  payload.extra.persistent_id = other_id;
  payload.nodes[0].persistent_id = first_id;
  payload.nodes[1].persistent_id = second_id;
  auto& first = node_traits::payload(payload.nodes[0]);
  auto& second = node_traits::payload(payload.nodes[1]);
  first.label = "first";
  second.label = "second";
  first.next = reference<node>(second_id, owner);
  second.next = reference<node>(first_id, owner);
  first.owner = reference<root>(root_id, owner);
  payload.info.links = {reference<node>(first_id, owner), reference<node>(second_id, owner)};
  payload.info.named_links.emplace(17U, reference<node>(first_id, owner));
  return storage;
}

// Validate exact identity metadata and resolve every edge in the selected complete snapshot.
void expect_graph(const store_type& store, bool edited = false) {
  const auto& value = traits::payload(*store.read());
  ASSERT_EQ(value.nodes.size(), 2U);
  EXPECT_EQ(store.read()->persistent_id, root_id);
  EXPECT_EQ(value.extra.persistent_id, other_id);
  EXPECT_EQ(value.nodes[0].persistent_id, first_id);
  EXPECT_EQ(value.nodes[1].persistent_id, second_id);
  EXPECT_EQ(store.document(), journal_test_document);
  EXPECT_EQ(value.ordinary_id, 9001U);
  const auto catalog = store.registry();
  const auto first = catalog.resolve(reference<node>(first_id));
  const auto second = catalog.resolve(reference<node>(second_id));
  EXPECT_EQ(first.value().label, edited ? "edited" : "first");
  EXPECT_EQ(first.value().next, edited ? std::nullopt :
      std::optional{reference<node>(second_id)});
  EXPECT_EQ(second.value().next, reference<node>(edited ? second_id : first_id));
  ASSERT_TRUE(first.value().owner);
  EXPECT_EQ(catalog.resolve(*first.value().owner).id(), root_id);
  ASSERT_EQ(value.info.links.size(), 2U);
  EXPECT_EQ(catalog.resolve(value.info.links[0]).id(), first_id);
  EXPECT_EQ(catalog.resolve(value.info.links[1]).id(), second_id);
  EXPECT_EQ(catalog.resolve(value.info.named_links.at(17U)).id(), first_id);
}

// Apply an optional-edge removal and a self-cycle without allocating any new native identities.
auto edit_graph(store_type& store) {
  return store.execute_transaction([](auto& tx) {
    tx.registry().template edit<node>(first_id).set_label("edited");
    tx.update([](auto& storage) {
      auto& value = traits::payload(storage);
      node_traits::payload(value.nodes[0]).next.reset();
      node_traits::payload(value.nodes[1]).next = reference<node>(second_id);
    });
  });
}

// Retire the whole cyclic component only after removing its surviving external edge inventory.
auto retire_graph(store_type& store) {
  return store.execute_transaction([](auto& tx) {
    tx.update([](auto& storage) {
      auto& value = traits::payload(storage);
      value.info.links.clear();
      value.info.named_links.clear();
    });
    const auto remove = [](auto& storage, auto id) {
      std::erase_if(traits::payload(storage).nodes,
          [id](const auto& item) { return item.persistent_id == id; });
    };
    tx.registry().template retire<node>(first_id, remove);
    tx.registry().template retire<node>(second_id, remove);
  });
}

// Restrict every fixture and recursive cleanup to one freshly created child of the ignored build tree.
struct journal_directory {
  const std::filesystem::path base{SERIALIZER_IDENTIFIED_BASELINE_TEST_DIRECTORY};
  const std::filesystem::path path{base / ("reference-journal-"
      SERIALIZER_IDENTIFIED_BASELINE_TEST_PROFILE "-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  // Reject a reused fixture or a path outside the caller's configured build directory.
  journal_directory() {
    if (!base.is_absolute() || path.parent_path() != base ||
        !std::filesystem::create_directory(path)) {
      throw std::runtime_error{"Invalid typed-reference journal directory"};
    }
  }
  // Release only this fixture after all local journal handles have closed.
  ~journal_directory() {
    std::error_code ignored;
    if (path.parent_path() == base) { std::filesystem::remove_all(path, ignored); }
  }
};

// A complete footer is the durable decision even when the acknowledgement callback throws afterward.
bool committed(event point) {
  return point == event::after_commit || point == event::after_flush;
}
} // namespace

// A rejected durable reservation cannot publish a graph, history entry, ID or observer notification.
TEST(managed_reference_journal, reservation_faults_recover_exact_burned_watermark_and_typed_graph) {
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    for (const auto point : append_points) {
      SCOPED_TRACE(static_cast<int>(storage_mode));
      SCOPED_TRACE(static_cast<int>(point));
      const journal_directory directory;
      const auto path = directory.path / "document";
      bool armed{};
      managed::journal_options options;
      options.fault_injector = [&](auto actual) {
        if (armed && actual == point) {
          throw std::system_error{std::make_error_code(std::errc::no_space_on_device)};
        }
      };
      {
        store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
        store.create_journal(path, storage_mode, options);
        const auto pin = store.read();
        const auto catalog = store.registry();
        const auto stamp = store.state();
        std::size_t notifications{};
        store.set_publication_callback([&](const auto&) { ++notifications; });
        id_type exposed{};
        armed = true;
        const auto outcome = store.execute_transaction([&](auto& tx) { exposed = tx.registry().reserve(); });
        EXPECT_EQ(outcome.status, point == event::before_append ?
            managed::transaction_status::failed : managed::transaction_status::indeterminate);
        EXPECT_EQ(exposed, 0U);
        EXPECT_EQ(store.allocated_id(), initial_watermark);
        EXPECT_EQ(store.read(), pin);
        EXPECT_EQ(store.state(), stamp);
        EXPECT_EQ(store.history_cursor(), 0U);
        EXPECT_EQ(store.journal_sequence(), 0U);
        EXPECT_EQ(notifications, 0U);
        EXPECT_EQ(catalog.resolve(reference<node>(first_id)).value().next, reference<node>(second_id));
        expect_graph(store);
      }
      store_type recovered{root{}};
      recovered.recover_journal(path);
      const auto burned = initial_watermark + (committed(point) ? 1U : 0U);
      EXPECT_EQ(recovered.allocated_id(), burned);
      EXPECT_EQ(recovered.journal_sequence(), committed(point) ? 1U : 0U);
      expect_graph(recovered);
      id_type next{};
      recovered.execute_transaction([&](auto& tx) { next = tx.registry().reserve(); tx.revert(); }).throw_if_failed();
      EXPECT_EQ(next, burned + 1U);
      EXPECT_EQ(recovered.history_cursor(), 0U);
    }
  }
}

// Failed import mapping burns only its durable reservation; accepted import/history retain exact target types.
TEST(managed_reference_journal, rejected_import_reservation_survives_recovery_before_valid_import) {
  constexpr managed::document_id source_document{601U, 602U};
  store_type source{managed::identified_baseline, baseline(source_document), source_document, initial_watermark};
  const auto imported = traits::payload(*source.read()).nodes[0];
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    const journal_directory directory;
    const auto path = directory.path / "document";
    {
      store_type destination{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
      destination.create_journal(path, storage_mode);
      const auto pin = destination.read();
      const auto stamp = destination.state();
      std::size_t notifications{};
      destination.set_publication_callback([&](const auto&) { ++notifications; });
      const auto rejected = destination.execute_transaction([&](auto& tx) {
        tx.registry().template import_storage<node>(source_document, imported,
            [](auto& storage, auto incoming) { traits::payload(storage).nodes.push_back(std::move(incoming)); });
      });
      EXPECT_EQ(rejected.status, managed::transaction_status::failed);
      EXPECT_THROW(rejected.throw_if_failed(), managed::registry_error);
      EXPECT_EQ(destination.read(), pin);
      EXPECT_EQ(destination.state(), stamp);
      EXPECT_EQ(destination.history_cursor(), 0U);
      EXPECT_EQ(notifications, 0U);
      EXPECT_EQ(destination.allocated_id(), initial_watermark + 1U);
      EXPECT_EQ(destination.journal_sequence(), 1U);
      EXPECT_FALSE(destination.journal_dirty());
      expect_graph(destination);
    }
    {
      store_type recovered{root{}};
      recovered.recover_journal(path);
      const auto pinned = recovered.registry();
      EXPECT_EQ(recovered.allocated_id(), initial_watermark + 1U);
      expect_graph(recovered);
      id_type adopted{};
      recovered.execute_transaction([&](auto& tx) {
        adopted = tx.registry().template import_storage<node>(source_document, imported,
            [](auto& storage, auto incoming) { traits::payload(storage).nodes.push_back(std::move(incoming)); },
            [&](const auto& edge) {
              using target = typename std::remove_cvref_t<decltype(edge)>::target_type;
              if constexpr (std::is_same_v<target, node>) { return reference<node>(second_id); }
              else { return reference<root>(root_id); }
            });
      }).throw_if_failed();
      EXPECT_EQ(adopted, initial_watermark + 2U);
      const auto accepted = recovered.registry().find<node>(adopted);
      EXPECT_EQ(accepted.value().next, reference<node>(second_id));
      EXPECT_EQ(accepted.value().owner, reference<root>(root_id));
      recovered.save_journal();
      recovered.undo();
      EXPECT_FALSE(recovered.registry().contains(adopted));
      expect_graph(recovered);
      recovered.redo();
      EXPECT_EQ(recovered.registry().resolve(*recovered.registry().find<node>(adopted).value().next).id(), second_id);
      EXPECT_EQ(pinned.resolve(reference<node>(first_id)).value().next, reference<node>(second_id));
    }
    store_type reopened{root{}};
    reopened.recover_journal(path);
    EXPECT_EQ(reopened.allocated_id(), initial_watermark + 2U);
    EXPECT_TRUE(reopened.registry().contains(static_cast<id_type>(initial_watermark + 2U)));
    reopened.undo();
    expect_graph(reopened);
  }
}

// A dangling one-node retirement fails complete-candidate validation before any durable journal work.
TEST(managed_reference_journal, dangling_cyclic_retirement_rejects_before_durable_io) {
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    const journal_directory directory;
    const auto path = directory.path / "document";
    {
      store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
      bool armed{};
      std::size_t io_events{};
      managed::journal_options options;
      options.fault_injector = [&](auto) { if (armed) { ++io_events; } };
      store.create_journal(path, storage_mode, options);
      const auto pin = store.read();
      const auto pinned = store.registry();
      const auto stamp = store.state();
      std::size_t notifications{};
      store.set_publication_callback([&](const auto&) { ++notifications; });
      armed = true;
      const auto rejected = store.execute_transaction([](auto& tx) {
        tx.registry().template retire<node>(first_id, [](auto& storage, auto id) {
          std::erase_if(traits::payload(storage).nodes,
              [id](const auto& item) { return item.persistent_id == id; });
        });
      });
      EXPECT_EQ(rejected.status, managed::transaction_status::failed);
      EXPECT_THROW(rejected.throw_if_failed(), managed::registry_error);
      EXPECT_EQ(store.read(), pin);
      EXPECT_EQ(store.state(), stamp);
      EXPECT_EQ(store.history_cursor(), 0U);
      EXPECT_EQ(store.allocated_id(), initial_watermark);
      EXPECT_EQ(store.journal_sequence(), 0U);
      EXPECT_FALSE(store.journal_needs_recovery());
      EXPECT_EQ(io_events, 0U);
      EXPECT_EQ(notifications, 0U);
      EXPECT_EQ(pinned.resolve(reference<node>(first_id)).value().next, reference<node>(second_id));
      expect_graph(store);
    }
    store_type recovered{root{}};
    recovered.recover_journal(path);
    expect_graph(recovered);
    EXPECT_EQ(recovered.journal_sequence(), 0U);
    retire_graph(recovered).throw_if_failed();
    EXPECT_TRUE(traits::payload(*recovered.read()).nodes.empty());
    EXPECT_EQ(recovered.journal_sequence(), 1U);
    EXPECT_EQ(recovered.allocated_id(), initial_watermark);
    recovered.undo();
    expect_graph(recovered);
  }
}

// Typed optional edits and complete cyclic deletion publish only after a committed native journal record.
TEST(managed_reference_journal, graph_publication_faults_retain_live_pins_and_recover_complete_candidates) {
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    for (const bool deleting : {false, true}) {
      for (const auto point : append_points) {
        SCOPED_TRACE(static_cast<int>(point));
        SCOPED_TRACE(deleting);
        const journal_directory directory;
        const auto path = directory.path / "document";
        bool armed{};
        managed::journal_options options;
        options.fault_injector = [&](auto actual) {
          if (armed && actual == point) { throw std::runtime_error{"typed graph journal interruption"}; }
        };
        {
          store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
          store.create_journal(path, storage_mode, options);
          const auto pin = store.read();
          const auto catalog = store.registry();
          const auto stamp = store.state();
          std::size_t notifications{};
          store.set_publication_callback([&](const auto&) { ++notifications; });
          armed = true;
          const auto outcome = deleting ? retire_graph(store) : edit_graph(store);
          EXPECT_EQ(outcome.status, point == event::before_append ?
              managed::transaction_status::failed : managed::transaction_status::indeterminate);
          EXPECT_EQ(store.read(), pin);
          EXPECT_EQ(store.state(), stamp);
          EXPECT_EQ(store.history_cursor(), 0U);
          EXPECT_EQ(store.allocated_id(), initial_watermark);
          EXPECT_EQ(notifications, 0U);
          EXPECT_EQ(catalog.resolve(reference<node>(second_id)).value().next, reference<node>(first_id));
          expect_graph(store);
        }
        store_type recovered{root{}};
        recovered.recover_journal(path);
        EXPECT_EQ(recovered.allocated_id(), initial_watermark);
        EXPECT_EQ(recovered.journal_sequence(), committed(point) ? 1U : 0U);
        if (deleting && committed(point)) {
          EXPECT_TRUE(traits::payload(*recovered.read()).nodes.empty());
          EXPECT_FALSE(recovered.registry().contains(first_id));
          recovered.undo();
          expect_graph(recovered);
          recovered.redo();
          EXPECT_TRUE(traits::payload(*recovered.read()).nodes.empty());
        } else {
          expect_graph(recovered, committed(point));
        }
      }
    }
  }
}

// Failed Undo/Redo controls retain both the chosen typed graph and the exact live history cursor.
TEST(managed_reference_journal, navigation_faults_recover_exact_typed_history_decision) {
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    for (const bool redo : {false, true}) {
      for (const auto point : append_points) {
        const journal_directory directory;
        const auto path = directory.path / "document";
        bool armed{};
        managed::journal_options options;
        options.fault_injector = [&](auto actual) {
          if (armed && actual == point) { throw std::runtime_error{"typed history interruption"}; }
        };
        {
          store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
          store.create_journal(path, storage_mode, options);
          edit_graph(store).throw_if_failed();
          if (redo) { store.undo(); }
          const auto pin = store.read();
          const auto stamp = store.state();
          const auto cursor = store.history_cursor();
          const auto sequence = store.journal_sequence();
          std::size_t notifications{};
          store.set_publication_callback([&](const auto&) { ++notifications; });
          armed = true;
          EXPECT_ANY_THROW(redo ? store.redo() : store.undo());
          EXPECT_EQ(store.read(), pin);
          EXPECT_EQ(store.state(), stamp);
          EXPECT_EQ(store.history_cursor(), cursor);
          EXPECT_EQ(store.journal_sequence(), sequence);
          EXPECT_EQ(notifications, 0U);
          expect_graph(store, !redo);
        }
        store_type recovered{root{}};
        recovered.recover_journal(path);
        const bool edited = committed(point) ? redo : !redo;
        EXPECT_EQ(recovered.history_cursor(), edited ? 1U : 0U);
        EXPECT_EQ(recovered.journal_sequence(), (redo ? 2U : 1U) + (committed(point) ? 1U : 0U));
        EXPECT_EQ(recovered.allocated_id(), initial_watermark);
        expect_graph(recovered, edited);
      }
    }
  }
}

// Full-Save faults retain complete cyclic history and unused reservations across generation replacement.
TEST(managed_reference_journal, save_faults_preserve_graph_history_and_burned_reservations) {
  for (const auto storage_mode : {mode::appended, mode::sidecar}) {
    for (const auto point : {event::after_sidecar_flush, event::after_base_flush,
             event::before_replace, event::after_replace, event::after_publish, event::before_cleanup}) {
      if (storage_mode == mode::appended && point == event::after_sidecar_flush) { continue; }
      const journal_directory directory;
      const auto path = directory.path / "document";
      bool armed{};
      managed::journal_options options;
      options.fault_injector = [&](auto actual) {
        if (armed && actual == point) { throw std::runtime_error{"typed Save interruption"}; }
      };
      {
        store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
        store.create_journal(path, storage_mode, options);
        store.execute_transaction([](auto& tx) { static_cast<void>(tx.registry().reserve()); tx.revert(); }).throw_if_failed();
        edit_graph(store).throw_if_failed();
        const auto pin = store.read();
        const auto pinned = store.registry();
        const auto stamp = store.state();
        std::size_t notifications{};
        store.set_publication_callback([&](const auto&) { ++notifications; });
        armed = true;
        if (point == event::before_cleanup) { EXPECT_NO_THROW(store.save_journal()); }
        else { EXPECT_ANY_THROW(store.save_journal()); }
        EXPECT_EQ(store.read(), pin);
        EXPECT_EQ(store.state(), stamp);
        EXPECT_EQ(store.history_cursor(), 1U);
        EXPECT_EQ(store.allocated_id(), initial_watermark + 1U);
        EXPECT_EQ(notifications, 0U);
        EXPECT_FALSE(pinned.resolve(reference<node>(first_id)).value().next);
        expect_graph(store, true);
      }
      store_type recovered{root{}};
      recovered.recover_journal(path);
      expect_graph(recovered, true);
      EXPECT_EQ(recovered.allocated_id(), initial_watermark + 1U);
      EXPECT_EQ(recovered.journal_sequence(), 2U);
      const bool published = point == event::after_replace || point == event::after_publish || point == event::before_cleanup;
      EXPECT_EQ(recovered.journal_dirty(), !published);
      recovered.undo();
      expect_graph(recovered);
      recovered.redo();
      expect_graph(recovered, true);
    }
  }
}
