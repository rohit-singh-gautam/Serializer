// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_binding {

enum class presence_policy { required, absent, nil, absent_or_nil };
enum class presence_state { absent, nil, value };

// Static presence metadata owns its policy and borrows a stable parsed schema's value type.
// The parsed AST must outlive this contract and must not relocate its member/type vectors.
struct presence_contract {
  presence_policy policy{presence_policy::required};
  bool explicit_policy{};
  std::size_t optional_depth{};
  bool collection{};
  bool lexical_list{};
  bool wrapper{};
  bool allows_absent{};
  bool allows_nil{};
  bool requires_element{true};
  const type_name* value_type{};
};

// Parse one exact, case-sensitive configuration token; unknown/empty tokens throw invalid_argument.
[[nodiscard]] presence_policy parse_presence_policy(std::string_view value);
// Resolve optional storage to explicit XML states; never infer absence/nil from an empty value.
// A missing policy preserves existing nonoptional and single-optional scalar/record semantics.
// Legacy lexical lists occupy one XML value and may use one optional layer without a wrapper.
[[nodiscard]] presence_contract bind_presence(const member& source,
    std::optional<presence_policy> policy = {}, bool attribute = false,
    bool text = false, bool wrapper = false, bool lexical_list = false);
// Classify one normalized element, or absent nullptr; reject prohibited/invalid xsi:nil and nil content.
[[nodiscard]] presence_state classify_presence(const presence_contract& contract,
    const xml::element* value);

// Sparse authoring selections bind one array of owning variant fragments, in native sequence order.
struct mixed_options {
  std::string field{};
  std::string variant_member{"value"};
  std::string text_branch{"text"};
  std::map<std::string, xml::name> children{};
};

// One child alternative owns its XML spelling and borrows the stable variant type metadata.
struct mixed_child_branch {
  std::string branch{};
  xml::name qualified_name{};
  std::size_t alternative{};
  const type_name* value_type{};
};

// The AST must outlive this contract; owned names survive destruction of the input options.
struct mixed_contract {
  const member* source{};
  const class_node* fragment{};
  const member* variant{};
  std::string text_branch{};
  std::size_t text_alternative{};
  std::vector<mixed_child_branch> children{};
};

// Validate an ordinary owning fragment shape and unique expanded child names before emission.
[[nodiscard]] mixed_contract bind_mixed_content(const class_node& source,
    const mixed_options& options, bool has_text_mapping = false,
    std::string_view default_namespace = {});

enum class mixed_item_kind { text, child };

// Text and child pointers borrow a caller-retained normalized document; validation retains nothing.
struct mixed_item_view {
  mixed_item_kind kind{mixed_item_kind::text};
  std::string_view text{};
  const xml::element* child{};
};

// These bounds cover the ordered fragment sequence, not already-parsed child subtree allocations.
struct mixed_trace_limits {
  std::size_t max_fragments{100000U};
  std::size_t max_text_bytes{8U * 1024U * 1024U};
};

// Reject unknown/malformed fragments and cumulative sequence bounds without changing their order.
void validate_mixed_trace(const mixed_contract& contract, std::span<const mixed_item_view> items,
    const mixed_trace_limits& limits = {}, xml::operation_context* context = nullptr);
// Observe a normalized tree in exact text/child order, preserving empty and adjacent text segments.
// Returned views remain valid only while the source tree's strings and child vectors remain stable.
[[nodiscard]] std::vector<mixed_item_view> observe_mixed_content(const mixed_contract& contract,
    const xml::element& value, const mixed_trace_limits& limits = {},
    xml::operation_context* context = nullptr);

} // namespace rohit::serializer::writer::xml_binding
