// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "fixture.hpp"

#include <array>
#include <iostream>
#include <type_traits>

namespace {
using namespace legacy_fixture;

// Inspect every retained snapshot's IDs, labels and tree ancestry using the current ordinary codec.
template <managed::history_mode Mode, managed::history_labels Labels>
void check_metadata(std::span<const std::uint8_t> bytes) {
  if constexpr (Mode == managed::history_mode::tree) {
    using envelope = std::conditional_t<Labels == managed::history_labels::enabled,
        managed::records::envelope, managed::records::unlabeled_envelope>;
    const auto value = decode<envelope, rohit::serializer::binary_integer>(bytes);
    require(value.current_revision == 2U && value.revision_high_water == 4U, "Tree cursors changed");
    require(value.revisions.size() == 4, "Retained tree branch count changed");
    const std::array<std::uint64_t, 4> parents{0U, 1U, 2U, 2U};
    for (std::size_t index = 0; index < value.revisions.size(); ++index) {
      const auto& revision = value.revisions[index];
      require(revision.number == index + 1 && revision.parent == parents[index], "Tree ancestry changed");
      const auto snapshot = decode<storage, rohit::serializer::binary_integer>(revision.snapshot);
      require(snapshot.persistent_id == 1U, "Retained root identity changed");
      if (index >= 2) {
        require(traits::payload(snapshot).entries.back().persistent_id == index + 2, "Retained branch identity changed");
      }
    }
    if constexpr (Labels == managed::history_labels::enabled) {
      require(value.revisions[1].label == "Retained update" && value.revisions[2].label == "Retained append" &&
          value.revisions[3].label == "Retained branch", "Tree labels changed");
    }
  } else {
    using envelope = std::conditional_t<Labels == managed::history_labels::enabled,
        managed::records::state_envelope, managed::records::unlabeled_state_envelope>;
    const auto value = decode<envelope, rohit::serializer::binary_integer>(bytes);
    if constexpr (Mode == managed::history_mode::linear) {
      require(value.cursor == 1U && value.entries.size() == 3, "Linear retained cursor/history changed");
      for (const auto& state : value.entries) {
        const auto snapshot = decode<storage, rohit::serializer::binary_integer>(state.snapshot);
        require(snapshot.persistent_id == 1U, "Retained linear root identity changed");
      }
      if constexpr (Labels == managed::history_labels::enabled) {
        require(value.entries[1].label == "Retained update" && value.entries[2].label == "Retained append", "Linear labels changed");
      }
    } else {
      require(value.entries.empty() && value.cursor == 0U, "Disabled history acquired side metadata");
    }
  }
}

// Load migrated saves without changing IDs; exercise redo branches and burned-ID nonreuse afterward.
template <managed::history_mode Mode, managed::history_labels Labels>
void verify_history(const std::filesystem::path& directory, std::string_view name) {
  const auto bytes = read(directory / (std::string{name} + ".compact"));
  store<Mode, Labels> value{initial(), {43U, 44U}};
  value.load(bytes);
  check_store<Mode, Labels>(value);
  check_metadata<Mode, Labels>(bytes);
  if constexpr (Mode == managed::history_mode::linear) {
    value.redo();
    require(traits::payload(*value.read()).entries.back().persistent_id == 4U, "Redo reassigned a persistent ID");
    value.undo();
  } else if constexpr (Mode == managed::history_mode::tree) {
    value.redo(3U);
    require(traits::payload(*value.read()).entries.back().persistent_id == 4U, "First tree branch target changed");
    value.undo();
    value.redo(4U);
    require(traits::payload(*value.read()).entries.back().persistent_id == 5U, "Second tree branch target changed");
    value.undo();
  }
  const auto previous = value.allocated_id();
  transact<Labels>(value, "Post-migration append", [](auto& edit) {
    entry added{};
    added.amount = 1000U;
    edit.root().entries().append(std::move(added));
  });
  require(traits::payload(*value.read()).entries.back().persistent_id == previous + 1, "Migration reused a burned identity");
}

// Every independently old-produced codec must decode correctly after each selected output codec.
template <template <rohit::serializer::serialize_type> typename Protocol>
void verify_raw(const std::filesystem::path& directory, std::string_view target) {
  for (const auto* variant : {"normal", "boundary"}) {
    for (const auto* source : {"binary_none", "binary_integer", "binary_string"}) {
      const auto name = std::string{"raw-"} + variant + '-' + source + '-' + std::string{target} + ".compact";
      check_raw(decode<storage, Protocol>(read(directory / name)), std::string_view{variant} == "boundary");
    }
  }
}
} // namespace

// Verify only current compiler/current runtime consumers of separately old-produced artifacts.
int main(int argc, char** argv) {
  try {
    if (argc != 2) { throw std::invalid_argument{"Expected an artifact directory"}; }
    const std::filesystem::path directory{argv[1]};
    verify_raw<rohit::serializer::binary_none>(directory, "binary_none");
    verify_raw<rohit::serializer::binary_integer>(directory, "binary_integer");
    verify_raw<rohit::serializer::binary_string>(directory, "binary_string");
    verify_history<managed::history_mode::disabled, managed::history_labels::disabled>(directory, "managed-disabled");
    verify_history<managed::history_mode::linear, managed::history_labels::disabled>(directory, "managed-linear");
    verify_history<managed::history_mode::linear, managed::history_labels::enabled>(directory, "managed-linear-labels");
    verify_history<managed::history_mode::tree, managed::history_labels::disabled>(directory, "managed-tree");
    verify_history<managed::history_mode::tree, managed::history_labels::enabled>(directory, "managed-tree-labels");
    std::cout << "Current verifier passed: " << sizeof(id_type) * 8 << " ID bits, 18 raw codec pairs and five native histories\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
