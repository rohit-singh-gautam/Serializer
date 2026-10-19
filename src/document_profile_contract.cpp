// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_profile_contract.hpp"
#include "xml_model_contract.hpp"
#include "xml_schema.hpp"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::package_codegen {
namespace {
// Quote metadata as exact C++ string bytes, including controls and UTF-8 without source injection.
std::string cpp_string(std::string_view value) {
  std::string result{"\""};
  for (const auto character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte == '"' || byte == '\\') {
      result += '\\';
      result += character;
    } else if (byte < 0x20U || byte >= 0x7fU) {
      result += '\\';
      result += static_cast<char>('0' + ((byte >> 6U) & 7U));
      result += static_cast<char>('0' + ((byte >> 3U) & 7U));
      result += static_cast<char>('0' + (byte & 7U));
    } else {
      result += character;
    }
  }
  return result + '"';
}

// Render expanded names as owned aggregate values, independent of generated namespace prefixes.
std::string cpp_name(const xml::name& value) {
  return "{" + cpp_string(value.local) + ", " + cpp_string(value.namespace_uri) + "}";
}
} // namespace

package_profile_contract compile_package_profile(const package_node& source,
                                                 const xml_options& options) {
  package_profile_contract result{};
  result.identity = source.get_full_name();
  std::set<std::string> roles{};
  for (const auto& role : source.parts) {
    if (!roles.insert(role.name).second) {
      throw std::invalid_argument{"Duplicate generated package role"};
    }
    document::part_profile part{};
    part.role = role.name;
    part.relationship = role.relationship;
    part.part_name = role.part_name;
    part.content_type = role.content_type;
    part.source = role.source;
    part.min_occurs = role.min_occurs;
    part.max_occurs = role.max_occurs;
    std::map<std::string, std::string> configuration{
        {"format", role.codec},
        {"compression", role.compression == "stored" ? "none" : role.compression}};
    part.envelope = document::parse_profile(configuration);
    if (role.value_type) {
      const auto& value = *role.value_type;
      if (!value.resolved_node || value.resolved_node->type != object_type::class_type) {
        throw std::invalid_argument{"Typed package dispatch requires a concrete record type"};
      }
      const auto& owner = static_cast<const class_node&>(*value.resolved_node);
      document::dispatch_metadata metadata{};
      metadata.model_identity = owner.get_full_name();
      if (part.envelope.format == document::payload_format::xml) {
        metadata.root = xml_binding::schema{options}.bind(owner).root;
        // Vocabulary identity derives from the bound namespace, never a generated prefix spelling.
        if (!metadata.root.namespace_uri.empty()) {
          metadata.vocabularies.push_back(metadata.root.namespace_uri);
        }
      } else {
        metadata.root = {owner.name, "urn:serializer:native"};
      }
      metadata.type =
          xml::name{owner.name, "urn:serializer:native:" +
                                    (owner.parent_namespace
                                         ? get_full_name_for_namespace(owner.parent_namespace)
                                         : std::string{})};
      part.decoder = std::move(metadata);
    }
    result.parts.push_back(std::move(part));
  }
  return result;
}

std::string cpp_profile_metadata(const package_node& source, const xml_options& options,
                                 std::string_view generated_owner_name) {
  const auto contract = compile_package_profile(source, options);
  if (generated_owner_name.empty()) {
    generated_owner_name = source.name;
  }
  // A same-named method would be parsed as a constructor; retain all valid existing package names.
  const auto factory_name = generated_owner_name == "document_profiles"
                                ? "document_profiles_metadata"
                                : "document_profiles";
  std::string result =
      "  // Inspect expected payload types and envelope defaults before selecting a decoder.\n"
      "  static ::std::vector<::rohit::serializer::document::part_profile> " +
      std::string{factory_name} +
      "() {\n"
      "    return {\n";
  for (const auto& part : contract.parts) {
    const auto format = part.envelope.format == document::payload_format::xml    ? "xml"
                        : part.envelope.format == document::payload_format::json ? "json"
                        : part.envelope.format == document::payload_format::opaque_bytes
                            ? "opaque_bytes"
                            : "binary_none";
    const auto compression = part.envelope.compression == compression::format::none   ? "none"
                             : part.envelope.compression == compression::format::gzip ? "gzip"
                             : part.envelope.compression == compression::format::zlib ? "zlib"
                             : part.envelope.compression == compression::format::zstd ? "zstd"
                             : part.envelope.compression == compression::format::lz4  ? "lz4"
                                                                                      : "deflate";
    result += "      {" + cpp_string(part.role) + ", " + cpp_string(part.relationship) +
              ", {::rohit::serializer::document::payload_format::" + format +
              ", ::rohit::serializer::compression::format::" + compression +
              ", ::rohit::serializer::document::container_format::none, "
              "::rohit::serializer::document::profile_policy::defaults, " +
              std::to_string(part.envelope.compression_level) + "}, ";
    if (!part.decoder) {
      result += "::std::nullopt";
    } else {
      const auto& decoder = *part.decoder;
      result += "::rohit::serializer::document::dispatch_metadata{" +
                cpp_string(decoder.model_identity) + ", " + cpp_name(decoder.root) +
                ", ::rohit::serializer::xml::name" + cpp_name(*decoder.type) + ", false, {";
      for (const auto& vocabulary : decoder.vocabularies) {
        result += cpp_string(vocabulary) + ", ";
      }
      result += "}}";
    }
    const auto optional_string = [](const std::optional<std::string>& value) {
      return value ? "::std::optional<::std::string>{" + cpp_string(*value) + "}"
                   : "::std::nullopt";
    };
    result += ", " + optional_string(part.part_name) + ", " + optional_string(part.content_type) +
              ", " + cpp_string(part.source) + ", " + std::to_string(part.min_occurs) + "U, " +
              (part.max_occurs
                   ? "::std::optional<::std::size_t>{" + std::to_string(*part.max_occurs) + "U}"
                   : "::std::nullopt") +
              "},\n";
  }
  return result + "    };\n  }\n";
}
} // namespace rohit::serializer::writer::package_codegen
