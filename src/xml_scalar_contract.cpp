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
} // namespace

struct compiled_scalar_contract::implementation {
  scalar_facets facets{};
  xml::pattern_limits limits{};
  std::vector<std::vector<xml::portable_pattern>> patterns{};
  std::vector<std::vector<std::shared_ptr<const xml::prepared_pattern>>> provider_patterns{};
  std::vector<compiled_scalar_contract> branches{};
  bool portable{true};
};

// Share complete prepared metadata across copied compiler contracts.
compiled_scalar_contract::compiled_scalar_contract(std::shared_ptr<const implementation> implementation)
    : implementation_{std::move(implementation)} {}

// Make provider fallback visible to every backend selecting a portable lowering path.
bool compiled_scalar_contract::patterns_portable() const noexcept { return implementation_->portable; }

// Build the borrowed lexical metadata on the stack; all referenced strings remain owned by this IR.
void compiled_scalar_contract::validate(std::string_view text, const xml::element& scope,
    xml::operation_context* context) const {
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
  if (contract.constraints.whitespace != xml::whitespace_policy::preserve) {
    if (context) { context->consume(xml::operation_resource::allocation_bytes, text.size()); }
    normalized = xml::normalize_whitespace(text, contract.constraints.whitespace);
    text = normalized;
  }
  // Existing lexical union validation preserves callback exceptions instead of hiding them as mismatches.
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
      try { branch.validate(text, scope, selected); matched = true; break; }
      catch (const std::invalid_argument&) {
        if (callback_failure) { std::rethrow_exception(callback_failure); }
      }
    }
    if (!matched) { throw std::invalid_argument{"XML value matches no compiled union member"}; }
  }
  xml::validate_lexical_contract(text, contract, scope, context);
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
}

// Retain lexical limits exactly and prepare each restriction group's alternative programs once.
compiled_scalar_contract compile_scalar_contract(const scalar_facets& facets, bool require_portable_patterns,
    const xml::pattern_limits& limits) {
  validate_static_facets(facets);
  auto result = std::make_shared<compiled_scalar_contract::implementation>();
  result->facets = facets;
  result->limits = limits;
  for (const auto& branch : facets.union_members) {
    auto compiled = compile_scalar_contract(branch, require_portable_patterns, limits);
    result->portable = result->portable && compiled.patterns_portable();
    result->branches.push_back(std::move(compiled));
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
