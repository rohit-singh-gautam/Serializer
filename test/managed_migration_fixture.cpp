// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_migration.hpp>
#include <rohit/managed_migration.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
namespace managed = rohit::managed;
namespace migration = managed::migration;
using root = migration_example::document;

// Write only test-owned fresh files through the same strict publication boundary as the actual CLI.
void prepare(const std::filesystem::path& directory) {
  root initial{};
  initial.entries.resize(2U);
  initial.entries[0].amount = 128U;
  initial.entries[1].amount = 16384U;
  initial.ordinary = 0x0102030405060708ULL;
  managed::model_store<root, managed::history_mode::linear, managed::history_labels::enabled> store{initial, {41U, 42U}};
  store.execute_transaction("Retained redo", [](auto& edit) {
    edit.update([](auto& value) { managed::model_traits<root>::payload(value).ordinary = 900U; });
  }).throw_if_failed();
  store.undo();
  const auto expected = store.save();
  migration::options reverse;
  reverse.source_encoding = managed::identity_encoding::compact_varint;
  reverse.target_encoding = managed::identity_encoding::fixed;
  migration::detail::publish(directory / "managed.fixed", migration::migrate_save<root>(expected, reverse));
  migration::detail::publish(directory / "managed.expected", expected);
  migration::detail::publish(directory / "raw.fixed", migration::detail::encode_value(*store.read(),
      managed::identity_encoding::fixed, migration::payload_codec::binary_none, 65536U));
  migration::detail::publish(directory / "raw.expected", migration::detail::encode_value(*store.read(),
      managed::identity_encoding::compact_varint, migration::payload_codec::binary_string, 65536U));
}

// Compare full converted files, including managed labels/history/cursors and independently selected raw keys.
void verify(const std::filesystem::path& directory) {
  for (const auto* stem : {"managed", "raw"}) {
    const auto expected = migration::detail::read_file(directory / (std::string{stem} + ".expected"), 65536U);
    const auto actual = migration::detail::read_file(directory / (std::string{stem} + ".compact"), 65536U);
    if (expected != actual) { throw std::runtime_error{"Migrated CLI file differs from the complete expected value"}; }
  }
}
} // namespace

// Supply and validate actual files for the separately built, schema-linked command-line tool.
int main(int argc, char** argv) {
  try {
    if (argc != 3) { throw std::invalid_argument{"Expected create|verify and a test directory"}; }
    if (std::string_view{argv[1]} == "create") { prepare(argv[2]); }
    else if (std::string_view{argv[1]} == "verify") { verify(argv[2]); }
    else { throw std::invalid_argument{"Unknown migration fixture action"}; }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
