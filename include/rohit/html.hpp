// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::html {

enum class event_kind { start_fragment, start_element, text, end_element, end_fragment };

struct attribute {
  std::string name{};
  std::string value{};
  bool boolean{};
};

// HTML events own their strings; they never carry XML namespaces, QName identity or xsi metadata.
// Void tags produce paired start/end events but output uses ordinary HTML void-element syntax.
struct event {
  event_kind kind{event_kind::start_fragment};
  std::string name{};
  std::vector<attribute> attributes{};
  std::string text{};
};

struct limits {
  std::size_t max_input_bytes{8U * 1024U * 1024U};
  std::size_t max_output_bytes{16U * 1024U * 1024U};
  std::size_t max_events{200000U};
  std::size_t max_depth{128U};
  std::size_t max_attributes_per_element{256U};
  std::size_t max_text_bytes{8U * 1024U * 1024U};
};

// Report the exact approved fragment subset; unsupported browser recovery is deliberately rejected.
[[nodiscard]] std::string_view supported_subset() noexcept;
// Parse bounded UTF-8 HTML fragments transactionally, preserving ordinary HTML void/attribute syntax.
// No XML parser, DOM, script execution, network access or implicit element-close recovery is used.
[[nodiscard]] std::vector<event> parse_fragment(std::string_view bytes, const limits& bounds = {},
                                                xml::operation_context* context = nullptr);
// Validate an owned event trace and return deterministic HTML, with quoted/escaped attribute values.
// Caller-retained borrowed projections must be copied into these owning events before owner expiry.
[[nodiscard]] std::string write_fragment(std::span<const event> events, const limits& bounds = {},
                                         xml::operation_context* context = nullptr);

} // namespace rohit::serializer::html
