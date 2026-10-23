// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/file_stream.hpp>
#include <rohit/managed_journal_document.hpp>
#include "managed_journal_paths.hpp"
#include "../src/native_file_path.hpp"

#include <ledger.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal_stream.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using bytes = std::vector<std::uint8_t>;
using mode = managed::journal_storage_mode;
using event = managed::journal_io_event;

// Construct independent host publication evidence from the bytes actually written by the host.
managed::journal_publication_evidence publication_evidence(std::span<const std::uint8_t> document) {
  return {document.size(), rohit::make_digest<rohit::digest_algorithm::sha256>(document)};
}

// Read exact document or companion bytes while retaining any insignificant external whitespace.
bytes read_document_bytes(const std::filesystem::path& path) {
  const rohit::detail::native_file_path native{path};
  std::ifstream input{native.get(), std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot read external journal fixture"};
  }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Replace only a closed fixture to prepare an external document or simulate interrupted publication.
void write_document_bytes(const std::filesystem::path& path, const bytes& value) {
  const rohit::detail::native_file_path native{path};
  std::ofstream output{native.get(), std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
  if (!output) {
    throw std::runtime_error{"Cannot write external journal fixture"};
  }
}

// Use generated owning codecs without coupling the journal adapter to any particular document format.
template <template <codec::serialize_type> class Protocol>
bytes encode_document(const ledger& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require an exact external message, including only the protocol's permitted trailing whitespace.
template <template <codec::serialize_type> class Protocol>
ledger decode_document(std::span<const std::uint8_t> input) {
  const auto stream = rohit::make_constant_full_stream(input.data(), input.size());
  return codec::deserialize_exact<ledger, Protocol>(stream);
}

// Give every test its own operating-system temporary directory and release stores before cleanup.
class managed_document_journal : public testing::Test {
protected:
  std::filesystem::path directory_;
  std::filesystem::path path_;

  // Derive a unique child of the temporary directory without touching repository or user files.
  void SetUp() override {
    const auto id = managed::make_document_id();
    directory_ =
        std::filesystem::temp_directory_path() /
        ("serializer-document-journal-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    path_ = directory_ / "ledger.json";
  }

  // Remove only the uniquely created fixture directory once all test-local handles have closed.
  void TearDown() override {
    std::error_code error;
    const rohit::detail::native_file_path native{directory_, true};
    std::filesystem::remove_all(native.get(), error);
    EXPECT_FALSE(error) << error.message();
  }

  // Supply a JSON implementation through the same public callbacks used by arbitrary external codecs.
  static managed::journal_options json_options() {
    managed::journal_options options;
    options.document_codec = managed::make_journal_document_codec<ledger>(
        "application/json", encode_document<codec::json>, decode_document<codec::json>);
    return options;
  }

  // Locate the sole active companion after successful generation cleanup.
  std::filesystem::path companion_path() const {
    return journal_test_paths::first_companion(path_);
  }
};

// The user document remains ordinary JSON while the journal retains IDs, edits and full undo state.
TEST_F(managed_document_journal, json_edits_recover_without_rewriting_main_until_save) {
  const auto options = json_options();
  bytes base;
  bytes expected;
  managed::document_id document;
  std::uint32_t child_id{};
  {
    store_type store{ledger{"Base", {{10, {"Entry", 200}}}}};
    document = store.document();
    child_id = store.read()->entries.at(10).persistent_id;
    store.create_journal(path_, mode::sidecar, options);
    base = read_document_bytes(path_);
    EXPECT_EQ(decode_document<codec::json>(base).name, "Base");
    store.execute_transaction([](auto& edit) { edit.root().set_name("Edited"); }).throw_if_failed();
    EXPECT_EQ(read_document_bytes(path_), base);
    EXPECT_TRUE(store.journal_dirty());
    expected = store.save();
  }
  {
    store_type recovered{ledger{}};
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), expected);
    EXPECT_EQ(recovered.document().high, document.high);
    EXPECT_EQ(recovered.document().low, document.low);
    EXPECT_EQ(recovered.read()->persistent_id, 1u);
    EXPECT_EQ(recovered.read()->entries.at(10).persistent_id, child_id);
    recovered.undo();
    EXPECT_EQ(recovered.read()->name, "Base");
    recovered.redo();
    recovered.save_journal();
    EXPECT_FALSE(recovered.journal_dirty());
    EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path_)).name, "Edited");
  }
  store_type reopened{ledger{}};
  reopened.recover_journal(path_, options);
  EXPECT_FALSE(reopened.journal_dirty());
  reopened.undo();
  EXPECT_EQ(reopened.read()->name, "Base");
  EXPECT_EQ(reopened.read()->entries.at(10).persistent_id, child_id);
}

// Existing documents attach without losing byte presentation or trusting their exported identities.
TEST_F(managed_document_journal, attaches_existing_json_byte_for_byte) {
  ledger existing{"Existing", {{10, {"Entry", 200}}}};
  existing.persistent_id = 91;
  existing.entries.at(10).persistent_id = 92;
  auto external = encode_document<codec::json>(existing);
  external.insert(external.end(), {'\n', ' ', '\t'});
  write_document_bytes(path_, external);
  const auto options = json_options();
  {
    store_type store{ledger{"Existing", {{10, {"Entry", 200}}}}};
    store.create_journal(path_, mode::sidecar, options);
    EXPECT_EQ(read_document_bytes(path_), external);
    EXPECT_EQ(store.read()->persistent_id, 1u);
    EXPECT_EQ(store.read()->entries.at(10).persistent_id, 2u);
  }
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(read_document_bytes(path_), external);
  EXPECT_EQ(recovered.read()->entries.at(10).persistent_id, 2u);
}

// External format callbacks are generic; string-keyed binary documents use the same durable workflow.
TEST_F(managed_document_journal, supports_application_selected_binary_document_format) {
  managed::journal_options options;
  options.document_codec = managed::make_journal_document_codec<ledger>(
      "application/x-serializer-binary-string", encode_document<codec::binary_string>,
      decode_document<codec::binary_string>);
  {
    store_type store{ledger{"Binary document"}};
    store.create_journal(path_, mode::sidecar, options);
    EXPECT_EQ(decode_document<codec::binary_string>(read_document_bytes(path_)).name,
              "Binary document");
    store.execute_transaction([](auto& edit) { edit.root().set_name("Saved binary"); })
        .throw_if_failed();
    store.save_journal();
  }
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.read()->name, "Saved binary");
}

// Sparse edits beyond the old snapshot-delta chain bound do not copy the large unchanged root.
TEST_F(managed_document_journal, companion_stays_small_across_more_than_32_edits) {
  constexpr std::size_t document_text_bytes = 256 * 1024;
  constexpr std::size_t edits = 40;
  constexpr std::size_t changed_offset = document_text_bytes / 2;
  const auto options = json_options();
  std::string text(document_text_bytes, 'a');
  bytes expected;
  {
    store_type store{ledger{text}};
    store.create_journal(path_, mode::sidecar, options);
    const auto original = read_document_bytes(path_);
    EXPECT_LT(std::filesystem::file_size(companion_path()), document_text_bytes / 2);
    for (std::size_t index = 0; index != edits; ++index) {
      text[changed_offset + index] = 'b';
      store.execute_transaction([&](auto& edit) { edit.root().set_name(text); }).throw_if_failed();
    }
    EXPECT_EQ(read_document_bytes(path_), original);
    EXPECT_LT(std::filesystem::file_size(companion_path()), document_text_bytes / 2);
    expected = store.save();
  }
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.save(), expected);
  EXPECT_EQ(recovered.read()->name, text);
  recovered.undo();
  EXPECT_EQ(recovered.read()->name[changed_offset + edits - 1], 'a');
}

// Hash and format binding reject even semantically equivalent substitutions before store publication.
TEST_F(managed_document_journal,
       rejects_tampered_document_and_wrong_format_without_changing_store) {
  const auto options = json_options();
  {
    store_type store{ledger{"Original"}};
    store.create_journal(path_, mode::sidecar, options);
  }
  const auto original = read_document_bytes(path_);
  auto tampered = original;
  tampered.push_back('\n');
  write_document_bytes(path_, tampered);
  store_type candidate{ledger{"Untouched"}};
  const auto pin = candidate.read();
  const auto before = candidate.save();
  EXPECT_THROW(candidate.recover_journal(path_, options), std::invalid_argument);
  EXPECT_EQ(candidate.read(), pin);
  EXPECT_EQ(candidate.save(), before);
  write_document_bytes(path_, original);
  auto wrong_format = options;
  wrong_format.document_codec = managed::make_journal_document_codec<ledger>(
      "application/another-format", encode_document<codec::json>, decode_document<codec::json>);
  EXPECT_THROW(candidate.recover_journal(path_, wrong_format), std::invalid_argument);
  EXPECT_EQ(candidate.read(), pin);
  EXPECT_NO_THROW(candidate.recover_journal(path_, options));
}

// A live document substitution rejects Save before any publication work and leaves retry available.
TEST_F(managed_document_journal, live_document_tamper_rejects_save_without_fencing_writer) {
  bool watch_io{};
  std::size_t io_events{};
  auto options = json_options();
  options.fault_injector = [&](auto) {
    if (watch_io) {
      ++io_events;
    }
  };
  store_type store{ledger{"Saved"}};
  store.create_journal(path_, mode::sidecar, options);
  const auto original = read_document_bytes(path_);
  store.execute_transaction([](auto& edit) { edit.root().set_name("Current"); }).throw_if_failed();
  const auto pin = store.read();
  const auto sequence = store.journal_sequence();
  auto tampered = original;
  tampered.push_back('\n');
  write_document_bytes(path_, tampered);
  watch_io = true;
  EXPECT_THROW(store.save_journal(), std::invalid_argument);
  EXPECT_EQ(io_events, 0u);
  EXPECT_EQ(store.read(), pin);
  EXPECT_EQ(store.journal_sequence(), sequence);
  EXPECT_EQ(read_document_bytes(path_), tampered);
  EXPECT_FALSE(store.journal_needs_recovery());
  EXPECT_TRUE(store.journal_dirty());
  write_document_bytes(path_, original);
  watch_io = false;
  EXPECT_NO_THROW(store.save_journal());
  EXPECT_FALSE(store.journal_dirty());
  EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path_)).name, "Current");
}

// Failing or lossy callbacks reject Save before any replacement and leave a healthy writer retryable.
TEST_F(managed_document_journal, failed_and_lossy_encoders_preserve_saved_file_and_live_writer) {
  bool fail{};
  bool lose_value{};
  auto options = json_options();
  options.document_codec = managed::make_journal_document_codec<ledger>(
      "application/json",
      [&](const ledger& value) {
        if (fail) {
          throw std::runtime_error{"Application encoder failed"};
        }
        auto encoded = value;
        if (lose_value) {
          encoded.name.clear();
        }
        return encode_document<codec::json>(encoded);
      },
      decode_document<codec::json>);
  store_type store{ledger{"Saved"}};
  store.create_journal(path_, mode::sidecar, options);
  const auto original = read_document_bytes(path_);
  store.execute_transaction([](auto& edit) { edit.root().set_name("Current"); }).throw_if_failed();
  fail = true;
  EXPECT_THROW(store.save_journal(), std::runtime_error);
  EXPECT_EQ(read_document_bytes(path_), original);
  EXPECT_FALSE(store.journal_needs_recovery());
  EXPECT_TRUE(store.journal_dirty());
  fail = false;
  lose_value = true;
  EXPECT_THROW(store.save_journal(), std::invalid_argument);
  EXPECT_EQ(read_document_bytes(path_), original);
  EXPECT_FALSE(store.journal_needs_recovery());
  lose_value = false;
  EXPECT_NO_THROW(store.save_journal());
  EXPECT_FALSE(store.journal_dirty());
}

// Preparation failures retain the old saved baseline; uncertain publication selects the matching pair.
TEST_F(managed_document_journal, interrupted_save_recovers_matching_document_generation) {
  for (const auto point : {event::after_sidecar_flush, event::after_base_flush,
                           event::before_replace, event::after_replace, event::after_publish}) {
    SCOPED_TRACE(static_cast<int>(point));
    const auto path = directory_ / ("fault-" + std::to_string(static_cast<int>(point)) + ".json");
    bool armed{};
    auto options = json_options();
    options.fault_injector = [&](auto observed) {
      if (armed && observed == point) {
        throw std::runtime_error{"Interrupted external Save"};
      }
    };
    const bool published = point == event::after_replace || point == event::after_publish;
    {
      store_type store{ledger{"Saved"}};
      store.create_journal(path, mode::sidecar, options);
      store.execute_transaction([](auto& edit) { edit.root().set_name("Current"); })
          .throw_if_failed();
      armed = true;
      if (published) {
        EXPECT_THROW(store.save_journal(), managed::journal_indeterminate_error);
      } else {
        EXPECT_THROW(store.save_journal(), std::runtime_error);
      }
      EXPECT_EQ(store.journal_needs_recovery(), published);
      EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path)).name,
                published ? "Current" : "Saved");
    }
    armed = false;
    store_type recovered{ledger{}};
    recovered.recover_journal(path, options);
    EXPECT_EQ(recovered.read()->name, "Current");
    EXPECT_EQ(recovered.journal_dirty(), !published);
    EXPECT_FALSE(std::filesystem::exists(journal_test_paths::manifest(path, true)));
    recovered.undo();
    EXPECT_EQ(recovered.read()->name, "Saved");
  }
}

// A prepared pending manifest cannot replace the old decision when the data rename never took effect.
TEST_F(managed_document_journal, pending_manifest_with_old_document_recovers_old_saved_baseline) {
  bool armed{};
  auto options = json_options();
  options.fault_injector = [&](auto observed) {
    if (armed && observed == event::after_replace) {
      throw std::runtime_error{"Interrupted manifest publication"};
    }
  };
  bytes original;
  {
    store_type store{ledger{"Saved"}};
    store.create_journal(path_, mode::sidecar, options);
    original = read_document_bytes(path_);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Current"); })
        .throw_if_failed();
    armed = true;
    EXPECT_THROW(store.save_journal(), managed::journal_indeterminate_error);
  }
  ASSERT_TRUE(std::filesystem::exists(journal_test_paths::manifest(path_, true)));
  // Reproduce the other allowed crash ordering using the still-retained old data generation.
  write_document_bytes(path_, original);
  armed = false;
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.read()->name, "Current");
  EXPECT_TRUE(recovered.journal_dirty());
  EXPECT_EQ(read_document_bytes(path_), original);
  EXPECT_FALSE(std::filesystem::exists(journal_test_paths::manifest(path_, true)));
  EXPECT_NO_THROW(recovered.save_journal());
}

// Discard preserves exact saved document presentation and never reuses abandoned native identities.
TEST_F(managed_document_journal, discard_restores_saved_root_and_preserves_allocation_marks) {
  const auto options = json_options();
  const ledger saved{"Saved", {{10, {"Existing", 200}}}};
  auto original = encode_document<codec::json>(saved);
  original.insert(original.end(), {'\n', ' ', '\t'});
  write_document_bytes(path_, original);
  std::uint32_t abandoned_id{};
  std::uint32_t reserved_id{};
  {
    store_type store{saved};
    store.create_journal(path_, mode::sidecar, options);
    EXPECT_EQ(read_document_bytes(path_), original);
    store
        .execute_transaction([&](auto& edit) {
          abandoned_id = edit.root().entries().insert(20, {"Abandoned", 300});
        })
        .throw_if_failed();
    store
        .execute_transaction([&](auto& edit) {
          reserved_id = edit.create_id();
          edit.revert();
        })
        .throw_if_failed();
    store.discard_journal_changes();
    EXPECT_FALSE(store.journal_dirty());
    EXPECT_EQ(store.read()->entries.size(), 1u);
    EXPECT_EQ(store.read()->entries.at(10).persistent_id, 2u);
    EXPECT_GT(reserved_id, abandoned_id);
    EXPECT_EQ(read_document_bytes(path_), original);
  }
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.read()->name, "Saved");
  EXPECT_EQ(recovered.read()->entries.size(), 1u);
  EXPECT_EQ(read_document_bytes(path_), original);
  recovered
      .execute_transaction([&](auto& edit) {
        EXPECT_GT(edit.root().entries().insert(30, {"New", 400}), reserved_id);
      })
      .throw_if_failed();
}

// External tree snapshots retain the selected cursor, labels, siblings and burned allocation gaps.
TEST_F(managed_document_journal, full_save_restores_external_tree_branches_and_allocation) {
  using tree_store =
      managed::model_store<ledger, managed::history_mode::tree, managed::history_labels::enabled>;
  const auto options = json_options();
  std::uint64_t first{}, tip{}, sibling{};
  std::uint32_t child_id{}, reserved_id{};
  bytes expected;
  {
    tree_store store{ledger{"Base", {{10, {"Existing", 200}}}}};
    child_id = store.read()->entries.at(10).persistent_id;
    store.create_journal(path_, mode::sidecar, options);
    first =
        store.execute_transaction("First", [](auto& tx) { tx.root().set_name("One"); }).revision;
    tip =
        store.execute_transaction("Tip", [](auto& tx) { tx.root().set_name("One tip"); }).revision;
    store.undo();
    store.undo();
    sibling =
        store.execute_transaction("Sibling", [](auto& tx) { tx.root().set_name("Two"); }).revision;
    store
        .execute_transaction([&](auto& tx) {
          reserved_id = tx.create_id();
          tx.revert();
        })
        .throw_if_failed();
    store.checkout(first);
    store.save_journal();
    expected = store.save();
    EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path_)).name, "One");
  }
  tree_store reopened{ledger{}};
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.save(), expected);
  EXPECT_EQ(reopened.current_revision(), first);
  EXPECT_EQ(reopened.undo_label(), "First");
  EXPECT_EQ(reopened.read()->entries.at(10).persistent_id, child_id);
  EXPECT_EQ(reopened.redo_children(), (std::vector<std::uint64_t>{tip}));
  EXPECT_EQ(reopened.redo_label(tip), "Tip");
  reopened.undo();
  EXPECT_EQ(reopened.redo_children(), (std::vector<std::uint64_t>{first, sibling}));
  EXPECT_EQ(reopened.redo_label(sibling), "Sibling");
  reopened.redo(sibling);
  EXPECT_EQ(reopened.read()->name, "Two");
  reopened
      .execute_transaction(
          [&](auto& tx) { EXPECT_GT(tx.root().entries().insert(20, {"Fresh", 300}), reserved_id); })
      .throw_if_failed();
}

// Saved byte identity and codec selection reject stale documents before application decoding.
TEST_F(managed_document_journal, stale_document_binding_rejects_before_payload_decoder) {
  std::size_t decoded{};
  auto options = json_options();
  options.document_codec = managed::make_journal_document_codec<ledger>(
      "application/json", encode_document<codec::json>, [&](std::span<const std::uint8_t> input) {
        ++decoded;
        return decode_document<codec::json>(input);
      });
  {
    store_type store{ledger{"Original"}};
    store.create_journal(path_, mode::sidecar, options);
  }
  const auto original = read_document_bytes(path_);
  auto altered = original;
  const std::string_view text{reinterpret_cast<const char*>(altered.data()), altered.size()};
  const auto position = text.find("Original");
  ASSERT_NE(position, std::string_view::npos);
  altered[position] = 'X';
  write_document_bytes(path_, altered);
  store_type destination{ledger{"Untouched"}};
  const auto before = destination.save();
  decoded = 0;
  EXPECT_THROW(destination.recover_journal(path_, options), std::invalid_argument);
  EXPECT_EQ(decoded, 0u);
  EXPECT_EQ(destination.save(), before);
  write_document_bytes(path_, original);
  auto wrong = options;
  wrong.document_codec = managed::make_journal_document_codec<ledger>(
      "application/another-format", encode_document<codec::json>,
      [&](std::span<const std::uint8_t> input) {
        ++decoded;
        return decode_document<codec::json>(input);
      });
  EXPECT_THROW(destination.recover_journal(path_, wrong), std::invalid_argument);
  EXPECT_EQ(decoded, 0u);
  EXPECT_EQ(destination.save(), before);
  const auto companion = companion_path();
  const auto original_companion = read_document_bytes(companion);
  constexpr auto native_header_bytes = 9 * managed::detail::wire_word_bytes;
  constexpr auto checksum_offset = native_header_bytes - managed::detail::wire_word_bytes;
  constexpr auto generation_offset = 4 * managed::detail::wire_word_bytes;
  constexpr auto payload_offset = native_header_bytes + managed::detail::frame_header_bytes;
  ASSERT_GT(original_companion.size(), payload_offset);
  for (unsigned fault = 0; fault != 3; ++fault) {
    SCOPED_TRACE(fault);
    auto damaged = original_companion;
    if (fault == 0) {
      // A valid header CRC cannot authorize a different selected generation.
      damaged[generation_offset] ^= 1;
      bytes checksum;
      managed::detail::put_word(
          checksum, managed::detail::checksum(std::span{damaged}.first(checksum_offset)));
      std::copy(checksum.begin(), checksum.end(), damaged.begin() + checksum_offset);
    } else if (fault == 1) {
      damaged[payload_offset] ^= 1;
    } else {
      damaged.resize(payload_offset);
    }
    write_document_bytes(companion, damaged);
    decoded = 0;
    EXPECT_THROW(destination.recover_journal(path_, options), std::invalid_argument);
    EXPECT_EQ(decoded, 0u);
    EXPECT_EQ(destination.save(), before);
  }
  write_document_bytes(companion, original_companion);
  EXPECT_NO_THROW(destination.recover_journal(path_, options));
  EXPECT_GT(decoded, 0u);
}

// A closed legacy external layout remains writable without creating a second competing lock namespace.
TEST_F(managed_document_journal, legacy_flat_layout_recovers_and_stays_flat) {
  const auto options = json_options();
  bytes expected;
  {
    store_type store{ledger{"Saved"}};
    store.create_journal(path_, mode::sidecar, options);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Changed"); }).throw_if_failed();
    expected = store.save();
  }
  const auto folder = journal_test_paths::directory(path_);
  // Build an old-runtime fixture only after every native handle has closed.
  std::vector<std::filesystem::path> components;
  for (const auto& item : std::filesystem::directory_iterator{folder}) { components.push_back(item.path()); }
  for (const auto& component : components) {
    const auto name = component.filename().string();
    auto destination = path_;
    if (name == "manifest") { destination += ".journal"; }
    else if (name == "writer.lock") { destination += ".lock"; }
    else if (name.starts_with("changes-")) { destination += ".journal-" + name.substr(std::string_view{"changes-"}.size()); }
    else { FAIL() << "Unexpected legacy fixture component: " << name; }
    std::filesystem::rename(component, destination);
  }
  ASSERT_TRUE(std::filesystem::remove(folder));
  {
    store_type recovered{ledger{}};
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), expected);
    recovered.save_journal();
    EXPECT_EQ(recovered.read()->name, "Changed");
    EXPECT_FALSE(std::filesystem::exists(folder));
  }
  store_type reopened{ledger{}};
  EXPECT_NO_THROW(reopened.recover_journal(path_, options));
  EXPECT_EQ(reopened.read()->name, "Changed");
}

// Grouping includes manifest publication and staging, without changing ordinary application bytes.
TEST_F(managed_document_journal, document_metadata_stays_inside_one_directory) {
  auto options = json_options();
  const auto folder = journal_test_paths::directory(path_);
  options.fault_injector = [&](event) {
    for (const auto& item : std::filesystem::directory_iterator{directory_}) {
      EXPECT_TRUE(item.path() == path_ || item.path() == folder) << item.path();
    }
  };
  {
    store_type store{ledger{"Saved"}};
    store.create_journal(path_, mode::sidecar, options);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Changed"); }).throw_if_failed();
    store.save_journal();
  }
  EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path_)).name, "Changed");
  EXPECT_TRUE(std::filesystem::exists(folder / "manifest"));
  EXPECT_TRUE(std::filesystem::exists(folder / "writer.lock"));
  EXPECT_EQ(static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator{folder},
      std::filesystem::directory_iterator{})), 3U);
}

// An ambiguous flat/directory pair is rejected before any decode or publication can choose wrong state.
TEST_F(managed_document_journal, conflicting_layouts_are_not_guessed_or_modified) {
  const auto options = json_options();
  {
    store_type store{ledger{"Saved"}};
    store.create_journal(path_, mode::sidecar, options);
  }
  const auto document = read_document_bytes(path_);
  const auto metadata = read_document_bytes(journal_test_paths::manifest(path_));
  auto lock = path_;
  lock += ".lock";
  write_document_bytes(lock, {});
  store_type recovered{ledger{}};
  EXPECT_THROW(recovered.recover_journal(path_, options), std::invalid_argument);
  EXPECT_EQ(read_document_bytes(path_), document);
  EXPECT_EQ(read_document_bytes(journal_test_paths::manifest(path_)), metadata);
}

// Native path composition preserves non-ASCII filenames without converting through the active code page.
TEST_F(managed_document_journal, unicode_document_basename_keeps_its_own_auxiliary_directory) {
  path_ = directory_ / std::filesystem::path{u8"r\u00e9sum\u00e9.json"};
  const auto options = json_options();
  {
    store_type store{ledger{"Saved"}};
    store.create_journal(path_, mode::sidecar, options);
  }
  const auto folder = directory_ / std::filesystem::path{u8".r\u00e9sum\u00e9.json.journal"};
  EXPECT_TRUE(std::filesystem::exists(folder / "manifest"));
  store_type recovered{ledger{}};
  EXPECT_NO_THROW(recovered.recover_journal(path_, options));
  EXPECT_EQ(recovered.read()->name, "Saved");
}

// Provenance survives native operations while source/cache ownership remains an application choice.
TEST_F(managed_document_journal, owned_copy_recovers_without_external_source_or_capture) {
  auto options = json_options();
  options.origin = {'c', 'a', 'p', 't', 'u', 'r', 'e', ':', '1', '7'};
  const auto source = directory_ / "external.json";
  const auto capture = directory_ / "capture.json";
  const auto original = encode_document<codec::json>(ledger{"Source", {{10, {"Entry", 200}}}});
  write_document_bytes(source, original);
  write_document_bytes(capture, original);
  // The host chooses to own a complete main instead of depending on its temporary source.
  write_document_bytes(path_, original);
  bytes expected;
  std::uint32_t burned{};
  {
    store_type store{decode_document<codec::json>(original)};
    store.create_journal(path_, mode::sidecar, options);
    store.execute_transaction([&](auto& tx) { burned = tx.create_id(); tx.revert(); }).throw_if_failed();
    store.execute_transaction([](auto& tx) { tx.root().set_name("Edited"); }).throw_if_failed();
    store.undo();
    store.redo();
    EXPECT_EQ(store.journal_origin(), options.origin);
    expected = store.save();
  }
  ASSERT_TRUE(std::filesystem::remove(source));
  ASSERT_TRUE(std::filesystem::remove(capture));
  {
    store_type recovered{ledger{}};
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), expected);
    EXPECT_EQ(recovered.journal_origin(), options.origin);
    recovered.save_journal();
    recovered.undo();
    EXPECT_EQ(recovered.read()->name, "Source");
    recovered.redo();
    recovered.execute_transaction([&](auto& tx) {
      EXPECT_GT(tx.root().entries().insert(20, {"Next", 300}), burned);
    }).throw_if_failed();
    recovered.save_journal();
  }
  store_type reopened{ledger{}};
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.journal_origin(), options.origin);
  EXPECT_EQ(reopened.read()->entries.size(), 2u);
  const auto output = read_document_bytes(path_);
  EXPECT_EQ(std::search(output.begin(), output.end(), options.origin.begin(), options.origin.end()),
            output.end());
}

// Explicit bounds reject before creating native files or changing the accepted binding.
TEST_F(managed_document_journal, origin_bounds_corruption_and_replacement_are_transactional) {
  auto options = json_options();
  options.max_origin_bytes = 4;
  options.origin = {1, 2, 3, 4, 5};
  store_type store{ledger{"Base"}};
  const auto initial = store.save();
  EXPECT_THROW(store.create_journal(path_, mode::sidecar, options), std::invalid_argument);
  EXPECT_EQ(store.save(), initial);
  EXPECT_TRUE(std::filesystem::is_empty(directory_));
  options.origin = {1, 2, 3};
  store.create_journal(path_, mode::sidecar, options);
  const auto manifest = journal_test_paths::manifest(path_);
  const auto accepted = read_document_bytes(manifest);
  const bytes oversized{1, 2, 3, 4, 5};
  EXPECT_THROW(store.save_journal(oversized), std::length_error);
  EXPECT_EQ(read_document_bytes(manifest), accepted);
  EXPECT_EQ(store.journal_origin(), options.origin);
  const bytes replacement{4, 5};
  store.save_journal(replacement);
  EXPECT_EQ(store.journal_origin(), replacement);
}

// Cold admission rejects damaged and over-budget origin metadata before invoking the payload codec.
TEST_F(managed_document_journal, invalid_origin_recovery_does_not_admit_state) {
  auto options = json_options();
  options.origin = {1, 2, 3, 4};
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, mode::sidecar, options);
  }
  const auto manifest = journal_test_paths::manifest(path_);
  const auto original = read_document_bytes(manifest);
  for (const bool truncate : {false, true}) {
    auto corrupt = original;
    if (truncate) { corrupt.pop_back(); } else { corrupt[corrupt.size() - 9] ^= 1; }
    write_document_bytes(manifest, corrupt);
    store_type destination{ledger{"Untouched"}};
    const auto before = destination.save();
    EXPECT_THROW(destination.recover_journal(path_, options), std::exception);
    EXPECT_EQ(destination.save(), before);
    EXPECT_EQ(read_document_bytes(manifest), corrupt);
  }
  write_document_bytes(manifest, original);
  options.max_origin_bytes = 3;
  store_type destination{ledger{"Untouched"}};
  const auto before = destination.save();
  EXPECT_THROW(destination.recover_journal(path_, options), std::length_error);
  EXPECT_EQ(destination.save(), before);
}

// A lost external acknowledgement leaves a native unresolved intent; confirmation preserves Undo/IDs.
TEST_F(managed_document_journal, prepared_save_survives_restart_and_exact_confirmation_retries) {
  auto options = json_options();
  options.origin = {1};
  const bytes next_origin{2, 3};
  managed::journal_prepared_save prepared;
  bytes expected, original;
  std::uint32_t burned{};
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, mode::sidecar, options);
    original = read_document_bytes(path_);
    store.execute_transaction([&](auto& tx) { burned = tx.create_id(); tx.revert(); }).throw_if_failed();
    store.execute_transaction([](auto& tx) { tx.root().set_name("Prepared"); }).throw_if_failed();
    expected = store.save();
    prepared = store.prepare_journal_save(next_origin);
    EXPECT_EQ(read_document_bytes(path_), original);
    EXPECT_EQ(store.journal_origin(), options.origin);
    EXPECT_EQ(store.save(), expected);
    EXPECT_EQ(store.execute_transaction([](auto& tx) { tx.root().set_name("Too late"); }).status,
              managed::transaction_status::failed);
    EXPECT_THROW(store.undo(), std::logic_error);
    EXPECT_THROW(store.save_journal(), std::logic_error);
    // Simulate host publication, followed by losing its acknowledgement before native confirmation.
    write_document_bytes(directory_ / "external.json", prepared.document);
  }
  const auto evidence = publication_evidence(read_document_bytes(directory_ / "external.json"));
  {
    store_type recovered{ledger{}};
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), expected);
    const auto unresolved = recovered.prepared_journal_save();
    ASSERT_TRUE(unresolved);
    EXPECT_EQ(unresolved->token, prepared.token);
    EXPECT_EQ(unresolved->origin, next_origin);
    auto wrong = evidence;
    ++wrong.document_bytes;
    EXPECT_THROW(recovered.confirm_journal_save(prepared.token, wrong), std::invalid_argument);
    auto tampered = prepared.token;
    tampered[16] ^= 1;
    EXPECT_THROW(recovered.confirm_journal_save(tampered, evidence), std::invalid_argument);
    EXPECT_EQ(read_document_bytes(path_), original);
    EXPECT_TRUE(recovered.prepared_journal_save());
    EXPECT_TRUE(recovered.confirm_journal_save(prepared.token, evidence));
    EXPECT_EQ(recovered.save(), expected);
    EXPECT_EQ(read_document_bytes(path_), prepared.document);
    EXPECT_EQ(recovered.journal_origin(), next_origin);
    EXPECT_FALSE(recovered.prepared_journal_save());
    EXPECT_FALSE(recovered.confirm_journal_save(prepared.token, evidence));
    recovered.undo();
    EXPECT_EQ(recovered.read()->name, "Base");
    EXPECT_TRUE(recovered.journal_dirty());
    EXPECT_FALSE(recovered.confirm_journal_save(prepared.token, evidence));
    EXPECT_TRUE(recovered.journal_dirty());
    recovered.redo();
    recovered.execute_transaction([&](auto& tx) {
      EXPECT_GT(tx.root().entries().insert(20, {"Next", 200}), burned);
    }).throw_if_failed();
  }
  store_type reopened{ledger{}};
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.journal_origin(), next_origin);
  EXPECT_FALSE(reopened.confirm_journal_save(prepared.token, evidence));
  EXPECT_TRUE(reopened.journal_dirty());
  reopened.save_journal();
}

// Every interruption retains an old unresolved or newly selected checkpoint, never a guessed success.
TEST_F(managed_document_journal, prepared_and_confirmed_save_fault_boundaries_recover) {
  const std::vector<event> points{event::before_prepare_publish, event::after_prepare_publish,
      event::before_confirm, event::before_replace, event::after_confirm_decision,
      event::after_replace, event::after_publish, event::before_cleanup};
  for (std::size_t index = 0; index < points.size(); ++index) {
    SCOPED_TRACE(index);
    const auto path = directory_ / ("fault-" + std::to_string(index) + ".json");
    bool armed{};
    auto options = json_options();
    options.origin = {1};
    options.fault_injector = [&](event point) {
      if (armed && point == points[index]) { throw std::runtime_error{"Interrupted publication"}; }
    };
    managed::journal_prepared_save prepared;
    bytes expected;
    const bool preparing = index < 2;
    {
      store_type store{ledger{"Base"}};
      store.create_journal(path, mode::sidecar, options);
      store.execute_transaction([](auto& tx) { tx.root().set_name("Changed"); }).throw_if_failed();
      expected = store.save();
      armed = preparing;
      if (preparing) {
        EXPECT_THROW(store.prepare_journal_save(bytes{2}), std::exception);
      } else {
        prepared = store.prepare_journal_save(bytes{2});
        armed = true;
        if (points[index] == event::before_cleanup) {
          EXPECT_TRUE(store.confirm_journal_save(prepared.token, publication_evidence(prepared.document)));
        } else {
          EXPECT_THROW(store.confirm_journal_save(prepared.token, publication_evidence(prepared.document)),
                       std::exception);
        }
      }
    }
    armed = false;
    store_type recovered{ledger{}};
    recovered.recover_journal(path, options);
    EXPECT_EQ(recovered.save(), expected);
    const bool new_base = index >= 5;
    EXPECT_EQ(recovered.journal_origin(), new_base ? bytes{2} : bytes{1});
    const auto unresolved = recovered.prepared_journal_save();
    EXPECT_EQ(unresolved.has_value(), index >= 1 && !new_base);
    if (unresolved) {
      EXPECT_TRUE(recovered.confirm_journal_save(unresolved->token,
                                                publication_evidence(unresolved->document)));
    }
    recovered.execute_transaction([](auto& tx) { tx.root().set_name("Next"); }).throw_if_failed();
  }
}

// Even valid outer checksums cannot substitute another activation or stale confirmation.
TEST_F(managed_document_journal, prepared_bindings_reject_foreign_stale_and_corrupt_inputs) {
  const auto options = json_options();
  managed::journal_prepared_save first;
  bytes expected;
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, mode::sidecar, options);
    store.execute_transaction([](auto& tx) { tx.root().set_name("Next"); }).throw_if_failed();
    expected = store.save();
    first = store.prepare_journal_save(bytes{1});
    auto forged = first.token;
    forged[16] ^= 1;
    managed::detail::journal_put_word(forged, forged.size() - 8,
        managed::detail::checksum(std::span{forged}.first(forged.size() - 8)));
    EXPECT_THROW(store.confirm_journal_save(forged, publication_evidence(first.document)),
                 std::invalid_argument);
    auto wrong_digest = publication_evidence(first.document);
    wrong_digest.document_digest.front() ^= 1;
    EXPECT_THROW(store.confirm_journal_save(first.token, wrong_digest), std::invalid_argument);
    EXPECT_EQ(store.save(), expected);
  }
  const auto folder = journal_test_paths::directory(path_);
  std::filesystem::path staged;
  for (const auto& entry : std::filesystem::directory_iterator(folder)) {
    if (entry.path().filename().string().starts_with("pending-") && entry.path().extension() != ".manifest") {
      staged = entry.path();
    }
  }
  ASSERT_FALSE(staged.empty());
  const auto original = read_document_bytes(staged);
  auto corrupt = original;
  corrupt.front() ^= 1;
  write_document_bytes(staged, corrupt);
  {
    store_type destination{ledger{"Untouched"}};
    const auto before = destination.save();
    EXPECT_THROW(destination.recover_journal(path_, options), std::invalid_argument);
    EXPECT_EQ(destination.save(), before);
    EXPECT_EQ(read_document_bytes(staged), corrupt);
  }
  write_document_bytes(staged, original);
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_TRUE(recovered.confirm_journal_save(first.token, publication_evidence(first.document)));
  const auto second = recovered.prepare_journal_save();
  EXPECT_THROW(recovered.confirm_journal_save(first.token, publication_evidence(first.document)),
               std::invalid_argument);
  EXPECT_TRUE(recovered.confirm_journal_save(second.token, publication_evidence(second.document)));
  EXPECT_EQ(recovered.journal_origin(), (bytes{1}));
  EXPECT_THROW(recovered.confirm_journal_save(first.token, publication_evidence(first.document)),
               std::invalid_argument);
  EXPECT_EQ(recovered.save(), expected);
}

// Empty provenance retains the default preparation path; native admission also checks receipt integrity.
TEST_F(managed_document_journal, empty_origin_prepare_and_invalid_confirmed_receipt) {
  const auto options = json_options();
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, mode::sidecar, options);
    store.execute_transaction([](auto& tx) { tx.root().set_name("Next"); }).throw_if_failed();
    const auto prepared = store.prepare_journal_save();
    EXPECT_TRUE(prepared.origin.empty());
    EXPECT_TRUE(store.confirm_journal_save(prepared.token, publication_evidence(prepared.document)));
    EXPECT_TRUE(store.journal_origin().empty());
  }
  const auto path = journal_test_paths::manifest(path_);
  auto corrupt = read_document_bytes(path);
  ASSERT_GE(corrupt.size(), 184u + 8u);
  corrupt[corrupt.size() - 184 - 8] ^= 1;
  managed::detail::journal_put_word(corrupt, corrupt.size() - 8,
      managed::detail::checksum(std::span{corrupt}.first(corrupt.size() - 8)));
  write_document_bytes(path, corrupt);
  store_type recovered{ledger{"Untouched"}};
  const auto before = recovered.save();
  EXPECT_THROW(recovered.recover_journal(path_, options), std::invalid_argument);
  EXPECT_EQ(recovered.save(), before);
}

// Identical authored bytes do not authorize provenance from another document namespace/generation.
TEST_F(managed_document_journal, foreign_origin_generation_rejects_before_document_decode) {
  auto options = json_options();
  options.origin = {1};
  const auto foreign = directory_ / "foreign.json";
  {
    store_type first{ledger{"Same"}};
    store_type other{ledger{"Same"}};
    first.create_journal(path_, mode::sidecar, options);
    other.create_journal(foreign, mode::sidecar, options);
  }
  const auto manifest = journal_test_paths::manifest(path_);
  const auto original = read_document_bytes(manifest);
  const auto foreign_manifest = read_document_bytes(journal_test_paths::manifest(foreign));
  write_document_bytes(manifest, foreign_manifest);
  bool decoded{};
  options.document_codec = managed::make_journal_document_codec<ledger>(
      "application/json", encode_document<codec::json>, [&](std::span<const std::uint8_t> input) {
        decoded = true;
        return decode_document<codec::json>(input);
      });
  store_type destination{ledger{"Untouched"}};
  const auto before = destination.save();
  EXPECT_THROW(destination.recover_journal(path_, options), std::exception);
  EXPECT_FALSE(decoded);
  EXPECT_EQ(destination.save(), before);
  EXPECT_EQ(read_document_bytes(manifest), foreign_manifest);
  write_document_bytes(manifest, original);
}

// Large user bytes are owned once; ordinary edits retain physical differential storage.
TEST_F(managed_document_journal, owned_large_main_uses_small_physical_differences) {
  auto options = json_options();
  options.origin = {'o', 'w', 'n', 'e', 'd'};
  constexpr std::size_t payload_bytes = 1024 * 1024;
  codec::decode_limits limits;
  limits.max_collection_elements = 2 * payload_bytes;
  options.document_codec = managed::make_journal_document_codec<ledger>(
      "application/json", encode_document<codec::json>, [limits](std::span<const std::uint8_t> input) {
        const auto stream = rohit::make_constant_full_stream(input.data(), input.size());
        return codec::deserialize_exact<ledger, codec::json>(stream, limits);
      }, limits);
  managed::store_options storage;
  storage.decode = limits;
  store_type store{ledger{std::string(payload_bytes, 'x'), {{10, {"Entry", 200}}}},
                   managed::make_document_id(), storage};
  store.create_journal(path_, mode::sidecar, options);
  const auto original = read_document_bytes(path_);
  const auto main_bytes = std::filesystem::file_size(path_);
  const auto before = std::filesystem::file_size(companion_path());
  for (int index = 0; index < 3; ++index) {
    store.execute_transaction([&](auto& tx) {
      tx.root().entries().edit(2).set_amount_minor_units(300 + index);
    }).throw_if_failed();
  }
  const auto companion_bytes = std::filesystem::file_size(companion_path());
  const auto manifest_bytes = std::filesystem::file_size(journal_test_paths::manifest(path_));
  EXPECT_EQ(read_document_bytes(path_), original);
  EXPECT_LT(companion_bytes, main_bytes / 8);
  EXPECT_LT(companion_bytes - before, main_bytes / 16);
  EXPECT_LT(manifest_bytes, 1024u);
  std::cout << "owned-main=" << main_bytes << " manifest=" << manifest_bytes
            << " companion=" << companion_bytes << " three-edit-growth=" << companion_bytes - before << '\n';
}

// External documents require a separate companion and cannot be mistaken for appended containers.
TEST_F(managed_document_journal, rejects_external_codec_in_appended_mode) {
  store_type store{ledger{"Saved"}};
  EXPECT_THROW(store.create_journal(path_, mode::appended, json_options()), std::invalid_argument);
  EXPECT_FALSE(std::filesystem::exists(path_));
  EXPECT_FALSE(store.journal_needs_recovery());
}
} // namespace

#ifdef _WIN32
// Deep external documents keep native identity/history and original bytes through recovery/Save.
TEST_F(managed_document_journal, extended_length_unicode_companions_preserve_native_state) {
  auto parent = directory_ / L"journal-\u03a9-\u4e2d";
  while (parent.native().size() < 280) { parent /= "level-0123456789"; }
  const rohit::detail::native_file_path native_parent{parent, true};
  ASSERT_TRUE(std::filesystem::create_directories(native_parent.get()));
  path_ = parent / L"ledger-\u03a9.json";
  ASSERT_GT(path_.native().size(), 271u);
  auto options = json_options();
  options.origin = {0, 7, 0, 255};
  bytes original, accepted;
  managed::document_id identity;
  std::uint32_t child_id{};
  std::shared_ptr<const ledger> old_pin;
  {
    store_type store{ledger{"Base", {{10, {"Entry", 200}}}}};
    identity = store.document(); old_pin = store.read();
    child_id = old_pin->entries.at(10).persistent_id;
    store.create_journal(path_, mode::sidecar, options);
    original = read_document_bytes(path_);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Edited"); }).throw_if_failed();
    EXPECT_EQ(read_document_bytes(path_), original);
    EXPECT_EQ(old_pin->name, "Base");
    accepted = store.save();
    const auto folder = journal_test_paths::directory(path_);
    EXPECT_THROW((rohit::file_stream{folder / "writer.lock", rohit::file_open_mode::lock}), std::system_error);
  }
  {
    store_type recovered{ledger{}};
    recovered.recover_journal(path_, options);
    EXPECT_EQ(recovered.save(), accepted);
    EXPECT_EQ(recovered.document().high, identity.high);
    EXPECT_EQ(recovered.document().low, identity.low);
    EXPECT_EQ(recovered.read()->entries.at(10).persistent_id, child_id);
    EXPECT_EQ(recovered.journal_origin(), options.origin);
    recovered.undo(); EXPECT_EQ(recovered.read()->name, "Base");
    recovered.redo(); EXPECT_EQ(recovered.read()->name, "Edited");
    recovered.save_journal();
    EXPECT_FALSE(recovered.journal_dirty());
    EXPECT_EQ(old_pin->name, "Base");
    EXPECT_EQ(decode_document<codec::json>(read_document_bytes(path_)).name, "Edited");
  }
  store_type reopened{ledger{}};
  reopened.recover_journal(path_, options);
  EXPECT_EQ(reopened.save(), accepted);
  EXPECT_FALSE(reopened.journal_dirty());
  EXPECT_EQ(reopened.journal_origin(), options.origin);
  reopened.undo(); EXPECT_EQ(reopened.read()->name, "Base");
  const rohit::detail::native_file_path native_main{path_};
  const auto canonical_root = std::filesystem::weakly_canonical(directory_);
  EXPECT_EQ(rohit::detail::canonical_file_path(path_), canonical_root / path_.lexically_relative(directory_));
  EXPECT_TRUE(native_main.get().native().starts_with(LR"(\\?\)"));
}
#endif
