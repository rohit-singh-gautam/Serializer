// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>

#include <algorithm>

namespace rohit::serializer::parser {

// Recognize generated passive records without trusting opaque native storage or dispatch.
inline bool is_stateless_record(const class_node& value) {
  return value.storage_modes == static_cast<std::uint8_t>(storage_mode::owning) &&
      (value.attributes & class_attributes::packed) == class_attributes::none &&
      std::all_of(value.member_list.begin(), value.member_list.end(), [](const auto& field) {
        return field.wire_only;
      }) && value.parents.empty() && !value.supports_managed() &&
      value.native_blocks.empty() &&
      std::none_of(value.functions.begin(), value.functions.end(), [](const auto& method) {
        return method.dispatch != function_declaration::dispatch_type::ordinary;
      });
}

} // namespace rohit::serializer::parser
