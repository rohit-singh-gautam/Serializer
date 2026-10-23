// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/managed.hpp>

#include <iostream>

// Create a typed cycle only after the store assigns identities to the named generic occurrences.
int main() {
  namespace managed = rohit::managed;
  using document = named_reference_models::document;
  using node = named_reference_models::number_node;
  using traits = managed::model_traits<document>;
  document initial{};
  initial.nodes.resize(2);
  initial.nodes[0].value = 11;
  initial.nodes[1].value = 22;
  managed::model_store<document> store{initial};
  const auto initial_view = store.read();
  const auto& nodes = traits::payload(*initial_view).nodes;
  const auto first = nodes[0].persistent_id;
  const auto second = nodes[1].persistent_id;
  store.execute_transaction([&](auto& transaction) {
    const auto registry = transaction.registry();
    transaction.root().nodes().edit(first).set_next(registry.template reference<node>(second));
    transaction.root().nodes().edit(second).set_next(registry.template reference<node>(first));
  }).throw_if_failed();
  const auto registry = store.registry();
  const auto first_node = registry.find<node>(first);
  const auto next_node = registry.resolve(*first_node.value().next);
  std::cout << first_node.value().value << " -> " << next_node.value().value << '\n';
  return registry.resolve(*next_node.value().next).id() == first ? 0 : 1;
}
