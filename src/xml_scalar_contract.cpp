// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_scalar_contract.hpp"

#include "xml_schema.hpp"

#include <rohit/xml_lexical_contract.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
namespace {
// Translate authored whitespace once; unknown policy values must never fall back silently.
xml::whitespace_policy whitespace(std::string_view value) {
  if (value.empty() || value == "preserve") { return xml::whitespace_policy::preserve; }
  if (value == "replace") { return xml::whitespace_policy::replace; }
  if (value == "collapse") { return xml::whitespace_policy::collapse; }
  throw std::invalid_argument{"Unsupported compiled XSD whitespace policy"};
}

// Borrow immutable bound strings without converting exact decimal limits to floating point.
xml::scalar_constraints constraints(const scalar_facets& facets) {
  xml::scalar_constraints result{};
  if (facets.min_inclusive) { result.lexical_min_inclusive = *facets.min_inclusive; }
  if (facets.max_inclusive) { result.lexical_max_inclusive = *facets.max_inclusive; }
  if (facets.min_exclusive) { result.lexical_min_exclusive = *facets.min_exclusive; }
  if (facets.max_exclusive) { result.lexical_max_exclusive = *facets.max_exclusive; }
  result.length = facets.length;
  result.min_length = facets.min_length;
  result.max_length = facets.max_length;
  result.finite = facets.finite;
  result.list_length = !facets.list_item_kind.empty();
  result.whitespace = whitespace(facets.whitespace);
  result.lexical_kind = facets.lexical_kind;
  return result;
}

// Check scalar bounds and lengths independently of any example payload or native storage width.
void validate_static_facets(const scalar_facets& facets) {
  if ((facets.min_length && facets.max_length && *facets.min_length > *facets.max_length) ||
      (facets.length && facets.min_length && *facets.length < *facets.min_length) ||
      (facets.length && facets.max_length && *facets.length > *facets.max_length)) {
    throw std::invalid_argument{"Inconsistent compiled XSD length facets"};
  }
  const bool floating = facets.lexical_kind == "float" || facets.lexical_kind == "double";
  const auto validate_bound = [&](const std::optional<std::string>& value) {
    if (value) {
      xml::validate_lexical_value(*value, facets.lexical_kind.empty() ? "decimal" : facets.lexical_kind, {});
      if (!floating) { (void)xml::compare_numeric_values(*value, *value); }
    }
  };
  validate_bound(facets.min_inclusive);
  validate_bound(facets.max_inclusive);
  validate_bound(facets.min_exclusive);
  validate_bound(facets.max_exclusive);
  const auto compare_floating = []<typename T>(std::string_view left, std::string_view right) {
    T first{}, second{};
    xml::detail::read_scalar(left, first);
    xml::detail::read_scalar(right, second);
    if (std::isnan(first) && std::isnan(second)) { return 0; }
    if (std::isnan(first) || std::isnan(second)) {
      throw std::invalid_argument{"Incomparable combined XSD numeric bounds"};
    }
    return first < second ? -1 : first > second ? 1 : 0;
  };
  const auto compare = [&](std::string_view left, std::string_view right) {
    if (facets.lexical_kind == "float") { return compare_floating.operator()<float>(left, right); }
    if (facets.lexical_kind == "double") { return compare_floating.operator()<double>(left, right); }
    return xml::compare_numeric_values(left, right);
  };
  const std::pair<const std::optional<std::string>*, bool> lowers[]{
      {&facets.min_inclusive, false}, {&facets.min_exclusive, true}};
  const std::pair<const std::optional<std::string>*, bool> uppers[]{
      {&facets.max_inclusive, false}, {&facets.max_exclusive, true}};
  for (const auto& [lower, exclusive_lower] : lowers) {
    if (!*lower) { continue; }
    // A single NaN or exclusive infinity facet can legally describe an empty value
    // space. Preserve it and let instance validation implement XSD 1.0 ordering.
    for (const auto& [upper, exclusive_upper] : uppers) {
      if (!*upper) { continue; }
      const auto order = compare(**lower, **upper);
      if (order > 0 || (order == 0 && (exclusive_lower || exclusive_upper))) {
        throw std::invalid_argument{"Inconsistent compiled XSD numeric bounds"};
      }
    }
  }
  (void)constraints(facets);
}

// Compare primitive value families independently of authored union aliases and integer widths.
std::string_view scalar_value_family(std::string_view kind) {
  if (kind == "decimal" || kind == "integer" || kind == "positiveInteger" || kind == "negativeInteger" ||
      kind == "nonPositiveInteger" || kind == "nonNegativeInteger" || kind == "byte" || kind == "unsignedByte" ||
      kind == "short" || kind == "unsignedShort" || kind == "int" || kind == "unsignedInt" ||
      kind == "long" || kind == "unsignedLong") { return "decimal"; }
  return kind == "boolean" || kind == "float" || kind == "double" || kind == "anyURI" || kind == "QName" ? kind : "string";
}

// Compare normalized scalar/list identities while retaining exact decimal precision and item order.
bool scalar_identity_equal(const scalar_facets& first, std::string_view left,
    const scalar_facets& second, std::string_view right, const xml::element& scope,
    xml::operation_context* context) {
  if (context) {
    context->consume(xml::operation_resource::work_units, left.size());
    context->consume(xml::operation_resource::work_units, right.size() + 1U);
  }
  if (first.list_item_kind.empty() != second.list_item_kind.empty()) { return false; }
  const auto kind = first.list_item_kind.empty() ? first.lexical_kind : first.list_item_kind;
  const auto other = second.list_item_kind.empty() ? second.lexical_kind : second.list_item_kind;
  if (scalar_value_family(kind) != scalar_value_family(other)) { return false; }
  if (!first.list_item_kind.empty()) { return xml::lexical_lists_equal(left, right, kind, scope); }
  if (scalar_value_family(kind) == "decimal") { return xml::compare_numeric_values(left, right) == 0; }
  if (scalar_value_family(kind) == "string" || kind == "anyURI") { return left == right; }
  return xml::lexical_lists_equal(left, right, kind, scope);
}
// Compare immutable selected identities recursively, independently of the original list item declarations.
bool prepared_identity_equal(const scalar_value_identity& first, const scalar_value_identity& second,
    const xml::element& scope, xml::operation_context* context) {
  if (context) { context->consume(xml::operation_resource::work_units, 1U); }
  if (first.list_item_kind.empty() != second.list_item_kind.empty()) { return false; }
  if (!first.list_item_kind.empty()) {
    return first.items.size() == second.items.size() && std::equal(first.items.begin(), first.items.end(), second.items.begin(),
        [&](const auto& left, const auto& right) { return prepared_identity_equal(left, right, scope, context); });
  }
  if (scalar_value_family(first.lexical_kind) != scalar_value_family(second.lexical_kind)) { return false; }
  if (first.lexical_kind == "QName") { return first.names == second.names; }
  scalar_facets left{}, right{};
  left.lexical_kind = first.lexical_kind; right.lexical_kind = second.lexical_kind;
  return scalar_identity_equal(left, first.lexical_value, right, second.lexical_value, scope, context);
}
} // namespace

struct compiled_scalar_contract::implementation {
  scalar_facets facets{};
  xml::pattern_limits limits{};
  std::vector<std::vector<xml::portable_pattern>> patterns{};
  std::vector<std::vector<std::shared_ptr<const xml::prepared_pattern>>> provider_patterns{};
  std::vector<compiled_scalar_contract> branches{};
  std::vector<compiled_scalar_contract> item_contracts{};
  bool portable{true};
};

// Share complete prepared metadata across copied compiler contracts.
compiled_scalar_contract::compiled_scalar_contract(std::shared_ptr<const implementation> implementation)
    : implementation_{std::move(implementation)} {}

// Make provider fallback visible to every backend selecting a portable lowering path.
bool compiled_scalar_contract::patterns_portable() const noexcept { return implementation_->portable; }

// Preserve only immutable value identity; source prefixes and provider trees are not retained.
scalar_value_identity compiled_scalar_contract::identify(std::string_view text, const xml::element& scope,
    xml::operation_context* context) const {
  const scalar_facets* leaf{};
  scalar_value_identity result{};
  validate_selected(text, scope, context, &leaf, &result.lexical_value);
  result.lexical_kind = leaf->lexical_kind;
  result.list_item_kind = leaf->list_item_kind;
  if (leaf->lexical_kind == "QName") { result.names.push_back(xml::resolve_qname(result.lexical_value, scope)); }
  else if (leaf->list_item_kind == "QName") {
    auto remaining = std::string_view{result.lexical_value};
    while (!(remaining = xml::detail::trim(remaining)).empty()) {
      const auto size = remaining.find_first_of(" \t\r\n");
      const auto token = remaining.substr(0U, size);
      result.names.push_back(xml::resolve_qname(token, scope));
      remaining.remove_prefix(token.size());
    }
  }
  if (!leaf->list_item_kind.empty()) {
    scalar_facets item_facets{};
    item_facets.lexical_kind = leaf->list_item_kind;
    item_facets.whitespace = "collapse";
    const auto item = compile_scalar_contract(leaf->list_item_facets.empty() ? item_facets : leaf->list_item_facets.front(),
        false, implementation_->limits);
    auto remaining = std::string_view{result.lexical_value};
    while (!(remaining = xml::detail::trim(remaining)).empty()) {
      const auto end = remaining.find_first_of(" \t\r\n");
      const auto token = remaining.substr(0U, end);
      if (context) { context->consume(xml::operation_resource::allocation_bytes, sizeof(scalar_value_identity)); }
      result.items.push_back(item.identify(token, scope, context));
      remaining.remove_prefix(token.size());
    }
  }
  return result;
}

// Build the borrowed lexical metadata on the stack; all referenced strings remain owned by this IR.
void compiled_scalar_contract::validate(std::string_view text, const xml::element& scope,
    xml::operation_context* context) const {
  validate_selected(text, scope, context, nullptr, nullptr);
}

// Return leaf identity only after every inherited restriction has validated the complete value.
void compiled_scalar_contract::validate_selected(std::string_view text, const xml::element& scope,
    xml::operation_context* context, const scalar_facets** selected_leaf, std::string* selected_lexical) const {
  const auto& stored = *implementation_;
  const auto& facets = stored.facets;
  std::vector<std::string_view> enumeration{};
  if (context) {
    context->consume(xml::operation_resource::allocation_bytes, facets.enumeration.size() * sizeof(std::string_view));
    context->consume(xml::operation_resource::work_units, facets.enumeration.size());
  }
  enumeration.reserve(facets.enumeration.size());
  for (const auto& value : facets.enumeration) { enumeration.emplace_back(value); }
  xml::lexical_contract contract{};
  contract.kind = facets.lexical_kind;
  contract.constraints = constraints(facets);
  contract.list_item_kind = facets.list_item_kind;
  contract.enumeration = enumeration;
  contract.qname_enumeration = facets.qname_enumeration;
  if (facets.lexical_kind == "QName" || !facets.qname_list_enumeration.empty()) {
    contract.enumeration = {};
  }
  std::string normalized{};
  if (stored.branches.empty() && contract.constraints.whitespace != xml::whitespace_policy::preserve) {
    if (context) { context->consume(xml::operation_resource::allocation_bytes, text.size()); }
    normalized = xml::normalize_whitespace(text, contract.constraints.whitespace);
    text = normalized;
  }
  if (!stored.item_contracts.empty()) {
    auto remaining = text;
    while (!(remaining = xml::detail::trim(remaining)).empty()) {
      const auto end = remaining.find_first_of(" \t\r\n");
      const auto token = remaining.substr(0U, end);
      stored.item_contracts.front().validate(token, scope, context);
      remaining.remove_prefix(token.size());
    }
    if (!facets.union_enumeration.empty()) { contract.enumeration = {}; }
  }
  // Existing lexical union validation preserves callback exceptions instead of hiding them as mismatches.
  const scalar_facets* leaf = &facets;
  if (!stored.branches.empty()) {
    std::exception_ptr callback_failure{};
    std::optional<xml::operation_context> guarded{};
    auto* selected = context;
    if (context && (context->charge || context->is_cancelled)) {
      guarded.emplace(*context);
      if (context->charge) {
        guarded->charge = [&](const auto resource, const auto amount) {
          try { context->charge(resource, amount); }
          catch (...) { callback_failure = std::current_exception(); throw; }
        };
      }
      if (context->is_cancelled) {
        guarded->is_cancelled = [&] {
          try { return context->is_cancelled(); }
          catch (...) { callback_failure = std::current_exception(); throw; }
        };
      }
      selected = &*guarded;
    }
    bool matched{};
    for (const auto& branch : stored.branches) {
      try { branch.validate_selected(text, scope, selected, &leaf, &normalized); matched = true; break; }
      catch (const std::invalid_argument&) {
        if (callback_failure) { std::rethrow_exception(callback_failure); }
      }
    }
    if (!matched) { throw std::invalid_argument{"XML value matches no compiled union member"}; }
    text = normalized;
    contract.constraints.whitespace = xml::whitespace_policy::preserve;
    contract.enumeration = {};
    if (!facets.enumeration.empty()) {
      const auto constant_matches = [&](const scalar_value_identity& expected) {
        if (!leaf->list_item_kind.empty()) {
          const auto candidate = compile_scalar_contract(*leaf, false, stored.limits).identify(text, scope, context);
          return prepared_identity_equal(candidate, expected, scope, context);
        }
        scalar_facets identity{};
        identity.lexical_kind = expected.lexical_kind;
        identity.list_item_kind = expected.list_item_kind;
        const auto kind = leaf->list_item_kind.empty() ? leaf->lexical_kind : leaf->list_item_kind;
        const auto other = identity.list_item_kind.empty() ? identity.lexical_kind : identity.list_item_kind;
        if (leaf->list_item_kind.empty() != identity.list_item_kind.empty() ||
            scalar_value_family(kind) != scalar_value_family(other)) { return false; }
        if (kind == "QName") {
          return !leaf->list_item_kind.empty() ? xml::lexical_qname_list_equal(text, expected.names, scope, context) :
              expected.names.size() == 1U && xml::resolve_qname(text, scope) == expected.names.front();
        }
        return scalar_identity_equal(*leaf, text, identity, expected.lexical_value, scope, context);
      };
      const auto matches = [&](const auto& allowed) {
        for (const auto& branch : stored.branches) {
          const scalar_facets* expected_leaf{};
          std::string expected;
          try { branch.validate_selected(allowed, scope, selected, &expected_leaf, &expected); }
          catch (const std::invalid_argument&) {
            if (callback_failure) { std::rethrow_exception(callback_failure); }
            continue;
          }
          return scalar_identity_equal(*leaf, text, *expected_leaf, expected, scope, context);
        }
        return false;
      };
      if (facets.union_enumeration.empty() ?
          std::none_of(facets.enumeration.begin(), facets.enumeration.end(), matches) :
          std::none_of(facets.union_enumeration.begin(), facets.union_enumeration.end(), constant_matches)) {
        throw std::invalid_argument{"XML union enumeration facet failed"};
      }
    }
  }
  xml::validate_lexical_contract(text, contract, scope, context);
  if (!stored.item_contracts.empty() && !facets.union_enumeration.empty()) {
    scalar_value_identity candidate{};
    candidate.lexical_kind = facets.lexical_kind;
    candidate.list_item_kind = facets.list_item_kind;
    auto remaining = text;
    while (!(remaining = xml::detail::trim(remaining)).empty()) {
      const auto end = remaining.find_first_of(" \t\r\n");
      const auto token = remaining.substr(0U, end);
      if (context) { context->consume(xml::operation_resource::allocation_bytes, sizeof(scalar_value_identity)); }
      candidate.items.push_back(stored.item_contracts.front().identify(token, scope, context));
      remaining.remove_prefix(token.size());
    }
    if (std::none_of(facets.union_enumeration.begin(), facets.union_enumeration.end(),
        [&](const auto& expected) { return prepared_identity_equal(candidate, expected, scope, context); })) {
      throw std::invalid_argument{"XML restricted-list enumeration facet failed"};
    }
  }
  if (!facets.qname_list_enumeration.empty()) {
    // Compare one item at a time against immutable schema identities rather than
    // resolving schema prefixes against the instance's unrelated namespace map.
    const auto matches = [&](const std::vector<xml::name>& expected) {
      auto remaining = text;
      for (const auto& item : expected) {
        remaining = xml::detail::trim(remaining);
        if (remaining.empty()) { return false; }
        const auto end = remaining.find_first_of(" \t\r\n");
        const auto token = remaining.substr(0U, end);
        if (context) {
          context->consume(xml::operation_resource::work_units, token.size());
          context->consume(xml::operation_resource::allocation_bytes, token.size());
          context->consume(xml::operation_resource::allocation_bytes, item.namespace_uri.size());
        }
        if (xml::resolve_qname(token, scope) != item) { return false; }
        remaining.remove_prefix(token.size());
      }
      return xml::detail::trim(remaining).empty();
    };
    if (std::none_of(facets.qname_list_enumeration.begin(), facets.qname_list_enumeration.end(), matches)) {
      throw std::invalid_argument{"XML QName list enumeration facet failed"};
    }
  }
  for (const auto& group : stored.patterns) {
    if (std::none_of(group.begin(), group.end(), [&](const auto& pattern) {
        return pattern.matches(text, stored.limits, context);
      })) { throw std::invalid_argument{"XML portable pattern facet failed"}; }
  }
  for (const auto& group : stored.provider_patterns) {
    if (std::none_of(group.begin(), group.end(), [&](const auto& pattern) {
        return pattern->matches(text, {}, context);
      })) { throw std::invalid_argument{"XML provider pattern facet failed"}; }
  }
  if (selected_leaf) { *selected_leaf = leaf; }
  if (selected_lexical) {
    if (context) { context->consume(xml::operation_resource::allocation_bytes, text.size()); }
    *selected_lexical = text;
  }
}

// Retain lexical limits exactly and prepare each restriction group's alternative programs once.
compiled_scalar_contract compile_scalar_contract(const scalar_facets& facets, bool require_portable_patterns,
    const xml::pattern_limits& limits) {
  validate_static_facets(facets);
  auto result = std::make_shared<compiled_scalar_contract::implementation>();
  result->facets = facets;
  result->limits = limits;
  if (facets.list_item_facets.size() > 1U) { throw std::invalid_argument{"A lexical list requires one item contract"}; }
  for (const auto& item : facets.list_item_facets) {
    if (!item.list_item_kind.empty()) { throw std::invalid_argument{"A lexical list item must be atomic"}; }
    auto compiled = compile_scalar_contract(item, require_portable_patterns, limits);
    result->portable = result->portable && compiled.patterns_portable();
    result->item_contracts.push_back(std::move(compiled));
  }
  for (const auto& branch : facets.union_members) {
    auto compiled = compile_scalar_contract(branch, require_portable_patterns, limits);
    result->portable = result->portable && compiled.patterns_portable();
    result->branches.push_back(std::move(compiled));
  }
  if (!facets.list_item_facets.empty() && !facets.enumeration.empty() && facets.union_enumeration.empty()) {
    // Standalone callers receive the same prepared item identities as schema-bound declarations.
    auto selection = facets;
    selection.enumeration.clear();
    const auto prepared = compile_scalar_contract(selection, require_portable_patterns, limits);
    for (const auto& literal : facets.enumeration) {
      result->facets.union_enumeration.push_back(prepared.identify(literal));
    }
  }
  const auto append = [&](const std::vector<std::string>& group) {
    if (group.empty()) { return; }
    // Legacy schemas retain the provider's own bounds when portable syntax or
    // lowering resources cannot represent a previously supported expression.
    const auto provider_group = [&] {
      std::vector<std::shared_ptr<const xml::prepared_pattern>> provider{};
      for (const auto& pattern : group) {
        provider.push_back(std::make_shared<const xml::prepared_pattern>(pattern));
      }
      result->provider_patterns.push_back(std::move(provider));
      result->portable = false;
    };
    std::vector<xml::portable_pattern> portable{};
    try {
      for (const auto& pattern : group) { portable.push_back(xml::compile_portable_pattern(pattern, limits)); }
      result->patterns.push_back(std::move(portable));
    } catch (const std::invalid_argument&) {
      if (require_portable_patterns) { throw; }
      provider_group();
    } catch (const std::length_error&) {
      if (require_portable_patterns) { throw; }
      provider_group();
    }
  };
  for (const auto& group : facets.pattern_groups) { append(group); }
  append(facets.patterns);
  return compiled_scalar_contract{std::move(result)};
}
} // namespace rohit::serializer::writer::xml_binding
