// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::xml {

// Authored slots stay foreign-only; schema wildcards can include absent or core namespaces.
enum class extension_namespace_match { explicit_foreign, any, one_of, except, foreign_fallback };
// Schema declaration checks are independent of the decision to preserve admitted content.
enum class wildcard_process_contents { skip, lax, strict };

// One exact slot: authored permissions are foreign-only; generated predicates follow their XSD.
struct extension_slot_contract {
  std::string id{};
  bool attribute{};
  // SIZE_MAX is the single generated unknown-policy fallback at every actual field boundary.
  std::size_t before_field{};
  std::vector<std::string> namespaces{};
  unknown_policy policy{unknown_policy::reject};
  std::size_t minimum{};
  std::optional<std::size_t> maximum{};
  extension_namespace_match namespace_match{extension_namespace_match::explicit_foreign};
  wildcard_process_contents process_contents{wildcard_process_contents::skip};
};

// Match one URI against a generated predicate, without changing authored foreign-slot rules.
[[nodiscard]] bool matches_extension_namespace(const extension_slot_contract& slot,
    std::string_view namespace_uri);

// Owning metadata can be shared immutably; capabilities mean understood, not merely preserved.
struct extension_contract {
  std::string core_namespace{};
  std::vector<name> known_names{};
  std::vector<extension_slot_contract> slots{};
  std::vector<std::string> required_namespaces{};
  std::vector<name> known_attributes{};
  bool distinct_known_kinds{};
  std::vector<std::size_t> known_positions{};
  // Generated immutable dispatch validates only declarations from the pinned local bundle.
  // Exactly one payload pointer is non-null; false means that no declaration was found.
  bool (*validate_declaration)(const element& scope, const attribute* attribute_value,
      const element* child, limits bounds, operation_context* context){};
};

// Apply processContents independently of retention, borrowing the caller's cumulative context.
void validate_wildcard_declaration(const extension_contract& contract,
    const extension_slot_contract& slot, const element& scope, const attribute* attribute_value,
    const element* child, limits bounds, operation_context* context = nullptr);

// Borrow the original ledger while isolating package-root permissions from nested declarations.
class wildcard_validation_scope {
  operation_context& context_;
  const name* root_type_{};
  unknown_policy unknown_{};
  bool mixed_{};
  bool validation_{};
  std::function<bool(std::string_view)> permissions_{};
public:
  // Moving the namespace predicate avoids copying caller-owned callback state or its counters.
  explicit wildcard_validation_scope(operation_context& context)
      : context_{context}, root_type_{context.permitted_root_type}, unknown_{context.unknown_fields},
        mixed_{context.allow_mixed_content}, validation_{context.declaration_validation},
        permissions_{std::move(context.permits_namespace)} {
    context_.permitted_root_type = nullptr;
    context_.unknown_fields = unknown_policy::reject;
    context_.allow_mixed_content = true;
    context_.declaration_validation = true;
  }
  wildcard_validation_scope(const wildcard_validation_scope&) = delete;
  wildcard_validation_scope& operator=(const wildcard_validation_scope&) = delete;
  // Restore borrowed caller policy even when a declaration or admission callback throws.
  ~wildcard_validation_scope() {
    context_.permitted_root_type = root_type_;
    context_.unknown_fields = unknown_;
    context_.allow_mixed_content = mixed_;
    context_.declaration_validation = validation_;
    context_.permits_namespace = std::move(permissions_);
  }
};

// Reuse a private generated XML codec; every replay byte and temporary value uses the same ledger.
template <typename Model> void validate_declared_element(const element& input, limits bounds,
    operation_context* context, bool strip_nil = false) {
  operation_context local{};
  auto& selected = context ? *context : local;
  wildcard_validation_scope policy{selected};
  static const std::array<name, 2U> markers{{{"nil", "http://www.w3.org/2001/XMLSchema-instance"},
      {"type", "http://www.w3.org/2001/XMLSchema-instance"}}};
  const auto bytes = strip_nil ? write_without_root_attributes(input, markers, bounds, &selected) : write(input, bounds, &selected);
  static_cast<void>(deserialize<Model>(bytes, bounds, nullptr, &selected));
}

// Validate the global element marker before choosing a private normal or attributes-only codec.
[[nodiscard]] inline bool declared_element_nil(const element& input, bool nillable, bool fixed) {
  for (const auto& item : input.attributes) {
    if (item.qualified_name != name{"nil", "http://www.w3.org/2001/XMLSchema-instance"}) { continue; }
    const auto lexical = detail::trim(item.value);
    if (!nillable || (lexical != "true" && lexical != "1" && lexical != "false" && lexical != "0")) {
      throw std::invalid_argument{"Invalid nil marker for a declared XML element"};
    }
    const bool nil = lexical == "true" || lexical == "1";
    if (nil && (fixed || !input.children.empty() || !input.text.empty())) {
      throw std::invalid_argument{"A nil XML element must have no fixed value or content"};
    }
    return nil;
  }
  return false;
}

// Resolve optional dynamic type in the original occurrence scope before removing its marker.
[[nodiscard]] inline std::optional<name> declared_element_type(const element& input,
    operation_context* context = nullptr) {
  for (const auto& item : input.attributes) {
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (item.qualified_name != name{"type", "http://www.w3.org/2001/XMLSchema-instance"}) { continue; }
    if (context) {
      context->consume(operation_resource::allocation_bytes, sizeof(name));
      context->consume(operation_resource::allocation_bytes, item.value.size());
      for (const auto& [prefix, uri] : input.namespace_bindings) {
        (void)prefix;
        context->consume(operation_resource::allocation_bytes, uri.size());
      }
    }
    return resolve_qname(detail::trim(item.value), input);
  }
  return {};
}

// A private attribute DTO preserves explicit-empty attribute semantics and original QName scope.
template <typename Model> void validate_declared_attribute(const element& scope,
    const attribute& input, limits bounds, operation_context* context) {
  element wrapper{};
  wrapper.qualified_name = Model::xml_root_name();
  build_budget budget{bounds, context};
  const auto depth = budget.enter(wrapper.qualified_name);
  for (const auto& [prefix, uri] : scope.namespace_bindings) {
    budget.namespace_binding(prefix, uri);
    wrapper.namespace_bindings.emplace(prefix, uri);
  }
  budget.attribute_name(input.qualified_name);
  budget.text(input.value.size());
  wrapper.attributes.push_back(input);
  validate_declared_element<Model>(wrapper, bounds, context);
}

// Logical preservation bounds exclude source/provider storage and allocator excess capacity.
struct extension_limits {
  limits document{};
  std::size_t max_slots{128U};
  std::size_t max_items{100000U};
  std::size_t max_preserved_bytes{8U * 1024U * 1024U};
};

// Reject ambiguous slots, duplicate IDs/namespaces, core permissions and impossible counts.
void validate_extension_contract(const extension_contract& contract, extension_limits bounds = {});
// Keep explicit native-local metadata admission attached to the active package field.
void validate_extension_contract_at(const extension_contract& contract, extension_limits bounds,
    operation_context* context);

// Resolve a required prefix list in its original scope; unknown capabilities always reject.
// The core namespace is understood implicitly. No provider or remote lookup is performed.
void validate_required_capabilities(const extension_contract& contract, const element& scope,
    std::string_view prefixes, std::span<const std::string> understood_namespaces,
    extension_limits bounds = {}, operation_context* context = nullptr);

// Validate every unknown item and return a complete owning preservation candidate.
// Skip retains nothing; preserve copies exact subtree namespace/mixed-text context. Callers
// provide well-formed normalized XML with full in-scope bindings; this policy layer does not
// reparse characters or grammar. Publish the result only after complete decoding.
// Failure leaves caller-owned output untouched; cumulative context charges are not refunded.
[[nodiscard]] extension_store collect_extension_slots(const extension_contract& contract,
    const element& scope, std::span<const extension_item_view> items,
    std::span<const std::string> understood_namespaces = {},
    std::string_view required_prefixes = {}, extension_limits bounds = {},
    operation_context* context = nullptr);

// Validate already owned opaque payloads without cloning their retained subtrees.
void validate_extension_slots(const extension_contract& contract, const element& scope,
    std::span<const extension_item_view> items, std::span<const std::string> understood_namespaces = {},
    extension_limits bounds = {}, operation_context* context = nullptr);

// Revalidate legacy tree sidecars before output, borrowing the original document and operation limits.
void validate_tree_extension_store(const extension_contract& contract, const name& owner,
    const extension_store& store, operation_context* context = nullptr);

} // namespace rohit::serializer::xml
