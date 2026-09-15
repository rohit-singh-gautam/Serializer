//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include "cpp_naming.hpp"

#include <rohit/serializer_creator.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::cpp {
namespace {
// Emit one schema using an isolated naming policy; the parsed schema is never rewritten.
class emitter : private naming {
public:
  // Bind the per-output C++ naming policy.
  explicit emitter(const cpp_options& options) : naming{options} {}

  // Return the public enumerator spelling used in generated mode selections.
  std::string storage_mode_name(storage_mode mode) {
    switch (mode) {
    case storage_mode::owning:
      return "::rohit::serializer::storage_mode::owning";
    case storage_mode::read_only_view:
      return "::rohit::serializer::storage_mode::read_only_view";
    case storage_mode::mutable_view:
      return "::rohit::serializer::storage_mode::mutable_view";
    }
    throw std::invalid_argument{"Unknown generated storage mode"};
  }

  // Select a nested class's concrete declaration or enabled template specialization.
  std::string storage_type_name(const std::string& name, const syntax_node* node,
                                storage_mode mode = storage_mode::owning) {
    if (!node) {
      const auto& primitive = serializer::get_cpp_type(name);
      return primitive.starts_with("std::") ? "::" + primitive : primitive;
    }
    auto result = full_type_name(node);
    if (node->type == object_type::class_type &&
        static_cast<const class_node*>(node)->multiple_modes()) {
      result += "<" + storage_mode_name(mode) + ">";
    }
    return result;
  }

  // Select the symbolic discriminator type independently of its storage field.
  std::string union_enum_name(const member& field) {
    return type_name("e_" + field.name);
  }

  // Select the owning union's internal type using the type naming convention.
  std::string union_storage_name(const member& field) {
    return type_name("u_" + field.name);
  }

  // Select the discriminator data member using the field naming convention.
  std::string union_tag_name(const member& field) {
    return field_name(field.name + "_type");
  }

  // Emit union storage and conversion functions while keeping wire alternative names unchanged.
  std::string get_cpp_type_support_union(const member& field) {
    const auto tag = union_enum_name(field);
    const auto storage = union_storage_name(field);
    std::string result{"  enum class " + tag + " {\n"};
    for (const auto& type : field.type_name_list) {
      result += "    " + enum_name(type.enum_name) + ",\n";
    }
    result += "  };\n  union " + storage + " {\n";
    for (const auto& type : field.type_name_list) {
      result += "    " + storage_type_name(type.name, type.resolved_node) + " " +
                field_name(type.enum_name) + ";\n";
    }
    result += "  };\n  static_assert(::std::is_trivially_destructible_v<" + storage +
              ">, \"Raw union payloads must be trivially destructible\");\n"
              "  // Return the wire name for a valid union alternative.\n"
              "  static ::std::string to_string(" +
              tag +
              (std::string{" "} + local_name("value") + ") {\n    switch (" + local_name("value") +
               ") {\n");
    for (const auto& type : field.type_name_list) {
      result += "    case " + tag + "::" + enum_name(type.enum_name) + ":\n      return \"" +
                type.enum_name + "\";\n";
    }
    result +=
        "    default:\n      throw ::std::invalid_argument{\"Invalid union discriminator\"};\n"
        "    }\n  }\n"
        "  // Match the full wire name, including when names share a hash.\n"
        "  static " +
        tag + " " + function_name("to_e_" + field.name) +
        (std::string{"(const auto& "} + local_name("value") + ") {\n    const ::std::string_view " +
         local_name("name") + "{" + local_name("value") + "};\n    const auto " +
         local_name("hash") + " = ::rohit::serializer::detail::field_name_hash(" +
         local_name("name") + ");\n");
    for (const auto& type : field.type_name_list) {
      result += (std::string{"    if ("} + local_name("hash") +
                 " == ::rohit::serializer::detail::field_name_hash(\"") +
                type.enum_name + "\") && " + local_name("name") + " == \"" + type.enum_name +
                "\") {\n      return " + tag + "::" + enum_name(type.enum_name) + ";\n    }\n";
    }
    result += "    throw ::std::invalid_argument{\"Unknown union alternative\"};\n  }";
    return result;
  }

  // Emit C++ support for the parsed schema.
  std::string get_cpp_type_support(const member& member) {
    switch (member.modifier) {
    default:
    case member::modifier_type::none:
      return {};
    case member::modifier_type::array:
      return {};
    case member::modifier_type::map:
      return {};
    case member::modifier_type::variant:
      return get_cpp_type_support_union(member);
    }
  }

  // Return the mapped C++ type, preserving user-defined type names.
  std::string get_cpp_type(const member& member) {
    switch (member.modifier) {
    default:
    case member::modifier_type::none:
      // TODO: Range check
      return storage_type_name(member.type_name_list[0].name,
                               member.type_name_list[0].resolved_node);
    case member::modifier_type::array:
      return std::string("::std::vector<") +
             storage_type_name(member.type_name_list[0].name,
                               member.type_name_list[0].resolved_node) +
             ">";
    case member::modifier_type::map:
      return std::string("::std::map<") + storage_type_name(member.key, member.key_node) + "," +
             storage_type_name(member.type_name_list[0].name,
                               member.type_name_list[0].resolved_node) +
             ">";
    case member::modifier_type::variant:
      return union_enum_name(member) + " " + union_tag_name(member) + "{};\n  " +
             union_storage_name(member);
    }
  }

  // Emit C++ access type for the parsed schema.
  void write_access_type(stream& out_stream, const access_type access) {
    switch (access) {
    default:
    case access_type::public_access:
      out_stream.write("public");
      break;

    case access_type::protected_access:
      out_stream.write("protected");
      break;

    case access_type::private_access:
      out_stream.write("private");
      break;
    }
  }

  // Emit C++ parent list for the parsed schema.
  void write_parent_list(stream& out_stream, const std::vector<parent>& parents) {
    bool first{true};
    for (const auto& parent : parents) {
      if (first) {
        first = false;
      } else {
        out_stream.write(", ");
      }
      write_access_type(out_stream, parent.access);
      out_stream.write(' ', storage_type_name(parent.name, parent.parent_class));
    }
  }

  // Emit C++ member list for the parsed schema.
  void write_member_list(stream& out_stream, const std::vector<member>& members) {
    access_type last_access{access_type::private_access};

    for (const auto& member : members) {
      if (member.access != last_access) {
        out_stream.write('\n');
        write_access_type(out_stream, member.access);
        out_stream.write(":\n");
        last_access = member.access;
      }
      auto support = get_cpp_type_support(member);
      if (!support.empty()) {
        out_stream.write(support, '\n');
      }
      out_stream.write("  ", get_cpp_type(member), ' ', field_name(member.name), "{");
      if (!member.default_value.empty()) {
        out_stream.write(default_value(member));
      }
      out_stream.write("};\n");
    }
  }

  // Emit C++ serializer out body for parent for the parsed schema.
  void write_serializer_out_body_for_parent(stream& out_stream, const class_node* obj,
                                            const rohit::serializer::serialize_key_type key_type,
                                            bool& first) {
    for (const auto& parent : obj->parents) {
      if (first) {
        first = false;
        out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                          ".struct_serialize_out_start("));
      } else {
        out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                          ".struct_serialize_out("));
      }
      if (key_type == rohit::serializer::serialize_key_type::string) {
        out_stream.write("::std::make_pair(::std::string_view { \"", parent.display_name,
                         "\" }, static_cast<const ",
                         storage_type_name(parent.name, parent.parent_class),
                         "*>(this))"
                         ");");
      } else if (key_type == rohit::serializer::serialize_key_type::integer) {
        out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", parent.id,
                         "), static_cast<const ",
                         storage_type_name(parent.name, parent.parent_class),
                         "*>(this))"
                         ");");
      } else {
        out_stream.write("static_cast<const ", storage_type_name(parent.name, parent.parent_class),
                         "*>(this)"
                         ");");
      }
    }
  }

  // Emit C++ serializer out body non union for the parsed schema.
  void write_serializer_out_body_non_union(stream& out_stream, const member& member,
                                           const rohit::serializer::serialize_key_type key_type,
                                           bool& first) {
    if (first) {
      first = false;
      out_stream.write((std::string{"\n      "} + local_name("serializer_protocol") +
                        ".struct_serialize_out_start("));
    } else {
      out_stream.write(
          (std::string{"\n      "} + local_name("serializer_protocol") + ".struct_serialize_out("));
    }
    if (key_type == rohit::serializer::serialize_key_type::string) {
      if (member.modifier != member::modifier_type::none ||
          member.type_name_list[0].type != object_type::enum_type) {
        out_stream.write("::std::make_pair(::std::string_view { \"", member.display_name, "\" }, ",
                         "::std::cref(this->", field_name(member.name),
                         "))"
                         ");");
      } else {
        out_stream.write("::std::make_pair(::std::string_view { \"", member.display_name, "\" }, ",
                         "::rohit::serializer::detail::enum_name(this->", field_name(member.name),
                         "))"
                         ");");
      }
    } else if (key_type == rohit::serializer::serialize_key_type::integer) {
      out_stream.write("::std::make_pair(static_cast<::std::uint32_t>(", member.id,
                       "), ::std::cref(this->", field_name(member.name),
                       "))"
                       ");");
    } else {
      out_stream.write("this->", field_name(member.name), ");");
    }
  }

  // Emit only the active union payload, preserving original wire names and numeric alternatives.
  void write_serializer_out_body_union(stream& output, const member& field,
                                       serialize_key_type key_type, bool& first) {
    output.write("\n      switch (this->", union_tag_name(field), ") {\n");
    for (std::size_t index = 0; index < field.type_name_list.size(); ++index) {
      const auto& alternative = field.type_name_list[index];
      const auto payload =
          "this->" + field_name(field.name) + "." + field_name(alternative.enum_name);
      output.write("      case ", union_enum_name(field), "::", enum_name(alternative.enum_name),
                   ":\n",
                   first ? (std::string{"        "} + local_name("serializer_protocol") +
                            ".struct_serialize_out_start(")
                         : (std::string{"        "} + local_name("serializer_protocol") +
                            ".struct_serialize_out("));
      if (key_type == serialize_key_type::string) {
        output.write("::std::make_pair(::std::string_view{\"", field.display_name, ":",
                     alternative.enum_name, "\"}, ::std::cref(", payload, "))");
      } else if (key_type == serialize_key_type::integer) {
        output.write("::std::make_tuple(static_cast<::std::uint32_t>(", field.id,
                     "), static_cast<::std::uint32_t>(", index, "), ::std::cref(", payload, "))");
      } else {
        output.write("::std::make_pair(static_cast<::std::uint32_t>(", index, "), ::std::cref(",
                     payload, "))");
      }
      output.write(");\n        break;\n");
    }
    first = false;
    output.write(
        "      default:\n        throw ::std::invalid_argument{\"Invalid union discriminator\"};\n"
        "      }");
  }

  // Emit C++ serializer out body for the parsed schema.
  void write_serializer_out_body(stream& out_stream, const class_node* obj,
                                 const rohit::serializer::serialize_key_type key_type) {
    bool first = true;
    write_serializer_out_body_for_parent(out_stream, obj, key_type, first);
    for (const auto& member : obj->member_list) {
      if (member.modifier != member::modifier_type::variant) {
        write_serializer_out_body_non_union(out_stream, member, key_type, first);
      } else {
        write_serializer_out_body_union(out_stream, member, key_type, first);
      }
    }
    out_stream.write(first ? (std::string{"\n      "} + local_name("serializer_protocol") +
                              ".struct_serialize_out_empty();")
                           : (std::string{"\n      "} + local_name("serializer_protocol") +
                              ".struct_serialize_out_end();"));
  }

  // Emit direct field writes selected at compile time by the output protocol type.
  void write_serializer_out_body(stream& out_stream, const class_node* obj) {
    out_stream.write(
        (std::string{
             "  // Encode fields directly using the protocol's compile-time key mode.\n  template "
             "<typename SerializeOutProtocol>\n  void serialize_out(SerializeOutProtocol& "} +
         local_name("serializer_protocol") +
         ") const {\n    static_assert(\n        SerializeOutProtocol::key_type == "
         "::rohit::serializer::serialize_key_type::none ||\n        SerializeOutProtocol::key_type "
         "== ::rohit::serializer::serialize_key_type::integer ||\n        "
         "SerializeOutProtocol::key_type == ::rohit::serializer::serialize_key_type::string,\n     "
         "  "
         " \"Unsupported serializer key type\");"));
    out_stream.write("\n    if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::none) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::none);
    out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::integer) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::integer);
    out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::string) {");
    write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::string);
    out_stream.write("\n    }\n  }\n\n");
    out_stream.write(
        (std::string{"  // Construct the requested protocol and write this object.\n  template "
                     "<template <::rohit::serializer::serialize_type> class Protocol>\n  void "
                     "serialize_out(::rohit::stream& "} +
         local_name("stream") + ") const {\n    using " + type_name("serializer_out_protocol") +
         " = Protocol<::rohit::serializer::serialize_type::out>;\n    " +
         type_name("serializer_out_protocol") + " " + local_name("serializer_protocol") + "{" +
         local_name("stream") + "};\n    serialize_out(" + local_name("serializer_protocol") +
         ");\n  }\n\n"));
  }

  // Emit C++ serializer in body for parent key none for the parsed schema.
  void write_serializer_in_body_for_parent_key_none(stream& out_stream, const class_node* obj) {
    for (const auto& parent : obj->parents) {
      out_stream.write(
          "      static_cast<", storage_type_name(parent.name, parent.parent_class),
          (std::string{"*>(this)->serialize_in("} + local_name("serializer_protocol") + ");\n"));
    }
  }

  // Emit C++ serializer in body for parent key integer for the parsed schema.
  void write_serializer_in_body_for_parent_key_integer(stream& out_stream, const class_node* obj) {
    for (const auto& parent : obj->parents) {
      out_stream.write("      case ", parent.id,
                       ":\n"
                       "        static_cast<",
                       storage_type_name(parent.name, parent.parent_class),
                       (std::string{"*>(this)->serialize_in("} + local_name("serializer_protocol") +
                        ");\n        break;\n"));
    }
  } // write_serializer_in_body_for_parent_key_integer

  // Emit C++ serializer in body non union key integer for the parsed schema.
  void write_serializer_in_body_non_union_key_integer(stream& out_stream, const member& member) {
    out_stream.write("      case ", member.id,
                     (std::string{":\n        "} + local_name("serializer_protocol") +
                      ".template serialize_in<"),
                     get_cpp_type(member), ">(this->", field_name(member.name),
                     ");\n"
                     "        break;\n");
  } // write_serializer_in_body_non_union_key_integer

  // Emit C++ serializer in body non union key none for the parsed schema.
  void write_serializer_in_body_non_union_key_none(stream& out_stream, const member& member) {
    out_stream.write(
        (std::string{"      "} + local_name("serializer_protocol") + ".template serialize_in<"),
        get_cpp_type(member), ">(this->", field_name(member.name), ");\n");
  } // write_serializer_in_body_non_union_key_none

  // Decode one active union value through its correctly renamed discriminator and storage member.
  void write_union_input(stream& output, const member& field) {
    output.write("      this->", union_tag_name(field), " = static_cast<", union_enum_name(field),
                 (std::string{">("} + local_name("serializer_protocol") +
                  ".serialize_in_variable());\n      switch (this->"),
                 union_tag_name(field), ") {\n");
    for (const auto& alternative : field.type_name_list) {
      const auto payload =
          "this->" + field_name(field.name) + "." + field_name(alternative.enum_name);
      output.write(
          "      case ", union_enum_name(field), "::", enum_name(alternative.enum_name),
          ":\n"
          "        ::std::construct_at(&",
          payload,
          (std::string{");\n        "} + local_name("serializer_protocol") + ".serialize_in("),
          payload,
          ");\n"
          "        break;\n");
    }
    output.write(
        (std::string{
             "      default:\n        throw ::rohit::serializer::exception::bad_input_data{"} +
         local_name("serializer_protocol") +
         ".get_stream(), \"Invalid union discriminator\"};\n      }\n"));
  }

  // Decode a union selected by its numeric field identifier.
  void write_serializer_in_body_union_key_integer(stream& output, const member& field) {
    output.write("      case ", field.id, ": {\n");
    write_union_input(output, field);
    output.write("        break;\n      }\n");
  }

  // Decode the next positional union without an unnecessary discriminator temporary.
  void write_serializer_in_body_union_key_none(stream& output, const member& field) {
    write_union_input(output, field);
  }

  // Emit C++ serializer in body key none for the parsed schema.
  void write_serializer_in_body_key_none(stream& out_stream, const class_node* obj) {
    out_stream.write((std::string{"      [[maybe_unused]] auto "} + local_name("object_scope") +
                      " = ::rohit::serializer::detail::enter_decode_object(" +
                      local_name("serializer_protocol") + ");\n"));
    write_serializer_in_body_for_parent_key_none(out_stream, obj);
    for (const auto& member : obj->member_list) {
      if (member.modifier != member::modifier_type::variant) {
        write_serializer_in_body_non_union_key_none(out_stream, member);
      } else if (member.type_name_list.size()) {
        write_serializer_in_body_union_key_none(out_stream, member);
      }
    }
  } // write_serializer_in_body_key_none

  // Emit C++ serializer in body with key integer for the parsed schema.
  void write_serializer_in_body_with_key_integer(stream& out_stream, const class_node* obj) {
    out_stream.write((std::string{"  // Decode the member selected by its numeric wire "
                                  "identifier.\n  void serialize_in_member_by_identifier(auto& "} +
                      local_name("serializer_protocol") +
                      (std::string{", const ::std::uint32_t "} + local_name("identifier") +
                       ") {\n    switch (" + local_name("identifier") + ") {\n")));

    write_serializer_in_body_for_parent_key_integer(out_stream, obj);

    for (const auto& member : obj->member_list) {
      if (member.modifier != member::modifier_type::variant) {
        write_serializer_in_body_non_union_key_integer(out_stream, member);
      } else if (member.type_name_list.size()) {
        write_serializer_in_body_union_key_integer(out_stream, member);
      }
    }

    out_stream.write(
        (std::string{
             "      default:\n        throw ::rohit::serializer::exception::key_not_found {"} +
         local_name("serializer_protocol") +
         ".get_stream(), \"Bad member identifier\"};\n    }\n  }\n\n"));
  }

  // Collect actual wire names so hash collisions share a case and always require full equality.
  void write_serializer_in_body_with_key_string(stream& out_stream, const class_node* obj) {
    struct entry {
      std::string name;
      const parent* base{};
      const member* field{};
      const serializer::type_name* alternative{};
    };
    std::map<std::uint64_t, std::vector<entry>> groups;
    for (const auto& base : obj->parents) {
      groups[detail::field_name_hash(base.display_name)].push_back({base.display_name, &base});
    }
    for (const auto& field : obj->member_list) {
      if (field.modifier == member::modifier_type::variant) {
        for (const auto& alternative : field.type_name_list) {
          const auto name = field.display_name + ":" + alternative.enum_name;
          groups[detail::field_name_hash(name)].push_back({name, nullptr, &field, &alternative});
        }
      } else {
        groups[detail::field_name_hash(field.display_name)].push_back(
            {field.display_name, nullptr, &field});
      }
    }
    out_stream.write((std::string{"  // Match a wire name exactly within its hash group.\n  void "
                                  "serialize_in_member_by_name(auto& "} +
                      local_name("serializer_protocol") +
                      (std::string{", ::std::string_view "} + local_name("name") +
                       ") {\n    switch (::rohit::serializer::detail::field_name_hash(" +
                       local_name("name") + ")) {\n")));
    for (const auto& [hash_value, entries] : groups) {
      static_cast<void>(hash_value);
      out_stream.write("      case ::rohit::serializer::detail::field_name_hash(\"",
                       entries.front().name, "\"):\n");
      for (const auto& item : entries) {
        out_stream.write((std::string{"        if ("} + local_name("name") + " == \""), item.name,
                         "\") {\n");
        if (item.base) {
          out_stream.write("          static_cast<",
                           storage_type_name(item.base->name, item.base->parent_class),
                           (std::string{"*>(this)->serialize_in("} +
                            local_name("serializer_protocol") + ");\n"));
        } else if (item.alternative) {
          out_stream.write(
              "          this->", union_tag_name(*item.field), " = ", union_enum_name(*item.field),
              "::", enum_name(item.alternative->enum_name),
              ";\n"
              "          ::std::construct_at(&this->",
              field_name(item.field->name), ".", field_name(item.alternative->enum_name),
              (std::string{");\n          "} + local_name("serializer_protocol") +
               ".serialize_in(this->"),
              field_name(item.field->name), ".", field_name(item.alternative->enum_name), ");\n");
        } else if (item.field->modifier == member::modifier_type::none &&
                   item.field->type_name_list[0].type == object_type::enum_type) {
          out_stream.write((std::string{"          ::rohit::serializer::detail::read_named_enum("} +
                            local_name("serializer_protocol") + ", this->"),
                           field_name(item.field->name), ");\n");
        } else {
          out_stream.write((std::string{"          "} + local_name("serializer_protocol") +
                            ".serialize_in(this->"),
                           field_name(item.field->name), ");\n");
        }
        out_stream.write("          return;\n        }\n");
      }
      out_stream.write("        break;\n");
    }
    out_stream.write((std::string{"      default: break;\n    }\n    throw "
                                  "::rohit::serializer::exception::key_not_found{"} +
                      local_name("serializer_protocol") +
                      ".get_stream(), \"Unknown field name\"};\n  }\n\n"));
  }
  // Emit field reads selected at compile time, retaining keyed input dispatch where needed.
  void write_serializer_in_body(stream& out_stream, const class_node* obj) {
    write_serializer_in_body_with_key_integer(out_stream, obj);
    write_serializer_in_body_with_key_string(out_stream, obj);
    out_stream.write((
        std::string{"  // Decode fields using the protocol's compile-time key mode.\n  template "
                    "<typename SerializeInProtocol>\n  void serialize_in(SerializeInProtocol& "} +
        local_name("serializer_protocol") +
        ") {\n    static_assert(\n        SerializeInProtocol::key_type == "
        "::rohit::serializer::serialize_key_type::none ||\n        SerializeInProtocol::key_type "
        "== "
        "::rohit::serializer::serialize_key_type::integer ||\n        "
        "SerializeInProtocol::key_type "
        "== ::rohit::serializer::serialize_key_type::string,\n        \"Unsupported serializer key "
        "type\");"));
    out_stream.write("\n    if constexpr (SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::none) {\n");
    write_serializer_in_body_key_none(out_stream, obj);
    out_stream.write(
        (std::string{"    } else if constexpr (SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::integer ||\n            "
                     "SerializeInProtocol::key_type == "
                     "::rohit::serializer::serialize_key_type::string) {\n      "} +
         local_name("serializer_protocol") + ".template struct_serialize_in<"),
        type_name(obj->name),
        (std::string{">(this);\n    }\n  }\n\n  // Construct the requested protocol and read this "
                     "object.\n  template <template <::rohit::serializer::serialize_type> class "
                     "Protocol>\n  void serialize_in(const ::rohit::stream& "} +
         local_name("stream") + ") {\n    using " + type_name("serializer_in_protocol") +
         " = Protocol<::rohit::serializer::serialize_type::in>;\n    " +
         type_name("serializer_in_protocol") + " " + local_name("serializer_protocol") + "{" +
         local_name("stream") + "};\n    serialize_in(" + local_name("serializer_protocol") +
         ");\n  }\n"));
  }

  // Emit C++ serializer for the parsed schema.
  void write_serializer(stream& out_stream, const class_node* obj) {
    write_serializer_out_body(out_stream, obj);
    write_serializer_in_body(out_stream, obj);
  }

  // Build a typed view codec expression without introducing a runtime field descriptor table.
  std::string view_codec_name(const std::string& name, const syntax_node* node, storage_mode mode) {
    const std::string prefix{"::rohit::serializer::detail::"};
    if (!node && name == "string") {
      return prefix + "string_view_codec";
    }
    const auto type = storage_type_name(name, node, mode);
    return prefix +
           (node && node->type == object_type::class_type ? "object_view_codec<"
                                                          : "scalar_view_codec<") +
           type + ">";
  }

  // Compose collection and union codecs from their statically resolved element types.
  std::string view_codec_name(const member& field, storage_mode mode) {
    const auto& type = field.type_name_list.front();
    auto codec = view_codec_name(type.name, type.resolved_node, mode);
    const std::string prefix{"::rohit::serializer::detail::"};
    switch (field.modifier) {
    case member::modifier_type::none:
      return codec;
    case member::modifier_type::array:
      return prefix + "array_view_codec<" + codec + ">";
    case member::modifier_type::map:
      return prefix + "map_view_codec<" +
             view_codec_name(field.key, field.key_node, storage_mode::read_only_view) + ", " +
             codec + ">";
    case member::modifier_type::variant:
      codec = prefix + "variant_view_codec<";
      for (std::size_t index = 0; index < field.type_name_list.size(); ++index) {
        const auto& alternative = field.type_name_list[index];
        if (index != 0) {
          codec += ", ";
        }
        codec += view_codec_name(alternative.name, alternative.resolved_node, mode);
      }
      return codec + ">";
    }
    throw std::invalid_argument{"Unknown view field modifier"};
  }

  // Emit a constant-offset getter; access follows the schema field or parent's visibility.
  void write_view_getter(stream& output, const std::string& name, std::size_t index,
                         access_type access) {
    write_access_type(output, access);
    output.write(":\n  // Read this field or borrow its nested view from the mapped buffer.\n"
                 "  auto ",
                 function_name(name),
                 "() const {\n"
                 "    return ",
                 type_name("serializer_view_field_" + std::to_string(index)),
                 "::read(this->template field_bytes<", index, ">(), this->view_limits);\n  }\n");
  }

  // Emit one concrete view representation with validated mapping and size-preserving mutation.
  void write_view_class(stream& output, const class_node* obj, storage_mode mode) {
    const auto count = obj->parents.size() + obj->member_list.size();
    const std::string byte_type =
        mode == storage_mode::read_only_view ? "const ::std::uint8_t" : "::std::uint8_t";
    const auto base = "::rohit::serializer::detail::binary_view_base<" + byte_type + ", " +
                      std::to_string(count) + ">";
    if (obj->multiple_modes()) {
      output.write("template <>\n");
    }
    output.write("class ", type_name(obj->name));
    if (obj->multiple_modes()) {
      output.write("<", storage_mode_name(mode), ">");
    }
    output.write(" : public ", base, (std::string{" {\n  using "} + type_name("view_base") + " = "),
                 base, (std::string{";\n  using "} + type_name("view_byte") + " = "), byte_type,
                 ";\n");
    std::size_t index{};
    for (const auto& parent : obj->parents) {
      output.write("  using ", type_name("serializer_view_field_" + std::to_string(index++)), " = ",
                   view_codec_name(parent.name, parent.parent_class, mode), ";\n");
    }
    for (const auto& field : obj->member_list) {
      output.write("  using ", type_name("serializer_view_field_" + std::to_string(index++)), " = ",
                   view_codec_name(field, mode), ";\n");
    }
    output.write(
        "  // Construct only through map(), validating before exposing the object.\n"
        "  explicit ",
        type_name(obj->name),
        (std::string{"(::std::span<"} + type_name("view_byte") +
         (std::string{"> "} + local_name("bytes") + ", ::rohit::serializer::decode_limits " +
          local_name("limits") + ")\n      : ") +
         type_name("view_base") +
         (std::string{"{"} + local_name("bytes") + ", " + local_name("limits") +
          ", serializer_scan} {}\npublic:\n  // Borrow an exact little-endian positional message; "
          "its storage must outlive all views.\n  static ")),
        type_name(obj->name),
        (std::string{" map(::std::span<"} + type_name("view_byte") +
         (std::string{"> "} + local_name("bytes") +
          ",\n                    ::rohit::serializer::decode_limits " + local_name("limits") +
          " = {}) {\n    return ")),
        type_name(obj->name),
        (std::string{"{"} + local_name("bytes") + ", " + local_name("limits") +
         "};\n  }\n  // Internal schema traversal: share nested budgets and optionally record "
         "field offsets.\n  static void "
         "serializer_scan(::rohit::serializer::detail::view_scanner& " +
         local_name("scanner") + ",\n                              ::std::size_t* " +
         local_name("offsets") + ") {\n    auto " + local_name("nesting") + " = " +
         local_name("scanner") + ".enter_object();\n"));
    for (index = 0; index < count; ++index) {
      output.write(
          (std::string{"    if ("} + local_name("offsets") + ") { " + local_name("offsets") + "["),
          index, (std::string{"] = "} + local_name("scanner") + ".position(); }\n    "),
          type_name("serializer_view_field_" + std::to_string(index)),
          (std::string{"::scan("} + local_name("scanner") + ");\n"));
    }
    output.write(
        (std::string{"    if ("} + local_name("offsets") + ") { " + local_name("offsets") + "["),
        count, (std::string{"] = "} + local_name("scanner") + ".position(); }\n  }\n"));
    index = 0;
    for (const auto& parent : obj->parents) {
      write_view_getter(output, "get_base_" + std::to_string(parent.id), index++, parent.access);
    }
    for (const auto& field : obj->member_list) {
      write_view_getter(output, "get_" + field.name, index, field.access);
      if (field.modifier == member::modifier_type::variant) {
        output.write("  enum class ", union_enum_name(field), " {\n");
        for (const auto& alternative : field.type_name_list) {
          output.write("    ", enum_name(alternative.enum_name), ",\n");
        }
        output.write("  };\n"
                     "  // Return the active alternative using the schema's symbolic names.\n"
                     "  ",
                     union_enum_name(field), " ", function_name("get_" + field.name + "_type"),
                     "() const {\n    return static_cast<", union_enum_name(field), ">(",
                     function_name("get_" + field.name), "().index());\n  }\n");
      } else if (mode == storage_mode::mutable_view &&
                 field.modifier == member::modifier_type::none &&
                 field.type_name_list.front().type != object_type::class_type) {
        const auto& type = field.type_name_list.front();
        const auto value_type = type.name == "string"
                                    ? "::std::string_view"
                                    : storage_type_name(type.name, type.resolved_node);
        output.write(
            "  // Update existing bytes; a size-changing replacement throws before mutation.\n"
            "  void ",
            function_name("set_" + field.name), "(", value_type,
            (std::string{" "} + local_name("value") + ") {\n    "),
            type_name("serializer_view_field_" + std::to_string(index)),
            "::write(this->template field_bytes<", index,
            (std::string{">(), "} + local_name("value") + ");\n  }\n"));
      }
      ++index;
    }
    output.write("}; // view ", type_name(obj->name), "\n\n");
  }

  // Emit the existing owning API, selecting owning specializations for nested classes.
  void write_owning_class(stream& out_stream, const class_node* obj) {
    if (obj->multiple_modes()) {
      out_stream.write("template <>\n");
    }
    if ((obj->attributes & class_attributes::packed) == class_attributes::packed) {
      out_stream.write("class __attribute__ ((__packed__)) ", type_name(obj->name));
    } else {
      out_stream.write("class ", type_name(obj->name));
    }
    if (obj->multiple_modes()) {
      out_stream.write("<", storage_mode_name(storage_mode::owning), ">");
    }

    if (!obj->parents.empty()) {
      out_stream.write(" : ");
      write_parent_list(out_stream, obj->parents);
    }

    out_stream.write(" {\n");
    write_member_list(out_stream, obj->member_list);

    out_stream.write("\npublic:\n");
    write_serializer(out_stream, obj);

    out_stream.write("}; // class ", type_name(obj->name), "\n\n");
  }

  // Emit only requested modes; a single mode has no class template declaration.
  void write_class(stream& output, const class_node* obj) {
    if (obj->multiple_modes()) {
      output.write("template <::rohit::serializer::storage_mode Mode>\nclass ",
                   type_name(obj->name), ";\n\n");
    }
    if (obj->has_mode(storage_mode::owning)) {
      write_owning_class(output, obj);
    }
    for (const auto mode : {storage_mode::read_only_view, storage_mode::mutable_view}) {
      if (obj->has_mode(mode)) {
        write_view_class(output, obj, mode);
      }
    }
  }

  // Keep view helpers out of generated headers whose schemas request only owning objects.
  bool contains_views(const std::vector<std::unique_ptr<syntax_node>>& statements) {
    for (const auto& statement : statements) {
      if (statement->type == object_type::class_type) {
        const auto* obj = static_cast<const class_node*>(statement.get());
        if (obj->has_mode(storage_mode::read_only_view) ||
            obj->has_mode(storage_mode::mutable_view)) {
          return true;
        }
      } else if (statement->type == object_type::namespace_type &&
                 contains_views(static_cast<const namespace_node*>(statement.get())->statements)) {
        return true;
      }
    }
    return false;
  }

  // Emit enum values, allocation-free spellings, and collision-aware name conversion.
  void write_enum(stream& out_stream, const enum_node* enum_ptr) {
    out_stream.write("enum class ", type_name(enum_ptr->name), " {\n");
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("  ", enum_name(name), ",\n");
    }
    out_stream.write("};\n\n"
                     "// Validate numeric input against the declared enum alternatives.\n"
                     "constexpr bool serializer_enum_valid(",
                     type_name(enum_ptr->name),
                     (std::string{" "} + local_name("value") + ") noexcept {\n  switch (" +
                      local_name("value") + ") {\n"));
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("    case ", type_name(enum_ptr->name), "::", enum_name(name),
                       ": return true;\n");
    }
    out_stream.write("    default: return false;\n  }\n}\n\n"
                     "// Borrow the unchanged wire spelling for this enum.\n"
                     "constexpr ::std::string_view serializer_enum_name(",
                     type_name(enum_ptr->name),
                     (std::string{" "} + local_name("value") + ") {\n  switch (" +
                      local_name("value") + ") {\n"));
    for (const auto& name : enum_ptr->enum_name_list) {
      out_stream.write("    case ", type_name(enum_ptr->name), "::", enum_name(name), ": return \"",
                       name, "\";\n");
    }
    out_stream.write(
        "    default: throw ::std::invalid_argument{\"Unknown enum value\"};\n  }\n}\n\n"
        "// Preserve the owning-string API for callers that need a copy.\n"
        "constexpr ::std::string to_string(",
        type_name(enum_ptr->name),
        (std::string{" "} + local_name("value") +
         ") {\n  return ::std::string{serializer_enum_name(" + local_name("value") +
         ")};\n}\n\n// Resolve only a complete matching wire spelling.\nconstexpr "),
        type_name(enum_ptr->name), " ", function_name("to_" + enum_ptr->name),
        (std::string{"(const auto& "} + local_name("value") + ") {\n  const ::std::string_view " +
         local_name("name") + "{" + local_name("value") +
         "};\n  switch (::rohit::serializer::detail::field_name_hash(" + local_name("name") +
         ")) {\n"));
    std::map<std::uint64_t, std::vector<std::string_view>> groups;
    for (const auto& name : enum_ptr->enum_name_list) {
      groups[detail::field_name_hash(name)].push_back(name);
    }
    for (const auto& [hash_value, names] : groups) {
      static_cast<void>(hash_value);
      out_stream.write("    case ::rohit::serializer::detail::field_name_hash(\"", names.front(),
                       "\"):\n");
      for (const auto name : names) {
        out_stream.write((std::string{"      if ("} + local_name("name") + " == \""), name,
                         "\") { return ", type_name(enum_ptr->name), "::", enum_name(name),
                         "; }\n");
      }
      out_stream.write("      break;\n");
    }
    out_stream.write("    default: break;\n  }\n"
                     "  throw ::std::invalid_argument{\"Unknown enum name\"};\n}\n\n"
                     "// Decode a name when this enum appears in a collection.\n"
                     "inline void serializer_enum_from_name(",
                     type_name(enum_ptr->name),
                     (std::string{"& "} + local_name("value") + ", ::std::string_view " +
                      local_name("name") + ") { " + local_name("value") + " = "),
                     function_name("to_" + enum_ptr->name),
                     (std::string{"("} + local_name("name") + "); }\n\n"));
  }
  // Emit C++ namespace for the parsed schema.
  void write_namespace(stream& out_stream, const namespace_node* namespace_ptr) {
    std::string full_name = namespace_name(namespace_ptr->name);
    while (namespace_ptr->statements.size() == 1 &&
           namespace_ptr->statements.back()->type == object_type::namespace_type) {
      namespace_ptr = dynamic_cast<namespace_node*>(namespace_ptr->statements.back().get());
      full_name += "::";
      full_name += namespace_name(namespace_ptr->name);
    }
    out_stream.write("namespace ", full_name, " {\n");
    write_statement_list(out_stream, namespace_ptr->statements);
    out_stream.write("} // namespace ", full_name, "\n\n");
  }

  // Emit C++ statement list for the parsed schema.
  void write_statement_list(stream& out_stream,
                            const std::vector<std::unique_ptr<syntax_node>>& statements) {
    if (statements.empty()) {
      return;
    }

    for (const auto& statement : statements) {
      switch (statement->type) {
      case object_type::namespace_type:
        write_namespace(out_stream, dynamic_cast<const namespace_node*>(statement.get()));
        break;

      case object_type::class_type:
        write_class(out_stream, dynamic_cast<const class_node*>(statement.get()));
        break;

      case object_type::enum_type:
        write_enum(out_stream, dynamic_cast<const enum_node*>(statement.get()));
        break;

      default:
        break;
      }
    }
  }

  // Generate a complete C++ header while retaining the parsed schema and original wire names.
  void emit(stream& out_stream, const std::vector<std::unique_ptr<syntax_node>>& statements) {
    validate_names(statements);
    const bool has_views = contains_views(statements);
    out_stream.write("// Generated by Serializer. Do not edit this file manually.\n"
                     "// https://github.com/rohit-singh-gautam/Serializer\n\n"
                     "#pragma once\n\n"
                     "#include <rohit/serializer.hpp>\n");
    if (has_views) {
      out_stream.write("#include <rohit/binary_view.hpp>\n");
    }
    out_stream.write('\n');
    if (has_views) {
      out_stream.write("#include <cstddef>\n#include <span>\n");
    }
    out_stream.write("#include <cstdint>\n"
                     "#include <functional>\n"
                     "#include <map>\n"
                     "#include <memory>\n"
                     "#include <stdexcept>\n"
                     "#include <string>\n"
                     "#include <string_view>\n"
                     "#include <tuple>\n"
                     "#include <type_traits>\n"
                     "#include <utility>\n"
                     "#include <vector>\n\n");
    write_statement_list(out_stream, statements);
  }
};
} // namespace

// Commit output to the caller only after name validation and formatting both succeed.
void write(stream& out_stream, const std::vector<std::unique_ptr<syntax_node>>& statements,
           const cpp_options& options) {
  static_cast<void>(coding_standard_name(options.standard));
  full_stream_auto_alloc raw{};
  emitter generator{options};
  generator.emit(raw, statements);
  const std::string_view source{reinterpret_cast<const char*>(raw.begin()), raw.current_offset()};
  out_stream.write(format_cpp(source, options));
}
} // namespace rohit::serializer::writer::cpp
