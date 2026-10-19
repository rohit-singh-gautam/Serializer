// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_model_contract.hpp"
#include "schema_version.hpp"

#include <rohit/digest.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::size_t max_model_depth = 128U;
constexpr std::size_t max_model_fields = 100000U;

// Length framing prevents punctuation in nested generic identities from aliasing other instances.
std::string frame(std::string_view text) {
  return std::to_string(text.size()) + ":" + std::string{text};
}

// Render fully resolved type and dimension identities without relying on backend-generated names.
std::string type_identity(const type_name& type, std::size_t depth) {
  if (depth > max_model_depth) {
    throw std::invalid_argument{"XML generic identity exceeds nesting limit"};
  }
  if (type.kind == generic_argument_kind::dimension) {
    return "N" + std::to_string(type.dimension);
  }
  if (type.type == object_type::unresolved ||
      (type.resolved_node && type.resolved_node->type == object_type::generic_definition)) {
    throw std::invalid_argument{"XML requires fully resolved concrete generic arguments"};
  }
  std::string result = "T" + frame(type.get_full_name());
  if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
    const auto& owner = static_cast<const class_node&>(*type.resolved_node);
    const auto& generic =
        owner.named_generic_source.empty() ? owner.generic_name : owner.named_generic_source;
    const auto& arguments = owner.named_generic_source.empty() ? owner.generic_arguments
                                                               : owner.named_generic_arguments;
    if (!generic.empty()) {
      result = "G" + frame(generic);
      for (const auto& argument : arguments) {
        result += frame(type_identity(argument, depth + 1U));
      }
      return result;
    }
  }
  if (type.is_digest()) {
    result += "D" + std::to_string(static_cast<unsigned>(type.digest)) + ":" +
              std::to_string(type.digest_extent);
  }
  for (const auto& argument : type.arguments) {
    result += frame(type_identity(argument, depth + 1U));
  }
  return result;
}

// Preserve dotted revision text as XML text while reusing the native numeric version validator.
std::string revision_literal(const member& version, std::string_view text) {
  const auto& type = version.type_name_list.front().name;
  if (type.starts_with("version")) {
    return "\"" + std::string{text} + "\"";
  }
  return std::string{text};
}
} // namespace

model_contract bind_model_contract(const class_node& source) {
  const auto& generic =
      source.named_generic_source.empty() ? source.generic_name : source.named_generic_source;
  const auto& arguments = source.named_generic_source.empty() ? source.generic_arguments
                                                              : source.named_generic_arguments;
  if (source.type == object_type::generic_definition ||
      (generic.empty() &&
       (!source.generic_parameters.empty() || !source.type_parameters.empty()))) {
    throw std::invalid_argument{
        "XML requires a concrete generic instance, not a generic definition"};
  }
  model_contract result{};
  result.concrete_generic = !generic.empty();
  result.concrete_identity = frame(generic.empty() ? source.get_full_name() : generic);
  for (const auto& argument : arguments) {
    result.concrete_identity += frame(type_identity(argument, 0));
  }
  if (result.concrete_generic && arguments.empty()) {
    throw std::invalid_argument{"XML concrete generic instance has no resolved arguments"};
  }
  result.inherited = !source.parents.empty();
  if (source.reference || !source.has_mode(storage_mode::owning)) {
    if (source.reference && !source.reference->value_projection) {
      throw std::invalid_argument{"XML field-address references have no portable data projection"};
    }
    result.projection = projection_kind::borrowed_output;
  } else if (source.supports_managed()) {
    result.projection = projection_kind::managed_snapshot;
  }
  // Flatten each base exactly once in declaration order; a diamond has no implicit merge policy.
  std::set<const class_node*> seen{};
  std::set<std::string> names{};
  const auto append = [&](auto&& self, const class_node& owner, std::size_t depth) -> void {
    if (depth > max_model_depth || !seen.insert(&owner).second) {
      throw std::invalid_argument{"XML inheritance is cyclic, repeated or too deep"};
    }
    for (const auto& base : owner.parents) {
      if (!base.parent_class) {
        throw std::invalid_argument{"XML inheritance requires resolved base types"};
      }
      self(self, *base.parent_class, depth + 1U);
    }
    if (owner.supports_managed() && result.projection != projection_kind::borrowed_output) {
      result.projection = projection_kind::managed_snapshot;
    }
    if (owner.has_magic() && !owner.magic_omits("xml")) {
      if (result.magic) {
        throw std::invalid_argument{"Flattened XML supports only one immutable magic identity"};
      }
      if (owner.magic_field) {
        result.magic = owner.magic_field->default_value;
        if (result.magic->empty()) {
          throw std::invalid_argument{"XML typed magic requires a fixed value"};
        }
      } else {
        const auto bytes =
            std::span{reinterpret_cast<const std::uint8_t*>(owner.magic_bytes.data()),
                      owner.magic_bytes.size()};
        result.magic = rohit::digest_to_hex(bytes);
      }
    }
    for (const auto& field : owner.member_list) {
      if (field.omits("xml") || field.persistent_identity) {
        continue;
      }
      if (result.flattened_fields.size() >= max_model_fields || !names.insert(field.name).second) {
        throw std::invalid_argument{
            "Flattened XML fields have ambiguous names or exceed the field bound"};
      }
      if (std::any_of(field.type_name_list.begin(), field.type_name_list.end(),
                      [](const auto& type) { return type.contains_managed_reference(); })) {
        throw std::invalid_argument{
            "Managed identity tokens have no external XML data representation"};
      }
      const auto contains_digest = [&](auto&& self, const type_name& value) -> bool {
        return value.is_digest() ||
               std::any_of(value.arguments.begin(), value.arguments.end(),
                           [&](const auto& argument) { return self(self, argument); });
      };
      result.digest =
          result.digest ||
          std::any_of(field.type_name_list.begin(), field.type_name_list.end(),
                      [&](const auto& type) { return contains_digest(contains_digest, type); });
      if (field.version) {
        if (result.revision || field.type_name_list.size() != 1U) {
          throw std::invalid_argument{"Flattened XML requires one scalar revision discriminator"};
        }
        result.revision = &field;
      }
      result.flattened_fields.push_back(&field);
    }
  };
  append(append, source, 0);
  if (result.revision) {
    const auto& revision = *result.revision;
    const auto& type = revision.type_name_list.front().name;
    const auto lower = schema_version::parse(type, schema_version::minimum(revision));
    const auto upper = schema_version::parse(type, revision.default_value);
    if (upper < lower) {
      throw std::invalid_argument{"XML revision floor exceeds current revision"};
    }
    for (const auto* field : result.flattened_fields) {
      if (!field->created_version.empty()) {
        (void)schema_version::parse(type, field->created_version);
      }
      if (!field->obsolete_version.empty()) {
        (void)schema_version::parse(type, field->obsolete_version);
      }
      if (!field->created_version.empty() && !field->obsolete_version.empty() &&
          !(schema_version::parse(type, field->created_version) <
            schema_version::parse(type, field->obsolete_version))) {
        throw std::invalid_argument{"XML field lifecycle has an empty or reversed interval"};
      }
    }
  } else if (std::any_of(result.flattened_fields.begin(), result.flattened_fields.end(),
                         [](const auto* field) {
                           return !field->created_version.empty() ||
                                  !field->obsolete_version.empty();
                         })) {
    throw std::invalid_argument{"XML field lifecycles require a revision discriminator"};
  }
  result.advanced = result.concrete_generic || result.inherited || result.digest ||
                    result.revision || result.magic ||
                    result.projection == projection_kind::managed_snapshot ||
                    source.multiple_modes() || !source.has_mode(storage_mode::owning);
  return result;
}

void validate_model_names(std::span<const model_contract> models,
                          std::span<const xml::name> roots) {
  if (models.size() != roots.size()) {
    throw std::invalid_argument{"XML model/root inventories differ"};
  }
  std::set<std::pair<std::string, std::string>> names{};
  for (std::size_t index = 0; index < roots.size(); ++index) {
    if (models[index].concrete_identity.empty() || roots[index].local.empty() ||
        !names.emplace(roots[index].namespace_uri, roots[index].local).second) {
      throw std::invalid_argument{"Concrete XML decoder roots must be distinct expanded names"};
    }
  }
}

std::vector<const member*> select_model_layout(const model_contract& contract,
                                               std::string_view revision,
                                               std::optional<std::string_view> magic) {
  if (contract.magic.has_value() != magic.has_value() || (magic && *magic != *contract.magic)) {
    throw std::invalid_argument{"XML immutable model magic is missing or incorrect"};
  }
  std::vector<const member*> result{};
  if (!contract.revision) {
    if (!revision.empty()) {
      throw std::invalid_argument{"Unversioned XML model cannot carry a revision"};
    }
    return contract.flattened_fields;
  }
  const auto& discriminator = *contract.revision;
  const auto& type = discriminator.type_name_list.front().name;
  const auto literal = revision_literal(discriminator, revision);
  const auto value = schema_version::parse(type, literal);
  if (value < schema_version::parse(type, schema_version::minimum(discriminator)) ||
      schema_version::parse(type, discriminator.default_value) < value) {
    throw std::invalid_argument{"XML revision is outside the accepted historical/current interval"};
  }
  for (const auto* field : contract.flattened_fields) {
    if (!field->version && schema_version::active(*field, discriminator, literal)) {
      result.push_back(field);
    }
  }
  return result;
}

std::vector<const member*> select_model_layout(const model_contract& contract,
                                               const xml::element& root) {
  std::optional<std::string_view> revision{};
  std::optional<std::string_view> magic{};
  for (const auto& attribute : root.attributes) {
    if (attribute.qualified_name.namespace_uri != model_metadata_namespace) {
      continue;
    }
    if (attribute.qualified_name.local == "revision" && !revision) {
      revision = attribute.value;
    } else if (attribute.qualified_name.local == "magic" && !magic) {
      magic = attribute.value;
    } else {
      throw std::invalid_argument{"Duplicate or unknown reserved XML model metadata"};
    }
  }
  if (contract.revision && !revision) {
    throw std::invalid_argument{"XML model revision metadata is required"};
  }
  if (!contract.revision && revision) {
    throw std::invalid_argument{"Unversioned XML model cannot carry revision metadata"};
  }
  return select_model_layout(contract, revision.value_or(std::string_view{}), magic);
}

std::vector<xml::attribute> model_metadata_attributes(const model_contract& contract,
                                                      std::string_view revision) {
  std::string current{};
  if (contract.revision && revision.empty()) {
    current = contract.revision->default_value;
    (void)schema_version::parse(contract.revision->type_name_list.front().name, current);
    if (contract.revision->type_name_list.front().name.starts_with("version")) {
      current = current.substr(1U, current.size() - 2U);
    }
    revision = current;
  }
  (void)select_model_layout(contract, revision,
                            contract.magic ? std::optional<std::string_view>{*contract.magic}
                                           : std::nullopt);
  std::vector<xml::attribute> result{};
  if (contract.revision) {
    result.push_back({{"revision", std::string{model_metadata_namespace}}, std::string{revision}});
  }
  if (contract.magic) {
    result.push_back({{"magic", std::string{model_metadata_namespace}}, *contract.magic});
  }
  return result;
}

void validate_model_direction(const model_contract& contract, bool decoding) {
  if (decoding && contract.projection == projection_kind::borrowed_output) {
    throw std::invalid_argument{"Borrowed XML projections are synchronous output-only; retain a "
                                "live owner and decode into owning values"};
  }
}

std::vector<std::uint8_t> decode_model_digest(const type_name& type, std::string_view text,
                                              std::size_t max_digest_bytes,
                                              xml::operation_context* context) {
  if (!type.is_digest() || text.size() % 2U != 0 || text.size() / 2U > max_digest_bytes) {
    throw std::invalid_argument{"Invalid XML digest type, hexadecimal length or byte bound"};
  }
  const auto count = text.size() / 2U;
  const auto algorithm_extent = rohit::digest_size(type.digest);
  if ((type.digest != rohit::digest_algorithm::none && algorithm_extent == 0U) ||
      (type.digest_extent != 0U && algorithm_extent != 0U &&
       type.digest_extent != algorithm_extent)) {
    throw std::invalid_argument{"XML digest algorithm and declared extent disagree"};
  }
  const auto fixed = type.digest_extent ? type.digest_extent : algorithm_extent;
  if (fixed != 0 && count != fixed) {
    throw std::invalid_argument{"XML digest does not match its declared extent"};
  }
  if (context) {
    context->consume(xml::operation_resource::work_units, text.size());
    context->consume(xml::operation_resource::allocation_bytes, count);
  }
  std::vector<std::uint8_t> result(count);
  if (!rohit::digest_from_hex(text, result)) {
    throw std::invalid_argument{"XML digest requires exact hexadecimal bytes"};
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
