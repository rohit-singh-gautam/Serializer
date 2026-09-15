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

#include <rohit/serializer_creator.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::cpp {

// Emit union storage plus exact alternative-name conversion without hash-only matches.
std::string get_cpp_type_support_union(const member& member) {
  std::string result{"  enum class e_" + member.name + " {\n"};
  for (const auto& type : member.type_name_list) {
    result += "    " + type.enum_name + ",\n";
  }
  result += "  };\n  union u_" + member.name + " {\n";
  for (const auto& type : member.type_name_list) {
    result += "    " + serializer::get_cpp_type(type.name) + " " + type.enum_name + ";\n";
  }
  result += "  };\n  static_assert(std::is_trivially_destructible_v<u_" + member.name +
            ">, \"Raw union payloads must be trivially destructible\");\n"
            "  // Return the wire name for a valid union alternative.\n"
            "  static std::string to_string(e_" + member.name + " value) {\n    switch (value) {\n";
  for (const auto& type : member.type_name_list) {
    result += "      case e_" + member.name + "::" + type.enum_name + ": return \"" + type.enum_name + "\";\n";
  }
  result += "      default: throw std::invalid_argument{\"Invalid union discriminator\"};\n    }\n  }\n"
            "  // Match the full wire name, including when names share a hash.\n"
            "  static e_" + member.name + " to_e_" + member.name + "(const auto& value) {\n"
            "    const std::string_view name{value};\n"
            "    const auto hash = rohit::serializer::detail::field_name_hash(name);\n";
  for (const auto& type : member.type_name_list) {
    result += "    if (hash == rohit::serializer::detail::field_name_hash(\"" + type.enum_name +
              "\") && name == \"" + type.enum_name + "\") { return e_" + member.name + "::" + type.enum_name + "; }\n";
  }
  result += "    throw std::invalid_argument{\"Unknown union alternative\"};\n  }";
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
    return serializer::get_cpp_type(member.type_name_list[0].name);
  case member::modifier_type::array:
    return std::string("std::vector<") + serializer::get_cpp_type(member.type_name_list[0].name) +
           ">";
  case member::modifier_type::map:
    return std::string("std::map<") + serializer::get_cpp_type(member.key) + "," +
           serializer::get_cpp_type(member.type_name_list[0].name) + ">";
  case member::modifier_type::variant:
    return "e_" + member.name + " " + member.name + "_type { };\n  " + "u_" + member.name;
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
    out_stream.write(' ', parent.name);
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
    out_stream.write("  ", get_cpp_type(member), ' ', member.name, " { ");
    if (!member.default_value.empty()) {
      out_stream.write(member.default_value, ' ');
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
      out_stream.write("\n      serializer_protocol.struct_serialize_out_start(");
    } else {
      out_stream.write("\n      serializer_protocol.struct_serialize_out(");
    }
    if (key_type == rohit::serializer::serialize_key_type::string) {
      out_stream.write("std::make_pair(std::string_view { \"", parent.display_name,
                       "\" }, static_cast<const ", parent.name,
                       " *>(this))"
                       ");");
    } else if (key_type == rohit::serializer::serialize_key_type::integer) {
      out_stream.write("std::make_pair(static_cast<std::uint32_t>(", parent.id,
                       "), static_cast<const ", parent.name,
                       " *>(this))"
                       ");");
    } else {
      out_stream.write("static_cast<const ", parent.name,
                       " *>(this)"
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
    out_stream.write("\n      serializer_protocol.struct_serialize_out_start(");
  } else {
    out_stream.write("\n      serializer_protocol.struct_serialize_out(");
  }
  if (key_type == rohit::serializer::serialize_key_type::string) {
    if (member.modifier != member::modifier_type::none ||
        member.type_name_list[0].type != object_type::enum_type) {
      out_stream.write("std::make_pair(std::string_view { \"", member.display_name, "\" }, ",
                       "std::cref(", member.name,
                       "))"
                       ");");
    } else {
      out_stream.write("std::make_pair(std::string_view { \"", member.display_name, "\" }, ",
                       "rohit::serializer::detail::enum_name(",
                       member.name,
                       "))"
                       ");");
    }
  } else if (key_type == rohit::serializer::serialize_key_type::integer) {
    out_stream.write("std::make_pair(static_cast<std::uint32_t>(", member.id, "), std::cref(", member.name,
                     "))"
                     ");");
  } else {
    out_stream.write(member.name, ");");
  }
}

// Emit C++ serializer out body union for the parsed schema.
void write_serializer_out_body_union(stream& out_stream, const member& member,
                                     const rohit::serializer::serialize_key_type key_type,
                                     bool& first) {
  out_stream.write("\n      switch (", member.name, "_type) {");
  for (std::size_t index{0}; index < member.type_name_list.size(); ++index) {
    out_stream.write("\n        case e_", member.name, "::", member.type_name_list[index].enum_name,
                     ":");
    if (first) {
      out_stream.write("\n          serializer_protocol.struct_serialize_out_start(");
    } else {
      out_stream.write("\n          serializer_protocol.struct_serialize_out(");
    }
    if (key_type == rohit::serializer::serialize_key_type::string) {
      out_stream.write("std::make_pair( std::string_view {\"", member.display_name, ":",
                       member.type_name_list[index].enum_name, "\"}, std::cref(", member.name, ".",
                       member.type_name_list[index].enum_name, ")));", "\n          break;");
    } else if (key_type == rohit::serializer::serialize_key_type::integer) {
      out_stream.write("std::make_tuple(static_cast<std::uint32_t>(", member.id,
                       "), static_cast<std::uint32_t>(", index, "), std::cref(", member.name, ".",
                       member.type_name_list[index].enum_name, ")));", "\n          break;");
    } else {
      out_stream.write("std::make_pair(static_cast<std::uint32_t>(", index, "), std::cref(", member.name, ".",
                       member.type_name_list[index].enum_name, ")));", "\n          break;");
    }
  }
  first = false;
  out_stream.write("\n        default: throw std::invalid_argument{\"Invalid union discriminator\"};"
                   "\n      }");
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
  out_stream.write(first ? "\n      serializer_protocol.struct_serialize_out_empty();" :
                           "\n      serializer_protocol.struct_serialize_out_end();");
}

// Emit direct field writes selected at compile time by the output protocol type.
void write_serializer_out_body(stream& out_stream, const class_node* obj) {
  out_stream.write("  // Encode fields directly using the protocol's compile-time key mode.\n  template "
                   "<typename SerializeOutProtocol>\n"
                   "  void serialize_out(SerializeOutProtocol& serializer_protocol) const {\n"
                   "    static_assert(\n"
                   "        SerializeOutProtocol::key_type == rohit::serializer::serialize_key_type::none ||\n"
                   "        SerializeOutProtocol::key_type == rohit::serializer::serialize_key_type::integer ||\n"
                   "        SerializeOutProtocol::key_type == rohit::serializer::serialize_key_type::string,\n"
                   "        \"Unsupported serializer key type\");");
  out_stream.write("\n    if constexpr (SerializeOutProtocol::key_type == "
                   "rohit::serializer::serialize_key_type::none) {");
  write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::none);
  out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                   "rohit::serializer::serialize_key_type::integer) {");
  write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::integer);
  out_stream.write("\n    } else if constexpr (SerializeOutProtocol::key_type == "
                   "rohit::serializer::serialize_key_type::string) {");
  write_serializer_out_body(out_stream, obj, rohit::serializer::serialize_key_type::string);
  out_stream.write("\n    }\n  }\n\n");
  out_stream.write(
      "  template <template<rohit::serializer::serialize_type> class SerializerProtocol>\n"
      "  // Construct the requested protocol and write this object.\n  void "
      "serialize_out(rohit::stream& stream) const {\n"
      "    using serializer_out_protocol = "
      "SerializerProtocol<rohit::serializer::serialize_type::out>;\n"
      "    serializer_out_protocol serializer_protocol { stream };\n"
      "    serialize_out(serializer_protocol);\n"
      "  }\n\n");
}

// Emit C++ serializer in body for parent key none for the parsed schema.
void write_serializer_in_body_for_parent_key_none(stream& out_stream, const class_node* obj) {
  for (const auto& parent : obj->parents) {
    out_stream.write("      this->", parent.name, "::serialize_in(serializer_protocol);\n");
  }
}

// Emit C++ serializer in body for parent key integer for the parsed schema.
void write_serializer_in_body_for_parent_key_integer(stream& out_stream, const class_node* obj) {
  for (const auto& parent : obj->parents) {
    out_stream.write("      case ", parent.id,
                     ":\n"
                     "        this->",
                     parent.name,
                     "::serialize_in(serializer_protocol);\n"
                     "        break;\n");
  }
} // write_serializer_in_body_for_parent_key_integer

// Emit C++ serializer in body non union key integer for the parsed schema.
void write_serializer_in_body_non_union_key_integer(stream& out_stream, const member& member) {
  out_stream.write("      case ", member.id,
                   ":\n"
                   "        serializer_protocol.template serialize_in<",
                   get_cpp_type(member), ">(this->", member.name,
                   ");\n"
                   "        break;\n");
} // write_serializer_in_body_non_union_key_integer

// Emit C++ serializer in body non union key none for the parsed schema.
void write_serializer_in_body_non_union_key_none(stream& out_stream, const member& member) {
  out_stream.write("      serializer_protocol.template serialize_in<", get_cpp_type(member),
                   ">(this->", member.name, ");\n");
} // write_serializer_in_body_non_union_key_none

// Emit C++ serializer in body union key integer for the parsed schema.
void write_serializer_in_body_union_key_integer(stream& out_stream, const member& member) {
  out_stream.write("      case ", member.id,
                   ": {\n"
                   "        this->",
                   member.name, "_type = static_cast<e_", member.name,
                   ">(serializer_protocol.serialize_in_variable());\n"
                   "        switch (this->",
                   member.name, "_type) {\n");

  for (std::size_t index{0}; index < member.type_name_list.size(); ++index) {
    out_stream.write("          case e_", member.name, "::", member.type_name_list[index].enum_name,
                     ":\n"
                     "            std::construct_at(&this->", member.name, ".",
                     member.type_name_list[index].enum_name, ");\n"
                     "            serializer_protocol.serialize_in(this->",
                     member.name, ".", member.type_name_list[index].enum_name,
                     ");\n"
                     "            break;\n");
  }
  out_stream.write("          default:\n"
                   "            throw rohit::serializer::exception::bad_input_data "
                   "{serializer_protocol.get_stream(), \"Invalid union discriminator\"};\n"
                   "        }\n"
                   "        break;\n"
                   "      }\n");
} // write_serializer_in_body_union_key_integer

// Emit C++ serializer in body union key none for the parsed schema.
void write_serializer_in_body_union_key_none(stream& out_stream, const member& member) {
  out_stream.write("      std::uint32_t ", member.name,
                   "_type_local { };\n"
                   "      ",
                   member.name,
                   "_type_local = serializer_protocol.serialize_in_variable();\n"
                   "      this->",
                   member.name, "_type = static_cast<e_", member.name, ">(", member.name,
                   "_type_local);\n"
                   "      switch (this->",
                   member.name, "_type) {\n");
  for (std::size_t index{0}; index < member.type_name_list.size(); ++index) {
    out_stream.write("        case e_", member.name, "::", member.type_name_list[index].enum_name,
                     ":\n"
                     "          std::construct_at(&this->", member.name, ".",
                     member.type_name_list[index].enum_name, ");\n"
                     "          serializer_protocol.serialize_in(this->",
                     member.name, ".", member.type_name_list[index].enum_name,
                     ");\n"
                     "          break;\n");
  }
  out_stream.write("        default: throw rohit::serializer::exception::bad_input_data{"
                   "serializer_protocol.get_stream(), \"Invalid union discriminator\"};\n"
                   "      }\n");
} // write_serializer_in_body_union_key_none

// Emit C++ serializer in body key none for the parsed schema.
void write_serializer_in_body_key_none(stream& out_stream, const class_node* obj) {
  out_stream.write("      [[maybe_unused]] auto object_scope = "
                   "rohit::serializer::detail::enter_decode_object(serializer_protocol);\n");
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
  out_stream.write("  // Decode the member selected by its numeric wire identifier.\n  void "
                   "serialize_in_member_by_identifier(auto& serializer_protocol, const "
                   "std::uint32_t identifier) {\n"
                   "    switch (identifier) {\n");

  write_serializer_in_body_for_parent_key_integer(out_stream, obj);

  for (const auto& member : obj->member_list) {
    if (member.modifier != member::modifier_type::variant) {
      write_serializer_in_body_non_union_key_integer(out_stream, member);
    } else if (member.type_name_list.size()) {
      write_serializer_in_body_union_key_integer(out_stream, member);
    }
  }

  out_stream.write("      default:\n"
                   "        throw rohit::serializer::exception::key_not_found "
                   "{serializer_protocol.get_stream(), \"Bad member identifier\"};\n"
                   "    }\n"
                   "  }\n\n");
}

// Collect actual wire names so hash collisions share a case and always require full equality.
void write_serializer_in_body_with_key_string(stream& out_stream, const class_node* obj) {
  struct entry {
    std::string name;
    const parent* base{};
    const member* field{};
    const type_name* alternative{};
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
      groups[detail::field_name_hash(field.display_name)].push_back({field.display_name, nullptr, &field});
    }
  }
  out_stream.write("  // Match a wire name exactly within its hash group.\n"
                   "  void serialize_in_member_by_name(auto& serializer_protocol, "
                   "std::string_view name) {\n"
                   "    switch (rohit::serializer::detail::field_name_hash(name)) {\n");
  for (const auto& [hash_value, entries] : groups) {
    static_cast<void>(hash_value);
    out_stream.write("      case rohit::serializer::detail::field_name_hash(\"", entries.front().name,
                     "\"):\n");
    for (const auto& item : entries) {
      out_stream.write("        if (name == \"", item.name, "\") {\n");
      if (item.base) {
        out_stream.write("          this->", item.base->name, "::serialize_in(serializer_protocol);\n");
      } else if (item.alternative) {
        out_stream.write("          this->", item.field->name, "_type = e_", item.field->name, "::",
                         item.alternative->enum_name, ";\n"
                         "          std::construct_at(&this->", item.field->name, ".",
                         item.alternative->enum_name, ");\n"
                         "          serializer_protocol.serialize_in(this->", item.field->name, ".",
                         item.alternative->enum_name, ");\n");
      } else if (item.field->modifier == member::modifier_type::none &&
                 item.field->type_name_list[0].type == object_type::enum_type) {
        out_stream.write("          rohit::serializer::detail::read_named_enum(serializer_protocol, this->",
                         item.field->name, ");\n");
      } else {
        out_stream.write("          serializer_protocol.serialize_in(this->", item.field->name, ");\n");
      }
      out_stream.write("          return;\n        }\n");
    }
    out_stream.write("        break;\n");
  }
  out_stream.write("      default: break;\n    }\n"
                   "    throw rohit::serializer::exception::key_not_found{"
                   "serializer_protocol.get_stream(), \"Unknown field name\"};\n  }\n\n");
}
// Emit field reads selected at compile time, retaining keyed input dispatch where needed.
void write_serializer_in_body(stream& out_stream, const class_node* obj) {
  write_serializer_in_body_with_key_integer(out_stream, obj);
  write_serializer_in_body_with_key_string(out_stream, obj);
  out_stream.write("  // Decode fields using the protocol's compile-time key mode.\n  template "
                   "<typename SerializeInProtocol>\n"
                   "  void serialize_in(SerializeInProtocol& serializer_protocol) {\n"
                   "    static_assert(\n"
                   "        SerializeInProtocol::key_type == rohit::serializer::serialize_key_type::none ||\n"
                   "        SerializeInProtocol::key_type == rohit::serializer::serialize_key_type::integer ||\n"
                   "        SerializeInProtocol::key_type == rohit::serializer::serialize_key_type::string,\n"
                   "        \"Unsupported serializer key type\");");
  out_stream.write("\n    if constexpr (SerializeInProtocol::key_type == "
                   "rohit::serializer::serialize_key_type::none) {\n");
  write_serializer_in_body_key_none(out_stream, obj);
  out_stream.write(
      "    } else if constexpr (SerializeInProtocol::key_type == "
      "rohit::serializer::serialize_key_type::integer ||\n"
      "            SerializeInProtocol::key_type == rohit::serializer::serialize_key_type::string) "
      "{\n"
      "      serializer_protocol.template struct_serialize_in<",
      obj->name,
      ">(this);\n"
      "    }\n"
      "  }\n\n"
      "  template <template<rohit::serializer::serialize_type> class SerializerProtocol>\n"
      "  // Construct the requested protocol and read this object.\n  void serialize_in(const "
      "rohit::stream& stream) {\n"
      "    using serializer_in_protocol = "
      "SerializerProtocol<rohit::serializer::serialize_type::in>;\n"
      "    serializer_in_protocol serializer_protocol { stream };\n"
      "    serialize_in(serializer_protocol);\n"
      "  }\n  ");
}

// Emit C++ serializer for the parsed schema.
void write_serializer(stream& out_stream, const class_node* obj) {
  write_serializer_out_body(out_stream, obj);
  write_serializer_in_body(out_stream, obj);
}

// Emit C++ class for the parsed schema.
void write_class(stream& out_stream, const class_node* obj) {
  if ((obj->attributes & class_attributes::packed) == class_attributes::packed) {
    out_stream.write("class __attribute__ ((__packed__)) ", obj->name);
  } else {
    out_stream.write("class ", obj->name);
  }

  if (!obj->parents.empty()) {
    out_stream.write(" : ");
    write_parent_list(out_stream, obj->parents);
  }

  out_stream.write(" {\n");
  write_member_list(out_stream, obj->member_list);

  out_stream.write("\npublic:\n");
  write_serializer(out_stream, obj);

  out_stream.write("}; // class ", obj->name, "\n\n");
}

// Emit enum values, allocation-free spellings, and collision-aware name conversion.
void write_enum(stream& out_stream, const enum_node* enum_ptr) {
  out_stream.write("enum class ", enum_ptr->name, " {\n");
  for (const auto& name : enum_ptr->enum_name_list) {
    out_stream.write("  ", name, ",\n");
  }
  out_stream.write("};\n\n"
                   "// Validate numeric input against the declared enum alternatives.\n"
                   "constexpr bool serializer_enum_valid(", enum_ptr->name, " value) noexcept {\n"
                   "  switch (value) {\n");
  for (const auto& name : enum_ptr->enum_name_list) {
    out_stream.write("    case ", enum_ptr->name, "::", name, ": return true;\n");
  }
  out_stream.write("    default: return false;\n  }\n}\n\n"
                   "// Borrow the unchanged wire spelling for this enum.\n"
                   "constexpr std::string_view serializer_enum_name(", enum_ptr->name, " value) {\n"
                   "  switch (value) {\n");
  for (const auto& name : enum_ptr->enum_name_list) {
    out_stream.write("    case ", enum_ptr->name, "::", name, ": return \"", name, "\";\n");
  }
  out_stream.write("    default: throw std::invalid_argument{\"Unknown enum value\"};\n  }\n}\n\n"
                   "// Preserve the owning-string API for callers that need a copy.\n"
                   "constexpr inline std::string to_string(", enum_ptr->name, " value) {\n"
                   "  return std::string{serializer_enum_name(value)};\n}\n\n"
                   "// Resolve only a complete matching wire spelling.\n"
                   "constexpr ", enum_ptr->name, " to_", enum_ptr->name, "(const auto& value) {\n"
                   "  const std::string_view name{value};\n"
                   "  switch (rohit::serializer::detail::field_name_hash(name)) {\n");
  std::map<std::uint64_t, std::vector<std::string_view>> groups;
  for (const auto& name : enum_ptr->enum_name_list) {
    groups[detail::field_name_hash(name)].push_back(name);
  }
  for (const auto& [hash_value, names] : groups) {
    static_cast<void>(hash_value);
    out_stream.write("    case rohit::serializer::detail::field_name_hash(\"", names.front(), "\"):\n");
    for (const auto name : names) {
      out_stream.write("      if (name == \"", name, "\") { return ", enum_ptr->name, "::", name, "; }\n");
    }
    out_stream.write("      break;\n");
  }
  out_stream.write("    default: break;\n  }\n"
                   "  throw std::invalid_argument{\"Unknown enum name\"};\n}\n\n"
                   "// Decode a name when this enum appears in a collection.\n"
                   "inline void serializer_enum_from_name(", enum_ptr->name,
                   "& value, std::string_view name) { value = to_", enum_ptr->name,
                   "(name); }\n\n");
}
// Emit C++ statement list for the parsed schema.
void write_statement_list(stream& out_stream,
                          const std::vector<std::unique_ptr<syntax_node>>& statements);

// Emit C++ namespace for the parsed schema.
void write_namespace(stream& out_stream, const namespace_node* namespace_ptr) {
  std::string full_name = namespace_ptr->name;
  while (namespace_ptr->statements.size() == 1 &&
         namespace_ptr->statements.back()->type == object_type::namespace_type) {
    namespace_ptr = dynamic_cast<namespace_node*>(namespace_ptr->statements.back().get());
    full_name += "::";
    full_name += namespace_ptr->name;
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

// Generate a complete C++ header from the resolved schema without rewriting its names.
void write(stream& out_stream, std::vector<std::unique_ptr<syntax_node>>& statements) {
  out_stream.write("/////////////////////////////////////////////////////////\n"
                   "// This is auto genarated file using serializer. Must  //\n"
                   "// not be manually edited. For more information refer  //\n"
                   "// to https://github.com/rohit-singh-gautam/Serializer //\n"
                   "/////////////////////////////////////////////////////////\n"
                   "\n"
                   "#pragma once\n"
                   "#include <rohit/serializer.hpp>\n\n"
                   "#include <cstdint>\n"
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

} // namespace rohit::serializer::writer::cpp
