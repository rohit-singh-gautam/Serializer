#pragma once

#include "managed_schema.hpp"

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

  // Reject unsupported ownership and metadata collisions before emitting any target language.
  void validate(const class_node* node) {
    if (validated_.contains(node)) {
      return;
    }
    if (!visiting_.insert(node).second) {
      throw std::invalid_argument{"Recursive managed ownership schemas are not supported"};
    }
    if (!node->parents.empty() ||
        node->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (node->attributes & class_attributes::packed) != class_attributes::none) {
      throw std::invalid_argument{
          "Managed models require unpacked owning classes without inheritance"};
    }
    for (const auto& field : node->member_list) {
      if (field.access != access_type::public_access ||
          field.modifier == member::modifier_type::variant) {
        throw std::invalid_argument{"Managed models require public fields without unions"};
      }
      if (node->supports_managed() && (field.id == constants::variable_four_byte_max ||
                                       field.display_name == "persistent_id")) {
        throw std::invalid_argument{
            "Managed persistent_id name and field ID 1073741823 are reserved"};
      }
      for (const auto& type : field.type_name_list) {
        if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
          validate(static_cast<const class_node*>(type.resolved_node));
        }
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
      } else {
        const auto& original = static_cast<const class_node&>(*node);
        auto object = std::make_unique<class_node>(node->type, std::string{node->name},
                                                   node->parent_namespace, original.attributes,
                                                   std::vector<parent>{original.parents});
        object->storage_modes = original.storage_modes;
        object->member_list = original.member_list;
        if (original.supports_managed()) {
          validate(&original);
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
          object->member_list.push_back(std::move(identity));
        }
        copy = std::move(object);
      }
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
      if (target->type != object_type::class_type) {
        continue;
      }
      auto& object = static_cast<class_node&>(*target);
      for (auto& base : object.parents) {
        base.current_namespace = remap(base.current_namespace);
        base.parent_class = remap(base.parent_class);
      }
      for (auto& field : object.member_list) {
        field.key_node = remap(field.key_node);
        for (auto& type : field.type_name_list) {
          type.declared_namespace = remap(type.declared_namespace);
          type.defined_namespace = remap(type.defined_namespace);
          type.resolved_node = remap(type.resolved_node);
        }
      }
    }
  }

public:
  std::map<const class_node*, std::string> bindings{};
  std::vector<std::unique_ptr<syntax_node>> statements{};

  // Lower the existing default direct representation with either supported identity width.
  explicit managed_lowering(const std::vector<std::unique_ptr<syntax_node>>& source,
                            std::string id_type = "uint32")
      : id_type_{std::move(id_type)} {
    if (id_type_ != "uint32" && id_type_ != "uint64") {
      throw std::invalid_argument{"managed.id_type must be uint32 or uint64"};
    }
    statements = clone(source);
    rebind();
  }
};

} // namespace rohit::serializer::writer
