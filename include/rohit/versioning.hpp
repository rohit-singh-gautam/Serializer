#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace rohit::serializer {

// Select accepted schema revisions and protocol-supported handling of unknown fields.
enum class read_policy { strict, compatible, flexible };

namespace detail {
// Custom codecs may opt into canonical dotted JSON strings with the same marker.
template <typename Protocol>
inline constexpr bool is_json_protocol = [] {
  if constexpr (requires { Protocol::is_json; }) {
    return Protocol::is_json;
  } else {
    return false;
  }
}();
} // namespace detail

// An ordered version with a schema-fixed count of unsigned 16-bit components.
template <std::size_t N>
struct dotted_version {
  static_assert(N >= 2 && N <= 4);
  std::array<std::uint16_t, N> components{};

  // Initialize all components to zero.
  constexpr dotted_version() = default;
  // Initialize integer components, rejecting narrowing rather than wrapping their values.
  template <typename... Components>
    requires(sizeof...(Components) == N && (std::integral<Components> && ...))
  constexpr dotted_version(Components... values)
      : components{static_cast<std::uint16_t>(values)...} {
    if (!(std::in_range<std::uint16_t>(values) && ...)) {
      throw std::invalid_argument{"Dotted version component exceeds uint16"};
    }
  }
  // Compare components numerically, never as decimal strings or floating point.
  constexpr auto operator<=>(const dotted_version&) const = default;

  // Parse exactly N canonical decimal components; invalid input throws before returning a value.
  static dotted_version parse(std::string_view text) {
    dotted_version result{};
    for (std::size_t index = 0; index < N; ++index) {
      const auto separator = text.find('.');
      const auto part = text.substr(0, separator);
      if (part.empty() || (part.size() > 1 && part.front() == '0')) {
        throw std::invalid_argument{"Invalid dotted version component"};
      }
      const auto parsed =
          std::from_chars(part.data(), part.data() + part.size(), result.components[index]);
      if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() ||
          (index + 1 < N ? separator == std::string_view::npos
                         : separator != std::string_view::npos)) {
        throw std::invalid_argument{"Invalid dotted version"};
      }
      if (separator != std::string_view::npos) {
        text.remove_prefix(separator + 1);
      }
    }
    return result;
  }

  // Return the canonical JSON spelling, with no leading zeros.
  std::string to_string() const {
    std::string result{};
    for (const auto component : components) {
      if (!result.empty()) {
        result += '.';
      }
      result += std::to_string(component);
    }
    return result;
  }

  // Decode a JSON string or a fixed sequence of binary components without a count prefix.
  void serialize_in(auto& protocol) {
    if constexpr (detail::is_json_protocol<std::remove_cvref_t<decltype(protocol)>>) {
      std::string text{};
      protocol.serialize_in(text);
      *this = parse(text);
    } else {
      for (auto& component : components) {
        protocol.serialize_in(component);
      }
    }
  }

  // Encode canonical JSON text or the schema-fixed binary components in protocol byte order.
  void serialize_out(auto& protocol) const {
    if constexpr (detail::is_json_protocol<std::remove_cvref_t<decltype(protocol)>>) {
      protocol.serialize_out(to_string());
    } else {
      for (const auto component : components) {
        protocol.serialize_out(component);
      }
    }
  }
};

using version2 = dotted_version<2>;
using version3 = dotted_version<3>;
using version4 = dotted_version<4>;

namespace detail {
// Custom protocols without a policy retain strict revision checking.
template <typename Protocol>
constexpr read_policy protocol_read_policy() {
  if constexpr (requires { Protocol::policy; }) {
    return Protocol::policy;
  } else {
    return read_policy::strict;
  }
}

// Floating revisions exclude nonfinite and negative values in every policy.
template <typename Version>
bool valid_version(const Version& value) {
  if constexpr (std::is_floating_point_v<Version>) {
    return std::isfinite(value) && value >= 0;
  } else {
    return true;
  }
}
} // namespace detail
} // namespace rohit::serializer
