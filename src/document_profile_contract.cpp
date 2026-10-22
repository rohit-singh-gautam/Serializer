// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_profile_contract.hpp"
#include "xml_model_contract.hpp"
#include "xml_schema.hpp"

#include <algorithm>
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

// Resolve package role settings separately from the schema's native payload declarations.
package_node configure_package(const package_node& source, const xml_options& options) {
  package_node result{std::string{source.name}, source.parent_namespace};
  result.parts = source.parts;
  const auto selected = options.package_profiles.find(source.get_full_name());
  if (selected == options.package_profiles.end()) { return result; }
  for (const auto& [key, value] : selected->second) {
    const auto separator = key.find('.');
    const auto role_name = key.substr(0, separator);
    const auto role = std::find_if(result.parts.begin(), result.parts.end(),
        [&](const auto& item) { return item.name == role_name; });
    if (separator == std::string::npos || role == result.parts.end()) {
      throw std::invalid_argument{"Unknown package profile role: " + source.get_full_name() + "." + role_name};
    }
    const auto setting = key.substr(separator + 1U);
    if (setting == "codec") {
      if ((role->value_type && value != "xml" && value != "json" && value != "binary_none") ||
          (!role->value_type && value != "bytes")) {
        throw std::invalid_argument{"Package profile codec conflicts with its declared model: " + key};
      }
      role->codec = value;
    } else if (setting == "compression") {
      if (value != "stored" && value != "deflate") { throw std::invalid_argument{"Invalid package compression: " + key}; }
      role->compression = value;
    } else if (setting == "policy") {
      if (value != "fixed" && value != "defaults") { throw std::invalid_argument{"Invalid package policy: " + key}; }
      role->profile_policy = value;
    } else if (setting == "requires_type") {
      if (value != "true" && value != "false") { throw std::invalid_argument{"Expected package requires_type boolean: " + key}; }
      role->requires_type = value == "true";
    } else if (setting == "target_mode") {
      if (value != "internal" && value != "external" && value != "both") { throw std::invalid_argument{"Invalid package target mode: " + key}; }
      if (value != "internal" && role->value_type) { throw std::invalid_argument{"External package targets must be inert bytes-role references: " + key}; }
      role->target_mode = value;
    } else if (setting == "vocabularies") {
      role->vocabularies.clear();
      std::size_t offset{};
      do {
        const auto end = value.find(',', offset);
        auto vocabulary = value.substr(offset, end == std::string::npos ? end : end - offset);
        const auto first = vocabulary.find_first_not_of(" \t\r\n");
        const auto last = vocabulary.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) { throw std::invalid_argument{"Empty package vocabulary: " + key}; }
        vocabulary = vocabulary.substr(first, last - first + 1U);
        if (std::any_of(vocabulary.begin(), vocabulary.end(), [](unsigned char byte) { return byte <= 0x20U || byte == 0x7fU; }) ||
            std::find(role->vocabularies.begin(), role->vocabularies.end(), vocabulary) != role->vocabularies.end()) {
          throw std::invalid_argument{"Invalid or repeated package vocabulary: " + key};
        }
        role->vocabularies.push_back(std::move(vocabulary));
        if (end == std::string::npos) { break; }
        offset = end + 1U;
      } while (true);
    } else { throw std::invalid_argument{"Unknown package profile setting: " + key}; }
  }
  for (const auto& part : result.parts) {
    if (part.codec != "xml" && (part.requires_type || !part.vocabularies.empty())) {
      throw std::invalid_argument{"Namespace/type package profiles require the XML codec: " + part.name};
    }
    if (part.target_mode != "internal" && (part.part_name || part.content_type)) {
      throw std::invalid_argument{"External/both package roles require dynamic names and media: " + part.name};
    }
    if (part.source != "/") {
      const auto parent = std::find_if(result.parts.begin(), result.parts.end(), [&](const auto& item) { return item.name == part.source; });
      if (parent != result.parts.end() && parent->target_mode != "internal") {
        throw std::invalid_argument{"An external/both package role cannot be a relationship source: " + part.source};
      }
    }
  }
  return result;
}

// Validate the complete selector inventory so a misspelling never silently falls back to defaults.
void validate_package_options(const std::vector<std::unique_ptr<syntax_node>>& statements,
                              const xml_options& options) {
  std::set<std::string> selected;
  const auto visit = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == object_type::namespace_type) {
        self(self, static_cast<const namespace_node&>(*node).statements);
      } else if (node->type == object_type::package_type) {
        const auto& package = static_cast<const package_node&>(*node);
        selected.insert(package.get_full_name());
        static_cast<void>(configure_package(package, options));
      }
    }
  };
  visit(visit, statements);
  for (const auto& [name, values] : options.package_profiles) {
    static_cast<void>(values);
    if (!selected.contains(name)) { throw std::invalid_argument{"Unknown package profile selector: " + name}; }
  }
}

package_profile_contract compile_package_profile(const package_node& source,
                                                 const xml_options& options) {
  package_profile_contract result{};
  result.identity = source.get_full_name();
  std::set<std::string> roles{};
  const xml_binding::schema bindings{options};
  const auto configured = configure_package(source, options);
  for (const auto& role : configured.parts) {
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
    part.target_mode = role.target_mode == "external" ? document::relationship_target_mode::external :
        role.target_mode == "both" ? document::relationship_target_mode::both : document::relationship_target_mode::internal;
    std::map<std::string, std::string> configuration{
        {"format", role.codec},
        {"compression", role.compression == "stored" ? "none" : role.compression}};
    part.envelope = document::parse_profile(configuration);
    part.envelope.policy = role.profile_policy == "fixed"
        ? document::profile_policy::fixed : document::profile_policy::defaults;
    if (role.value_type) {
      const auto& value = *role.value_type;
      if (!value.resolved_node || value.resolved_node->type != object_type::class_type) {
        throw std::invalid_argument{"Typed package dispatch requires a concrete record type"};
      }
      const auto& owner = static_cast<const class_node&>(*value.resolved_node);
      document::dispatch_metadata metadata{};
      metadata.model_identity = owner.get_full_name();
      metadata.root = bindings.bind(owner).root;
      // Native envelope selection never changes the owning model's declared XML identity.
      if (!metadata.root.namespace_uri.empty()) {
        metadata.vocabularies.push_back(metadata.root.namespace_uri);
      }
      std::set<const class_node*> visited;
      const auto remember = [&](const xml::name& name) {
        if (!name.namespace_uri.empty() && std::find(metadata.vocabularies.begin(), metadata.vocabularies.end(), name.namespace_uri) == metadata.vocabularies.end()) {
          metadata.vocabularies.push_back(name.namespace_uri);
        }
      };
      // Compile only namespaces understood by generated mappings; opaque wildcard namespaces stay explicit.
      const auto visit = [&](auto&& self, const class_node& current) -> void {
        if (!visited.insert(&current).second) { return; }
        const auto contract = bindings.bind(current);
        remember(contract.root);
        const auto visit_type = [&](auto&& walk, const type_name& type) -> void {
          if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
            self(self, static_cast<const class_node&>(*type.resolved_node));
          }
          for (const auto& argument : type.arguments) { walk(walk, argument); }
        };
        for (const auto& field : contract.fields) {
          remember(field.qualified_name);
          if (field.wrapper) { remember(*field.wrapper); }
          if (field.map_binding) {
            const auto& map = *field.map_binding;
            for (const auto& name : {map.wrapper, map.entry, map.key, map.value}) { remember(name); }
            visit_type(visit_type, map.value_type);
          } else if (field.variant_binding) {
            remember(field.variant_binding->wrapper);
            for (const auto& alternative : field.variant_binding->alternatives) {
              remember(alternative.tag);
              remember(alternative.type_identity);
              visit_type(visit_type, alternative.native_type);
            }
          } else if (!contract.mixed || field.source != contract.mixed->source) {
            for (const auto& type : field.source->type_name_list) { visit_type(visit_type, type); }
          }
        }
        if (contract.mixed) {
          for (const auto& child : contract.mixed->children) {
            remember(child.qualified_name);
            if (child.value_type) { visit_type(visit_type, *child.value_type); }
          }
        }
      };
      visit(visit, owner);
      metadata.type =
          xml::name{owner.name, "urn:serializer:native:" +
                                    (owner.parent_namespace
                                         ? get_full_name_for_namespace(owner.parent_namespace)
                                         : std::string{})};
      remember(*metadata.type);
      metadata.requires_type = role.requires_type;
      for (const auto& vocabulary : role.vocabularies) {
        if (std::find(metadata.vocabularies.begin(), metadata.vocabularies.end(), vocabulary) == metadata.vocabularies.end()) {
          metadata.vocabularies.push_back(vocabulary);
        }
      }
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
              "::rohit::serializer::document::profile_policy::" +
              std::string{part.envelope.policy == document::profile_policy::fixed ? "fixed" : "defaults"} + ", " +
              std::to_string(part.envelope.compression_level) + "}, ";
    if (!part.decoder) {
      result += "::std::nullopt";
    } else {
      const auto& decoder = *part.decoder;
      result += "::rohit::serializer::document::dispatch_metadata{" +
                cpp_string(decoder.model_identity) + ", " + cpp_name(decoder.root) +
                ", ::rohit::serializer::xml::name" + cpp_name(*decoder.type) + ", " + std::string{decoder.requires_type ? "true" : "false"} + ", {";
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
                   : "::std::nullopt") + ", ::rohit::serializer::document::relationship_target_mode::" +
              std::string{part.target_mode == document::relationship_target_mode::external ? "external" :
                  part.target_mode == document::relationship_target_mode::both ? "both" : "internal"} + "},\n";
  }
  return result + "    };\n  }\n";
}
} // namespace rohit::serializer::writer::package_codegen
