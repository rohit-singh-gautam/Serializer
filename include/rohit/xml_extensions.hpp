// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::xml {

// One explicitly positioned foreign-vocabulary slot; unknown core names always reject.
struct extension_slot_contract {
  std::string id{};
  bool attribute{};
  std::size_t before_field{};
  std::vector<std::string> namespaces{};
  unknown_policy policy{unknown_policy::reject};
  std::size_t minimum{};
  std::optional<std::size_t> maximum{};
};

// Owning metadata can be shared immutably; capabilities mean understood, not merely preserved.
struct extension_contract {
  std::string core_namespace{};
  std::vector<name> known_names{};
  std::vector<extension_slot_contract> slots{};
  std::vector<std::string> required_namespaces{};
};

// A generated traversal supplies exactly one payload and its precise field/child position.
// All payload and namespace-context storage is borrowed through collect_extension_slots().
struct extension_item_view {
  const xml::attribute* attribute{};
  const element* child{};
  std::size_t before_field{};
  std::size_t before_child{};
};

// Logical preservation bounds exclude source/provider storage and allocator excess capacity.
struct extension_limits {
  limits document{};
  std::size_t max_slots{128U};
  std::size_t max_items{100000U};
  std::size_t max_preserved_bytes{8U * 1024U * 1024U};
};

// Reject ambiguous slots, duplicate IDs/namespaces, core permissions and impossible counts.
void validate_extension_contract(const extension_contract& contract, extension_limits bounds = {});

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

} // namespace rohit::serializer::xml
