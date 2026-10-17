// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
// Reject misspelled/unused XML class selectors before any backend publishes generated output.
void validate_options(const std::vector<std::unique_ptr<syntax_node>>& statements, const xml_options& options);
// One checked, statically bound XML field in encoder/decoder sequence order.
struct field {
  const member* source{};
  xml::name qualified_name{};
  bool attribute{false};
  std::size_t min_occurs{1U};
  std::optional<std::size_t> max_occurs{1U};
  bool repeated{false};
};
// One complete generated XML contract, independent of other format layouts.
struct record {
  xml::name root{};
  std::vector<field> fields{};
};
// Read and prepare a supported fixed XSD subset once, or retain deterministic configured mapping.
class schema {
  xml_options options_;
  std::optional<xml::element> document_{};
public:
  // Read an optional fixed standalone XSD; unsupported schema grammar fails before generation.
  explicit schema(const xml_options& options);
  // Bind all durable fields exactly; XSD placement/types/order take authority over config defaults.
  [[nodiscard]] record bind(const class_node& source) const;
};
} // namespace rohit::serializer::writer::xml_binding
