// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml_event_reader.hpp>
#include <rohit/xml_identity.hpp>
#include <rohit/xml_extensions.hpp>
#include <rohit/xml_lexical_contract.hpp>
#include <rohit/digest.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer::xml {

// Direct XML output retains only open names and attributes, never an element tree.
// A failed pass cannot publish bytes or be reused; calls are not thread-safe.
class token_writer {
  struct implementation;
  std::unique_ptr<implementation> state_;
public:
  // Bind the complete output bounds and borrow an optional operation context.
  explicit token_writer(limits bounds = {}, operation_context* context = nullptr);
  // Release unfinished or failed output without publishing it.
  ~token_writer();
  token_writer(const token_writer&) = delete;
  token_writer& operator=(const token_writer&) = delete;
  // Begin an element; attributes must precede its first child or text token.
  void open(const name& selected);
  // Retain lexical QName context for an opaque or explicitly namespaced value without prefix collisions.
  void open(const name& selected, const std::map<std::string, std::string>& namespaces);
  // Verify namespace-sensitive union identity against the actual open element's emitted scope.
  void validate_namespace_value(std::string_view lexical, const element& original_scope,
      const lexical_contract& contract);
  // Emit one ordinary expanded attribute, rejecting duplicate expanded names.
  void attribute_value(const name& selected, std::string_view value);
  // Emit a QName-valued control attribute with an owned, unambiguous local namespace binding.
  void qname_attribute_value(const name& selected, const name& value);
  // Check one generated owner scope during emission; register before its first attribute/content.
  void begin_identity(const identity_scope& scope);
  // Select explicit local bounds instead of the optional operation-context identity policy.
  void begin_identity(const identity_scope& scope, identity_limits bounds);
  // Append decoded text with XML escaping and bounded UTF-8 validation.
  void text(std::string_view value);
  // Finish the current element and restore its parent's namespace scope.
  void close();
  // Publish exactly one complete document; unclosed or failed passes throw.
  [[nodiscard]] std::string finish();
  // Borrow the shared operation context for lexical adapters.
  [[nodiscard]] operation_context* context() const noexcept;
  // Inspect immutable document/map bounds shared by nested static encoders.
  [[nodiscard]] const limits& bounds() const noexcept;
};

// An owned copy of one normalized event; it never owns descendants.
struct token {
  event_kind kind{event_kind::start_document};
  name qualified_name{};
  std::vector<attribute> attributes{};
  std::string text{};
  std::map<std::string, std::string> namespaces{};
  std::optional<attribute> consumed_nil{};
  std::optional<attribute> consumed_type{};
  std::vector<attribute> consumed_attributes{};
};

// Pull adapter over the incremental checked event producer. Input remains borrowed until
// destruction; a bounded input chunk and one scalar may be buffered, never a document tree.
class token_reader {
  struct implementation;
  std::unique_ptr<implementation> state_;
public:
  // Select a positive byte chunk size to exercise arbitrary UTF-8/token boundaries.
  explicit token_reader(std::string_view input, limits bounds = {},
      operation_context* context = nullptr, std::size_t chunk_bytes = default_token_chunk_bytes);
  // Release the producer and any unconsumed tokens.
  ~token_reader();
  token_reader(const token_reader&) = delete;
  token_reader& operator=(const token_reader&) = delete;
  // Inspect the current token; references expire at the next advance.
  [[nodiscard]] const token& current() const;
  // Consume the current token, incrementally producing its successor.
  void advance();
  // Skip XML whitespace between mapped children, preserving scalar whitespace.
  void skip_space();
  // Remove an authorized xsi:nil marker from the current start token; malformed values reject.
  [[nodiscard]] bool take_nil(bool allowed);
  // Resolve and remove optional xsi:type using the current complete namespace scope.
  [[nodiscard]] std::optional<name> take_type();
  // Hide an authorized attribute from ordinary field matching while keeping identity observers complete.
  void consume_attribute(const name& selected);
  // Copy the current complete lexical namespace scope, admitting optional retained storage first.
  [[nodiscard]] std::map<std::string, std::string> namespace_scope(build_budget* budget = nullptr) const;
  // New retained child snapshots cumulatively share max_name_bytes, independently of callbacks/tree bounds.
  [[nodiscard]] std::map<std::string, std::string> namespace_scope(build_budget* budget,
      bool retain_child_scope) const;
  // Count already consumed direct children of the current open owner, excluding nested descendants.
  [[nodiscard]] std::size_t child_position() const;
  // Collect allocator-family prefixes before schema defaults can declare a previously unbound alias.
  void collect_namespace_prefixes();
  // Borrow the normalized document inventory after opting in; it remains owned by this reader.
  [[nodiscard]] const std::set<std::string>& reserved_namespace_prefixes() const noexcept;
  // Observe this current start token and its descendants through exact local-scope completion.
  void begin_identity(const identity_scope& scope);
  // Select explicit local bounds instead of the optional operation-context identity policy.
  void begin_identity(const identity_scope& scope, identity_limits bounds);
  // Require checked byte EOF after the root, rejecting every trailing document.
  void finish();
  // Borrow the same context used by the incremental parser.
  [[nodiscard]] operation_context* context() const noexcept;
  // Inspect immutable document/map bounds shared by nested static decoders.
  [[nodiscard]] const limits& bounds() const noexcept;
};

enum class token_presence { required, absent, nil, absent_or_nil };

// Stream known fields while retaining only explicitly preserved foreign attributes/subtrees.
// Metadata and the reader are borrowed; publish the target only after scope and document completion.
class token_extension_input {
  struct implementation;
  std::unique_ptr<implementation> state_;
public:
  // Select per-operation bounds/capabilities and consume permitted foreign owner attributes.
  token_extension_input(token_reader& reader, const extension_contract& contract, extension_slot& target);
  ~token_extension_input();
  token_extension_input(const token_extension_input&) = delete;
  token_extension_input& operator=(const token_extension_input&) = delete;
  // Consume only foreign children explicitly admitted at this exact generated field boundary.
  void before_field(std::size_t index);
  // Check final positions/minimum counts and publish the complete opaque storage candidate.
  void finish(std::size_t end_index);
};

// Borrow retained opaque storage and emit it among directly traversed known fields.
class token_extension_output {
  token_writer& writer_;
  const extension_store& retained_;
  std::size_t next_child_{};
public:
  // Validate complete retained content and capabilities before any extension attribute is emitted.
  token_extension_output(token_writer& writer, const extension_contract& contract, const extension_store& retained);
  // Emit preserved children at their exact generated field boundary.
  void before_field(std::size_t index);
  // Require every retained child to belong to an existing field/end boundary.
  void finish(std::size_t end_index);
};

namespace detail {
// Borrow a stable owner scope while a scalar child checks its original lexical QName context.
struct token_namespace_projection {
  const element* owner{};
  bool list{}, prefixes{}, allow_empty{};
  const lexical_contract* contract{};
  std::vector<element>* retained{};
  const lexical_name_list* schema_default{};
};

// Borrow per-occurrence scopes while recursively traversing one optional/repeated scalar field.
struct token_namespace_output {
  std::span<const element> scopes{};
  std::size_t occurrence{};
  const lexical_contract* contract{};
  const element* owner{};
};
// Keep digest byte storage scalar even though its native representation is a standard collection.
template <typename T> inline constexpr bool digest_byte_storage = [] {
  if constexpr (collection_traits<T>::value) {
    return std::same_as<typename collection_traits<T>::type, std::uint8_t>;
  } else { return false; }
}();

// Check byte-pair count and fixed extent before allocation; reject malformed hex transactionally.
template <typename T> void read_token_digest(std::string_view lexical, T& value,
    const limits& bounds, operation_context* context) {
  if (lexical.size() % 2U || lexical.size() / 2U > bounds.max_digest_bytes) {
    if (!(lexical.size() % 2U) && context && context->limit_location) {
      context->reject_limit(lexical.size() / 2U, bounds.max_digest_bytes, "XML digest byte limit exceeded");
    }
    throw std::invalid_argument{"XML digest requires bounded hexadecimal byte pairs"};
  }
  const auto count = lexical.size() / 2U;
  T candidate{};
  if constexpr (collection_traits<T>::fixed) {
    if (count != candidate.size()) { throw std::invalid_argument{"XML digest has an incorrect fixed extent"}; }
  } else {
    if (context) { context->consume(operation_resource::allocation_bytes, count); }
    candidate.resize(count);
  }
  if (context) { context->consume(operation_resource::work_units, lexical.size()); }
  if (!::rohit::digest_from_hex(lexical, candidate)) { throw std::invalid_argument{"Invalid XML digest hexadecimal bytes"}; }
  value = std::move(candidate);
}

// Admit the hexadecimal text allocation before formatting exact bytes; no hashing is performed.
template <typename T> std::string token_digest_text(const T& value, const limits& bounds,
    operation_context* context) {
  if (value.size() > bounds.max_digest_bytes || value.size() > std::numeric_limits<std::size_t>::max() / 2U) {
    reject_limit(context, "XML digest byte limit exceeded", value.size(), bounds.max_digest_bytes);
  }
  if (context) {
    context->consume(operation_resource::allocation_bytes, value.size() * 2U);
    context->consume(operation_resource::work_units, value.size());
  }
  return ::rohit::digest_to_hex(value);
}

// Model revisions use canonical schema literals, rather than the broader ordinary XML numeric lexemes.
template <typename T> void read_model_revision(std::string_view text, T& value) {
  if constexpr (requires { value.to_string(); T::parse(text); }) { value = T::parse(text); }
  else {
    std::size_t position{};
    const auto digits = [&] {
      const auto start = position;
      while (position < text.size() && text[position] >= '0' && text[position] <= '9') { ++position; }
      return position != start;
    };
    if (!digits() || (position > 1U && text.front() == '0')) { throw std::invalid_argument{"Invalid canonical XML model revision"}; }
    if constexpr (std::floating_point<T>) {
      if (position < text.size() && text[position] == '.') {
        ++position;
        if (!digits()) { throw std::invalid_argument{"Invalid fractional XML model revision"}; }
      }
      if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
        ++position;
        if (position < text.size() && (text[position] == '+' || text[position] == '-')) { ++position; }
        if (!digits()) { throw std::invalid_argument{"Invalid exponent in XML model revision"}; }
      }
    }
    if (position != text.size()) { throw std::invalid_argument{"Invalid canonical XML model revision"}; }
    read_scalar(text, value);
    if (!::rohit::serializer::detail::valid_version(value)) { throw std::invalid_argument{"Invalid finite XML model revision"}; }
  }
}

// Discard unsupported generated branches before their deliberately rejecting methods instantiate.
template <typename T> consteval bool token_mapping_available() {
  if constexpr (requires { T::xml_tokens_supported; }) { return T::xml_tokens_supported; }
  else { return true; }
}

// Account the owning string copy separately from a queued token or accumulated lexical value.
template <typename T> void charge_token_scalar(operation_context* context, std::size_t bytes) {
  if constexpr (std::same_as<T, std::string>) {
    if (context) { context->consume(operation_resource::allocation_bytes, bytes); }
  }
}

// Keep an explicit empty value from activating a nonempty XSD element default.
// Collapsed whitespace has a nonempty lexical representation of the empty value;
// preserve/replace strings have none and must reject before publishing output.
inline std::string_view token_element_lexical(std::string_view value,
    std::optional<std::string_view> empty_default, whitespace_policy whitespace) {
  if (value.empty() && empty_default && !empty_default->empty()) {
    if (whitespace == whitespace_policy::collapse) { return " "; }
    throw std::invalid_argument{"Empty XML value cannot preserve its nonempty element default"};
  }
  return value;
}

// Bound lexical items before any native list element allocation, independently of XML node count.
inline void check_token_list(std::string_view lexical, const limits& bounds, operation_context* context = nullptr) {
  std::size_t count{};
  auto remaining = trim(lexical);
  while (!remaining.empty()) {
    if (count >= bounds.max_nodes) { reject_limit(context, "XML list item limit exceeded", 1U, 0U); }
    ++count;
    const auto separator = remaining.find_first_of(" \t\r\n");
    remaining = separator == std::string_view::npos ? std::string_view{} : trim(remaining.substr(separator));
  }
}

// Admit a borrowed native list before lexical validation or joining allocates its output spelling.
template <typename T> void check_token_list_storage(const T& value, const limits& bounds, operation_context* context = nullptr) {
  if constexpr (optional_traits<T>::value) {
    if (value) { check_token_list_storage(*value, bounds, context); }
  } else if (value.size() > bounds.max_nodes) { reject_limit(context, "XML list item limit exceeded", value.size(), bounds.max_nodes); }
}
} // namespace detail

// Emit one scalar, borrowed projection, or generated record directly into an open element.
template <typename T> void write_token_value(token_writer& writer, const T& value,
    std::optional<std::string_view> empty_default = {}, whitespace_policy whitespace = whitespace_policy::preserve) {
  (void)empty_default; (void)whitespace;
  if constexpr (requires { typename T::reference_value_type; }) {
    value.with_value([&](const auto& selected) { write_token_value(writer, selected, empty_default, whitespace); });
  } else if constexpr (requires { value.serialize_xml_tokens_out(writer); }) {
    value.serialize_xml_tokens_out(writer);
  } else if constexpr (requires(element& node, build_budget& budget) { value.serialize_xml_out(node, budget); }) {
    throw std::invalid_argument{"This generated XML contract has no direct token mapping"};
  } else if constexpr (std::same_as<T, std::string>) { writer.text(detail::token_element_lexical(value, empty_default, whitespace)); }
  else { writer.text(detail::scalar_text(value)); }
}

// Omit absent optional attributes while preserving empty present string values.
template <typename T> void write_token_attribute(token_writer& writer,
    const name& selected, const T& value) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { write_token_attribute(writer, selected, *value); }
  } else if constexpr (requires { typename T::reference_value_type; }) {
    value.with_value([&](const auto& retained) { write_token_attribute(writer, selected, retained); });
  } else if constexpr (std::same_as<T, std::string>) { writer.attribute_value(selected, value); }
  else { writer.attribute_value(selected, detail::scalar_text(value)); }
}

// Format one scalar-list attribute, preserving an optional container's absent state.
template <typename T> void write_token_list_attribute(token_writer& writer,
    const name& selected, const T& values) {
  if constexpr (detail::optional_traits<T>::value) {
    if (values) { write_token_list_attribute(writer, selected, *values); }
  } else {
    if (values.size() > writer.bounds().max_nodes) { reject_limit(writer.context(), "XML list item limit exceeded", values.size(), writer.bounds().max_nodes); }
    writer.attribute_value(selected, format_list(values, writer.context()));
  }
}

// Encode one lexical list without treating its native collection as repeated XML elements.
template <typename T> void write_token_list_value(token_writer& writer, const T& values,
    std::optional<std::string_view> empty_default = {}, whitespace_policy whitespace = whitespace_policy::preserve) {
  if (values.size() > writer.bounds().max_nodes) { reject_limit(writer.context(), "XML list item limit exceeded", values.size(), writer.bounds().max_nodes); }
  write_token_value(writer, format_list(values, writer.context()), empty_default, whitespace);
}

// Keep a missing optional list distinct from one present empty lexical list element.
template <typename T> void write_token_list_child(token_writer& writer, const name& selected, const T& values,
    std::optional<std::string_view> empty_default, whitespace_policy whitespace,
    detail::token_namespace_output* namespaces) {
  if constexpr (detail::optional_traits<T>::value) {
    if (values) { write_token_list_child(writer, selected, *values, empty_default, whitespace, namespaces); }
  } else {
    const element* original_scope = namespaces ? namespaces->owner : nullptr;
    if (namespaces && namespaces->occurrence < namespaces->scopes.size()) {
      original_scope = &namespaces->scopes[namespaces->occurrence++];
      writer.open(selected, original_scope->namespace_bindings);
    } else { writer.open(selected); }
    write_token_list_value(writer, values, empty_default, whitespace);
    writer.close();
  }
}

// Preserve the original list-child API for callers without retained per-occurrence scopes.
template <typename T> void write_token_list_child(token_writer& writer, const name& selected, const T& values,
    std::optional<std::string_view> empty_default = {}, whitespace_policy whitespace = whitespace_policy::preserve) {
  write_token_list_child(writer, selected, values, empty_default, whitespace, nullptr);
}

// Traverse optionals and contiguous repeated values without allocating intermediate nodes.
template <typename T> void write_token_child(token_writer& writer,
    const name& selected, const T& value, std::optional<std::string_view> empty_default,
    whitespace_policy whitespace, detail::token_namespace_output* namespaces) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { write_token_child(writer, selected, *value, empty_default, whitespace, namespaces); }
  } else if constexpr (requires { typename T::reference_value_type; }) {
    // A weak owner must remain pinned while choosing lexical scope and visiting
    // the entire payload, including callbacks that can release external owners.
    value.with_value([&](const auto& retained) { write_token_child(writer, selected, retained, empty_default, whitespace, namespaces); });
  } else if constexpr (detail::collection_traits<T>::value) {
    for (const auto& item : value) {
      if constexpr (std::same_as<typename detail::collection_traits<T>::type, bool>) {
        write_token_child(writer, selected, static_cast<bool>(item), empty_default, whitespace, namespaces);
      } else { write_token_child(writer, selected, item, empty_default, whitespace, namespaces); }
    }
  } else {
    const element* original_scope = namespaces ? namespaces->owner : nullptr;
    if (namespaces && namespaces->occurrence < namespaces->scopes.size()) {
      original_scope = &namespaces->scopes[namespaces->occurrence++];
      writer.open(selected, original_scope->namespace_bindings);
    } else if constexpr (requires { value.xml_token_namespace_bindings(); }) {
      writer.open(selected, value.xml_token_namespace_bindings());
    } else { writer.open(selected); }
    if (namespaces && namespaces->contract && original_scope) {
      if constexpr (std::same_as<T, std::string>) {
        writer.validate_namespace_value(value, *original_scope, *namespaces->contract);
      }
    }
    write_token_value(writer, value, empty_default, whitespace);
    writer.close();
  }
}

// Preserve the original child API and its ordinary allocation-free traversal.
template <typename T> void write_token_child(token_writer& writer,
    const name& selected, const T& value, std::optional<std::string_view> empty_default = {},
    whitespace_policy whitespace = whitespace_policy::preserve) {
  write_token_child(writer, selected, value, empty_default, whitespace, nullptr);
}

// Encode digest bytes as one hexadecimal scalar, with ordinary optional/repeated child placement.
template <typename T> void write_token_digest_child(token_writer& writer, const name& selected, const T& value) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { write_token_digest_child(writer, selected, *value); }
  } else if constexpr (detail::digest_byte_storage<T>) {
    writer.open(selected);
    writer.text(detail::token_digest_text(value, writer.bounds(), writer.context()));
    writer.close();
  } else {
    for (const auto& item : value) { write_token_digest_child(writer, selected, item); }
  }
}

// Keep absent optional digest attributes distinct from an empty present raw-byte digest.
template <typename T> void write_token_digest_attribute(token_writer& writer, const name& selected, const T& value) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { write_token_digest_attribute(writer, selected, *value); }
  } else { writer.attribute_value(selected, detail::token_digest_text(value, writer.bounds(), writer.context())); }
}

// Preserve absent/nil/value independently of an empty string or empty collection wrapper.
template <typename T> void write_token_presence(token_writer& writer, const name& selected,
    const T& value, token_presence presence, const name& wrapper_item,
    std::optional<std::string_view> empty_default, whitespace_policy whitespace,
    detail::token_namespace_output* namespaces) {
  const auto nil = [&] {
    writer.open(selected);
    writer.attribute_value({"nil", "http://www.w3.org/2001/XMLSchema-instance"}, "true");
    writer.close();
  };
  const auto present = [&](const auto& item) {
    if (wrapper_item.local.empty()) { write_token_child(writer, selected, item, empty_default, whitespace, namespaces); }
    else {
      writer.open(selected);
      write_token_child(writer, wrapper_item, item, empty_default, whitespace, namespaces);
      writer.close();
    }
  };
  if constexpr (detail::optional_traits<T>::value) {
    using inner_type = typename detail::optional_traits<T>::type;
    if constexpr (detail::optional_traits<inner_type>::value) {
      if (value) {
        if (*value) { present(**value); }
        else { nil(); }
      }
    } else {
      if (value) { present(*value); }
      else if (presence == token_presence::nil) { nil(); }
    }
  } else { present(value); }
}

// Keep the established explicit-presence entrypoint available with its original function signature.
template <typename T> void write_token_presence(token_writer& writer, const name& selected,
    const T& value, token_presence presence, const name& wrapper_item = {},
    std::optional<std::string_view> empty_default = {}, whitespace_policy whitespace = whitespace_policy::preserve) {
  write_token_presence(writer, selected, value, presence, wrapper_item, empty_default, whitespace, nullptr);
}

// Check item cardinality only for a present collection; absent and nil states have no item sequence.
template <typename T> void check_token_occurrence(const T& value, std::size_t minimum,
    std::optional<std::size_t> maximum) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { check_token_occurrence(*value, minimum, maximum); }
  } else {
    if (value.size() < minimum || (maximum && value.size() > *maximum)) {
      throw std::invalid_argument{"XML collection occurrence failed"};
    }
  }
}

// Emit a declared map wrapper with complete statically named key/value entries.
template <typename Map> void write_token_map(token_writer& writer, const name& wrapper,
    const name& entry, const name& key_name, const name& value_name, bool attribute_key,
    const Map& values) {
  if constexpr (detail::optional_traits<Map>::value) {
    if (values) { write_token_map(writer, wrapper, entry, key_name, value_name, attribute_key, *values); }
  } else {
    if (values.size() > writer.bounds().max_map_entries) {
      reject_limit(writer.context(), "XML map entry limit exceeded", values.size(), writer.bounds().max_map_entries);
    }
    writer.open(wrapper);
    std::size_t key_bytes{};
    for (const auto& [key, value] : values) {
      const auto lexical = [&] {
        if constexpr (std::same_as<typename Map::key_type, std::string>) { return std::string_view{key}; }
        else { return detail::scalar_text(key); }
      }();
      if (lexical.size() > writer.bounds().max_map_key_bytes - key_bytes) {
        reject_limit(writer.context(), "XML map key byte limit exceeded", lexical.size(), writer.bounds().max_map_key_bytes - key_bytes);
      }
      key_bytes += lexical.size();
      writer.open(entry);
      if (attribute_key) { write_token_attribute(writer, key_name, key); }
      else { write_token_child(writer, key_name, key); }
      write_token_child(writer, value_name, value);
      writer.close();
    }
    writer.close();
  }
}

// Decode an open scalar or generated record, rejecting attributes and children on scalars.
template <typename T> void read_token_value(token_reader& reader, T& value,
    std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
    detail::token_namespace_projection projection = {}) {
  if constexpr (requires { typename T::reference_value_type; }) {
    (void)reader; (void)value;
    throw std::invalid_argument{"XML value references are export-only; decode an owning value"};
  } else if constexpr (requires { value.serialize_xml_tokens_in(reader); }) {
    value.serialize_xml_tokens_in(reader);
  } else if constexpr (requires(const element& node) { value.serialize_xml_in(node); }) {
    throw std::invalid_argument{"This generated XML contract has no direct token mapping"};
  } else {
    if (reader.current().kind != event_kind::start_element || !reader.current().attributes.empty()) {
      throw std::invalid_argument{"XML scalar requires an element without attributes"};
    }
    element scalar_scope{};
    if (projection.owner) {
      if (reader.context()) {
        reader.context()->consume(operation_resource::allocation_bytes,
            reader.current().qualified_name.local.size() + reader.current().qualified_name.namespace_uri.size());
      }
      scalar_scope.qualified_name = reader.current().qualified_name;
      scalar_scope.namespace_bindings = reader.namespace_scope(nullptr, projection.retained != nullptr);
    }
    reader.advance();
    std::string lexical{};
    while (reader.current().kind == event_kind::text) {
      const auto& part = reader.current().text;
      if (reader.context()) { reader.context()->consume(operation_resource::allocation_bytes, part.size()); }
      lexical += part;
      reader.advance();
    }
    if (reader.current().kind != event_kind::end_element) {
      throw std::invalid_argument{"XML scalar contains nested content"};
    }
    if (projection.owner) {
      scalar_scope.text = std::move(lexical);
      if (scalar_scope.text.empty() && projection.schema_default) {
        scalar_scope.text = qualify_qname_list(projection.schema_default->items, scalar_scope, reader.context(),
            &reader.reserved_namespace_prefixes());
      }
      if (projection.retained) {
        retain_child_namespace_scope(*projection.retained, scalar_scope, reader.context());
      } else if (projection.contract) {
        validate_child_lexical_projection(std::span{&scalar_scope, 1U}, scalar_scope.qualified_name,
            *projection.owner, *projection.contract, reader.context());
      } else {
        validate_child_namespace_projection(std::span{&scalar_scope, 1U}, scalar_scope.qualified_name,
            *projection.owner, projection.list, projection.prefixes, projection.allow_empty, reader.context());
      }
    }
    const auto text = projection.owner ? std::string_view{scalar_scope.text} : std::string_view{lexical};
    const auto selected = text.empty() && empty_default ? *empty_default : text;
    if (validator) { validator(selected, scalar_scope, reader.context()); }
    detail::charge_token_scalar<T>(reader.context(), selected.size());
    detail::read_scalar(selected, value);
    reader.advance();
  }
}

// Ordered generated record traversal retains only the current element's attributes.
class token_fields {
  struct all_field {
    name selected{};
    std::function<void()> decode{};
    bool required{};
    bool seen{};
  };
  token_reader& reader_;
  std::vector<attribute> attributes_{};
  std::vector<bool> used_{};
  std::vector<all_field> all_fields_{};
  const element* lexical_scope_{};
  // Borrow the caller's shallow lexical context without allocating an empty fallback per scalar.
  [[nodiscard]] const element& lexical_scope() const noexcept {
    static const element empty{};
    return lexical_scope_ ? *lexical_scope_ : empty;
  }
public:
  // Enter the current start element and retain its bounded attribute set.
  explicit token_fields(token_reader& reader, const element* lexical_scope = nullptr);
  // Deferred all-group callbacks borrow this reader and require its stable address.
  token_fields(const token_fields&) = delete;
  token_fields& operator=(const token_fields&) = delete;
  token_fields(token_fields&&) = delete;
  token_fields& operator=(token_fields&&) = delete;
  // Inspect original attribute presence independently of its empty value or a materialized default.
  [[nodiscard]] bool has_attribute(const name& selected) const noexcept {
    return std::any_of(attributes_.begin(), attributes_.end(),
        [&](const auto& item) { return item.qualified_name == selected; });
  }
  // Decode one mapped attribute and account exact membership and optional absence.
  template <typename T> void attribute_value(const name& selected, T& value,
      std::optional<std::string_view> absent_default = {}, lexical_validator validator = nullptr,
      const element* default_scope = nullptr) {
    for (std::size_t index{}; index < attributes_.size(); ++index) {
      if (attributes_[index].qualified_name == selected) {
        if (used_[index]) { throw std::invalid_argument{"XML attribute mapped more than once"}; }
        used_[index] = true;
        if (validator) { validator(attributes_[index].value, lexical_scope(), reader_.context()); }
        if constexpr (detail::optional_traits<T>::value) {
          typename detail::optional_traits<T>::type item{};
          detail::charge_token_scalar<typename detail::optional_traits<T>::type>(reader_.context(), attributes_[index].value.size());
          detail::read_scalar(attributes_[index].value, item);
          value = std::move(item);
        } else {
          detail::charge_token_scalar<T>(reader_.context(), attributes_[index].value.size());
          detail::read_scalar(attributes_[index].value, value);
        }
        return;
      }
    }
    if (absent_default) {
      if (validator) { validator(*absent_default, default_scope ? *default_scope : lexical_scope(), reader_.context()); }
      if constexpr (detail::optional_traits<T>::value) {
        typename detail::optional_traits<T>::type item{};
        detail::charge_token_scalar<typename detail::optional_traits<T>::type>(reader_.context(), absent_default->size());
        detail::read_scalar(*absent_default, item); value = std::move(item);
      } else {
        detail::charge_token_scalar<T>(reader_.context(), absent_default->size());
        detail::read_scalar(*absent_default, value);
      }
    } else {
      if constexpr (detail::optional_traits<T>::value) { value.reset(); }
      else { throw std::invalid_argument{"Missing required XML attribute"}; }
    }
  }
  // Decode consecutive children with exact fixed-array cardinality and ordered optional fields.
  template <typename T> void child_value(const name& selected, T& value,
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    reader_.skip_space();
    const auto matches = [&] {
      return reader_.current().kind == event_kind::start_element && reader_.current().qualified_name == selected;
    };
    if constexpr (detail::optional_traits<T>::value) {
      value.reset();
      if (matches()) {
        typename detail::optional_traits<T>::type item{};
        read_token_value(reader_, item, empty_default, validator, projection);
        value = std::move(item);
      }
    } else if constexpr (detail::collection_traits<T>::value) {
      T candidate{};
      std::size_t count{};
      while (matches()) {
        typename detail::collection_traits<T>::type item{};
        if constexpr (detail::collection_traits<T>::fixed) {
          if (count >= candidate.size()) { throw std::invalid_argument{"Too many XML collection values"}; }
          read_token_value(reader_, item, empty_default, validator, projection);
          candidate[count] = std::move(item);
        } else {
          if (reader_.context()) { reader_.context()->consume(operation_resource::allocation_bytes, sizeof(item)); }
          read_token_value(reader_, item, empty_default, validator, projection);
          candidate.push_back(std::move(item));
        }
        ++count;
        reader_.skip_space();
      }
      if constexpr (detail::collection_traits<T>::fixed) {
        if (count != candidate.size()) { throw std::invalid_argument{"Too few XML collection values"}; }
      }
      value = std::move(candidate);
    } else {
      if (!matches()) { throw std::invalid_argument{"Missing or out-of-order XML child"}; }
      read_token_value(reader_, value, empty_default, validator, projection);
    }
  }
  // Decode exact hexadecimal bytes without treating a digest's native byte array as repeated XML.
  template <typename T> void digest_child_value(const name& selected, T& value) {
    reader_.skip_space();
    const auto matches = [&] {
      return reader_.current().kind == event_kind::start_element && reader_.current().qualified_name == selected;
    };
    if constexpr (detail::optional_traits<T>::value) {
      value.reset();
      if (matches()) {
        typename detail::optional_traits<T>::type candidate{};
        digest_child_value(selected, candidate);
        value = std::move(candidate);
      }
    } else if constexpr (detail::digest_byte_storage<T>) {
      if (!matches()) { throw std::invalid_argument{"Missing or out-of-order XML digest child"}; }
      std::string lexical{};
      read_token_value(reader_, lexical);
      detail::read_token_digest(lexical, value, reader_.bounds(), reader_.context());
    } else {
      T candidate{};
      std::size_t count{};
      while (matches()) {
        typename detail::collection_traits<T>::type item{};
        if constexpr (detail::collection_traits<T>::fixed) {
          if (count >= candidate.size()) { throw std::invalid_argument{"Too many XML digest collection items"}; }
        } else if (reader_.context()) { reader_.context()->consume(operation_resource::allocation_bytes, sizeof(item)); }
        digest_child_value(selected, item);
        if constexpr (detail::collection_traits<T>::fixed) { candidate[count] = std::move(item); }
        else { candidate.push_back(std::move(item)); }
        ++count;
        reader_.skip_space();
      }
      if constexpr (detail::collection_traits<T>::fixed) {
        if (count != candidate.size()) { throw std::invalid_argument{"Too few XML digest collection items"}; }
      }
      value = std::move(candidate);
    }
  }
  // Decode one required or optional hexadecimal digest attribute with exact membership accounting.
  template <typename T> void digest_attribute_value(const name& selected, T& value) {
    if constexpr (detail::optional_traits<T>::value) {
      std::optional<std::string> lexical{};
      attribute_value(selected, lexical);
      if (!lexical) { value.reset(); return; }
      typename detail::optional_traits<T>::type candidate{};
      detail::read_token_digest(*lexical, candidate, reader_.bounds(), reader_.context());
      value = std::move(candidate);
    } else {
      std::string lexical{};
      attribute_value(selected, lexical);
      detail::read_token_digest(lexical, value, reader_.bounds(), reader_.context());
    }
  }
  // Decode simpleContent digest bytes separately from the enclosing record's mapped attributes.
  template <typename T> void digest_text_value(T& value) {
    std::string lexical{};
    text_value(lexical);
    detail::read_token_digest(lexical, value, reader_.bounds(), reader_.context());
  }
  // Register a singleton digest in the same deferred unordered-child traversal as ordinary fields.
  template <typename T> void all_digest_child_value(const name& selected, T& value) {
    if (reader_.context()) { reader_.context()->consume(operation_resource::allocation_bytes, sizeof(all_field) + selected.local.size() + selected.namespace_uri.size()); }
    all_fields_.push_back({selected, [this, &value, selected] { digest_child_value(selected, value); }, !detail::optional_traits<T>::value, false});
  }
  // Traverse an explicit collection wrapper, retaining it even for empty collections.
  template <typename T> void wrapped_child_value(const name& wrapper, const name& selected, T& value,
      lexical_validator validator = nullptr, detail::token_namespace_projection projection = {}) {
    reader_.skip_space();
    if (reader_.current().kind != event_kind::start_element || reader_.current().qualified_name != wrapper) {
      throw std::invalid_argument{"Missing XML collection wrapper"};
    }
    token_fields fields{reader_};
    fields.child_value(selected, value, {}, validator, projection);
    fields.finish();
  }
  // Decode simpleContent while separately retaining mapped attributes.
  template <typename T> void text_value(T& value, lexical_validator validator = nullptr,
      std::optional<std::string_view> empty_default = {}, const lexical_name_list* schema_default = nullptr,
      element* schema_scope = nullptr) {
    std::string lexical{};
    while (reader_.current().kind == event_kind::text) {
      const auto& part = reader_.current().text;
      if (reader_.context()) { reader_.context()->consume(operation_resource::allocation_bytes, part.size()); }
      lexical += part;
      reader_.advance();
    }
    if (lexical.empty() && schema_default && schema_scope) {
      lexical = qualify_qname_list(schema_default->items, *schema_scope, reader_.context(),
          &reader_.reserved_namespace_prefixes());
      if (validator) { validator(lexical, *schema_scope, reader_.context()); }
      detail::charge_token_scalar<T>(reader_.context(), lexical.size());
      detail::read_scalar(lexical, value);
      return;
    }
    const auto selected = lexical.empty() && empty_default ? *empty_default : std::string_view{lexical};
    if (validator) { validator(selected, lexical_scope(), reader_.context()); }
    detail::charge_token_scalar<T>(reader_.context(), selected.size());
    detail::read_scalar(selected, value);
  }
  // Decode a lexical scalar list from simpleContent with the same authoritative facets.
  template <typename T> void list_text_value(T& value, lexical_validator validator = nullptr,
      std::optional<std::string_view> empty_default = {}, const lexical_name_list* schema_default = nullptr,
      element* schema_scope = nullptr) {
    std::string lexical{};
    text_value(lexical, validator, empty_default, schema_default, schema_scope);
    detail::check_token_list(lexical, reader_.bounds(), reader_.context());
    value = parse_list<T>(lexical, reader_.context());
  }
  // Decode one present or defaulted scalar-list attribute without retaining any XML nodes.
  template <typename T> void list_attribute_value(const name& selected, T& value,
      std::optional<std::string_view> absent_default = {}, lexical_validator validator = nullptr,
      const element* default_scope = nullptr) {
    if constexpr (detail::optional_traits<T>::value) {
      std::optional<std::string> lexical{};
      attribute_value(selected, lexical, absent_default, validator, default_scope);
      if (lexical) { detail::check_token_list(*lexical, reader_.bounds(), reader_.context()); value = parse_list<T>(*lexical, reader_.context()); }
      else { value.reset(); }
    } else {
      std::string lexical{};
      attribute_value(selected, lexical, absent_default, validator, default_scope);
      detail::check_token_list(lexical, reader_.bounds(), reader_.context());
      value = parse_list<T>(lexical, reader_.context());
    }
  }
  // Consume one typed lexical list using the same scalar lexical/facet context as its XML element.
  template <typename T> void list_child_value(const name& selected, T& value,
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    if constexpr (detail::optional_traits<T>::value) {
      std::optional<std::string> lexical{};
      child_value(selected, lexical, empty_default, validator, projection);
      if (lexical) { detail::check_token_list(*lexical, reader_.bounds(), reader_.context()); value = parse_list<T>(*lexical, reader_.context()); }
      else { value.reset(); }
    } else {
      std::string lexical{};
      child_value(selected, lexical, empty_default, validator, projection);
      detail::check_token_list(lexical, reader_.bounds(), reader_.context());
      value = parse_list<T>(lexical, reader_.context());
    }
  }
  // Defer one lexical list as a singleton all child rather than registering its individual native items.
  template <typename T> void all_list_child_value(const name& selected, T& value,
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    if (reader_.context()) { reader_.context()->consume(operation_resource::allocation_bytes,
        sizeof(all_field) + selected.local.size() + selected.namespace_uri.size()); }
    if constexpr (detail::optional_traits<T>::value) { value.reset(); }
    all_fields_.push_back({selected, [this, selected, &value, empty_default, validator, projection] {
      list_child_value(selected, value, empty_default, validator, projection);
    }, !detail::optional_traits<T>::value, false});
  }
  // Decode an explicitly mapped presence state before its scalar/record/collection payload.
  template <typename T> void presence_child_value(const name& selected, T& value,
      token_presence presence, const name& wrapper_item = {},
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    reader_.skip_space();
    const bool present = reader_.current().kind == event_kind::start_element &&
        reader_.current().qualified_name == selected;
    const bool allows_absent = presence == token_presence::absent || presence == token_presence::absent_or_nil;
    const bool allows_nil = presence == token_presence::nil || presence == token_presence::absent_or_nil;
    if (!present) {
      if (!allows_absent) { throw std::invalid_argument{"Required XML presence element is absent"}; }
      if constexpr (detail::optional_traits<T>::value) { value.reset(); }
      return;
    }
    const bool nil = reader_.take_nil(allows_nil);
    if (nil) {
      if (!reader_.current().attributes.empty()) {
        throw std::invalid_argument{"Nil XML value contains unmapped attributes"};
      }
      reader_.advance();
      if (reader_.current().kind != event_kind::end_element) {
        throw std::invalid_argument{"Nil XML value contains text or children"};
      }
      reader_.advance();
      if constexpr (detail::optional_traits<T>::value) {
        using inner_type = typename detail::optional_traits<T>::type;
        if constexpr (detail::optional_traits<inner_type>::value) { value.emplace(); }
        else { value.reset(); }
      }
      return;
    }
    const auto read = [&](auto& item) {
      if constexpr (detail::collection_traits<std::remove_cvref_t<decltype(item)>>::value) {
        if (wrapper_item.local.empty()) { throw std::invalid_argument{"XML collection presence requires a wrapper"}; }
        token_fields fields{reader_};
        fields.child_value(wrapper_item, item, empty_default, validator, projection);
        fields.finish();
      } else {
        if (wrapper_item.local.empty()) { read_token_value(reader_, item, empty_default, validator, projection); }
        else {
          token_fields fields{reader_};
          fields.child_value(wrapper_item, item, empty_default, validator, projection);
          fields.finish();
        }
      }
    };
    if constexpr (detail::optional_traits<T>::value) {
      using inner_type = typename detail::optional_traits<T>::type;
      if constexpr (detail::optional_traits<inner_type>::value) {
        typename detail::optional_traits<inner_type>::type item{};
        read(item);
        value.emplace(std::move(item));
      } else {
        inner_type item{};
        read(item);
        value = std::move(item);
      }
    } else { read(value); }
  }
  // Defer singleton nil/absent traversal within XSD all while retaining its exact presence requirement.
  template <typename T> void all_presence_child_value(const name& selected, T& value,
      token_presence presence, const name& wrapper_item = {},
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    if (reader_.context()) {
      reader_.context()->consume(operation_resource::allocation_bytes,
          sizeof(all_field) + sizeof(name) + 2U * (selected.local.size() + selected.namespace_uri.size()));
    }
    if constexpr (detail::optional_traits<T>::value) { value.reset(); }
    all_fields_.push_back({selected, [this, selected, &value, presence, wrapper_item, empty_default, validator, projection] {
      presence_child_value(selected, value, presence, wrapper_item, empty_default, validator, projection);
    }, presence == token_presence::required || presence == token_presence::nil, false});
  }
  // Decode complete map entries and reject duplicate native key identities before publication.
  template <typename Map> void map_value(const name& wrapper, const name& entry,
      const name& key_name, const name& value_name, bool attribute_key, Map& value) {
    reader_.skip_space();
    const auto matches = [&](const name& selected) {
      return reader_.current().kind == event_kind::start_element && reader_.current().qualified_name == selected;
    };
    if constexpr (detail::optional_traits<Map>::value) {
      value.reset();
      if (matches(wrapper)) {
        typename detail::optional_traits<Map>::type candidate{};
        map_value(wrapper, entry, key_name, value_name, attribute_key, candidate);
        value = std::move(candidate);
      }
    } else {
      if (!matches(wrapper)) { throw std::invalid_argument{"Missing XML map wrapper"}; }
      token_fields wrapped{reader_};
      Map candidate{};
      std::size_t key_bytes{};
      reader_.skip_space();
      while (matches(entry)) {
        if (candidate.size() >= reader_.bounds().max_map_entries) {
          reject_limit(reader_.context(), "XML map entry limit exceeded", 1U, 0U);
        }
        if (reader_.context()) {
          reader_.context()->consume(operation_resource::allocation_bytes, sizeof(typename Map::value_type));
        }
        token_fields fields{reader_};
        typename Map::key_type key{};
        typename Map::mapped_type item{};
        std::string lexical{};
        bool value_read{};
        if (attribute_key) { fields.attribute_value(key_name, lexical); }
        else {
          reader_.skip_space();
          if (matches(value_name)) {
            fields.child_value(value_name, item);
            value_read = true;
          }
          fields.child_value(key_name, lexical);
        }
        detail::charge_token_scalar<typename Map::key_type>(reader_.context(), lexical.size());
        detail::read_scalar(lexical, key);
        const auto canonical = [&] {
          if constexpr (std::same_as<typename Map::key_type, std::string>) { return std::string_view{key}; }
          else { return detail::scalar_text(key); }
        }();
        const auto admitted = std::max(lexical.size(), canonical.size());
        if (admitted > reader_.bounds().max_map_key_bytes - key_bytes) {
          reject_limit(reader_.context(), "XML map key byte limit exceeded", admitted, reader_.bounds().max_map_key_bytes - key_bytes);
        }
        key_bytes += admitted;
        if (!value_read) { fields.child_value(value_name, item); }
        fields.finish();
        if (!candidate.emplace(std::move(key), std::move(item)).second) {
          throw std::invalid_argument{"Duplicate XML map key"};
        }
        reader_.skip_space();
      }
      wrapped.finish();
      value = std::move(candidate);
    }
  }
  // Register a singleton XSD all child without consuming its position in the input stream.
  template <typename T> void all_child_value(const name& selected, T& value,
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr,
      detail::token_namespace_projection projection = {}) {
    if (reader_.context()) {
      reader_.context()->consume(operation_resource::allocation_bytes,
          sizeof(all_field) + sizeof(name) + 2U * (selected.local.size() + selected.namespace_uri.size()));
    }
    if constexpr (detail::optional_traits<T>::value) { value.reset(); }
    all_fields_.push_back({selected, [this, selected, &value, empty_default, validator, projection] {
      child_value(selected, value, empty_default, validator, projection);
    }, !detail::optional_traits<T>::value, false});
  }
  // Dispatch all-group children in their native input order with one bounded seen flag per field.
  void finish_all(bool required_group = true);
  // Reject unconsumed attributes/children before closing the current record.
  void finish();
};

// Encode a generated record without tree allocation; rich unsupported contracts reject explicitly.
template <typename T> [[nodiscard]] std::string serialize_tokens(const T& value,
    limits bounds, operation_context* context) {
  if constexpr (!detail::token_mapping_available<T>()) {
    throw std::invalid_argument{"This XML contract has no direct token mapping"};
  } else {
    token_writer writer{bounds, context};
    write_token_child(writer, T::xml_root_name(), value);
    return writer.finish();
  }
}

// Publish a fresh candidate only after exact-message syntax and mapping validation complete.
template <typename T> [[nodiscard]] T deserialize_tokens(std::string_view bytes,
    limits bounds, operation_context* context, std::size_t chunk_bytes) {
  if constexpr (!detail::token_mapping_available<T>()) {
    throw std::invalid_argument{"This XML contract has no direct token mapping"};
  } else {
    token_reader reader{bytes, bounds, context, chunk_bytes};
    if (reader.current().kind != event_kind::start_element || reader.current().qualified_name != T::xml_root_name()) {
      throw std::invalid_argument{"Unexpected XML root name or namespace"};
    }
    if (context && context->permitted_root_type) {
      const auto type = reader.take_type();
      if (type && *type != *context->permitted_root_type) {
        throw std::invalid_argument{"Unexpected package XML root type"};
      }
    }
    if (context) { context->consume(operation_resource::allocation_bytes, sizeof(T)); }
    T candidate{};
    read_token_value(reader, candidate);
    reader.finish();
    return candidate;
  }
}

} // namespace rohit::serializer::xml
