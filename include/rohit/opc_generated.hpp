// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/package.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <rohit/document_profile.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::package {

// Borrow a typed resource for one synchronous generated package Save.
// The caller keeps the value and string storage alive and unchanged until Save returns.
template <typename Model>
struct model_part_view {
  const Model* value{};
  std::string_view name{};
  std::string_view content_type{};
  std::size_t source_index{};
};

// Borrow one binary resource without duplicating its uncompressed payload.
struct binary_part_view {
  std::span<const std::uint8_t> bytes{};
  std::string_view name{};
  std::string_view content_type{};
  std::size_t source_index{};
};

// Retain the actual discovered name and media type with a decoded dynamic model.
template <typename Model>
struct model_part {
  std::string name{};
  std::string content_type{};
  Model value{};
};

// Fail before a generated profile can write or publish an invalid occurrence count.
inline void require_occurrences(std::size_t count, std::size_t minimum,
    std::size_t maximum, std::string_view role) {
  if (count < minimum || count > maximum) {
    throw error{error_code::semantic, "Package role occurrence constraint failed",
        {}, 0, count, maximum, std::string{role}};
  }
}

// Produce stable source-local relationship IDs from a declared role and occurrence.
inline std::string generated_relationship_id(std::string_view role, std::size_t index) {
  return "srl_" + std::string{role} + "_" + std::to_string(index);
}

// Select a declared parent instance without interpreting it as a filesystem path.
inline const std::string& require_source(const std::vector<std::string>& names,
    std::size_t index, std::string_view role) {
  if (index >= names.size()) {
    throw error{error_code::invalid_relationship, "Package source occurrence is missing",
        {}, 0, index, names.size(), std::string{role}};
  }
  return names[index];
}

// Charge retained discovery names before copying them into a generated profile index.
inline void remember_source(std::vector<std::string>& names, std::string_view name,
    budget& shared) {
  shared.consume(resource::decoder_allocation_bytes, sizeof(std::string), name);
  shared.consume(resource::decoder_allocation_bytes, name.size(), name);
  shared.consume(resource::work_units, 1, name);
  names.emplace_back(name);
}

// Encode a borrowed model with the same cumulative package and XML operation context.
template <typename Model>
void write_xml_part(opc_writer& writer, std::string_view name,
    std::string_view content_type, const Model& value, entry_options storage,
    std::string_view relationship_id) {
  auto context = writer.shared_budget()->xml_context(name, relationship_id);
  xml::limits bounds{};
  const auto& configured = writer.shared_budget()->configured();
  bounds.max_output_bytes = static_cast<std::size_t>(
      (std::min)(configured.max_part_bytes,
          static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
  writer.write_part(name, content_type, [&](std::ostream& destination) {
    const auto bytes = xml::serialize(value, bounds, nullptr, &context);
    destination.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }, storage);
}

// Preserve the original caller-authored helper while generated facades supply their known link.
template <typename Model>
void write_xml_part(opc_writer& writer, std::string_view name,
    std::string_view content_type, const Model& value, entry_options storage = {}) {
  write_xml_part(writer, name, content_type, value, storage, {});
}

// Decode a fresh typed resource while charging generated/XML work to its package budget.
template <typename Model>
[[nodiscard]] Model read_xml_part(std::span<const std::uint8_t> bytes, budget& shared,
    std::string_view part) {
  auto context = shared.xml_context(part);
  xml::limits bounds{};
  bounds.max_input_bytes = bytes.size();
  return xml::deserialize<Model>(
      {reinterpret_cast<const char*>(bytes.data()), bytes.size()}, bounds, nullptr, &context);
}

// Preserve the original context-free spelling for existing callback implementations.
// Inspect one bounded root event before allowing a generated typed codec to construct its model.
template <typename Model>
[[nodiscard]] Model read_xml_part(std::span<const std::uint8_t> bytes, budget& shared,
    std::string_view part, const document::dispatch_metadata& expected) {
  auto context = shared.xml_context(part);
  xml::limits bounds{};
  bounds.max_input_bytes = bytes.size();
  {
    xml::token_reader reader{{reinterpret_cast<const char*>(bytes.data()), bytes.size()}, bounds, &context};
    while (reader.current().kind != xml::event_kind::start_element) {
      if (reader.current().kind == xml::event_kind::end_document) {
        throw error{error_code::codec, "Package XML root is missing", std::string{part}};
      }
      reader.advance();
    }
    const auto type = reader.take_type();
    if (!document::select_decoder(std::span{&expected, 1U}, expected.model_identity,
        reader.current().qualified_name, type, expected.vocabularies)) {
      throw error{error_code::codec, "Package XML root or type does not match its declared role", std::string{part}};
    }
  }
  context.permitted_root_type = expected.type ? &*expected.type : nullptr;
  return xml::deserialize<Model>(
      {reinterpret_cast<const char*>(bytes.data()), bytes.size()}, bounds, nullptr, &context);
}

// Preserve the original context-free spelling for existing callback implementations.
template <typename Model>
[[nodiscard]] Model read_xml_part(std::span<const std::uint8_t> bytes, budget& shared) {
  return read_xml_part<Model>(bytes, shared, {});
}

} // namespace rohit::serializer::package
