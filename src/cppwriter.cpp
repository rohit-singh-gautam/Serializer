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

#include <rohit/serializercreator.h>

namespace rohit::serializer::Writer::CPP {

const std::string GetCPPTypeSupportUnion(const Member &member) {
    std::string retUnion { "\tenum class e_" };
    retUnion += member.Name;
    retUnion += " {\n";
    for(auto &typeName: member.typeNameList) {
        retUnion += "\t\t" + typeName.EnumName + ",\n";
    }
    retUnion += "\t};\n\tunion u_" + member.Name + " {\n";
    for(size_t index { 0 }; index < member.typeNameList.size(); ++index) {
        retUnion += "\t\t" + member.typeNameList[index].Name + " " + member.typeNameList[index].EnumName + ";\n";
    }
    retUnion += "\t};\n\tstatic std::string to_string(const e_" + member.Name + " v) {\n\t\tswitch(v) {\n\t\t\tdefault:";
    for(auto &typeName: member.typeNameList) {
        retUnion += "\n\t\t\tcase e_" + member.Name + "::" + typeName.EnumName + ": return {\"" + typeName.EnumName + "\"}; ";
    }
    retUnion += "\n\t\t}\n\t};\n\tstatic e_" + member.Name + " to_e_" + member.Name + "(const auto &v) {"
                "\n\t\tswitch(rohit::Hash(v)) {";
    for(auto &typeName: member.typeNameList) {
        retUnion += "\n\t\t\tcase rohit::Hash(\"" + typeName.EnumName + "\"): return e_" + member.Name + "::" + typeName.EnumName + ";";
    }
    retUnion += "\n\t\t\tdefault: throw std::runtime_error(\"Bad Enum Name\");"
                "\n\t\t}"
                "\n\t}";
    return retUnion;
}

const std::string GetCPPTypeSupport(const Member &member) {
    switch(member.modifer) {
    default:
    case Member::none:
        return { };
    case Member::array:
        return { };
    case Member::map:
        return { };
    case Member::Union:
        return GetCPPTypeSupportUnion(member);
    }
}

const std::string GetCPPType(const Member &member) {
    switch(member.modifer) {
    default:
    case Member::none:
        // TODO: Range check
        return serializer::GetCPPType(member.typeNameList[0].Name);
    case Member::array:
        return std::string("std::vector<") + serializer::GetCPPType(member.typeNameList[0].Name) + ">";
    case Member::map:
        return std::string("std::map<") + serializer::GetCPPType(member.Key) + "," + serializer::GetCPPType(member.typeNameList[0].Name) + ">";
    case Member::Union:
        return "e_" + member.Name + " " + member.Name + "_type { };\n\t" + "u_" + member.Name;
    }
}

void WriteAccessType(Stream &outStream, const AccessType access) {
    switch(access) {
        default:
        case AccessType::Public:
            outStream.Write("public");
            break;
        
        case AccessType::Protected:
            outStream.Write("protected");
            break;
        
        case AccessType::Private:
            outStream.Write("private");
            break;
    }
}

void WriteParentList(Stream &outStream, const std::vector<Parent> &parents) {
    bool first { true };
    for(auto &parent: parents) {
        if (first) first = false;
        else outStream.Write(", ");
        WriteAccessType(outStream, parent.access);
        outStream.Write(' ', parent.Name);
    }
}

void WriteMemberList(Stream &outStream, const std::vector<Member> &members) {
    AccessType lastaccess { AccessType::Private };

    for(auto &member: members) {
        if (member.access != lastaccess) {
            outStream.Write('\n');
            WriteAccessType(outStream, member.access);
            outStream.Write(":\n");
            lastaccess = member.access;
        }
        auto support = GetCPPTypeSupport(member);
        if (!support.empty()) outStream.Write(support, '\n');
        outStream.Write('\t', GetCPPType(member), ' ', member.Name, " { ");
        if (!member.defaultValue.empty()) outStream.Write(member.defaultValue, ' ');
        outStream.Write("};\n");
        
    }
}

void WriteSerializerOutBodyForParent(Stream &outStream, const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    for(auto &parent: obj->parentlist) {
        if (first) {
            first = false;
            outStream.Write("\n\t\t\tSerializerProtocol::StructSerializeOutStart(stream, ");
        }
        else outStream.Write("\n\t\t\tSerializerProtocol::StructSerializeOut(stream, ");
        if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
            outStream.Write(
                "std::make_pair(std::string_view { \"", parent.Name, "\" }, static_cast<const ", parent.Name," *>(this))"
                ");");
        } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
            outStream.Write(
                "std::make_pair(static_cast<uint32_t>(", parent.id,"), static_cast<const ", parent.Name," *>(this))"
                ");");
        } else {
            outStream.Write(
                "static_cast<const ", parent.Name," *>(this)"
                ");");
        }
    }
}

void WriteSerializerOutBodyNonUnion(Stream &outStream, const Member &member, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    if (first) {
        first = false;
        outStream.Write("\n\t\t\tSerializerProtocol::StructSerializeOutStart(stream, ");
    }
    else outStream.Write("\n\t\t\tSerializerProtocol::StructSerializeOut(stream, ");
    if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
        if (member.typeNameList[0].type != ObjectType::Enum) {
            outStream.Write(
                "std::make_pair(std::string_view { \"", member.displayName, "\" }, ", member.Name,")"
                ");");
        } else {
            outStream.Write(
                "std::make_pair(std::string_view { \"", member.displayName, "\" }, ", member.typeNameList[0].declaredNameSpace->GetFullName() , "::to_string(", member.Name,"))"
                ");");
        }
    } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
        outStream.Write(
            "std::make_pair(static_cast<uint32_t>(", member.id,"), ", member.Name,")"
            ");");
    } else {
        outStream.Write(
            member.Name, ");");
    }
}

void WriteSerializerOutBodyUnion(Stream &outStream, const Member &member, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    outStream.Write("\n\t\t\tswitch(", member.Name, "_type) {");
    for(size_t index { 0 }; index < member.typeNameList.size(); ++index) {
        outStream.Write("\n\t\t\t\tcase e_", member.Name, "::", member.typeNameList[index].EnumName, ":");
        if (first) {
            outStream.Write("\n\t\t\t\t\tSerializerProtocol::StructSerializeOutStart(stream, ");
        }
        else outStream.Write("\n\t\t\t\t\tSerializerProtocol::StructSerializeOut(stream, ");
        if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
            outStream.Write("std::make_pair( std::string_view {\"", member.displayName, ":", member.typeNameList[index].EnumName, "\"}, ",
                member.Name, ".", member.typeNameList[index].EnumName, "));",
                "\n\t\t\t\t\tbreak;");
        } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {
            outStream.Write("std::make_tuple(static_cast<uint32_t>(", member.id, "), static_cast<uint32_t>(", index, "), ",
                member.Name, ".", member.typeNameList[index].EnumName, "));",
                "\n\t\t\t\t\tbreak;");
        } else {
            outStream.Write("std::make_pair(static_cast<uint32_t>(", index, "), ",
                member.Name, ".", member.typeNameList[index].EnumName, "));",
                "\n\t\t\t\t\tbreak;");
        }
    }
    first = false;
    outStream.Write("\n\t\t\t}");
}

void WriteSerializerOutBody(Stream &outStream, const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type) {
    bool first = true;
    WriteSerializerOutBodyForParent(outStream, obj, serialize_key_type, first);
    for(auto &member: obj->MemberList) {
        if (member.modifer != Member::Union) {
            WriteSerializerOutBodyNonUnion(outStream, member, serialize_key_type, first);
        }
        else {
            WriteSerializerOutBodyUnion(outStream, member, serialize_key_type, first);
        }
    }
    outStream.Write("\n\t\t\tSerializerProtocol::StructSerializeOutEnd(stream);\n");
}

void WriteSerializerOutBody(Stream &outStream, const Class *obj) {
    outStream.Write(
        "\ttemplate <typename SerializerProtocol>"
        "\n\tvoid SerializeOut(rohit::Stream &stream) const {"
    );
    outStream.Write("\n\t\tif constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::None) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::None);
    outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::Integer);
    outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::String) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::String);
    outStream.Write("\n\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n\t}\n\n");
}


void WriteSerializerInBodyForParent(Stream &outStream, const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    for(auto &parent: obj->parentlist) {
        if (!first) outStream.Write(",\n");
        else first = false;
        if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
            outStream.Write("\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {\"", parent.Name, "\"}, [this] (const rohit::FullStream &stream) { this->", parent.Name, "::template SerializeIn<SerializerProtocol>(stream); }}");
        } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
            outStream.Write("\t\t\t\tstd::pair<uint32_t, std::function<void(const rohit::FullStream &)>> { static_cast<uint32_t>(", parent.id, "), [this] (const rohit::FullStream &stream) { this->", parent.Name, "::template SerializeIn<SerializerProtocol>(stream); }}");
        } else {
            outStream.Write("\t\t\t\t[this] (const rohit::FullStream &stream) { this->", parent.Name, "::template SerializeIn<SerializerProtocol>(stream); }");
        }
    }
}

void WriteSerializerInBodyNonUnion(Stream &outStream, const Member &member, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    if (!first) outStream.Write(",\n");
    else first = false;
    if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
        if (member.typeNameList[0].type != ObjectType::Enum) {
            outStream.Write(
                "\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> {"
                "\n\t\t\t\t\tstd::string_view {\"", member.displayName, "\"}, [this] (const rohit::FullStream &stream) {"
                "\n\t\t\t\t\t\tSerializerProtocol::template SerializeIn<", GetCPPType(member),">(stream, this->", member.Name, ");"
                "\n\t\t\t\t\t}"
                "\n\t\t\t\t}");
        } else {
            outStream.Write(
                "\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> {"
                "\n\t\t\t\t\tstd::string_view {\"", member.displayName, "\"}, [this] (const rohit::FullStream &stream) {"
                "\n\t\t\t\t\t\tstd::string str_", member.Name, " { };"
                "\n\t\t\t\t\t\tSerializerProtocol::template SerializeIn<std::string>(stream, str_", member.Name, ");"
                "\n\t\t\t\t\t\tthis->", member.Name, " = to_", member.typeNameList[0].Name,"(str_", member.Name, ");"
                "\n\t\t\t\t\t}"
                "\n\t\t\t\t}");
        }
    } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
        outStream.Write(
            "\t\t\t\tstd::pair<uint32_t, std::function<void(const rohit::FullStream &)>> {"
            "\n\t\t\t\t\tstatic_cast<uint32_t>(", member.id, "), [this] (const rohit::FullStream &stream) {"
            "\n\t\t\t\t\t\tSerializerProtocol::template SerializeIn<", GetCPPType(member),">(stream, this->", member.Name, ");"
            "\n\t\t\t\t\t}"
            "\n\t\t\t\t}");
    } else {
        outStream.Write(
            "\t\t\t\tstatic_cast<std::function<void(const rohit::FullStream &)>>([this] (const rohit::FullStream &stream) {"
            "\n\t\t\t\t\t\tSerializerProtocol::template SerializeIn<", GetCPPType(member),">(stream, this->", member.Name, ");"
            "\n\t\t\t\t\t}"
            "\n\t\t\t\t)");
    }
} // WriteSerializerInBodyNonUnion

void WriteSerializerInBodyUnionKeyNonString(Stream &outStream, const Member &member, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    if (!first) outStream.Write(",\n");
    else first = false;
    if (serialize_key_type == rohit::serializer::SerializeKeyType::None) {
        outStream.Write(
            "\t\t\t\t\tstatic_cast<std::function<void(const rohit::FullStream &)>> (\n\t\t\t\t\t"
        );
    } else {
        outStream.Write(
            "\t\t\t\tstd::pair<uint32_t, std::function<void(const rohit::FullStream &)>> {"
            "\n\t\t\t\tstatic_cast<uint32_t>(", member.id, "), "
        );
    }
    outStream.Write(
        "[this] (const rohit::FullStream &stream) {\n"
        "\t\t\t\t\t\tuint32_t ", member.Name, "_type { };\n"
        "\t\t\t\t\t\t", member.Name, "_type = SerializerProtocol::SerializeInVariable(stream);\n"
        "\t\t\t\t\t\tthis->", member.Name, "_type = static_cast<e_", member.Name, ">(", member.Name, "_type);\n"
        "\t\t\t\t\t\tswitch(this->", member.Name, "_type) {\n"
    );
    for(size_t index { 0 }; index < member.typeNameList.size(); ++index) {
        outStream.Write(
            "\t\t\t\t\t\t\tcase e_", member.Name, "::", member.typeNameList[index].EnumName, ":\n"
            "\t\t\t\t\t\t\t\tSerializerProtocol::template SerializeIn(stream, this->", member.Name,".", member.typeNameList[index].EnumName, ");\n"
            "\t\t\t\t\t\t\t\tbreak;\n"
        );
    }
    outStream.Write(
        "\t\t\t\t\t\t}\n"
        "\t\t\t\t\t}\n"
    );
    if (serialize_key_type == rohit::serializer::SerializeKeyType::None) {
        outStream.Write(
            "\t\t\t\t)"
        );
    } else {
        outStream.Write(
            "\t\t\t\t}"
        );
    }
} // WriteSerializerInBodyUnionKeyNonString

void WriteSerializerInBodyUnion(Stream &outStream, const Member &member, const rohit::serializer::SerializeKeyType serialize_key_type, bool &first) {
    if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
        for(auto &typeName: member.typeNameList) {
            if (!first) outStream.Write(",\n");
            else first = false;
            outStream.Write(
                "\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> {"
                "\n\t\t\t\t\tstd::string_view {\"", member.displayName, ":", typeName.EnumName, "\"}, [this] (const rohit::FullStream &stream) {"
                "\n\t\t\t\t\t\tthis->", member.Name, "_type = e_", member.Name, "::", typeName.EnumName, ";",
                "\n\t\t\t\t\t\tSerializerProtocol::template SerializeIn(stream, this->", member.Name, ".", typeName.EnumName, ");"
                "\n\t\t\t\t\t}"
                "\n\t\t\t\t}");
        }
    } else {
        WriteSerializerInBodyUnionKeyNonString(outStream, member, serialize_key_type, first);
    }
} // WriteSerializerInBodyUnion


void WriteSerializerInBody(Stream &outStream, const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type) {
    outStream.Write(
        "\t\t\tSerializerProtocol::StructSerializeIn(\n"
        "\t\t\t\tstream,\n"
    );
    bool first = true;
    WriteSerializerInBodyForParent(outStream, obj, serialize_key_type, first);
    if (!first) outStream.Write(",\n");
    first = true;
    for(auto &member: obj->MemberList) {
        if (member.modifer != Member::Union) {
            WriteSerializerInBodyNonUnion(outStream, member, serialize_key_type, first);
        } else if (member.typeNameList.size()) {
            WriteSerializerInBodyUnion(outStream, member, serialize_key_type, first);
        }
    }
    outStream.Write("\n\t\t\t);");
}

void WriteSerializerInBody(Stream &outStream, const Class *obj) {
    outStream.Write(
        "\ttemplate <typename SerializerProtocol>\n"
        "\tvoid SerializeIn(const rohit::FullStream &stream) {"
    );

    outStream.Write("\n\t\tif constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::None) {\n");
    WriteSerializerInBody(outStream, obj, rohit::serializer::SerializeKeyType::None);
    outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {\n");
    WriteSerializerInBody(outStream, obj, rohit::serializer::SerializeKeyType::Integer);
    outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::String) {\n");
    WriteSerializerInBody(outStream, obj, rohit::serializer::SerializeKeyType::String);
    outStream.Write("\n\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n\t}\n");
}

void WriteSerializer(Stream &outStream, const Class *obj) {
    WriteSerializerOutBody(outStream, obj);
    WriteSerializerInBody(outStream, obj);
}

void WriteClass(Stream &outStream, const Class *obj) {
    if ((obj->attributes & ClassAtributes::Packed) == ClassAtributes::Packed)
        outStream.Write("class __attribute__ ((__packed__)) ", obj->Name);
    else outStream.Write("class ", obj->Name);

    if (!obj->parentlist.empty()) {
        outStream.Write(" : ");
        WriteParentList(outStream, obj->parentlist);
    }

    outStream.Write(" {\n");
    WriteMemberList(outStream, obj->MemberList);

    outStream.Write('\n');
    WriteSerializer(outStream, obj);

    outStream.Write("}; // class ", obj->Name, "\n\n");
}

void WriteEnum(Stream &outStream, const Enum *enumptr) {
    outStream.Write("enum class ", enumptr->Name, " {\n");
    for(auto &enumName: enumptr->enumNameList) {
        outStream.Write("\t", enumName, ",\n");
    }
    outStream.Write("}; // enum class ", enumptr->Name, "\n\n");

    outStream.Write("constexpr inline std::string to_string(const ", enumptr->Name, " v) {\n");
    outStream.Write("\tswitch(v) {\n");
    for(auto &enumName: enumptr->enumNameList) {
        outStream.Write("\t\tcase ", enumptr->Name, "::", enumName, ": return {\"", enumName, "\"};\n");
    }
    outStream.Write("\t\tdefault: throw std::runtime_error(\"Bad Enum Name\");\n");
    outStream.Write("\t}\n");
    outStream.Write("};\n\n");

    outStream.Write("constexpr inline ", enumptr->Name, " to_", enumptr->Name, "(const auto &v) {\n");
    outStream.Write("\tswitch(rohit::Hash(v)) {\n");
    for(auto &enumName: enumptr->enumNameList) {
        outStream.Write("\t\tcase rohit::Hash(\"", enumName, "\"): return ", enumptr->Name, "::", enumName, ";\n");
    }
    outStream.Write("\t\tdefault: throw std::runtime_error(\"Bad Enum Name\");\n");
    outStream.Write("\t}\n");
    outStream.Write("};\n\n");
}

void WriteStatementList(Stream &outStream, const std::vector<std::unique_ptr<Base>> &statementlist);

void WriteNamespace(Stream &outStream, const Namespace *namespaceptr) {
    std::string completename = namespaceptr->Name;
    while (namespaceptr->statementlist.size() == 1 && namespaceptr->statementlist.back()->type == ObjectType::Namespace) {
        namespaceptr = dynamic_cast<Namespace *>(namespaceptr->statementlist.back().get());
        completename += "::";
        completename += namespaceptr->Name;
    }
    outStream.Write("namespace ", completename, " {\n");
    WriteStatementList(outStream, namespaceptr->statementlist);
    outStream.Write("} // namespace ", completename,"\n\n");
}

void WriteStatementList(Stream &outStream, const std::vector<std::unique_ptr<Base>> &statementlist) {
    if (statementlist.empty()) return;

    for(auto &statement: statementlist) {
        switch (statement->type)
        {
        case ObjectType::Namespace:
            WriteNamespace(outStream, dynamic_cast<const Namespace *>(statement.get()));
            break;

        case ObjectType::Class:
            WriteClass(outStream, dynamic_cast<const Class *>(statement.get()));
            break;

        case ObjectType::Enum:
            WriteEnum(outStream, dynamic_cast<const Enum *>(statement.get()));
            break;
        
        default:
            break;
        }
    }
}

void Write(Stream &outStream, std::vector<std::unique_ptr<Base>> &statementlist) {
    outStream.Write(
        "/////////////////////////////////////////////////////////\n"
        "// This is auto genarated file using serializer. Must  //\n"
        "// not be manually edited. For more information refer  //\n"
        "// to https://github.com/rohit-singh-gautam/Serializer //\n"
        "/////////////////////////////////////////////////////////\n"
        "\n"
        "#pragma once\n"
        "#include <rohit/serializer.h>\n\n"
    );
    WriteStatementList(outStream, statementlist);
}

} // namespace rohit::serializer::Writer::CPP