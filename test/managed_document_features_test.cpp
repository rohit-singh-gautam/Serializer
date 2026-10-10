#include <gtest/gtest.h>
#include <managed_document_features.hpp>
#include "../example/managed/document_features/behavior.hpp"
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
namespace model = managed_document_features;
namespace managed = rohit::managed;
using document_store = managed::model_store<model::document_data>;

// Create an ordinary document value; the store supplies every activated identity.
model::document_data initial_document() {
  model::document_data value{};
  value.title = "Project notes";
  std::get<0>(value.content).text = "First paragraph";
  return value;
}
} // namespace

// Managed base occurrences have separate IDs while sharing one atomic history.
TEST(managed_document_features, private_fields_and_independent_base_identities) {
  document_store store{initial_document()};
  const auto root_id = store.read()->persistent_id;
  const auto author_id = static_cast<const model::author_data&>(*store.read()).persistent_id;
  const auto review_id = static_cast<const model::review_data&>(*store.read()).persistent_id;
  ASSERT_NE(root_id, author_id);
  ASSERT_NE(root_id, review_id);
  ASSERT_NE(author_id, review_id);
  store.execute_transaction([&](auto& tx) {
    EXPECT_EQ(tx.root().author().id(), author_id);
    tx.root().author().set_name("Ada");
    tx.root().review().set_status("approved");
  }).throw_if_failed();
  store.execute_transaction([](auto& tx) {
    EXPECT_EQ(tx.root().author().get_name(), "Ada");
  }).throw_if_failed();
  store.undo();
  EXPECT_EQ(static_cast<const model::review_data&>(*store.read()).status, "draft");
  EXPECT_EQ(static_cast<const model::author_data&>(*store.read()).persistent_id, author_id);
  store.redo();
  document_store reopened{model::document_data{}};
  reopened.load(store.save());
  EXPECT_EQ(static_cast<const model::review_data&>(*reopened.read()).persistent_id, review_id);
  EXPECT_EQ(static_cast<const model::review_data&>(*reopened.read()).status, "approved");
}

// Owned alternatives preserve tags and child identities through history and Save/load.
TEST(managed_document_features, managed_variant_switch_restore_and_stale_editor) {
  document_store store{initial_document()};
  const auto text_id = std::get<0>(store.read()->content).persistent_id;
  store.execute_transaction([](auto& tx) {
    auto document = tx.root();
    document.template edit_content<0>().set_text("Edited text");
  }).throw_if_failed();
  store.execute_transaction([](auto& tx) {
    model::attachment_data attachment{};
    attachment.filename = "readme.txt";
    attachment.bytes = {65, 66, 67};
    tx.root().template emplace_content<1>(std::move(attachment));
  }).throw_if_failed();
  EXPECT_EQ(store.read()->content.index(), 1u);
  const auto attachment_id = std::get<1>(store.read()->content).persistent_id;
  EXPECT_GT(attachment_id, text_id);
  store.undo();
  EXPECT_EQ(std::get<0>(store.read()->content).persistent_id, text_id);
  EXPECT_EQ(std::get<0>(store.read()->content).text, "Edited text");
  store.redo();
  document_store reopened{model::document_data{}};
  reopened.load(store.save());
  EXPECT_EQ(std::get<1>(reopened.read()->content).persistent_id, attachment_id);
  const auto failed = store.execute_transaction([](auto& tx) {
    auto previous = tx.root().template edit_content<1>();
    tx.root().template emplace_content<0>(model::text_data{});
    tx.root().template emplace_content<1>(model::attachment_data{});
    EXPECT_THROW(previous.set_filename("stale.txt"), std::out_of_range);
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_EQ(std::get<1>(store.read()->content).filename, "readme.txt");
}

// Base-local IDs remain distinct collaboration addresses under one enclosing identity.
TEST(managed_document_features, ordinary_inherited_field_paths_and_conditional_undo) {
  using root = model::ordinary_bases_document;
  using authority_type = managed::collaboration_authority<root>;
  using accepted_type = authority_type::accepted_change_type;
  const auto round_trip = [](const accepted_type& value) {
    return managed::decode_collaboration_record<accepted_type>(managed::encode_collaboration_record(value));
  };
  authority_type authority{root{}, 1};
  authority.open_session(1);
  authority.open_session(2);
  managed::collaboration_replica<root> first;
  managed::collaboration_replica<root> second;
  first.synchronize(authority.snapshot(), authority.context());
  second.synchronize(authority.snapshot(), authority.context());
  const auto title_change = first.propose(1, 1, [](auto& tx) {
    tx.root().title_part().set_title("Meeting notes");
  });
  const auto accepted_title = authority.submit_change(title_change, 1, 0);
  ASSERT_EQ(accepted_title->status, managed::collaboration_status::accepted);
  const auto delivered_title = round_trip(*accepted_title->accepted);
  first.apply_accepted_change(delivered_title, authority.context());
  second.apply_accepted_change(delivered_title, authority.context());
  const auto language_change = second.propose(2, 1, [](auto& tx) {
    tx.root().language_part().set_language("fr");
  });
  const auto accepted_language = authority.submit_change(language_change, 2, 0);
  ASSERT_EQ(accepted_language->status, managed::collaboration_status::accepted);
  first.apply_accepted_change(round_trip(*accepted_language->accepted), authority.context());
  const auto reversed_title = authority.submit_change(first.undo(1, 2), 1, 0);
  ASSERT_EQ(reversed_title->status, managed::collaboration_status::accepted);
  EXPECT_EQ(static_cast<const model::language_data&>(*authority.read()).language, "fr");
  const auto& record = delivered_title.history.fields;
  ASSERT_EQ(record.size(), 1u);
  EXPECT_EQ(record.front().field, 1u);
  EXPECT_EQ(record.front().path, (std::vector<std::uint32_t>{1}));
}

// A named managed generic specialization binds its own concrete schema and editor types.
TEST(managed_document_features, concrete_generic_specialization) {
  managed::model_store<model::string_annotation> store{model::string_annotation{}};
  store.execute_transaction([](auto& tx) { tx.root().set_note("review this paragraph"); })
      .throw_if_failed();
  EXPECT_EQ(store.read()->note, "review this paragraph");
  store.undo();
  EXPECT_TRUE(store.read()->note.empty());
  store.redo();
  EXPECT_EQ(store.clone_value().note, "review this paragraph");
}

// Ordinary payload lifecycle metadata remains available on generated managed owning classes.
TEST(managed_document_features, managed_version_metadata_and_round_trip) {
  managed::model_store<model::versioned_document> store{model::versioned_document{}};
  store.execute_transaction([](auto& tx) { tx.root().set_title("Version two title"); })
      .throw_if_failed();
  managed::model_store<model::versioned_document> reopened{model::versioned_document{}};
  reopened.load(store.save());
  EXPECT_EQ(reopened.read()->title, "Version two title");
  reopened.undo();
  EXPECT_EQ(reopened.read()->title, "untitled");
}

// Inactive payload fields neither own live identities nor expose writable managed editors.
TEST(managed_document_features, versioned_entity_lifetimes) {
  managed::model_store<model::versioned_entity_document> store{model::versioned_entity_document{}};
  EXPECT_EQ(store.read()->retired_text.persistent_id, 0u);
  EXPECT_NE(store.read()->current_text.persistent_id, 0u);
  std::size_t entities{};
  managed::model_traits<model::versioned_entity_document>::visit_entities(
      *store.read(), [&](auto, auto) { ++entities; });
  EXPECT_EQ(entities, 2u);
  const auto rejected = store.execute_transaction([](auto& tx) {
    EXPECT_THROW(tx.root().retired_text(), std::invalid_argument);
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  store.execute_transaction([](auto& tx) { tx.root().current_text().set_text("Current revision"); })
      .throw_if_failed();
  const auto saved = store.save();
  managed::model_store<model::versioned_entity_document> loaded{model::versioned_entity_document{}};
  loaded.load(saved);
  EXPECT_EQ(loaded.read()->retired_text.persistent_id, 0u);
  EXPECT_EQ(loaded.read()->current_text.persistent_id, store.read()->current_text.persistent_id);
}

// Runtime editors can navigate durable ownership without acquiring durable mutation APIs.
TEST(managed_document_features, nested_runtime_fields_and_variant_payloads) {
  document_store store{initial_document()};
  const auto before = store.save();
  store.update_runtime([](auto& runtime) {
    runtime.root().author().set_lookup_cache(7);
    runtime.root().template edit_content<0>().set_word_cache(11);
  }).throw_if_failed();
  EXPECT_EQ(store.save(), before);
  EXPECT_EQ(static_cast<const model::author_data&>(*store.read()).lookup_cache, 7u);
  EXPECT_EQ(std::get<0>(store.read()->content).word_cache, 11u);
  store.execute_transaction([](auto& tx) { tx.root().set_title("New durable title"); }).throw_if_failed();
  EXPECT_EQ(static_cast<const model::author_data&>(*store.read()).lookup_cache, 0u);
  EXPECT_EQ(std::get<0>(store.read()->content).word_cache, 0u);
}

// Alternative selection obeys the owning field's lifecycle before exposing an editor.
TEST(managed_document_features, versioned_variant_lifetimes) {
  managed::model_store<model::versioned_variant_document> store{model::versioned_variant_document{}};
  EXPECT_EQ(std::get<0>(store.read()->retired_content).persistent_id, 0u);
  EXPECT_NE(std::get<0>(store.read()->current_content).persistent_id, 0u);
  const auto rejected = store.execute_transaction([](auto& tx) {
    EXPECT_THROW((void)tx.root().template edit_retired_content<0>(), std::invalid_argument);
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  store.execute_transaction([](auto& tx) {
    tx.root().template edit_current_content<0>().set_text("Live versioned content");
  }).throw_if_failed();
  EXPECT_EQ(std::get<0>(store.read()->current_content).text, "Live versioned content");
}

// Fixed child arrays retain their extent and identities while permitting checked child edits.
TEST(managed_document_features, fixed_managed_array_navigation) {
  using root = model::fixed_review_document;
  managed::model_store<root> store{root{}};
  const auto first_id = store.read()->paragraphs[0].persistent_id;
  const auto second_id = store.read()->paragraphs[1].persistent_id;
  EXPECT_NE(first_id, second_id);
  store.execute_transaction([&](auto& tx) {
    tx.root().paragraphs().edit(second_id).set_text("Second paragraph");
  }).throw_if_failed();
  EXPECT_EQ(store.read()->paragraphs.size(), 2u);
  EXPECT_EQ(store.read()->paragraphs[1].text, "Second paragraph");
  store.undo();
  EXPECT_EQ(store.read()->paragraphs[1].persistent_id, second_id);
  EXPECT_TRUE(store.read()->paragraphs[1].text.empty());
}

// A managed-capable generic argument remains a value unless its occurrence is managed.
TEST(managed_document_features, managed_capable_generic_argument) {
  using root = model::text_annotation;
  managed::model_store<root> store{root{}};
  store.execute_transaction([](auto& tx) {
    tx.root().note().set_text("An annotated paragraph");
  }).throw_if_failed();
  const auto saved = store.save();
  store.update_runtime([](auto& runtime) {
    runtime.root().note().set_word_cache(31);
  }).throw_if_failed();
  EXPECT_EQ(store.save(), saved);
  EXPECT_EQ(store.clone_value().note.word_cache, 31u);
  store.undo();
  EXPECT_TRUE(store.clone_value().note.text.empty());
}

// Existing flat sessions retain their public aliases/protocols and exclude transient path values.
TEST(managed_document_features, flat_wire_family_remains_unchanged) {
  using records = managed::collaboration_record_types<>;
  static_assert(records::command_protocol == 5 && records::model_protocol == 6);
  static_assert(managed::collaboration_record_types<std::string>::command_protocol == 7);
  static_assert(managed::collaboration_record_types<std::string>::model_protocol == 8);
  static_assert(std::same_as<records::accepted_change,
                            managed::collaboration_records::accepted_change>);
  records::field_change flat;
  flat.entity = 4;
  flat.field = 1;
  flat.before_present = flat.after_present = true;
  flat.before = {10};
  flat.after = {20};
  flat.before_version = 2;
  flat.after_version = 3;
  const auto existing_bytes = managed::encode_collaboration_record(flat);
  flat.path = {11, 12};
  EXPECT_EQ(managed::encode_collaboration_record(flat), existing_bytes);
  const auto decoded = managed::decode_collaboration_record<records::field_change>(existing_bytes);
  EXPECT_TRUE(decoded.path.empty());
  EXPECT_EQ(decoded.entity, 4u);
  EXPECT_EQ(decoded.field, 1u);
  EXPECT_EQ(decoded.before, std::vector<std::uint8_t>{10});
  EXPECT_EQ(decoded.after, std::vector<std::uint8_t>{20});
}

// Typed sessions preserve inherited field scopes in the generated nested accepted record envelope.
TEST(managed_document_features, nested_typed_wire_protocol_and_cache_binding) {
  using root = model::ordinary_bases_document;
  using sessions = managed::collaboration_session<
      std::string, managed::collaboration_session_traits<std::string>, true>;
  using authority_type = sessions::authority<root>;
  static_assert(authority_type::records_type::command_protocol == 11);
  static_assert(authority_type::records_type::model_protocol == 12);
  using accepted_type = authority_type::accepted_change_type;
  const auto round_trip = [](const accepted_type& value) {
    return managed::decode_collaboration_record<accepted_type>(
        managed::encode_collaboration_record(value));
  };
  authority_type authority{root{}, 1};
  authority.open_session("writer");
  const auto original = authority.snapshot().snapshot;
  sessions::replica<root> replica;
  replica.synchronize(round_trip(authority.snapshot()), authority.context());
  sessions::lock_cache cache;
  cache.synchronize(authority.lock_snapshot(), authority.context());
  const auto request = replica.propose("writer", 1, [](auto& tx) {
    tx.root().title_part().set_title("Typed document title");
  });
  const auto accepted = authority.submit_change(request, "writer", 0);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  const auto delivered = round_trip(*accepted->accepted);
  ASSERT_EQ(delivered.history.fields.size(), 1u);
  EXPECT_EQ(delivered.history.fields.front().path, std::vector<std::uint32_t>{1});
  replica.apply_accepted_change(delivered, authority.context());
  EXPECT_EQ(managed::detail::encode(*replica.read()), authority.snapshot().snapshot);
  const auto reversed = authority.submit_change(replica.undo("writer", 2), "writer", 0);
  ASSERT_EQ(reversed->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(round_trip(*reversed->accepted), authority.context());
  EXPECT_EQ(managed::detail::encode(*replica.read()), original);

  auto legacy = authority.context();
  legacy.protocol_version = 8;
  auto untrusted = round_trip(authority.snapshot());
  untrusted.context = legacy;
  EXPECT_THROW(replica.synchronize(untrusted, legacy), std::invalid_argument);
  auto locks = authority.lock_snapshot();
  locks.context = legacy;
  EXPECT_THROW(cache.synchronize(locks, legacy), std::invalid_argument);
  const auto before = authority.snapshot();
  auto wrong_request = request;
  wrong_request.context = legacy;
  EXPECT_THROW(authority.submit_change(wrong_request, "writer", 0), std::invalid_argument);
  EXPECT_EQ(authority.snapshot().sequence, before.sequence);
}

// Editing a child payload changes that child, while changing membership reports owner/birth/deletion IDs.
TEST(managed_document_features, precise_changed_ids_distinguish_membership_and_child_payload) {
  document_store store{initial_document()};
  using id_type = document_store::id_type;
  const auto root_id = store.read()->persistent_id;
  const auto text_id = std::get<0>(store.read()->content).persistent_id;
  const auto author_id = static_cast<const model::author_data&>(*store.read()).persistent_id;
  std::vector<id_type> changed, invalidated;
  bool precise{};
  store.set_publication_callback([&](const auto& event) {
    changed = event.changed_ids;
    invalidated = event.invalidated_ids;
    precise = event.changed_ids_precise;
    EXPECT_EQ(event.after, store.read());
  });
  store.execute_transaction([](auto& tx) {
    tx.root().template edit_content<0>().set_text("Revised paragraph");
  }).throw_if_failed();
  EXPECT_EQ(changed, (std::vector<id_type>{text_id}));
  EXPECT_TRUE(precise);
  EXPECT_GT(invalidated.size(), changed.size());
  store.execute_transaction([](auto& tx) { tx.root().author().set_name("Ada"); }).throw_if_failed();
  EXPECT_EQ(changed, (std::vector<id_type>{author_id}));
  store.undo();
  EXPECT_EQ(changed, (std::vector<id_type>{author_id}));
  store.redo();
  EXPECT_EQ(changed, (std::vector<id_type>{author_id}));
  store.execute_transaction([](auto& tx) {
    model::attachment_data attachment{};
    attachment.filename = "note.txt";
    tx.root().template emplace_content<1>(std::move(attachment));
  }).throw_if_failed();
  const auto attachment_id = std::get<1>(store.read()->content).persistent_id;
  std::vector<id_type> expected{root_id, text_id, attachment_id};
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(changed, expected);
  EXPECT_TRUE(precise);
  store.undo();
  EXPECT_EQ(changed, expected);
}

// Equal leaf keys in ordinary bases remain separate full paths and identify their enclosing entity.
TEST(managed_document_features, precise_changed_ids_compare_scoped_ordinary_base_fields) {
  using root = model::ordinary_bases_document;
  using store_type = managed::model_store<root>;
  store_type store{root{}};
  const auto root_id = store.read()->persistent_id;
  std::vector<store_type::id_type> changed;
  store.set_publication_callback([&](const auto& event) {
    EXPECT_TRUE(event.changed_ids_precise);
    changed = event.changed_ids;
  });
  store.execute_transaction([](auto& tx) {
    tx.root().title_part().set_title("Minutes");
    tx.root().language_part().set_language("fr");
  }).throw_if_failed();
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
  store.undo();
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
}

// Qualified base types and keyword wire names use stable-ID accessors when aliases are not C++ identifiers.
TEST(managed_document_features, qualified_base_and_wire_name_accessors) {
  using root = model::qualified_bases_document;
  managed::model_store<root> store{root{}};
  const auto contributor = static_cast<const model::attribution::contributor_data&>(*store.read()).persistent_id;
  store.execute_transaction([&](auto& tx) {
    EXPECT_EQ(tx.root().base_1().id(), contributor);
    tx.root().base_1().set_label("Document contributor");
    tx.root().base_2().set_status("reviewed");
    tx.root().base_3().set_name("Ada");
  }).throw_if_failed();
  EXPECT_EQ(static_cast<const model::attribution::contributor_data&>(*store.read()).label, "Document contributor");
  store.undo();
  EXPECT_EQ(static_cast<const model::review_data&>(*store.read()).status, "draft");
}

namespace {
// Runtime receivers must not acquire any durable behavior mutation entry point.
template <typename Editor>
concept metrics_edit_receiver = requires(Editor& editor) {
  editor.set_metrics(12.5, 2);
  editor.add_pages(1);
  editor.serializer_require_behavior_definitions();
};
} // namespace

// Native and externally defined behavior share one candidate and never execute during restoration.
TEST(managed_document_features, schema_edit_behavior_is_atomic_and_transaction_bound) {
  using store_type = managed::model_store<model::document_metrics>;
  store_type store{model::document_metrics{}};
  EXPECT_DOUBLE_EQ(store.read()->estimated_words(), 10.0);
  const auto initial_cursor = store.history_cursor();
  store.execute_transaction([](auto& tx) {
    auto editor = tx.root();
    static_assert(metrics_edit_receiver<decltype(editor)>);
    editor.serializer_require_behavior_definitions();
    editor.set_metrics(12.5, 2);
    editor.add_pages(1);
  }).throw_if_failed();
  EXPECT_EQ(store.history_cursor(), initial_cursor + 1);
  EXPECT_DOUBLE_EQ(store.read()->estimated_words(), 37.5);
  const auto accepted = store.save();
  const auto failed = store.execute_transaction([](auto& tx) {
    EXPECT_THROW(tx.root().set_metrics(99.0, 0), std::invalid_argument);
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), accepted);
  store.undo();
  EXPECT_DOUBLE_EQ(store.read()->estimated_words(), 10.0);
  store.redo();
  EXPECT_DOUBLE_EQ(store.read()->estimated_words(), 37.5);
  store.update_runtime([](auto& runtime) {
    static_assert(!metrics_edit_receiver<decltype(runtime.root())>);
    runtime.root().set_lookup_cache(73);
  }).throw_if_failed();
  EXPECT_EQ(store.save(), accepted);
  typename store_type::outcome_type outcome;
  auto transaction = store.begin_transaction(outcome);
  auto escaped = transaction.root();
  transaction.revert();
  EXPECT_THROW(escaped.set_metrics(1.0, 1), std::logic_error);
  EXPECT_THROW(escaped.add_pages(1), std::logic_error);
}

namespace {
// Baseline bindings independently reconstructed from the pre-metadata FNV field traversal.
struct legacy_omitted_traits : managed::model_traits<model::legacy_omitted_document> {
  static constexpr std::string_view schema_id =
      "managed.direct.v1:managed_document_features::legacy_omitted_document:8152964830775220772:uint32";
};
struct legacy_child_traits : managed::model_traits<model::legacy_versioned_child_document> {
  static constexpr std::string_view schema_id =
      "managed.direct.v1:managed_document_features::legacy_versioned_child_document:3597778639276977091:uint32";
};

// A same-model converter explicitly authorizes the old binding without weakening strict load.
template <typename Root, typename LegacyTraits, typename Change, typename Read>
void qualify_legacy_binding_migration(Change change, Read read) {
  using old_store = managed::model_store<Root, managed::history_mode::linear,
                                       managed::history_labels::disabled, LegacyTraits>;
  old_store previous{Root{}, {201, 202}};
  const auto root_id = previous.read()->persistent_id;
  for (std::uint32_t index = 1; index != 4; ++index) {
    previous.execute_transaction([&](auto& tx) {
      static_cast<void>(tx.create_id()); // Consumed IDs remain consumed after migration.
      change(tx.root(), index);
    }).throw_if_failed();
  }
  previous.undo();
  const auto old_save = previous.save();
  const auto current_bytes = managed::detail::encode(*previous.read());
  managed::model_store<Root> target{Root{}, {203, 204}};
  const auto untouched = target.read();
  EXPECT_THROW(target.load(old_save), std::invalid_argument);
  EXPECT_EQ(target.read(), untouched);
  std::vector<std::uint32_t> converted;
  target.template load_migrated<Root, LegacyTraits>(old_save, [&](const Root& value) {
    EXPECT_EQ(value.persistent_id, root_id);
    converted.push_back(read(value));
    return value;
  });
  std::sort(converted.begin(), converted.end());
  EXPECT_EQ(converted, (std::vector<std::uint32_t>{0, 1, 2, 3}));
  EXPECT_EQ(target.history_cursor(), previous.history_cursor());
  EXPECT_EQ(target.allocated_id(), previous.allocated_id());
  EXPECT_EQ(managed::detail::encode(*target.read()), current_bytes);
  EXPECT_EQ(target.read()->persistent_id, root_id);
  target.undo();
  EXPECT_EQ(read(*target.read()), 1u);
  target.undo();
  EXPECT_EQ(read(*target.read()), 0u);
  EXPECT_THROW(target.undo(), std::out_of_range);
  for (std::uint32_t index = 1; index != 4; ++index) {
    target.redo();
    EXPECT_EQ(read(*target.read()), index);
    EXPECT_EQ(target.read()->persistent_id, root_id);
  }
  EXPECT_THROW(target.redo(), std::out_of_range);
}
} // namespace

// An existing JSON-only omission changes its binding, not its managed positional snapshot bytes.
TEST(managed_document_features, legacy_json_omission_binding_requires_explicit_migration) {
  qualify_legacy_binding_migration<model::legacy_omitted_document, legacy_omitted_traits>(
      [](auto editor, std::uint32_t next) { editor.set_count(next); },
      [](const auto& value) { return value.count; });
}

// Ordinary versioned children also acquire metadata-bound hashes; every retained state must convert.
TEST(managed_document_features, legacy_versioned_child_binding_requires_explicit_migration) {
  qualify_legacy_binding_migration<model::legacy_versioned_child_document, legacy_child_traits>(
      [](auto editor, std::uint32_t next) { editor.data().set_title(std::to_string(next)); },
      [](const auto& value) {
        return value.data.title.empty() ? std::uint32_t{} :
            static_cast<std::uint32_t>(std::stoul(value.data.title));
      });
}

// Fixed-inline dotted revisions work with owning/runtime estimates and finite admission budgets.
TEST(managed_document_features, dotted_revision_slots_memory_budget_and_history) {
  using root = model::dotted_version_document;
  using traits = managed::model_traits<root>;
  using store_type = managed::model_store<root>;
  EXPECT_EQ(managed::detail::estimate_dynamic_memory(rohit::serializer::version2{1, 0}), 0u);
  EXPECT_EQ(managed::detail::estimate_dynamic_memory(rohit::serializer::version3{2, 0, 0}), 0u);
  EXPECT_EQ(managed::detail::estimate_dynamic_memory(rohit::serializer::version4{1, 2, 3, 4}), 0u);
  managed::store_options limits;
  limits.max_resident_bytes = 1024 * 1024;
  store_type store{root{}, {211, 212}, limits};
  EXPECT_EQ(store.read()->schema_version, (rohit::serializer::version3{2, 0, 0}));
  const auto memory = traits::estimate_memory(*store.read());
  EXPECT_GE(memory, sizeof(root));
  EXPECT_LE(store.resident_bytes(), limits.max_resident_bytes);
  const auto saved = store.save();
  store.update_runtime([](auto& runtime) {
    runtime.root().set_lookup_cache(std::vector<std::uint8_t>(4096, 7));
  }).throw_if_failed();
  EXPECT_EQ(store.save(), saved);
  EXPECT_GE(traits::estimate_memory(*store.read()), memory + 4096);
  const auto rejected = store.execute_transaction([](auto& tx) {
    EXPECT_THROW(tx.root().get_old_title(), std::invalid_argument);
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  store.execute_transaction([](auto& tx) {
    auto document = tx.root();
    EXPECT_EQ(document.get_schema_version(), (rohit::serializer::version3{2, 0, 0}));
    static_assert(!requires { document.set_schema_version(rohit::serializer::version3{1, 0, 0}); });
    document.set_title("Dotted revision document");
  }).throw_if_failed();
  EXPECT_TRUE(store.read()->lookup_cache.empty());
  store_type reopened{root{}, {213, 214}, limits};
  reopened.load(store.save());
  EXPECT_EQ(reopened.read()->schema_version, (rohit::serializer::version3{2, 0, 0}));
  EXPECT_EQ(reopened.read()->title, "Dotted revision document");
  reopened.undo();
  EXPECT_EQ(reopened.read()->title, "untitled");
  reopened.redo();
  EXPECT_EQ(reopened.read()->title, "Dotted revision document");
}
