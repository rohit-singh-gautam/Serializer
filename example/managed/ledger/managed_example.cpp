// See README.md for the detailed walkthrough.
#include <ledger.hpp>
#include <rohit/managed.hpp>

#include <cstdint>
#include <iostream>

// Demonstrate transaction completion, stable identity, undo, and memory save/load.
int main() {
  namespace managed = rohit::managed;
  using ledger_store = managed::model_store<ledger_example::ledger>;
  static constexpr std::uint64_t draft_key = 100;

  // Automatically create the document namespace and assign root ID 1.
  ledger_store store{ledger_example::ledger{"Draft ledger"}};
  ledger_store::id_type entry_id{};

  // Manual commit; the outcome must outlive the transaction guard.
  managed::transaction_outcome inserted;
  {
    auto transaction = store.begin_transaction(inserted);
    entry_id = transaction.root().entries().insert(draft_key, {"Supplies", 1500});
    transaction.commit();
  }
  inserted.throw_if_failed();

  // Normal scope exit commits; check the outcome after guard destruction.
  managed::transaction_outcome adjusted;
  {
    auto transaction = store.begin_transaction(adjusted);
    transaction.root().entries().edit(entry_id).set_amount_minor_units(1800);
  }
  adjusted.throw_if_failed();

  // The callback form completes the transaction before returning.
  const auto described = store.execute_transaction([entry_id](auto& transaction) {
    transaction.root().entries().edit(entry_id).set_memo("Office supplies");
  });
  described.throw_if_failed();

  // Undo the description change, preserving the amount adjustment and identity.
  store.undo();

  // Reload the saved document namespace, object IDs, and history from memory.
  const auto bytes = store.save();
  ledger_store restored{ledger_example::ledger{"Temporary"}};
  restored.load(bytes);
  const auto value = restored.clone_value();
  if (restored.read()->entries.at(draft_key).persistent_id != entry_id ||
      value.entries.at(draft_key).memo != "Supplies" ||
      value.entries.at(draft_key).amount_minor_units != 1800) {
    return 1;
  }
  std::cout
      << "Managed ledger: generated editors, identity, transactions, undo, and reload verified\n";
}
