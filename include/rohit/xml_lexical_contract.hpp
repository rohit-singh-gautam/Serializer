// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <algorithm>
#include <exception>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::xml {
// A generated matcher shares its bounded portable program or explicit legacy provider across instances.
using lexical_pattern_matcher = bool (*)(std::string_view, operation_context*);

// Patterns at one restriction step are alternatives; separate inherited steps remain conjunctive.
struct lexical_pattern_group {
  std::span<const lexical_pattern_matcher> alternatives{};
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
};

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
inline void validate_lexical_contract(std::string_view text, const lexical_contract& contract,
    const element& scope, operation_context* context = nullptr) {
  if (!contract.union_members.empty() &&
      (!contract.enumeration.empty() || !contract.qname_enumeration.empty())) {
    throw std::logic_error{"XSD union enumeration requires selected-member value identities"};
  }
  if (contract.list_item_kind == "QName" && !contract.enumeration.empty()) {
    throw std::logic_error{"XSD QName list enumeration requires expanded item identities"};
  }
  if (context) {
    context->consume(operation_resource::work_units, text.size());
    context->consume(operation_resource::work_units, 1U);
  }
  std::string normalized;
  if (contract.constraints.whitespace != whitespace_policy::preserve) {
    if (context) { context->consume(operation_resource::allocation_bytes, text.size()); }
    normalized = normalize_whitespace(text, contract.constraints.whitespace);
    text = normalized;
  }
  const auto lexical = text;
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
        validate_lexical_contract(lexical, alternative, scope, selected);
        matched = true;
        break;
      } catch (const std::invalid_argument&) {
        if (callback_failure) { std::rethrow_exception(callback_failure); }
        // Only a value-space mismatch selects another branch; exhausted budgets must still fail.
      }
    }
    if (!matched) { throw std::invalid_argument{"XML value matches no XSD union member"}; }
  } else if (!contract.list_item_kind.empty()) {
    validate_lexical_list(lexical, contract.list_item_kind, scope, context);
  } else {
    validate_lexical_value(lexical, contract.kind, scope, context);
  }
  if (contract.constraints.lexical_kind.empty() && (contract.kind == "float" || contract.kind == "double")) {
    auto constraints = contract.constraints;
    constraints.lexical_kind = contract.kind;
    validate_scalar_constraints(lexical, constraints, context);
  } else {
    validate_scalar_constraints(lexical, contract.constraints, context);
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
  if (!contract.enumeration.empty()) {
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
}
} // namespace rohit::serializer::xml
