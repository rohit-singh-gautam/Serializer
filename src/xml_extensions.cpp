// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_extensions.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::xml {
namespace {

// Admit before arithmetic so caller-selected maximum bounds cannot wrap counters.
void add_bytes(std::size_t& used, std::size_t amount, std::size_t maximum) {
  if (used > maximum || amount > maximum - used) {
    throw std::length_error{"XML extension resource limit exceeded"};
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
    add_bytes(bytes, sizeof(std::pair<const std::string, std::string>), maximum);
    add_bytes(bytes, prefix.size(), maximum);
    add_bytes(bytes, uri.size(), maximum);
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
  add_bytes(bytes, sizeof(attribute), maximum);
  add_bytes(bytes, value.qualified_name.local.size(), maximum);
  add_bytes(bytes, value.qualified_name.namespace_uri.size(), maximum);
  add_bytes(bytes, value.value.size(), maximum);
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
  add_bytes(bytes, sizeof(element), maximum);
  add_bytes(bytes, value.qualified_name.local.size(), maximum);
  add_bytes(bytes, value.qualified_name.namespace_uri.size(), maximum);
  add_bytes(bytes, value.text.size(), maximum);
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
    add_bytes(bytes, sizeof(element::text_segment), maximum);
    add_bytes(bytes, segment.value.size(), maximum);
  }
  if ((!value.text_segments.empty() && text_offset != value.text.size()) ||
      (!value.children.empty() && !value.text.empty() && value.text_segments.empty())) {
    throw std::invalid_argument{"Incomplete preserved XML mixed-content order"};
  }
  for (const auto& child : value.children) { measure_child(child, budget, bytes, maximum, context); }
}

} // namespace

// Exact namespace lists make slot overlap statically decidable and keep core names closed.
void validate_extension_contract(const extension_contract& contract, extension_limits bounds) {
  if (contract.slots.size() > bounds.max_slots || contract.known_names.size() > bounds.max_items ||
      contract.required_namespaces.size() > bounds.max_items) {
    throw std::length_error{"XML extension contract metadata limit exceeded"};
  }
  std::set<std::string> ids{};
  std::set<std::pair<bool, std::pair<std::size_t, std::string>>> destinations{};
  std::size_t metadata_bytes{};
  for (const auto& known : contract.known_names) {
    add_bytes(metadata_bytes, known.local.size(), bounds.document.max_name_bytes);
    add_bytes(metadata_bytes, known.namespace_uri.size(), bounds.document.max_name_bytes);
  }
  for (const auto& slot : contract.slots) {
    add_bytes(metadata_bytes, slot.id.size(), bounds.document.max_name_bytes);
    if (slot.id.empty() || !ids.insert(slot.id).second || slot.namespaces.empty() ||
        slot.namespaces.size() > bounds.max_items ||
        (slot.maximum && slot.minimum > *slot.maximum) ||
        (slot.policy == unknown_policy::reject && slot.minimum != 0U)) {
      throw std::invalid_argument{"Invalid XML extension slot contract"};
    }
    if (slot.policy != unknown_policy::reject && slot.policy != unknown_policy::skip &&
        slot.policy != unknown_policy::preserve) {
      throw std::invalid_argument{"Unknown XML extension policy"};
    }
    for (const auto& uri : slot.namespaces) {
      add_bytes(metadata_bytes, uri.size(), bounds.document.max_name_bytes);
      if (uri.empty() || uri == contract.core_namespace ||
          !destinations.emplace(slot.attribute,
              std::pair{slot.attribute ? 0U : slot.before_field, uri}).second) {
        throw std::invalid_argument{"Ambiguous or core XML extension namespace slot"};
      }
    }
  }
  std::set<std::string> required{};
  for (const auto& uri : contract.required_namespaces) {
    add_bytes(metadata_bytes, uri.size(), bounds.document.max_name_bytes);
    if (uri.empty() || !required.insert(uri).second) {
      throw std::invalid_argument{"Invalid required XML capability namespace"};
    }
  }
}

// Preservation and skipping never imply semantic support for required vocabularies.
void validate_required_capabilities(const extension_contract& contract, const element& scope,
    std::string_view prefixes, std::span<const std::string> understood_namespaces,
    extension_limits bounds, operation_context* context) {
  if (prefixes.size() > bounds.document.max_text_bytes ||
      contract.required_namespaces.size() > bounds.max_items ||
      understood_namespaces.size() > bounds.max_items) {
    throw std::length_error{"XML required capability metadata limit exceeded"};
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
    add_bytes(prefix_count, 1U, bounds.max_items);
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
    add_bytes(resolved_bytes, uri.size(), bounds.document.max_name_bytes);
    if (!understands(contract, uri, understood_namespaces, context)) {
      throw std::invalid_argument{"Required XML capability is not understood"};
    }
    prefixes = end == std::string_view::npos ? std::string_view{} : detail::trim(prefixes.substr(end));
  }
}

// Keep candidate storage local so any late occurrence/capability failure publishes nothing.
extension_store collect_extension_slots(const extension_contract& contract, const element& scope,
    std::span<const extension_item_view> items,
    std::span<const std::string> understood_namespaces, std::string_view required_prefixes,
    extension_limits bounds, operation_context* context) {
  if (context) { context->check(); }
  validate_extension_contract(contract, bounds);
  if (scope.qualified_name.namespace_uri != contract.core_namespace) {
    throw std::invalid_argument{"XML extension owner namespace differs from contract"};
  }
  if (items.size() > bounds.max_items) {
    throw std::length_error{"XML extension item limit exceeded"};
  }
  validate_required_capabilities(contract, scope, required_prefixes, understood_namespaces, bounds, context);
  if (context) {
    context->consume(operation_resource::allocation_bytes,
        contract.slots.size() * sizeof(std::size_t));
  }
  std::vector<std::size_t> counts(contract.slots.size());
  extension_store candidate{};
  build_budget budget{bounds.document};
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
    if (selected_name.namespace_uri.empty() || selected_name.namespace_uri == contract.core_namespace ||
        std::find(contract.known_names.begin(), contract.known_names.end(), selected_name) !=
            contract.known_names.end()) {
      throw std::invalid_argument{"Known or core XML name cannot enter an extension slot"};
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
          (!slot.attribute && slot.before_field != item.before_field)) { continue; }
      for (const auto& uri : slot.namespaces) {
        if (context) {
          context->consume(operation_resource::work_units, 1U);
          context->consume(operation_resource::work_units,
              std::min(uri.size(), selected_name.namespace_uri.size()));
        }
        if (uri == selected_name.namespace_uri) { selected = index; break; }
      }
      if (selected != contract.slots.size()) { break; }
    }
    if (selected == contract.slots.size() ||
        contract.slots[selected].policy == unknown_policy::reject) {
      throw std::invalid_argument{"XML extension has no permitted slot"};
    }
    const auto& slot = contract.slots[selected];
    add_bytes(counts[selected], 1U,
        slot.maximum.value_or(std::numeric_limits<std::size_t>::max()));
    if (slot.policy == unknown_policy::skip) { continue; }
    const auto before = preserved_bytes;
    if (item.attribute) {
      measure_attribute(*item.attribute, budget, preserved_bytes, bounds.max_preserved_bytes);
      if (!scope_retained) {
        measure_scope(scope.namespace_bindings, budget, preserved_bytes, bounds.max_preserved_bytes);
      }
    } else {
      measure_child(*item.child, budget, preserved_bytes, bounds.max_preserved_bytes, context);
      constexpr auto slot_metadata_bytes = sizeof(extension_store::child_slot) - sizeof(element);
      budget.retained_storage(slot_metadata_bytes);
      add_bytes(preserved_bytes, slot_metadata_bytes, bounds.max_preserved_bytes);
    }
    if (context) {
      context->consume(operation_resource::preserved_bytes, preserved_bytes - before);
      context->consume(operation_resource::allocation_bytes, preserved_bytes - before);
    }
    if (item.attribute) {
      if (!scope_retained) { candidate.namespace_bindings = scope.namespace_bindings; scope_retained = true; }
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

} // namespace rohit::serializer::xml
