// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/xml.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rohit::serializer::xml {

// Bound compilation and matching independently of an optional regular-expression provider.
struct pattern_limits {
  std::size_t max_expression_bytes{16384U};
  std::size_t max_states{16384U};
  std::size_t max_nesting_depth{64U};
  std::size_t max_repetitions{4096U};
  std::size_t max_input_bytes{8U * 1024U * 1024U};
  std::size_t max_work_units{64U * 1024U * 1024U};
};

// Inclusive Unicode scalar ranges are independent of each target language's regex dialect.
struct pattern_range {
  std::uint32_t first{}, last{};
};

// Thompson states contain epsilon edges or one character predicate and its successor.
struct pattern_state {
  std::vector<std::size_t> epsilon{};
  std::vector<pattern_range> ranges{};
  std::size_t next{};
  bool consumes{}, inverted{}, dot{}, accepting{};
};

// Immutable compiled portable XSD 1.0 regex; copies share read-only instructions safely.
class portable_pattern {
  std::shared_ptr<const std::vector<pattern_state>> states_{};
  std::size_t start_{};
  // Compile checked instructions; clients obtain programs through compile_portable_pattern.
  portable_pattern(std::vector<pattern_state> states, std::size_t start);
  friend portable_pattern compile_portable_pattern(std::string_view, const pattern_limits&,
      operation_context*);
public:
  // Expose immutable target-independent instructions for backend lowering and inspection.
  [[nodiscard]] std::span<const pattern_state> states() const noexcept;
  // Return the initial instruction index in states().
  [[nodiscard]] std::size_t start() const noexcept;
  // Match the entire valid XML UTF-8 string with bounded, non-backtracking execution.
  [[nodiscard]] bool matches(std::string_view text, const pattern_limits& limits = {},
      operation_context* context = nullptr) const;
};

// Support XSD literals, dot, groups, alternatives, repetitions, class subtraction and set escapes.
// General categories/blocks use frozen Unicode 17 data with historical XSD block spellings;
// i/c name classes use XML 1.0 fifth-edition names. Lookaround and backreferences reject.
// ^ and $ are literal characters, as in XSD 1.0, rather than host-regex anchors.
[[nodiscard]] portable_pattern compile_portable_pattern(std::string_view expression,
    const pattern_limits& limits = {}, operation_context* context = nullptr);

} // namespace rohit::serializer::xml
