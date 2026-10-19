// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/serializer.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <map>
#include <optional>
#include <span>
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

// Bounded typed XML tree; opaque opt-in extension subtrees may retain ordered text segments.
struct element {
  struct text_segment {
    std::size_t before_child{};
    std::string value{};
  };
  name qualified_name{};
  std::vector<attribute> attributes{};
  std::vector<element> children{};
  std::string text{};
  // In-scope prefix bindings are retained for QName-valued metadata such as XSD type names.
  std::map<std::string, std::string> namespace_bindings{};
  std::vector<text_segment> text_segments{};
};

enum class operation_resource { allocation_bytes, work_units, preserved_bytes };
enum class unknown_policy { reject, skip, preserve };
enum class input_encoding_policy { utf8_only, utf8_or_utf16 };

// An opt-in, caller-owned context follows one XML/package operation. It is not thread-safe.
// Callbacks and the context must outlive every borrowed generated decoder or builder.
struct operation_context {
  std::function<bool()> is_cancelled{};
  std::optional<std::chrono::steady_clock::time_point> deadline{};
  std::function<void(operation_resource, std::size_t)> charge{};
  unknown_policy unknown_fields{unknown_policy::reject};
  // Only explicitly permitted foreign namespaces may be skipped/preserved; core names still reject.
  std::function<bool(std::string_view)> permits_namespace{};
  // Optional namespace context for output-only borrowed projections; the caller retains ownership.
  const std::map<std::string, std::string>* namespace_bindings{};
  // UTF-16 input requires its XML byte-order mark; output remains deterministic UTF-8.
  input_encoding_policy input_encoding{input_encoding_policy::utf8_only};
  // Raw-tree clients may retain ordered mixed text; typed field readers remain strict.
  bool allow_mixed_content{false};
  // Check cancellation and the monotonic deadline without resetting cumulative counters.
  void check() const;
  // Check interruption and charge before retaining storage or performing bounded work.
  void consume(operation_resource selected, std::size_t amount) const;
  // Select a deadline relative to now; nonpositive durations expire immediately.
  void set_timeout(std::chrono::steady_clock::duration duration);
};

// Inspect original text with its lexical namespace scope before any typed scalar/list conversion.
using lexical_validator = void (*)(std::string_view, const element&, operation_context*);

// Preserve semantic XML extensions in one explicitly selected record, not lexical formatting.
struct extension_store {
  struct wrapper_slot;
  struct child_slot {
    std::size_t before_child{};
    element value{};
    std::size_t before_field{};
  };
  std::vector<attribute> attributes{};
  std::vector<child_slot> children{};
  std::map<std::string, std::string> namespace_bindings{};
  std::vector<wrapper_slot> wrappers{};
};

// Configured collection wrappers keep their own extension placement without copying known children.
struct extension_store::wrapper_slot {
  name qualified_name{};
  extension_store extensions{};
};

// Empty preserving records retain one pointer; opaque storage is allocated only when data exists.
// Copies own independent extension trees, preserving ordinary generated value semantics.
class extension_slot {
  std::unique_ptr<extension_store> value_{};
public:
  // Keep the common extension-free record allocation-free.
  extension_slot() noexcept = default;
  // Deep-copy semantic extension state when an owning model is copied.
  extension_slot(const extension_slot& other)
      : value_{other.value_ ? std::make_unique<extension_store>(*other.value_) : nullptr} {}
  // Preserve the destination if allocation/copying fails.
  extension_slot& operator=(const extension_slot& other) {
    if (this != &other) { extension_slot candidate{other}; value_.swap(candidate.value_); }
    return *this;
  }
  extension_slot(extension_slot&&) noexcept = default;
  extension_slot& operator=(extension_slot&&) noexcept = default;
  // Inspect presence without allocating an empty store.
  [[nodiscard]] extension_store* get() noexcept { return value_.get(); }
  // Inspect presence through a const model without allocating an empty store.
  [[nodiscard]] const extension_store* get() const noexcept { return value_.get(); }
  // Discard a previous model's opaque state before a fresh decode.
  void reset() noexcept { value_.reset(); }
  // Materialize retained storage under the same selected operation allocation budget.
  extension_store& value(operation_context* context = nullptr) {
    if (!value_) {
      if (context) { context->consume(operation_resource::allocation_bytes, sizeof(extension_store)); }
      value_ = std::make_unique<extension_store>();
    }
    return *value_;
  }
  // Read an empty store through shared immutable metadata without changing a const model.
  [[nodiscard]] const extension_store& value() const noexcept {
    static const extension_store empty{};
    return value_ ? *value_ : empty;
  }
};

// A fixed-XSD wildcard retains its own attribute/element namespace and occurrence contract.
struct extension_rule {
  bool attribute{};
  std::string_view namespace_rule{"##other"};
  std::string_view target_namespace{};
  bool strict{};
  std::size_t minimum{};
  std::optional<std::size_t> maximum{};
  std::size_t before_field{};
};

// Match the XSD namespace token subset independently of authored prefix spelling.
[[nodiscard]] bool matches_namespace_rule(std::string_view uri, std::string_view rule,
    std::string_view target_namespace);

enum class whitespace_policy { preserve, replace, collapse };

// Portable facets accepted by the fixed-XSD binder. Patterns require a separately defined subset.
struct scalar_constraints {
  std::optional<long double> min_inclusive{}, max_inclusive{};
  std::optional<long double> min_exclusive{}, max_exclusive{};
  std::optional<std::string_view> lexical_min_inclusive{}, lexical_max_inclusive{};
  std::optional<std::string_view> lexical_min_exclusive{}, lexical_max_exclusive{};
  std::optional<std::size_t> length{}, min_length{}, max_length{};
  std::span<const std::string_view> enumeration{};
  bool finite{};
  bool list_length{};
  whitespace_policy whitespace{whitespace_policy::preserve};
  // Select the XSD float/double value space for bounds; empty preserves exact decimal comparison.
  std::string_view lexical_kind{};
};

// Normalize XML whitespace without locale-dependent character classification.
[[nodiscard]] std::string normalize_whitespace(std::string_view text, whitespace_policy policy);
// Validate a complete scalar lexical value and its Unicode length/enum/numeric facets.
void validate_scalar_constraints(std::string_view text, const scalar_constraints& constraints,
    operation_context* context = nullptr);
// Compare decimal numeric lexical values exactly; return negative, zero or positive ordering.
[[nodiscard]] int compare_numeric_values(std::string_view left, std::string_view right);
// Resolve a QName through this element's in-scope bindings; unbound/invalid names reject.
[[nodiscard]] name resolve_qname(std::string_view lexical, const element& scope);
// Generate a stable QName and retain its declaration, avoiding conflicting prefix bindings.
[[nodiscard]] std::string qualify_qname(const name& value, element& scope);
// Resolve whitespace-separated namespace prefixes to URI identities, rejecting unbound prefixes.
[[nodiscard]] std::vector<std::string> resolve_prefix_list(std::string_view lexical,
    const element& scope);
// Generate a prefix list and its declarations; URI identities survive arbitrary source prefixes.
[[nodiscard]] std::string qualify_prefix_list(std::span<const std::string> namespaces,
    element& scope);
// Validate a built-in XSD lexical type or each item of a built-in scalar list.
void validate_lexical_value(std::string_view text, std::string_view kind, const element& scope,
    operation_context* context = nullptr);
// Validate list items without allocating a temporary collection of decoded values.
void validate_lexical_list(std::string_view text, std::string_view item_kind, const element& scope,
    operation_context* context = nullptr);
// Compare list values in their built-in item value space instead of comparing numeric spellings.
[[nodiscard]] bool lexical_lists_equal(std::string_view first, std::string_view second,
    std::string_view item_kind, const element& scope);
// Validate preserved extension namespaces, fixed wildcard positions and counts before emission.
void validate_extensions(const extension_store& extensions, const name& owner,
    operation_context* context, std::span<const extension_rule> rules = {});

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
// Apply caller-selected cumulative budgets, interruption and extension policy while parsing.
// An explicit context may additionally permit BOM-marked UTF-16 while retaining decoded tree bounds.
[[nodiscard]] element parse(std::string_view document, limits bounds, operation_context* context);
// Emit deterministic XML 1.0 UTF-8, with escaping and the same tree/resource checks.
[[nodiscard]] std::string write(const element& document, limits bounds = {});
// Emit under an opt-in shared operation context without changing ordinary XML callers.
[[nodiscard]] std::string write(const element& document, limits bounds, operation_context* context);

// A prepared XML Schema regular expression. Matching uses independent provider execution state.
class prepared_pattern {
  struct implementation;
  std::unique_ptr<implementation> implementation_{};
public:
  // Compile a trusted expression after input/character and optional operation-budget checks.
  explicit prepared_pattern(std::string_view expression, limits bounds = {},
      operation_context* context = nullptr);
  // Release the provider-owned immutable automaton after callers have finished matching.
  ~prepared_pattern();
  // Transfer ownership; moving or destroying while another thread uses this object is invalid.
  prepared_pattern(prepared_pattern&&) noexcept;
  // Replace the expression by transferring ownership without copying provider state.
  prepared_pattern& operator=(prepared_pattern&&) noexcept;
  prepared_pattern(const prepared_pattern&) = delete;
  prepared_pattern& operator=(const prepared_pattern&) = delete;
  // Match an already whitespace-normalized lexical value; concurrent calls are supported.
  // Provider calls are not preemptible; cancellation/deadline checks surround each bounded call.
  [[nodiscard]] bool matches(std::string_view value, limits bounds = {},
      operation_context* context = nullptr) const;
};

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
  // Apply operation checks before/after provider validation; provider work is not preemptible.
  void validate(std::string_view document, limits bounds, operation_context* context) const;
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
  if constexpr (requires { typename T::reference_value_type; }) {
    std::string result{};
    value.with_value([&](const auto& selected) { result = scalar_text(selected); });
    return result;
  } else if constexpr (std::same_as<T, std::string>) { return value; }
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
  if constexpr (requires { typename T::reference_value_type; }) {
    throw std::invalid_argument{"XML value references are export-only; decode an owning value"};
  } else if constexpr (std::same_as<T, std::string>) { value = text; }
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

// Decode a complete whitespace-separated scalar list, preserving fixed extents and failure isolation.
template <typename Collection> [[nodiscard]] Collection parse_list_values(
    std::string_view lexical, operation_context* context = nullptr) {
  static_assert(detail::collection_traits<Collection>::value, "XML lists require a vector or array");
  using item_type = typename detail::collection_traits<Collection>::type;
  Collection candidate{};
  std::size_t index{};
  auto remaining = detail::trim(lexical);
  while (!remaining.empty()) {
    const auto end = remaining.find_first_of(" \t\r\n");
    const auto token = remaining.substr(0, end);
    if (context) { context->consume(operation_resource::work_units, token.size() + 1U); }
    item_type item{};
    detail::read_scalar(token, item);
    if constexpr (detail::collection_traits<Collection>::fixed) {
      if (index >= candidate.size()) { throw std::invalid_argument{"Too many XML list values"}; }
      candidate[index++] = std::move(item);
    } else {
      if (context) { context->consume(operation_resource::allocation_bytes, sizeof(item_type)); }
      candidate.push_back(std::move(item));
    }
    if (end == std::string_view::npos) { remaining = {}; }
    else { remaining = detail::trim(remaining.substr(end)); }
  }
  if constexpr (detail::collection_traits<Collection>::fixed) {
    if (index != candidate.size()) { throw std::invalid_argument{"Too few XML list values"}; }
  }
  return candidate;
}

// Preserve absent-vs-present list state while reusing the same exact item decoder.
template <typename Collection> [[nodiscard]] Collection parse_list(
    std::string_view lexical, operation_context* context = nullptr) {
  if constexpr (detail::optional_traits<Collection>::value) {
    return Collection{parse_list_values<typename detail::optional_traits<Collection>::type>(lexical, context)};
  } else { return parse_list_values<Collection>(lexical, context); }
}

// Encode a scalar collection into one deterministic space-separated lexical value.
template <typename Collection> [[nodiscard]] std::string format_list(
    const Collection& values, operation_context* context = nullptr) {
  static_assert(detail::collection_traits<Collection>::value, "XML lists require a vector or array");
  std::string result{};
  for (const auto& value : values) {
    const auto text = [&] {
      if constexpr (std::same_as<typename detail::collection_traits<Collection>::type, std::string>) {
        return std::string_view{value};
      } else if constexpr (std::same_as<typename detail::collection_traits<Collection>::type, bool>) {
        return detail::scalar_text(static_cast<bool>(value));
      } else { return detail::scalar_text(value); }
    }();
    if (text.empty() || text.find_first_of(" \t\r\n") != std::string::npos) {
      throw std::invalid_argument{"XML list item contains whitespace or is empty"};
    }
    const auto separator = static_cast<std::size_t>(!result.empty());
    if (context) {
      context->consume(operation_resource::work_units, text.size() + separator);
      context->consume(operation_resource::allocation_bytes, text.size() + separator);
    }
    if (separator) { result += ' '; }
    result += text;
  }
  return result;
}

// Bound the generated output tree while constructing it, before copying model strings/collections.
class build_budget {
  limits bounds_;
  operation_context* context_{};
  std::size_t nodes_{};
  std::size_t attributes_{};
  std::size_t text_bytes_{};
  std::size_t name_bytes_{};
  std::size_t depth_{};
  std::size_t tree_bytes_{};
  std::size_t namespace_bindings_{};
  // Charge one bounded counter before allocation or arithmetic overflow.
  static void charge(std::size_t& used, std::size_t amount, std::size_t maximum) {
    if (used > maximum || amount > maximum - used) { throw std::length_error{"XML output tree limit exceeded"}; }
    used += amount;
  }
public:
  // Bind complete-object budgets before building the first element.
  explicit build_budget(limits bounds = {}, operation_context* context = nullptr)
      : bounds_{bounds}, context_{context} {
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
    if (context_) {
      context_->consume(operation_resource::work_units, 1U);
      context_->consume(operation_resource::allocation_bytes,
          sizeof(element) + field_name.local.size() + field_name.namespace_uri.size());
    }
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
  // Enter an already counted wrapper while charging newly restored descendant extensions.
  [[nodiscard]] depth_guard enter_existing() {
    if (depth_ >= bounds_.max_depth) { throw std::length_error{"XML output depth exceeded"}; }
    ++depth_; return depth_guard{*this};
  }
  // Charge one scalar string before it is copied from the logical model.
  void text(std::size_t bytes) {
    if (context_) {
      context_->consume(operation_resource::work_units, bytes);
      context_->consume(operation_resource::allocation_bytes, bytes);
    }
    charge(text_bytes_, bytes, bounds_.max_text_bytes);
    charge(tree_bytes_, bytes, bounds_.max_tree_bytes);
  }
  // Charge an attribute name/count before allocating its value.
  void attribute_name(const name& field_name) {
    if (context_) {
      context_->consume(operation_resource::work_units, 1U);
      context_->consume(operation_resource::allocation_bytes,
          sizeof(attribute) + field_name.local.size() + field_name.namespace_uri.size());
    }
    charge(attributes_, 1U, bounds_.max_attributes);
    charge(tree_bytes_, sizeof(attribute), bounds_.max_tree_bytes);
    charge(name_bytes_, field_name.local.size(), bounds_.max_name_bytes);
    charge(name_bytes_, field_name.namespace_uri.size(), bounds_.max_name_bytes);
    charge(tree_bytes_, field_name.local.size(), bounds_.max_tree_bytes);
    charge(tree_bytes_, field_name.namespace_uri.size(), bounds_.max_tree_bytes);
  }
  // Borrow the context used by nested generated builders and lexical adapters.
  [[nodiscard]] operation_context* context() const noexcept { return context_; }
  // Charge a retained lexical namespace declaration before copying its strings.
  void namespace_binding(std::string_view prefix, std::string_view uri) {
    charge(namespace_bindings_, 1U, bounds_.max_namespace_bindings);
    charge(name_bytes_, prefix.size(), bounds_.max_name_bytes);
    charge(name_bytes_, uri.size(), bounds_.max_name_bytes);
    charge(tree_bytes_, sizeof(std::pair<const std::string, std::string>), bounds_.max_tree_bytes);
    charge(tree_bytes_, prefix.size(), bounds_.max_tree_bytes);
    charge(tree_bytes_, uri.size(), bounds_.max_tree_bytes);
    if (context_) {
      context_->consume(operation_resource::work_units, 1U);
      context_->consume(operation_resource::allocation_bytes,
          sizeof(std::pair<const std::string, std::string>));
      context_->consume(operation_resource::allocation_bytes, prefix.size());
      context_->consume(operation_resource::allocation_bytes, uri.size());
    }
  }
  // Bound auxiliary retained XML storage without counting its copied text twice as document text.
  void retained_storage(std::size_t bytes) {
    charge(tree_bytes_, bytes, bounds_.max_tree_bytes);
    if (context_) { context_->consume(operation_resource::allocation_bytes, bytes); }
  }
};

// Merge explicitly retained extension slots; collisions fail instead of replacing known data.
void restore_extensions(element& node, const extension_store& extensions, build_budget& budget);

// Populate one scalar or statically bound record, retaining the caller-selected element name.
template <typename T> void set_value(element& node, const T& value, build_budget& budget) {
  if constexpr (requires { typename T::reference_value_type; }) {
    value.with_value([&](const auto& selected) { set_value(node, selected, budget); });
  } else if constexpr (requires { value.serialize_xml_out(node, budget); }) { value.serialize_xml_out(node, budget); }
  else {
    if constexpr (std::same_as<T, std::string>) { budget.text(value.size()); node.text = value; }
    else { auto text = detail::scalar_text(value); budget.text(text.size()); node.text = std::move(text); }
  }
}
// Decode one scalar or statically bound record, rejecting nested content in scalar slots.
template <typename T> void get_value(
    const element& node, T& value, operation_context* context = nullptr,
    std::optional<std::string_view> empty_default = {}) {
  if (context) { context->consume(operation_resource::work_units, 1U); }
  if constexpr (requires { typename T::reference_value_type; }) {
    throw std::invalid_argument{"XML value references are export-only; decode an owning value"};
  } else if constexpr (requires { value.serialize_xml_in(node, context); }) {
    value.serialize_xml_in(node, context);
  } else if constexpr (requires { value.serialize_xml_in(node); }) { value.serialize_xml_in(node); }
  else {
    if (!node.attributes.empty() || !node.children.empty()) {
      throw std::invalid_argument{"XML scalar contains attributes or children"};
    }
    if constexpr (std::same_as<T, std::string>) {
      if (context) { context->consume(operation_resource::allocation_bytes, node.text.size()); }
    }
    detail::read_scalar(node.text.empty() && empty_default ? *empty_default : std::string_view{node.text}, value);
  }
}
// Append an explicitly mapped scalar attribute; absent optionals are omitted.
template <typename T> void add_attribute(element& node, name field_name, const T& value, build_budget& budget) {
  if constexpr (detail::optional_traits<T>::value) {
    if (value) { add_attribute(node, std::move(field_name), *value, budget); }
  } else if constexpr (requires { typename T::reference_value_type; }) {
    value.with_value([&](const auto& selected) { add_attribute(node, std::move(field_name), selected, budget); });
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

// Encode a scalar as simple record content while retaining its separately mapped attributes.
template <typename T> void set_text(element& node, const T& value, build_budget& budget) {
  if constexpr (requires { typename T::reference_value_type; }) {
    value.with_value([&](const auto& selected) { set_text(node, selected, budget); });
  } else if constexpr (std::same_as<T, std::string>) {
    budget.text(value.size());
    node.text = value;
  } else {
    auto text = detail::scalar_text(value);
    budget.text(text.size());
    node.text = std::move(text);
  }
}

// Encode a typed scalar collection inside a single simple-content lexical slot.
template <typename T> void set_list_text(element& node, const T& values, build_budget& budget) {
  set_text(node, format_list(values, budget.context()), budget);
}

// Encode a typed scalar collection into one attribute instead of repeated child elements.
template <typename T> void add_list_attribute(element& node, name field_name,
    const T& values, build_budget& budget) {
  if constexpr (detail::optional_traits<T>::value) {
    if (values) { add_list_attribute(node, std::move(field_name), *values, budget); }
  } else { add_attribute(node, std::move(field_name), format_list(values, budget.context()), budget); }
}

// Retain an explicit collection wrapper even when the collection is empty.
template <typename T> void add_wrapped_children(element& node, const name& wrapper_name,
    const name& item_name, const T& value, build_budget& budget) {
  const auto depth = budget.enter(wrapper_name);
  element wrapper{};
  wrapper.qualified_name = wrapper_name;
  add_child(wrapper, item_name, value, budget);
  node.children.push_back(std::move(wrapper));
}

// Ordered field decoder enforces exact attribute membership and child sequence/cardinality.
class field_reader {
  const element& node_;
  std::vector<bool> used_attributes_;
  mutable std::size_t next_child_{};
  std::size_t known_children_read_{};
  std::size_t next_field_{};
  mutable std::size_t active_field_{};
  operation_context* context_{};
  extension_store* extensions_{};
  extension_slot* lazy_extensions_{};
  std::span<const name> known_children_{};
  std::span<const extension_rule> extension_rules_{};
  mutable std::vector<std::size_t> extension_counts_{};
  mutable bool finished_{};
  // Keep registered core/known declarations strict even when a foreign namespace is permitted.
  [[nodiscard]] bool accepts_extension(const name& value, bool child) const;
  // Charge and enforce the first matching fixed wildcard's occurrence bound.
  void record_extension(const name& value, bool child) const;
  // Skip or retain foreign children before the next requested static field.
  void skip_extensions(const name* requested = nullptr) const;
  // Allocate an opted-in slot only when the first unknown value actually needs preservation.
  extension_store& retain_extensions() const {
    if (extensions_) { return *extensions_; }
    if (lazy_extensions_) { return lazy_extensions_->value(context_); }
    throw std::invalid_argument{"XML preservation requires an explicit extension slot"};
  }
  // Advance exactly one known child while tracking its extension insertion position.
  const element& take_child() { ++known_children_read_; return node_.children[next_child_++]; }
public:
  // Copy traversal state while borrowing the same immutable element and optional operation storage.
  field_reader(const field_reader&) = default;
  // Readers cannot rebind their immutable parent element after construction.
  field_reader& operator=(const field_reader&) = delete;
  // Transfer traversal buffers while retaining the existing parent/context borrowing contract.
  field_reader(field_reader&&) = default;
  // Assignment cannot replace the reader's immutable parent element.
  field_reader& operator=(field_reader&&) = delete;
  // Report the next borrowed child offset without advancing decoding or allocating state.
  [[nodiscard]] std::size_t child_position() const noexcept { return next_child_; }
  // Bind one record; indentation whitespace is accepted but mixed scalar text is rejected.
  explicit field_reader(const element& node, bool has_text = false,
      operation_context* context = nullptr, extension_store* extensions = nullptr,
      std::span<const name> known_children = {}, std::span<const extension_rule> rules = {})
      : node_{node}, used_attributes_(node.attributes.size(), false), context_{context},
        extensions_{extensions}, known_children_{known_children}, extension_rules_{rules},
        extension_counts_(rules.size(), 0U) {
    if (context_) {
      context_->consume(operation_resource::work_units, 1U);
      context_->consume(operation_resource::allocation_bytes, node.attributes.size());
    }
    if (extensions_) { *extensions_ = {}; }
    if (!has_text && !detail::trim(node.text).empty()) { throw std::invalid_argument{"Unexpected XML record text"}; }
  }
  // Use a lazy generated slot while keeping the explicit raw-store constructor compatible.
  field_reader(const element& node, bool has_text, operation_context* context,
      extension_slot& extensions, std::span<const name> known_children = {},
      std::span<const extension_rule> rules = {})
      : field_reader{node, has_text, context, nullptr, known_children, rules} {
    extensions.reset();
    lazy_extensions_ = &extensions;
  }
  // Decode simple content; child markup is forbidden even when its text would be empty.
  template <typename T> void text_value(T& value, lexical_validator validator = nullptr) {
    if (!node_.children.empty()) { throw std::invalid_argument{"XML simple content contains children"}; }
    if (validator) { validator(node_.text, node_, context_); }
    if constexpr (std::same_as<T, std::string>) {
      if (context_) { context_->consume(operation_resource::allocation_bytes, node_.text.size()); }
    }
    detail::read_scalar(node_.text, value);
  }
  // Decode an exact typed list from one simple-content value.
  template <typename T> void list_text_value(T& value, lexical_validator validator = nullptr) {
    if (!node_.children.empty()) { throw std::invalid_argument{"XML list text contains children"}; }
    if (validator) { validator(node_.text, node_, context_); }
    value = parse_list<T>(node_.text, context_);
  }
  // Decode an exact typed list from one required attribute.
  template <typename T> void list_attribute_value(const name& field_name, T& value,
      std::optional<std::string_view> absent_default = {}, lexical_validator validator = nullptr) {
    if constexpr (detail::optional_traits<T>::value) {
      std::optional<std::string> lexical{};
      attribute_value(field_name, lexical, absent_default, validator);
      if (lexical) { value = parse_list<T>(*lexical, context_); }
      else { value.reset(); }
    } else {
      std::string lexical{};
      attribute_value(field_name, lexical, absent_default, validator);
      value = parse_list<T>(lexical, context_);
    }
  }
  // Decode one required/optional attribute; duplicate attributes are rejected by the parser/writer.
  template <typename T> void attribute_value(const name& field_name, T& value,
      std::optional<std::string_view> absent_default = {}, lexical_validator validator = nullptr) {
    for (std::size_t index{}; index < node_.attributes.size(); ++index) {
      if (context_) { context_->consume(operation_resource::work_units, 1U); }
      if (node_.attributes[index].qualified_name != field_name) { continue; }
      if (used_attributes_[index]) { throw std::invalid_argument{"Repeated XML attribute mapping"}; }
      used_attributes_[index] = true;
      if (context_) {
        context_->consume(operation_resource::work_units, node_.attributes[index].value.size());
        context_->consume(operation_resource::allocation_bytes, node_.attributes[index].value.size());
      }
      if (validator) { validator(node_.attributes[index].value, node_, context_); }
      if constexpr (detail::optional_traits<T>::value) {
        typename detail::optional_traits<T>::type candidate{};
        detail::read_scalar(node_.attributes[index].value, candidate);
        value = std::move(candidate);
      } else { detail::read_scalar(node_.attributes[index].value, value); }
      return;
    }
    if (absent_default) {
      if (context_) {
        context_->consume(operation_resource::work_units, absent_default->size());
        context_->consume(operation_resource::allocation_bytes, absent_default->size());
      }
      if (validator) { validator(*absent_default, node_, context_); }
      if constexpr (detail::optional_traits<T>::value) {
        typename detail::optional_traits<T>::type candidate{};
        detail::read_scalar(*absent_default, candidate); value = std::move(candidate);
      } else { detail::read_scalar(*absent_default, value); }
    } else {
      if constexpr (detail::optional_traits<T>::value) { value.reset(); }
      else { throw std::invalid_argument{"Required XML attribute missing"}; }
    }
  }
  // Decode one scalar/record, optional, or repeated array in declared sequence order.
  template <typename T> void child_value(const name& field_name, T& value,
      std::optional<std::string_view> empty_default = {}, lexical_validator validator = nullptr) {
    active_field_ = next_field_++;
    const auto present = [&] {
      skip_extensions(&field_name);
      return next_child_ < node_.children.size() &&
             node_.children[next_child_].qualified_name == field_name;
    };
    // Validate each exact child/default spelling while its original prefix bindings remain available.
    const auto read_child = [&](const element& child, auto& destination) {
      if (validator) {
        validator(child.text.empty() && empty_default ? *empty_default : std::string_view{child.text},
            child, context_);
      }
      get_value(child, destination, context_, empty_default);
    };
    if constexpr (detail::optional_traits<T>::value) {
      if (!present()) { value.reset(); return; }
      if (context_) {
        context_->consume(operation_resource::allocation_bytes, sizeof(typename detail::optional_traits<T>::type));
      }
      typename detail::optional_traits<T>::type candidate{};
      read_child(take_child(), candidate);
      value = std::move(candidate);
    } else if constexpr (detail::collection_traits<T>::value) {
      if constexpr (detail::collection_traits<T>::fixed) {
        for (auto& item : value) {
          if (!present()) { throw std::invalid_argument{"XML fixed array item missing"}; }
          read_child(take_child(), item);
        }
      } else {
        value.clear();
        while (present()) {
          if (context_) {
            context_->consume(operation_resource::allocation_bytes, sizeof(typename detail::collection_traits<T>::type));
          }
          typename detail::collection_traits<T>::type candidate{};
          read_child(take_child(), candidate);
          value.push_back(std::move(candidate));
        }
      }
    } else {
      if (!present()) { throw std::invalid_argument{"Required XML child missing or out of order"}; }
      read_child(take_child(), value);
    }
  }
  // Require one wrapper, then apply the same exact item contract within its own scope.
  template <typename T> void wrapped_child_value(const name& wrapper_name, const name& item_name, T& value,
      lexical_validator validator = nullptr) {
    active_field_ = next_field_++;
    skip_extensions(&wrapper_name);
    if (next_child_ >= node_.children.size() || node_.children[next_child_].qualified_name != wrapper_name) {
      throw std::invalid_argument{"Required XML collection wrapper missing or out of order"};
    }
    const auto& wrapped = take_child();
    extension_slot retained{};
    const std::array known_items{item_name};
    field_reader wrapper{wrapped, false, context_, retained, known_items};
    wrapper.child_value(item_name, value, {}, validator);
    wrapper.finish();
    if (retained.get()) {
      auto& extensions = retain_extensions();
      context_->consume(operation_resource::allocation_bytes, sizeof(extension_store::wrapper_slot));
      context_->consume(operation_resource::allocation_bytes, wrapper_name.local.size());
      context_->consume(operation_resource::allocation_bytes, wrapper_name.namespace_uri.size());
      extensions.wrappers.push_back({wrapper_name, std::move(retained.value())});
    }
  }
  // Reject every undeclared attribute and every unknown, duplicate or misplaced child.
  void finish() const;
};

// Encode one complete owning object, optionally validating its emitted XML with prepared XSD.
template <typename T> [[nodiscard]] std::string serialize(
    const T& value, limits bounds = {}, const prepared_schema* schema = nullptr,
    operation_context* context = nullptr) {
  std::optional<operation_context> selected_context{};
  if constexpr (requires { T::xml_operation_context(context); }) {
    selected_context.emplace(T::xml_operation_context(context)); context = &*selected_context;
  }
  element document{};
  document.qualified_name = T::xml_root_name();
  build_budget budget{bounds, context};
  const auto depth = budget.enter(document.qualified_name);
  set_value(document, value, budget);
  auto bytes = write(document, bounds, context);
  if (schema) {
    if constexpr (T::xml_runtime_xsd) {
      if constexpr (T::xml_validate_output) { schema->validate(bytes, bounds, context); }
    }
    else { throw std::invalid_argument{"Runtime XSD was not enabled during model generation"}; }
  }
  return bytes;
}
// Decode into a fresh candidate and return only after syntax, mapping and optional XSD succeed.
template <typename T> [[nodiscard]] T deserialize(
    std::string_view bytes, limits bounds = {}, const prepared_schema* schema = nullptr,
    operation_context* context = nullptr) {
  std::optional<operation_context> selected_context{};
  if constexpr (requires { T::xml_operation_context(context); }) {
    selected_context.emplace(T::xml_operation_context(context)); context = &*selected_context;
  }
  if (schema) {
    if constexpr (T::xml_runtime_xsd) {
      if constexpr (T::xml_validate_input) { schema->validate(bytes, bounds, context); }
    }
    else { throw std::invalid_argument{"Runtime XSD was not enabled during model generation"}; }
  }
  const auto document = parse(bytes, bounds, context);
  if (document.qualified_name != T::xml_root_name()) {
    throw std::invalid_argument{"Unexpected XML root name or namespace"};
  }
  if (context) { context->consume(operation_resource::allocation_bytes, sizeof(T)); }
  T candidate{};
  get_value(document, candidate, context);
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
