// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace rohit::serializer::xml {

// Normalized semantic events; empty elements always produce a start/end pair.
enum class event_kind { start_document, start_element, text, end_element, end_document };

// Expanded identity borrowed from a producer; prefixes do not participate in equality.
struct event_name {
  std::string_view local{};
  std::string_view namespace_uri{};
  // Compare expanded identities without allocating or retaining the producer's storage.
  bool operator==(const event_name&) const = default;
};

// One borrowed ordinary attribute; namespace declarations have their own payload.
struct event_attribute {
  event_name qualified_name{};
  std::string_view value{};
};

// A declaration local to this start element, including an empty default-namespace reset.
struct namespace_declaration {
  std::string_view prefix{};
  std::string_view namespace_uri{};
};

// All views and spans are producer-owned and must remain valid through synchronous use.
// Consumers must copy data they retain after accept/callback return. No field owns storage.
struct event_view {
  event_kind kind{event_kind::start_document};
  event_name qualified_name{};
  std::span<const event_attribute> attributes{};
  std::span<const namespace_declaration> namespaces{};
  std::string_view text{};
};

// Event bounds supplement the existing cumulative document limits. Byte adapters enforce
// max_input_bytes/max_output_bytes; max_tree_bytes is unused because no tree is retained.
struct event_limits {
  limits document{};
  std::size_t max_events{1000000U};
  std::size_t max_event_bytes{16U * 1024U * 1024U};
  // Logical open-name storage plus temporary duplicate indexes, excluding excess capacity.
  std::size_t max_state_bytes{1024U * 1024U};
};

// Cumulative admissions; state_bytes alone decreases as elements close. Failed events may
// leave partial admissions, so these counters are diagnostic after failure.
struct event_usage {
  std::size_t events{};
  std::size_t nodes{};
  std::size_t attributes{};
  std::size_t text_bytes{};
  std::size_t name_bytes{};
  std::size_t namespace_bindings{};
  std::size_t state_bytes{};
};

// Provider-independent structural/resource checker, retaining only owned open expanded names.
// Producers validate XML characters, names, namespace scope, DTD rejection and byte EOF.
// This mutable checker and its borrowed context are not thread-safe or reentrant.
class event_validator {
  enum class phase { before_document, in_document, after_document, failed };
  event_limits bounds_{};
  operation_context* context_{};
  event_usage usage_{};
  std::vector<name> open_elements_{};
  phase phase_{phase::before_document};
  bool root_seen_{};

  // Validate and account one event; the public boundary fences every exception.
  void accept_event(const event_view& value);

public:
  // Borrow the optional context for the validator's lifetime without resetting its counters.
  explicit event_validator(event_limits bounds = {}, operation_context* context = nullptr);
  // A validation pass has one owner and cannot be copied into an independent budget.
  event_validator(const event_validator&) = delete;
  // Replacing a pass would discard its publication/failure state.
  event_validator& operator=(const event_validator&) = delete;
  // Validate before consumers use an event; any exception permanently fences this pass.
  // Only structural checks are made; successful prefixes never authorize model publication.
  void accept(const event_view& value);
  // Require explicit end_document, issued by a producer only after checked byte EOF.
  // Idempotent after success; failure or interruption permanently fences this pass.
  void finish();
  // Report complete validation rather than merely a closed root element.
  [[nodiscard]] bool complete() const noexcept;
  // Report terminal failure, including exceptions from operation callbacks or allocation.
  [[nodiscard]] bool failed() const noexcept;
  // Inspect accounting; after failure, partially charged counters must not be reused.
  [[nodiscard]] const event_usage& usage() const noexcept;
};

} // namespace rohit::serializer::xml
