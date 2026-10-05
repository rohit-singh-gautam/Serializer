#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>
#include <rohit/managed_file_journal.hpp>
#include <rohit/managed_record_budget.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using features = managed::store_features;
using history = managed::history_mode;
using labels = managed::history_labels;
using traits = managed::model_traits<ledger>;
template <history Mode, features Features>
using selected_store = managed::model_store<ledger, Mode, labels::disabled, traits, Features>;
using history_store = selected_store<history::linear, features::none>;
using journal_store = selected_store<history::disabled, features::journal>;
using collaboration_store = selected_store<history::disabled, features::collaboration>;
using plain_store = selected_store<history::disabled, features::none>;
using authority_type = managed::collaboration_authority<
    ledger, history::disabled, labels::disabled, traits, std::uint64_t,
    managed::collaboration_session_traits<std::uint64_t>, features::collaboration>;

template <typename Store>
concept journal_api = requires(Store& store) { store.create_journal("document"); };
template <typename Store>
concept collaboration_api = requires(Store& store) { store.collaborate(std::uint64_t{10}); };
template <typename Store>
concept undo_api = requires(Store& store) { store.undo(); };
template <typename State>
concept undo_state = requires(State& state) {
  state.undo;
  state.redo;
};

static_assert(!journal_api<history_store> && !collaboration_api<history_store> &&
              undo_api<history_store>);
static_assert(journal_api<journal_store> && !collaboration_api<journal_store> &&
              !undo_api<journal_store>);
static_assert(!journal_api<collaboration_store> && collaboration_api<collaboration_store> &&
              !undo_api<collaboration_store>);
static_assert(!undo_state<managed::collaboration::pending_client_state>);
static_assert(sizeof(managed::collaboration::pending_client_transaction) <
              sizeof(managed::collaboration::client_transaction));
static_assert(std::is_empty_v<managed::detail::store_journal_storage<false>>);
static_assert(std::is_empty_v<managed::detail::store_collaboration_storage<false, ledger, int>>);
static_assert(sizeof(plain_store) < sizeof(journal_store));
static_assert(sizeof(plain_store) < sizeof(collaboration_store));

// Native history remains usable with journal and collaboration code/storage compiled out.
TEST(managed_features, history_only_round_trip_and_navigation) {
  history_store store{ledger{"Before"}};
  store.execute_transaction([](auto& edit) { edit.root().set_name("After"); }).throw_if_failed();
  store.undo();
  EXPECT_EQ(store.read()->name, "Before");
  history_store loaded{ledger{}};
  loaded.load(store.save());
  loaded.redo();
  EXPECT_EQ(loaded.read()->name, "After");
}

// Journal-only stores recover edits without native history or collaboration attachments.
TEST(managed_features, journal_only_recovery) {
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto id = managed::make_document_id();
    const auto directory =
        std::filesystem::current_path() / ("feature-journal-" + std::to_string(id.high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto path = directory / "document";
    {
      journal_store store{ledger{"Before"}, id};
      store.create_journal(path, mode);
      store.execute_transaction([](auto& edit) { edit.root().set_name("After"); })
          .throw_if_failed();
      EXPECT_TRUE(store.journal_dirty());
    }
    {
      journal_store loaded{ledger{}};
      loaded.recover_journal(path);
      EXPECT_EQ(loaded.read()->name, "After");
      loaded.save_journal();
      EXPECT_FALSE(loaded.journal_dirty());
    }
    std::filesystem::remove_all(directory);
  }
}

// Completed synchronization releases snapshots; local numbers remain monotonic across compaction.
TEST(managed_features, collaboration_only_releases_acknowledged_transactions) {
  authority_type authority{ledger{"Before"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  collaboration_store store{ledger{}};
  managed::collaboration_client_options options;
  options.max_transactions = 1;
  store.collaborate(std::uint64_t{10}, options).bind(transport);
  store.synchronize();
  for (std::uint64_t number = 1; number <= 3; ++number) {
    store.execute_transaction([&](auto& edit) { edit.root().set_name(std::to_string(number)); })
        .throw_if_failed();
    EXPECT_EQ(store.collaboration().state()->transactions.front().number, number);
    store.synchronize();
    EXPECT_TRUE(store.collaboration().state()->transactions.empty());
    EXPECT_TRUE(store.collaboration().state()->pending.empty());
    EXPECT_EQ(authority.read()->name, std::to_string(number));
  }
  collaboration_store loaded{ledger{}};
  loaded.collaborate(std::uint64_t{10});
  loaded.load(store.save());
  EXPECT_EQ(loaded.collaboration().state()->next_transaction, 3u);
  EXPECT_TRUE(loaded.collaboration().state()->transactions.empty());
}

// A pinned inspection snapshot remains immutable while an unpinned append reuses existing payloads.
TEST(managed_features, local_append_preserves_pins_and_reuses_unpinned_payloads) {
  using local_store = selected_store<history::linear, features::collaboration>;
  managed::collaboration_authority<ledger> authority{ledger{"Before"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  local_store store{ledger{}};
  store.collaborate(std::uint64_t{10}).bind(transport);
  store.synchronize();
  store.execute_transaction([](auto& edit) { edit.root().set_name("One"); }).throw_if_failed();
  const auto* bytes = store.collaboration().state()->transactions.front().before.data();
  store.execute_transaction([](auto& edit) { edit.root().set_name("Two"); }).throw_if_failed();
  EXPECT_EQ(store.collaboration().state()->transactions.front().before.data(), bytes);
  const auto pin = store.collaboration().state();
  store.execute_transaction([](auto& edit) { edit.root().set_name("Three"); }).throw_if_failed();
  EXPECT_EQ(pin->transactions.size(), 2u);
  EXPECT_EQ(store.collaboration().state()->transactions.size(), 3u);
  EXPECT_NE(pin->transactions.front().before.data(),
            store.collaboration().state()->transactions.front().before.data());
}

// Preflight agrees with the real generated codec at individual resource-limit boundaries.
TEST(managed_features, checkpoint_budget_matches_decoder) {
  managed::collaboration::client_checkpoint checkpoint;
  checkpoint.kind = 2;
  checkpoint.binding = "serializer.collaboration.client.v1";
  checkpoint.state.session_format = "uint64";
  checkpoint.state.session = {10};
  checkpoint.state.transactions.push_back({1, 1, 0, {1, 2}, {3, 4}, 0, 0, 1, {}, false, "label"});
  const auto bytes = managed::encode_collaboration_record(checkpoint);
  EXPECT_EQ(bytes,
            managed::detail::encode(
                managed::detail::checkpoint_view<managed::collaboration::client_state>{
                    checkpoint.kind, checkpoint.model, checkpoint.state, checkpoint.binding}));
  using limits_type = rohit::serializer::decode_limits;
  for (const auto member : {&limits_type::max_input_bytes, &limits_type::max_string_bytes,
                            &limits_type::max_collection_elements, &limits_type::max_nesting_depth,
                            &limits_type::max_allocation_bytes, &limits_type::max_work_units}) {
    for (std::size_t boundary = 0; boundary <= 1024; ++boundary) {
      limits_type limits;
      limits.*member = boundary;
      bool decoded = false;
      bool inspected = false;
      try {
        static_cast<void>(
            managed::decode_collaboration_record<decltype(checkpoint)>(bytes, limits));
        decoded = true;
      } catch (const std::exception&) {
      }
      try {
        managed::detail::record_budget budget{limits};
        budget.check(managed::detail::checkpoint_view<managed::collaboration::client_state>{
            checkpoint.kind, checkpoint.model, checkpoint.state, checkpoint.binding});
        inspected = true;
      } catch (const std::exception&) {
      }
      ASSERT_EQ(inspected, decoded) << "Budget boundary " << boundary;
    }
  }
}

// Count root snapshot encoding separately from generated entity/field projections.
struct counted_storage : ledger {
  static inline std::size_t encodes{};
  // Permit fresh-value decoding of the instrumented storage.
  counted_storage() = default;
  // Transfer generated managed storage without altering its identities or values.
  explicit counted_storage(ledger value) : ledger{std::move(value)} {}
  // Delegate nested protocol traversal without counting another root encoding.
  template <typename Protocol>
  void serialize_out(Protocol& protocol) const {
    ledger::serialize_out(protocol);
  }
  // Count only the complete root encoding used by model_store and authority snapshots.
  template <template <rohit::serializer::serialize_type> class Protocol, typename Stream>
  void serialize_out(Stream& stream) const {
    ++encodes;
    ledger::serialize_out<Protocol>(stream);
  }
};
struct counted_traits : traits {
  using storage_type = counted_storage;
  // Keep the generated adapter's ownership/identity rules while instrumenting root encoding.
  static storage_type make_storage(ledger value) {
    return storage_type{traits::make_storage(std::move(value))};
  }
};

// Authority publication reuses the store's encoded candidate, and snapshot polling reuses its bytes.
TEST(managed_features, authority_encodes_each_candidate_once) {
  using authority = managed::collaboration_authority<
      ledger, history::disabled, labels::disabled, counted_traits, std::uint64_t,
      managed::collaboration_session_traits<std::uint64_t>, features::collaboration>;
  authority server{ledger{"Before"}, 1, [](auto& edit, auto) {
                     edit.update([](auto& value) { value.name = "After"; });
                   }};
  server.open_session(10);
  typename authority::change_proposal_type request{server.context(), 10, 1, 0, {}, {}};
  counted_storage::encodes = 0;
  ASSERT_EQ(server.submit_change(request, 10, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(counted_storage::encodes, 1u);
  static_cast<void>(server.snapshot());
  EXPECT_EQ(counted_storage::encodes, 1u);
}

// Tree byte accounting survives failed appends, load and reset without scanning retained revisions.
TEST(managed_features, tree_retention_counter_publication) {
  using tree_store = selected_store<history::tree, features::none>;
  tree_store probe{ledger{"000"}};
  const auto snapshot_bytes = managed::detail::encode(*probe.read()).size();
  managed::store_options options;
  options.max_history_bytes = 3 * snapshot_bytes;
  tree_store store{ledger{"000"}, managed::make_document_id(), options};
  store.execute_transaction([](auto& edit) { edit.root().set_name("111"); }).throw_if_failed();
  store.execute_transaction([](auto& edit) { edit.root().set_name("222"); }).throw_if_failed();
  const auto saved = store.save();
  EXPECT_EQ(store.execute_transaction([](auto& edit) { edit.root().set_name("333"); }).status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  tree_store loaded{ledger{}, managed::make_document_id(), options};
  loaded.load(saved);
  EXPECT_EQ(loaded.execute_transaction([](auto& edit) { edit.root().set_name("333"); }).status,
            managed::transaction_status::failed);
  loaded.reset_history();
  loaded.execute_transaction([](auto& edit) { edit.root().set_name("333"); }).throw_if_failed();
  loaded.execute_transaction([](auto& edit) { edit.root().set_name("444"); }).throw_if_failed();
  EXPECT_EQ(loaded.read()->name, "444");
}

// Pending-only journals survive both queue growth and acknowledgement compaction on recovery.
TEST(managed_features, pending_only_journal_recovery_and_compaction) {
  using durable_store = selected_store<history::disabled, features::all>;
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    authority_type authority{ledger{"Before"}, 1};
    authority.open_session(10);
    managed::collaboration_transport transport{authority, std::uint64_t{10}};
    const auto id = managed::make_document_id();
    const auto directory =
        std::filesystem::current_path() / ("pending-journal-" + std::to_string(id.high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto path = directory / "document";
    {
      durable_store store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.synchronize();
      store.create_journal(path, mode);
      static_cast<void>(store.collaboration().reserve_operation_id());
      store.execute_transaction([](auto& edit) { edit.root().set_name("Queued"); })
          .throw_if_failed();
    }
    {
      bool checked_metadata = false;
      auto journal = managed::detail::recover_file_journal(
          path, {}, [&](bool base, std::span<const std::uint8_t> bytes) {
            if (!base && !checked_metadata) {
              const auto checkpoint = managed::decode_collaboration_record<
                  managed::collaboration::pending_client_checkpoint>(bytes);
              EXPECT_EQ(checkpoint.kind, 2u);
              EXPECT_TRUE(checkpoint.model.empty());
              checked_metadata = true;
            }
          });
      EXPECT_TRUE(checked_metadata);
    }
    {
      durable_store store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.recover_journal(path);
      ASSERT_EQ(store.collaboration().pending_count(), 1u);
      store.synchronize();
      EXPECT_TRUE(store.collaboration().state()->transactions.empty());
    }
    {
      durable_store store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.recover_journal(path);
      EXPECT_TRUE(store.collaboration().state()->transactions.empty());
      EXPECT_EQ(store.read()->name, "Queued");
      EXPECT_EQ(store.collaboration().state()->next_transaction, 1u);
    }
    std::filesystem::remove_all(directory);
  }
}

// Removing history from the authority rejects inverse requests without changing accepted state.
TEST(managed_features, history_free_authority_rejects_undo) {
  authority_type authority{ledger{"Before"}, 1};
  authority.open_session(10);
  managed::collaboration::model_change change;
  change.format_version = managed::collaboration_history_change_version;
  change.history_action = static_cast<std::uint32_t>(managed::collaboration_history_action::undo);
  change.target_operation = 1;
  typename authority_type::change_proposal_type request{
      authority.context(), 10, 2, 0, managed::encode_collaboration_record(change), {}};
  const auto result = authority.submit_change(request, 10, 0);
  EXPECT_EQ(result->status, managed::collaboration_status::failed);
  EXPECT_EQ(authority.sequence(), 0u);
  EXPECT_EQ(authority.read()->name, "Before");
}

// A failed in-place preflight restores both retained undo and the redo branch it tentatively replaced.
TEST(managed_features, rejected_append_restores_redo_and_pinned_values) {
  using local_store = managed::model_store<ledger, history::linear, labels::enabled, traits,
                                           features::collaboration>;
  managed::collaboration_authority<ledger> authority{ledger{"Before"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  managed::store_options options;
  options.decode.max_string_bytes = 128;
  local_store store{ledger{}, managed::make_document_id(), options};
  store.collaborate(std::uint64_t{10}).bind(transport);
  store.synchronize();
  store.execute_transaction("First", [](auto& edit) { edit.root().set_name("One"); })
      .throw_if_failed();
  store.undo();
  const auto saved = store.save();
  EXPECT_EQ(store
                .execute_transaction(std::string(129, 'x'),
                                     [](auto& edit) { edit.root().set_name("Failed"); })
                .status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  EXPECT_EQ(store.redo_label(), "First");
  store.redo();
  EXPECT_EQ(store.read()->name, "One");
}

// A host policy cannot mutate an already encoded candidate through a previously borrowed editor.
TEST(managed_features, authority_preparation_preserves_validation_reentry_guard) {
  authority_type::edit_type* borrowed = nullptr;
  authority_type::command_handler handler = [&](auto& edit, auto) {
    borrowed = &edit;
    edit.root().set_name("After");
  };
  authority_type::change_policy policy = [&](const auto&, const auto&, const auto&, const auto&) {
    EXPECT_THROW(borrowed->update([](auto& value) { value.name = "Bypassed"; }), std::logic_error);
    return true;
  };
  authority_type authority{
      ledger{"Before"}, 1, handler, managed::make_document_id(), {}, {}, policy};
  authority.open_session(10);
  authority_type::change_proposal_type request{authority.context(), 10, 1, 0, {}, {}};
  EXPECT_EQ(authority.submit_change(request, 10, 0)->status, managed::collaboration_status::failed);
  EXPECT_EQ(authority.read()->name, "Before");
  EXPECT_EQ(authority.sequence(), 0u);
}

} // namespace
