// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <document.hpp>
#include <rohit/managed.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <vector>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace managed = rohit::managed;
using document = runtime_features::document_data;
using traits = managed::model_traits<document>;
using store_type = managed::model_store<document>;

// Represent a released fingerprint whose text convention is upgraded by an explicit converter.
struct previous_traits : traits {
  static constexpr std::string_view schema_id = "serializer.example.documents.previous";
};
using previous_store = managed::model_store<document, managed::history_mode::linear,
                                           managed::history_labels::disabled, previous_traits>;

// Initialize ordinary model values; the store assigns document-local persistent identities.
document draft() {
  document value;
  value.text = "Draft";
  return value;
}

// Assert observable behavior so this example is also a runnable integration fixture.
void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

// Apply one text edit as a durable history step through the generated root editor.
void edit_text(auto& store, std::string text) {
  store.execute_transaction([&](auto& edit) {
    edit.root().set_text(std::move(text));
  }).throw_if_failed();
}

// Upgrade the text convention while preserving every generated storage identity.
traits::storage_type upgrade(const traits::storage_type& previous) {
  auto result = previous;
  traits::payload(result).text = "Reviewed: " + traits::payload(previous).text;
  return result;
}
} // namespace

// Demonstrate memory-only state, copying, observers, navigation, patches and whole-history upgrades.
int main() {
  try {
    store_type store{draft(), {101, 102}};
    std::size_t notifications = 0;
    store.set_publication_callback([&](const auto& event) {
      ++notifications;
      require(event.after == store.read(), "Observer must see accepted immutable state");
      require(event.changed_ids_precise, "Generated models expose precise durable changed IDs");
      if (event.kind == managed::publication_kind::runtime_update) {
        require(event.changed_ids.empty(), "Runtime updates have no durable changed entities");
      } else {
        require(event.changed_ids.size() == 1, "This text edit changes only its document entity");
      }
    });
    const auto baseline = store.save();
    const auto pinned = store.read();
    store.update_runtime([](auto& edit) { edit.root().set_lookup_cache(77); }).throw_if_failed();
    require(traits::payload(*pinned).lookup_cache == 0, "Pinned values must remain immutable");
    require(store.save() == baseline, "Runtime changes must not change durable bytes");
    auto ordinary_copy = store.clone_value();
    auto normal_copy = ordinary_copy;
    require(normal_copy.lookup_cache == 77, "Normal copying must preserve transient state");
    require(store.clone_value({managed::runtime_copy_policy::reset_to_defaults}).lookup_cache == 0,
            "Explicit reset cloning must restore defaults");
    edit_text(store, "Final draft");
    require(traits::payload(*store.read()).lookup_cache == 0, "Durable edits must invalidate cache keys");
    require(store.history_cursor() == 1, "One durable change must create one history step");
    const auto patch = store.save_delta(baseline);
    store_type receiver{draft(), {103, 104}};
    receiver.load_delta(baseline, patch);
    require(receiver.save() == store.save(), "Delta transfer must preserve complete history");
    receiver.undo();
    require(traits::payload(*receiver.read()).text == "Draft", "Transferred undo must work");
    managed::snapshot_delta_chain chain{baseline, baseline.size() + 4096, 2};
    chain.append(store.save());
    require(chain.current() == store.save(), "Bounded patch chains must reconstruct the tip");

    managed::store_options admission;
    admission.max_resident_bytes = 128 * 1024;
    admission.allocation_resource = std::make_shared<std::pmr::synchronized_pool_resource>();
    store_type bounded{draft(), {151, 152}, admission};
    auto older_pin = bounded.read();
    const auto base_resident = bounded.resident_bytes();
    bounded.update_runtime([](auto& edit) {
      edit.root().set_lookup_buffer(std::vector<std::uint8_t>(1024, 1));
    }).throw_if_failed();
    require(bounded.resident_bytes() > base_resident, "Owning cache and old pins must be charged");
    const auto with_pin = bounded.resident_bytes();
    older_pin.reset();
    require(bounded.resident_bytes() < with_pin, "Releasing an old pin must release its charge");
    const auto accepted = bounded.read();
    const auto too_large = bounded.update_runtime([](auto& edit) {
      edit.root().set_lookup_buffer(std::vector<std::uint8_t>(256 * 1024, 1));
    });
    require(too_large.status == managed::transaction_status::failed,
            "Oversized runtime state must fail admission");
    require(bounded.read() == accepted, "Failed admission must preserve accepted state");

    previous_store previous{draft(), {201, 202}};
    edit_text(previous, "Final draft");
    previous.undo();
    store_type migrated{draft(), {203, 204}};
    migrated.load_migrated<document, previous_traits>(previous.save(), upgrade);
    require(traits::payload(*migrated.read()).text == "Reviewed: Draft", "Current state must migrate");
    migrated.redo();
    require(traits::payload(*migrated.read()).text == "Reviewed: Final draft", "Redo state must migrate");

    managed::model_store<document, managed::history_mode::tree> tree{draft(), {301, 302}};
    const auto revision = tree.current_revision();
    edit_text(tree, "A branch");
    tree.undo();
    require(tree.current_revision() == revision, "Tree navigation must expose selected revision");

    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() /
                           ("serializer-runtime-example-" + std::to_string(nonce));
    std::filesystem::create_directory(directory);
    struct cleanup {
      std::filesystem::path path;
      // Clean only this example's unique temporary directory after journal handles are released.
      ~cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup_directory{directory};
    {
      previous_store journaled{draft(), {401, 402}};
      journaled.create_journal(directory / "previous.document");
      edit_text(journaled, "Final draft");
    }
    {
      store_type upgraded{draft(), {403, 404}};
      upgraded.recover_migrated_journal<document, previous_traits>(
          directory / "previous.document", directory / "current.document", upgrade);
      require(traits::payload(*upgraded.read()).text == "Reviewed: Final draft",
              "Recovered edits must migrate before durable publication");
      upgraded.undo();
      require(traits::payload(*upgraded.read()).text == "Reviewed: Draft", "Journal undo must migrate");
    }
    managed::store_options delta_options;
    delta_options.delta_journal = true;
    delta_options.max_delta_chain = 2;
    const auto delta_path = directory / "delta.document";
    std::vector<std::uint8_t> expected_delta_state;
    {
      auto value = draft();
      value.text.assign(16 * 1024, 'a');
      store_type journaled{value, {501, 502}, delta_options};
      journaled.create_journal(delta_path);
      for (std::size_t index = 0; index < 5; ++index) {
        value.text[index] = 'b';
        edit_text(journaled, value.text);
      }
      expected_delta_state = journaled.save();
    }
    {
      store_type recovered{draft(), {503, 504}, delta_options};
      recovered.recover_journal(delta_path);
      require(recovered.save() == expected_delta_state, "Delta journal replay must retain full history");
      recovered.undo();
      recovered.redo();
      recovered.save_journal();
    }
    std::cout << "runtime state, clone policies, notifications, navigation, deltas and migration passed; "
              << notifications << " accepted notifications\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
