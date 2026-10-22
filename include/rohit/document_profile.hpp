// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/compression.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::document {

enum class payload_format { xml, json, binary_none, opaque_bytes };
enum class container_format { none, zip, opc };
enum class profile_policy { fixed, defaults };
enum class relationship_target_mode { internal, external, both };

// Portable complete-object envelope options; this metadata neither opens providers nor owns models.
struct profile {
  payload_format format{payload_format::xml};
  compression::format compression{compression::format::none};
  container_format container{container_format::none};
  profile_policy policy{profile_policy::defaults};
  int compression_level{6};
};

// Caller overrides select an envelope only; model/root/vocabulary identities cannot be replaced.
struct profile_override {
  std::optional<payload_format> format{};
  std::optional<compression::format> compression{};
  std::optional<container_format> container{};
  std::optional<int> compression_level{};
};

// Strict case-sensitive profile configuration accepts format/compression/container/policy/level only.
[[nodiscard]] profile parse_profile(const std::map<std::string, std::string>& values);
// Validate defaults or apply allowed runtime overrides transactionally, without fetching providers.
[[nodiscard]] profile resolve_profile(const profile& defaults,
                                      const profile_override& runtime = {});

// Expanded names and schema identities are owning metadata; prefixes never choose a decoder.
struct dispatch_metadata {
  std::string model_identity{};
  xml::name root{};
  std::optional<xml::name> type{};
  bool requires_type{};
  std::vector<std::string> vocabularies{};
};

// One generated package role carries its complete expected-type and envelope contract.
struct part_profile {
  std::string role{};
  std::string relationship{};
  profile envelope{};
  std::optional<dispatch_metadata> decoder{};
  std::optional<std::string> part_name{};
  std::optional<std::string> content_type{};
  std::string source{"/"};
  std::size_t min_occurs{1U};
  std::optional<std::size_t> max_occurs{1U};
  relationship_target_mode target_mode{relationship_target_mode::internal};
};

// Reject duplicate model/root/type dispatch keys, invalid roots and duplicate vocabulary declarations.
void validate_dispatch_inventory(std::span<const dispatch_metadata> inventory);
// Match the caller's exact expected identity before payload decode; wrong/missing metadata returns null.
// The result borrows inventory storage, which must remain stable through synchronous decode.
[[nodiscard]] const dispatch_metadata*
select_decoder(std::span<const dispatch_metadata> inventory, std::string_view expected_identity,
               const xml::name& root, const std::optional<xml::name>& type = {},
               std::span<const std::string> understood_vocabularies = {});
// Resolve an original document's xsi:type in its in-scope bindings before matching a decoder.
[[nodiscard]] const dispatch_metadata*
select_decoder(std::span<const dispatch_metadata> inventory, std::string_view expected_identity,
               const xml::element& root, std::span<const std::string> understood_vocabularies = {});

} // namespace rohit::serializer::document
