// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
// Portable scalar restrictions retained from fixed XSD declarations for generated checks.
struct scalar_facets {
  std::optional<std::string> min_inclusive{}, max_inclusive{}, min_exclusive{}, max_exclusive{};
  std::optional<std::size_t> length{}, min_length{}, max_length{};
  std::vector<std::string> enumeration{};
  std::string whitespace{};
  bool finite{};
  std::string lexical_kind{};
  std::vector<std::string> patterns{};
  // Restrictions intersect groups; alternatives within one restriction's group form a union.
  std::vector<std::vector<std::string>> pattern_groups{};
  std::string list_item_kind{};
  std::vector<xml::name> qname_enumeration{};
  // Union branches are independent complete lexical contracts, never intersected facets.
  std::vector<scalar_facets> union_members{};
  // Compare the static validation contract without inspecting generated model values.
  bool operator==(const scalar_facets&) const = default;
};
// Reject misspelled/unused XML class selectors before any backend publishes generated output.
void validate_options(const std::vector<std::unique_ptr<syntax_node>>& statements,
                      const xml_options& options, std::string_view language = "cpp");
// Enable only schema-authored XML/package use, respecting an explicit generator opt-out.
[[nodiscard]] xml_options effective_options(
    const std::vector<std::unique_ptr<syntax_node>>& statements, xml_options options);
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
  scalar_facets facets{};
  std::optional<std::string> default_value{}, fixed_value{};
  bool list{}, qname{}, prefix_list{};
  bool value_projection{};
  std::optional<xml::name> qname_default{}, qname_fixed{};
};
// One choice branch retains the occurrence interval for a known field or positioned wildcard.
struct choice_branch {
  const member* source{};
  std::optional<std::size_t> wildcard_index{};
  std::size_t minimum{1U};
  std::optional<std::size_t> maximum{1U};
};
// Singleton choices use optional alternatives; repeated groups retain their full branch contract.
struct choice_group {
  std::vector<const member*> alternatives{};
  std::size_t minimum{1U}, maximum{1U};
  bool repeated{};
  std::size_t before_field{};
  std::optional<std::size_t> max_repetitions{1U};
  std::vector<choice_branch> branches{};
};
// Wildcards retain their namespace/process contract instead of silently deleting foreign content.
struct wildcard {
  bool attribute{};
  std::string namespace_rule{"##any"};
  std::string process_contents{"strict"};
  std::size_t minimum{1U};
  std::optional<std::size_t> maximum{1U};
  std::size_t before_field{};
  std::optional<std::string> target_namespace{};
};
// One complete generated XML contract, independent of other format layouts.
struct record {
  xml::name root{};
  std::vector<field> fields{};
  std::vector<choice_group> choices{};
  std::vector<wildcard> wildcards{};
  std::optional<xml_unknown_policy> unknown{};
};
// Reject newly bound semantics until the selected backend emits their complete validation contract.
void validate_backend(const record& contract, std::string_view language);
// One reader-direction incompatibility between two fully bound XML contracts.
struct compatibility_diagnostic {
  std::string member{};
  std::string message{};
};
// Describe whether the current reader accepts every document of the previous bound contract.
[[nodiscard]] std::vector<compatibility_diagnostic> compare_contracts(
    const record& previous, const record& current);
// One pinned local bundle, shared by the optimized binder and schema importer.
struct schema_bundle {
  struct document {
    std::filesystem::path path{};
    std::string bytes{};
    std::string target{};
    xml::element root{};
  };
  std::vector<document> documents{};
  std::string identity{};
};
// Resolve a simple declaration once for both native type selection and generated validation.
struct scalar_type {
  xml::name primitive{};
  scalar_facets facets{};
};
[[nodiscard]] scalar_type resolve_scalar_type(const schema_bundle& bundle,
                                             const xml::element& source);
// Preserve the defining document when expanding inherited or grouped declarations.
struct schema_component {
  const schema_bundle::document* owner{};
  const xml::element* node{};
};
struct complex_content {
  bool mixed{};
  std::vector<schema_component> children{};
};
// Expand static complex-type extension and attribute groups without changing qualified identities.
[[nodiscard]] complex_content expand_complex_type(const schema_bundle& bundle,
    const schema_bundle::document& owner, const xml::element& source);
// Resolve a model group to its original compositor, retaining its qualification defaults.
[[nodiscard]] schema_component resolve_model_group(const schema_bundle& bundle,
    const schema_bundle::document& owner, const xml::element& reference);
// Resolve only a bounded local bundle; importer mode performs its own exact subset checks.
[[nodiscard]] std::shared_ptr<const schema_bundle> read_schema_bundle(
    const std::filesystem::path& path, bool optimized_subset = true);
// Read and prepare a supported fixed XSD subset once, or retain deterministic configured mapping.
class schema {
  xml_options options_;
  std::shared_ptr<const schema_bundle> bundle_{};
  mutable std::map<std::filesystem::path, std::shared_ptr<const schema_bundle>> inline_bundles_{};
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
