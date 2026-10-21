// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <algorithm>
#include <exception>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::xml {
// A generated matcher shares its bounded portable program or explicit legacy provider across instances.
using lexical_pattern_matcher = bool (*)(std::string_view, operation_context*);

// Patterns at one restriction step are alternatives; separate inherited steps remain conjunctive.
struct lexical_pattern_group {
  std::span<const lexical_pattern_matcher> alternatives{};
};

// One ordered schema-owned QName list borrows immutable generated expanded identities.
struct lexical_name_list {
  std::span<const name> items{};
};

// Static lexical metadata for an XSD union branch; spans borrow generated immutable arrays.
// Empty union_members selects the built-in kind, while nested unions retain independent restrictions.
struct lexical_contract {
  std::string_view kind{};
  scalar_constraints constraints{};
  std::string_view list_item_kind{};
  std::span<const std::string_view> enumeration{};
  std::span<const name> qname_enumeration{};
  std::span<const lexical_pattern_group> pattern_groups{};
  std::span<const lexical_contract> union_members{};
  std::span<const lexical_name_list> qname_list_enumeration{};
};

// Compare a lexical list through its instance scope without constructing an owning payload list.
inline bool lexical_qname_list_equal(std::string_view lexical, std::span<const name> expected,
    const element& scope, operation_context* context = nullptr) {
  for (const auto& item : expected) {
    lexical = detail::trim(lexical);
    if (lexical.empty()) { return false; }
    const auto end = lexical.find_first_of(" \t\r\n");
    const auto token = lexical.substr(0U, end);
    if (context) {
      context->consume(operation_resource::work_units, token.size() + 1U);
      context->consume(operation_resource::allocation_bytes, token.size());
      const auto colon = token.find(':');
      const auto prefix = colon == std::string_view::npos ? std::string_view{} : token.substr(0U, colon);
      const auto found = scope.namespace_bindings.find(std::string{prefix});
      if (found != scope.namespace_bindings.end()) {
        context->consume(operation_resource::allocation_bytes, found->second.size());
      }
    }
    if (resolve_qname(token, scope) != item) { return false; }
    lexical.remove_prefix(token.size());
  }
  return detail::trim(lexical).empty();
}

// Render schema identities into one private field scope before exposing its lexical native value.
inline std::string qualify_qname_list(std::span<const name> values, element& scope,
    operation_context* context = nullptr, const std::set<std::string>* reserved = nullptr) {
  if (std::any_of(values.begin(), values.end(), [](const auto& value) { return value.namespace_uri.empty(); })) {
    if (context && !scope.namespace_bindings.contains("")) {
      context->consume(operation_resource::allocation_bytes, sizeof(decltype(scope.namespace_bindings)::value_type));
    }
    scope.namespace_bindings[""] = "";
  }
  std::string result{};
  for (const auto& value : values) {
    if (context) {
      context->consume(operation_resource::work_units, value.local.size() + value.namespace_uri.size() + 1U);
      context->consume(operation_resource::allocation_bytes,
          sizeof(decltype(scope.namespace_bindings)::value_type) + value.local.size() + value.namespace_uri.size());
    }
    const auto item = reserved ? qualify_qname(value, scope, *reserved) : qualify_qname(value, scope);
    if (context) { context->consume(operation_resource::allocation_bytes, item.size() + !result.empty()); }
    if (!result.empty()) { result += ' '; }
    result += item;
  }
  return result;
}

// XML-only child scopes follow field occurrence order; native codecs never serialize this state.
using child_namespace_store = std::map<std::string, std::vector<element>, std::less<>>;

// Create field state only when decoding that field, admitting its map node before allocation.
inline std::vector<element>& child_namespace_destination(child_namespace_store& retained,
    std::string_view field, operation_context* context = nullptr) {
  const auto found = retained.find(field);
  if (found != retained.end()) { return found->second; }
  if (context) {
    context->consume(operation_resource::allocation_bytes,
        sizeof(child_namespace_store::value_type));
    context->consume(operation_resource::allocation_bytes, field.size());
  }
  return retained.emplace(std::string{field}, std::vector<element>{}).first->second;
}

// Borrow all saved occurrence scopes without creating storage for a newly authored field.
inline std::span<const element> child_namespace_occurrences(const child_namespace_store& retained,
    std::string_view field) {
  const auto found = retained.find(field);
  return found == retained.end() ? std::span<const element>{} : std::span<const element>{found->second};
}

// Charge retained namespace storage before copying it; a callback failure publishes no new scope.
inline void retain_child_namespace_scope(std::vector<element>& destination,
    const element& source, operation_context* context = nullptr) {
  if (context) {
    context->consume(operation_resource::allocation_bytes, sizeof(element));
    for (const auto& [prefix, uri] : source.namespace_bindings) {
      context->consume(operation_resource::work_units, 1U);
      context->consume(operation_resource::allocation_bytes,
          sizeof(std::pair<const std::string, std::string>));
      context->consume(operation_resource::allocation_bytes, prefix.size());
      context->consume(operation_resource::allocation_bytes, uri.size());
    }
  }
  element candidate{};
  candidate.namespace_bindings = source.namespace_bindings;
  if (!candidate.namespace_bindings.contains("")) {
    if (context) { context->consume(operation_resource::allocation_bytes,
        sizeof(std::pair<const std::string, std::string>)); }
    candidate.namespace_bindings.emplace("", "");
  }
  destination.push_back(std::move(candidate));
}

// Retain each scalar child's complete scope, including default namespace resets and shadowing.
inline void retain_child_namespace_scopes(std::span<const element> children,
    const name& child_name, std::vector<element>& destination,
    operation_context* context = nullptr) {
  for (const auto& child : children) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (child.qualified_name == child_name) {
      retain_child_namespace_scope(destination, child, context);
    }
  }
}

// Borrow an occurrence's original scope; newly authored values use their current owner scope.
inline const element& child_namespace_scope(const child_namespace_store& retained,
    std::string_view field, std::size_t occurrence, const element& owner) {
  const auto found = retained.find(field);
  return found != retained.end() && occurrence < found->second.size() ?
      found->second[occurrence] : owner;
}

// Let schema defaults add declarations to the exact child scope that will later be emitted.
inline element& child_namespace_scope(child_namespace_store& retained,
    std::string_view field, std::size_t occurrence, element& owner) {
  const auto found = retained.find(field);
  return found != retained.end() && occurrence < found->second.size() ?
      found->second[occurrence] : owner;
}

// Restore only matching scalar children, accounting namespace allocations through the build budget.
inline void restore_child_namespace_scopes(std::span<element> children, const name& child_name,
    const child_namespace_store& retained, std::string_view field, build_budget& budget,
    const lexical_contract* contract = nullptr, const element* fallback_scope = nullptr) {
  const auto found = retained.find(field);
  if (found == retained.end() && !contract) { return; }
  std::size_t occurrence{};
  for (auto& child : children) {
    if (budget.context()) { budget.context()->consume(operation_resource::work_units, 1U); }
    if (child.qualified_name != child_name) { continue; }
    const auto* scope = found != retained.end() && occurrence < found->second.size() ?
        &found->second[occurrence] : fallback_scope;
    ++occurrence;
    child.namespace_contract = contract;
    if (!scope) { continue; }
    for (const auto& [prefix, uri] : scope->namespace_bindings) {
      budget.namespace_binding(prefix, uri);
    }
    child.namespace_bindings = scope->namespace_bindings;
  }
}

// Reject child-only/shadowed QName bindings that cannot be retained by a legacy record-wide scope.
// Inherited equivalent bindings retain existing behavior; no source or target tree is modified.
inline void validate_child_namespace_projection(std::span<const element> children,
    const name& child_name, const element& target_scope, bool list, bool prefixes,
    bool allow_empty, operation_context* context = nullptr) {
  for (const auto& child : children) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (child.qualified_name != child_name) { continue; }
    auto text = detail::trim(child.text);
    if ((list || allow_empty) && text.empty()) { continue; }
    if (context) {
      context->consume(operation_resource::work_units, text.size());
      context->consume(operation_resource::allocation_bytes, text.size());
    }
    if (prefixes) {
      if (resolve_prefix_list(text, child) != resolve_prefix_list(text, target_scope)) {
        throw std::invalid_argument{"C++ child prefix bindings require per-field namespace identities"};
      }
    } else {
      do {
        const auto end = list ? text.find_first_of(" \t\r\n") : std::string_view::npos;
        const auto token = text.substr(0U, end);
        if (resolve_qname(token, child) != resolve_qname(token, target_scope)) {
          throw std::invalid_argument{"C++ child QName bindings require per-field namespace identities"};
        }
        text = end == std::string_view::npos ? std::string_view{} : detail::trim(text.substr(end));
      } while (!text.empty());
    }
  }
}

// Validate one complete branch, propagating resource/provider failures instead of hiding them as mismatches.
// The caller keeps scope and all static metadata alive throughout this synchronous operation.
inline void validate_lexical_contract_member(std::string_view text, const lexical_contract& contract,
    const element& scope, operation_context* context, const lexical_contract** selected_member,
    std::optional<std::string>* selected_lexical = nullptr) {
  if (!contract.union_members.empty() &&
      (!contract.enumeration.empty() || !contract.qname_enumeration.empty())) {
    throw std::logic_error{"XSD union enumeration requires selected-member value identities"};
  }
  if (contract.list_item_kind == "QName" && !contract.enumeration.empty() &&
      contract.qname_list_enumeration.empty()) {
    throw std::logic_error{"XSD QName list enumeration requires expanded item identities"};
  }
  if (context) {
    context->consume(operation_resource::work_units, text.size());
    context->consume(operation_resource::work_units, 1U);
  }
  std::optional<std::string> normalized;
  if (contract.union_members.empty() &&
      contract.constraints.whitespace != whitespace_policy::preserve) {
    if (context) { context->consume(operation_resource::allocation_bytes, text.size()); }
    normalized = normalize_whitespace(text, contract.constraints.whitespace);
    text = *normalized;
  }
  const lexical_contract* member = &contract;
  if (!contract.union_members.empty()) {
    std::exception_ptr callback_failure;
    std::optional<operation_context> guarded;
    auto* selected = context;
    if (context && (context->is_cancelled || context->charge)) {
      guarded.emplace(*context);
      // A caller can throw invalid_argument too; preserve its failure instead of trying another branch.
      if (context->is_cancelled) {
        guarded->is_cancelled = [&] {
          try { return context->is_cancelled(); }
          catch (...) { callback_failure = std::current_exception(); throw; }
        };
      }
      if (context->charge) {
        guarded->charge = [&](operation_resource resource, std::size_t amount) {
          try { context->charge(resource, amount); }
          catch (...) { callback_failure = std::current_exception(); throw; }
        };
      }
      selected = &*guarded;
    }
    bool matched{};
    for (const auto& alternative : contract.union_members) {
      try {
        // A union does not normalize before selection; each member sees the original lexical value.
        validate_lexical_contract_member(text, alternative, scope, selected, &member, &normalized);
        matched = true;
        break;
      } catch (const std::invalid_argument&) {
        if (callback_failure) { std::rethrow_exception(callback_failure); }
        // Only a value-space mismatch selects another branch; exhausted budgets must still fail.
      }
    }
    if (!matched) { throw std::invalid_argument{"XML value matches no XSD union member"}; }
    if (normalized) { text = *normalized; }
  } else if (!contract.list_item_kind.empty()) {
    validate_lexical_list(text, contract.list_item_kind, scope, context);
  } else {
    validate_lexical_value(text, contract.kind, scope, context);
  }
  const auto lexical = text;
  if (contract.constraints.lexical_kind.empty() && (contract.kind == "float" || contract.kind == "double")) {
    auto constraints = contract.constraints;
    constraints.lexical_kind = contract.kind;
    validate_scalar_constraints(lexical, constraints, context);
  } else {
    auto constraints = contract.constraints;
    // Union facets inspect the successful member's normalized value without selecting it again.
    if (!contract.union_members.empty()) { constraints.whitespace = whitespace_policy::preserve; }
    validate_scalar_constraints(lexical, constraints, context);
  }
  for (const auto& group : contract.pattern_groups) {
    const auto matches = [&](const auto match) { return match && match(lexical, context); };
    if (std::find_if(group.alternatives.begin(), group.alternatives.end(), matches) ==
        group.alternatives.end()) {
      throw std::invalid_argument{"XML pattern facet failed"};
    }
  }
  if (!contract.qname_enumeration.empty()) {
    const auto expanded = resolve_qname(lexical, scope);
    if (std::find(contract.qname_enumeration.begin(), contract.qname_enumeration.end(), expanded) ==
        contract.qname_enumeration.end()) {
      throw std::invalid_argument{"XML QName enumeration facet failed"};
    }
  }
  if (!contract.qname_list_enumeration.empty() &&
      std::none_of(contract.qname_list_enumeration.begin(), contract.qname_list_enumeration.end(),
          [&](const auto& allowed) { return lexical_qname_list_equal(lexical, allowed.items, scope, context); })) {
    throw std::invalid_argument{"XML QName list enumeration facet failed"};
  }
  // Expanded QName restrictions already retain their schema scope; lexical prefixes belong to it.
  if (!contract.enumeration.empty() &&
      !(contract.kind == "QName" && !contract.qname_enumeration.empty()) &&
      contract.qname_list_enumeration.empty()) {
    const auto matches = [&](std::string_view allowed) {
      if (context) {
        context->consume(operation_resource::work_units, lexical.size());
        context->consume(operation_resource::work_units, allowed.size());
      }
      if (!contract.list_item_kind.empty()) {
        return lexical_lists_equal(lexical, allowed, contract.list_item_kind, scope);
      }
      if (contract.kind.empty() || contract.kind == "string" || contract.kind == "normalizedString" ||
          contract.kind == "token" || contract.kind == "anyURI") {
        if (contract.constraints.whitespace == whitespace_policy::preserve) { return lexical == allowed; }
        if (context) { context->consume(operation_resource::allocation_bytes, allowed.size()); }
        return lexical == normalize_whitespace(allowed, contract.constraints.whitespace);
      }
      return lexical_lists_equal(lexical, allowed, contract.kind, scope);
    };
    if (std::find_if(contract.enumeration.begin(), contract.enumeration.end(), matches) ==
        contract.enumeration.end()) {
      throw std::invalid_argument{"XML enumeration facet failed"};
    }
  }
  if (selected_member) { *selected_member = member; }
  if (selected_lexical) { *selected_lexical = std::move(normalized); }
}

// Validate the complete restriction without retaining the selected branch beyond this operation.
inline void validate_lexical_contract(std::string_view text, const lexical_contract& contract,
    const element& scope, operation_context* context = nullptr) {
  validate_lexical_contract_member(text, contract, scope, context, nullptr);
}

// Copy namespace-only state with admission before allocation; no XML descendants are retained.
inline element copy_namespace_scope(const element& source, operation_context* context = nullptr) {
  if (context) {
    for (const auto& [prefix, uri] : source.namespace_bindings) {
      context->consume(operation_resource::work_units, 1U);
      context->consume(operation_resource::allocation_bytes,
          sizeof(decltype(source.namespace_bindings)::value_type) + prefix.size() + uri.size());
    }
  }
  element result{};
  result.namespace_bindings = source.namespace_bindings;
  return result;
}

// Re-present one namespace-sensitive value after schema defaults require a different owner default.
// Only the first successful namespace-dependent union member is rewritten; other strings stay lexical.
inline std::string reproject_namespace_lexical(std::string_view text, const lexical_contract& contract,
    const element& original, element& destination, bool prefixes,
    const std::set<std::string>& reserved, operation_context* context = nullptr) {
  const lexical_contract* selected{};
  validate_lexical_contract_member(text, contract, original, context, &selected);
  if (prefixes) {
    const auto values = resolve_prefix_list(text, original);
    if (resolve_prefix_list(text, destination) == values) {
      if (context) { context->consume(operation_resource::allocation_bytes, text.size()); }
      return std::string{text};
    }
    std::string result{};
    for (const auto& uri : values) {
      const auto lexical = qualify_qname({"value", uri}, destination, reserved);
      const auto colon = lexical.find(':');
      if (colon == std::string::npos) { throw std::invalid_argument{"XML prefix list requires a namespace"}; }
      if (context) { context->consume(operation_resource::allocation_bytes, colon + !result.empty()); }
      if (!result.empty()) { result += ' '; }
      result += lexical.substr(0U, colon);
    }
    return result;
  }
  if (selected->kind != "QName" && selected->list_item_kind != "QName") {
    if (context) { context->consume(operation_resource::allocation_bytes, text.size()); }
    return std::string{text};
  }
  const bool list = selected->list_item_kind == "QName";
  std::vector<name> names{};
  bool unchanged = true;
  auto remaining = detail::trim(text);
  while (!remaining.empty()) {
    const auto end = list ? remaining.find_first_of(" \t\r\n") : std::string_view::npos;
    const auto token = remaining.substr(0U, end);
    if (context) {
      context->consume(operation_resource::work_units, token.size());
      context->consume(operation_resource::allocation_bytes, sizeof(name) + token.size());
      const auto colon = token.find(':');
      const auto prefix = colon == std::string_view::npos ? std::string_view{} : token.substr(0U, colon);
      const auto found = original.namespace_bindings.find(std::string{prefix});
      if (found != original.namespace_bindings.end()) {
        context->consume(operation_resource::allocation_bytes, found->second.size());
      } else if (prefix == "xml") {
        context->consume(operation_resource::allocation_bytes, std::string_view{"http://www.w3.org/XML/1998/namespace"}.size());
      }
    }
    names.push_back(resolve_qname(token, original));
    const auto colon = token.find(':');
    const auto prefix = colon == std::string_view::npos ? std::string_view{} : token.substr(0U, colon);
    const auto found = destination.namespace_bindings.find(std::string{prefix});
    const auto uri = prefix == "xml" ? std::string_view{"http://www.w3.org/XML/1998/namespace"} :
        found == destination.namespace_bindings.end() ? std::string_view{} : std::string_view{found->second};
    unchanged = unchanged && (prefix.empty() || prefix == "xml" || found != destination.namespace_bindings.end()) &&
        uri == names.back().namespace_uri;
    remaining = end == std::string_view::npos ? std::string_view{} : detail::trim(remaining.substr(end));
  }
  if (unchanged) {
    if (context) { context->consume(operation_resource::allocation_bytes, text.size()); }
    return std::string{text};
  }
  return qualify_qname_list(names, destination, context, &reserved);
}

// Preserve optional/list native storage while adopting one compatible owner presentation vocabulary.
template <typename T> void reproject_namespace_value(T& value, const lexical_contract& contract,
    const element& original, element& destination, bool prefixes,
    const std::set<std::string>& reserved, operation_context* context = nullptr) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { reproject_namespace_value(*value, contract, original, destination, prefixes, reserved, context); }
  } else if constexpr (detail::collection_traits<T>::value) {
    value = parse_list<T>(reproject_namespace_lexical(format_list(value, context), contract,
        original, destination, prefixes, reserved, context), context);
  } else {
    value = reproject_namespace_lexical(value, contract, original, destination, prefixes, reserved, context);
  }
}

// Materialize only lexical-token prefixes needed by a union's namespace-dependent alternatives.
template <typename Lookup> element lexical_namespace_scope(std::string_view text,
    Lookup&& lookup, operation_context* context = nullptr) {
  element result{};
  if (context) { context->consume(operation_resource::work_units, text.size()); }
  const auto retain = [&](std::string_view prefix) {
    if (context) { context->consume(operation_resource::allocation_bytes, prefix.size()); }
    std::string prefix_text{prefix};
    if (result.namespace_bindings.contains(prefix_text)) { return; }
    const auto uri = lookup(prefix_text);
    if (context) {
      context->consume(operation_resource::work_units, prefix.size());
      context->consume(operation_resource::allocation_bytes,
          sizeof(std::pair<const std::string, std::string>));
      context->consume(operation_resource::allocation_bytes, uri.size());
    }
    result.namespace_bindings.emplace(std::move(prefix_text), uri);
  };
  retain("");
  auto remaining = detail::trim(text);
  while (!remaining.empty()) {
    const auto end = remaining.find_first_of(" \t\r\n");
    const auto token = remaining.substr(0U, end);
    const auto colon = token.find(':');
    if (colon != std::string_view::npos) { retain(token.substr(0U, colon)); }
    remaining = end == std::string_view::npos ? std::string_view{} : detail::trim(remaining.substr(end));
  }
  return result;
}

// Reject final inherited/generated bindings that change the first successful member or QName identities.
inline void validate_lexical_namespace_identity(std::string_view text, const lexical_contract& contract,
    const element& original_scope, const element& emitted_scope, operation_context* context = nullptr) {
  const lexical_contract* original_member{};
  const lexical_contract* emitted_member{};
  std::optional<std::string> normalized;
  validate_lexical_contract_member(text, contract, original_scope, context, &original_member, &normalized);
  validate_lexical_contract_member(text, contract, emitted_scope, context, &emitted_member);
  if (original_member != emitted_member) {
    throw std::invalid_argument{"Inherited XML namespace changes the selected union member"};
  }
  if (original_member->kind != "QName" && original_member->list_item_kind != "QName") { return; }
  auto lexical = detail::trim(normalized ? std::string_view{*normalized} : text);
  const bool list = original_member->list_item_kind == "QName";
  if (list && lexical.empty()) { return; }
  do {
    const auto end = list ? lexical.find_first_of(" \t\r\n") : std::string_view::npos;
    const auto token = lexical.substr(0U, end);
    if (resolve_qname(token, original_scope) != resolve_qname(token, emitted_scope)) {
      throw std::invalid_argument{"Inherited XML namespace changes a union QName identity"};
    }
    lexical = end == std::string_view::npos ? std::string_view{} : detail::trim(lexical.substr(end));
  } while (!lexical.empty());
}

// Keep the first successful union member and its QName identities unchanged in record-wide storage.
// Local/shadowed bindings that would change either selection or expanded names reject explicitly.
inline void validate_child_lexical_projection(std::span<const element> children,
    const name& child_name, const element& target_scope, const lexical_contract& contract,
    operation_context* context = nullptr) {
  for (const auto& child : children) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (child.qualified_name != child_name) { continue; }
    const lexical_contract* source_member{};
    const lexical_contract* target_member{};
    validate_lexical_contract_member(child.text, contract, child, context, &source_member);
    validate_lexical_contract_member(child.text, contract, target_scope, context, &target_member);
    if (source_member != target_member) {
      throw std::invalid_argument{"C++ child union selection requires per-field namespace identities"};
    }
    if (source_member->kind == "QName" || source_member->list_item_kind == "QName") {
      validate_child_namespace_projection(std::span{&child, 1U}, child_name, target_scope,
          source_member->list_item_kind == "QName", false, false, context);
    }
  }
}
} // namespace rohit::serializer::xml
