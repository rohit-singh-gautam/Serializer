// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <ledger.hpp>
#include <rohit/native_database.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
bool fail_allocations{};
bool fail_remaining_allocations{};
std::size_t allocations_before_failure{};

// Intercept ordinary C++ allocation in this isolated executable, outside the test framework.
void* allocate(std::size_t size) {
  if (fail_allocations) {
    if (fail_remaining_allocations || allocations_before_failure == 0) {
      throw std::bad_alloc{};
    }
    --allocations_before_failure;
  }
  if (auto* pointer = std::malloc(size == 0 ? 1 : size)) { return pointer; }
  throw std::bad_alloc{};
}

// Release the probe before any diagnostics or exception construction can allocate.
struct allocation_scope {
  // Start a precise failure point or deny every further ordinary new allocation.
  explicit allocation_scope(std::size_t before, bool remaining = false) {
    allocations_before_failure = before;
    fail_remaining_allocations = remaining;
    fail_allocations = true;
  }
  // Return runtime allocation to its normal behavior on all exception paths.
  ~allocation_scope() { fail_allocations = false; }
};

// Own only one newly created temporary directory; remove named artifacts without recursive deletion.
struct database_directory {
  std::filesystem::path directory;
  // Refuse an existing temporary child rather than assuming ownership of its contents.
  database_directory() {
    const auto identity = rohit::managed::make_document_id();
    directory = std::filesystem::temp_directory_path() /
        ("serializer-database-allocation-" + std::to_string(identity.high) + "-" +
         std::to_string(identity.low));
    if (!std::filesystem::create_directory(directory)) {
      throw std::runtime_error{"Cannot create isolated allocation fixture"};
    }
  }
  // Preserve unrelated files if a failed test left unexpected contents in the directory.
  ~database_directory() {
    std::error_code ignored;
    std::filesystem::remove(directory / "state.sdb", ignored);
    std::filesystem::remove(directory / "state.sdb.writer.lock", ignored);
    std::filesystem::remove(directory / "state.sdb.old", ignored);
    // Allocation-denied post-publication cleanup can leave an empty task-owned staging directory.
    for (const auto& child : std::filesystem::directory_iterator{directory, ignored}) {
      if (child.path().filename().string().starts_with("state.sdb.rebaseline-")) {
        std::filesystem::remove(child.path(), ignored);
      }
    }
    std::filesystem::remove(directory, ignored);
  }
  // Select this fixture's single authoritative database file.
  auto path() const { return directory / "state.sdb"; }
};

// Exhaust preparation allocation sites, then forbid ordinary allocations throughout publication.
void verify_allocation_failures() {
  namespace managed = rohit::managed;
  using database = managed::native_database<ledger_example::ledger>;
  constexpr std::array<std::uint8_t, 1> request{1};
  constexpr std::uint64_t epoch = 11;
  database_directory directory;
  bool guard_publication{};
  managed::database_options options;
  options.fault_injector = [&](auto event) {
    if (guard_publication && event == managed::database_io_event::before_publish) {
      fail_remaining_allocations = true;
      fail_allocations = true;
    }
  };
  database state{directory.path(), ledger_example::ledger{"Ledger", {{10, {"One", 100}}}},
                 managed::document_id{13, 17}, epoch, options};
  const auto original = state.read();
  const auto baseline_bytes = std::filesystem::file_size(directory.path());
  constexpr std::size_t maximum_probe_allocations = 4096;
  std::size_t failed_preparations{};
  bool complete{};
  for (std::size_t count = 0; count < maximum_probe_allocations; ++count) {
    std::optional<database::prepared_transaction> prepared;
    bool failed{};
    try {
      const allocation_scope probe{count};
      prepared.emplace(state.prepare({1, 1}, epoch, 0, request, [](auto& edit) {
        edit.root().entries().edit(2).set_amount_minor_units(200);
      }));
    } catch (const std::bad_alloc&) {
      failed = true;
    }
    if (state.revision() != 0 || state.read() != original || state.needs_recovery() ||
        std::filesystem::file_size(directory.path()) != baseline_bytes) {
      throw std::runtime_error{"Preparation allocation failure changed accepted state"};
    }
    if (failed) { ++failed_preparations; continue; }
    if (!prepared) { throw std::runtime_error{"Probe lost its successful preparation"}; }
    // Enable the global allocation guard only after durable acceptance. The returned outcome
    // and accepted immutable state must require no allocation after this boundary.
    guard_publication = true;
    managed::database_outcome accepted;
    try {
      accepted = state.accept(std::move(*prepared));
    } catch (...) {
      fail_allocations = false;
      throw;
    }
    fail_allocations = false;
    if (accepted.revision != 1 || state.revision() != 1 ||
        state.read()->entries.at(10).amount_minor_units != 200 ||
        original->entries.at(10).amount_minor_units != 100) {
      throw std::runtime_error{"Guarded publication did not preserve accepted and historical state"};
    }
    complete = true;
    break;
  }
  if (!complete || failed_preparations == 0) {
    throw std::runtime_error{"Allocation campaign did not reach all preparation/publication gates"};
  }
  std::cout << failed_preparations << " rejected preparation allocations; allocation-free publication passed\n";
}

// Deny every ordinary allocation after atomic migration replacement, before writer transfer.
void verify_migration_publication() {
  namespace managed = rohit::managed;
  using database = managed::native_database<ledger_example::ledger>;
  database_directory directory;
  auto archive = directory.path();
  archive += ".old";
  managed::database_options options;
  options.fault_injector = [](auto event) {
    if (event == managed::database_io_event::after_rebaseline_replace) {
      fail_remaining_allocations = true;
      fail_allocations = true;
    }
  };
  database source{directory.path(), ledger_example::ledger{"Ledger", {{10, {"One", 100}}}},
                  managed::document_id{13, 17}, 11, options};
  std::unique_ptr<database> migrated;
  try {
    migrated = source.migrate(12, archive, [](const auto& value) { return value; });
  } catch (...) {
    fail_allocations = false;
    throw;
  }
  fail_allocations = false;
  if (!source.retired() || migrated->epoch() != 12 || migrated->revision() != 0 ||
      migrated->read()->entries.at(10).amount_minor_units != 100) {
    throw std::runtime_error{"Guarded migration writer transfer lost accepted state"};
  }
  std::cout << "Allocation-denied native migration publication passed\n";
}
} // namespace

// Route scalar allocation through the executable-local failure probe.
void* operator new(std::size_t size) { return allocate(size); }
// Route array allocation through the same failure probe.
void* operator new[](std::size_t size) { return allocate(size); }
// Release scalar storage acquired from malloc.
void operator delete(void* pointer) noexcept { std::free(pointer); }
// Release array storage acquired from malloc.
void operator delete[](void* pointer) noexcept { std::free(pointer); }
// Support sized scalar deallocation without allocation.
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
// Support sized array deallocation without allocation.
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

// Keep global allocation interception isolated from all framework and concurrent runtime tests.
int main() {
  try {
    verify_allocation_failures();
    verify_migration_publication();
    return 0;
  } catch (const std::exception& error) {
    fail_allocations = false;
    std::cerr << error.what() << '\n';
    return 1;
  }
}
