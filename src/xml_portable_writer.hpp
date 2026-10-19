// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xml_schema.hpp"
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_portable {
using field_name = std::function<std::string(const member&)>;
using type_name_fn = std::function<std::string(const type_name&)>;
using scalar_codec = std::function<std::string(const type_name&, const std::string&, bool)>;
using alternative_name = std::function<std::string(std::string_view)>;
// Recognize one complete singleton all-group that portable pull readers can dispatch directly.
[[nodiscard]] bool supports_all(const xml_binding::record& contract);
// Recognize one nonrepeated singleton choice represented by ordinary single-layer optional fields.
[[nodiscard]] bool supports_choices(const xml_binding::record& contract);
// Count native optional alternatives before XML output or candidate publication.
[[nodiscard]] std::string choice_check(const xml_binding::record& contract,
    const field_name& access, std::string_view language);
// Admit complete map wrappers with dedicated key/value contracts and no scalar-only overlays.
[[nodiscard]] bool supports_map(const xml_binding::field& field);
// Admit a dedicated element- or xsi:type-selected variant wrapper without scalar-only overlays.
[[nodiscard]] bool supports_variant(const xml_binding::field& field);
// Admit explicit presence and optional wrapped collections represented by ordinary native optionals.
[[nodiscard]] bool supports_presence(const xml_binding::field& field);
// Select element fields that need state traversal beyond the legacy single optional scalar adapter.
[[nodiscard]] bool needs_presence(const xml_binding::field& field);
// Select fields whose native lexical adapter needs an additional compiled scalar rule.
[[nodiscard]] bool needs_scalar_rule(const xml_binding::field& field);
// Return the per-record static scalar helper name for a selected Java or C# field.
[[nodiscard]] std::string scalar_rule_name(std::size_t index, std::string_view language);
// Prepare immutable scalar facets and portable pattern instructions once per generated field.
[[nodiscard]] std::string scalar_rule(const xml_binding::field& field, std::size_t index,
                                      std::string_view language);
// Apply the prepared rule to one lexical value, preserving the existing expression when unrestricted.
[[nodiscard]] std::string scalar_check(const xml_binding::field& field, std::size_t index,
    const std::string& expression, std::string_view language);
// Apply XSD defaults only to missing attributes or empty element/simple-content values.
[[nodiscard]] std::string scalar_input(const xml_binding::field& field, std::size_t index,
    const std::string& expression, std::string_view language);
// Preserve a normalized empty element value when a nonempty XSD default would otherwise replace it.
[[nodiscard]] std::string scalar_output(const xml_binding::field& field, std::size_t index,
    const std::string& expression, std::string_view language);
// Select native sequences/fixed arrays carried by one lexical XML list value.
[[nodiscard]] bool typed_list(const xml_binding::field& field);
// Emit one complete lexical list using existing native scalar adapters and field-level facets.
[[nodiscard]] std::string list_write(const xml_binding::field& field, std::size_t index,
    const std::string& access, const scalar_codec& scalar, std::string_view language);
// Decode a complete lexical list privately before assigning its native container.
[[nodiscard]] std::string list_read(const xml_binding::field& field, std::size_t index,
    const std::string& access, const std::string& expression, const type_name_fn& type,
    const scalar_codec& scalar, std::string_view language, const std::string& array_initializer = {});
// Supply exact scalar comparisons and the bounded portable-pattern machine for Java or C#.
[[nodiscard]] std::string scalar_runtime(std::string_view language);
// Supply independent immutable per-wrapper map bounds without modifying native decoder limits.
[[nodiscard]] std::string map_runtime(std::string_view language);
// Supply immutable mixed-sequence policies and operation-local fragment/text admission.
[[nodiscard]] std::string mixed_runtime(std::string_view language);

// Emit immutable owner-local identity metadata and the bounded direct token observer runtime.
[[nodiscard]] std::string identity_metadata(const xml::identity_scope& scope, std::string_view language);
[[nodiscard]] std::string identity_runtime(std::string_view language);

// Emit approved ordinary HTML helpers separately from XML namespaces, codecs and provider behavior.
[[nodiscard]] std::string html_runtime(std::string_view language);
// Supply canonical revision and owning digest adapters with independent immutable byte admission.
[[nodiscard]] std::string model_runtime(std::string_view language);
// Emit independent opaque-vocabulary admission and direct token preservation helpers.
[[nodiscard]] std::string extension_runtime(std::string_view language);
// Emit native mixed text/record alternatives in their authored order with private sequence budgets.
[[nodiscard]] std::string mixed_write(const xml_binding::mixed_contract& mixed,
    const std::string& access, const field_name& field, const alternative_name& alternative,
    std::string_view language);
// Decode logical text runs and known children into native fragments without retaining XML subtrees.
[[nodiscard]] std::string mixed_read(const xml_binding::mixed_contract& mixed,
    const std::string& access, const type_name_fn& type, const field_name& field,
    const alternative_name& alternative, std::string_view language,
    const std::string& array_initializer = {});
// Emit direct map tokens using backend-native typed key ordering and lexical adapters.
[[nodiscard]] std::string map_write(const xml_binding::map_contract& map, const std::string& access,
    const type_name_fn& type, const scalar_codec& scalar, std::string_view language,
    const std::string& key_comparator = {});
// Decode a complete private map with arbitrary key/value order and typed-key duplicate checks.
[[nodiscard]] std::string map_read(const xml_binding::map_contract& map, const std::string& access,
    const type_name_fn& type, const scalar_codec& scalar, std::string_view language,
    const std::string& initializer = {});
// Emit exactly one statically selected variant payload, rejecting invalid native indexes.
[[nodiscard]] std::string variant_write(const xml_binding::variant_contract& variant,
    const std::string& access, const alternative_name& alternative, const scalar_codec& scalar,
    std::string_view language);
// Decode one known payload into native index/alternative storage before closing its wrapper.
[[nodiscard]] std::string variant_read(const xml_binding::variant_contract& variant,
    const std::string& access, const alternative_name& alternative, const type_name_fn& type,
    const scalar_codec& scalar, std::string_view language);
// Emit native optional layers as absent, nil or present payloads without changing model storage.
[[nodiscard]] std::string presence_write(const xml_binding::field& field, std::size_t rule_index,
    const std::string& access, const scalar_codec& scalar, std::string_view language);
// Rebuild optional layers after complete scalar, record or wrapped collection validation.
[[nodiscard]] std::string presence_read(const xml_binding::field& field, std::size_t rule_index,
    const std::string& access, const type_name_fn& type, const scalar_codec& scalar,
    std::string_view language, const std::string& array_initializer = {});
// Return package-level Go imports before any declarations.
std::string imports(std::string_view language);
// Return the selected backend's bounded, namespace-aware data XML runtime.
std::string runtime(std::string_view language);
// Export live native references without creating an owning target or synthesizing input ownership.
std::string reference_methods(const class_node& object, const std::string& identifier,
    const type_name_fn& type, std::string_view language);
// Emit statically bound owning-record methods at the backend's class/package scope.
std::string methods(const class_node& object, const xml_binding::record& contract,
    const std::string& identifier, const field_name& field, const type_name_fn& type,
    const xml_options& options, std::string_view language, const alternative_name& alternative,
    bool readonly = false, const field_name& projection = {});
} // namespace rohit::serializer::writer::xml_portable
