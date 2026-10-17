// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/serializer.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer::xml {

// An expanded XML name; prefixes are presentation only and never part of identity.
struct name {
  std::string local{};
  std::string namespace_uri{};
  // Compare expanded names independently of document prefixes.
  bool operator==(const name&) const = default;
};

// One attribute; unprefixed attributes belong to the empty namespace.
struct attribute {
  name qualified_name{};
  std::string value{};
};

// Bounded data XML tree. Mixed text/element content is deliberately unsupported.
struct element {
  name qualified_name{};
  std::vector<attribute> attributes{};
  std::vector<element> children{};
  std::string text{};
  // In-scope prefix bindings are retained for QName-valued metadata such as XSD type names.
  std::map<std::string, std::string> namespace_bindings{};
};

// Shared budgets cover the complete document, including attribute and text bytes.
struct limits {
  std::size_t max_input_bytes{16U * 1024U * 1024U};
  std::size_t max_output_bytes{16U * 1024U * 1024U};
  std::size_t max_text_bytes{8U * 1024U * 1024U};
  std::size_t max_nodes{100000U};
  std::size_t max_depth{128U};
  std::size_t max_attributes{100000U};
  std::size_t max_name_bytes{8U * 1024U * 1024U};
  std::size_t max_namespace_bindings{100000U};
  // Logical retained tree storage, excluding provider buffers and allocator bookkeeping/capacity.
  std::size_t max_tree_bytes{64U * 1024U * 1024U};
};

// Report whether this linked runtime includes its optional XML provider.
[[nodiscard]] bool available() noexcept;
// Parse exactly one UTF-8 XML 1.0 document; reject DTDs, entities, mixed content and excess budgets.
[[nodiscard]] element parse(std::string_view document, limits bounds = {});
// Emit deterministic XML 1.0 UTF-8, with escaping and the same tree/resource checks.
[[nodiscard]] std::string write(const element& document, limits bounds = {});

// Prepared trusted standalone XSD 1.0. Includes/imports/redefines are rejected, never fetched.
class prepared_schema {
  struct implementation;
  std::unique_ptr<implementation> implementation_{};
public:
  // Compile one trusted in-memory schema; failures retain no prepared validator.
  explicit prepared_schema(std::string_view xsd, limits bounds = {});
  // Release the provider-owned prepared schema.
  ~prepared_schema();
  // Transfer ownership without copying provider state.
  prepared_schema(prepared_schema&&) noexcept;
  // Replace the prepared schema by transferring ownership.
  prepared_schema& operator=(prepared_schema&&) noexcept;
  prepared_schema(const prepared_schema&) = delete;
  prepared_schema& operator=(const prepared_schema&) = delete;
  // Validate a complete document. Each call owns its validation context; the schema is immutable.
  void validate(std::string_view document, limits bounds = {}) const;
};

namespace detail {
template <typename T> inline constexpr bool dependent_false = false;
template <typename T> struct optional_traits { static constexpr bool value = false; };
template <typename T> struct optional_traits<std::optional<T>> {
  static constexpr bool value = true;
  using type = T;
};
template <typename T> struct collection_traits { static constexpr bool value = false; };
template <typename T, typename Allocator> struct collection_traits<std::vector<T, Allocator>> {
  static constexpr bool value = true;
  static constexpr bool fixed = false;
  using type = T;
};
template <typename T, std::size_t N> struct collection_traits<std::array<T, N>> {
  static constexpr bool value = true;
  static constexpr bool fixed = true;
  using type = T;
};
// Trim XML whitespace around numeric/Boolean lexical values.
inline std::string_view trim(std::string_view value) {
  constexpr std::string_view whitespace{" \t\r\n"};
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string_view::npos) { return {}; }
  return value.substr(first, value.find_last_not_of(whitespace) - first + 1U);
}
// Convert supported scalar values without locale-sensitive formatting.
template <typename T> std::string scalar_text(const T& value) {
  if constexpr (std::same_as<T, std::string>) { return value; }
  else if constexpr (std::same_as<T, bool>) { return value ? "true" : "false"; }
  else if constexpr (std::is_enum_v<T>) {
    if constexpr (requires { serializer_enum_name(value); }) {
      return std::string{serializer_enum_name(value)};
    } else { return scalar_text(static_cast<std::underlying_type_t<T>>(value)); }
  } else if constexpr (std::integral<T> || std::floating_point<T>) {
    if constexpr (std::floating_point<T>) {
      if (std::isnan(value)) { return "NaN"; }
      if (std::isinf(value)) { return value < 0 ? "-INF" : "INF"; }
    }
    std::array<char, 128> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc{}) { throw std::invalid_argument{"XML scalar formatting failed"}; }
    return {buffer.data(), result.ptr};
  } else { static_assert(dependent_false<T>, "Unsupported XML scalar type"); }
}
// Decode the entire scalar lexical value; never narrow or accept trailing garbage.
template <typename T> void read_scalar(std::string_view text, T& value) {
  if constexpr (std::same_as<T, std::string>) { value = text; }
  else if constexpr (std::is_enum_v<T>) {
    if constexpr (requires { serializer_enum_from_name(value, text); }) {
      serializer_enum_from_name(value, text);
    } else {
      std::underlying_type_t<T> number{};
      read_scalar(text, number);
      value = static_cast<T>(number);
    }
  } else if constexpr (std::same_as<T, bool>) {
    text = trim(text);
    if (text == "true" || text == "1") { value = true; }
    else if (text == "false" || text == "0") { value = false; }
    else { throw std::invalid_argument{"Invalid XML Boolean"}; }
  } else if constexpr (std::integral<T> || std::floating_point<T>) {
    text = trim(text);
    if constexpr (std::floating_point<T>) {
      if (text == "INF") { value = std::numeric_limits<T>::infinity(); return; }
      if (text == "-INF") { value = -std::numeric_limits<T>::infinity(); return; }
      if (text == "NaN") { value = std::numeric_limits<T>::quiet_NaN(); return; }
    }
    if (!text.empty() && text.front() == '+') {
      text.remove_prefix(1U);
      if (!text.empty() && (text.front() == '-' || text.front() == '+')) {
        throw std::invalid_argument{"Invalid XML numeric sign"};
      }
    }
    if (text.empty()) { throw std::invalid_argument{"Empty XML number"}; }
    if constexpr (std::unsigned_integral<T>) {
      if (text.front() == '-') {
        const auto digits = text.substr(1U);
        if (!digits.empty() && digits.find_first_not_of('0') == std::string_view::npos) { value = 0; return; }
      }
    }
    if constexpr (std::floating_point<T>) {
      // from_chars also accepts spellings such as nan(payload); XML Schema does not.
      std::size_t offset = text.front() == '-' ? 1U : 0U;
      const auto digit = [&](std::size_t index) { return index < text.size() && text[index] >= '0' && text[index] <= '9'; };
      std::size_t digits{};
      while (digit(offset)) { ++offset; ++digits; }
      if (offset < text.size() && text[offset] == '.') {
        ++offset;
        while (digit(offset)) { ++offset; ++digits; }
      }
      if (digits == 0U) { throw std::invalid_argument{"Invalid XML floating-point lexical value"}; }
      if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E')) {
        ++offset;
        if (offset < text.size() && (text[offset] == '+' || text[offset] == '-')) { ++offset; }
        const auto exponent_start = offset;
        while (digit(offset)) { ++offset; }
        if (exponent_start == offset) { throw std::invalid_argument{"Invalid XML exponent"}; }
      }
      if (offset != text.size()) { throw std::invalid_argument{"Invalid XML floating-point lexical value"}; }
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
      throw std::invalid_argument{"Invalid or overflowing XML number"};
    }
  } else { static_assert(dependent_false<T>, "Unsupported XML scalar type"); }
}
} // namespace detail

// Bound the generated output tree while constructing it, before copying model strings/collections.
class build_budget {
  limits bounds_;
  std::size_t nodes_{};
  std::size_t attributes_{};
  std::size_t text_bytes_{};
  std::size_t name_bytes_{};
  std::size_t depth_{};
  std::size_t tree_bytes_{};
  // Charge one bounded counter before allocation or arithmetic overflow.
  static void charge(std::size_t& used, std::size_t amount, std::size_t maximum) {
    if (used > maximum || amount > maximum - used) { throw std::length_error{"XML output tree limit exceeded"}; }
    used += amount;
  }
public:
  // Bind complete-object budgets before building the first element.
  explicit build_budget(limits bounds = {}) : bounds_{bounds} {
    bounds_.max_text_bytes = std::min(bounds_.max_text_bytes, bounds_.max_output_bytes);
    bounds_.max_name_bytes = std::min(bounds_.max_name_bytes, bounds_.max_output_bytes);
  }
  // Restore lexical depth when a nested generated record/field finishes or throws.
  struct depth_guard {
    build_budget& owner;
    // Return to the parent level after completing this element.
    ~depth_guard() { --owner.depth_; }
    depth_guard(const depth_guard&) = delete;
    depth_guard& operator=(const depth_guard&) = delete;
    // Bind exactly one successful element entry.
    explicit depth_guard(build_budget& value) : owner{value} {}
  };
  // Charge an element and its name before allocating its tree storage.
  [[nodiscard]] depth_guard enter(const name& field_name) {
    if (depth_ >= bounds_.max_depth) { throw std::length_error{"XML output depth exceeded"}; }
    charge(nodes_, 1U, bounds_.max_nodes);
    charge(tree_bytes_, sizeof(element), bounds_.max_tree_bytes);
    charge(name_bytes_, field_name.local.size(), bounds_.max_name_bytes);
    charge(name_bytes_, field_name.namespace_uri.size(), bounds_.max_name_bytes);
    charge(tree_bytes_, field_name.local.size(), bounds_.max_tree_bytes);
    charge(tree_bytes_, field_name.namespace_uri.size(), bounds_.max_tree_bytes);
    ++depth_;
    return depth_guard{*this};
  }
  // Charge one scalar string before it is copied from the logical model.
  void text(std::size_t bytes) {
    charge(text_bytes_, bytes, bounds_.max_text_bytes);
    charge(tree_bytes_, bytes, bounds_.max_tree_bytes);
  }
  // Charge an attribute name/count before allocating its value.
  void attribute_name(const name& field_name) {
    charge(attributes_, 1U, bounds_.max_attributes);
    charge(tree_bytes_, sizeof(attribute), bounds_.max_tree_bytes);
    charge(name_bytes_, field_name.local.size(), bounds_.max_name_bytes);
    charge(name_bytes_, field_name.namespace_uri.size(), bounds_.max_name_bytes);
    charge(tree_bytes_, field_name.local.size(), bounds_.max_tree_bytes);
    charge(tree_bytes_, field_name.namespace_uri.size(), bounds_.max_tree_bytes);
  }
};

// Populate one scalar or statically bound record, retaining the caller-selected element name.
template <typename T> void set_value(element& node, const T& value, build_budget& budget) {
  if constexpr (requires { value.serialize_xml_out(node, budget); }) { value.serialize_xml_out(node, budget); }
  else {
    if constexpr (std::same_as<T, std::string>) { budget.text(value.size()); node.text = value; }
    else { auto text = detail::scalar_text(value); budget.text(text.size()); node.text = std::move(text); }
  }
}
// Decode one scalar or statically bound record, rejecting nested content in scalar slots.
template <typename T> void get_value(const element& node, T& value) {
  if constexpr (requires { value.serialize_xml_in(node); }) { value.serialize_xml_in(node); }
  else {
    if (!node.attributes.empty() || !node.children.empty()) {
      throw std::invalid_argument{"XML scalar contains attributes or children"};
    }
    detail::read_scalar(node.text, value);
  }
}
// Append an explicitly mapped scalar attribute; absent optionals are omitted.
template <typename T> void add_attribute(element& node, name field_name, const T& value, build_budget& budget) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { add_attribute(node, std::move(field_name), *value, budget); }
  } else {
    budget.attribute_name(field_name);
    if constexpr (std::same_as<T, std::string>) {
      budget.text(value.size()); node.attributes.push_back({std::move(field_name), value});
    } else {
      auto text = detail::scalar_text(value);
      budget.text(text.size()); node.attributes.push_back({std::move(field_name), std::move(text)});
    }
  }
}
// Append an element, absent optional, or contiguous repeated collection using static mapping.
template <typename T> void add_child(element& node, const name& field_name, const T& value, build_budget& budget) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { add_child(node, field_name, *value, budget); }
  } else if constexpr (detail::collection_traits<T>::value) {
    for (const auto& item : value) {
      if constexpr (std::same_as<typename detail::collection_traits<T>::type, bool>) {
        add_child(node, field_name, static_cast<bool>(item), budget);
      } else { add_child(node, field_name, item, budget); }
    }
  } else {
    const auto depth = budget.enter(field_name);
    element child{};
    child.qualified_name = field_name;
    set_value(child, value, budget);
    node.children.push_back(std::move(child));
  }
}

// Ordered field decoder enforces exact attribute membership and child sequence/cardinality.
class field_reader {
  const element& node_;
  std::vector<bool> used_attributes_;
  std::size_t next_child_{};
public:
  // Bind one record; indentation whitespace is accepted but mixed scalar text is rejected.
  explicit field_reader(const element& node)
      : node_{node}, used_attributes_(node.attributes.size(), false) {
    if (!detail::trim(node.text).empty()) { throw std::invalid_argument{"Unexpected XML record text"}; }
  }
  // Decode one required/optional attribute; duplicate attributes are rejected by the parser/writer.
  template <typename T> void attribute_value(const name& field_name, T& value) {
    for (std::size_t index{}; index < node_.attributes.size(); ++index) {
      if (node_.attributes[index].qualified_name != field_name) { continue; }
      if (used_attributes_[index]) { throw std::invalid_argument{"Repeated XML attribute mapping"}; }
      used_attributes_[index] = true;
      if constexpr (detail::optional_traits<T>::value) {
        typename detail::optional_traits<T>::type candidate{};
        detail::read_scalar(node_.attributes[index].value, candidate);
        value = std::move(candidate);
      } else { detail::read_scalar(node_.attributes[index].value, value); }
      return;
    }
    if constexpr (detail::optional_traits<T>::value) { value.reset(); }
    else { throw std::invalid_argument{"Required XML attribute missing"}; }
  }
  // Decode one scalar/record, optional, or repeated array in declared sequence order.
  template <typename T> void child_value(const name& field_name, T& value) {
    const auto present = [&] {
      return next_child_ < node_.children.size() &&
             node_.children[next_child_].qualified_name == field_name;
    };
    if constexpr (detail::optional_traits<T>::value) {
      if (!present()) { value.reset(); return; }
      typename detail::optional_traits<T>::type candidate{};
      get_value(node_.children[next_child_++], candidate);
      value = std::move(candidate);
    } else if constexpr (detail::collection_traits<T>::value) {
      if constexpr (detail::collection_traits<T>::fixed) {
        for (auto& item : value) {
          if (!present()) { throw std::invalid_argument{"XML fixed array item missing"}; }
          get_value(node_.children[next_child_++], item);
        }
      } else {
        value.clear();
        while (present()) {
          typename detail::collection_traits<T>::type candidate{};
          get_value(node_.children[next_child_++], candidate);
          value.push_back(std::move(candidate));
        }
      }
    } else {
      if (!present()) { throw std::invalid_argument{"Required XML child missing or out of order"}; }
      get_value(node_.children[next_child_++], value);
    }
  }
  // Reject every undeclared attribute and every unknown, duplicate or misplaced child.
  void finish() const {
    for (const auto used : used_attributes_) {
      if (!used) { throw std::invalid_argument{"Unexpected XML attribute"}; }
    }
    if (next_child_ != node_.children.size()) { throw std::invalid_argument{"Unexpected XML child"}; }
  }
};

// Encode one complete owning object, optionally validating its emitted XML with prepared XSD.
template <typename T> [[nodiscard]] std::string serialize(
    const T& value, limits bounds = {}, const prepared_schema* schema = nullptr) {
  element document{};
  document.qualified_name = T::xml_root_name();
  build_budget budget{bounds};
  const auto depth = budget.enter(document.qualified_name);
  set_value(document, value, budget);
  auto bytes = write(document, bounds);
  if (schema) {
    if constexpr (T::xml_runtime_xsd) {
      if constexpr (T::xml_validate_output) { schema->validate(bytes, bounds); }
    }
    else { throw std::invalid_argument{"Runtime XSD was not enabled during model generation"}; }
  }
  return bytes;
}
// Decode into a fresh candidate and return only after syntax, mapping and optional XSD succeed.
template <typename T> [[nodiscard]] T deserialize(
    std::string_view bytes, limits bounds = {}, const prepared_schema* schema = nullptr) {
  if (schema) {
    if constexpr (T::xml_runtime_xsd) {
      if constexpr (T::xml_validate_input) { schema->validate(bytes, bounds); }
    }
    else { throw std::invalid_argument{"Runtime XSD was not enabled during model generation"}; }
  }
  const auto document = parse(bytes, bounds);
  if (document.qualified_name != T::xml_root_name()) {
    throw std::invalid_argument{"Unexpected XML root name or namespace"};
  }
  T candidate{};
  get_value(document, candidate);
  return candidate;
}

// XML participates in the same complete-object stream/compression APIs as existing codecs.
template <serialize_type Direction, typename Stream = rohit::stream> class codec;

// Compose XML output with caller-owned buffers and existing compression adapters.
template <typename Stream> class codec<serialize_type::out, Stream> {
  Stream& output_;
  limits bounds_{};
public:
  using stream_type = Stream;
  template <typename OtherStream> using rebind_stream = codec<serialize_type::out, OtherStream>;
  static constexpr serialize_key_type key_type = serialize_key_type::none;
  static constexpr wire_format format = wire_format::xml;
  // Borrow the destination; encoding completes before any XML bytes are appended.
  explicit codec(Stream& output, limits bounds = {}) : output_{output}, bounds_{bounds} {}
  // Dispatch one statically bound object through its independent XML mapping.
  template <typename T> void xml_object(const T& value) {
    const auto bytes = serialize(value, bounds_);
    output_.append(bytes.data(), bytes.size());
  }
  // Preserve the common protocol-level object entry point.
  template <typename T> void serialize_out(const T& value) { xml_object(value); }
};

// Read exactly one bounded XML message and commit its fresh owning value on success.
template <typename Stream> class codec<serialize_type::in, Stream> {
  const Stream& input_;
  limits bounds_{};
public:
  using stream_type = Stream;
  template <typename OtherStream> using rebind_stream = codec<serialize_type::in, OtherStream>;
  static constexpr serialize_key_type key_type = serialize_key_type::none;
  static constexpr wire_format format = wire_format::xml;
  // Translate shared codec budgets to complete-document XML budgets.
  explicit codec(const Stream& input, decode_limits shared = {}) : input_{input} {
    bounds_.max_input_bytes = shared.max_input_bytes;
    bounds_.max_text_bytes = std::min(shared.max_string_bytes, shared.max_allocation_bytes);
    bounds_.max_nodes = shared.max_collection_elements;
    bounds_.max_attributes = shared.max_collection_elements;
    bounds_.max_namespace_bindings = shared.max_collection_elements;
    bounds_.max_name_bytes = shared.max_allocation_bytes;
    bounds_.max_tree_bytes = shared.max_allocation_bytes;
    bounds_.max_depth = shared.max_nesting_depth;
  }
  // Bind XML-specific complete-document limits without changing other codecs.
  codec(const Stream& input, limits bounds) : input_{input}, bounds_{bounds} {}
  // Decode and assign only after the entire input satisfies the generated mapping.
  template <typename T> void xml_object(T& value) {
    const auto size = input_.remaining_buffer();
    auto candidate = deserialize<T>({reinterpret_cast<const char*>(input_.curr()), size}, bounds_);
    value = std::move(candidate);
    input_.get_curr_and_increase_unchecked(size);
  }
  // Preserve the common protocol-level object entry point.
  template <typename T> void serialize_in(T& value) { xml_object(value); }
  // Reject callers that attempt to leave any bytes outside the selected XML object.
  void finish() const {
    if (!input_.full()) { throw std::invalid_argument{"Trailing XML input"}; }
  }
};
} // namespace rohit::serializer::xml
