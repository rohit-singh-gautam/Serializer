// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/managed_journal_document.hpp>
#include "managed_journal_paths.hpp"

#include <ledger.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using authority_type = managed::collaboration_authority<ledger>;
using bytes = std::vector<std::uint8_t>;

// Export only ordinary model values; collaboration session and outbox data remain in the companion.
bytes encode_json_document(const ledger& value) {
  rohit::full_stream_auto_alloc output;
  value.serialize_out<codec::json>(output);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode one owning user document without importing any client-session checkpoint metadata.
ledger decode_json_document(std::span<const std::uint8_t> input) {
  const auto stream = rohit::make_constant_full_stream(input.data(), input.size());
  return codec::deserialize_exact<ledger, codec::json>(stream);
}

// Capture exact proposals while optionally losing a response after the authority has accepted it.
struct document_transport {
  managed::collaboration_transport<authority_type> direct;
  bool lose_reply{};
  std::vector<bytes> requests;

  // Authenticate this transport independently of the submitted proposal bytes.
  explicit document_transport(authority_type& authority) : direct{authority, std::uint64_t{10}} {}

  // Supply the authority's authenticated collaboration context.
  auto context() {
    return direct.context();
  }

  // Supply the accepted native model and allocation state during initial join.
  auto snapshot() {
    return direct.snapshot();
  }

  // Retrieve accepted changes without resubmitting pending client operations.
  auto accepted_since(std::uint64_t cursor) {
    return direct.accepted_since(cursor);
  }

  // Prefetch durable native identities before disconnected local edits.
  auto reserve_ids(std::uint64_t count) {
    return direct.reserve_ids(count);
  }

  // Record the exact native request and simulate only the response loss, never the authority result.
  auto submit_change(const authority_type::change_proposal_type& request) {
    requests.push_back(managed::encode_collaboration_record(request));
    auto result = direct.submit_change(request);
    if (lose_reply) {
      lose_reply = false;
      throw std::runtime_error{"Lost accepted response"};
    }
    return result;
  }
};

// Keep external-document collaboration qualification isolated from other journals and user files.
class managed_document_collaboration : public testing::Test {
protected:
  std::filesystem::path directory_;
  std::filesystem::path path_;
  authority_type authority_{ledger{"Original"}, 1};
  document_transport transport_{authority_};

  // Create one unique temporary root and authenticate the reusable client session.
  void SetUp() override {
    const auto id = managed::make_document_id();
    directory_ = std::filesystem::temp_directory_path() /
                 ("serializer-document-collaboration-" + std::to_string(id.high) + "-" +
                  std::to_string(id.low));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    path_ = directory_ / "client.json";
    authority_.open_session(10);
  }

  // Delete only this newly created temporary directory after test-local stores release their locks.
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
    EXPECT_FALSE(error) << error.message();
  }

  // Bind the ordinary JSON model codec to the external sidecar API.
  static managed::journal_options json_options() {
    managed::journal_options options;
    options.document_codec = managed::make_journal_document_codec<ledger>(
        "application/json", encode_json_document, decode_json_document);
    return options;
  }

  // Attach without synchronizing; recovery must restore the previous durable session first.
  void attach(store_type& store) {
    store.collaborate(std::uint64_t{10}).bind(transport_);
  }
};

// Full Save retains unsent work; reopening and synchronizing publishes the original native IDs once.
TEST_F(managed_document_collaboration, saved_unsent_change_survives_reopen_and_sends_once) {
  auto options = json_options();
  options.allow_document_retirement = true;
  std::uint32_t child_id{};
  {
    store_type store{ledger{}};
    attach(store);
    store.synchronize();
    store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    store
        .execute_transaction([&](auto& edit) {
          child_id = edit.root().entries().insert(100, {"Pending entry", 500});
        })
        .throw_if_failed();
    EXPECT_EQ(store.collaboration().pending_count(), 1u);
    store.save_journal();
    EXPECT_FALSE(store.retire_journal());
    EXPECT_EQ(store.collaboration().pending_count(), 1u);
    EXPECT_EQ(authority_.sequence(), 0u);
    EXPECT_TRUE(transport_.requests.empty());
  }
  {
    store_type recovered{ledger{}};
    attach(recovered);
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.collaboration().pending_count(), 1u);
    EXPECT_EQ(recovered.read()->entries.at(100).persistent_id, child_id);
    recovered.save_journal();
    EXPECT_EQ(recovered.collaboration().pending_count(), 1u);
    recovered.synchronize();
    EXPECT_EQ(authority_.sequence(), 1u);
    EXPECT_EQ(authority_.read()->entries.at(100).persistent_id, child_id);
    EXPECT_EQ(recovered.collaboration().pending_count(), 0u);
    EXPECT_FALSE(recovered.retire_journal());
    EXPECT_EQ(transport_.requests.size(), 1u);
    recovered.save_journal();
  }
  store_type reopened{ledger{}};
  attach(reopened);
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.collaboration().pending_count(), 0u);
  EXPECT_EQ(reopened.read()->entries.at(100).persistent_id, child_id);
  reopened.synchronize();
  EXPECT_EQ(authority_.sequence(), 1u);
  EXPECT_EQ(transport_.requests.size(), 1u);
}

// Retirement never sends. Unsent discard stays local; acknowledged changes need a new shared inverse.
TEST_F(managed_document_collaboration, retirement_and_discard_do_not_unsend_acknowledged_changes) {
  auto options = json_options(); options.allow_document_retirement = true;
  store_type store{ledger{}};
  attach(store); store.synchronize();
  store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
  store.execute_transaction([](auto& edit) { edit.root().set_name("Unsent"); }).throw_if_failed();
  store.save_journal();
  EXPECT_FALSE(store.retire_journal());
  EXPECT_TRUE(transport_.requests.empty()); EXPECT_EQ(authority_.read()->name, "Original");
  const auto draft = store.collaboration().discard_pending();
  EXPECT_EQ(draft->name, "Unsent"); EXPECT_EQ(store.read()->name, "Original");
  EXPECT_EQ(store.collaboration().pending_count(), 0u); EXPECT_TRUE(transport_.requests.empty());
  store.execute_transaction([](auto& edit) { edit.root().set_name("Shared"); }).throw_if_failed();
  store.synchronize();
  EXPECT_EQ(authority_.read()->name, "Shared"); ASSERT_EQ(transport_.requests.size(), 1u);
  static_cast<void>(store.collaboration().discard_pending());
  EXPECT_EQ(store.read()->name, "Shared"); EXPECT_EQ(authority_.read()->name, "Shared");
  EXPECT_EQ(transport_.requests.size(), 1u); EXPECT_FALSE(store.retire_journal());
  store.undo();
  EXPECT_EQ(store.read()->name, "Original"); EXPECT_EQ(authority_.read()->name, "Shared");
  store.synchronize();
  EXPECT_EQ(authority_.read()->name, "Original"); EXPECT_EQ(transport_.requests.size(), 2u);
}

// Saving an accepted request with a lost reply preserves identical retry bytes across restart.
TEST_F(managed_document_collaboration, save_and_reopen_preserve_exact_lost_reply_retry) {
  auto options = json_options(); options.allow_document_retirement = true;
  bytes in_flight;
  {
    store_type store{ledger{}};
    attach(store);
    store.synchronize();
    store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Once"); }).throw_if_failed();
    transport_.lose_reply = true;
    EXPECT_THROW(store.synchronize(), std::runtime_error);
    in_flight = store.collaboration().state()->in_flight;
    ASSERT_FALSE(in_flight.empty());
    EXPECT_THROW(static_cast<void>(store.collaboration().discard_pending()), std::logic_error);
    store.save_journal();
    EXPECT_FALSE(store.retire_journal());
    EXPECT_EQ(store.collaboration().state()->in_flight, in_flight);
    EXPECT_EQ(authority_.sequence(), 1u);
  }
  store_type recovered{ledger{}};
  attach(recovered);
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.collaboration().state()->in_flight, in_flight);
  recovered.synchronize();
  ASSERT_EQ(transport_.requests.size(), 2u);
  EXPECT_EQ(transport_.requests.front(), transport_.requests.back());
  EXPECT_EQ(authority_.sequence(), 1u);
  EXPECT_EQ(authority_.read()->name, "Once");
  EXPECT_TRUE(recovered.collaboration().state()->in_flight.empty());
  EXPECT_EQ(recovered.collaboration().pending_count(), 0u);
}

// Equal visible JSON bytes still select the newer durable in-flight metadata after interrupted Save.
TEST_F(managed_document_collaboration, pending_manifest_selects_metadata_only_saved_generation) {
  bool armed{};
  auto options = json_options();
  options.fault_injector = [&](auto point) {
    if (armed && point == managed::journal_io_event::after_replace) {
      throw std::runtime_error{"Interrupted metadata-only Save"};
    }
  };
  bytes in_flight;
  {
    store_type store{ledger{}};
    attach(store);
    store.synchronize();
    store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Once"); }).throw_if_failed();
    store.save_journal();
    transport_.lose_reply = true;
    EXPECT_THROW(store.synchronize(), std::runtime_error);
    in_flight = store.collaboration().state()->in_flight;
    ASSERT_FALSE(in_flight.empty());
    armed = true;
    EXPECT_THROW(store.save_journal(), managed::journal_indeterminate_error);
  }
  armed = false;
  store_type recovered{ledger{}};
  attach(recovered);
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.collaboration().state()->in_flight, in_flight);
  EXPECT_FALSE(std::filesystem::exists(journal_test_paths::manifest(path_, true)));
  recovered.synchronize();
  ASSERT_EQ(transport_.requests.size(), 2u);
  EXPECT_EQ(transport_.requests.front(), transport_.requests.back());
  EXPECT_EQ(authority_.sequence(), 1u);
}
// Prepared Save pins pending transport/session state and prevents effects until explicit confirmation.
TEST_F(managed_document_collaboration, prepared_save_retains_unsent_session_and_blocks_transport) {
  auto options = json_options();
  options.origin = {1};
  managed::journal_prepared_save prepared;
  bytes checkpoint;
  std::uint32_t child_id{};
  {
    store_type store{ledger{}};
    attach(store);
    store.synchronize();
    store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    store.execute_transaction([&](auto& tx) {
      child_id = tx.root().entries().insert(100, {"Pending", 500});
    }).throw_if_failed();
    checkpoint = store.save();
    prepared = store.prepare_journal_save(bytes{2});
    EXPECT_THROW(store.synchronize(), std::logic_error);
    EXPECT_TRUE(transport_.requests.empty());
    EXPECT_EQ(store.collaboration().pending_count(), 1u);
  }
  {
    store_type recovered{ledger{}};
    attach(recovered);
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), checkpoint);
    EXPECT_EQ(recovered.collaboration().pending_count(), 1u);
    EXPECT_TRUE(recovered.prepared_journal_save());
    EXPECT_THROW(recovered.synchronize(), std::logic_error);
    EXPECT_TRUE(transport_.requests.empty());
    const managed::journal_publication_evidence evidence{prepared.document.size(),
        rohit::make_digest<rohit::digest_algorithm::sha256>(prepared.document)};
    EXPECT_TRUE(recovered.confirm_journal_save(prepared.token, evidence));
    EXPECT_EQ(recovered.save(), checkpoint);
    recovered.synchronize();
    EXPECT_EQ(authority_.sequence(), 1u);
    EXPECT_EQ(authority_.read()->entries.at(100).persistent_id, child_id);
    recovered.save_journal();
  }
  store_type reopened{ledger{}};
  attach(reopened);
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.journal_origin(), (bytes{2}));
  EXPECT_EQ(reopened.collaboration().pending_count(), 0u);
}

// Confirmation preserves the exact in-flight request after the host loses a transport acknowledgement.
TEST_F(managed_document_collaboration, prepared_save_preserves_in_flight_exact_retry) {
  auto options = json_options();
  bytes in_flight;
  managed::journal_prepared_save prepared;
  {
    store_type store{ledger{}};
    attach(store);
    store.synchronize();
    store.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    store.execute_transaction([](auto& tx) { tx.root().set_name("Pending"); }).throw_if_failed();
    transport_.lose_reply = true;
    EXPECT_THROW(store.synchronize(), std::runtime_error);
    in_flight = store.collaboration().state()->in_flight;
    ASSERT_FALSE(in_flight.empty());
    prepared = store.prepare_journal_save(bytes{3});
  }
  store_type recovered{ledger{}};
  attach(recovered);
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.collaboration().state()->in_flight, in_flight);
  const managed::journal_publication_evidence evidence{prepared.document.size(),
      rohit::make_digest<rohit::digest_algorithm::sha256>(prepared.document)};
  EXPECT_TRUE(recovered.confirm_journal_save(prepared.token, evidence));
  EXPECT_EQ(recovered.collaboration().state()->in_flight, in_flight);
  recovered.synchronize();
  ASSERT_EQ(transport_.requests.size(), 2u);
  EXPECT_EQ(transport_.requests.front(), transport_.requests.back());
  EXPECT_EQ(authority_.sequence(), 1u);
}
} // namespace
