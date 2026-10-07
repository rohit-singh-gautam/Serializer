//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <rohit/constant_binary_output.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace rohit::serializer {
namespace detail {
// Use the unchanged optimized runtime protocol for existing-memory output.
template <class Value>
std::size_t serialize_constant_binary_runtime(std::span<std::uint8_t> destination,
                                              const Value& value) {
  rohit::full_stream output{destination.data(), destination.size()};
  binary_none<serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  return output.current_offset();
}

} // namespace detail

// Preserve optimized owning runtime output; constant evaluation uses the shared memory traversal.
template <class Value>
  requires(!detail::has_emission_binary_support<Value>() && !detail::constant_binary_span<Value>)
[[nodiscard]] constexpr std::size_t serialize_binary_none_to(std::span<std::uint8_t> destination,
                                                             const Value& value) {
  if (!std::is_constant_evaluated()) {
    return detail::serialize_constant_binary_runtime(destination, value);
  }
  return detail::serialize_constant_binary_to(destination, value);
}

} // namespace rohit::serializer
