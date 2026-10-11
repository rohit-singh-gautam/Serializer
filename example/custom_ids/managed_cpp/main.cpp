// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "model.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
namespace model = custom_ids_managed;
using store_type = rohit::managed::model_store<model::library>;
using descriptors = rohit::managed::fields<model::book>;

// Create a typed, customer-scoped application ID independent of internal storage identity.
model::book_id key(std::uint64_t customer, std::uint64_t number) {
  model::book_id result;
  result.scope.customer = customer;
  result.number = number;
  return result;
}

// Fail the runnable example if a documented state-management guarantee is violated.
void require(bool condition) {
  if (!condition) { throw std::logic_error{"Custom ID example invariant failed"}; }
}
} // namespace

// Demonstrate custom addressing while Serializer retains scalar IDs and transaction authority.
int main() {
  model::library initial;
  initial.books.resize(2);
  initial.books[0].key = key(17, 100);
  initial.books[0].title = "First";
  initial.books[1].key = key(17, 101);
  initial.books[1].title = "Second";
  store_type store{initial};
  store.declare_unique_key(descriptors::field_key());

  auto selected = store.registry();
  auto custom_keys = selected.ordered(descriptors::field_key(), true);
  const auto internal_id = custom_keys.at(key(17, 100)).front();
  const auto pinned = selected.find<model::book>(internal_id);

  // Rebinding a customer key changes ordinary state, preserving the persistent identity.
  store.execute_transaction([&](auto& tx) {
    tx.registry().bind(internal_id, descriptors::field_key(), key(17, 200));
  }).throw_if_failed();
  require(store.registry().find<model::book>(internal_id).value().key.number == 200);
  require(pinned.value().key.number == 100);
  require(custom_keys.at(key(17, 100)).front() == internal_id);
  store.undo();
  require(store.registry().find<model::book>(internal_id).value().key.number == 100);
  store.redo();

  const auto duplicate = store.execute_transaction([&](auto& tx) {
    tx.registry().bind(internal_id, descriptors::field_key(), key(17, 101));
  });
  require(duplicate.status == rohit::managed::transaction_status::failed);
  require(store.registry().find<model::book>(internal_id).value().key.number == 200);

  // New imports receive fresh reserved scalar identity; removal never recycles that number.
  auto imported = model::book{};
  imported.key = key(18, 100); // The same local number is valid in another scope.
  imported.title = "Imported";
  auto adopted = store_type::id_type{};
  store.execute_transaction([&](auto& tx) {
    adopted = tx.registry().adopt(imported, [](auto& candidate, auto value) {
      candidate.books.push_back(std::move(value));
    });
  }).throw_if_failed();
  require(store.registry().find<model::book>(adopted).value().key.scope.customer == 18);
  const auto watermark = store.allocated_id();
  store.execute_transaction([&](auto& tx) {
    tx.registry().template retire<model::book>(adopted, [](auto& candidate, auto id) {
      std::erase_if(candidate.books, [id](const auto& value) { return value.persistent_id == id; });
    });
  }).throw_if_failed();
  require(!store.registry().contains(adopted));
  require(store.allocated_id() == watermark);
  store.undo();
  require(store.registry().contains(adopted));
  std::cout << "PASS: managed custom keys, pinned reads, rebind, rollback, import, retirement and undo\n";
}
