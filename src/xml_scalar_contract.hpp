// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml_pattern.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {
struct scalar_facets;
struct scalar_value_identity;

// Immutable compiled restrictions own all lexical bounds, enums and portable pattern programs.
// Copies share read-only state; synchronous validation borrows only the supplied XML scope.
class compiled_scalar_contract {
  struct implementation;
  std::shared_ptr<const implementation> implementation_{};
  // Accept a complete compiled candidate only after every facet has been checked.
  explicit compiled_scalar_contract(std::shared_ptr<const implementation> implementation);
  // Preserve the first successful primitive leaf and its normalized lexical value for outer union facets.
  void validate_selected(std::string_view text, const xml::element& scope, xml::operation_context* context,
      const scalar_facets** selected_leaf, std::string* selected_lexical) const;
  friend compiled_scalar_contract compile_scalar_contract(const scalar_facets&, bool,
      const xml::pattern_limits&);
public:
  // State whether every pattern can be lowered to the documented provider-independent subset.
  [[nodiscard]] bool patterns_portable() const noexcept;
  // Check exact bounds, Unicode code-point lengths, value-space enums and all pattern groups.
  void validate(std::string_view text, const xml::element& scope = {},
      xml::operation_context* context = nullptr) const;
  // Resolve one complete value's selected primitive identity in its own schema or instance scope.
  [[nodiscard]] scalar_value_identity identify(std::string_view text, const xml::element& scope = {},
      xml::operation_context* context = nullptr) const;
};

// Reject inconsistent numeric/length restrictions and invalid portable patterns before publication.
// Set require_portable_patterns=false only for legacy provider-backed patterns; their status is explicit.
[[nodiscard]] compiled_scalar_contract compile_scalar_contract(const scalar_facets& facets,
    bool require_portable_patterns = true, const xml::pattern_limits& limits = {});
} // namespace rohit::serializer::writer::xml_binding
