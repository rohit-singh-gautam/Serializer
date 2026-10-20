// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <managed_entity_references.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
constexpr managed::document_id journal_test_document{401U, 402U}, source_document{601U, 602U};
constexpr id_type root_id{19U}, first_id{7U}, second_id{93U}, other_id{31U};
constexpr std::uint64_t initial_watermark{130U};
constexpr int interruption_exit_code{86};

// Use the public keyed codec to construct typed references with exact native identity widths.
template <typename Plain>
managed::managed_ref<Plain, id_type> reference(id_type id, managed::document_id owner = journal_test_document) {
  const auto text = std::string{"{\"document_high\":"} + std::to_string(owner.high) +
      ",\"document_low\":" + std::to_string(owner.low) + ",\"class_id\":" +
      std::to_string(managed::model_traits<Plain>::class_id) + ",\"object_id\":" +
      std::to_string(id) + "}";
  const auto stream = rohit::make_constant_full_stream(text.data(), text.size());
  return serializer::deserialize_exact<managed::managed_ref<Plain, id_type>, serializer::json>(stream);
}

// Seed explicit sparse identities, forward/cyclic optional edges and unrelated ordinary payload integers.
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

// Reject semantic or identity drift using active checks in optimized subprocess builds.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Translate documented fault names without binding the driver to enum representation.
managed::journal_io_event parse_event(std::string_view name) {
  using event = managed::journal_io_event;
  constexpr std::pair<std::string_view, event> events[]{
      {"before_append", event::before_append}, {"after_header", event::after_header},
      {"after_payload", event::after_payload}, {"after_commit", event::after_commit},
      {"after_flush", event::after_flush}, {"after_sidecar_flush", event::after_sidecar_flush},
      {"after_base_flush", event::after_base_flush}, {"before_replace", event::before_replace},
      {"after_replace", event::after_replace}, {"after_publish", event::after_publish},
      {"before_cleanup", event::before_cleanup}, {"before_tail_repair", event::before_tail_repair},
      {"after_tail_repair", event::after_tail_repair}};
  for (const auto& [label, point] : events) { if (name == label) { return point; } }
  throw std::invalid_argument{"Unknown typed-reference crash boundary"};
}

// Produce a changed optional edge and a self-cycle without consuming an identity reservation.
void edit_graph(store_type& store) {
  store.execute_transaction([](auto& tx) {
    tx.registry().template edit<node>(first_id).set_label("edited");
    tx.update([](auto& storage) {
      auto& payload = traits::payload(storage);
      node_traits::payload(payload.nodes[0]).next.reset();
      node_traits::payload(payload.nodes[1]).next = reference<node>(second_id);
    });
  }).throw_if_failed();
}

// Validate and publish the complete cyclic retirement in a single native transaction.
void retire_graph(store_type& store) {
  store.execute_transaction([](auto& tx) {
    tx.update([](auto& storage) {
      auto& payload = traits::payload(storage);
      payload.info.links.clear();
      payload.info.named_links.clear();
      payload.nodes.clear();
    });
  }).throw_if_failed();
}

// Import a foreign node with explicit typed external mappings, or reject before any candidate publication.
void import_graph(store_type& store, bool reject) {
  store_type source{managed::identified_baseline, baseline(source_document), source_document, initial_watermark};
  auto imported = traits::payload(*source.read()).nodes[0];
  node_traits::payload(imported).label = "imported";
  const auto outcome = store.execute_transaction([&](auto& tx) {
    const auto attach = [](auto& storage, auto incoming) {
      traits::payload(storage).nodes.push_back(std::move(incoming));
    };
    if (reject) { tx.registry().template import_storage<node>(source_document, imported, attach); }
    else {
      tx.registry().template import_storage<node>(source_document, imported, attach,
          [](const auto& edge) {
            using target = typename std::remove_cvref_t<decltype(edge)>::target_type;
            if constexpr (std::is_same_v<target, node>) { return reference<node>(second_id); }
            else { return reference<root>(root_id); }
          });
    }
  });
  if (reject) {
    require(outcome.status == managed::transaction_status::failed, "Unmapped import unexpectedly published");
    try { outcome.throw_if_failed(); }
    catch (const managed::registry_error&) { return; }
    throw std::runtime_error{"Rejected import lost its typed registry cause"};
  }
  outcome.throw_if_failed();
}

// Check all surviving typed targets before exposing a machine-readable recovery summary.
void inspect(const store_type& store) {
  const auto& payload = traits::payload(*store.read());
  require(store.document() == journal_test_document && store.read()->persistent_id == root_id &&
      payload.extra.persistent_id == other_id && payload.ordinary_id == 9001U,
      "Recovered document identity or ordinary payload changed");
  const auto catalog = store.registry();
  std::string_view phase{"retired"};
  if (payload.nodes.empty()) {
    require(payload.info.links.empty() && payload.info.named_links.empty() &&
        !catalog.contains(first_id) && !catalog.contains(second_id), "Partial cyclic retirement recovered");
  } else {
    require(payload.nodes.size() == 2U || payload.nodes.size() == 3U, "Partial typed aggregate recovered");
    require(payload.nodes[0].persistent_id == first_id && payload.nodes[1].persistent_id == second_id,
        "Recovered original IDs were reassigned");
    const auto& first = node_traits::payload(payload.nodes[0]);
    const auto& second = node_traits::payload(payload.nodes[1]);
    const bool edited = first.label == "edited";
    phase = edited ? "edited" : "linked";
    require(first.label == (edited ? "edited" : "first") && second.label == "second", "Recovered labels changed");
    require(first.next == (edited ? std::nullopt : std::optional{reference<node>(second_id)}) &&
        second.next == reference<node>(edited ? second_id : first_id), "Recovered optional cycle changed");
    require(first.owner && catalog.resolve(*first.owner).id() == root_id, "Recovered owner reference changed");
    require(payload.info.links.size() == 2U && catalog.resolve(payload.info.links[0]).id() == first_id &&
        catalog.resolve(payload.info.links[1]).id() == second_id &&
        catalog.resolve(payload.info.named_links.at(17U)).id() == first_id, "Recovered typed collection references changed");
    if (payload.nodes.size() == 3U) {
      const auto& incoming = node_traits::payload(payload.nodes[2]);
      const auto id = payload.nodes[2].persistent_id;
      require(id > initial_watermark && id <= store.allocated_id(), "Recovered new ID crossed its burned watermark");
      if (incoming.label == "imported") {
        phase = "imported";
        require(incoming.next == reference<node>(second_id) && incoming.owner == reference<root>(root_id),
            "Recovered import retained a foreign/unmapped typed edge");
      } else {
        phase = "expanded";
        require(incoming.label == "fresh" && !incoming.next && !incoming.owner,
            "Fresh native identity acquired a discarded typed target");
      }
    }
  }
  std::cout << "{\"phase\":\"" << phase << "\",\"root\":" << store.read()->persistent_id
      << ",\"first\":" << (payload.nodes.empty() ? 0U : first_id)
      << ",\"second\":" << (payload.nodes.empty() ? 0U : second_id)
      << ",\"new_id\":" << (payload.nodes.size() == 3U ? payload.nodes[2].persistent_id : 0U)
      << ",\"watermark\":" << store.allocated_id() << ",\"sequence\":" << store.journal_sequence()
      << ",\"cursor\":" << store.history_cursor() << ",\"dirty\":" << (store.journal_dirty() ? "true" : "false") << "}\n";
}
} // namespace

// Abrupt _Exit skips all store/file destructors and forces the next invocation to recover durable bytes.
int main(int argc, char** argv) {
  if (argc != 6) { return 2; }
  try {
    const std::filesystem::path path{argv[1]};
    const std::string_view mode_name{argv[2]}, operation{argv[3]}, point_name{argv[4]};
    require(mode_name == "appended" || mode_name == "sidecar", "Unknown typed-reference journal mode");
    const auto storage_mode = mode_name == "appended" ? managed::journal_storage_mode::appended :
        managed::journal_storage_mode::sidecar;
    const auto selected_record = static_cast<std::size_t>(std::stoull(argv[5]));
    const auto point = point_name == "none" ? managed::journal_io_event::before_append : parse_event(point_name);
    bool armed = operation == "recover";
    std::size_t record{};
    managed::journal_options options;
    options.fault_injector = [&](auto actual) {
      if (!armed || point_name == "none") { return; }
      if (actual == managed::journal_io_event::before_append) { ++record; }
      if (actual == point && (selected_record == 0U || selected_record == record)) {
        std::_Exit(interruption_exit_code);
      }
    };
    store_type store{managed::identified_baseline, baseline(), journal_test_document, initial_watermark};
    if (operation == "create") { store.create_journal(path, storage_mode, options); return 0; }
    store.recover_journal(path, options);
    armed = true;
    record = 0U;
    if (operation == "edit") { edit_graph(store); }
    else if (operation == "delete") { retire_graph(store); }
    else if (operation == "import" || operation == "reject_import") { import_graph(store, operation == "reject_import"); }
    else if (operation == "reserve") {
      store.execute_transaction([](auto& tx) { static_cast<void>(tx.registry().reserve()); tx.revert(); }).throw_if_failed();
    } else if (operation == "allocate") {
      const auto before = store.allocated_id();
      id_type assigned{};
      store.execute_transaction([&](auto& tx) {
        node incoming{};
        incoming.label = "fresh";
        assigned = tx.registry().adopt(std::move(incoming), [](auto& storage, auto value) {
          traits::payload(storage).nodes.push_back(std::move(value));
        });
      }).throw_if_failed();
      require(assigned == before + 1U, "Recovered allocator reused a burned ID");
      inspect(store);
    } else if (operation == "undo") { store.undo(); }
    else if (operation == "redo") { store.redo(); }
    else if (operation == "save") { store.save_journal(); }
    else if (operation == "inspect") { inspect(store); }
    else if (operation == "history") {
      const auto before = store.save();
      const auto watermark = store.allocated_id();
      const auto pin = store.registry();
      const bool retained = pin.contains(first_id);
      store.undo();
      require(store.registry().resolve(reference<node>(first_id)).id() == first_id,
          "Recovered history did not restore the original typed target");
      store.redo();
      require(store.save() == before && store.allocated_id() == watermark,
          "Recovered history changed native bytes or recycled IDs");
      require(pin.contains(first_id) == retained, "Recovered history invalidated an immutable registry pin");
      if (retained) {
        require(pin.resolve(reference<node>(first_id)).id() == first_id,
            "Recovered history retargeted an immutable typed registry pin");
      }
      inspect(store);
    } else if (operation != "recover") { throw std::invalid_argument{"Unknown typed-reference crash operation"}; }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
