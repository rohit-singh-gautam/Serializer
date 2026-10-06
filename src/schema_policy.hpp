#pragma once

#include <rohit/serializer_creator.hpp>

#include <string>

namespace rohit::serializer::schema_policy {

// Pin one checked UTC date before parsing an entry and all its included schemas.
parser::parse_options capture_options(const parser::parse_options& options);

// Validate compiler-only release metadata and resolve policy trees to ordinary version floors.
// Warnings use the supplied callback; promoted warnings abort before generated output is published.
void resolve(std::vector<std::unique_ptr<syntax_node>>& statements,
             const parser::parse_options& options);

} // namespace rohit::serializer::schema_policy
