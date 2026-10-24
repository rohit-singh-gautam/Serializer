#pragma once

#include "managed_schema.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rohit::serializer::writer {

// Own an independent direct-identity schema while retaining original declaration metadata.
// The source AST must outlive this object; no generator may mutate the source tree.
class managed_lowering {
  std::map<const syntax_node*, syntax_node*> copies_{};
  std::set<const class_node*> visiting_{};
  std::set<const class_node*> validated_{};
  std::map<const class_node*, std::uint64_t> hashes_{};
  std::string id_type_;
  bool codec_only_{};
  persistent_id_encoding id_encoding_{};

  // Identify values whose ordinary reset would leave a required token invalid.
  static bool requires_token(const type_name& type, std::set<const class_node*>& visited) {
    if (type.is_managed_reference()) { return true; }
    if (type.is_optional() || type.is_sequence() || type.is_associative()) { return false; }
    if (!type.resolved_node || type.resolved_node->type != object_type::class_type) { return false; }
    const auto* record = static_cast<const class_node*>(type.resolved_node);
    if (record->reference || !visited.insert(record).second) { return false; }
    for (const auto& base : record->parents) {
      type_name base_type{std::string{base.parent_class->name}, record->parent_namespace};
      base_type.resolved_node = base.parent_class;
      if (requires_token(base_type, visited)) { return true; }
    }
    return std::any_of(record->member_list.begin(), record->member_list.end(),
        [&](const auto& field) { return requires_token(field, visited); });
  }

  // Empty variable containers are valid resets; fixed arrays and first variants retain requirements.
  static bool requires_token(const member& field, std::set<const class_node*>& visited) {
    if (std::any_of(field.type_name_list.begin(), field.type_name_list.end(),
        [](const auto& type) { return type.contains_managed_reference(); }) &&
        (field.initializer || !field.default_value.empty() || !field.default_factory.empty())) {
      return true;
    }
    if (field.modifier == member::modifier_type::map ||
        (field.modifier == member::modifier_type::array && field.extent_expression.empty() &&
         field.fixed_extent == 0U)) {
      return false;
    }
    return !field.type_name_list.empty() && requires_token(field.type_name_list.front(), visited);
  }

  // Reject unsupported ownership and metadata collisions before emitting any target language.
  void validate(const class_node* node) {
    if (node->reference) {
      throw std::invalid_argument{"Managed storage cannot contain ordinary live reference bindings"};
    }
    if (validated_.contains(node)) {
      return;
    }
    if (!visiting_.insert(node).second) {
      throw std::invalid_argument{"Recursive managed ownership schemas are not supported"};
    }
    if (node->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (node->attributes & class_attributes::packed) != class_attributes::none) {
      throw std::invalid_argument{
          "Managed models require unpacked owning classes"};
    }
    std::set<const class_node*> ancestors;
    const auto check_ancestors = [&](auto&& recurse, const class_node* current) -> void {
      for (const auto& base : current->parents) {
        if (base.access != access_type::public_access || !ancestors.insert(base.parent_class).second) {
          throw std::invalid_argument{"Managed inheritance requires distinct public nonvirtual bases"};
        }
        recurse(recurse, base.parent_class);
      }
    };
    check_ancestors(check_ancestors, node);
    for (const auto& base : node->parents) { validate(base.parent_class); }
    for (const auto& field : node->member_list) {
      if (field.modifier == member::modifier_type::variant && !field.owning_variant) {
        throw std::invalid_argument{"Managed models require owning variants instead of raw unions"};
      }
      if (node->supports_managed() && (field.id == constants::variable_four_byte_max ||
                                       field.display_name == "persistent_id")) {
        throw std::invalid_argument{
            "Managed persistent_id name and field ID 1073741823 are reserved"};
      }
      for (const auto& type : field.type_name_list) {
        const auto check = [&](auto&& recurse, const type_name& value) -> void {
          if (value.is_managed_reference()) { return; }
          if (value.resolved_node && value.resolved_node->type == object_type::class_type) {
            validate(static_cast<const class_node*>(value.resolved_node));
          }
          for (const auto& argument : value.arguments) { recurse(recurse, argument); }
        };
        check(check, type);
      }
    }
    visiting_.erase(node);
    validated_.insert(node);
  }

  // Copy declarations first so forward/self references can be rebound in a second pass.
  std::vector<std::unique_ptr<syntax_node>>
  clone(const std::vector<std::unique_ptr<syntax_node>>& source) {
    std::vector<std::unique_ptr<syntax_node>> result;
    for (const auto& node : source) {
      if (codec_only_ && node->type != object_type::namespace_type &&
          node->type != object_type::enum_type && node->type != object_type::class_type) {
        throw std::invalid_argument{"Managed token codecs require concrete record and enum declarations"};
      }
      std::unique_ptr<syntax_node> copy;
      if (node->type == object_type::namespace_type) {
        auto space = std::make_unique<namespace_node>(node->type, std::string{node->name},
                                                      node->parent_namespace);
        space->statements = clone(static_cast<const namespace_node&>(*node).statements);
        copy = std::move(space);
      } else if (node->type == object_type::enum_type) {
        copy = std::make_unique<enum_node>(
            node->type, std::string{node->name}, node->parent_namespace,
            std::vector<std::string>{static_cast<const enum_node&>(*node).enum_name_list});
      } else if (node->type == object_type::native_code) {
        copy = std::make_unique<native_code_node>(static_cast<const native_code_node&>(*node).code,
                                                  node->parent_namespace);
      } else {
        const auto& original = static_cast<const class_node&>(*node);
        auto object = std::make_unique<class_node>(node->type, std::string{node->name},
                                                   node->parent_namespace, original.attributes,
                                                   std::vector<parent>{original.parents});
        object->storage_modes = original.storage_modes;
        object->member_list = original.member_list;
        object->reference = original.reference;
        object->functions = original.functions;
        object->constructions = original.constructions;
        object->conversions = original.conversions;
        object->arithmetic = original.arithmetic;
        object->native_blocks = original.native_blocks;
        object->body_order = original.body_order;
        object->magic_bytes = original.magic_bytes;
        object->magic_field = original.magic_field;
        object->magic_access = original.magic_access;
        object->magic_id = original.magic_id;
        object->magic_explicit_id = original.magic_explicit_id;
        object->magic_omitted_formats = original.magic_omitted_formats;
        object->generic_name = original.generic_name;
        object->generic_arguments = original.generic_arguments;
        object->generic_parameters = original.generic_parameters;
        object->type_parameters = original.type_parameters;
        object->instance_of = original.instance_of;
        object->reserved_ids = original.reserved_ids;
        object->reserved_variables = original.reserved_variables;
        object->reserved_names = original.reserved_names;
        object->named_generic_source = original.named_generic_source;
        object->named_generic_arguments = original.named_generic_arguments;
        object->named_managed_self_binding = original.named_managed_self_binding;
        object->explicit_class_id = original.explicit_class_id;
        object->xml = original.xml;
        object->prepared_xml_validation = original.prepared_xml_validation;
        if (codec_only_) {
          if (!original.generic_name.empty() || !original.functions.empty() ||
              !original.constructions.empty() || !original.conversions.empty() ||
              !original.arithmetic.empty() || original.reference || !original.native_blocks.empty()) {
            throw std::invalid_argument{"Managed token codecs require ordinary concrete records without behavior or bindings"};
          }
          for (const auto& base : original.parents) {
            if (base.managed && !base.parent_class->supports_managed()) {
              throw std::invalid_argument{"Managed bases require an explicitly managed target class"};
            }
          }
          for (const auto& field : original.member_list) {
            if (field.cached || field.indexing != member::index_method::none || !field.readonly_getter.empty()) {
              throw std::invalid_argument{"Managed token codecs do not provide caches, indexes or readonly exposure"};
            }
            if (field.managed) {
              for (const auto& type : field.type_name_list) {
                if (!type.resolved_node || type.resolved_node->type != object_type::class_type ||
                    !static_cast<const class_node*>(type.resolved_node)->supports_managed()) {
                  throw std::invalid_argument{"Managed members require an explicitly managed target class"};
                }
              }
            }
            std::set<const class_node*> token_path;
            if ((field.transient || !field.omitted_formats.empty()) && requires_token(field, token_path)) {
              throw std::invalid_argument{"Required managed tokens cannot be transient or omitted"};
            }
          }
        }
        if (original.supports_managed() && node->type != object_type::generic_definition) {
          validate(&original);
          if (codec_only_) {
            constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
            constexpr std::uint64_t prime = 1099511628211ULL;
            auto tag = offset_basis;
            for (const auto byte : original.get_full_name()) {
              tag = (tag ^ static_cast<unsigned char>(byte)) * prime;
            }
            tag = original.explicit_class_id.value_or(tag);
            if (tag == 0 || std::any_of(class_ids.begin(), class_ids.end(),
                [&](const auto& item) { return item.second == tag; })) {
              throw std::invalid_argument{"Managed class identity is zero or collides"};
            }
            class_ids.emplace(object.get(), tag);
          }
          bindings.emplace(object.get(),
                           "managed.direct.v1:" + original.get_full_name() + ":" +
                               std::to_string(managed_schema_hash(&original, hashes_)) + ":" +
                               id_type_);
          member identity{access_type::public_access,
                          member::modifier_type::none,
                          {type_name{std::string{id_type_}, original.parent_namespace}},
                          "persistent_id",
                          "persistent_id",
                          constants::variable_four_byte_max,
                          "",
                          "",
                          true};
          identity.type_name_list.front().type = object_type::primitive;
          identity.fixed_name = true;
          if (codec_only_ && id_encoding_ == persistent_id_encoding::compact_varint) {
            identity.compact = compact_encoding::varint;
          }
          object->member_list.push_back(std::move(identity));
          if (!object->body_order.empty()) {
            object->body_order.push_back({class_body_item::kind_type::field,
                                          object->member_list.size() - 1});
          }
        }
        if (codec_only_) {
          for (const auto& field : original.member_list) {
            if (field.managed) { managed_fields[object.get()].insert(field.name); }
          }
          object->attributes = static_cast<class_attributes>(static_cast<std::uint8_t>(object->attributes) &
              ~static_cast<std::uint8_t>(class_attributes::managed));
          for (auto& base : object->parents) { base.managed = false; }
          for (auto& field : object->member_list) { field.managed = false; }
        }
        copy = std::move(object);
      }
      copy->source_path = node->source_path;
      copy->source_notices = node->source_notices;
      copies_.emplace(node.get(), copy.get());
      result.push_back(std::move(copy));
    }
    return result;
  }

  // Rebind only owned AST pointers; null namespaces and primitive types remain null.
  template <typename T>
  T* remap(T* value) const {
    return value ? static_cast<T*>(copies_.at(value)) : nullptr;
  }

  // Resolve copied fields against copied declarations without changing names or field IDs.
  void rebind() {
    for (const auto& [source, target] : copies_) {
      target->parent_namespace = remap(source->parent_namespace);
      if (target->type != object_type::class_type &&
          target->type != object_type::generic_definition) {
        continue;
      }
      auto& object = static_cast<class_node&>(*target);
      if (object.reference) {
        const auto bind = [&](auto&& recurse, type_name& value) -> void {
          value.declared_namespace = remap(value.declared_namespace);
          value.defined_namespace = remap(value.defined_namespace);
          value.resolved_node = remap(value.resolved_node);
          for (auto& argument : value.arguments) { recurse(recurse, argument); }
        };
        bind(bind, object.reference->owner);
        bind(bind, object.reference->value_type);
      }
      for (auto& base : object.parents) {
        base.current_namespace = remap(base.current_namespace);
        base.parent_class = remap(base.parent_class);
      }
      for (auto& field : object.member_list) {
        field.key_node = remap(field.key_node);
        for (auto& type : field.type_name_list) {
          const auto bind = [&](auto&& recurse, type_name& value) -> void {
            value.declared_namespace = remap(value.declared_namespace);
            value.defined_namespace = remap(value.defined_namespace);
            value.resolved_node = remap(value.resolved_node);
            for (auto& argument : value.arguments) { recurse(recurse, argument); }
          };
          bind(bind, type);
        }
      }
    }
  }

public:
  std::map<const class_node*, std::string> bindings{};
  std::map<const class_node*, std::uint64_t> class_ids{};
  std::map<const class_node*, std::set<std::string>> managed_fields{};
  std::vector<std::unique_ptr<syntax_node>> statements{};

  // Lower the existing default direct representation with either supported identity width.
  explicit managed_lowering(const std::vector<std::unique_ptr<syntax_node>>& source,
                            std::string id_type = "uint32", bool codec_only = false,
                            persistent_id_encoding id_encoding = persistent_id_encoding::compact_varint)
      : id_type_{std::move(id_type)}, codec_only_{codec_only}, id_encoding_{id_encoding} {
    if (id_type_ != "uint32" && id_type_ != "uint64") {
      throw std::invalid_argument{"managed.id_type must be uint32 or uint64"};
    }
    statements = clone(source);
    rebind();
  }
};

} // namespace rohit::serializer::writer
