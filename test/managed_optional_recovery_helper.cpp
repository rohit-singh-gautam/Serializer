// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <optional_managed.hpp>
#include <rohit/managed.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Separate invocations verify that journal replay has no hidden process-local optional state.
int main(int argc, char** argv) {
  if (argc != 4) { return 2; }
  try {
    namespace managed = rohit::managed;
    using document = optional_managed::document;
    using leaf = optional_managed::leaf;
    managed::model_store<document> store{document{}};
    const auto mode = std::string_view{argv[2]} == "appended" ?
        managed::journal_storage_mode::appended : managed::journal_storage_mode::sidecar;
    const std::filesystem::path path{argv[1]};
    if (std::string_view{argv[3]} == "create") {
      store.create_journal(path, mode);
      store.execute_transaction([](auto& tx) {
        tx.root().set_scalar(42);
        if (tx.root().child().emplace(leaf{"durable"}) != 2) {
          throw std::runtime_error{"Unexpected initial optional identity"};
        }
      }).throw_if_failed();
      return 0;
    }
    store.recover_journal(path);
    if (!store.read()->child || store.read()->child->persistent_id != 2 ||
        store.read()->child->text != "durable" || store.read()->child->cache != 7 ||
        store.read()->scalar != 42) {
      throw std::runtime_error{"Optional journal recovery mismatch"};
    }
    store.undo();
    if (store.read()->child || store.read()->scalar) {
      throw std::runtime_error{"Recovered optional history mismatch"};
    }
    store.execute_transaction([](auto& tx) {
      if (tx.root().child().emplace(leaf{"next"}) != 3) {
        throw std::runtime_error{"Recovered optional allocator reused an identity"};
      }
    }).throw_if_failed();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
