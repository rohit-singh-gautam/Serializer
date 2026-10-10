#pragma once

#include <rohit/serializer_creator.hpp>
#include "managed_schema.hpp"

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
    if (!source->parents.empty() ||
        source->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (source->attributes & class_attributes::packed) == class_attributes::packed) {
      throw std::invalid_argument{
          "Managed C++ editors require unpacked owning classes without inheritance: " +
          source->get_full_name()};
    }
    editor_classes.push_back(source);
    visiting_editors.push_back(source);
    for (const auto& field : source->member_list) {
      if (field.access != access_type::public_access ||
          field.modifier == member::modifier_type::variant) {
        throw std::invalid_argument{"Managed C++ editors require public fields without union or variant: " +
                                    source->get_full_name()};
      }
      const auto* node = field.type_name_list.front().resolved_node;
      if (node && node->type == object_type::class_type) {
        collect_editor(static_cast<const class_node*>(node));
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
            class_attributes::stable_ids, std::vector<parent>{});
        item.data->member_list = source->member_list;
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
        managed_classes.push_back(std::move(item));
        continue;
      }
      item.data = std::make_unique<class_node>(
          object_type::class_type, "managed_" + source->name + "_data", source->parent_namespace,
          class_attributes::stable_ids, std::vector<parent>{});
      item.data->member_list = source->member_list;
      for (auto& field : item.data->member_list) {
        if (field.managed) {
          field.default_value.clear();
          const auto& child = managed_for(field.type_name_list.front().resolved_node);
          field.type_name_list.front().resolved_node = child.storage.get();
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
      for (const auto& field : source->member_list) {
        for (const auto& prefix : {"get_", "set_"}) {
          if (!methods.insert(function_name(std::string{prefix} + field.name)).second) {
            throw std::invalid_argument{"Managed editor method name collision"};
          }
        }
        const auto* node = field.type_name_list.front().resolved_node;
        if (field.managed || (node && node->type == object_type::class_type &&
                              field.modifier == member::modifier_type::none)) {
          if (!methods.insert(function_name(field.name)).second) {
            throw std::invalid_argument{"Managed editor method name collision"};
          }
        }
      }
    }
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
    for (const auto& field : source->member_list) {
      const auto member = field_name(field.name);
      const auto* child = field.type_name_list.front().resolved_node;
      const auto child_class = child && child->type == object_type::class_type
                                   ? static_cast<const class_node*>(child)
                                   : nullptr;
      // Ordinary getters return copies; managed children are addressed through editors, never raw storage.
      const auto constraint = field.managed ? " requires (!Access::is_managed)" : "";
      write_managed_text(
          output, "  // Copy this owned value without exposing mutable snapshot storage.\n  auto ",
          function_name("get_" + field.name), "() const", constraint,
          " { ::std::optional<decltype(payload_type::", member,
          ")> result;\n"
          "    access_.update([&](const auto& value) { result.emplace(value.",
          member,
          "); }); return ::std::move(*result); }\n"
          "  // Assign this value inside the current transaction.\n  void ",
          function_name("set_" + field.name), "(decltype(payload_type::", member, ") value) const",
          constraint, " { access_.update([&](auto& target) { target.", member,
          " = ::std::move(value); }); }\n");
      if (!child_class) {
        continue;
      }
      if (field.managed) {
        write_managed_text(
            output, "  // Edit identified children only when this occurrence is managed.\n  auto ",
            function_name(field.name), "() const");
        if (field.modifier != member::modifier_type::none) {
          write_managed_text(output, " requires Access::is_managed");
        }
        write_managed_text(output,
                           " {\n    if constexpr (Access::is_managed) {\n      auto child = "
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
            "() const { auto child = access_.template member<false>(&payload_type::", member,
            "); return ", editor_name(child_class), "<decltype(child)>{::std::move(child)}; }\n");
      }
    }
    write_managed_text(output, "};\n");
    close_managed_namespace(output, source);
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
    for (const auto& field : item.source->member_list) {
      const auto name = field_name(field.name);
      const auto from = std::string{to_storage ? "input." : "input.value."} + name;
      const auto to = std::string{to_storage ? "result.value." : "result."} + name;
      if (!field.managed) {
        write_managed_text(output, "    ", to, " = ",
                           to_storage ? "::std::move(" + from + ")" : from, ";\n");
        continue;
      }
      const auto traits = "::rohit::managed::model_traits<" +
                          full_type_name(field.type_name_list.front().resolved_node) + ">::";
      const auto function = traits + (to_storage ? "make_storage" : "clone_value");
      if (field.modifier == member::modifier_type::none) {
        write_managed_text(output, "    ", to, " = ", function, "(",
                           to_storage ? "::std::move(" + from + ")" : from, ");\n");
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
    for (const auto& field : item.source->member_list) {
      const auto value = std::string{separate_values_ ? "storage.value." : "storage."} +
                         field_name(field.name);
      if (!field.managed) {
        write_managed_text(output, "    visitor.value(", value, ");\n");
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
    }
    write_managed_text(output, "    visitor.end_entity();\n");
    for (const auto& field : item.source->member_list) {
      if (!field.managed) {
        continue;
      }
      const auto traits = "model_traits<" +
                          full_type_name(field.type_name_list.front().resolved_node) + ">";
      const auto value = std::string{separate_values_ ? "storage.value." : "storage."} +
                         field_name(field.name);
      if (field.modifier == member::modifier_type::none) {
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
    }
    write_managed_text(output, "  }\n");
  }

  // Expose stable field addresses and typed three-way reversal without exposing editor aliases.
  void write_collaboration_history(auto& output, const managed_class& item) {
    write_managed_text(
        output,
        "  // Collect entity/field addresses and ownership membership for selective history.\n"
        "  static void visit_collaboration_fields(const storage_type& storage, auto& visitor) {\n"
        "    static_cast<void>(storage); static_cast<void>(visitor);\n");
    for (const auto& field : item.source->member_list) {
      const auto value =
          std::string{separate_values_ ? "storage.value." : "storage."} + field_name(field.name);
      const auto method =
          field.managed ? "template owned<model_traits<" +
                              full_type_name(field.type_name_list.front().resolved_node) + ">>"
                        : "value";
      write_managed_text(output, "    visitor.", method, "(storage.persistent_id, ", field.id,
                         "u, ", value, ");\n");
    }
    write_managed_text(
        output,
        "  }\n"
        "  // Reverse only changed fields, preserving unrelated current values.\n"
        "  static void merge_collaboration(storage_type& current, const storage_type& expected,\n"
        "                                  const storage_type& desired, auto& merger) {\n"
        "    static_cast<void>(current); static_cast<void>(expected);\n"
        "    static_cast<void>(desired); static_cast<void>(merger);\n");
    for (const auto& field : item.source->member_list) {
      const auto member = std::string{separate_values_ ? ".value." : "."} + field_name(field.name);
      const auto method =
          field.managed ? "template owned<model_traits<" +
                              full_type_name(field.type_name_list.front().resolved_node) + ">>"
                        : "value";
      write_managed_text(output, "    merger.", method, "(current.persistent_id, ", field.id,
                         "u, current", member, ", expected", member, ", desired", member, ");\n");
    }
    write_managed_text(output, "  }\n");
  }

  // Generate all companions and traits; no handwritten adapters are required by consumers.
  void write_managed(auto& output) {
    for (const auto& item : managed_classes) {
      if (!separate_values_) {
        continue;
      }
      open_managed_namespace(output, item.source);
      write_owning_class(output, item.data.get());
      write_owning_class(output, item.storage.get());
      close_managed_namespace(output, item.source);
    }
    for (const auto* source : editor_classes) {
      write_editor(output, source);
    }
    for (const auto& item : managed_classes) {
      const auto hash = hash_managed_schema(item.source);
      write_managed_text(output,
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
      write_managed_text(
          output,
          "  // Select application fields for the editor without exposing identity setters.\n"
          "  static auto& payload(storage_type& input) { return ",
          separate_values_ ? "input.value" : "input",
          "; }\n"
          "  // Read the same payload through an immutable storage view.\n"
          "  static const auto& payload(const storage_type& input) { return ",
          separate_values_ ? "input.value" : "input", "; }\n");
      write_conversion(output, item, true);
      write_conversion(output, item, false);
      write_managed_text(
          output,
          "  // Visit only independently identified occurrences; ordinary children remain values.\n"
          "  static void visit_entities(auto& storage, auto&& visitor) {\n"
          "    visitor(storage.persistent_id, \"",
          item.source->get_full_name(), "\");\n");
      for (const auto& field : item.source->member_list) {
        if (!field.managed) {
          continue;
        }
        const auto traits =
            "model_traits<" + full_type_name(field.type_name_list.front().resolved_node) + ">";
        const auto value =
            std::string{separate_values_ ? "storage.value." : "storage."} + field_name(field.name);
        if (field.modifier == member::modifier_type::none) {
          write_managed_text(output, "    ", traits, "::visit_entities(", value, ", visitor);\n");
        } else {
          write_managed_text(
              output, "    for (auto& child : ", value, ") { ", traits, "::visit_entities(child",
              field.modifier == member::modifier_type::map ? ".second" : "", ", visitor); }\n");
        }
      }
      write_managed_text(output, "  }\n");
      write_collaboration_traversal(output, item);
      write_collaboration_history(output, item);
      write_managed_text(
          output,
          "  // Structural decoding and store identity checks precede application "
          "validation.\n"
          "  static void validate(const storage_type&) {}\n"
          "  // Bind the generated editor without exposing writable identity metadata.\n"
          "  static auto make_editor(auto access) { return ",
          editor_name(item.source),
          "<decltype(access)>{::std::move(access)}; }\n"
          "};\n}\n");
      if (separate_values_) {
        write_managed_text(output,
                           "namespace rohit::managed {\n"
                           "// Let editor access resolve this explicit storage wrapper's payload.\n"
                           "template <> struct model_traits<",
                           full_type_name(item.storage.get()), "> : model_traits<",
                           full_type_name(item.source), "> {};\n}\n");
      }
    }
  }

public:
  // Borrow the per-output writer; neither the parsed schema nor its naming policy is modified.
  managed_writer(Writer& writer, std::string id_type, bool separate_values)
      : writer_{writer}, managed_id_type{std::move(id_type)}, separate_values_{separate_values} {}
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
