// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_extensions.hpp>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;

// Reserve one foreign child slot after one known field.
xml::extension_contract child_contract(xml::unknown_policy policy) {
  return {.core_namespace = "urn:core", .known_names = {{"known", "urn:foreign"}},
      .slots = {{"foreign_children", false, 1U, {"urn:foreign"}, policy, 0U, 2U}}};
}

// Keep full lexical namespace context on the owner and each preserved subtree.
xml::element owner() {
  xml::element result{};
  result.qualified_name = {"root", "urn:core"};
  result.namespace_bindings = {{"f", "urn:foreign"}, {"q", "urn:outer"}};
  return result;
}

// Build an opaque text/child/text subtree with a child-local shadowed QName prefix.
xml::element foreign_child() {
  xml::element result{};
  result.qualified_name = {"extra", "urn:foreign"};
  result.namespace_bindings = owner().namespace_bindings;
  result.attributes.push_back({{"kind", ""}, "q:Outer"});
  result.text = "beforeafter";
  result.text_segments = {{0U, "before"}, {1U, "after"}};
  xml::element nested{};
  nested.qualified_name = {"nested", "urn:foreign"};
  nested.namespace_bindings = {{"f", "urn:foreign"}, {"q", "urn:inner"}};
  nested.attributes.push_back({{"kind", ""}, "q:Inner"});
  result.children.push_back(std::move(nested));
  return result;
}
} // namespace

// Policies cannot forgive unknown core names or misplaced known foreign fields.
TEST(xml_extension_slot, rejects_core_known_unpermitted_and_wrong_positions) {
  const auto scope = owner();
  auto child = foreign_child();
  const auto contract = child_contract(xml::unknown_policy::skip);
  std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  EXPECT_NO_THROW((void)xml::collect_extension_slots(contract, scope, items));
  items.front().before_field = 0U;
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items), std::invalid_argument);
  items.front().before_field = 1U;
  for (const auto& name : {xml::name{"extra", "urn:core"}, xml::name{"known", "urn:foreign"},
      xml::name{"extra", "urn:unpermitted"}, xml::name{"extra", ""}}) {
    child.qualified_name = name;
    EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items), std::invalid_argument);
  }
}

// Explicit foreign permissions cannot capture or replay Serializer's reserved model metadata vocabulary.
TEST(xml_extension_slot, rejects_reserved_model_names_in_all_slot_policies) {
  const auto scope = owner();
  auto child = foreign_child();
  child.qualified_name = {"future", std::string{xml::model_metadata_namespace}};
  const xml::attribute attribute{{"future", std::string{xml::model_metadata_namespace}}, "1"};
  const std::array children{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  const std::array attributes{xml::extension_item_view{&attribute, nullptr, 0U, 0U}};
  for (const auto policy : {xml::unknown_policy::reject, xml::unknown_policy::skip, xml::unknown_policy::preserve}) {
    auto contract = child_contract(policy);
    contract.slots.front().namespaces = {std::string{xml::model_metadata_namespace}};
    EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, children), std::invalid_argument);
    EXPECT_THROW(xml::validate_extension_slots(contract, scope, children), std::invalid_argument);
    contract.slots.front().attribute = true;
    EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, attributes), std::invalid_argument);
    EXPECT_THROW(xml::validate_extension_slots(contract, scope, attributes), std::invalid_argument);
  }
}

// Reject never admits a value; skip counts occurrences without retaining payload or bindings.
TEST(xml_extension_slot, reject_skip_and_exact_occurrence_contracts) {
  const auto scope = owner();
  const auto child = foreign_child();
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  EXPECT_THROW((void)xml::collect_extension_slots(child_contract(xml::unknown_policy::reject),
      scope, items), std::invalid_argument);
  auto contract = child_contract(xml::unknown_policy::skip);
  contract.slots.front().minimum = 1U;
  contract.slots.front().maximum = 1U;
  const auto skipped = xml::collect_extension_slots(contract, scope, items);
  EXPECT_TRUE(skipped.children.empty());
  EXPECT_TRUE(skipped.namespace_bindings.empty());
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, {}), std::invalid_argument);
  const std::array repeated{items.front(), items.front()};
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, repeated), std::length_error);
  contract.slots.front().namespace_match = xml::extension_namespace_match::one_of;
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, repeated), std::invalid_argument);
}

// processContents validates declared payloads before either discarding or publishing opaque state.
TEST(xml_extension_slot, declaration_validation_is_independent_of_retention) {
  auto scope = owner();
  auto child = foreign_child();
  child.children.clear(); child.attributes.clear(); child.text_segments.clear(); child.text = "q:Value";
  child.namespace_bindings["q"] = "urn:allowed";
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  for (const auto policy : {xml::unknown_policy::skip, xml::unknown_policy::preserve}) {
    auto contract = child_contract(policy);
    contract.validate_declaration = [](const xml::element&, const xml::attribute* attribute,
        const xml::element* element, xml::limits, xml::operation_context* context) {
      if (context) { context->consume(xml::operation_resource::work_units, 1U); }
      if (attribute || element->qualified_name.local != "extra") { return false; }
      if (xml::resolve_qname(element->text, *element).namespace_uri != "urn:allowed") {
        throw std::invalid_argument{"Declared QName does not satisfy its restriction"};
      }
      return true;
    };
    for (const auto process : {xml::wildcard_process_contents::lax, xml::wildcard_process_contents::strict}) {
      contract.slots.front().process_contents = process;
      const auto retained = xml::collect_extension_slots(contract, scope, items);
      EXPECT_EQ(retained.children.empty(), policy == xml::unknown_policy::skip);
      child.namespace_bindings["q"] = "urn:shadowed";
      EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items), std::invalid_argument);
      child.namespace_bindings["q"] = "urn:allowed";
      child.qualified_name.local = "undeclared";
      if (process == xml::wildcard_process_contents::strict) {
        EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items), std::invalid_argument);
      } else {
        EXPECT_NO_THROW((void)xml::collect_extension_slots(contract, scope, items));
      }
      child.qualified_name.local = "extra";
    }
    contract.slots.front().process_contents = xml::wildcard_process_contents::skip;
    child.namespace_bindings["q"] = "urn:shadowed";
    EXPECT_NO_THROW((void)xml::collect_extension_slots(contract, scope, items));
    child.namespace_bindings["q"] = "urn:allowed";
    contract.slots.front().process_contents = xml::wildcard_process_contents::strict;
    xml::operation_context context{};
    context.charge = [](xml::operation_resource resource, std::size_t amount) {
      if (resource == xml::operation_resource::work_units && amount == 1U) {
        throw std::runtime_error{"Original caller interruption"};
      }
    };
    EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, {}, {}, {}, &context), std::runtime_error);
  }
}

// Independent owning copies preserve exact mixed order and lexical scope through serialization.
TEST(xml_extension_slot, preserves_subtree_order_and_shadowed_namespace_context) {
  const auto scope = owner();
  auto child = foreign_child();
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  const auto retained = xml::collect_extension_slots(child_contract(xml::unknown_policy::preserve),
      scope, items);
  ASSERT_EQ(retained.children.size(), 1U);
  child.text = "changed";
  const auto& saved = retained.children.front().value;
  EXPECT_EQ(saved.text, "beforeafter");
  ASSERT_EQ(saved.text_segments.size(), 2U);
  EXPECT_EQ(saved.text_segments[1].before_child, 1U);
  EXPECT_EQ(xml::resolve_qname(saved.attributes.front().value, saved), (xml::name{"Outer", "urn:outer"}));
  EXPECT_EQ(xml::resolve_qname(saved.children.front().attributes.front().value,
      saved.children.front()), (xml::name{"Inner", "urn:inner"}));
  if (xml::available()) {
    xml::operation_context mixed{};
    mixed.allow_mixed_content = true;
    const auto round_trip = xml::parse(xml::write(saved), {}, &mixed);
    EXPECT_EQ(round_trip.text, "beforeafter");
    EXPECT_EQ(xml::resolve_qname(round_trip.children.front().attributes.front().value,
        round_trip.children.front()), (xml::name{"Inner", "urn:inner"}));
  }
}

// Attributes retain their owner scope, reject duplicates and never overwrite known data.
TEST(xml_extension_slot, preserves_attribute_scope_and_rejects_duplicate_attributes) {
  const auto scope = owner();
  const xml::attribute value{{"tag", "urn:foreign"}, "q:Outer"};
  auto contract = child_contract(xml::unknown_policy::preserve);
  contract.slots.front().attribute = true;
  const std::array items{xml::extension_item_view{&value}};
  const auto retained = xml::collect_extension_slots(contract, scope, items);
  ASSERT_EQ(retained.attributes.size(), 1U);
  EXPECT_EQ(retained.namespace_bindings.at("q"), "urn:outer");
  const std::array duplicates{items.front(), items.front()};
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, duplicates), std::invalid_argument);
}

// Required vocabularies need explicit understanding even when optional opaque data is retained.
TEST(xml_extension_slot, required_capabilities_resolve_prefixes_and_never_follow_preserve_policy) {
  const auto scope = owner();
  auto contract = child_contract(xml::unknown_policy::preserve);
  const auto child = foreign_child();
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, {}, "f"), std::invalid_argument);
  const std::vector<std::string> supported{"urn:foreign"};
  EXPECT_NO_THROW((void)xml::collect_extension_slots(contract, scope, items, supported, "f"));
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, supported, "missing"), std::invalid_argument);
  contract.required_namespaces = {"urn:outer"};
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, supported), std::invalid_argument);
  const std::vector<std::string> all{"urn:foreign", "urn:outer"};
  EXPECT_NO_THROW((void)xml::collect_extension_slots(contract, scope, items, all));
}

// Reject overlapping positions and malformed payload inventories before publication.
TEST(xml_extension_slot, contract_and_payload_ambiguities_reject) {
  const auto scope = owner();
  auto contract = child_contract(xml::unknown_policy::preserve);
  auto duplicate = contract.slots.front();
  duplicate.id = "other";
  contract.slots.push_back(duplicate);
  EXPECT_THROW(xml::validate_extension_contract(contract), std::invalid_argument);
  contract.slots.back().before_field = 2U;
  EXPECT_NO_THROW(xml::validate_extension_contract(contract));
  contract.slots.back().namespaces = {"urn:core"};
  EXPECT_THROW(xml::validate_extension_contract(contract), std::invalid_argument);
  const std::array invalid{xml::extension_item_view{}};
  EXPECT_THROW((void)xml::collect_extension_slots(child_contract(xml::unknown_policy::preserve),
      scope, invalid), std::invalid_argument);
}

// A failing preservation budget or callback leaves an existing destination untouched.
TEST(xml_extension_slot, bounds_and_context_fail_transactionally) {
  const auto scope = owner();
  const auto child = foreign_child();
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  const auto contract = child_contract(xml::unknown_policy::preserve);
  xml::extension_limits bounds{};
  bounds.max_preserved_bytes = 1U;
  xml::extension_store destination{};
  destination.attributes.push_back({{"old", "urn:old"}, "old"});
  EXPECT_THROW(destination = xml::collect_extension_slots(contract, scope, items, {}, {}, bounds), std::length_error);
  ASSERT_EQ(destination.attributes.size(), 1U);
  EXPECT_EQ(destination.attributes.front().value, "old");
  bounds = {};
  bounds.document.max_depth = 1U;
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, {}, {}, bounds), std::length_error);
  xml::operation_context context{};
  context.is_cancelled = [] { return true; };
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, {}, {}, {}, &context), std::runtime_error);
}

// Output validation borrows opaque payloads instead of allocating a second preserved subtree.
TEST(xml_extension_slot, validates_owned_payloads_without_clone_allocation) {
  const auto scope = owner();
  const auto child = foreign_child();
  const std::array items{xml::extension_item_view{nullptr, &child, 1U, 1U}};
  const auto contract = child_contract(xml::unknown_policy::preserve);
  std::size_t allocation_remaining = contract.slots.size() * sizeof(std::size_t);
  xml::operation_context context{};
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    if (resource == xml::operation_resource::allocation_bytes) {
      if (amount > allocation_remaining) { throw std::length_error{"clone allocation is forbidden"}; }
      allocation_remaining -= amount;
    }
  };
  EXPECT_NO_THROW(xml::validate_extension_slots(contract, scope, items, {}, {}, &context));
  EXPECT_EQ(child.text, "beforeafter");
  allocation_remaining = contract.slots.size() * sizeof(std::size_t);
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, items, {}, {}, {}, &context), std::length_error);
  xml::extension_limits bounds{}; bounds.max_preserved_bytes = 0U;
  EXPECT_THROW(xml::validate_extension_slots(contract, scope, items, {}, bounds), std::length_error);
}

// Required-vocabulary comparisons consume the operation's CPU ledger before scanning further.
TEST(xml_extension_slot, required_capability_search_respects_work_budget) {
  const auto scope = owner();
  auto contract = child_contract(xml::unknown_policy::preserve);
  contract.required_namespaces = {"urn:foreign"};
  const std::vector<std::string> capabilities{"urn:one", "urn:two", "urn:foreign"};
  std::size_t remaining_work_units{3U};
  xml::operation_context context{};
  // Interrupt vocabulary comparisons when their cumulative work admission exhausts the budget.
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    if (resource == xml::operation_resource::work_units) {
      if (amount > remaining_work_units) { throw std::length_error{"work limit"}; }
      remaining_work_units -= amount;
    }
  };
  EXPECT_THROW((void)xml::collect_extension_slots(contract, scope, {}, capabilities, {}, {},
      &context), std::length_error);
  xml::extension_limits no_slots{};
  no_slots.max_slots = 0U;
  EXPECT_THROW(xml::validate_extension_contract(contract, no_slots), std::length_error);
}
