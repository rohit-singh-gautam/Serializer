// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_events.hpp>

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>

namespace rohit::serializer::xml {
namespace {

// Check before addition so even caller-selected size_t maximum bounds cannot wrap.
void charge(std::size_t& used, std::size_t amount, std::size_t maximum, operation_context* context = nullptr) {
  if (used > maximum || amount > maximum - used) {
    reject_limit(context, "XML event resource limit exceeded", amount, maximum - std::min(used, maximum));
  }
  used += amount;
}

// Detect unexpected data even when an event has no meaningful expanded name.
bool has_name(const event_name& value) noexcept {
  return !value.local.empty() || !value.namespace_uri.empty();
}

// Bound all borrowed string bytes before allocating checker state or invoking consumers.
std::size_t payload_bytes(const event_view& value, std::size_t maximum, operation_context* context) {
  std::size_t bytes{};
  charge(bytes, value.qualified_name.local.size(), maximum, context);
  charge(bytes, value.qualified_name.namespace_uri.size(), maximum, context);
  charge(bytes, value.text.size(), maximum, context);
  for (const auto& attribute : value.attributes) {
    charge(bytes, attribute.qualified_name.local.size(), maximum, context);
    charge(bytes, attribute.qualified_name.namespace_uri.size(), maximum, context);
    charge(bytes, attribute.value.size(), maximum, context);
  }
  for (const auto& declaration : value.namespaces) {
    charge(bytes, declaration.prefix.size(), maximum, context);
    charge(bytes, declaration.namespace_uri.size(), maximum, context);
  }
  return bytes;
}

// Sort borrowed indexes so duplicate checking stays O(n log n) without copying payloads.
void check_duplicates(const event_view& value, std::size_t index_count) {
  if (index_count < 2U) {
    return;
  }
  std::vector<std::size_t> indexes(index_count);
  std::iota(indexes.begin(), indexes.end(), std::size_t{});
  const auto attribute_end = indexes.begin() + value.attributes.size();
  const auto attribute_key = [&value](std::size_t index) {
    const auto& qualified_name = value.attributes[index].qualified_name;
    return std::tie(qualified_name.namespace_uri, qualified_name.local);
  };
  std::sort(indexes.begin(), attribute_end, [&attribute_key](auto left, auto right) {
    return attribute_key(left) < attribute_key(right);
  });
  for (std::size_t index{1U}; index < value.attributes.size(); ++index) {
    if (attribute_key(indexes[index - 1U]) == attribute_key(indexes[index])) {
      throw std::invalid_argument{"Duplicate XML event expanded attribute name"};
    }
  }
  std::iota(indexes.begin(), indexes.end(), std::size_t{});
  const auto namespace_end = indexes.begin() + value.namespaces.size();
  std::sort(indexes.begin(), namespace_end, [&value](auto left, auto right) {
    return value.namespaces[left].prefix < value.namespaces[right].prefix;
  });
  for (std::size_t index{1U}; index < value.namespaces.size(); ++index) {
    if (value.namespaces[indexes[index - 1U]].prefix ==
        value.namespaces[indexes[index]].prefix) {
      throw std::invalid_argument{"Duplicate XML event namespace prefix"};
    }
  }
}

} // namespace

// Store immutable bounds and the caller-owned cumulative operation context.
event_validator::event_validator(event_limits bounds, operation_context* context)
    : bounds_{bounds}, context_{context} {}

// Fence all failures, including user callbacks, before a caller can submit another event.
void event_validator::accept(const event_view& value) {
  if (failed()) {
    throw std::logic_error{"XML event validator has failed"};
  }
  try {
    accept_event(value);
  } catch (...) {
    phase_ = phase::failed;
    throw;
  }
}

// Enforce normalized event shape, exact document framing and cumulative admission bounds.
void event_validator::accept_event(const event_view& value) {
  if (phase_ == phase::after_document) {
    throw std::invalid_argument{"XML event after end_document"};
  }
  if (context_) {
    context_->check();
  }
  const bool starts_element = value.kind == event_kind::start_element;
  const bool ends_element = value.kind == event_kind::end_element;
  const bool is_text = value.kind == event_kind::text;
  if ((!starts_element && (!value.attributes.empty() || !value.namespaces.empty())) ||
      (!is_text && !value.text.empty()) ||
      (!starts_element && !ends_element && has_name(value.qualified_name)) ||
      ((starts_element || ends_element) && value.qualified_name.local.empty()) ||
      (is_text && value.text.empty())) {
    throw std::invalid_argument{"Invalid XML event payload shape"};
  }
  if (phase_ == phase::before_document && value.kind != event_kind::start_document) {
    throw std::invalid_argument{"XML events require start_document first"};
  }
  charge(usage_.events, 1U, bounds_.max_events, context_);
  // Reject descriptor counts before scanning their strings or allocating duplicate indexes.
  if (starts_element) {
    if (open_elements_.empty() && root_seen_) {
      throw std::invalid_argument{"XML events require exactly one root"};
    }
    if (open_elements_.size() >= bounds_.document.max_depth) {
      reject_limit(context_, "XML event depth limit exceeded", 1U, 0U);
    }
    charge(usage_.nodes, 1U, bounds_.document.max_nodes, context_);
    charge(usage_.attributes, value.attributes.size(), bounds_.document.max_attributes, context_);
    charge(usage_.namespace_bindings, value.namespaces.size(),
        bounds_.document.max_namespace_bindings, context_);
  }
  if (context_) {
    context_->consume(operation_resource::work_units, 1U);
    context_->consume(operation_resource::work_units, value.attributes.size());
    context_->consume(operation_resource::work_units, value.namespaces.size());
  }
  const auto bytes = payload_bytes(value, bounds_.max_event_bytes, context_);
  if (context_) {
    context_->consume(operation_resource::work_units, bytes);
  }
  switch (value.kind) {
  case event_kind::start_document:
    if (phase_ != phase::before_document) {
      throw std::invalid_argument{"Repeated XML start_document"};
    }
    phase_ = phase::in_document;
    break;
  case event_kind::start_element: {
    // Count identities once when introduced, rather than counting end-name repetitions.
    charge(usage_.name_bytes, value.qualified_name.local.size(), bounds_.document.max_name_bytes, context_);
    charge(usage_.name_bytes, value.qualified_name.namespace_uri.size(),
        bounds_.document.max_name_bytes, context_);
    for (const auto& attribute : value.attributes) {
      if (attribute.qualified_name.local.empty()) {
        throw std::invalid_argument{"XML event attribute requires a local name"};
      }
      charge(usage_.name_bytes, attribute.qualified_name.local.size(),
          bounds_.document.max_name_bytes, context_);
      charge(usage_.name_bytes, attribute.qualified_name.namespace_uri.size(),
          bounds_.document.max_name_bytes, context_);
      charge(usage_.text_bytes, attribute.value.size(), bounds_.document.max_text_bytes, context_);
    }
    for (const auto& declaration : value.namespaces) {
      charge(usage_.name_bytes, declaration.prefix.size(), bounds_.document.max_name_bytes, context_);
      charge(usage_.name_bytes, declaration.namespace_uri.size(), bounds_.document.max_name_bytes, context_);
    }
    std::size_t frame_bytes{sizeof(name)};
    charge(frame_bytes, value.qualified_name.local.size(), bounds_.max_state_bytes, context_);
    charge(frame_bytes, value.qualified_name.namespace_uri.size(), bounds_.max_state_bytes, context_);
    const auto index_count = std::max(value.attributes.size(), value.namespaces.size());
    std::size_t scratch_bytes{};
    if (index_count >= 2U) {
      if (index_count > bounds_.max_state_bytes / sizeof(std::size_t)) {
        reject_limit(context_, "XML event duplicate index limit exceeded");
      }
      scratch_bytes = index_count * sizeof(std::size_t);
    }
    auto state_bytes = usage_.state_bytes;
    charge(state_bytes, frame_bytes, bounds_.max_state_bytes, context_);
    charge(state_bytes, scratch_bytes, bounds_.max_state_bytes, context_);
    if (context_) {
      context_->consume(operation_resource::allocation_bytes, frame_bytes);
      context_->consume(operation_resource::allocation_bytes, scratch_bytes);
    }
    check_duplicates(value, index_count);
    // Only the open expanded name survives this call; no attribute/text buffers are retained.
    open_elements_.push_back({std::string{value.qualified_name.local},
        std::string{value.qualified_name.namespace_uri}});
    usage_.state_bytes += frame_bytes;
    root_seen_ = true;
    break;
  }
  case event_kind::text:
    if (open_elements_.empty()) {
      throw std::invalid_argument{"XML text event outside the root"};
    }
    charge(usage_.text_bytes, value.text.size(), bounds_.document.max_text_bytes, context_);
    break;
  case event_kind::end_element:
    if (open_elements_.empty() ||
        open_elements_.back().local != value.qualified_name.local ||
        open_elements_.back().namespace_uri != value.qualified_name.namespace_uri) {
      throw std::invalid_argument{"Mismatched XML end_element"};
    }
    usage_.state_bytes -= sizeof(name) + open_elements_.back().local.size() +
        open_elements_.back().namespace_uri.size();
    open_elements_.pop_back();
    break;
  case event_kind::end_document:
    if (!root_seen_ || !open_elements_.empty()) {
      throw std::invalid_argument{"Incomplete XML event document"};
    }
    phase_ = phase::after_document;
    break;
  default:
    throw std::invalid_argument{"Unknown XML event kind"};
  }
}

// Successful root closure alone is insufficient: the producer must explicitly certify EOF.
void event_validator::finish() {
  if (failed()) {
    throw std::logic_error{"XML event validator has failed"};
  }
  try {
    if (context_) {
      context_->check();
    }
    if (!complete()) {
      throw std::invalid_argument{"Missing XML end_document"};
    }
  } catch (...) {
    phase_ = phase::failed;
    throw;
  }
}

// Completion is revoked by any later invalid submission or interruption.
bool event_validator::complete() const noexcept {
  return phase_ == phase::after_document;
}

// A failed pass cannot be reused or publish a partially validated document.
bool event_validator::failed() const noexcept {
  return phase_ == phase::failed;
}

// Expose counters without copying storage or resetting cumulative operation charges.
const event_usage& event_validator::usage() const noexcept {
  return usage_;
}

} // namespace rohit::serializer::xml
