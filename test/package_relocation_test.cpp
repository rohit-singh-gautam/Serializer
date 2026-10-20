// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "package_relocation.hpp"

#include <rohit/package.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
using archive = relocation::archive;

constexpr std::string_view previous_destination = "previous destination";

// Preserve a caller-owned failure token independently of exception copy construction.
struct caller_failure {
  const int* identity;
};

// Keep assertions enabled in optimized standalone SDK consumers.
void check(bool condition, std::string_view message, int& checks) {
  if (!condition) {
    throw std::runtime_error{std::string{message}};
  }
  ++checks;
}

// Read complete test-owned bytes without interpreting package metadata.
std::string read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream) {
    throw std::runtime_error{"Could not read consumer fixture"};
  }
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

// Publish only the consumer's own completed output or previous-destination sentinel.
void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream stream{path, std::ios::binary};
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!stream) {
    throw std::runtime_error{"Could not write consumer output"};
  }
}

// Snapshot every public preserved byte, authored link and relocation before any canonical write.
std::string snapshot(const package::preserved_extensions& extensions) {
  std::ostringstream output;
  for (const auto& part : extensions.parts) {
    output << part.metadata.name << '\0' << part.metadata.content_type << '\0';
    output.write(reinterpret_cast<const char*>(part.bytes.data()), static_cast<std::streamsize>(part.bytes.size()));
    output << '\0';
  }
  for (const auto& link : extensions.relationships) {
    output << link.source << '\0' << link.id << '\0' << link.type << '\0' << link.target << '\0'
           << static_cast<int>(link.mode) << '\0';
  }
  for (const auto& move : extensions.relocations) {
    output << move.original << '\0' << move.canonical << '\0';
  }
  return output.str();
}

// Encode through the existing public borrowed-stream facade and one cumulative caller budget.
std::string encode(const archive::input& input, std::shared_ptr<package::budget> budget = {}) {
  std::ostringstream output;
  archive::write(output, input, std::move(budget));
  return output.str();
}

// Prove exact shared limits and original cancellation failures before file publication.
void resource_controls(const std::filesystem::path& directory, const archive::input& input,
    const package::preserved_extensions& extensions, int& checks) {
  auto measured = std::make_shared<package::budget>();
  static_cast<void>(encode(input, measured));
  for (const auto selected : {package::resource::work_units, package::resource::decoder_allocation_bytes}) {
    const auto maximum = selected == package::resource::work_units ?
        measured->configured().max_work_units : measured->configured().max_decoder_allocation_bytes;
    const auto used = maximum - measured->remaining(selected);
    check(used > 0, "Shared canonical resource was not admitted", checks);
    for (const bool tight : {false, true}) {
      package::limits limits;
      const auto bound = used - static_cast<std::uint64_t>(tight);
      if (selected == package::resource::work_units) {
        limits.max_work_units = bound;
      } else {
        limits.max_decoder_allocation_bytes = bound;
      }
      try {
        static_cast<void>(encode(input, std::make_shared<package::budget>(limits)));
        check(!tight, "One-tight canonical bound accepted", checks);
      } catch (const package::error& failure) {
        check(tight && failure.code == package::error_code::resource_limit,
            "Exact canonical bound failed or wrong resource category", checks);
      }
    }
  }
  const int identity{};
  int polls{};
  package::limits limits;
  limits.is_cancelled = [&] {
    ++polls;
    if (polls >= 7) {
      throw caller_failure{&identity};
    }
    return false;
  };
  const auto destination = directory / "cancelled.opc";
  write_bytes(destination, previous_destination);
  try {
    archive::save(destination, input, limits);
    throw std::runtime_error{"Cancelled canonical Save published"};
  } catch (const caller_failure& failure) {
    check(failure.identity == &identity && read_bytes(destination) == previous_destination,
        "Caller cancellation identity or Save rollback lost", checks);
  }
  auto conflicting = extensions;
  conflicting.relocations = {{"/same.xml", "/left/A.xml"}, {"/SAME.xml", "/right/B.xml"}};
  auto invalid = input;
  invalid.extensions = &conflicting;
  try {
    static_cast<void>(encode(invalid));
    throw std::runtime_error{"Conflicting authored origin accepted"};
  } catch (const package::error& failure) {
    check(failure.code == package::error_code::invalid_relationship,
        "Authored origin conflict classification changed", checks);
  }
}

// Execute one independently authored corpus case using generated public typed package APIs only.
int run(int argc, char** argv) {
  if (argc != 5) {
    throw std::runtime_error{"Expected fixture directory, case, action and decision"};
  }
  const std::filesystem::path directory{argv[1]};
  const std::string name{argv[2]}, action{argv[3]}, expected{argv[4]};
  int checks{};
  std::ifstream source{directory / (name + ".opc"), std::ios::binary};
  package::extension_policy policy;
  policy.parts = package::unknown_policy::preserve;
  policy.relationships = package::unknown_policy::preserve;
  auto loaded = [&] {
    try {
      return archive::load(source, {}, policy);
    } catch (const package::error&) {
      if (expected != "LOAD_ERROR") {
        throw;
      }
      ++checks;
      return package::aggregate_result<archive::value>{};
    }
  }();
  if (expected == "LOAD_ERROR") {
    check(checks == 1, "Invalid duplicate origin published", checks);
    std::cout << "{\"case\":\"" << name << "\",\"decision\":\"" << expected
              << "\",\"checks\":" << checks << ",\"failure_stage\":\"load\"}\n";
    return 0;
  }
  std::vector<package::model_part_view<relocation::payload>> children;
  for (const auto& item : loaded.value.children) {
    // The declared parent is a fixed singleton, so its only occurrence is zero.
    children.push_back({&item.value, item.name, item.content_type, 0});
  }
  archive::input input;
  input.first = &loaded.value.first;
  input.second = action == "remove_second" || !loaded.value.second ? nullptr : &*loaded.value.second;
  input.children = children;
  input.extensions = &loaded.extensions;
  check(loaded.value.first.id == 11 && children.size() == 1 && children.front().source_index == 0,
      "Typed identities or dynamic child source occurrence changed", checks);
  const auto original = snapshot(loaded.extensions);
  const auto destination = directory / (name + "-saved.opc");
  write_bytes(destination, previous_destination);
  if (expected == "REJECT") {
    try {
      static_cast<void>(encode(input));
      throw std::runtime_error{"Conflicting canonical graph encoded"};
    } catch (const package::error&) {
      ++checks;
    }
    try {
      archive::save(destination, input);
      throw std::runtime_error{"Conflicting canonical graph saved"};
    } catch (const package::error&) {
      check(read_bytes(destination) == previous_destination, "Rejected Save changed previous bytes", checks);
    }
  } else {
    write_bytes(directory / (name + "-encoded.opc"), encode(input));
    write_bytes(directory / (name + "-repeat.opc"), encode(input));
    archive::save(destination, input);
    if (name == "alternate") {
      resource_controls(directory, input, loaded.extensions, checks);
    }
  }
  check(snapshot(loaded.extensions) == original, "Canonicalization mutated caller preservation records", checks);
  std::cout << "{\"case\":\"" << name << "\",\"decision\":\"" << expected
            << "\",\"checks\":" << checks << "}\n";
  return 0;
}
} // namespace

// Keep failures visible as ordinary diagnostics without invoking a native crash-reporting dialog.
int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
  } catch (...) {
    std::cerr << "Unexpected caller failure escaped consumer controls\n";
  }
  return 1;
}
