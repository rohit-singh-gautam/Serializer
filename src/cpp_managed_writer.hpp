#pragma once

#include <rohit/serializer_creator.hpp>
#include "managed_schema.hpp"
#include "cpp_naming.hpp"
#include "schema_version.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::cpp {
// Generate direct managed classes or opt-in companions through the ordinary codec pipeline.
template <typename Writer>
class managed_writer {
  Writer& writer_;
  std::string managed_id_type;
  bool separate_values_;

  // Delegate public type spelling to the owning writer's configured profile.
  std::string type_name(std::string_view name) const {
    return writer_.type_name(name);
  }
  // Delegate payload-field spelling while runtime metadata keeps its fixed ABI.
  std::string field_name(std::string_view name) const {
    return writer_.field_name(name);
  }
  // Apply the owning profile to generated accessors and setters.
  std::string function_name(std::string_view name) const {
    return writer_.function_name(name);
  }
  // Resolve a declaration in its renamed C++ namespace.
  std::string full_type_name(const syntax_node* node) const {
    return writer_.full_type_name(node);
  }
  // Locate the identified terminal declaration beneath built-in owning wrappers.
  const syntax_node* child_node(const serializer::type_name& type) const {
    if (type.is_optional() || type.is_sequence() || type.is_associative()) {
      return child_node(type.arguments.back());
    }
    return type.resolved_node;
  }
  // Identify wrapper type expressions requiring recursive identity/projection traversal.
  bool wrapped_type(const member& field) const {
    const auto& type = field.type_name_list.front();
    return type.is_optional() || type.is_sequence() || type.is_associative();
  }
  // Identify ordinary collection fields eligible for callback-scoped transaction operations.
  bool collection_type(const member& field) const {
    const auto& type = field.type_name_list.front();
    return field.modifier == member::modifier_type::array ||
           field.modifier == member::modifier_type::map ||
           type.is_sequence() || type.is_associative();
  }
  // Generate synthetic data and metadata through the ordinary schema codec pipeline.
  void write_owning_class(auto& output, const class_node* node) {
    writer_.write_owning_class(output, node);
  }

  // Adapt conditional string literals to the stream's length-aware text API.
  template <typename Value>
  static decltype(auto) managed_text(const Value& value) {
    if constexpr (std::is_same_v<Value, const char*>) {
      return std::string_view{value};
    } else {
      return (value);
    }
  }

  // Write generated text without passing zero-terminated pointers to the bounded stream API.
  void write_managed_text(auto& output, const auto&... values) {
    output.write(managed_text(values)...);
  }

  // A lowered managed schema reuses ordinary codec emission through its owning writer.
  struct managed_class {
    const class_node* source{};
    std::unique_ptr<class_node> data{};
    std::unique_ptr<class_node> storage{};
  };
  std::vector<const class_node*> editor_classes{};
  std::vector<const class_node*> visiting_editors{};
  std::vector<managed_class> managed_classes{};
  std::map<const class_node*, std::uint64_t> managed_hashes{};

  // Preserve codec lifetimes and behavior while lowering only managed identity metadata.
  static void copy_class_metadata(class_node& target, const class_node& source) {
    target.attributes = source.attributes;
    target.member_list = source.member_list;
    target.functions = source.functions;
    target.constructions = source.constructions;
    target.conversions = source.conversions;
    target.native_blocks = source.native_blocks;
    target.named_generic_source = source.named_generic_source;
    target.body_order = source.body_order;
    target.magic_bytes = source.magic_bytes;
    target.magic_field = source.magic_field;
    target.magic_access = source.magic_access;
    target.magic_id = source.magic_id;
    target.magic_explicit_id = source.magic_explicit_id;
    target.magic_omitted_formats = source.magic_omitted_formats;
    target.reserved_ids = source.reserved_ids;
    target.reserved_variables = source.reserved_variables;
    target.reserved_names = source.reserved_names;
  }

  // Resolve a generated companion by schema declaration identity, independently of spelling.
  const managed_class& managed_for(const syntax_node* source) const {
    for (const auto& item : managed_classes) {
      if (item.source == source) {
        return item;
      }
    }
    throw std::invalid_argument{"Missing managed companion"};
  }

  // Collect declarations in dependency order; the parser already rejects forward references.
  void collect_classes(const std::vector<std::unique_ptr<syntax_node>>& statements,
                       std::vector<const class_node*>& result) {
    for (const auto& node : statements) {
      if (node->type == object_type::namespace_type) {
        collect_classes(static_cast<const namespace_node*>(node.get())->statements, result);
      } else if (node->type == object_type::class_type) {
        result.push_back(static_cast<const class_node*>(node.get()));
      }
    }
  }

  // Validate every value reached by a managed editor, including ordinary owned child values.
  void collect_editor(const class_node* source) {
    if (std::find(visiting_editors.begin(), visiting_editors.end(), source) !=
        visiting_editors.end()) {
      throw std::invalid_argument{"Recursive managed ownership schemas are not supported: " +
                                  source->get_full_name()};
    }
    if (std::find(editor_classes.begin(), editor_classes.end(), source) != editor_classes.end()) {
      return;
    }
    if (source->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (source->attributes & class_attributes::packed) == class_attributes::packed) {
      throw std::invalid_argument{
          "Managed C++ editors require unpacked owning classes: " +
          source->get_full_name()};
    }
    editor_classes.push_back(source);
    visiting_editors.push_back(source);
    std::set<const class_node*> ancestors;
    const auto check_ancestors = [&](auto&& recurse, const class_node* node) -> void {
      for (const auto& base : node->parents) {
        if (base.access != access_type::public_access || !ancestors.insert(base.parent_class).second) {
          throw std::invalid_argument{"Managed inheritance requires distinct public nonvirtual bases"};
        }
        recurse(recurse, base.parent_class);
      }
    };
    check_ancestors(check_ancestors, source);
    for (const auto& base : source->parents) { collect_editor(base.parent_class); }
    for (const auto& field : source->member_list) {
      if (field.modifier == member::modifier_type::variant && !field.owning_variant) {
        throw std::invalid_argument{"Managed C++ editors require owning variants instead of raw unions: " +
                                    source->get_full_name()};
      }
      for (const auto& type : field.type_name_list) {
        const auto* node = child_node(type);
        if (node && node->type == object_type::class_type) {
          collect_editor(static_cast<const class_node*>(node));
        }
      }
    }
    visiting_editors.pop_back();
  }

  // Include enums and namespaces when checking synthetic names against existing declarations.
  void collect_managed_names(const std::vector<std::unique_ptr<syntax_node>>& statements,
                             std::set<std::string>& names) {
    for (const auto& node : statements) {
      names.insert(full_type_name(node.get()));
      if (node->type == object_type::namespace_type) {
        collect_managed_names(static_cast<const namespace_node*>(node.get())->statements, names);
      }
    }
  }

  // Lower identity metadata into ordinary schema nodes and reuse the established codec generator.
  void prepare_managed(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    if (managed_id_type != "uint32" && managed_id_type != "uint64") {
      throw std::invalid_argument{"managed.id_type must be uint32 or uint64"};
    }
    std::vector<const class_node*> classes;
    collect_classes(statements, classes);
    for (const auto* source : classes) {
      if (!source->supports_managed()) {
        continue;
      }
      collect_editor(source);
      managed_class item;
      item.source = source;
      if (!separate_values_) {
        if (writer_.protobuf_enabled) {
          throw std::invalid_argument{"Direct managed classes do not support Protobuf output"};
        }
        // Reserve a metadata key without renumbering application fields or initializers.
        constexpr auto identity_field_id = constants::variable_four_byte_max;
        for (const auto& field : source->member_list) {
          if (field.id == identity_field_id || field.display_name == "persistent_id" ||
              field_name(field.name) == "persistent_id") {
            throw std::invalid_argument{
                "Managed persistent_id name and field ID 1073741823 are reserved"};
          }
        }
        item.data = std::make_unique<class_node>(
            object_type::class_type, std::string{source->name}, source->parent_namespace,
            class_attributes::stable_ids, std::vector<parent>{source->parents});
        copy_class_metadata(*item.data, *source);
        member id{access_type::public_access,
                  member::modifier_type::none,
                  {serializer::type_name{std::string{managed_id_type}, source->parent_namespace}},
                  "persistent_id",
                  "persistent_id",
                  identity_field_id,
                  "",
                  "",
                  true};
        id.type_name_list.front().type = object_type::primitive;
        id.fixed_name = true;
        item.data->member_list.push_back(std::move(id));
        if (!item.data->body_order.empty()) {
          item.data->body_order.push_back({class_body_item::kind_type::field,
                                           item.data->member_list.size() - 1});
        }
        managed_classes.push_back(std::move(item));
        continue;
      }
      item.data = std::make_unique<class_node>(
          object_type::class_type, "managed_" + source->name + "_data", source->parent_namespace,
          class_attributes::stable_ids, std::vector<parent>{});
      copy_class_metadata(*item.data, *source);
      item.data->parents = source->parents;
      for (auto& base : item.data->parents) {
        if (base.managed) {
          base.parent_class = managed_for(base.parent_class).storage.get();
        }
      }
      for (auto& field : item.data->member_list) {
        if (field.managed) {
          field.default_value.clear();
          for (auto& type : field.type_name_list) {
            const auto bind = [&](auto&& recurse, serializer::type_name& value) -> void {
              if (value.is_optional() || value.is_sequence() || value.is_associative()) {
                recurse(recurse, value.arguments.back());
              } else {
                value.resolved_node = managed_for(value.resolved_node).storage.get();
              }
            };
            bind(bind, type);
          }
        }
      }
      item.storage = std::make_unique<class_node>(
          object_type::class_type, "managed_" + source->name + "_storage", source->parent_namespace,
          class_attributes::stable_ids, std::vector<parent>{});
      member id{access_type::public_access,
                member::modifier_type::none,
                {serializer::type_name{std::string{managed_id_type}, source->parent_namespace}},
                "persistent_id",
                "persistent_id",
                1,
                "",
                "",
                true};
      id.type_name_list.front().type = object_type::primitive;
      id.fixed_name = true;
      member value{access_type::public_access,
                   member::modifier_type::none,
                   {serializer::type_name{std::string{item.data->name}, source->parent_namespace}},
                   "value",
                   "value",
                   2,
                   "",
                   "",
                   true};
      value.type_name_list.front().resolved_node = item.data.get();
      value.type_name_list.front().type = object_type::class_type;
      value.fixed_name = true;
      item.storage->member_list = {std::move(id), std::move(value)};
      managed_classes.push_back(std::move(item));
    }
    // Restore schema dependency order after recursive reachability discovery.
    std::vector<const class_node*> ordered;
    for (const auto* source : classes) {
      if (std::find(editor_classes.begin(), editor_classes.end(), source) != editor_classes.end()) {
        ordered.push_back(source);
      }
    }
    editor_classes = std::move(ordered);
    std::set<std::string> names;
    collect_managed_names(statements, names);
    for (const auto& item : managed_classes) {
      if (!separate_values_) {
        continue;
      }
      for (const auto* generated : {item.data.get(), item.storage.get()}) {
        if (!names.insert(full_type_name(generated)).second) {
          throw std::invalid_argument{"Managed companion name collision"};
        }
      }
    }
    for (const auto* source : editor_classes) {
      if (!names.insert(editor_name(source)).second) {
        throw std::invalid_argument{"Managed editor name collision"};
      }
      std::set<std::string> methods{"id", "access_", "payload_type", "Access",
                                    type_name(source->name + "_editor")};
      for (const auto& base : source->parents) {
        if (!methods.insert(base_accessor_name(base)).second) {
          throw std::invalid_argument{"Managed base editor method name collision"};
        }
      }
      for (const auto& field : source->member_list) {
        const auto reserve_method = [&](std::string_view prefix) {
          if (!methods.insert(function_name(std::string{prefix} + field.name)).second) {
            throw std::invalid_argument{"Managed editor method name collision"};
          }
        };
        reserve_method("get_");
        if (field.owning_variant) {
          reserve_method("emplace_");
          reserve_method("edit_");
        } else if (!field.version) { reserve_method("set_"); }
        if (!field.owning_variant && collection_type(field)) { reserve_method("edit_"); }
        const auto* node = child_node(field.type_name_list.front());
        if (!field.owning_variant && (field.managed ||
            (node && node->type == object_type::class_type && field.modifier == member::modifier_type::none))) {
          if (!methods.insert(function_name(field.name)).second) {
            throw std::invalid_argument{"Managed editor method name collision"};
          }
        }
      }
      for (const auto& method : source->functions) {
        if (method.effect == function_declaration::effect_type::edit &&
            methods.contains(function_name(method.name))) {
          throw std::invalid_argument{"Managed behavior method collides with a generated editor accessor"};
        }
      }
    }
  }

  // Preserve simple base aliases while keeping qualified or arbitrary wire names out of C++ identifiers.
  std::string base_accessor_name(const parent& base) const {
    const auto alias = function_name(base.display_name);
    return naming::valid_identifier(alias) ? alias : function_name("base_" + std::to_string(base.id));
  }

  // Select the representation of an independently managed base occurrence for this output policy.
  std::string base_storage_type(const parent& base) const {
    if (separate_values_ && base.managed) { return full_type_name(managed_for(base.parent_class).storage.get()); }
    return full_type_name(base.parent_class);
  }

  // Capability traversal follows every durable type and base, independently of presentation names.
  bool has_inherited_field_paths(const class_node* source, std::set<const class_node*>& visited) const {
    if (!visited.insert(source).second) { return false; }
    for (const auto& base : source->parents) {
      if (!base.managed || has_inherited_field_paths(base.parent_class, visited)) { return true; }
    }
    for (const auto& field : source->member_list) {
      if (field.transient) { continue; }
      for (const auto& type : field.type_name_list) {
        const auto* child = child_node(type);
        if (child && child->type == object_type::class_type &&
            has_inherited_field_paths(static_cast<const class_node*>(child), visited)) {
          return true;
        }
      }
    }
    return false;
  }

  // Give editor types the same namespace and presentation profile as ordinary generated classes.
  std::string editor_name(const class_node* source) {
    class_node node{object_type::class_type,
                    source->name + "_editor",
                    source->parent_namespace,
                    class_attributes::none,
                    {}};
    return full_type_name(&node);
  }

  // Open the full generated namespace, including reopened or qualified schema namespaces.
  void open_managed_namespace(auto& output, const class_node* source) {
    if (source->parent_namespace) {
      const auto full = full_type_name(source);
      write_managed_text(output, "namespace ", full.substr(2, full.rfind("::") - 2), " {\n");
    }
  }

  // Close only a namespace emitted for this companion.
  void close_managed_namespace(auto& output, const class_node* source) {
    if (source->parent_namespace) {
      write_managed_text(output, "}\n");
    }
  }

  // Emit typed mutation methods for both entity occurrences and ordinary child values.
  void write_editor(auto& output, const class_node* source) {
    open_managed_namespace(output, source);
    const auto name = type_name(source->name + "_editor");
    write_managed_text(
        output, "template <typename Access> class ", name,
        " {\n  Access access_;\n"
        "  using payload_type = typename Access::value_type;\npublic:\n"
        "  // Bind generated operations to one checked transaction path.\n  explicit ",
        name,
        "(Access access) : access_{::std::move(access)} {}\n"
        "  // Return the immutable identity only for a managed occurrence.\n"
        "  auto id() const requires Access::is_managed { return access_.id(); }\n");
    for (const auto& base : source->parents) {
      const auto type = full_type_name(base.parent_class);
      write_managed_text(output, "  // Edit this inherited occurrence without replacing its lifetime.\n  auto ",
          base_accessor_name(base), "() const { ");
      if (separate_values_ && base.managed) {
        write_managed_text(output, "if constexpr (Access::is_managed) { auto base = access_.template base<",
            base_storage_type(base), ", true>(); return ", editor_name(base.parent_class),
            "<decltype(base)>{::std::move(base)}; } else { auto base = access_.template base<", type,
            ", false>(); return ", editor_name(base.parent_class),
            "<decltype(base)>{::std::move(base)}; } }\n");
      } else {
        write_managed_text(output, "auto base = access_.template base<", type,
            base.managed ? ", true" : ", false", ">(); return ", editor_name(base.parent_class),
            "<decltype(base)>{::std::move(base)}; }\n");
      }
    }
    for (const auto& field : source->member_list) {
      if (field.owning_variant) { write_variant_editor(output, source, field); continue; }
      const auto member = field_name(field.name);
      const auto* child = child_node(field.type_name_list.front());
      const auto child_class = child && child->type == object_type::class_type
                                   ? static_cast<const class_node*>(child)
                                   : nullptr;
      // Ordinary getters return copies; managed children are addressed through editors, never raw storage.
      const auto constraint = field.transient ? " requires Access::is_runtime_access" :
          field.managed ? " requires (!Access::is_managed && !Access::is_runtime_access)" :
                          " requires (!Access::is_runtime_access)";
      write_managed_text(
          output, "  // Copy this owned value without exposing mutable snapshot storage.\n  auto ",
          function_name("get_" + field.name), "() const", constraint,
          " { ::std::optional<decltype(payload_type::", member,
          ")> result;\n    access_.update([&](const auto& value) { ", lifetime_guard(source, field, "value"),
          "result.emplace(value.", member, "); }); return ::std::move(*result); }\n");
      if (!field.version) {
        write_managed_text(output,
            "  // Assign this value inside the current transaction.\n  void ",
            function_name("set_" + field.name), "(decltype(payload_type::", member, ") value) const",
            constraint, " { access_.update([&](auto& target) { ", lifetime_guard(source, field, "target"),
            "target.", member, " = ::std::move(value); }); }\n");
      }
      if (collection_type(field)) {
        write_managed_text(output,
            "  // Edit ordinary collection elements under their nearest owner's checked boundary.\n  auto ",
            function_name("edit_" + field.name), "() const", constraint, " { ");
        const auto guard = lifetime_guard(source, field, "value");
        if (!guard.empty()) {
          write_managed_text(output, "access_.update([&](const auto& value) { ", guard, " }); ");
        }
        write_managed_text(output,
            "auto child = access_.template member<false>(&payload_type::", member,
            "); return ::rohit::managed::detail::value_collection_editor<decltype(child)>{::std::move(child)}; }\n");
      }
      if (!child_class) {
        continue;
      }
      if (wrapped_type(field)) {
        if (field.managed) {
          write_managed_text(output,
              "  // Edit engaged managed descendants through their standard owning wrappers.\n  auto ",
              function_name(field.name), "() const requires Access::is_managed { ",
              "auto child = access_.template member<false>(&payload_type::", member,
              "); return ::rohit::managed::detail::make_owned_editor<",
              writer_.get_cpp_type(field), ">(::std::move(child)); }\n");
        }
        continue;
      }
      if (field.managed) {
        write_managed_text(
            output, "  // Edit identified children only when this occurrence is managed.\n  auto ",
            function_name(field.name), "() const");
        if (field.modifier != member::modifier_type::none) {
          write_managed_text(output, " requires Access::is_managed");
        }
        write_managed_text(output, " {\n");
        if (!lifetime_guard(source, field, "value").empty()) {
          write_managed_text(output, "    access_.update([&](const auto& value) { ",
              lifetime_guard(source, field, "value"), "});\n");
        }
        write_managed_text(output,
                           "    if constexpr (Access::is_managed) {\n      auto child = "
                           "access_.template member<",
                           field.modifier == member::modifier_type::none ? "true" : "false",
                           ">(&payload_type::", member, ");\n");
        if (field.modifier == member::modifier_type::none) {
          write_managed_text(output, "      return ::rohit::managed::detail::make_editor<",
                             full_type_name(child), ">(::std::move(child));\n");
        } else {
          write_managed_text(
              output, "      return ::rohit::managed::detail::",
              field.modifier == member::modifier_type::map ? "map_editor" : "array_editor",
              "<decltype(child), ", full_type_name(child), ">{::std::move(child)};\n");
        }
        write_managed_text(output, "    }");
        if (field.modifier == member::modifier_type::none) {
          write_managed_text(
              output, " else { auto child = access_.template member<false>(&payload_type::", member,
              "); return ", editor_name(child_class), "<decltype(child)>{::std::move(child)}; }");
        }
        write_managed_text(output, "\n  }\n");
      } else if (field.modifier == member::modifier_type::none) {
        write_managed_text(
            output, "  // Edit a value child under its nearest managed owner's identity.\n  auto ",
            function_name(field.name),
            "() const { ");
        if (!lifetime_guard(source, field, "value").empty()) {
          write_managed_text(output, "access_.update([&](const auto& value) { ",
              lifetime_guard(source, field, "value"), "}); ");
        }
        write_managed_text(output, "auto child = access_.template member<false>(&payload_type::", member,
            "); return ", editor_name(child_class), "<decltype(child)>{::std::move(child)}; }\n");
      }
    }
    for (const auto& method : source->functions) {
      if (method.effect == function_declaration::effect_type::edit) {
        writer_.write_behavior_method(output, source, method, true);
      }
    }
    writer_.write_behavior_contract(output, source, true);
    write_managed_text(output, "};\n");
    close_managed_namespace(output, source);
  }

  // Bind owning alternatives through generation-checked paths; no mutable aliases escape.
  void write_variant_editor(auto& output, const class_node* source, const member& field) {
    const auto name = field_name(field.name);
    const auto constraint = field.transient ? " requires Access::is_runtime_access" :
                                             " requires (!Access::is_runtime_access)";
    write_managed_text(output,
        "  // Copy the active payload and discriminator together.\n  auto ",
        function_name("get_" + field.name), "() const", constraint,
        " { ::std::optional<decltype(payload_type::", name,
        ")> result; access_.update([&](const auto& value) { ", lifetime_guard(source, field, "value"), "result.emplace(value.", name,
        "); }); return ::std::move(*result); }\n"
        "  // Replace an owned alternative atomically inside the transaction candidate.\n  template<::std::size_t Index, class... Args> void ",
        function_name("emplace_" + field.name), "(Args&&... args) const", constraint,
        " { access_.guard([&] { using variant_type = decltype(payload_type::", name, ");\n");
    if (separate_values_ && field.managed) {
      write_managed_text(output, "    using plain_type = ::std::variant_alternative_t<Index, decltype(",
          full_type_name(source), "::", name, ")>; plain_type plain{::std::forward<Args>(args)...};\n"
          "    auto storage = ::rohit::managed::model_traits<plain_type>::make_storage(::std::move(plain));\n"
          "    variant_type replacement{::std::in_place_index<Index>, ::std::move(storage)};\n");
    } else {
      write_managed_text(output,
          "    variant_type replacement{::std::in_place_index<Index>, ::std::forward<Args>(args)...};\n");
    }
    if (field.managed) {
      write_managed_text(output,
          "    ::std::visit([&](auto& child) { using child_type = ::std::remove_cvref_t<decltype(child)>; "
          "::rohit::managed::model_traits<child_type>::visit_entities(child, "
          "[&](auto& id, auto) { id = access_.create_id(); }); }, replacement);\n");
    }
    write_managed_text(output,
        "    access_.replace_variant([&](auto& target) { ", lifetime_guard(source, field, "target"), "target.", name,
        " = ::std::move(replacement); }); }); }\n"
        "  // Select only the current alternative; any replacement invalidates this handle.\n"
        "  template<::std::size_t Index> auto ", function_name("edit_" + field.name), "() const { ");
    const auto guard = lifetime_guard(source, field, "value");
    if (!guard.empty()) {
      write_managed_text(output, "access_.update([&](const auto& value) { ", guard, " }); ");
    }
    write_managed_text(output, "auto variant = access_.template member<false>(&payload_type::",
        name, "); auto child = variant.template alternative<Index, ",
        field.managed ? "Access::is_managed" : "false", ">();\n"
        "");
    write_managed_text(output, "    if constexpr (false) {}\n");
    for (std::size_t index{}; index < field.type_name_list.size(); ++index) {
      const auto* type = field.type_name_list[index].resolved_node;
      if (type && type->type == object_type::class_type) {
        write_managed_text(output, "    else if constexpr (Index == ", index, ") { return ",
            editor_name(static_cast<const class_node*>(type)),
            "<decltype(child)>{::std::move(child)}; }\n");
      }
    }
    write_managed_text(output,
        "    else { return ::rohit::managed::detail::scalar_editor<decltype(child)>{::std::move(child)}; }\n  }\n");
  }

  // Generate the memory-only projection independently from persistence and identity metadata.
  void write_runtime_traits(auto& output, const class_node* source) {
    write_managed_text(output, "namespace rohit::managed {\n"
        "// Reset and copy only schema-declared memory fields; normal C++ copying is unchanged.\n"
        "template<> struct runtime_traits<", full_type_name(source), "> {\n"
        "  // Rebuild the memory projection from schema defaults without changing durable state.\n"
        "  static void reset(", full_type_name(source), "& value) {\n"
        "    static_cast<void>(value);\n");
    write_runtime_projection(output, source, false);
    write_managed_text(output, "  }\n  // Preserve durable values and identities while copying runtime state.\n"
        "  static void copy(", full_type_name(source), "& value, const ", full_type_name(source),
        "& source) {\n    static_cast<void>(value); static_cast<void>(source);\n");
    write_runtime_projection(output, source, true);
    write_managed_text(output, "  }\n"
        "  // Estimate resident model bytes, including runtime fields and owned container capacity.\n"
        "  static ::std::size_t estimate_memory(const ", full_type_name(source), "& value) {\n"
        "    ::std::size_t result = sizeof(value); static_cast<void>(value);\n");
    for (const auto& base : source->parents) {
      write_managed_text(output, "    detail::add_memory_estimate(result, detail::estimate_dynamic_memory(static_cast<const ",
          full_type_name(base.parent_class), "&>(value)));\n");
    }
    for (const auto& field : source->member_list) {
      write_managed_text(output, "    detail::add_memory_estimate(result, detail::estimate_dynamic_memory(value.",
          field_name(field.name), "));\n");
    }
    write_managed_text(output, "    return result;\n  }\n};\n}\n");
  }

  // Traverse base scopes, containers and active alternatives without interpreting runtime resources.
  void write_runtime_projection(auto& output, const class_node* source, bool copy) {
    for (const auto& base : source->parents) {
      const auto type = full_type_name(base.parent_class);
      write_managed_text(output, "    runtime_traits<", type, ">::", copy ? "copy" : "reset",
          "(static_cast<", type, "&>(value)");
      if (copy) { write_managed_text(output, ", static_cast<const ", type, "&>(source)"); }
      write_managed_text(output, ");\n");
    }
    for (const auto& field : source->member_list) {
      const auto name = field_name(field.name);
      if (field.transient) {
        write_managed_text(output, "    value.", name, " = ",
            copy ? "source." + name : field.default_value.empty() ? "decltype(value." + name + "){}" :
              "decltype(value." + name + "){" + field.default_value + "}", ";\n");
        continue;
      }
      if (field.owning_variant) {
        for (std::size_t index{}; index < field.type_name_list.size(); ++index) {
          write_managed_text(output, "    if (value.", name, ".index() == ", index);
          if (copy) { write_managed_text(output, " && source.", name, ".index() == ", index); }
          write_managed_text(output, ") { detail::",
              copy ? "copy_owned_runtime" : "reset_owned_runtime", "(::std::get<", index, ">(value.", name, ")");
          if (copy) { write_managed_text(output, ", ::std::get<", index, ">(source.", name, ")"); }
          write_managed_text(output, "); }\n");
        }
        continue;
      }
      if (wrapped_type(field)) {
        const auto* child = child_node(field.type_name_list.front());
        if (!child || child->type != object_type::class_type) { continue; }
        write_managed_text(output, "    detail::", copy ? "copy_owned_runtime" : "reset_owned_runtime",
            "(value.", name);
        if (copy) { write_managed_text(output, ", source.", name); }
        write_managed_text(output, ");\n");
        continue;
      }
      const auto* child = field.type_name_list.front().resolved_node;
      if (!child || child->type != object_type::class_type) { continue; }
      const auto type = full_type_name(child);
      if (field.modifier == member::modifier_type::none) {
        write_managed_text(output, "    runtime_traits<", type, ">::", copy ? "copy" : "reset",
            "(value.", name);
        if (copy) { write_managed_text(output, ", source.", name); }
        write_managed_text(output, ");\n");
      } else if (field.modifier == member::modifier_type::map) {
        write_managed_text(output, "    for (auto& [key, child] : value.", name, ") { ");
        if (copy) {
          write_managed_text(output, "const auto found = source.", name,
              ".find(key); if (found != source.", name, ".end()) { runtime_traits<", type,
              ">::copy(child, found->second); } ");
        } else { write_managed_text(output, "static_cast<void>(key); runtime_traits<", type, ">::reset(child); "); }
        write_managed_text(output, "}\n");
      } else {
        if (copy) {
          write_managed_text(output, "    for (::std::size_t index = 0; index < value.", name,
              ".size() && index < source.", name, ".size(); ++index) { runtime_traits<", type,
              ">::copy(value.", name, "[index], source.", name, "[index]); }\n");
        } else {
          write_managed_text(output, "    for (auto& child : value.", name, ") { runtime_traits<", type,
              ">::reset(child); }\n");
        }
      }
    }
  }

  // Evaluate a field's durable lifetime using the class's existing payload-version contract.
  std::string active_field(const class_node* source, const member& field,
                           const std::string& value) const {
    const auto* version = source->version_member();
    if (!version || (field.created_version.empty() && field.obsolete_version.empty())) { return "true"; }
    const auto revision = value + "." + field_name(version->name);
    std::string condition;
    if (!field.created_version.empty()) {
      condition = "!(" + revision + " < " + schema_version::cpp_literal(*version, field.created_version) + ")";
    }
    if (!field.obsolete_version.empty()) {
      if (!condition.empty()) { condition += " && "; }
      condition += revision + " < " + schema_version::cpp_literal(*version, field.obsolete_version);
    }
    return condition;
  }

  // Reject edits to retired/not-yet-created fields before changing candidate storage.
  std::string lifetime_guard(const class_node* source, const member& field,
                             const std::string& value) const {
    const auto condition = active_field(source, field, value);
    return condition == "true" ? "" : "if (!(" + condition + ")) { throw ::std::invalid_argument{\"Managed field outside payload-version lifetime\"}; } ";
  }

  // Hash the complete reachable schema contract, not the C++ presentation names or source whitespace.
  std::uint64_t hash_managed_schema(const class_node* source) {
    return managed_schema_hash(source, managed_hashes);
  }

  // Emit deep ordinary/storage conversion while preserving field wire IDs and application map keys.
  void write_conversion(auto& output, const managed_class& item, bool to_storage) {
    const auto plain = full_type_name(item.source);
    if (!separate_values_) {
      write_managed_text(
          output,
          to_storage ? "  // Adopt a value; creation assigns IDs, loading preserves saved IDs.\n"
                     : "  // Copy an identified value independently of the store lifetime.\n",
          "  static ", plain, to_storage ? " make_storage(" : " clone_value(const ", plain,
          to_storage ? " input) { return input; }\n" : "& input) { return input; }\n");
      return;
    }
    write_managed_text(
        output, "  // ",
        to_storage ? "Create unbound storage; the store allocates all entity IDs."
                   : "Deep-copy ordinary values without persistent identity metadata.",
        "\n  static ", to_storage ? "storage_type" : plain,
        to_storage ? " make_storage(" : " clone_value(const storage_type& input",
        to_storage ? plain + " input" : "", ") {\n    ", to_storage ? "storage_type" : plain,
        " result{};\n    static_cast<void>(input);\n");
    for (const auto& base : item.source->parents) {
      const auto type = full_type_name(base.parent_class);
      const auto stored = base_storage_type(base);
      write_managed_text(output, "    static_cast<", to_storage ? stored : type, "&>(",
          to_storage ? "result.value" : "result", ") = ");
      if (base.managed) {
        write_managed_text(output, "::rohit::managed::model_traits<", type, ">::",
            to_storage ? "make_storage(::std::move(static_cast<" : "clone_value(static_cast<const ",
            to_storage ? type : stored,
            to_storage ? "&>(input)));\n" : "&>(input.value));\n");
      } else if (to_storage) {
        write_managed_text(output, "::std::move(static_cast<", type, "&>(input));\n");
      } else { write_managed_text(output, "static_cast<const ", type, "&>(input.value);\n"); }
    }
    for (const auto& field : item.source->member_list) {
      const auto name = field_name(field.name);
      const auto from = std::string{to_storage ? "input." : "input.value."} + name;
      const auto to = std::string{to_storage ? "result.value." : "result."} + name;
      if (!field.managed) {
        write_managed_text(output, "    ", to, " = ",
                           to_storage ? "::std::move(" + from + ")" : from, ";\n");
        continue;
      }
      if (wrapped_type(field)) {
        write_managed_text(output, "    ", to,
            " = detail::convert_owned_value<::std::remove_cvref_t<decltype(", to, ")>>(",
            to_storage ? "::std::move(" + from + ")" : from, ");\n");
        continue;
      }
      const auto traits = "::rohit::managed::model_traits<" +
                          full_type_name(child_node(field.type_name_list.front())) + ">::";
      const auto function = traits + (to_storage ? "make_storage" : "clone_value");
      if (field.owning_variant) {
        write_managed_text(output, "    if (", from,
            ".valueless_by_exception()) { throw ::std::invalid_argument{\"Cannot convert a valueless managed variant\"}; }\n");
        for (std::size_t index{}; index < field.type_name_list.size(); ++index) {
          const auto child_traits = "::rohit::managed::model_traits<" +
              full_type_name(field.type_name_list[index].resolved_node) + ">::";
          write_managed_text(output, "    if (", from, ".index() == ", index, ") { ", to,
              ".template emplace<", index, ">(", child_traits,
              to_storage ? "make_storage(::std::move(" : "clone_value(",
              "::std::get<", index, ">(", from, ")", to_storage ? "))" : ")", "); }\n");
        }
      } else if (field.modifier == member::modifier_type::none) {
        write_managed_text(output, "    ", to, " = ", function, "(",
                           to_storage ? "::std::move(" + from + ")" : from, ");\n");
      } else if (field.fixed_extent != 0) {
        write_managed_text(output, "    for (::std::size_t index = 0; index < ", to,
            ".size(); ++index) { ", to, "[index] = ", function, "(",
            to_storage ? "::std::move(" + from + "[index])" : from + "[index]", "); }\n");
      } else {
        write_managed_text(
            output, "    for (", to_storage ? "auto&" : "const auto&",
            field.modifier == member::modifier_type::map ? " [key, child]" : " child", " : ", from,
            ") {\n      ", to,
            field.modifier == member::modifier_type::map ? ".emplace(key, " : ".push_back(",
            function, "(", to_storage ? "::std::move(child)" : "child", "));\n    }\n");
      }
    }
    write_managed_text(output, "    return result;\n  }\n");
  }

  // Describe owned values and child identity edges without copying or serializing child subtrees.
  void write_collaboration_traversal(auto& output, const managed_class& item) {
    write_managed_text(output,
        "  // Traverse ownership and entity-local fields for collaboration conflict/lock checks.\n"
        "  static void visit_collaboration(const storage_type& storage, id_type parent, auto& visitor) {\n"
        "    visitor.begin_entity(storage.persistent_id, parent);\n");
    for (const auto& base : item.source->parents) {
      const auto type = full_type_name(base.parent_class);
      if (base.managed) {
        write_managed_text(output, "    visitor.value(static_cast<const ", base_storage_type(base),
            "&>(", separate_values_ ? "storage.value" : "storage", ").persistent_id);\n");
      } else {
        write_managed_text(output, "    visitor.value(static_cast<const ", type, "&>(",
            separate_values_ ? "storage.value" : "storage", "));\n");
      }
    }
    for (const auto& field : item.source->member_list) {
      if (field.transient) { continue; }
      const auto value = std::string{separate_values_ ? "storage.value." : "storage."} +
                         field_name(field.name);
      write_managed_text(output, "    if (", active_field(item.source, field,
          separate_values_ ? "storage.value" : "storage"), ") {\n");
      if (!field.managed) {
        write_managed_text(output, "    visitor.value(", value, ");\n");
      } else if (wrapped_type(field)) {
        write_managed_text(output,
            "    visitor.value(detail::owned_membership(", value, "));\n");
      } else if (field.owning_variant) {
        write_managed_text(output, "    visitor.value(static_cast<::std::uint64_t>(", value,
            ".index())); ::std::visit([&](const auto& child) { visitor.value(child.persistent_id); }, ",
            value, ");\n");
      } else if (field.modifier == member::modifier_type::none) {
        write_managed_text(output, "    visitor.value(", value, ".persistent_id);\n");
      } else {
        write_managed_text(output, "    visitor.value(static_cast<::std::uint64_t>(", value,
                           ".size()));\n    for (const auto& child : ", value, ") {\n");
        if (field.modifier == member::modifier_type::map) {
          write_managed_text(output, "      visitor.value(child.first);\n");
        }
        write_managed_text(output, "      visitor.value(child",
                           field.modifier == member::modifier_type::map ? ".second" : "",
                           ".persistent_id);\n    }\n");
      }
      write_managed_text(output, "    }\n");
    }
    write_managed_text(output, "    visitor.end_entity();\n");
    for (const auto& base : item.source->parents) {
      if (!base.managed) { continue; }
      const auto type = full_type_name(base.parent_class);
      write_managed_text(output, "    visitor.edge(", base.id, "u, 0u); model_traits<", type,
          ">::visit_collaboration(static_cast<const ", base_storage_type(base),
          "&>(", separate_values_ ? "storage.value" : "storage", "), storage.persistent_id, visitor);\n");
    }
    for (const auto& field : item.source->member_list) {
      if (field.transient) { continue; }
      if (!field.managed) {
        continue;
      }
      const auto traits = "model_traits<" +
                          full_type_name(child_node(field.type_name_list.front())) + ">";
      const auto value = std::string{separate_values_ ? "storage.value." : "storage."} +
                         field_name(field.name);
      write_managed_text(output, "    if (", active_field(item.source, field,
          separate_values_ ? "storage.value" : "storage"), ") {\n");
      if (wrapped_type(field)) {
        write_managed_text(output,
            "    { ::std::uint64_t index = 0; detail::visit_owned_children(", value,
            ", [&](const auto& child) { visitor.edge(", field.id, "u, index++); ", traits,
            "::visit_collaboration(child, storage.persistent_id, visitor); }); }\n");
      } else if (field.owning_variant) {
        write_managed_text(output, "    visitor.edge(", field.id, "u, ", value, ".index());\n"
            "    ::std::visit([&](const auto& child) { using child_type = ::std::remove_cvref_t<decltype(child)>; "
            "model_traits<child_type>::visit_collaboration(child, storage.persistent_id, visitor); }, ",
            value, ");\n");
      } else if (field.modifier == member::modifier_type::none) {
        write_managed_text(output, "    visitor.edge(", field.id, "u, 0u);\n    ", traits,
                           "::visit_collaboration(", value,
                           ", storage.persistent_id, visitor);\n");
      } else {
        if (field.modifier != member::modifier_type::map) {
          write_managed_text(output, "    { ::std::uint64_t index = 0;\n");
        }
        write_managed_text(output, "    for (const auto& child : ", value, ") {\n      visitor.edge(",
                           field.id, "u, ",
                           field.modifier == member::modifier_type::map ? "child.first" : "index++",
                           ");\n      ", traits,
                           "::visit_collaboration(child",
                           field.modifier == member::modifier_type::map ? ".second" : "",
                           ", storage.persistent_id, visitor);\n    }\n");
        if (field.modifier != member::modifier_type::map) {
          write_managed_text(output, "    }\n");
        }
      }
      write_managed_text(output, "    }\n");
    }
    write_managed_text(output, "  }\n");
  }

  // Collect inherited ordinary fields with nested base keys and dormant child identities.
  void write_base_fields(auto& output, const class_node* source, std::string object,
                         std::string expected, std::string desired, bool merge) {
    for (const auto& base : source->parents) {
      const auto type = full_type_name(base.parent_class);
      const auto value = "static_cast<" + std::string{merge ? "" : "const "} + base_storage_type(base) + "&>(" + object + ")";
      if (base.managed && source->supports_managed()) {
        write_managed_text(output, "    ", merge ? "merger" : "visitor", ".template owned<model_traits<",
            type, ">>(storage.persistent_id, ", base.id, "u, ", value);
        if (merge) {
          write_managed_text(output, ", static_cast<const ", base_storage_type(base), "&>(", expected,
              "), static_cast<const ", base_storage_type(base), "&>(", desired, ")");
        }
        write_managed_text(output, ");\n");
      } else {
        write_managed_text(output, "    ", merge ? "merger" : "visitor", ".with_path(", base.id,
            "u, [&](auto& ", merge ? "merger" : "visitor", ") {\n");
        write_ordinary_base_fields(output, base.parent_class, value,
            "static_cast<const " + type + "&>(" + expected + ")",
            "static_cast<const " + type + "&>(" + desired + ")", merge);
        write_managed_text(output, "    });\n");
      }
    }
  }

  // An ordinary base occurrence contributes fields to its nearest managed owner, not another ID.
  void write_ordinary_base_fields(auto& output, const class_node* source, std::string object,
                                 std::string expected, std::string desired, bool merge) {
    for (const auto& base : source->parents) {
      const auto type = full_type_name(base.parent_class);
      write_managed_text(output, "    ", merge ? "merger" : "visitor", ".with_path(", base.id,
          "u, [&](auto& ", merge ? "merger" : "visitor", ") {\n");
      write_ordinary_base_fields(output, base.parent_class,
          "static_cast<" + std::string{merge ? "" : "const "} + type + "&>(" + object + ")",
          "static_cast<const " + type + "&>(" + expected + ")",
          "static_cast<const " + type + "&>(" + desired + ")", merge);
      write_managed_text(output, "    });\n");
    }
    for (const auto& field : source->member_list) {
      if (field.transient) { continue; }
      const auto name = field_name(field.name);
      write_managed_text(output, "    if (", active_field(source, field, "(" + object + ")"), ") { ",
          merge ? "merger" : "visitor", ".value(storage.persistent_id, ", field.id, "u, (", object, ").", name);
      if (merge) { write_managed_text(output, ", (", expected, ").", name, ", (", desired, ").", name); }
      write_managed_text(output, "); }\n");
    }
  }

  // Expose stable field addresses and typed three-way reversal without exposing editor aliases.
  void write_collaboration_history(auto& output, const managed_class& item) {
    write_managed_text(
        output,
        "  // Collect entity/field addresses and ownership membership for selective history.\n"
        "  static void visit_collaboration_fields(const storage_type& storage, auto& visitor) {\n"
        "    static_cast<void>(storage); static_cast<void>(visitor);\n");
    write_base_fields(output, item.source, separate_values_ ? "storage.value" : "storage", "", "", false);
    for (const auto& field : item.source->member_list) {
      if (field.transient) { continue; }
      const auto value =
          std::string{separate_values_ ? "storage.value." : "storage."} + field_name(field.name);
      const auto method =
          field.managed && field.owning_variant ? "owned_variant" :
          field.managed ? "template owned<model_traits<" +
                              full_type_name(child_node(field.type_name_list.front())) + ">>"
                        : "value";
      write_managed_text(output, "    if (", active_field(item.source, field,
          separate_values_ ? "storage.value" : "storage"), ") { visitor.", method,
          "(storage.persistent_id, ", field.id, "u, ", value, "); }\n");
    }
    write_managed_text(
        output,
        "  }\n"
        "  // Reverse only changed fields, preserving unrelated current values.\n"
        "  static void merge_collaboration(storage_type& current, const storage_type& expected,\n"
        "                                  const storage_type& desired, auto& merger) {\n"
        "    static_cast<void>(current); static_cast<void>(expected);\n"
        "    static_cast<void>(desired); static_cast<void>(merger);\n");
    if (!item.source->parents.empty()) {
      write_managed_text(output, "    const auto& storage = current; static_cast<void>(storage);\n");
      write_base_fields(output, item.source, separate_values_ ? "current.value" : "current",
          separate_values_ ? "expected.value" : "expected", separate_values_ ? "desired.value" : "desired", true);
    }
    for (const auto& field : item.source->member_list) {
      if (field.transient) { continue; }
      const auto member = std::string{separate_values_ ? ".value." : "."} + field_name(field.name);
      const auto method =
          field.managed && field.owning_variant ? "owned_variant" :
          field.managed ? "template owned<model_traits<" +
                              full_type_name(child_node(field.type_name_list.front())) + ">>"
                        : "value";
      write_managed_text(output, "    if (", active_field(item.source, field,
          separate_values_ ? "current.value" : "current"), ") { merger.", method,
          "(current.persistent_id, ", field.id, "u, current", member,
          ", expected", member, ", desired", member, "); }\n");
    }
    write_managed_text(output, "  }\n");
  }

  // Generate all companions and traits; no handwritten adapters are required by consumers.
  void write_managed(auto& output) {
    for (const auto& item : managed_classes) {
      if (!separate_values_) {
        continue;
      }
      full_stream_auto_alloc unit_output{};
      open_managed_namespace(unit_output, item.source);
      write_owning_class(unit_output, item.data.get());
      write_owning_class(unit_output, item.storage.get());
      close_managed_namespace(unit_output, item.source);
      writer_.write_reusable_unit(output, "managed-storage:" + full_type_name(item.source),
          {reinterpret_cast<const char*>(unit_output.begin()), unit_output.current_offset()});
    }
    for (const auto* source : editor_classes) {
      full_stream_auto_alloc unit_output{};
      write_editor(unit_output, source);
      write_runtime_traits(unit_output, source);
      if (separate_values_ && source->supports_managed()) {
        const auto& item = managed_for(source);
        write_runtime_traits(unit_output, item.data.get());
        write_runtime_traits(unit_output, item.storage.get());
      }
      writer_.write_reusable_unit(output, "managed-editor:" + full_type_name(source),
          {reinterpret_cast<const char*>(unit_output.begin()), unit_output.current_offset()});
    }
    for (const auto& item : managed_classes) {
      full_stream_auto_alloc unit_output{};
      const auto hash = hash_managed_schema(item.source);
      std::set<const class_node*> path_visited;
      const auto inherited_paths = has_inherited_field_paths(item.source, path_visited);
      write_managed_text(unit_output,
                         "namespace rohit::managed {\n// Generated managed binding for this "
                         "schema type.\n"
                         "template <> struct model_traits<",
                         full_type_name(item.source),
                         "> {\n"
                         "  using storage_type = ",
                         full_type_name(separate_values_ ? item.storage.get() : item.source),
                         ";\n"
                         "  using id_type = ",
                         managed_id_type == "uint32" ? "::std::uint32_t" : "::std::uint64_t",
                         ";\n"
                         "  static constexpr ::std::string_view schema_id = \"",
                         separate_values_ ? "managed.v1:" : "managed.direct.v1:",
                         item.source->get_full_name(), ":", hash, ":", managed_id_type, "\";\n");
      write_managed_text(unit_output, "  // Stable concrete schema key for typed registry catalogs.\n"
          "  static constexpr ::std::string_view type_key = \"", item.source->get_full_name(), "\";\n");
      write_managed_text(unit_output,
          "  // Select an explicitly negotiated nested-path collaboration wire contract when required.\n"
          "  static constexpr bool has_inherited_field_paths = ", inherited_paths ? "true" : "false", ";\n");
      write_managed_text(
          unit_output,
          "  // Select application fields for the editor without exposing identity setters.\n"
          "  static auto& payload(storage_type& input) { return ",
          separate_values_ ? "input.value" : "input",
          "; }\n"
          "  // Read the same payload through an immutable storage view.\n"
          "  static const auto& payload(const storage_type& input) { return ",
          separate_values_ ? "input.value" : "input", "; }\n");
      write_conversion(unit_output, item, true);
      write_conversion(unit_output, item, false);
      write_managed_text(unit_output,
          "  // Reset only memory fields after accepted durable restoration.\n"
          "  static void reset_runtime_fields(storage_type& value) { runtime_traits<storage_type>::reset(value); }\n"
          "  // Carry memory fields across an isolated runtime-only update.\n"
          "  static void copy_runtime_fields(storage_type& value, const storage_type& source) { runtime_traits<storage_type>::copy(value, source); }\n"
          "  // Include durable and memory-only owned state in resident-byte estimates.\n"
          "  static ::std::size_t estimate_memory(const storage_type& value) { return runtime_traits<storage_type>::estimate_memory(value); }\n");
      write_managed_text(
          unit_output,
          "  // Visit only independently identified occurrences; ordinary children remain values.\n"
          "  static void visit_entities(auto& storage, auto&& visitor) {\n"
          "    if constexpr (requires { visitor.template entity<", full_type_name(item.source),
          ">(storage); }) { visitor.template entity<", full_type_name(item.source),
          ">(storage); } else { visitor(storage.persistent_id, \"",
          item.source->get_full_name(), "\"); }\n");
      for (const auto& base : item.source->parents) {
        if (base.managed) {
          write_managed_text(unit_output, "    model_traits<", full_type_name(base.parent_class),
              ">::visit_entities(static_cast<::std::conditional_t<::std::is_const_v<::std::remove_reference_t<decltype(storage)>>, const ",
              base_storage_type(base), ", ", base_storage_type(base),
              ">&>(", separate_values_ ? "storage.value" : "storage", "), visitor);\n");
        }
      }
      for (const auto& field : item.source->member_list) {
        if (field.transient) { continue; }
        if (!field.managed) {
          continue;
        }
        write_managed_text(unit_output, "    if (", active_field(item.source, field,
            separate_values_ ? "storage.value" : "storage"), ") {\n");
        const auto traits =
            "model_traits<" + full_type_name(child_node(field.type_name_list.front())) + ">";
        const auto value =
            std::string{separate_values_ ? "storage.value." : "storage."} + field_name(field.name);
        if (wrapped_type(field)) {
          write_managed_text(unit_output, "    detail::visit_owned_children(", value,
              ", [&](auto& child) { ", traits, "::visit_entities(child, visitor); });\n");
        } else if (field.owning_variant) {
          write_managed_text(unit_output,
              "    ::std::visit([&](auto& child) { using child_type = ::std::remove_cvref_t<decltype(child)>; "
              "model_traits<child_type>::visit_entities(child, visitor); }, ", value, ");\n");
        } else if (field.modifier == member::modifier_type::none) {
          write_managed_text(unit_output, "    ", traits, "::visit_entities(", value, ", visitor);\n");
        } else {
          write_managed_text(
              unit_output, "    for (auto& child : ", value, ") { ", traits, "::visit_entities(child",
              field.modifier == member::modifier_type::map ? ".second" : "", ", visitor); }\n");
        }
        write_managed_text(unit_output, "    }\n");
      }
      write_managed_text(unit_output, "  }\n");
      write_collaboration_traversal(unit_output, item);
      write_collaboration_history(unit_output, item);
      write_managed_text(
          unit_output,
          "  // Structural decoding and store identity checks precede application "
          "validation.\n"
          "  static void validate(const storage_type&) {}\n"
          "  // Bind the generated editor without exposing writable identity metadata.\n"
          "  static auto make_editor(auto access) { return ",
          editor_name(item.source),
          "<decltype(access)>{::std::move(access)}; }\n"
          "};\n}\n");
      if (separate_values_) {
        write_managed_text(unit_output,
                           "namespace rohit::managed {\n"
                           "// Let editor access resolve this explicit storage wrapper's payload.\n"
                           "template <> struct model_traits<",
                           full_type_name(item.storage.get()), "> : model_traits<",
                           full_type_name(item.source), "> {};\n}\n");
      }
      writer_.write_reusable_unit(output, "managed-traits:" + full_type_name(item.source),
          {reinterpret_cast<const char*>(unit_output.begin()), unit_output.current_offset()});
    }
    for (const auto* source : editor_classes) {
      write_registry_fields(output, source);
      write_registry_fields(output, source, true);
    }
  }

  // Emit one reusable exact-type descriptor set; no table is stored inside individual objects.
  void write_registry_fields(auto& output, const class_node* source, bool ordinary = false) {
    full_stream_auto_alloc unit_output{};
    const auto plain = full_type_name(source);
    const auto wrapped = !ordinary && separate_values_ && source->supports_managed();
    const auto storage = wrapped
        ? full_type_name(managed_for(source).storage.get()) : plain;
    const auto prefix = wrapped ? "input.value." : "input.";
    write_managed_text(unit_output, "namespace rohit::managed {\n"
        "// Generated stable field descriptors for this owning schema type.\n"
        "template <> struct ", ordinary ? "value_fields" : "fields", "<", plain, "> {\n");
    std::vector<std::string> names;
    for (const auto& base : source->parents) {
      if (base.managed) { continue; }
      const auto accessor = function_name("base_" + base_accessor_name(base));
      names.push_back(accessor);
      const auto type = full_type_name(base.parent_class);
      write_managed_text(unit_output, "  // Traverse this ordinary base by its stable schema ID.\n"
          "  static constexpr auto ", accessor, "() {\n    using storage_type = ", storage,
          ";\n    using value_type = ", type, ";\n    return member_descriptor<", plain,
          ", storage_type, value_type>{{", base.id, "u, \"", base.display_name, "\", false},\n"
          "      +[](const storage_type& input) -> const value_type& { return static_cast<const value_type&>(",
          wrapped ? "input.value" : "input", "); }, nullptr,\n"
          "      +[](storage_type& input) -> value_type& { return static_cast<value_type&>(",
          wrapped ? "input.value" : "input", "); }};\n  }\n");
    }
    for (const auto& field : source->member_list) {
      if (field.managed) { continue; }
      const auto accessor = function_name("field_" + field.name);
      names.push_back(accessor);
      const auto member = field_name(field.name);
      const auto writable = !field.version && !field.transient;
      write_managed_text(unit_output, "  // Select this stable schema field without an ABI offset.\n"
          "  static constexpr auto ", accessor, "() {\n    using storage_type = ", storage,
          ";\n    using value_type = ::std::remove_cvref_t<decltype(::std::declval<storage_type&>().",
          wrapped ? "value." : "", member, ")>;\n"
          "    return member_descriptor<", plain, ", storage_type, value_type>{{", field.id,
          "u, \"", field.display_name, "\", ", writable ? "true" : "false", "},\n"
          "      +[](const storage_type& input) -> const value_type& { ",
          lifetime_guard(source, field, wrapped ? "input.value" : "input"),
          "return ", prefix, member, "; },\n");
      if (writable) {
        // Version-lifetime checks remain in typed descriptors just as in generated editors.
        write_managed_text(unit_output, "      +[](storage_type& input, value_type value) { ",
            lifetime_guard(source, field, wrapped ? "input.value" : "input"),
            prefix, member, " = ::std::move(value); },\n"
            "      +[](storage_type& input) -> value_type& { ", lifetime_guard(source, field, wrapped ? "input.value" : "input"),
            "return ", prefix, member, "; }};\n  }\n");
      } else {
        write_managed_text(unit_output, "      nullptr};\n  }\n");
      }
    }
    write_managed_text(unit_output, "  // Dispatch exact typed descriptors through one per-type table.\n"
        "  static void visit(auto&& visitor) {\n    static_cast<void>(visitor);\n");
    for (const auto& name : names) { write_managed_text(unit_output, "    visitor(", name, "());\n"); }
    write_managed_text(unit_output, "  }\n};\n}\n");
    writer_.write_reusable_unit(output, std::string{ordinary ? "managed-value-fields:" : "managed-fields:"} + plain,
        {reinterpret_cast<const char*>(unit_output.begin()), unit_output.current_offset()});
  }

public:
  // Borrow the per-output writer; neither the parsed schema nor its naming policy is modified.
  managed_writer(Writer& writer, std::string id_type, bool separate_values)
      : writer_{writer}, managed_id_type{std::move(id_type)}, separate_values_{separate_values} {}
  // Grant only generated checked helpers access to private schema-owned state.
  void write_access_friends(auto& output, const class_node* source) {
    const class_node* original = source;
    for (const auto& item : managed_classes) {
      if (item.data.get() == source) { original = item.source; break; }
    }
    if (std::find(editor_classes.begin(), editor_classes.end(), original) == editor_classes.end()) {
      return;
    }
    write_managed_text(output, "  template<class> friend class ",
                       type_name(original->name + "_editor"), ";\n"
                       "  template<class> friend struct ::rohit::managed::model_traits;\n"
                       "  template<class> friend struct ::rohit::managed::fields;\n"
                       "  template<class> friend struct ::rohit::managed::value_fields;\n"
                       "  template<class> friend struct ::rohit::managed::runtime_traits;\n");
  }
  // Emit identity directly on the schema class unless separate value classes were requested.
  bool write_primary(auto& output, const class_node* source) {
    if (separate_values_ || !source->supports_managed()) {
      return false;
    }
    write_owning_class(output, managed_for(source).data.get());
    return true;
  }
  // Validate and lower all managed declarations before any output is published.
  void prepare(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    prepare_managed(statements);
  }
  // Keep managed runtime includes out of outputs that contain no managed declarations.
  bool empty() const {
    return managed_classes.empty();
  }
  // Emit companions after their ordinary payload declarations.
  void write(auto& output) {
    write_managed(output);
  }
};
} // namespace rohit::serializer::writer::cpp
