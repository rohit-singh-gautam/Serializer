#pragma once

#include <rohit/serializer_creator.hpp>
#include <ranges>

namespace rohit::serializer::writer {

// Keep memory-only fields in models while excluding them from durable wire operations.
inline auto wire_members(const class_node& value) {
  return value.member_list | std::views::filter([](const member& field) {
    return !field.transient;
  });
}

} // namespace rohit::serializer::writer
