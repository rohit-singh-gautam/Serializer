#include <ledger.hpp>
#include <rohit/managed.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
namespace managed = rohit::managed;
constexpr int interruption_exit_code = 86;

// Resolve names shared with the subprocess driver without exposing enum ordinals as a wire contract.
managed::journal_io_event parse_event(std::string_view name) {
  using event = managed::journal_io_event;
  constexpr std::pair<std::string_view, event> events[] = {
      {"before_append", event::before_append},
      {"after_header", event::after_header},
      {"after_payload", event::after_payload},
      {"after_commit", event::after_commit},
      {"after_flush", event::after_flush},
      {"after_sidecar_flush", event::after_sidecar_flush},
      {"after_base_flush", event::after_base_flush},
      {"before_replace", event::before_replace},
      {"after_replace", event::after_replace},
      {"after_publish", event::after_publish},
      {"before_cleanup", event::before_cleanup},
      {"before_tail_repair", event::before_tail_repair},
      {"after_tail_repair", event::after_tail_repair}};
  for (const auto& [text, value] : events) {
    if (name == text) {
      return value;
    }
  }
  throw std::invalid_argument{"Unknown crash boundary"};
}
} // namespace

// Terminate at a precise storage boundary without running store or file destructors.
int main(int argc, char** argv) {
  if (argc != 5) {
    return 2;
  }
  try {
    const std::filesystem::path path{argv[1]};
    const auto mode = std::string_view{argv[2]} == "appended"
                          ? managed::journal_storage_mode::appended
                          : managed::journal_storage_mode::sidecar;
    const std::string_view operation{argv[3]};
    const auto point = parse_event(argv[4]);
    bool armed = operation == "recover";
    managed::journal_options options;
    options.fault_injector = [&](auto actual) {
      if (armed && point == actual) {
        std::_Exit(interruption_exit_code);
      }
    };
    managed::model_store<ledger_example::ledger> store{ledger_example::ledger{"Base"}};
    if (operation == "create") {
      store.create_journal(path, mode, options);
      return 0;
    }
    store.recover_journal(path, options);
    armed = operation != "inspect" && operation != "edit";
    if (operation == "append" || operation == "edit") {
      store.execute_transaction([](auto& tx) { tx.root().set_name("Changed"); }).throw_if_failed();
    } else if (operation == "reserve") {
      store.execute_transaction([](auto& tx) { tx.root().entries().insert(10, {"New", 10}); })
          .throw_if_failed();
    } else if (operation == "undo") {
      store.undo();
    } else if (operation == "save") {
      store.save_journal();
    } else if (operation == "inspect") {
      const auto envelope =
          managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
      std::cout << store.read()->name << ' ' << store.journal_sequence() << ' '
                << store.journal_dirty() << ' ' << envelope.allocated_id << '\n';
    } else if (operation != "recover") {
      throw std::invalid_argument{"Unknown crash operation"};
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
