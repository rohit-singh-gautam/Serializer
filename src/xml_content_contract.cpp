// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_content_contract.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsi_namespace{"http://www.w3.org/2001/XMLSchema-instance"};
constexpr std::size_t absent_or_nil_depth{2U};

// Identify an ordinary owning record without importing managed/reference/revision storage contracts.
bool is_ordinary_record(const class_node& source) {
  return source.type == object_type::class_type && source.parents.empty() &&
      (static_cast<std::uint8_t>(source.attributes) &
       static_cast<std::uint8_t>(class_attributes::managed)) == 0U &&
      source.storage_modes == static_cast<std::uint8_t>(storage_mode::owning) &&
      source.type_parameters.empty() && source.generic_parameters.empty() &&
      !source.reference && !source.has_magic() && source.version_member() == nullptr;
}

// Select one XML-participating member, rejecting missing and duplicate native selectors.
const member& select_member(const class_node& source, std::string_view name) {
  const member* result{};
  for (const auto& field : source.member_list) {
    if (field.name != name || field.omits("xml")) { continue; }
    if (result) { throw std::invalid_argument{"Ambiguous XML mixed-content member"}; }
    result = &field;
  }
  if (!result) {
    throw std::invalid_argument{"Unknown XML mixed-content member: " + std::string{name}};
  }
  return *result;
}

// Validate finite fragment accounting before a sum can overflow or allocation can occur.
void add_bounded(std::size_t& total, std::size_t amount, std::size_t limit,
    std::string_view message) {
  if (amount > limit || total > limit - amount) {
    throw std::length_error{std::string{message}};
  }
  total += amount;
}

// Require one actual ordinary record alternative, rather than an optional/container/scalar branch.
const class_node& ordinary_alternative(const type_name& value) {
  if (value.type != object_type::class_type || !value.resolved_node || value.application ||
      !value.arguments.empty()) {
    throw std::invalid_argument{"XML mixed child alternatives require ordinary record types"};
  }
  const auto& result = static_cast<const class_node&>(*value.resolved_node);
  if (!is_ordinary_record(result)) {
    throw std::invalid_argument{"XML mixed child alternatives require ordinary owning records"};
  }
  return result;
}

// Remove only XML whitespace around the XML Schema boolean lexical value.
std::string_view trim_xml_space(std::string_view value) {
  constexpr std::string_view whitespace{" \t\r\n"};
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string_view::npos) { return {}; }
  return value.substr(first, value.find_last_not_of(whitespace) - first + 1U);
}
} // namespace

// Keep configuration spelling closed so misspellings cannot silently select a default contract.
presence_policy parse_presence_policy(std::string_view value) {
  if (value == "required") { return presence_policy::required; }
  if (value == "absent") { return presence_policy::absent; }
  if (value == "nil") { return presence_policy::nil; }
  if (value == "absent_or_nil") { return presence_policy::absent_or_nil; }
  throw std::invalid_argument{"Invalid XML presence policy: " + std::string{value}};
}

// Peel optional layers once and require a one-to-one relationship between storage and XML states.
presence_contract bind_presence(const member& source, std::optional<presence_policy> policy,
    bool attribute, bool text, bool wrapper, bool lexical_list) {
  if (source.type_name_list.size() != 1U ||
      source.modifier == member::modifier_type::variant ||
      source.modifier == member::modifier_type::map || source.managed || source.wire_only) {
    throw std::invalid_argument{"XML presence requires one ordinary scalar, record or sequence"};
  }
  presence_contract result{};
  result.explicit_policy = policy.has_value();
  result.wrapper = wrapper;
  result.lexical_list = lexical_list;
  const auto* value_type = &source.type_name_list.front();
  while (value_type->is_optional()) {
    if (value_type->arguments.size() != 1U) {
      throw std::invalid_argument{"XML optional presence requires one type argument"};
    }
    ++result.optional_depth;
    value_type = &value_type->arguments.front();
  }
  result.value_type = value_type;
  result.collection = source.modifier == member::modifier_type::array || value_type->is_sequence();
  if (source.modifier == member::modifier_type::array && result.optional_depth != 0U) {
    throw std::invalid_argument{"XML optional collection items require a separate record mapping"};
  }
  if (value_type->is_sequence()) {
    if (value_type->arguments.size() != 1U || value_type->arguments.front().is_optional() ||
        value_type->arguments.front().is_sequence() ||
        source.modifier == member::modifier_type::array) {
      throw std::invalid_argument{"XML nested/optional collection items are ambiguous"};
    }
  }
  if (value_type->is_associative() || value_type->is_expected() ||
      value_type->is_managed_reference()) {
    throw std::invalid_argument{"Unsupported XML presence value shape"};
  }
  if (lexical_list && (policy || wrapper || !result.collection || result.optional_depth > 1U ||
      (value_type->is_sequence() ? value_type->arguments.front().type : value_type->type) ==
          object_type::class_type)) {
    throw std::invalid_argument{"XML lexical-list presence requires its legacy scalar-list contract"};
  }
  if (result.optional_depth != 0U && result.collection && !wrapper && !lexical_list) {
    throw std::invalid_argument{"XML optional collections require an explicit wrapper"};
  }
  if (policy && result.collection && !wrapper) {
    throw std::invalid_argument{"Explicit XML collection presence requires a wrapper"};
  }
  result.policy = policy.value_or(result.optional_depth == 0U ?
      presence_policy::required : presence_policy::absent);
  const auto required_depth = result.policy == presence_policy::required ? 0U :
      result.policy == presence_policy::absent_or_nil ? absent_or_nil_depth : 1U;
  if (result.optional_depth != required_depth) {
    throw std::invalid_argument{"XML presence policy does not match optional storage depth"};
  }
  result.allows_absent = result.policy == presence_policy::absent ||
      result.policy == presence_policy::absent_or_nil;
  result.allows_nil = result.policy == presence_policy::nil ||
      result.policy == presence_policy::absent_or_nil;
  result.requires_element = !result.allows_absent;
  if ((attribute || text) && result.allows_nil) {
    throw std::invalid_argument{"XML xsi:nil requires an element mapping"};
  }
  if (text && result.optional_depth != 0U) {
    throw std::invalid_argument{"XML optional text cannot distinguish absence from an empty value"};
  }
  return result;
}

// Recognize nil by expanded attribute identity; prefix choices and empty values never change state.
presence_state classify_presence(const presence_contract& contract, const xml::element* value) {
  if (!value) {
    if (!contract.allows_absent) { throw std::invalid_argument{"Required XML element is absent"}; }
    return presence_state::absent;
  }
  const xml::attribute* nil{};
  for (const auto& attribute : value->attributes) {
    if (attribute.qualified_name.local != "nil" ||
        attribute.qualified_name.namespace_uri != xsi_namespace) { continue; }
    if (nil) { throw std::invalid_argument{"Duplicate XML xsi:nil attribute"}; }
    nil = &attribute;
  }
  if (!nil) { return presence_state::value; }
  if (!contract.allows_nil) { throw std::invalid_argument{"XML xsi:nil is not permitted by presence policy"}; }
  const auto lexical = trim_xml_space(nil->value);
  if (lexical == "false" || lexical == "0") { return presence_state::value; }
  if (lexical != "true" && lexical != "1") {
    throw std::invalid_argument{"Invalid XML xsi:nil boolean value"};
  }
  if (!value->text.empty() || !value->children.empty() ||
      std::ranges::any_of(value->text_segments, [](const auto& segment) {
        return !segment.value.empty();
      })) {
    throw std::invalid_argument{"Nil XML element cannot contain text or child elements"};
  }
  return presence_state::nil;
}

// Bind explicit native ordered storage; parallel text/child fields cannot represent interleaving.
mixed_contract bind_mixed_content(const class_node& source, const mixed_options& options,
    bool has_text_mapping, std::string_view default_namespace) {
  if (!is_ordinary_record(source) || options.field.empty() || options.variant_member.empty() ||
      options.text_branch.empty() || has_text_mapping) {
    throw std::invalid_argument{"XML mixed content requires one explicit ordinary ordered field"};
  }
  const auto& selected = select_member(source, options.field);
  if (selected.managed || selected.wire_only || selected.type_name_list.size() != 1U) {
    throw std::invalid_argument{"XML mixed content requires an ordinary fragment array"};
  }
  const auto* fragment_type = &selected.type_name_list.front();
  if (selected.modifier == member::modifier_type::array) {
    if (fragment_type->is_sequence()) {
      throw std::invalid_argument{"XML mixed content cannot use nested fragment arrays"};
    }
  } else if (selected.modifier == member::modifier_type::none && fragment_type->is_sequence() &&
      fragment_type->arguments.size() == 1U) {
    fragment_type = &fragment_type->arguments.front();
  } else {
    throw std::invalid_argument{"XML mixed content requires a nonoptional fragment array"};
  }
  const auto& fragment = ordinary_alternative(*fragment_type);
  const auto& variant = select_member(fragment, options.variant_member);
  const auto durable_count = std::ranges::count_if(fragment.member_list, [](const auto& field) {
    return !field.transient;
  });
  if (durable_count != 1 || variant.modifier != member::modifier_type::variant ||
      !variant.owning_variant || variant.managed || variant.wire_only ||
      variant.type_name_list.size() < 2U) {
    throw std::invalid_argument{"XML mixed fragments require exactly one owning text/child variant"};
  }
  mixed_contract result{&selected, &fragment, &variant, options.text_branch,
      options.element_only ? variant.type_name_list.size() : 0U, {}, options.element_only};
  std::set<std::string> branch_names{};
  std::set<std::pair<std::string, std::string>> child_names{};
  std::size_t text_count{};
  for (std::size_t index{}; index < variant.type_name_list.size(); ++index) {
    const auto& alternative = variant.type_name_list[index];
    const auto branch = alternative.enum_name.empty() ? alternative.name : alternative.enum_name;
    if (!branch_names.insert(branch).second) {
      throw std::invalid_argument{"Ambiguous XML mixed variant branch"};
    }
    if (alternative.type == object_type::primitive && alternative.name == "string" &&
        !alternative.application) {
      if (options.element_only) {
        throw std::invalid_argument{"XML ordered content accepts only record alternatives, without text"};
      }
      ++text_count;
      if (branch != options.text_branch) {
        throw std::invalid_argument{"XML mixed text branch selector does not match the string branch"};
      }
      result.text_alternative = index;
      continue;
    }
    const auto& child = ordinary_alternative(alternative);
    const auto renamed = options.children.find(branch);
    auto name = renamed == options.children.end() ?
        xml::name{child.name, std::string{default_namespace}} : renamed->second;
    if (!child_names.emplace(name.namespace_uri, name.local).second) {
      throw std::invalid_argument{"Invalid or colliding XML mixed child expanded names"};
    }
    xml::element name_check{};
    name_check.qualified_name = name;
    (void)xml::write(name_check);
    result.children.push_back({branch, std::move(name), index, &alternative});
  }
  if ((!options.element_only && text_count != 1U) || result.children.empty()) {
    throw std::invalid_argument{"XML mixed variants require exactly one string branch and record children"};
  }
  for (const auto& [branch, name] : options.children) {
    (void)name;
    if (std::ranges::none_of(result.children, [&](const auto& child) { return child.branch == branch; })) {
      throw std::invalid_argument{"Unknown XML mixed child branch override: " + branch};
    }
  }
  return result;
}

// Bound fragments/text cumulatively and resolve each child without sorting or coalescing its peers.
void validate_mixed_trace(const mixed_contract& contract, std::span<const mixed_item_view> items,
    const mixed_trace_limits& limits, xml::operation_context* context) {
  if (context) { context->check(); }
  if (items.size() > limits.max_fragments) {
    throw std::length_error{"XML mixed fragment limit exceeded"};
  }
  std::size_t text_bytes{};
  for (const auto& item : items) {
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
    if (item.kind == mixed_item_kind::text) {
      if (contract.element_only) {
        throw std::invalid_argument{"XML ordered content cannot contain text fragments"};
      }
      if (item.child) { throw std::invalid_argument{"XML mixed text fragment contains a child"}; }
      add_bounded(text_bytes, item.text.size(), limits.max_text_bytes,
          "XML mixed text limit exceeded");
    } else if (item.kind == mixed_item_kind::child) {
      if (!item.child || !item.text.empty()) {
        throw std::invalid_argument{"Invalid XML mixed child fragment"};
      }
      if (std::ranges::none_of(contract.children, [&](const auto& branch) {
        return branch.qualified_name == item.child->qualified_name;
      })) {
        throw std::invalid_argument{"Unknown XML mixed child element"};
      }
    } else {
      throw std::invalid_argument{"Unknown XML mixed fragment kind"};
    }
  }
}

// Recover exact segment positions from the normalized tree and reject a lossy unordered text shape.
std::vector<mixed_item_view> observe_mixed_content(const mixed_contract& contract,
    const xml::element& value, const mixed_trace_limits& limits, xml::operation_context* context) {
  if (contract.element_only) {
    // Formatting has no stored occurrence in an element-only grammar. Check it before
    // charging the child-only sequence so whitespace cannot consume the fragment quota.
    if (value.text.find_first_not_of(" \t\r\n") != std::string::npos ||
        std::ranges::any_of(value.text_segments, [](const auto& segment) {
          return segment.value.find_first_not_of(" \t\r\n") != std::string::npos;
        })) {
      throw std::invalid_argument{"XML ordered content cannot contain text fragments"};
    }
    if (value.children.size() > limits.max_fragments ||
        value.children.size() > std::numeric_limits<std::size_t>::max() / sizeof(mixed_item_view)) {
      throw std::length_error{"XML mixed fragment limit exceeded"};
    }
    if (context) {
      context->consume(xml::operation_resource::allocation_bytes, value.children.size() * sizeof(mixed_item_view));
    }
    std::vector<mixed_item_view> result;
    result.reserve(value.children.size());
    for (const auto& child : value.children) { result.push_back({mixed_item_kind::child, {}, &child}); }
    validate_mixed_trace(contract, result, limits, context);
    return result;
  }
  std::size_t count{value.children.size()};
  add_bounded(count, value.text_segments.empty() ? (value.text.empty() ? 0U : 1U) :
      value.text_segments.size(), limits.max_fragments, "XML mixed fragment limit exceeded");
  if (value.text.size() > limits.max_text_bytes) {
    throw std::length_error{"XML mixed text limit exceeded"};
  }
  std::vector<mixed_item_view> result{};
  if (context) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(mixed_item_view)) {
      throw std::length_error{"XML mixed fragment storage overflow"};
    }
    context->consume(xml::operation_resource::allocation_bytes, count * sizeof(mixed_item_view));
  }
  result.reserve(count);
  if (value.text_segments.empty()) {
    if (!value.children.empty() && !value.text.empty()) {
      throw std::invalid_argument{"XML mixed text requires ordered text segments"};
    }
    if (!value.text.empty()) { result.push_back({mixed_item_kind::text, value.text, nullptr}); }
    for (const auto& child : value.children) { result.push_back({mixed_item_kind::child, {}, &child}); }
  } else {
    std::size_t previous{}, text_offset{};
    for (const auto& segment : value.text_segments) {
      if (segment.before_child < previous || segment.before_child > value.children.size() ||
          segment.value.size() > value.text.size() - text_offset ||
          value.text.compare(text_offset, segment.value.size(), segment.value) != 0) {
        throw std::invalid_argument{"Invalid XML mixed text segment order or aggregate"};
      }
      previous = segment.before_child;
      text_offset += segment.value.size();
    }
    if (text_offset != value.text.size()) {
      throw std::invalid_argument{"XML mixed text segments do not cover aggregate text"};
    }
    std::size_t next_segment{};
    for (std::size_t child_index{}; child_index <= value.children.size(); ++child_index) {
      while (next_segment < value.text_segments.size() &&
          value.text_segments[next_segment].before_child == child_index) {
        result.push_back({mixed_item_kind::text, value.text_segments[next_segment++].value, nullptr});
      }
      if (child_index < value.children.size()) {
        result.push_back({mixed_item_kind::child, {}, &value.children[child_index]});
      }
    }
  }
  validate_mixed_trace(contract, result, limits, context);
  return result;
}

} // namespace rohit::serializer::writer::xml_binding
