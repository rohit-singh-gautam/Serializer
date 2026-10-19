// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_binding {

inline constexpr std::string_view model_metadata_namespace = "urn:serializer:xml:model";

enum class projection_kind { owning, managed_snapshot, borrowed_output };

// These pointers borrow an immutable parsed AST, which must outlive every bound contract.
struct model_contract {
  std::string concrete_identity{};
  bool concrete_generic{};
  bool inherited{};
  bool digest{};
  projection_kind projection{projection_kind::owning};
  std::vector<const member*> flattened_fields{};
  const member* revision{};
  std::optional<std::string> magic{};
  // The presence of model metadata requires explicit backend support rather than silent omission.
  bool advanced{};
};

// Bind concrete generic identity, depth-first base order and an explicit data-only projection.
// Repeated base occurrences, cycles, ambiguous field names and unresolved generics reject.
[[nodiscard]] model_contract bind_model_contract(const class_node& source);
// Reject two distinct concrete model identities sharing one expanded root in a decoder inventory.
void validate_model_names(std::span<const model_contract> models, std::span<const xml::name> roots);
// Select an accepted historical/current layout; revision is XML text, with unquoted dotted forms.
// Magic is exact lower-case hexadecimal for raw bytes, or the typed declaration's checked literal.
[[nodiscard]] std::vector<const member*>
select_model_layout(const model_contract& contract, std::string_view revision,
                    std::optional<std::string_view> magic = {});
// Read only reserved expanded-name metadata from a normalized XML root, rejecting duplicates/unknowns.
[[nodiscard]] std::vector<const member*> select_model_layout(const model_contract& contract,
                                                             const xml::element& root);
// Produce the selected revision and immutable magic attributes after checking the accepted layout.
// Empty revision selects the current declared revision; unversioned models emit no revision attribute.
[[nodiscard]] std::vector<xml::attribute> model_metadata_attributes(const model_contract& contract,
                                                                    std::string_view revision = {});
// Enforce output-only borrowed/view lifetime semantics before a decoder is selected.
void validate_model_direction(const model_contract& contract, bool decoding);
// Decode the portable hexadecimal digest value, checking the declared extent before allocation.
[[nodiscard]] std::vector<std::uint8_t>
decode_model_digest(const type_name& type, std::string_view text,
                    std::size_t max_digest_bytes = 1024U * 1024U,
                    xml::operation_context* context = nullptr);

} // namespace rohit::serializer::writer::xml_binding
