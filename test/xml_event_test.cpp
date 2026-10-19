// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_events.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;

// Construct a document boundary with no borrowed payload.
xml::event_view boundary(xml::event_kind kind) {
  return {.kind = kind};
}

// Construct an element boundary using an expanded name.
xml::event_view element_event(xml::event_kind kind, xml::event_name name) {
  return {.kind = kind, .qualified_name = name};
}

// Construct a nonempty text event without element-only fields.
xml::event_view text_event(std::string_view text) {
  return {.kind = xml::event_kind::text, .text = text};
}

// Compare all documented counters rather than relying on the object's representation.
void expect_usage(const xml::event_usage& actual, const xml::event_usage& expected) {
  EXPECT_EQ(actual.events, expected.events);
  EXPECT_EQ(actual.nodes, expected.nodes);
  EXPECT_EQ(actual.attributes, expected.attributes);
  EXPECT_EQ(actual.text_bytes, expected.text_bytes);
  EXPECT_EQ(actual.name_bytes, expected.name_bytes);
  EXPECT_EQ(actual.namespace_bindings, expected.namespace_bindings);
  EXPECT_EQ(actual.state_bytes, expected.state_bytes);
}

// Feed a minimal complete document and require explicit, idempotent finalization.
void accept_empty_root(xml::event_validator& validator, xml::event_name name = {"r", ""}) {
  validator.accept(boundary(xml::event_kind::start_document));
  validator.accept(element_event(xml::event_kind::start_element, name));
  validator.accept(element_event(xml::event_kind::end_element, name));
  validator.accept(boundary(xml::event_kind::end_document));
  validator.finish();
  validator.finish();
  EXPECT_TRUE(validator.complete());
  EXPECT_FALSE(validator.failed());
}

// A failure permanently fences event consumption and leaves existing counters charged.
void expect_fenced(xml::event_validator& validator) {
  const auto charged = validator.usage();
  EXPECT_TRUE(validator.failed());
  EXPECT_FALSE(validator.complete());
  EXPECT_THROW(validator.accept(boundary(xml::event_kind::start_document)), std::exception);
  EXPECT_THROW(validator.finish(), std::exception);
  expect_usage(validator.usage(), charged);
}
} // namespace

// Golden traces specify expanded names, mixed text order, and exact cumulative accounting.
TEST(xml_event, golden_nested_trace_has_exact_usage_and_releases_open_state) {
  constexpr xml::event_attribute attributes[]{
      {{"id", ""}, "7"}, {{"flag", "urn:a"}, "yes"}};
  constexpr xml::namespace_declaration namespaces[]{{"", "urn:r"}, {"a", "urn:a"}};
  constexpr auto root_state_bytes = sizeof(xml::name) + std::string_view{"rooturn:r"}.size();
  constexpr auto child_state_bytes = sizeof(xml::name) + std::string_view{"itemurn:r"}.size();
  struct trace_step {
    xml::event_view event;
    xml::event_usage expected;
  };
  const trace_step trace[]{
      {boundary(xml::event_kind::start_document), {.events = 1U}},
      {{.kind = xml::event_kind::start_element, .qualified_name = {"root", "urn:r"},
           .attributes = attributes, .namespaces = namespaces},
          {.events = 2U, .nodes = 1U, .attributes = 2U, .text_bytes = 4U,
              .name_bytes = 31U, .namespace_bindings = 2U, .state_bytes = root_state_bytes}},
      {text_event("lead"), {.events = 3U, .nodes = 1U, .attributes = 2U, .text_bytes = 8U,
          .name_bytes = 31U, .namespace_bindings = 2U, .state_bytes = root_state_bytes}},
      {element_event(xml::event_kind::start_element, {"item", "urn:r"}),
          {.events = 4U, .nodes = 2U, .attributes = 2U, .text_bytes = 8U,
              .name_bytes = 40U, .namespace_bindings = 2U,
              .state_bytes = root_state_bytes + child_state_bytes}},
      {text_event("42"), {.events = 5U, .nodes = 2U, .attributes = 2U, .text_bytes = 10U,
          .name_bytes = 40U, .namespace_bindings = 2U,
          .state_bytes = root_state_bytes + child_state_bytes}},
      {element_event(xml::event_kind::end_element, {"item", "urn:r"}),
          {.events = 6U, .nodes = 2U, .attributes = 2U, .text_bytes = 10U,
              .name_bytes = 40U, .namespace_bindings = 2U, .state_bytes = root_state_bytes}},
      {text_event("tail"), {.events = 7U, .nodes = 2U, .attributes = 2U, .text_bytes = 14U,
          .name_bytes = 40U, .namespace_bindings = 2U, .state_bytes = root_state_bytes}},
      {element_event(xml::event_kind::end_element, {"root", "urn:r"}),
          {.events = 8U, .nodes = 2U, .attributes = 2U, .text_bytes = 14U,
              .name_bytes = 40U, .namespace_bindings = 2U}},
      {boundary(xml::event_kind::end_document), {.events = 9U, .nodes = 2U,
          .attributes = 2U, .text_bytes = 14U, .name_bytes = 40U, .namespace_bindings = 2U}}};
  xml::event_validator validator{};
  expect_usage(validator.usage(), {});
  EXPECT_FALSE(validator.complete());
  EXPECT_FALSE(validator.failed());
  for (std::size_t index = 0; index < std::size(trace); ++index) {
    SCOPED_TRACE(index);
    validator.accept(trace[index].event);
    expect_usage(validator.usage(), trace[index].expected);
  }
  EXPECT_TRUE(validator.complete());
  EXPECT_NO_THROW(validator.finish());
  EXPECT_NO_THROW(validator.finish());
  expect_usage(validator.usage(), trace[std::size(trace) - 1U].expected);
}

// An empty root is a valid document, and expanded-name identity includes the URI.
TEST(xml_event, golden_empty_root_and_distinct_attribute_namespaces) {
  xml::event_validator empty{};
  accept_empty_root(empty);
  expect_usage(empty.usage(), {.events = 4U, .nodes = 1U, .name_bytes = 1U});
  constexpr xml::event_attribute attributes[]{
      {{"id", ""}, ""}, {{"id", "urn:a"}, ""}, {{"id", "urn:b"}, ""}};
  constexpr xml::namespace_declaration namespaces[]{{"a", "urn:a"}, {"b", "urn:a"}};
  xml::event_validator validator{};
  validator.accept(boundary(xml::event_kind::start_document));
  validator.accept({.kind = xml::event_kind::start_element, .qualified_name = {"r", ""},
      .attributes = attributes, .namespaces = namespaces});
  validator.accept(element_event(xml::event_kind::end_element, {"r", ""}));
  validator.accept(boundary(xml::event_kind::end_document));
  EXPECT_NO_THROW(validator.finish());
}

// Borrowed event storage may be reused immediately after accept returns.
TEST(xml_event, copies_only_open_names_before_borrowed_payloads_are_reused) {
  xml::event_validator validator{};
  validator.accept(boundary(xml::event_kind::start_document));
  {
    std::string local{"root"};
    std::string uri{"urn:original"};
    std::string attribute_value{"borrowed value"};
    std::array<xml::event_attribute, 1U> attributes{{{{"id", ""}, attribute_value}}};
    std::array<xml::namespace_declaration, 1U> namespaces{{{"p", uri}}};
    validator.accept({.kind = xml::event_kind::start_element, .qualified_name = {local, uri},
        .attributes = attributes, .namespaces = namespaces});
    local.assign(local.size(), 'x');
    uri.assign(uri.size(), 'x');
    attribute_value.assign(attribute_value.size(), 'x');
    attributes.front() = {};
    namespaces.front() = {};
  }
  {
    std::string text{"transient"};
    validator.accept(text_event(text));
    text.assign(text.size(), 'x');
  }
  validator.accept(element_event(xml::event_kind::end_element, {"root", "urn:original"}));
  validator.accept(boundary(xml::event_kind::end_document));
  EXPECT_NO_THROW(validator.finish());
  EXPECT_EQ(validator.usage().state_bytes, 0U);
}

// Invalid document order and mismatched expanded names fail without recovery.
TEST(xml_event, rejects_malformed_order_and_unbalanced_element_boundaries) {
  const auto start_document = boundary(xml::event_kind::start_document);
  const auto end_document = boundary(xml::event_kind::end_document);
  const auto start_root = element_event(xml::event_kind::start_element, {"r", "urn:r"});
  const auto end_root = element_event(xml::event_kind::end_element, {"r", "urn:r"});
  const std::vector<xml::event_view> invalid_traces[]{
      {start_root},
      {end_document},
      {start_document, start_document},
      {start_document, end_document},
      {start_document, end_root},
      {start_document, text_event(" ")},
      {start_document, start_root, end_document},
      {start_document, start_root,
          element_event(xml::event_kind::end_element, {"other", "urn:r"})},
      {start_document, start_root, element_event(xml::event_kind::end_element, {"r", ""})},
      {start_document, start_root, end_root, start_root},
      {start_document, start_root, end_root, end_root},
      {start_document, start_root, end_root, text_event(" ")},
      {start_document, boundary(static_cast<xml::event_kind>(
          std::numeric_limits<unsigned char>::max()))}};
  for (std::size_t index = 0; index < std::size(invalid_traces); ++index) {
    SCOPED_TRACE(index);
    xml::event_validator validator{};
    const auto& trace = invalid_traces[index];
    for (std::size_t event_index = 0; event_index + 1U < trace.size(); ++event_index) {
      validator.accept(trace[event_index]);
    }
    EXPECT_THROW(validator.accept(trace.back()), std::invalid_argument);
    expect_fenced(validator);
  }
}

// Each event kind rejects payload fields that belong to a different event kind.
TEST(xml_event, rejects_event_shape_empty_names_and_empty_text) {
  constexpr xml::event_attribute attributes[]{{{"id", ""}, "value"}};
  constexpr xml::namespace_declaration namespaces[]{{"p", "urn:p"}};
  const xml::event_view invalid_events[]{
      {.kind = xml::event_kind::start_document, .qualified_name = {"r", ""}},
      {.kind = xml::event_kind::start_document, .attributes = attributes},
      {.kind = xml::event_kind::start_document, .namespaces = namespaces},
      {.kind = xml::event_kind::start_document, .text = "x"},
      {.kind = xml::event_kind::start_element},
      {.kind = xml::event_kind::start_element, .qualified_name = {"", "urn:r"}},
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", ""}, .text = "x"},
      {.kind = xml::event_kind::text},
      {.kind = xml::event_kind::text, .qualified_name = {"r", ""}, .text = "x"},
      {.kind = xml::event_kind::text, .attributes = attributes, .text = "x"},
      {.kind = xml::event_kind::text, .namespaces = namespaces, .text = "x"},
      {.kind = xml::event_kind::end_element},
      {.kind = xml::event_kind::end_element, .qualified_name = {"r", ""},
          .attributes = attributes},
      {.kind = xml::event_kind::end_element, .qualified_name = {"r", ""},
          .namespaces = namespaces},
      {.kind = xml::event_kind::end_element, .qualified_name = {"r", ""}, .text = "x"},
      {.kind = xml::event_kind::end_document, .qualified_name = {"r", ""}},
      {.kind = xml::event_kind::end_document, .attributes = attributes},
      {.kind = xml::event_kind::end_document, .namespaces = namespaces},
      {.kind = xml::event_kind::end_document, .text = "x"}};
  for (std::size_t index = 0; index < std::size(invalid_events); ++index) {
    SCOPED_TRACE(index);
    const auto& event = invalid_events[index];
    xml::event_validator validator{};
    if (event.kind != xml::event_kind::start_document) {
      validator.accept(boundary(xml::event_kind::start_document));
      if (event.kind != xml::event_kind::start_element) {
        validator.accept(element_event(xml::event_kind::start_element, {"r", ""}));
        if (event.kind == xml::event_kind::end_document) {
          validator.accept(element_event(xml::event_kind::end_element, {"r", ""}));
        }
      }
    }
    EXPECT_THROW(validator.accept(event), std::invalid_argument);
    expect_fenced(validator);
  }
}

// Attributes use expanded identity, while namespace declarations use prefix identity.
TEST(xml_event, rejects_duplicate_attributes_prefixes_and_unnamed_attributes) {
  constexpr xml::event_attribute duplicate_attributes[]{
      {{"id", "urn:a"}, "1"}, {{"id", "urn:a"}, "2"}};
  constexpr xml::event_attribute unnamed_attributes[]{{{"", "urn:a"}, "1"}};
  constexpr xml::namespace_declaration duplicate_prefixes[]{{"p", "urn:a"}, {"p", "urn:b"}};
  constexpr xml::namespace_declaration duplicate_defaults[]{{"", "urn:a"}, {"", "urn:b"}};
  const xml::event_view invalid_events[]{
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", ""},
          .attributes = duplicate_attributes},
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", ""},
          .attributes = unnamed_attributes},
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", ""},
          .namespaces = duplicate_prefixes},
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", ""},
          .namespaces = duplicate_defaults}};
  for (const auto& event : invalid_events) {
    xml::event_validator validator{};
    validator.accept(boundary(xml::event_kind::start_document));
    EXPECT_THROW(validator.accept(event), std::invalid_argument);
    expect_fenced(validator);
  }
}

// Explicit end_document is required even after the root closes; completion rejects more events.
TEST(xml_event, finish_requires_terminal_boundary_and_completion_rejects_trailing_events) {
  const xml::event_view prefix[]{boundary(xml::event_kind::start_document),
      element_event(xml::event_kind::start_element, {"r", ""}),
      element_event(xml::event_kind::end_element, {"r", ""})};
  for (std::size_t prefix_length = 0; prefix_length <= std::size(prefix); ++prefix_length) {
    xml::event_validator validator{};
    for (std::size_t index = 0; index < prefix_length; ++index) {
      validator.accept(prefix[index]);
    }
    EXPECT_THROW(validator.finish(), std::invalid_argument);
    expect_fenced(validator);
  }
  xml::event_validator complete{};
  accept_empty_root(complete);
  EXPECT_THROW(complete.accept(boundary(xml::event_kind::end_document)), std::exception);
  expect_fenced(complete);
}

// The event limit includes document and end-element events and accepts the exact boundary.
TEST(xml_event, enforces_total_event_count) {
  xml::event_limits exact_bounds{};
  exact_bounds.max_events = 4U;
  xml::event_validator exact{exact_bounds};
  accept_empty_root(exact);
  auto short_bounds = exact_bounds;
  --short_bounds.max_events;
  xml::event_validator short_limit{short_bounds};
  short_limit.accept(boundary(xml::event_kind::start_document));
  short_limit.accept(element_event(xml::event_kind::start_element, {"r", ""}));
  short_limit.accept(element_event(xml::event_kind::end_element, {"r", ""}));
  EXPECT_THROW(short_limit.accept(boundary(xml::event_kind::end_document)), std::length_error);
  expect_fenced(short_limit);
  xml::event_limits zero_bounds{};
  zero_bounds.max_events = 0;
  xml::event_validator zero{zero_bounds};
  EXPECT_THROW(zero.accept(boundary(xml::event_kind::start_document)), std::length_error);
  expect_fenced(zero);
}

// Each event's byte ceiling covers every borrowed name, value and namespace byte.
TEST(xml_event, enforces_event_payload_bytes_including_end_names) {
  constexpr xml::event_attribute attributes[]{{{"id", ""}, "7"}};
  constexpr xml::namespace_declaration namespaces[]{{"p", "urn:r"}};
  const xml::event_view start{.kind = xml::event_kind::start_element,
      .qualified_name = {"r", "urn:r"}, .attributes = attributes, .namespaces = namespaces};
  constexpr std::size_t event_payload_bytes = 15U;
  for (const auto max_bytes : {event_payload_bytes, event_payload_bytes - 1U}) {
    xml::event_limits bounds{};
    bounds.max_event_bytes = max_bytes;
    xml::event_validator validator{bounds};
    validator.accept(boundary(xml::event_kind::start_document));
    if (max_bytes == event_payload_bytes) {
      validator.accept(start);
      validator.accept(element_event(xml::event_kind::end_element, {"r", "urn:r"}));
      validator.accept(boundary(xml::event_kind::end_document));
      EXPECT_NO_THROW(validator.finish());
    } else {
      EXPECT_THROW(validator.accept(start), std::length_error);
      expect_fenced(validator);
    }
  }
  xml::event_limits bounds{};
  bounds.max_event_bytes = std::string_view{"r"}.size();
  xml::event_validator validator{bounds};
  validator.accept(boundary(xml::event_kind::start_document));
  validator.accept(element_event(xml::event_kind::start_element, {"r", ""}));
  EXPECT_THROW(validator.accept(element_event(xml::event_kind::end_element, {"long", ""})),
      std::length_error);
  expect_fenced(validator);
}

// The existing document counters remain cumulative across event boundaries.
TEST(xml_event, enforces_shared_document_limits_at_the_exact_boundary) {
  constexpr xml::event_attribute attributes[]{{{"id", ""}, "7"}};
  constexpr xml::namespace_declaration namespaces[]{{"p", "urn:r"}};
  const xml::event_view trace[]{boundary(xml::event_kind::start_document),
      {.kind = xml::event_kind::start_element, .qualified_name = {"r", "urn:r"},
          .attributes = attributes, .namespaces = namespaces},
      text_event("abc"), element_event(xml::event_kind::end_element, {"r", "urn:r"}),
      boundary(xml::event_kind::end_document)};
  struct limit_case {
    std::size_t xml::limits::* member;
    std::size_t exact;
  };
  constexpr limit_case cases[]{{&xml::limits::max_nodes, 1U},
      {&xml::limits::max_depth, 1U}, {&xml::limits::max_attributes, 1U},
      {&xml::limits::max_text_bytes, 4U}, {&xml::limits::max_name_bytes, 14U},
      {&xml::limits::max_namespace_bindings, 1U}};
  for (std::size_t index = 0; index < std::size(cases); ++index) {
    SCOPED_TRACE(index);
    xml::event_limits bounds{};
    bounds.document.*cases[index].member = cases[index].exact;
    xml::event_validator exact{bounds};
    for (const auto& event : trace) {
      exact.accept(event);
    }
    EXPECT_NO_THROW(exact.finish());
    --(bounds.document.*cases[index].member);
    xml::event_validator short_limit{bounds};
    bool rejected = false;
    for (const auto& event : trace) {
      try {
        short_limit.accept(event);
      } catch (const std::length_error&) {
        rejected = true;
        break;
      }
    }
    EXPECT_TRUE(rejected);
    expect_fenced(short_limit);
  }
}

// Element depth is live state, whereas the node count includes closed siblings.
TEST(xml_event, enforces_nested_depth_and_cumulative_sibling_nodes) {
  for (const auto member : {&xml::limits::max_depth, &xml::limits::max_nodes}) {
    xml::event_limits bounds{};
    bounds.document.*member = 1U;
    xml::event_validator validator{bounds};
    validator.accept(boundary(xml::event_kind::start_document));
    validator.accept(element_event(xml::event_kind::start_element, {"r", ""}));
    EXPECT_THROW(validator.accept(element_event(xml::event_kind::start_element, {"c", ""})),
        std::length_error);
    expect_fenced(validator);
  }
  xml::event_limits bounds{};
  bounds.document.max_nodes = 2U;
  xml::event_validator siblings{bounds};
  siblings.accept(boundary(xml::event_kind::start_document));
  siblings.accept(element_event(xml::event_kind::start_element, {"r", ""}));
  siblings.accept(element_event(xml::event_kind::start_element, {"c", ""}));
  siblings.accept(element_event(xml::event_kind::end_element, {"c", ""}));
  EXPECT_THROW(siblings.accept(element_event(xml::event_kind::start_element, {"c", ""})),
      std::length_error);
  expect_fenced(siblings);
}

// State bytes count owned open names, release closed frames, and bound sorting scratch space.
TEST(xml_event, enforces_open_name_and_duplicate_check_state_budget) {
  constexpr auto root_bytes = sizeof(xml::name) + std::string_view{"rurn:r"}.size();
  xml::event_limits bounds{};
  bounds.max_state_bytes = root_bytes;
  xml::event_validator exact{bounds};
  accept_empty_root(exact, {"r", "urn:r"});
  --bounds.max_state_bytes;
  xml::event_validator short_limit{bounds};
  short_limit.accept(boundary(xml::event_kind::start_document));
  EXPECT_THROW(short_limit.accept(element_event(xml::event_kind::start_element, {"r", "urn:r"})),
      std::length_error);
  expect_fenced(short_limit);
  constexpr xml::event_attribute attributes[]{{{"a", ""}, ""}, {{"b", ""}, ""}};
  constexpr auto scratch_bytes = std::size(attributes) * sizeof(std::size_t);
  for (const auto max_bytes : {root_bytes + scratch_bytes, root_bytes + scratch_bytes - 1U}) {
    bounds.max_state_bytes = max_bytes;
    xml::event_validator validator{bounds};
    validator.accept(boundary(xml::event_kind::start_document));
    const xml::event_view event{.kind = xml::event_kind::start_element,
        .qualified_name = {"r", "urn:r"}, .attributes = attributes};
    if (max_bytes == root_bytes + scratch_bytes) {
      validator.accept(event);
      EXPECT_EQ(validator.usage().state_bytes, root_bytes);
      validator.accept(element_event(xml::event_kind::end_element, {"r", "urn:r"}));
      validator.accept(boundary(xml::event_kind::end_document));
      EXPECT_NO_THROW(validator.finish());
    } else {
      EXPECT_THROW(validator.accept(event), std::length_error);
      expect_fenced(validator);
    }
  }
}

// Callback charges survive failures, and a fenced validator never calls the context again.
TEST(xml_event, operation_context_charges_work_and_allocation_without_refunding_failures) {
  std::size_t work_units{};
  std::size_t allocation_bytes{};
  std::size_t callbacks{};
  xml::operation_context context{};
  // Record logical resource charges from the event contract.
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    ++callbacks;
    if (resource == xml::operation_resource::work_units) {
      work_units += amount;
    } else if (resource == xml::operation_resource::allocation_bytes) {
      allocation_bytes += amount;
    }
  };
  xml::event_validator validator{{}, &context};
  validator.accept(boundary(xml::event_kind::start_document));
  validator.accept(element_event(xml::event_kind::start_element, {"r", "urn:r"}));
  EXPECT_GT(work_units, 0U);
  EXPECT_GE(allocation_bytes, sizeof(xml::name) + std::string_view{"rurn:r"}.size());
  const auto previous_work = work_units;
  const auto previous_allocation = allocation_bytes;
  EXPECT_THROW(validator.accept(element_event(xml::event_kind::end_element, {"wrong", "urn:r"})),
      std::invalid_argument);
  EXPECT_GE(work_units, previous_work);
  EXPECT_GE(allocation_bytes, previous_allocation);
  const auto charged_callbacks = callbacks;
  expect_fenced(validator);
  EXPECT_EQ(callbacks, charged_callbacks);
}

// Allocation callback exceptions become terminal and preserve the callback's own charge.
TEST(xml_event, throwing_resource_callback_fences_without_reentering_the_context) {
  std::size_t allocations{};
  xml::operation_context context{};
  // Fail after accounting the attempted open-name allocation.
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    if (resource == xml::operation_resource::allocation_bytes) {
      allocations += amount;
      throw std::runtime_error{"test allocation budget exhausted"};
    }
  };
  xml::event_validator validator{{}, &context};
  validator.accept(boundary(xml::event_kind::start_document));
  EXPECT_THROW(validator.accept(element_event(xml::event_kind::start_element, {"r", ""})),
      std::runtime_error);
  EXPECT_GT(allocations, 0U);
  const auto charged_allocations = allocations;
  expect_fenced(validator);
  EXPECT_EQ(allocations, charged_allocations);
  std::size_t checks{};
  xml::operation_context throwing_check{};
  // Non-standard callback exceptions must fence the pass as well.
  throwing_check.is_cancelled = [&]() -> bool {
    ++checks;
    throw 7;
  };
  xml::event_validator interrupted{{}, &throwing_check};
  EXPECT_THROW(interrupted.accept(boundary(xml::event_kind::start_document)), int);
  expect_fenced(interrupted);
  EXPECT_EQ(checks, 1U);
}

// Cancellation and expired deadlines fence accept and finish without allowing a retry.
TEST(xml_event, cancellation_and_deadline_fence_events_and_finalization) {
  bool cancelled = false;
  std::size_t checks{};
  xml::operation_context context{};
  // Permit the first boundary and interrupt the next one.
  context.is_cancelled = [&] {
    ++checks;
    return cancelled;
  };
  xml::event_validator validator{{}, &context};
  validator.accept(boundary(xml::event_kind::start_document));
  cancelled = true;
  EXPECT_THROW(validator.accept(element_event(xml::event_kind::start_element, {"r", ""})),
      std::runtime_error);
  const auto interrupted_checks = checks;
  cancelled = false;
  expect_fenced(validator);
  EXPECT_EQ(checks, interrupted_checks);
  xml::operation_context expired{};
  expired.set_timeout(std::chrono::steady_clock::duration::zero());
  xml::event_validator deadline{{}, &expired};
  EXPECT_THROW(deadline.accept(boundary(xml::event_kind::start_document)), std::runtime_error);
  expect_fenced(deadline);
  xml::event_validator finish_cancelled{{}, &context};
  accept_empty_root(finish_cancelled);
  cancelled = true;
  EXPECT_THROW(finish_cancelled.finish(), std::runtime_error);
  cancelled = false;
  expect_fenced(finish_cancelled);
}
