#include <dimensions_managed.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_file_journal.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace managed = rohit::managed;
using root = dimension_history::sample;
using traits = managed::model_traits<root>;
using store_type = managed::model_store<root, managed::history_mode::tree, managed::history_labels::enabled>;

// Fail a subprocess immediately on an observable contract mismatch.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Fill every nested array with distinct nonzero data, including ordinary generic containment.
root sample(double seed) {
  root value{};
  for (std::size_t index = 0; index < value.position.coordinates.size(); ++index) {
    value.position.coordinates[index] = seed + static_cast<double>(index);
    value.coordinate_frame.origin.coordinates[index] = seed + 10 + static_cast<double>(index);
  }
  for (std::size_t index = 0; index < value.implicit_square.elements.size(); ++index) {
    value.implicit_square.elements[index] = seed + 20 + static_cast<double>(index);
    value.explicit_square.elements[index] = seed + 40 + static_cast<double>(index);
    value.coordinate_frame.basis.elements[index] = seed + 60 + static_cast<double>(index);
  }
  return value;
}

// Replace ordinary values exclusively through generated setters in a live transaction.
auto replace(store_type& store, double seed, bool revert = false) {
  const auto value = sample(seed);
  return store.execute_transaction("dimension " + std::to_string(seed), [&](auto& transaction) {
    auto editor = transaction.root();
    editor.set_position(value.position);
    editor.set_coordinate_frame(value.coordinate_frame);
    editor.set_implicit_square(value.implicit_square);
    editor.set_explicit_square(value.explicit_square);
    if (revert) { transaction.revert(); }
  });
}

// Compare all fields after crossing process, history, and storage boundaries.
void verify(const store_type& store, double seed) {
  const auto expected = sample(seed);
  const auto state = store.read();
  const auto& value = traits::payload(*state);
  require(value.position.coordinates == expected.position.coordinates, "position");
  require(value.coordinate_frame.origin.coordinates == expected.coordinate_frame.origin.coordinates, "origin");
  require(value.coordinate_frame.basis.elements == expected.coordinate_frame.basis.elements, "basis");
  require(value.implicit_square.elements == expected.implicit_square.elements, "implicit square");
  require(value.explicit_square.elements == expected.explicit_square.elements, "explicit square");
  require(state->persistent_id == 1, "root identity");
}
} // namespace

// Run one independent native journal process; the Python driver controls interruption/corruption.
int main(int argc, char** argv) {
  try {
    if (argc != 3) { throw std::invalid_argument{"expected mode and journal path"}; }
    const std::string mode = argv[1];
    const std::filesystem::path path = argv[2];
    store_type store{sample(99)};
    if (mode == "create") {
      store.create_journal(path, managed::journal_storage_mode::sidecar);
      const auto first = replace(store, 1); first.throw_if_failed();
      const auto second = replace(store, 2); second.throw_if_failed();
      store.undo();
      const auto alternate = replace(store, 3); alternate.throw_if_failed();
      store.checkout(second.revision);
      const auto saved = store.save();
      require(replace(store, 2).status == managed::transaction_status::no_change, "no change");
      require(replace(store, 4, true).status == managed::transaction_status::reverted, "revert");
      require(store.save() == saved, "failed transaction changed root or cursor");
      store_type memory{sample(99)};
      memory.load(saved);
      require(memory.save() == saved, "memory snapshot identity/history");
      verify(memory, 2);
      store.undo();
      require(store.redo_children() == std::vector<std::uint64_t>{second.revision, alternate.revision}, "retained branches");
      store.redo(alternate.revision); verify(store, 3);
      store.checkout(second.revision);
      store.save_journal();
      std::ofstream expected{path.string() + ".expected", std::ios::binary};
      expected.write(reinterpret_cast<const char*>(saved.data()), static_cast<std::streamsize>(saved.size()));
      require(static_cast<bool>(expected), "expected snapshot output");
    } else if (mode == "recover" || mode == "crash") {
      managed::journal_options options{};
      if (mode == "crash") {
        options.fault_injector = [](managed::journal_io_event event) {
          if (event == managed::journal_io_event::after_payload) { std::_Exit(86); }
        };
      }
      store.recover_journal(path, options);
      std::ifstream expected{path.string() + ".expected", std::ios::binary};
      const std::vector<std::uint8_t> saved{std::istreambuf_iterator<char>{expected}, {}};
      require(store.save() == saved, "fresh-process root/document/revisions");
      verify(store, 2);
      if (mode == "crash") { replace(store, 5).throw_if_failed(); throw std::runtime_error{"missing interruption"}; }
      store.undo();
      const auto children = store.redo_children();
      require(children.size() == 2, "recovered branches");
      store.redo(children.back()); verify(store, 3);
    } else if (mode == "malformed") {
      const auto original = store.save();
      auto envelope = managed::detail::decode<managed::records::envelope>(original, {});
      const auto point = managed::detail::encode(sample(99).position);
      // Corrupt only a generated point's sequence count inside otherwise valid generated records.
      const auto corrupt = [&](auto& snapshot) {
        const auto found = std::search(snapshot.begin(), snapshot.end(), point.begin(), point.end());
        require(found != snapshot.end() && point.size() > 1 && point[0] == 1 && point[1] == 3,
                "point count fixture");
        *(found + 1) = 2;
      };
      corrupt(envelope.current_snapshot);
      for (auto& revision : envelope.revisions) { corrupt(revision.snapshot); }
      const auto malformed = managed::detail::encode(envelope);
      bool rejected = false;
      try { store.load(malformed); } catch (const std::exception&) { rejected = true; }
      require(rejected && store.save() == original, "malformed memory snapshot published");
      bool indeterminate = false;
      const auto sink = managed::detail::create_file_journal(path, managed::journal_storage_mode::sidecar,
          malformed, {}, indeterminate);
      require(!indeterminate, "malformed fixture journal publication");
    } else if (mode == "budget" || mode == "reject") {
      const auto original = store.save();
      managed::journal_options options{};
      if (mode == "budget") { options.max_record_bytes = 1; }
      bool rejected = false;
      try { store.recover_journal(path, options); } catch (const std::exception&) { rejected = true; }
      require(rejected, "invalid journal accepted");
      require(store.save() == original, "partial recovered state published");
      verify(store, 99);
    } else { throw std::invalid_argument{"unknown mode"}; }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
