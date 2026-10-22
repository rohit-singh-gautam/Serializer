// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_extensions.hpp>
#include "xml_extension_occurrence.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::xml {
namespace {

// Admit before arithmetic so caller-selected maximum bounds cannot wrap counters.
void add_bytes(std::size_t& used, std::size_t amount, std::size_t maximum, operation_context* context = nullptr) {
  if (used > maximum || amount > maximum - used) {
    reject_limit(context, "XML extension resource limit exceeded", amount, maximum - std::min(used, maximum));
  }
  used += amount;
}

// Test vocabulary support by expanded URI identity rather than prefix spelling.
bool understands(const extension_contract& contract, std::string_view uri,
    std::span<const std::string> capabilities, operation_context* context) {
  if (uri == contract.core_namespace) { return true; }
  for (const auto& capability : capabilities) {
    if (context) {
      context->consume(operation_resource::work_units, 1U);
      context->consume(operation_resource::work_units, std::min(uri.size(), capability.size()));
    }
    if (capability == uri) { return true; }
  }
  return false;
}

// Measure one namespace scope before copying map nodes or their borrowed strings.
void measure_scope(const std::map<std::string, std::string>& bindings, build_budget& budget,
    std::size_t& bytes, std::size_t maximum) {
  for (const auto& [prefix, uri] : bindings) {
    budget.namespace_binding(prefix, uri);
    add_bytes(bytes, sizeof(std::pair<const std::string, std::string>), maximum, budget.context());
    add_bytes(bytes, prefix.size(), maximum, budget.context());
    add_bytes(bytes, uri.size(), maximum, budget.context());
  }
}

// Measure attribute payload without charging an opaque value as a second XML node.
void measure_attribute(const attribute& value, build_budget& budget,
    std::size_t& bytes, std::size_t maximum) {
  if (value.qualified_name.local.empty()) {
    throw std::invalid_argument{"XML extension attribute requires a local name"};
  }
  budget.attribute_name(value.qualified_name);
  budget.text(value.value.size());
  add_bytes(bytes, sizeof(attribute), maximum, budget.context());
  add_bytes(bytes, value.qualified_name.local.size(), maximum, budget.context());
  add_bytes(bytes, value.qualified_name.namespace_uri.size(), maximum, budget.context());
  add_bytes(bytes, value.value.size(), maximum, budget.context());
}

// Recursion is depth-bounded before visiting children; retained text order is checked exactly.
void measure_child(const element& value, build_budget& budget,
    std::size_t& bytes, std::size_t maximum, operation_context* context) {
  if (context) { context->consume(operation_resource::work_units, 1U); }
  if (value.qualified_name.local.empty()) {
    throw std::invalid_argument{"XML extension child requires a local name"};
  }
  const auto depth = budget.enter(value.qualified_name);
  budget.text(value.text.size());
  add_bytes(bytes, sizeof(element), maximum, budget.context());
  add_bytes(bytes, value.qualified_name.local.size(), maximum, budget.context());
  add_bytes(bytes, value.qualified_name.namespace_uri.size(), maximum, budget.context());
  add_bytes(bytes, value.text.size(), maximum, budget.context());
  measure_scope(value.namespace_bindings, budget, bytes, maximum);
  for (const auto& item : value.attributes) { measure_attribute(item, budget, bytes, maximum); }
  std::size_t previous_child{}, text_offset{};
  for (const auto& segment : value.text_segments) {
    if (segment.before_child < previous_child || segment.before_child > value.children.size() ||
        text_offset > value.text.size() ||
        !std::string_view{value.text}.substr(text_offset).starts_with(segment.value)) {
      throw std::invalid_argument{"Invalid preserved XML mixed-content order"};
    }
    previous_child = segment.before_child;
    text_offset += segment.value.size();
    budget.retained_storage(sizeof(element::text_segment));
    budget.retained_storage(segment.value.size());
    add_bytes(bytes, sizeof(element::text_segment), maximum, budget.context());
    add_bytes(bytes, segment.value.size(), maximum, budget.context());
  }
  if ((!value.text_segments.empty() && text_offset != value.text.size()) ||
      (!value.children.empty() && !value.text.empty() && value.text_segments.empty())) {
    throw std::invalid_argument{"Incomplete preserved XML mixed-content order"};
  }
  for (const auto& child : value.children) { measure_child(child, budget, bytes, maximum, context); }
}

} // namespace

// The finite set/complement representation keeps namespace matching and overlap decidable.
bool matches_extension_namespace(const extension_slot_contract& slot, std::string_view namespace_uri) {
  const bool contains = std::find(slot.namespaces.begin(), slot.namespaces.end(), namespace_uri) != slot.namespaces.end();
  switch (slot.namespace_match) {
  case extension_namespace_match::any: return true;
  case extension_namespace_match::except:
  case extension_namespace_match::foreign_fallback: return !contains;
  case extension_namespace_match::explicit_foreign:
  case extension_namespace_match::one_of: return contains;
  }
  throw std::invalid_argument{"Unknown XML extension namespace predicate"};
}

// Preserve foreign-only authored slots while checking generated namespace predicates for overlap.
void validate_extension_contract_at(const extension_contract& contract, extension_limits bounds, operation_context* context) {
  if (contract.slots.size() > bounds.max_slots || contract.known_names.size() > bounds.max_items ||
      contract.known_attributes.size() > bounds.max_items ||
      contract.required_namespaces.size() > bounds.max_items) {
    reject_limit(context, "XML extension contract metadata limit exceeded");
  }
  if (!contract.known_positions.empty() && contract.known_positions.size() != contract.known_names.size()) {
    throw std::invalid_argument{"Invalid XML known-child position metadata"};
  }
  std::set<std::string> ids{};
  std::size_t metadata_bytes{};
  for (const auto& known : contract.known_names) {
    add_bytes(metadata_bytes, known.local.size(), bounds.document.max_name_bytes, context);
    add_bytes(metadata_bytes, known.namespace_uri.size(), bounds.document.max_name_bytes, context);
  }
  for (const auto& known : contract.known_attributes) {
    add_bytes(metadata_bytes, known.local.size(), bounds.document.max_name_bytes, context);
    add_bytes(metadata_bytes, known.namespace_uri.size(), bounds.document.max_name_bytes, context);
  }
  for (const auto& slot : contract.slots) {
    add_bytes(metadata_bytes, slot.id.size(), bounds.document.max_name_bytes, context);
    const bool finite = slot.namespace_match == extension_namespace_match::explicit_foreign ||
        slot.namespace_match == extension_namespace_match::one_of;
    if (slot.id.empty() || !ids.insert(slot.id).second || (finite && slot.namespaces.empty()) ||
        slot.namespaces.size() > bounds.max_items ||
        (slot.maximum && slot.minimum > *slot.maximum) ||
        (slot.policy == unknown_policy::reject && slot.minimum != 0U)) {
      throw std::invalid_argument{"Invalid XML extension slot contract"};
    }
    if (slot.policy != unknown_policy::reject && slot.policy != unknown_policy::skip &&
        slot.policy != unknown_policy::preserve) {
      throw std::invalid_argument{"Unknown XML extension policy"};
    }
    if ((slot.namespace_match != extension_namespace_match::explicit_foreign &&
        slot.namespace_match != extension_namespace_match::any && slot.namespace_match != extension_namespace_match::one_of &&
        slot.namespace_match != extension_namespace_match::except && slot.namespace_match != extension_namespace_match::foreign_fallback) ||
        (slot.process_contents != wildcard_process_contents::skip && slot.process_contents != wildcard_process_contents::lax &&
        slot.process_contents != wildcard_process_contents::strict)) {
      throw std::invalid_argument{"Unknown XML wildcard predicate or process mode"};
    }
    std::set<std::string> local_namespaces;
    for (const auto& uri : slot.namespaces) {
      add_bytes(metadata_bytes, uri.size(), bounds.document.max_name_bytes, context);
      if (!local_namespaces.insert(uri).second || (slot.namespace_match == extension_namespace_match::explicit_foreign &&
          (uri.empty() || uri == contract.core_namespace))) {
        throw std::invalid_argument{"Ambiguous or core XML extension namespace slot"};
      }
    }
    if (slot.namespace_match == extension_namespace_match::foreign_fallback &&
        (!local_namespaces.contains("") || !local_namespaces.contains(contract.core_namespace))) {
      throw std::invalid_argument{"XML foreign fallback must exclude absent and core namespaces"};
    }
    for (const auto& previous : contract.slots) {
      if (&previous == &slot) { break; }
      const auto any_position = [](const auto& selected) {
        return selected.namespace_match == extension_namespace_match::foreign_fallback &&
            selected.before_field == std::numeric_limits<std::size_t>::max();
      };
      if (previous.attribute != slot.attribute || (!slot.attribute && previous.before_field != slot.before_field &&
          !any_position(previous) && !any_position(slot))) { continue; }
      const bool previous_finite = previous.namespace_match == extension_namespace_match::explicit_foreign ||
          previous.namespace_match == extension_namespace_match::one_of;
      const bool overlap = finite ? std::any_of(slot.namespaces.begin(), slot.namespaces.end(),
          [&](const auto& uri) { return matches_extension_namespace(previous, uri); }) :
          previous_finite ? std::any_of(previous.namespaces.begin(), previous.namespaces.end(),
              [&](const auto& uri) { return matches_extension_namespace(slot, uri); }) : true;
      if (overlap) { throw std::invalid_argument{"Ambiguous XML extension namespace predicates"}; }
    }
  }
  std::set<std::string> required{};
  for (const auto& uri : contract.required_namespaces) {
    add_bytes(metadata_bytes, uri.size(), bounds.document.max_name_bytes, context);
    if (uri.empty() || !required.insert(uri).second) {
      throw std::invalid_argument{"Invalid required XML capability namespace"};
    }
  }
}

// Preserve the original context-free public contract-validation entry point.
void validate_extension_contract(const extension_contract& contract, extension_limits bounds) {
  validate_extension_contract_at(contract, bounds, nullptr);
}

// Preservation and skipping never imply semantic support for required vocabularies.
void validate_required_capabilities(const extension_contract& contract, const element& scope,
    std::string_view prefixes, std::span<const std::string> understood_namespaces,
    extension_limits bounds, operation_context* context) {
  if (prefixes.size() > bounds.document.max_text_bytes ||
      contract.required_namespaces.size() > bounds.max_items ||
      understood_namespaces.size() > bounds.max_items) {
    reject_limit(context, "XML required capability metadata limit exceeded");
  }
  for (const auto& uri : contract.required_namespaces) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (!understands(contract, uri, understood_namespaces, context)) {
      throw std::invalid_argument{"Required XML capability is not understood: " + uri};
    }
  }
  std::size_t prefix_count{}, resolved_bytes{};
  prefixes = detail::trim(prefixes);
  while (!prefixes.empty()) {
    const auto end = prefixes.find_first_of(" \t\r\n");
    const auto prefix = prefixes.substr(0U, end);
    add_bytes(prefix_count, 1U, bounds.max_items, context);
    if (prefix.size() > bounds.document.max_name_bytes || prefix.find(':') != std::string_view::npos ||
        prefix == "xmlns") {
      throw std::invalid_argument{"Invalid required capability prefix"};
    }
    if (context) {
      context->consume(operation_resource::work_units, prefix.size());
      context->consume(operation_resource::allocation_bytes, prefix.size());
    }
    const auto found = scope.namespace_bindings.find(std::string{prefix});
    const std::string_view uri = prefix == "xml" ?
        "http://www.w3.org/XML/1998/namespace" : found == scope.namespace_bindings.end() ?
        std::string_view{} : found->second;
    if (uri.empty()) { throw std::invalid_argument{"Unbound required XML capability prefix"}; }
    add_bytes(resolved_bytes, uri.size(), bounds.document.max_name_bytes, context);
    if (!understands(contract, uri, understood_namespaces, context)) {
      throw std::invalid_argument{"Required XML capability is not understood"};
    }
    prefixes = end == std::string_view::npos ? std::string_view{} : detail::trim(prefixes.substr(end));
  }
}

// Keep candidate storage local so any late occurrence/capability failure publishes nothing.
// Declaration validation is orthogonal to namespace admission and opaque retention.
void validate_wildcard_declaration(const extension_contract& contract,
    const extension_slot_contract& slot, const element& scope, const attribute* attribute_value,
    const element* child, limits bounds, operation_context* context) {
  if (slot.process_contents == wildcard_process_contents::skip) { return; }
  if (context) { context->consume(operation_resource::work_units, 1U); }
  if ((attribute_value == nullptr) == (child == nullptr)) {
    throw std::invalid_argument{"A validating XML wildcard requires exactly one payload"};
  }
  const bool declared = contract.validate_declaration &&
      contract.validate_declaration(scope, attribute_value, child, bounds, context);
  if (!declared && slot.process_contents == wildcard_process_contents::strict) {
    throw std::invalid_argument{"Strict XML wildcard has no declaration in the pinned schema bundle"};
  }
}

static extension_store inspect_extension_slots(const extension_contract& contract, const element& scope,
    std::span<const extension_item_view> items,
    std::span<const std::string> understood_namespaces, std::string_view required_prefixes,
    extension_limits bounds, operation_context* context, bool retain) {
  if (context) { context->check(); }
  validate_extension_contract_at(contract, bounds, context);
  if (scope.qualified_name.namespace_uri != contract.core_namespace) {
    throw std::invalid_argument{"XML extension owner namespace differs from contract"};
  }
  if (items.size() > bounds.max_items) {
    reject_limit(context, "XML extension item limit exceeded");
  }
  validate_required_capabilities(contract, scope, required_prefixes, understood_namespaces, bounds, context);
  if (context) {
    context->consume(operation_resource::allocation_bytes,
        contract.slots.size() * sizeof(std::size_t));
  }
  std::vector<std::size_t> counts(contract.slots.size());
  extension_store candidate{};
  operation_context local_bounds{};
  if (context && context->limit_location) {
    local_bounds.limit_location = [context](std::size_t requested, std::size_t available,
        std::string_view message, const diagnostic_field*) { context->reject_limit(requested, available, message); };
  }
  build_budget budget{bounds.document, local_bounds.limit_location ? &local_bounds : nullptr};
  std::size_t preserved_bytes{}, previous_child{}, previous_field{};
  bool has_child{}, scope_retained{};
  std::set<std::pair<std::string, std::string>> attribute_names{};
  for (const auto& item : items) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if ((item.attribute == nullptr) == (item.child == nullptr)) {
      throw std::invalid_argument{"XML extension item requires exactly one payload"};
    }
    const auto& selected_name = item.attribute ? item.attribute->qualified_name :
        item.child->qualified_name;
    if (context) { context->consume(operation_resource::work_units, contract.known_names.size()); }
    const auto& known_names = item.attribute && contract.distinct_known_kinds ? contract.known_attributes : contract.known_names;
    const auto known = std::find(known_names.begin(), known_names.end(), selected_name);
    if (selected_name.namespace_uri == model_metadata_namespace) {
      throw std::invalid_argument{"Known, core or reserved XML name cannot enter an extension slot"};
    }
    if (item.child) {
      if (has_child && (item.before_child < previous_child || item.before_field < previous_field)) {
        throw std::invalid_argument{"XML extension positions are unordered"};
      }
      has_child = true;
      previous_child = item.before_child;
      previous_field = item.before_field;
    } else if (!attribute_names.emplace(selected_name.namespace_uri, selected_name.local).second) {
      throw std::invalid_argument{"Duplicate XML extension attribute"};
    }
    std::size_t selected = contract.slots.size();
    for (std::size_t index{}; index < contract.slots.size(); ++index) {
      const auto& slot = contract.slots[index];
      if (context) { context->consume(operation_resource::work_units, 1U); }
      if (slot.attribute != (item.attribute != nullptr) ||
          (!slot.attribute && slot.before_field != item.before_field &&
              !(slot.namespace_match == extension_namespace_match::foreign_fallback && slot.before_field == std::numeric_limits<std::size_t>::max()))) { continue; }
      if (known != known_names.end()) {
        if (slot.namespace_match == extension_namespace_match::explicit_foreign || slot.namespace_match == extension_namespace_match::foreign_fallback || item.attribute) { continue; }
        const auto known_index = static_cast<std::size_t>(known - known_names.begin());
        if (!contract.known_positions.empty() && contract.known_positions[known_index] >= item.before_field &&
            counts[index] >= slot.minimum) { continue; }
      }
      for (const auto& uri : slot.namespaces) {
        if (context) {
          context->consume(operation_resource::work_units, 1U);
          context->consume(operation_resource::work_units,
              std::min(uri.size(), selected_name.namespace_uri.size()));
        }
      }
      if (matches_extension_namespace(slot, selected_name.namespace_uri)) { selected = index; }
      if (selected != contract.slots.size()) { break; }
    }
    if (selected == contract.slots.size() ||
        contract.slots[selected].policy == unknown_policy::reject) {
      throw std::invalid_argument{"XML extension has no permitted slot"};
    }
    const auto& slot = contract.slots[selected];
    detail::validate_wildcard_occurrence(slot, counts[selected]);
    add_bytes(counts[selected], 1U,
        slot.maximum.value_or(std::numeric_limits<std::size_t>::max()), context);
    validate_wildcard_declaration(contract, slot, scope, item.attribute, item.child,
        bounds.document, context);
    if (slot.policy == unknown_policy::skip || (context && context->declaration_validation)) { continue; }
    const auto before = preserved_bytes;
    const bool copy_scope = item.attribute && !scope_retained;
    if (item.attribute) {
      measure_attribute(*item.attribute, budget, preserved_bytes, bounds.max_preserved_bytes);
      if (!scope_retained) {
        measure_scope(scope.namespace_bindings, budget, preserved_bytes, bounds.max_preserved_bytes);
      }
    } else {
      measure_child(*item.child, budget, preserved_bytes, bounds.max_preserved_bytes, context);
      constexpr auto slot_metadata_bytes = sizeof(extension_store::child_slot) - sizeof(element);
      budget.retained_storage(slot_metadata_bytes);
      add_bytes(preserved_bytes, slot_metadata_bytes, bounds.max_preserved_bytes, context);
    }
    if (context) {
      context->consume(operation_resource::preserved_bytes, preserved_bytes - before);
      if (retain) { context->consume(operation_resource::allocation_bytes, preserved_bytes - before); }
    }
    if (item.attribute) { scope_retained = true; }
    if (!retain) { continue; }
    if (item.attribute) {
      if (copy_scope) { candidate.namespace_bindings = scope.namespace_bindings; }
      candidate.attributes.push_back(*item.attribute);
    } else {
      candidate.children.push_back({item.before_child, *item.child, item.before_field});
    }
  }
  for (std::size_t index{}; index < counts.size(); ++index) {
    if (counts[index] < contract.slots[index].minimum) {
      throw std::invalid_argument{"Required XML extension slot occurrence missing"};
    }
  }
  if (context) { context->check(); }
  return candidate;
}

// Build an owning candidate only after each payload's logical preservation storage is admitted.
extension_store collect_extension_slots(const extension_contract& contract, const element& scope,
    std::span<const extension_item_view> items, std::span<const std::string> understood_namespaces,
    std::string_view required_prefixes, extension_limits bounds, operation_context* context) {
  return inspect_extension_slots(contract, scope, items, understood_namespaces, required_prefixes, bounds, context, true);
}

// Reuse the same slot/capability/occurrence checks while borrowing existing owning payloads.
void validate_extension_slots(const extension_contract& contract, const element& scope,
    std::span<const extension_item_view> items, std::span<const std::string> understood_namespaces,
    extension_limits bounds, operation_context* context) {
  (void)inspect_extension_slots(contract, scope, items, understood_namespaces, {}, bounds, context, false);
}

// Build only bounded borrowed views and the original attribute scope before validating retained data.
void validate_tree_extension_store(const extension_contract& contract, const name& owner,
    const extension_store& store, operation_context* context) {
  auto bounds = context && context->extension_bounds ? *context->extension_bounds : extension_limits{};
  if (context && !context->extension_bounds && context->document_bounds) { bounds.document = *context->document_bounds; }
  if (store.attributes.size() > bounds.max_items || store.children.size() > bounds.max_items - store.attributes.size()) {
    reject_limit(context, "XML extension item limit exceeded");
  }
  const auto count = store.attributes.size() + store.children.size();
  if (count > std::numeric_limits<std::size_t>::max() / sizeof(extension_item_view)) {
    reject_limit(context, "XML extension traversal metadata overflow");
  }
  if (context) {
    context->consume(operation_resource::allocation_bytes, count * sizeof(extension_item_view));
    context->consume(operation_resource::allocation_bytes, owner.local.size() + owner.namespace_uri.size());
  }
  std::vector<extension_item_view> items;
  items.reserve(count);
  for (const auto& item : store.attributes) { items.push_back({&item}); }
  for (const auto& item : store.children) { items.push_back({nullptr, &item.value, item.before_field, item.before_child}); }
  element scope{};
  scope.qualified_name = owner;
  for (const auto& [prefix, uri] : store.namespace_bindings) {
    if (context) { context->consume(operation_resource::allocation_bytes,
        sizeof(std::pair<const std::string, std::string>) + prefix.size() + uri.size()); }
    scope.namespace_bindings.emplace(prefix, uri);
  }
  validate_extension_slots(contract, scope, items, context && context->understood_namespaces ?
      std::span<const std::string>{*context->understood_namespaces} : std::span<const std::string>{}, bounds, context);
}

} // namespace rohit::serializer::xml
