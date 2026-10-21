// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/package.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rohit::serializer::package {

inline constexpr std::string_view core_properties_content_type =
    "application/vnd.openxmlformats-package.core-properties+xml";
inline constexpr std::string_view core_properties_relationship_type =
    "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties";
inline constexpr std::string_view default_core_properties_part = "/docProps/core.xml";

struct core_property_keyword {
  std::string value{};
  std::optional<std::string> language{};
  // Compare an individual localized keyword's text and declared language.
  bool operator==(const core_property_keyword&) const = default;
};

// Fifth-edition keywords retain ordered text and localized value elements without inventing delimiters.
struct core_property_keywords {
  std::optional<std::string> language{};
  std::vector<std::variant<std::string, core_property_keyword>> content{};
  // Compare ordered keyword content and its optional enclosing language.
  bool operator==(const core_property_keywords&) const = default;
};

// Standard fifth-edition OPC metadata belongs to the application. Missing and present-empty strings
// are distinct; timestamps retain their original precision/offset and are never set implicitly.
struct core_properties {
  std::optional<std::string> category{}, content_status{}, created{}, creator{};
  std::optional<std::string> description{}, identifier{}, language{}, last_modified_by{};
  std::optional<std::string> last_printed{}, modified{}, revision{}, subject{}, title{}, version{};
  std::optional<core_property_keywords> keywords{};
  // Compare semantic property values without depending on XML prefixes or element ordering.
  bool operator==(const core_properties&) const = default;
};

// Encode all standard properties as deterministic UTF-8 XML under a cumulative caller budget.
// created/modified use the imported W3CDTF gYear/gYearMonth/date/dateTime union;
// last_printed uses XML Schema dateTime. Lexical spelling and date precision are retained.
// Unknown properties, markup compatibility and arbitrary property extensions have no adapter.
[[nodiscard]] std::string encode_core_properties(const core_properties& value, budget& shared,
    std::string_view part_name = default_core_properties_part);

// Decode one bounded UTF-8 or BOM-marked UTF-16 core-properties document into a fresh candidate.
// Reject duplicate/foreign properties, attributes, nested content and invalid date/type values.
[[nodiscard]] core_properties decode_core_properties(std::string_view bytes, budget& shared,
    std::string_view part_name = default_core_properties_part);

// Add application-supplied values and one root relationship; call once per OPC writer and finish
// the writer normally. Standard full-Save staging supplies file-level transactional publication.
void write_core_properties(opc_writer& writer, const core_properties& value,
    std::string_view part_name = default_core_properties_part,
    std::string_view relationship_id = "core-properties", entry_options storage = {});

// Discover the optional singleton through its standard root relationship, not a fixed filename.
// Mark the part consumed in the caller's loader; wrong types/modes/cardinality fail before return.
[[nodiscard]] std::optional<core_properties> read_core_properties(profile_loader& loader);

} // namespace rohit::serializer::package
