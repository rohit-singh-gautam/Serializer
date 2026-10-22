// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/document_profile.hpp>
#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::package_codegen {

// A generated package contract retains role order and statically expected payload identities.
struct package_profile_contract {
  std::string identity{};
  document::container_format container{document::container_format::opc};
  std::vector<document::part_profile> parts{};
};

// Apply one explicit package-qualified INI role profile without altering model fields or wire identities.
[[nodiscard]] package_node configure_package(const package_node& source, const xml_options& options);
// Reject unused package/role selectors before any generated output is published.
void validate_package_options(const std::vector<std::unique_ptr<syntax_node>>& statements,
                              const xml_options& options);

// Compile existing schema package declarations to portable profile/dispatch metadata.
// Typed XML roots use the same authoritative binder as their generated codecs.
[[nodiscard]] package_profile_contract compile_package_profile(const package_node& source,
                                                               const xml_options& options);
// Emit the C++ facade's owning metadata factory; no provider or model is accessed by the factory.
[[nodiscard]] std::string cpp_profile_metadata(const package_node& source,
                                               const xml_options& options,
                                               std::string_view generated_owner_name = {});

} // namespace rohit::serializer::writer::package_codegen
