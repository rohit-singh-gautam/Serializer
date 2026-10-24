// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <ledger.hpp>
#include <rohit/native_database.hpp>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace managed = rohit::managed;
using database = managed::native_database<ledger_example::ledger>;
constexpr managed::document_id document_namespace{17, 23};
constexpr std::uint64_t epoch = 42;
constexpr std::array<std::uint8_t, 3> request{1, 2, 3};
constexpr int interrupted_exit = 73;

// Resolve a crash boundary selected by the independent subprocess test.
managed::database_io_event boundary(std::string_view name) {
  if (name == "before_write") { return managed::database_io_event::before_write; }
  if (name == "after_write") { return managed::database_io_event::after_write; }
  if (name == "after_flush") { return managed::database_io_event::after_flush; }
  if (name == "before_publish") { return managed::database_io_event::before_publish; }
  if (name == "after_rebaseline_archive") { return managed::database_io_event::after_rebaseline_archive; }
  if (name == "before_rebaseline_replace") { return managed::database_io_event::before_rebaseline_replace; }
  if (name == "after_rebaseline_replace") { return managed::database_io_event::after_rebaseline_replace; }
  throw std::invalid_argument{"Unknown database crash boundary"};
}

// Mutate business state and an application-owned outbox entry in one native managed candidate.
void edit(auto& transaction) {
  transaction.root().entries().edit(2).set_amount_minor_units(200);
  transaction.root().entries().insert(20, {"Outbox", 5});
}
} // namespace

// Execute in separate processes so abrupt termination bypasses destructors and error recovery.
int main(int count, char** arguments) {
  try {
    if (count < 3) { return 2; }
    const std::string mode{arguments[1]};
    const std::filesystem::path path{arguments[2]};
    if (mode == "create") {
      database store{path, ledger_example::ledger{"Ledger", {{10, {"One", 100}}}},
                     document_namespace, epoch};
      return 0;
    }
    if (mode == "crash" && count == 4) {
      const auto event = boundary(arguments[3]);
      managed::database_options options;
      options.fault_injector = [event](auto current) {
        if (current == event) { std::_Exit(interrupted_exit); }
      };
      database store{path, options};
      auto prepared = store.prepare({7, 9}, epoch, 0, request,
                                    [](auto& transaction) { edit(transaction); });
      store.accept(std::move(prepared));
      return 3;
    }
    if (mode == "crash_migrate" && count == 4) {
      const auto event = boundary(arguments[3]);
      managed::database_options options;
      options.fault_injector = [event](auto current) {
        if (current == event) { std::_Exit(interrupted_exit); }
      };
      database store{path, options};
      auto archive = path;
      archive += ".old";
      const auto migrated = store.migrate(epoch + 1, archive, [](const auto& value) {
        auto converted = value;
        converted.name = "Migrated";
        return converted;
      });
      return 3;
    }
    if (mode == "recover_migration" && count == 4) {
      const auto expected_epoch = std::stoull(arguments[3]);
      database store{path};
      if (store.epoch() != expected_epoch || store.revision() != 0 ||
          store.document() != document_namespace || store.read()->entries.at(10).persistent_id != 2 ||
          store.read()->name != (expected_epoch == epoch ? "Ledger" : "Migrated")) {
        return 7;
      }
      auto archive = path;
      archive += ".old";
      database previous{archive};
      if (previous.epoch() != epoch || previous.read()->name != "Ledger") { return 8; }
      return 0;
    }
    if (mode == "recover" && count == 4) {
      const auto expected = std::stoull(arguments[3]);
      database store{path};
      if (store.revision() != expected || store.document() != document_namespace || store.epoch() != epoch) {
        return 4;
      }
      bool called{};
      auto prepared = store.prepare({7, 9}, epoch, 0, request, [&](auto& transaction) {
        called = true;
        edit(transaction);
      });
      if (called != (expected == 0) || store.accept(std::move(prepared)).revision != 1 ||
          store.read()->entries.size() != 2 ||
          store.read()->entries.at(10).amount_minor_units != 200 ||
          store.read()->entries.at(20).persistent_id != 3) {
        return 5;
      }
      auto retry = store.prepare({7, 9}, epoch, 0, request, [](auto&) {
        throw std::runtime_error{"Exact retry executed the business callback"};
      });
      if (store.accept(std::move(retry)).revision != 1 || store.revision() != 1) { return 6; }
      std::cout << "native database crash recovery verified\n";
      return 0;
    }
    return 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
