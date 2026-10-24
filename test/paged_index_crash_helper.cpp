// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/paged_index.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace managed = rohit::managed;
constexpr int interrupted_exit = 73;

// Select a fully specified source identity shared with the independent crash driver.
managed::paged_index_binding source_binding(std::uint64_t revision) {
  return {17, 23, 42, revision,
      rohit::make_digest<rohit::digest_algorithm::sha256>("schema"),
      rohit::make_digest<rohit::digest_algorithm::sha256>("definition:uint64:v1"),
      rohit::make_digest<rohit::digest_algorithm::sha256>(std::to_string(revision))};
}

// Map only the qualification boundaries supported by the immutable replacement protocol.
managed::paged_index_io_event selected_boundary(std::string_view name) {
  if (name == "before_page_write") { return managed::paged_index_io_event::before_page_write; }
  if (name == "after_flush") { return managed::paged_index_io_event::after_flush; }
  if (name == "before_publish") { return managed::paged_index_io_event::before_publish; }
  if (name == "after_publish") { return managed::paged_index_io_event::after_publish; }
  throw std::invalid_argument{"Unknown paged index crash boundary"};
}

// Force external runs and multiple leaf pages with a very small controlled page count.
managed::paged_index_options test_options() {
  managed::paged_index_options options;
  options.page_bytes = 512;
  options.max_cache_pages = 1;
  options.max_cache_bytes = 8192;
  options.max_run_entries = 37;
  options.merge_fan_in = 3;
  return options;
}

// Build one complete derived generation without retaining an owning key table.
void build(const std::filesystem::path& path, std::uint64_t revision,
           const managed::paged_index_options& options) {
  std::uint64_t ordinal{};
  managed::paged_index::build(path, source_binding(revision), [&](auto& entry) {
    if (ordinal == 200) { return false; }
    entry = {revision * 1000 + (ordinal * 79) % 200, ordinal + 1};
    ++ordinal;
    return true;
  }, options);
}
} // namespace

// Terminate without C++ unwinding to verify that only the atomically selected generation opens.
int main(int count, char** arguments) {
  try {
    if (count < 3) { return 2; }
    const std::string mode{arguments[1]};
    const std::filesystem::path path{arguments[2]};
    auto options = test_options();
    if (mode == "create") { build(path, 1, options); return 0; }
    if (mode == "crash" && count == 4) {
      const auto boundary = selected_boundary(arguments[3]);
      options.fault_injector = [boundary](auto event) {
        if (event == boundary) { std::_Exit(interrupted_exit); }
      };
      build(path, 2, options);
      return 3;
    }
    if (mode == "recover" && count == 4) {
      const auto revision = std::stoull(arguments[3]);
      managed::paged_index index{path, source_binding(revision), options};
      auto cursor = index.range(revision * 1000, revision * 1000 + 199);
      for (std::uint64_t ordinal = 0; ordinal < 200; ++ordinal) {
        const auto entry = cursor.next();
        if (!entry || entry->key != revision * 1000 + ordinal) { return 4; }
      }
      if (cursor.next() || index.statistics().peak_cache_bytes > options.max_cache_bytes ||
          index.statistics().peak_resident_pages > options.max_cache_pages) { return 5; }
      std::cout << "paged index crash generation verified\n";
      return 0;
    }
    return 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
