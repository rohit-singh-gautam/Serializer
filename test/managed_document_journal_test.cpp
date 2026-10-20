// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/managed_journal_document.hpp>

#include <ledger.hpp>
#include <rohit/managed.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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
using bytes = std::vector<std::uint8_t>;
using mode = managed::journal_storage_mode;
using event = managed::journal_io_event;

// Read exact document or companion bytes while retaining any insignificant external whitespace.
bytes read_document_bytes(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot read external journal fixture"};
  }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Replace only a closed fixture to prepare an external document or simulate interrupted publication.
void write_document_bytes(const std::filesystem::path& path, const bytes& value) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
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
    std::filesystem::remove_all(directory_, error);
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
    const auto prefix = path_.filename().string() + ".journal-";
    for (const auto& item : std::filesystem::directory_iterator{directory_}) {
      if (item.path().filename().string().starts_with(prefix)) {
        return item.path();
      }
    }
    throw std::runtime_error{"Missing external document journal companion"};
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
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".journal.pending"));
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
  ASSERT_TRUE(std::filesystem::exists(path_.string() + ".journal.pending"));
  // Reproduce the other allowed crash ordering using the still-retained old data generation.
  write_document_bytes(path_, original);
  armed = false;
  store_type recovered{ledger{}};
  recovered.recover_journal(path_, options);
  EXPECT_EQ(recovered.read()->name, "Current");
  EXPECT_TRUE(recovered.journal_dirty());
  EXPECT_EQ(read_document_bytes(path_), original);
  EXPECT_FALSE(std::filesystem::exists(path_.string() + ".journal.pending"));
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

// External documents require a separate companion and cannot be mistaken for appended containers.
TEST_F(managed_document_journal, rejects_external_codec_in_appended_mode) {
  store_type store{ledger{"Saved"}};
  EXPECT_THROW(store.create_journal(path_, mode::appended, json_options()), std::invalid_argument);
  EXPECT_FALSE(std::filesystem::exists(path_));
  EXPECT_FALSE(store.journal_needs_recovery());
}
} // namespace
