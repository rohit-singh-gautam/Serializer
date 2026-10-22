// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml_extensions.hpp>

#include <cstddef>
#include <stdexcept>

namespace rohit::serializer::xml::detail {

// Check the next XSD occurrence without changing authored preservation-resource ceilings.
inline void validate_wildcard_occurrence(const extension_slot_contract& slot, std::size_t count) {
  if (slot.namespace_match != extension_namespace_match::explicit_foreign &&
      slot.namespace_match != extension_namespace_match::foreign_fallback &&
      slot.maximum && count >= *slot.maximum) {
    throw std::invalid_argument{"XML wildcard occurrence maximum exceeded"};
  }
}

} // namespace rohit::serializer::xml::detail
