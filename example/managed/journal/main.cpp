#include <ledger.hpp>
#include <rohit/managed.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace managed = rohit::managed;
using ledger_store = managed::model_store<ledger_example::ledger>;

// Fail the example visibly if a persistence, cursor, or dirty-state expectation is not met.
void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

// Close without full Save, recover journaled edits, navigate, checkpoint, then reopen retained history.
void demonstrate(const std::filesystem::path& path, managed::journal_storage_mode mode) {
  {
    ledger_store store{ledger_example::ledger{"Saved ledger"}};
    store.create_journal(path, mode);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Journaled ledger"); })
        .throw_if_failed();
    require(store.journal_dirty(), "Journaled edits should still be unsaved");
    const auto sequence = store.journal_sequence();
    const auto unchanged = store.execute_transaction([](auto&) {});
    unchanged.throw_if_failed();
    require(unchanged.status == managed::transaction_status::no_change &&
                store.journal_sequence() == sequence,
            "An unchanged transaction must not append a snapshot");
    // Destruction releases the writer lock. There is deliberately no full Save here.
  }
  {
    ledger_store reopened{ledger_example::ledger{}};
    reopened.recover_journal(path);
    require(reopened.read()->name == "Journaled ledger" && reopened.journal_dirty(),
            "Recovery must replay the edit beyond the saved base");
    reopened.undo();
    require(reopened.read()->name == "Saved ledger" && !reopened.journal_dirty(),
            "Undo must restore the saved values");
    reopened.redo();
    require(reopened.read()->name == "Journaled ledger" && reopened.journal_dirty(),
            "Redo must restore the journaled values");
    reopened
        .save_journal(); // Full checkpoint: includes retained history, then rotates the sidecar.
    require(!reopened.journal_dirty(), "Full Save must update the saved baseline");
  }
  {
    ledger_store reopened{ledger_example::ledger{}};
    reopened.recover_journal(path);
    require(reopened.read()->name == "Journaled ledger" && !reopened.journal_dirty(),
            "The full Save must reopen cleanly");
    reopened.undo();
    require(reopened.read()->name == "Saved ledger", "Full Save must preserve retained undo");
    reopened.redo();
    require(!reopened.journal_dirty(), "Returning to saved values must clear dirty state");
    std::cout << path << ": edit, no-change, recovery, undo/redo, full Save verified\n";
  }
}
} // namespace

// Leave inspectable files in a fresh child directory; no existing document is overwritten or deleted.
int main(int argc, char** argv) {
  try {
    if (argc > 2) {
      throw std::invalid_argument{"Usage: managed_journal_example [output-directory]"};
    }
    const auto root =
        argc == 2 ? std::filesystem::path{argv[1]} : std::filesystem::path{"journal_demo"};
    std::filesystem::create_directories(root);
    const auto generation = managed::make_document_id();
    const auto directory =
        root / ("run-" + std::to_string(generation.high) + "-" + std::to_string(generation.low));
    require(std::filesystem::create_directory(directory), "Example directory already exists");
    demonstrate(directory / "appended.srj", managed::journal_storage_mode::appended);
    demonstrate(directory / "sidecar.srj", managed::journal_storage_mode::sidecar);
    std::cout << "Journal examples passed; files retained in " << directory << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
