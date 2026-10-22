// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
struct schema_bundle;
// Import policy identifies generated declarations and the retained fixed schema dependency.
struct import_options {
  std::string namespace_name{"imported"};
  std::string root_name{};
  std::string schema_location{};
};
// Import is side-effect free; the CLI owns transactional publication of generated source.
struct import_result {
  std::string schema_text{};
  std::vector<std::filesystem::path> dependencies{};
  std::string bundle_identity{};
};
// Import a checked local XSD subset into stable-ID owning models with inline XML mappings.
// Unsupported constraints fail rather than producing a weaker contract; no resource is fetched.
[[nodiscard]] import_result import_xsd(const std::filesystem::path& input,
                                      const import_options& options = {});
// Reuse an already pinned bundle without file access; options.schema_location supplies its entry identity.
[[nodiscard]] import_result import_xsd(const schema_bundle& bundle, const import_options& options);
} // namespace rohit::serializer::writer::xml_binding
