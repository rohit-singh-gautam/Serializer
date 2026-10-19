// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_binding {

enum class map_key_placement { element, attribute };
enum class map_key_kind { boolean, signed_integer, unsigned_integer, string, enumeration };

// A map owns a required collection wrapper and one complete key/value pair per repeated entry.
// Copied native type metadata borrows resolved nodes; the parsed source tree must outlive this IR.
struct map_contract {
  xml::name wrapper{}, entry{}, key{}, value{};
  map_key_placement key_placement{map_key_placement::element};
  type_name key_type{std::string{}, nullptr}, value_type{std::string{}, nullptr};
  map_key_kind key_kind{map_key_kind::string};
  std::vector<std::string> key_enumerators{};
  bool optional_container{};
};

// Bind legacy and generic maps without changing native members or their wire identities.
[[nodiscard]] map_contract bind_map_contract(const member& source,
    const std::optional<xml::name>& wrapper, const xml::name& entry, const xml::name& key,
    const xml::name& value, map_key_placement key_placement);

// Canonicalize the native key value; invalid lexical values and native-width overflow reject.
[[nodiscard]] std::string canonical_map_key(const map_contract& contract, std::string_view lexical);

// Bound reference validation independently of the XML parser's own tree/document limits.
struct map_validation_limits {
  std::size_t max_entries{1024U};
  // Each key charges the larger of its lexical and canonical byte lengths, cumulatively.
  std::size_t max_key_bytes{1024U * 1024U};
};

// Validate entry/cardinality/presence/duplicate-key rules; nested record validation is separate.
void validate_map_entries(const map_contract& contract, const xml::element& wrapper,
    const map_validation_limits& limits = {}, xml::operation_context* context = nullptr);

enum class variant_selection { element, xsi_type };

// Override one stable native alternative alias using expanded XML identities.
struct variant_tag_override {
  std::string alternative{};
  xml::name tag{};
  std::optional<xml::name> type_identity{};
};

// One alternative retains its native identity independently of prefixes or generated naming styles.
// Native type resolution borrows the parsed source tree for the lifetime of this IR.
struct variant_alternative {
  std::string alias{}, native_identity{};
  type_name native_type{std::string{}, nullptr};
  xml::name tag{}, type_identity{};
};

// Exactly one payload child selects one alternative inside the required field wrapper.
struct variant_contract {
  xml::name wrapper{};
  variant_selection selection{variant_selection::element};
  std::vector<variant_alternative> alternatives{};
};

// Bind explicit xsi:type inventories or stable element aliases, rejecting incomplete/ambiguous tags.
[[nodiscard]] variant_contract bind_variant_contract(const member& source,
    const std::optional<xml::name>& wrapper, variant_selection selection,
    const std::vector<variant_tag_override>& overrides = {});

// Select one wrapper payload by expanded names; malformed, unknown or conflicting metadata rejects.
[[nodiscard]] std::size_t select_variant_alternative(const variant_contract& contract,
    const xml::element& wrapper, xml::operation_context* context = nullptr);

} // namespace rohit::serializer::writer::xml_binding
