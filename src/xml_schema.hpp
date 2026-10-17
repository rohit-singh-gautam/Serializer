// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <filesystem>
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
  bool text{false};
  std::optional<xml::name> wrapper{};
};
// One complete generated XML contract, independent of other format layouts.
struct record {
  xml::name root{};
  std::vector<field> fields{};
};
// One reader-direction incompatibility between two fully bound XML contracts.
struct compatibility_diagnostic {
  std::string member{};
  std::string message{};
};
// Describe whether the current reader accepts every document of the previous bound contract.
[[nodiscard]] std::vector<compatibility_diagnostic> compare_contracts(
    const record& previous, const record& current);
struct schema_bundle;
// Read and prepare a supported fixed XSD subset once, or retain deterministic configured mapping.
class schema {
  xml_options options_;
  std::shared_ptr<const schema_bundle> bundle_{};
public:
  // Read a fixed, bounded local XSD bundle; unsupported grammar fails before generation.
  explicit schema(const xml_options& options);
  // Bind all durable fields exactly; XSD placement/types/order take authority over config defaults.
  [[nodiscard]] record bind(const class_node& source) const;
  // Return the canonical root and transitive local bundle inputs, ordered by relative path.
  [[nodiscard]] std::vector<std::filesystem::path> dependencies() const;
  // Identify exact sorted relative paths and bytes with SHA-256, independent of checkout location.
  [[nodiscard]] std::string bundle_identity() const;
  // Export one namespace's actual bound scalar/nested record contract; reject inexpressible mappings.
  [[nodiscard]] std::string export_xsd(const std::vector<const class_node*>& sources) const;
};
} // namespace rohit::serializer::writer::xml_binding
