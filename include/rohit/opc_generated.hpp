// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/package.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <rohit/document_profile.hpp>
#include <rohit/serializer.hpp>

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
  std::size_t source_index{};
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

// Borrow logical native-codec diagnostics while its nested reader charges the shared operation ledger.
struct native_part_context {
  budget& shared;
  std::string_view part;
  std::string_view relationship_id;
  std::string_view field_path{};
};

// Preserve package policy failures and admit actual native allocations/work before committing them.
inline void native_part_consume(void* state, decode_operation_resource selected, std::size_t count) {
  const auto& context = *static_cast<native_part_context*>(state);
  context.shared.consume_at(selected == decode_operation_resource::work_units ? resource::work_units :
      resource::decoder_allocation_bytes, count, {context.part, context.field_path, {}, context.relationship_id});
}

// Decode one exact native document with cumulative allocation/work and existing per-part structural bounds.
template <typename Model, template <serialize_type> class Protocol>
[[nodiscard]] Model read_native_part(std::span<const std::uint8_t> bytes, budget& shared,
    std::string_view part, std::string_view relationship_id = {}, std::string_view field_path = {}) {
  native_part_context context{shared, part, relationship_id, field_path};
  const decode_operation operation{&context, native_part_consume};
  decode_limits bounds{};
  bounds.max_input_bytes = bytes.size();
  bounds.max_string_bytes = bytes.size();
  bounds.operation = &operation;
  const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  try {
    return deserialize_exact<Model, Protocol>(input, bounds);
  } catch (const exception::resource_limit&) {
    throw error{error_code::resource_limit, "Native package codec resource limit", std::string{part},
        0, 0, 0, std::string{field_path}, {}, std::string{relationship_id}};
  } catch (const rohit::exception::base_parser&) {
    throw error{error_code::codec, "Native package codec rejected payload", std::string{part},
        0, 0, 0, std::string{field_path}, {}, std::string{relationship_id}};
  }
}

// Admit every logical reservation before growing native output, using one borrowed package operation.
class native_part_output : public rohit::full_stream_auto_alloc_limits {
  native_part_context context_;
public:
  // Start empty; the supplied policy and diagnostic strings outlive this synchronous traversal.
  native_part_output(const rohit::stream_limits& bounds, native_part_context context)
      : rohit::full_stream_auto_alloc_limits{&bounds}, context_{context} {}
  // Charge zero-byte records and collection visits independently from their encoded byte count.
  void native_output_visit(std::size_t count) {
    native_part_consume(&context_, decode_operation_resource::work_units, count);
  }
  // Preserve first-failure behavior by completing admission before any buffer allocation or write.
  void reserve(std::size_t bytes) override {
    const auto maximum = context_.shared.configured().max_part_bytes;
    if (current_offset() > maximum || bytes > maximum - current_offset()) {
      throw error{error_code::resource_limit, "Native package output exceeds part limit", std::string{context_.part},
          current_offset(), bytes, maximum, std::string{context_.field_path}, {}, std::string{context_.relationship_id}};
    }
    native_output_visit(bytes);
    native_part_consume(&context_, decode_operation_resource::allocation_bytes, bytes);
    rohit::full_stream_auto_alloc_limits::reserve(bytes);
  }
};

// Encode a borrowed native model with pre-admitted output and actual recursive traversal work.
template <template <serialize_type> class Protocol, typename Model>
void write_native_part(opc_writer& writer, std::string_view name, std::string_view content_type,
    const Model& value, entry_options storage, std::string_view relationship_id, std::string_view field_path = {}) {
  auto& shared = *writer.shared_budget();
  const auto maximum = static_cast<std::size_t>((std::min)(shared.configured().max_part_bytes,
      static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
  const rohit::stream_limits bounds{0U, maximum};
  native_part_output bytes{bounds, {shared, name, relationship_id, field_path}};
  shared.check_cancelled();
  serialize_to<Protocol>(bytes, value);
  writer.add_part(name, content_type,
      {reinterpret_cast<const std::uint8_t*>(bytes.begin()), bytes.current_offset()}, storage);
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
  context.understood_namespaces = &expected.vocabularies;
  return xml::deserialize<Model>(
      {reinterpret_cast<const char*>(bytes.data()), bytes.size()}, bounds, nullptr, &context);
}

// Preserve the original context-free spelling for existing callback implementations.
template <typename Model>
[[nodiscard]] Model read_xml_part(std::span<const std::uint8_t> bytes, budget& shared) {
  return read_xml_part<Model>(bytes, shared, {});
}

// Select only an envelope for the statically generated model; runtime metadata cannot replace its type.
template <typename Model, bool SupportsJson = true>
[[nodiscard]] Model read_profile_part(std::span<const std::uint8_t> bytes, budget& shared,
    std::string_view part, const document::part_profile& profile, std::string_view relationship_id = {}) {
  switch (profile.envelope.format) {
  case document::payload_format::xml:
    if (!profile.decoder) { throw error{error_code::codec, "Typed package profile lacks XML dispatch", std::string{part}}; }
    return read_xml_part<Model>(bytes, shared, part, *profile.decoder);
  case document::payload_format::json:
    if constexpr (SupportsJson) {
      return read_native_part<Model, json>(bytes, shared, part, relationship_id, profile.role);
    } else {
      throw error{error_code::codec, "JSON is excluded by the generated native protocol selection", std::string{part}};
    }
  case document::payload_format::binary_none:
    return read_native_part<Model, binary_none>(bytes, shared, part, relationship_id, profile.role);
  case document::payload_format::opaque_bytes:
    throw error{error_code::codec, "Typed model cannot use an opaque package profile", std::string{part}};
  }
  throw error{error_code::invalid_state, "Unknown package profile format", std::string{part}};
}

// Compose the selected XML/native envelope under the same package storage and operation budget.
template <bool SupportsJson = true, typename Model>
void write_profile_part(opc_writer& writer, std::string_view name, std::string_view content_type,
    const Model& value, const document::part_profile& profile, std::string_view relationship_id) {
  const entry_options storage{.method = profile.envelope.compression == compression::format::none ?
      zip_method::stored : zip_method::deflate, .compression_level = profile.envelope.compression_level};
  switch (profile.envelope.format) {
  case document::payload_format::xml:
    if (profile.decoder && profile.decoder->requires_type) {
      auto context = writer.shared_budget()->xml_context(name, relationship_id);
      context.allow_mixed_content = true;
      xml::limits bounds{};
      bounds.max_input_bytes = bounds.max_output_bytes = static_cast<std::size_t>((std::min)(
          writer.shared_budget()->configured().max_part_bytes,
          static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
      const auto plain = xml::serialize(value, bounds, nullptr, &context);
      auto document = xml::parse(plain, bounds, &context);
      if (!profile.decoder->type) { throw error{error_code::codec, "Required package XML type is unavailable", std::string{name}}; }
      document.attributes.push_back({{"type", "http://www.w3.org/2001/XMLSchema-instance"}, xml::qualify_qname(*profile.decoder->type, document)});
      const auto encoded = xml::write(document, bounds, &context);
      writer.add_part(name, content_type, {reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size()}, storage);
    } else {
      write_xml_part(writer, name, content_type, value, storage, relationship_id);
    }
    return;
  case document::payload_format::json:
    if constexpr (SupportsJson) {
      write_native_part<json>(writer, name, content_type, value, storage, relationship_id, profile.role);
    } else {
      throw error{error_code::codec, "JSON is excluded by the generated native protocol selection", std::string{name}};
    }
    return;
  case document::payload_format::binary_none:
    write_native_part<binary_none>(writer, name, content_type, value, storage, relationship_id, profile.role);
    return;
  case document::payload_format::opaque_bytes:
    throw error{error_code::codec, "Typed model cannot use an opaque package profile", std::string{name}};
  }
  throw error{error_code::invalid_state, "Unknown package profile format", std::string{name}};
}

} // namespace rohit::serializer::package
