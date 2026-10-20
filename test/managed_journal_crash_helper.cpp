#include <ledger.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal_document.hpp>
#include <rohit/serializer.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
constexpr int interruption_exit_code = 86;

// Export ordinary JSON model fields while keeping native recovery metadata in the companion.
std::vector<std::uint8_t> encode_document(const ledger_example::ledger& value) {
  rohit::full_stream_auto_alloc output;
  value.serialize_out<rohit::serializer::json>(output);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require one complete owning JSON model independently of the journal's binary metadata.
ledger_example::ledger decode_document(std::span<const std::uint8_t> input) {
  const auto stream = rohit::make_constant_full_stream(input.data(), input.size());
  return rohit::serializer::deserialize_exact<ledger_example::ledger, rohit::serializer::json>(
      stream);
}

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
  if (argc != 5 && (argc != 6 || std::string_view{argv[5]} != "--delta")) {
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
    if (std::string_view{argv[2]} == "document") {
      options.document_codec = managed::make_journal_document_codec<ledger_example::ledger>(
          "application/json", encode_document, decode_document);
    }
    options.fault_injector = [&](auto actual) {
      if (armed && point == actual) {
        std::_Exit(interruption_exit_code);
      }
    };
    const bool delta = argc == 6;
    const std::string initial = delta ? std::string(100000, 'a') : std::string{"Base"};
    managed::store_options storage;
    storage.delta_journal = delta;
    storage.max_delta_chain = 2;
    managed::model_store<ledger_example::ledger> store{ledger_example::ledger{initial},
                                                       managed::make_document_id(), storage};
    if (operation == "create") {
      store.create_journal(path, mode, options);
      return 0;
    }
    store.recover_journal(path, options);
    armed = operation != "inspect" && operation != "edit";
    if (operation == "append" || operation == "edit") {
      auto changed = delta ? initial : std::string{"Changed"};
      if (delta) {
        changed[50000] = 'b';
      }
      store.execute_transaction([&](auto& tx) { tx.root().set_name(changed); }).throw_if_failed();
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
      const auto name = delta ? (store.read()->name[50000] == 'b' ? "Changed" : "Base")
                              : store.read()->name.c_str();
      std::cout << name << ' ' << store.journal_sequence() << ' ' << store.journal_dirty() << ' '
                << envelope.allocated_id << '\n';
    } else if (operation != "recover") {
      throw std::invalid_argument{"Unknown crash operation"};
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
