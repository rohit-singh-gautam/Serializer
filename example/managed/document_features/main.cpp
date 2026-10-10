// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <document.hpp>
#include "behavior.hpp"
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace model = managed_document_features;
namespace managed = rohit::managed;

// Keep the example's checks active in Release builds as well as Debug builds.
void require(bool condition, const char* message) {
  if (!condition) { throw std::runtime_error{message}; }
}

// Demonstrate independent base identities, private fields, owning alternatives and memory-only state.
void edit_document() {
  using store_type = managed::model_store<model::document_data>;
  model::document_data value{};
  value.title = "Team notes";
  std::get<0>(value.content).text = "Agenda";
  store_type store{std::move(value)};
  const auto author = static_cast<const model::author_data&>(*store.read()).persistent_id;
  require(author != store.read()->persistent_id, "Each managed base needs its own identity");
  store.execute_transaction([](auto& tx) {
    tx.root().author().set_name("Ada"); // Private schema field, checked public editor.
    tx.root().review().set_status("approved");
    tx.root().template edit_content<0>().set_text("Approved agenda");
  }).throw_if_failed();
  const auto before_runtime = store.save();
  store.update_runtime([](auto& edit) {
    edit.root().set_preview_cache(51);
    edit.root().author().set_lookup_cache(73);
  }).throw_if_failed();
  require(store.save() == before_runtime, "Memory-only updates must not change saved bytes");
  const auto copied = store.clone_value();
  require(copied.preview_cache == 51, "Ordinary copies preserve memory fields");
  const auto reset = store.clone_value({managed::runtime_copy_policy::reset_to_defaults});
  require(reset.preview_cache == 0, "Explicit reset cloning clears memory fields");
  store.execute_transaction([](auto& tx) {
    model::attachment_data file{};
    file.filename = "minutes.txt";
    file.bytes = {65, 66, 67};
    tx.root().template emplace_content<1>(std::move(file));
  }).throw_if_failed();
  const auto attachment = std::get<1>(store.read()->content).persistent_id;
  store.undo();
  require(store.read()->content.index() == 0, "Undo restores the earlier alternative");
  store.redo();
  require(std::get<1>(store.read()->content).persistent_id == attachment,
          "Redo restores the same child identity");
  require(store.read()->preview_cache == 0, "Durable publication resets runtime state");
  store_type loaded{model::document_data{}};
  loaded.load(store.save());
  require(static_cast<const model::author_data&>(*loaded.read()).persistent_id == author,
          "Save/load preserves managed base identities");
}

// Distinct ordinary base scopes share a root ID without aliasing equal local field IDs.
void edit_inherited_fields() {
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
  const auto proposal = first.propose(1, 1, [](auto& tx) {
    tx.root().title_part().set_title("Meeting notes");
  });
  const auto accepted = authority.submit_change(proposal, 1, 0);
  require(accepted->status == managed::collaboration_status::accepted, "Title edit must be accepted");
  const auto delivered = round_trip(*accepted->accepted);
  require(delivered.history.fields.front().path == std::vector<std::uint32_t>{1},
          "Encoded collaboration retains ordinary-base scope");
  first.apply_accepted_change(delivered, authority.context());
  second.apply_accepted_change(delivered, authority.context());
  const auto language = second.propose(2, 1, [](auto& tx) {
    tx.root().language_part().set_language("fr");
  });
  const auto changed = authority.submit_change(language, 2, 0);
  require(changed->status == managed::collaboration_status::accepted, "Language edit must be accepted");
  first.apply_accepted_change(round_trip(*changed->accepted), authority.context());
  const auto undone = authority.submit_change(first.undo(1, 2), 1, 0);
  require(undone->status == managed::collaboration_status::accepted, "Selective undo must succeed");
  require(static_cast<const model::language_data&>(*authority.read()).language == "fr",
          "Undo must preserve the unrelated base edit");
}

// Named generic specialization and ordinary payload lifecycle metadata bind managed codecs/history.
void edit_specialized_and_versioned_records() {
  managed::model_store<model::string_annotation> annotations{model::string_annotation{}};
  annotations.execute_transaction([](auto& tx) { tx.root().set_note("Review paragraph two"); })
      .throw_if_failed();
  require(annotations.read()->note == "Review paragraph two", "Concrete generic editor must work");
  managed::model_store<model::versioned_document> document{model::versioned_document{}};
  document.execute_transaction([](auto& tx) { tx.root().set_title("Revision two document"); })
      .throw_if_failed();
  managed::model_store<model::versioned_document> loaded{model::versioned_document{}};
  loaded.load(document.save());
  require(loaded.read()->title == "Revision two document", "Managed lifecycle codecs must round-trip");
  loaded.undo();
  require(loaded.read()->title == "untitled", "Undo restores the versioned payload");
}
// Exercise fixed membership, nested generic arguments and active managed payload fields.
void edit_fixed_and_versioned_children() {
  managed::model_store<model::fixed_review_document> fixed{model::fixed_review_document{}};
  const auto child = fixed.read()->paragraphs[1].persistent_id;
  fixed.execute_transaction([&](auto& tx) {
    tx.root().paragraphs().edit(child).set_text("Fixed review paragraph");
  }).throw_if_failed();
  require(fixed.read()->paragraphs.size() == 2, "Fixed child cardinality never changes");
  fixed.undo();
  require(fixed.read()->paragraphs[1].persistent_id == child, "Undo preserves fixed child identity");

  managed::model_store<model::text_annotation> notes{model::text_annotation{}};
  notes.execute_transaction([](auto& tx) { tx.root().note().set_text("Nested generic paragraph"); })
      .throw_if_failed();
  require(notes.clone_value().note.text == "Nested generic paragraph", "Managed generic arguments bind editors");

  managed::model_store<model::versioned_entity_document> entities{model::versioned_entity_document{}};
  require(entities.read()->retired_text.persistent_id == 0, "Retired fields do not own live identities");
  require(entities.read()->current_text.persistent_id != 0, "Active fields own live identities");
  const auto rejected_child = entities.execute_transaction([](auto& tx) { tx.root().retired_text(); });
  require(rejected_child.status == managed::transaction_status::failed, "Retired child editors reject access");

  managed::model_store<model::versioned_variant_document> choices{model::versioned_variant_document{}};
  const auto rejected_variant = choices.execute_transaction([](auto& tx) {
    tx.root().template edit_retired_content<0>();
  });
  require(rejected_variant.status == managed::transaction_status::failed, "Retired alternative editors reject access");
  choices.execute_transaction([](auto& tx) {
    tx.root().template edit_current_content<0>().set_text("Current alternative");
  }).throw_if_failed();
  require(std::get<0>(choices.read()->current_content).text == "Current alternative", "Active alternative edits succeed");
}
// Execute schema-native and application-defined edit methods on the same live receiver.
void edit_schema_behavior() {
  managed::model_store<model::document_metrics> store{model::document_metrics{}};
  store.execute_transaction([](auto& tx) {
    auto document = tx.root();
    document.serializer_require_behavior_definitions();
    document.set_metrics(12.5, 2);
    document.add_pages(1);
  }).throw_if_failed();
  require(store.read()->estimated_words() == 37.5, "Readonly portable behavior reads accepted data");
  store.undo();
  require(store.read()->estimated_words() == 10.0, "Undo restores values without executing behavior");
  store.redo();
  require(store.read()->estimated_words() == 37.5, "Redo restores the complete behavior transaction");
}
} // namespace

// Execute the generated document model examples and report validation failures to the caller.
int main() {
  try {
    edit_document();
    edit_inherited_fields();
    edit_specialized_and_versioned_records();
    edit_fixed_and_versioned_children();
    edit_schema_behavior();
    std::cout << "Managed document features verified\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
