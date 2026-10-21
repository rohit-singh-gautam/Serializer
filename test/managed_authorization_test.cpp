// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <collaboration_model.hpp>
#include <ledger.hpp>
#include <rohit/managed_allocation.hpp>
#include <rohit/managed_capabilities.hpp>
#include <rohit/managed_collaboration.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using permission = managed::authorization_permission;
using decision = managed::authorization_decision;
using scope = managed::authorization_scope;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;

struct empty_entity { std::uint32_t persistent_id{}; };
struct empty_entity_root {
  std::uint32_t persistent_id{};
  std::vector<empty_entity> children{};
};
struct empty_entity_traits {
  // Identity-only children have no durable payload fields.
  static void visit_collaboration_fields(const empty_entity&, auto&) {}
};
struct empty_entity_root_traits {
  // Supply the normal generated owning-field contract for an identity-only child fixture.
  static void visit_collaboration_fields(const empty_entity_root& value, auto& visitor) {
    visitor.template owned<empty_entity_traits>(value.persistent_id, 1, value.children);
  }
};

// Grant both content and structure under one independently identified ownership subtree.
std::vector<managed::authorization_rule> editable_subtree(std::uint64_t entity) {
  return {{{entity, {}}, permission::content}, {{entity, {}}, permission::structure}};
}

// Install a durable generated-graph policy while explicitly rejecting document-namespace replacement.
template <typename Store>
void install_policy(Store& store, const managed::authorization_policy& policy) {
  store.set_authorization_policy([&policy](auto operation, const auto& before,
                                           const auto& candidate, bool replaced_namespace) {
    if (operation == managed::authorization_operation::replicate) { return true; }
    return !replaced_namespace && managed::authorize_managed_candidate<typename Store::traits_type>(
        policy, before, candidate, managed::store_options::default_snapshot_bytes);
  });
}

// Remove task-owned journals even when an assertion throws or aborts a test body.
class temporary_journal_directory {
  const std::filesystem::path path_;
public:
  // Create a uniquely named test-owned directory in the active build/test working directory.
  temporary_journal_directory()
      : path_{std::filesystem::current_path() /
          ("serializer-authorization-" + std::to_string(managed::make_document_id().high))} {
    if (!std::filesystem::create_directory(path_)) {
      throw std::runtime_error{"Authorization test directory already exists"};
    }
  }
  // The checked path was created by this owner; never clean a caller-supplied directory.
  ~temporary_journal_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  // Address only this owner's journal and its native companions.
  std::filesystem::path document() const { return path_ / "document"; }
};

// Run every optional mechanism through real native publication and Save/reopen for one history policy.
template <managed::history_mode Mode, managed::history_labels Labels>
void check_native_capability_combinations() {
  using selected_store = managed::model_store<ledger, Mode, Labels>;
  for (std::uint32_t optional = 0; optional < 8; ++optional) {
    temporary_journal_directory directory;
    const bool collaboration = (optional & 1U) != 0;
    const bool authorization = (optional & 2U) != 0;
    const bool journal = (optional & 4U) != 0;
    auto selected = Mode == managed::history_mode::disabled ? managed::managed_capability::none
        : managed::managed_capability::history;
    if (collaboration) { selected = selected | managed::managed_capability::collaboration; }
    if (authorization) { selected = selected | managed::managed_capability::authorization; }
    if (journal) { selected = selected | managed::managed_capability::journal; }
    managed::managed_capability_profile profile;
    profile.selected = managed::managed_capability_profile::version_one_all;
    profile.excluded = static_cast<managed::managed_capability>(
        static_cast<std::uint32_t>(profile.selected) & ~static_cast<std::uint32_t>(selected));
    managed::collaboration_authority<ledger> authority{ledger{"Before"}, 1};
    authority.open_session(10);
    managed::collaboration_transport transport{authority, std::uint64_t{10}};
    std::size_t authorization_calls{};
    const auto make_store = [&] {
      auto result = std::make_unique<selected_store>(ledger{"Before"});
      typename selected_store::authorization_callback callback;
      if (authorization) {
        callback = [&](auto, const auto&, const auto&, bool) { ++authorization_calls; return true; };
      }
      result->configure_capabilities(profile, std::move(callback));
      EXPECT_EQ(result->enabled_capabilities(), selected);
      if (collaboration) { result->collaborate(std::uint64_t{10}).bind(transport); }
      return result;
    };
    auto store = make_store();
    EXPECT_NE(store->read()->persistent_id, 0U);
    if (collaboration) { store->synchronize(); }
    if (journal) { store->create_journal(directory.document()); }
    else { EXPECT_THROW(store->create_journal(directory.document()), std::logic_error); }
    store->execute_transaction([](auto& tx) { tx.root().set_name("After"); }).throw_if_failed();
    if (collaboration) { store->synchronize(); }
    if constexpr (Mode != managed::history_mode::disabled) {
      store->undo();
      if (collaboration) { store->synchronize(); }
      EXPECT_EQ(store->read()->name, "Before");
    }
    const auto expected = store->read()->name;
    const auto document = store->document();
    const auto allocated = store->allocated_id();
    if (journal) { store->save_journal(); }
    const auto bytes = store->save();
    store.reset();
    auto restored = make_store();
    if (journal) { restored->recover_journal(directory.document()); }
    else { restored->load(bytes); }
    EXPECT_EQ(restored->read()->name, expected);
    EXPECT_EQ(restored->document(), document);
    EXPECT_EQ(restored->allocated_id(), allocated);
    EXPECT_EQ(restored->enabled_capabilities(), selected);
    EXPECT_EQ(authorization_calls != 0, authorization);
  }
}
} // namespace

// Missing grants deny, capabilities stay separate, and a descendant grant cannot override an ancestor denial.
TEST(managed_authorization, inherited_deny_and_separate_permissions) {
  const std::array<managed::authorization_target, 2> chain{{{1, {2}}, {7, {1}}}};
  managed::authorization_policy child_only{{{{7, {}}, permission::content}}};
  EXPECT_TRUE(child_only.permits(chain, permission::content));
  EXPECT_FALSE(child_only.permits(chain, permission::structure));
  EXPECT_FALSE(child_only.permits(chain, permission::policy));
  EXPECT_FALSE(child_only.permits({}, permission::content));
  managed::authorization_policy denied{{{{1, {}}, permission::content, decision::deny},
                                         {{7, {}}, permission::content}}};
  EXPECT_FALSE(denied.permits(chain, permission::content));
  managed::authorization_policy exact_parent{{{{1, {}}, permission::content, decision::allow, scope::target}}};
  EXPECT_FALSE(exact_parent.permits(chain, permission::content));
  EXPECT_THROW(managed::authorization_policy({{{0, {}}, permission::content}}), std::invalid_argument);
}

// A field grant covers its stable ordinary scope; neighboring fields and policy administration remain denied.
TEST(managed_authorization, stable_field_scopes) {
  managed::authorization_policy policy{{{{1, {4, 2}}, permission::content}}};
  const std::array<managed::authorization_target, 1> allowed{{{1, {4, 2, 1}}}};
  const std::array<managed::authorization_target, 1> other{{{1, {4, 3}}}};
  EXPECT_TRUE(policy.permits(allowed, permission::content));
  EXPECT_FALSE(policy.permits(other, permission::content));
  EXPECT_FALSE(policy.permits(allowed, permission::policy));
}

// Explicit nearest-rule policy supports editable exceptions while equal-specificity denials win ties.
TEST(managed_authorization, nearest_rule_exceptions_are_explicit_and_deterministic) {
  const std::array<managed::authorization_target, 2> chain{{{1, {2}}, {7, {1}}}};
  const std::vector<managed::authorization_rule> rules{
      {{1, {}}, permission::content, decision::deny}, {{7, {}}, permission::content}};
  managed::authorization_policy defaults{rules};
  managed::authorization_policy nearest{rules, managed::authorization_precedence::nearest_rule};
  EXPECT_FALSE(defaults.permits(chain, permission::content));
  EXPECT_TRUE(nearest.permits(chain, permission::content));
  auto ties = rules;
  ties.push_back({{7, {}}, permission::content, decision::deny});
  managed::authorization_policy denied{ties, managed::authorization_precedence::nearest_rule};
  EXPECT_FALSE(denied.permits(chain, permission::content));
  std::reverse(ties.begin(), ties.end());
  managed::authorization_policy reversed{ties, managed::authorization_precedence::nearest_rule};
  EXPECT_FALSE(reversed.permits(chain, permission::content));
}

// Generated field comparisons apply child exceptions independently of the parent's content permission.
TEST(managed_authorization, generated_content_and_structural_boundaries) {
  store_type store{ledger{"Before", {{10, {"Protected", 100}}, {20, {"Editable", 200}}}}};
  const auto root = store.read()->persistent_id;
  const auto protected_id = store.read()->entries.at(10).persistent_id;
  auto rules = editable_subtree(root);
  rules.push_back({{protected_id, {}}, permission::content, decision::deny});
  managed::authorization_policy policy{std::move(rules)};
  install_policy(store, policy);
  const auto saved = store.save();
  const auto pin = store.read();
  auto outcome = store.execute_transaction([](auto& tx) {
    tx.update([](auto& value) { value.entries.at(10).memo = "Denied"; });
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_THROW(outcome.throw_if_failed(), managed::authorization_error);
  EXPECT_EQ(store.save(), saved);
  EXPECT_EQ(store.read(), pin);
  outcome = store.execute_transaction([](auto& tx) {
    tx.update([](auto& value) { value.entries.erase(10); });
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  store.execute_transaction([](auto& tx) {
    tx.update([](auto& value) { value.entries.at(20).memo = "Allowed"; });
  }).throw_if_failed();
  EXPECT_EQ(store.read()->entries.at(20).memo, "Allowed");
}

// Changing a collection key is structural even when every entity and its payload remain identical.
TEST(managed_authorization, collection_rekey_requires_structure) {
  store_type store{ledger{"Before", {{10, {"Entry", 100}}}}};
  managed::authorization_policy policy{{{{store.read()->persistent_id, {}}, permission::content}}};
  install_policy(store, policy);
  const auto saved = store.save();
  const auto outcome = store.execute_transaction([](auto& tx) {
    tx.update([](auto& value) {
      auto node = value.entries.extract(10);
      node.key() = 20;
      value.entries.insert(std::move(node));
    });
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
}

// Protection applies to identity-only entities as well as entities with encoded payload fields.
TEST(managed_authorization, empty_entity_deletion_still_requires_permission) {
  const empty_entity_root before{1, {{2}}}, after{1, {}};
  managed::authorization_policy policy{{{{1, {}}, permission::structure},
      {{1, {}}, permission::content}, {{2, {}}, permission::content, decision::deny}}};
  EXPECT_FALSE(managed::authorize_managed_candidate<empty_entity_root_traits>(policy, before, after, 1024));
}

// Failed authorization burns any exposed allocations while preserving the accepted model/history.
TEST(managed_authorization, rejected_insert_never_reuses_issued_identity) {
  store_type store{ledger{"Before"}};
  const auto allocated = store.allocated_id();
  store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return false; });
  EXPECT_EQ(store.execute_transaction([](auto& tx) {
    tx.root().entries().insert(10, {"Denied", 100});
  }).status, managed::transaction_status::failed);
  EXPECT_TRUE(store.read()->entries.empty());
  EXPECT_GT(store.allocated_id(), allocated);
  const auto burned = store.allocated_id();
  store.set_authorization_policy({});
  store.execute_transaction([](auto& tx) {
    tx.root().entries().insert(10, {"Allowed", 100});
  }).throw_if_failed();
  EXPECT_GT(store.read()->entries.at(10).persistent_id, burned);
}

// A move checks the old and new ancestors, including unchanged child bytes and nested protection.
TEST(managed_authorization, moving_out_of_protected_subtree_cannot_evade_denial) {
  using root_type = collaboration_test::workspace;
  managed::model_store<root_type> store{
      root_type{{{1, {"First", {{10, {"Child", 100}}}}}, {2, {"Second"}}}}};
  auto rules = editable_subtree(store.read()->persistent_id);
  rules.push_back({{store.read()->ledgers.at(1).persistent_id, {}}, permission::content, decision::deny});
  managed::authorization_policy policy{std::move(rules)};
  install_policy(store, policy);
  const auto saved = store.save();
  const auto outcome = store.execute_transaction([](auto& tx) {
    tx.update([](auto& value) {
      auto node = value.ledgers.at(1).entries.extract(10);
      value.ledgers.at(2).entries.insert(std::move(node));
    });
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
}

// Undo/redo use current permissions and leave the cursor intact after denial.
TEST(managed_authorization, current_policy_gates_history_and_load) {
  store_type store{ledger{"Before"}};
  const auto original = store.save();
  store.execute_transaction([](auto& tx) { tx.root().set_name("After"); }).throw_if_failed();
  const auto modified = store.save();
  managed::authorization_policy deny{{}};
  install_policy(store, deny);
  EXPECT_THROW(store.undo(), managed::authorization_error);
  EXPECT_EQ(store.save(), modified);
  EXPECT_THROW(store.load(original), managed::authorization_error);
  EXPECT_EQ(store.save(), modified);
  store.set_authorization_policy({});
  store.undo();
  install_policy(store, deny);
  EXPECT_THROW(store.redo(), managed::authorization_error);
  EXPECT_EQ(store.read()->name, "Before");
  store.set_authorization_policy({});
  store.redo();
  EXPECT_EQ(store.read()->name, "After");
}

// A document-scoped rule never grants permission to a coincidentally equal ID in another document.
TEST(managed_authorization, foreign_namespace_load_is_explicitly_flagged) {
  store_type store{ledger{"Before"}}, foreign{ledger{"Foreign"}};
  const auto saved = store.save();
  bool saw_replacement = false;
  store.set_authorization_policy([&](auto operation, const auto&, const auto&, bool replaced_namespace) {
    saw_replacement = replaced_namespace && operation == managed::authorization_operation::replace;
    return !replaced_namespace;
  });
  EXPECT_THROW(store.load(foreign.save()), managed::authorization_error);
  EXPECT_TRUE(saw_replacement);
  EXPECT_EQ(store.save(), saved);
}

// Policy exceptions and reentrant mutation fail before state/history publication and permit later recovery.
TEST(managed_authorization, exception_and_reentry_are_atomic) {
  store_type store{ledger{"Before"}};
  const auto saved = store.save();
  store.set_authorization_policy([&](auto, const auto&, const auto&, bool) {
    store.set_authorization_policy({});
    return true;
  });
  EXPECT_EQ(store.execute_transaction([](auto& tx) { tx.root().set_name("Denied"); }).status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  store.set_authorization_policy([](auto, const auto&, const auto&, bool) -> bool {
    throw std::runtime_error{"Policy unavailable"};
  });
  EXPECT_EQ(store.execute_transaction([](auto& tx) { tx.root().set_name("Denied"); }).status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  store.set_authorization_policy({});
  store.execute_transaction([](auto& tx) { tx.root().set_name("Recovered"); }).throw_if_failed();
}

// Denied writes never reach either journal layout; recovery and discard evaluate complete candidates.
TEST(managed_authorization, journal_denial_and_recovery_boundaries) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    temporary_journal_directory directory;
    {
      store_type store{ledger{"Before"}};
      store.create_journal(directory.document(), mode);
      store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return false; });
      EXPECT_EQ(store.execute_transaction([](auto& tx) { tx.root().set_name("Denied"); }).status,
                managed::transaction_status::failed);
      EXPECT_FALSE(store.journal_dirty());
      store.set_authorization_policy({});
      store.execute_transaction([](auto& tx) { tx.root().set_name("Accepted"); }).throw_if_failed();
      store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return false; });
      EXPECT_THROW(store.discard_journal_changes(), managed::authorization_error);
      EXPECT_EQ(store.read()->name, "Accepted");
    }
    {
      store_type store{ledger{"Fresh"}};
      store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return false; });
      EXPECT_THROW(store.recover_journal(directory.document()), managed::authorization_error);
      EXPECT_EQ(store.read()->name, "Fresh");
      store.set_authorization_policy({});
      store.recover_journal(directory.document());
      EXPECT_EQ(store.read()->name, "Accepted");
    }
  }
}

// Trusted replication remains available to a read-only client while local authorship is denied.
TEST(managed_authorization, readonly_replication_is_separate_from_editing) {
  managed::collaboration_authority<ledger> authority{ledger{"Before"}, 1};
  authority.open_session(10);
  authority.open_session(20, false);
  managed::collaboration_transport writer_transport{authority, std::uint64_t{10}};
  managed::collaboration_transport reader_transport{authority, std::uint64_t{20}};
  store_type writer{ledger{}}, reader{ledger{}};
  writer.collaborate(std::uint64_t{10}).bind(writer_transport);
  reader.collaborate(std::uint64_t{20}).bind(reader_transport);
  reader.set_authorization_policy([](auto operation, const auto&, const auto&, bool) {
    return operation == managed::authorization_operation::replicate;
  });
  writer.synchronize();
  reader.synchronize();
  EXPECT_EQ(reader.read()->name, "Before");
  EXPECT_EQ(reader.execute_transaction([](auto& tx) { tx.root().set_name("Denied"); }).status,
            managed::transaction_status::failed);
  writer.execute_transaction([](auto& tx) { tx.root().set_name("Published"); }).throw_if_failed();
  writer.synchronize();
  reader.synchronize();
  EXPECT_EQ(reader.read()->name, "Published");
  EXPECT_EQ(reader.collaboration().pending_count(), 0u);
}

// The same generated rules enforce the authenticated authority boundary and exact retry decisions.
TEST(managed_authorization, authority_uses_identical_complete_candidate_rules) {
  managed::collaboration_authority<ledger> authority{
      ledger{"Before", {{10, {"Entry", 100}}}}, 1, managed::make_document_id(), {}, {},
      [](const auto&, const auto& before, const auto& candidate, const auto&) {
        auto rules = editable_subtree(before.persistent_id);
        rules.push_back({{before.persistent_id, {1}}, permission::content, decision::deny});
        const managed::authorization_policy policy{std::move(rules)};
        return managed::authorize_managed_candidate<store_type::traits_type>(
            policy, before, candidate, managed::store_options::default_snapshot_bytes);
      }};
  authority.open_session(10);
  managed::collaboration_replica<ledger> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto denied = replica.propose(10, 1, [](auto& tx) { tx.root().set_name("Denied"); });
  const auto result = authority.submit_change(denied, 10, 0);
  EXPECT_EQ(result->status, managed::collaboration_status::denied);
  EXPECT_EQ(authority.submit_change(denied, 10, 1), result);
  EXPECT_EQ(authority.read()->name, "Before");
  const auto id = replica.read()->entries.at(10).persistent_id;
  const auto allowed = replica.propose(10, 2, [id](auto& tx) {
    tx.root().entries().edit(id).set_amount_minor_units(200);
  });
  EXPECT_EQ(authority.submit_change(allowed, 10, 2)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.read()->entries.at(10).amount_minor_units, 200);
}

// Bulk editable application and imported identities pass the same content/structure publication rules.
TEST(managed_authorization, editable_and_import_share_the_complete_candidate_boundary) {
  store_type store{ledger{"Before", {{10, {"Protected", 100}}}}};
  const auto root = store.read()->persistent_id;
  const auto child = store.read()->entries.at(10).persistent_id;
  auto rules = editable_subtree(root);
  rules.push_back({{child, {}}, permission::content, decision::deny});
  managed::authorization_policy policy{std::move(rules)};
  install_policy(store, policy);
  const auto baseline = store.read();
  auto candidate = *baseline;
  candidate.entries.at(10).memo = "Denied";
  candidate.persistent_id = 0;
  EXPECT_EQ(store.execute_transaction([&](auto& tx) {
    tx.root().apply_editable(std::move(candidate));
  }).status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), baseline);
  rules = editable_subtree(root);
  rules.push_back({{root, {2}}, permission::structure, decision::deny});
  policy = managed::authorization_policy{std::move(rules)};
  EXPECT_EQ(store.execute_transaction([](auto& tx) {
    tx.registry().adopt(ledger_example::entry{"Imported", 200}, [](auto& destination, auto value) {
      destination.entries.emplace(20, std::move(value));
    });
  }).status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), baseline);
  const auto burned = store.allocated_id();
  policy = managed::authorization_policy{editable_subtree(root)};
  store.execute_transaction([](auto& tx) {
    tx.registry().adopt(ledger_example::entry{"Imported", 200}, [](auto& destination, auto value) {
      destination.entries.emplace(20, std::move(value));
    });
  }).throw_if_failed();
  EXPECT_GT(store.read()->entries.at(20).persistent_id, burned);
}

// Application context revocation is read at commit; an already issued editor is not a permanent grant.
TEST(managed_authorization, required_provider_checks_current_host_context_at_publication) {
  store_type store{ledger{"Before"}};
  bool authenticated = true;
  managed::managed_capability_profile profile;
  profile.selected = managed::managed_capability::history | managed::managed_capability::authorization;
  store.configure_capabilities(profile, [&](auto, const auto&, const auto&, bool) { return authenticated; });
  const auto before = store.read();
  store_type::outcome_type outcome;
  auto transaction = store.begin_transaction(outcome);
  transaction.root().set_name("Candidate");
  authenticated = false;
  EXPECT_THROW(transaction.commit(), managed::authorization_error);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_THROW(store.reset_history(), managed::authorization_error);
  authenticated = true;
  EXPECT_NO_THROW(store.reset_history());
}

// Explicit profile versions cannot silently expand features or bypass required authorization.
TEST(managed_capabilities, versioned_selection_and_engine_validation) {
  using capability = managed::managed_capability;
  managed::managed_capability_profile profile;
  profile.selected = managed::managed_capability_profile::version_one_all;
  profile.excluded = capability::collaboration | capability::journal;
  store_type store{ledger{}};
  EXPECT_THROW(managed::validate_managed_capability_profile(store, profile), std::invalid_argument);
  store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return true; });
  EXPECT_NO_THROW(managed::validate_managed_capability_profile(store, profile, true));
  profile.excluded = profile.excluded | capability::authorization;
  EXPECT_THROW(managed::validate_managed_capability_profile(store, profile, true), std::invalid_argument);
  profile.version = 2;
  EXPECT_THROW(profile.resolve(), std::invalid_argument);
  profile.version = 1;
  profile.selected = static_cast<capability>(16);
  EXPECT_THROW(profile.resolve(), std::invalid_argument);
  profile.selected = capability::none;
  profile.excluded = capability::journal;
  EXPECT_THROW(profile.resolve(), std::invalid_argument);
}

// All forty legal mode/label/mechanism combinations execute real history, journal and session engines.
TEST(managed_capabilities, exhaustive_native_component_activation_and_recovery) {
  check_native_capability_combinations<managed::history_mode::disabled, managed::history_labels::disabled>();
  check_native_capability_combinations<managed::history_mode::linear, managed::history_labels::disabled>();
  check_native_capability_combinations<managed::history_mode::linear, managed::history_labels::enabled>();
  check_native_capability_combinations<managed::history_mode::tree, managed::history_labels::disabled>();
  check_native_capability_combinations<managed::history_mode::tree, managed::history_labels::enabled>();
}

// Required authorization cannot be cleared or downgraded, and invalid reconfiguration preserves the old gate.
TEST(managed_capabilities, required_authorization_and_exclusions_cannot_be_bypassed) {
  store_type store{ledger{"Before"}};
  managed::managed_capability_profile profile;
  profile.selected = managed::managed_capability::history | managed::managed_capability::authorization;
  EXPECT_THROW(store.configure_capabilities(profile), std::invalid_argument);
  store.configure_capabilities(profile, [](auto, const auto&, const auto&, bool) { return false; });
  EXPECT_THROW(store.set_authorization_policy({}), std::invalid_argument);
  auto omitted = profile;
  omitted.excluded = managed::managed_capability::authorization;
  EXPECT_THROW(store.configure_capabilities(omitted), std::invalid_argument);
  EXPECT_EQ(store.execute_transaction([](auto& tx) { tx.root().set_name("Denied"); }).status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.read()->name, "Before");
  EXPECT_THROW(store.collaborate(std::uint64_t{10}), std::logic_error);
  store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return true; });
  store.execute_transaction([](auto& tx) { tx.root().set_name("Accepted"); }).throw_if_failed();
  EXPECT_EQ(store.read()->name, "Accepted");
  store_type excluded{ledger{}};
  profile.selected = managed::managed_capability::history;
  excluded.configure_capabilities(profile);
  EXPECT_THROW(excluded.set_authorization_policy([](auto, const auto&, const auto&, bool) { return true; }),
               std::invalid_argument);
}

// Attached durable/session engines reject exclusion rather than silently dropping recovery/outbox state.
TEST(managed_capabilities, attached_components_cannot_be_excluded) {
  temporary_journal_directory directory;
  store_type journal{ledger{}};
  journal.create_journal(directory.document());
  managed::managed_capability_profile profile;
  profile.selected = managed::managed_capability::history;
  EXPECT_THROW(journal.configure_capabilities(profile), std::logic_error);
  store_type client{ledger{}};
  client.collaborate(std::uint64_t{10});
  EXPECT_THROW(client.configure_capabilities(profile), std::logic_error);
  using plain_store = managed::model_store<ledger, managed::history_mode::disabled,
      managed::history_labels::disabled, managed::model_traits<ledger>, managed::store_features::none>;
  plain_store plain{ledger{"Before"}};
  profile.selected = managed::managed_capability::journal;
  EXPECT_THROW(plain.configure_capabilities(profile), std::invalid_argument);
  profile.selected = managed::managed_capability::collaboration;
  EXPECT_THROW(plain.configure_capabilities(profile), std::invalid_argument);
  profile.selected = managed::managed_capability::none;
  plain.configure_capabilities(profile);
  plain.execute_transaction([](auto& tx) { tx.root().set_name("Identified"); }).throw_if_failed();
  EXPECT_NE(plain.read()->persistent_id, 0U);
}

// Bounded requests account actual PMR allocation sizes and release capacity for later allocations.
TEST(managed_allocation, hard_routed_limit_and_peak) {
  managed::bounded_memory_resource resource{64};
  auto* allocation = resource.allocate(64, alignof(std::max_align_t));
  EXPECT_EQ(resource.reserved_bytes(), 64u);
  EXPECT_THROW(static_cast<void>(resource.allocate(1, 1)), std::bad_alloc);
  EXPECT_EQ(resource.reserved_bytes(), 64u);
  resource.deallocate(allocation, 64, alignof(std::max_align_t));
  EXPECT_EQ(resource.reserved_bytes(), 0u);
  EXPECT_EQ(resource.peak_reserved_bytes(), 64u);
  EXPECT_EQ(resource.maximum_bytes(), 64u);
  EXPECT_THROW(managed::bounded_memory_resource(64, {}), std::invalid_argument);
  managed::bounded_memory_resource maximum{std::numeric_limits<std::size_t>::max()};
  EXPECT_FALSE(resource.is_equal(maximum));
}

// Upstream allocation failures return their reservation; callers can retry without rebuilding the resource.
TEST(managed_allocation, upstream_failure_rolls_back_accounting) {
  const auto upstream = std::shared_ptr<std::pmr::memory_resource>{
      std::pmr::null_memory_resource(), [](std::pmr::memory_resource*) {}};
  managed::bounded_memory_resource resource{64, upstream};
  EXPECT_THROW(static_cast<void>(resource.allocate(64, 1)), std::bad_alloc);
  EXPECT_EQ(resource.reserved_bytes(), 0u);
  EXPECT_EQ(resource.peak_reserved_bytes(), 64u);
}

// Model pins preserve the allocation resource, and rejected candidates leave accepted storage readable.
TEST(managed_allocation, native_store_hook_and_pinned_lifetime) {
  auto resource = std::make_shared<managed::bounded_memory_resource>(1024 * 1024);
  std::weak_ptr<managed::bounded_memory_resource> weak = resource;
  managed::store_options options;
  options.allocation_resource = resource;
  auto store = std::make_unique<store_type>(ledger{"Before"}, managed::make_document_id(), options);
  auto pin = store->read();
  EXPECT_GT(resource->reserved_bytes(), 0u);
  store->execute_transaction([](auto& tx) { tx.root().set_name("After"); }).throw_if_failed();
  EXPECT_EQ(pin->name, "Before");
  store.reset();
  options.allocation_resource.reset();
  resource.reset();
  EXPECT_FALSE(weak.expired());
  pin.reset();
  EXPECT_TRUE(weak.expired());
}

// Concurrent reservations share one strict cap without changing native store thread confinement.
TEST(managed_allocation, concurrent_resource_reservations) {
  managed::bounded_memory_resource resource{128};
  std::atomic<bool> failed{};
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index) {
    threads.emplace_back([&] {
      for (int attempt = 0; attempt < 1000; ++attempt) {
        try {
          auto* block = resource.allocate(32, alignof(std::max_align_t));
          if (resource.reserved_bytes() > resource.maximum_bytes()) { failed = true; }
          resource.deallocate(block, 32, alignof(std::max_align_t));
        } catch (const std::bad_alloc&) {}
      }
    });
  }
  for (auto& thread : threads) { thread.join(); }
  EXPECT_FALSE(failed);
  EXPECT_EQ(resource.reserved_bytes(), 0u);
  EXPECT_LE(resource.peak_reserved_bytes(), resource.maximum_bytes());
}
