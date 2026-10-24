#pragma once

#include <rohit/serializer_creator.hpp>
#include <ranges>
#include <stdexcept>

namespace rohit::serializer::writer {

// Reject bit fields in outputs whose address/lifetime mapping has no qualified contract.
inline void validate_bit_field_output(const std::vector<std::unique_ptr<syntax_node>>& nodes,
                                     bool supported) {
  for (const auto& node : nodes) {
    if (node->type == object_type::namespace_type) {
      validate_bit_field_output(static_cast<const namespace_node&>(*node).statements, supported);
    } else if (!supported && node->type == object_type::class_type) {
      for (const auto& field : static_cast<const class_node&>(*node).member_list) {
        if (field.bit_width != 0) {
          throw std::invalid_argument{"Bit fields currently require C++ owning native codecs: " +
              node->get_full_name() + "." + field.name};
        }
      }
    }
  }
}

// Keep memory-only fields in models while excluding them from durable wire operations.
inline auto wire_members(const class_node& value) {
  return value.member_list | std::views::filter([](const member& field) {
    return !field.transient;
  });
}

} // namespace rohit::serializer::writer
