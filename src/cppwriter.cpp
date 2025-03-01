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
            outStream.Write("\n\t\t\tserializerProtocol.StructSerializeOutStart(");
        }
        else outStream.Write("\n\t\t\tserializerProtocol.StructSerializeOut(");
        if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
            outStream.Write(
                "std::make_pair(std::string_view { \"", parent.displayName, "\" }, static_cast<const ", parent.Name," *>(this))"
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
        outStream.Write("\n\t\t\tserializerProtocol.StructSerializeOutStart(");
    }
    else outStream.Write("\n\t\t\tserializerProtocol.StructSerializeOut(");
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
            outStream.Write("\n\t\t\t\t\tserializerProtocol.StructSerializeOutStart(");
        }
        else outStream.Write("\n\t\t\t\t\tserializerProtocol.StructSerializeOut(");
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
    outStream.Write("\n\t\t\tserializerProtocol.StructSerializeOutEnd();");
}

void WriteSerializerOutBody(Stream &outStream, const Class *obj) {
    outStream.Write(
        "\ttemplate <typename SerializeOutProtocol>\n"
        "\tvoid SerializeOut(SerializeOutProtocol &serializerProtocol) const {"
    );
    outStream.Write("\n\t\tif constexpr (serializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::None) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::None);
    outStream.Write("\n\t\t} else if constexpr (serializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::Integer);
    outStream.Write("\n\t\t} else if constexpr (serializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::String) {");
    WriteSerializerOutBody(outStream, obj, rohit::serializer::SerializeKeyType::String);
    outStream.Write("\n\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n\t}\n\n");
    outStream.Write(
        "\ttemplate <template<rohit::serializer::SerializeType> class SerializerProtocol>\n"
        "\tvoid SerializeOut(rohit::Stream &stream) const {\n"
        "\t\tusing SerializerOutProtocol = SerializerProtocol<rohit::serializer::SerializeType::Out>;\n"
        "\t\tSerializerOutProtocol serializerProtocol { stream };\n"
        "\t\tSerializeOut(serializerProtocol);\n"
        "\t}\n\n"
    );
}

void WriteSerializerInBodyForParentKeyNone(Stream &outStream, const Class *obj) {
    for(auto &parent: obj->parentlist) {
        outStream.Write("\t\t\tthis->", parent.Name, "::SerializeIn(serializerProtocol);\n");
    }
}

void WriteSerializerInBodyForParentKeyInteger(Stream &outStream, const Class *obj) {
    for(auto &parent: obj->parentlist) {
        outStream.Write(
            "\t\t\tcase ", parent.id, ":\n"
            "\t\t\t\tthis->", parent.Name, "::SerializeIn(serializerProtocol);\n"
            "\t\t\t\tbreak;\n");
    }
} // WriteSerializerInBodyForParentKeyInteger

void WriteSerializerInBodyForParentKeyString(Stream &outStream, const Class *obj) {
    for(auto &parent: obj->parentlist) {
        outStream.Write(
            "\t\t\tcase rohit::Hash(\"", parent.displayName, "\"):\n"
            "\t\t\t\tthis->", parent.Name, "::SerializeIn(serializerProtocol);\n"
            "\t\t\t\tbreak;\n");
    }
} // WriteSerializerInBodyForParentKeyString

void WriteSerializerInBodyNonUnionKeyString(Stream &outStream, const Member &member) {
    if (member.typeNameList[0].type != ObjectType::Enum) {
        outStream.Write(
            "\t\t\tcase rohit::Hash(\"", member.displayName, "\"):\n"
            "\t\t\t\tserializerProtocol.template SerializeIn<", GetCPPType(member),">(this->", member.Name, ");\n"
            "\t\t\t\tbreak;\n");
    } else {
        outStream.Write(
            "\t\t\tcase rohit::Hash(\"", member.displayName, "\"): {\n"
            "\t\t\t\tstd::string str_", member.Name, " { };\n"
            "\t\t\t\tserializerProtocol.template SerializeIn<std::string>(str_", member.Name, ");\n"
            "\t\t\t\tthis->", member.Name, " = to_", member.typeNameList[0].Name,"(str_", member.Name, ");\n"
            "\t\t\t\tbreak;\n"
            "\t\t\t}\n");
    }
} // WriteSerializerInBodyNonUnionKeyString

void WriteSerializerInBodyNonUnionKeyInteger(Stream &outStream, const Member &member) {
    outStream.Write(
        "\t\t\tcase ", member.id, ":\n"
        "\t\t\t\tserializerProtocol.template SerializeIn<", GetCPPType(member),">(this->", member.Name, ");\n"
        "\t\t\t\tbreak;\n");
} // WriteSerializerInBodyNonUnionKeyInteger

void WriteSerializerInBodyNonUnionKeyNone(Stream &outStream, const Member &member) {
    outStream.Write(
        "\t\t\tserializerProtocol.template SerializeIn<", GetCPPType(member),">(this->", member.Name, ");\n");
} // WriteSerializerInBodyNonUnionKeyNone


void WriteSerializerInBodyUnionKeyInteger(Stream &outStream, const Member &member) {
    outStream.Write(
        "\t\t\tcase ", member.id, ": {\n"
        "\t\t\t\tthis->", member.Name, "_type = static_cast<e_", member.Name, ">(serializerProtocol.SerializeInVariable());\n"
        "\t\t\t\tswitch(this->", member.Name, "_type) {\n"
    );

    for(size_t index { 0 }; index < member.typeNameList.size(); ++index) {
        outStream.Write(
            "\t\t\t\t\tcase e_", member.Name, "::", member.typeNameList[index].EnumName, ":\n"
            "\t\t\t\t\t\tserializerProtocol.SerializeIn(this->", member.Name,".", member.typeNameList[index].EnumName, ");\n"
            "\t\t\t\t\t\tbreak;\n"
        );
    }
    outStream.Write(
        "\t\t\t\t\tdefault:\n"
        "\t\t\t\t\t\tthrow rohit::serializer::exception::KeyNotFound {serializerProtocol.GetStream(), \"Bad Enum Name\"};\n"
        "\t\t\t\t}\n"
        "\t\t\t\tbreak;\n"
        "\t\t\t}\n"
    );
} // WriteSerializerInBodyUnionKeyInteger

void WriteSerializerInBodyUnionKeyNone(Stream &outStream, const Member &member) {
    outStream.Write(
        "\t\t\tuint32_t ", member.Name, "_type_local { };\n"
        "\t\t\t", member.Name, "_type_local = serializerProtocol.SerializeInVariable();\n"
        "\t\t\tthis->", member.Name, "_type = static_cast<e_", member.Name, ">(", member.Name, "_type_local);\n"
        "\t\t\tswitch(this->", member.Name, "_type) {\n"
    );
    for(size_t index { 0 }; index < member.typeNameList.size(); ++index) {
        outStream.Write(
            "\t\t\t\tcase e_", member.Name, "::", member.typeNameList[index].EnumName, ":\n"
            "\t\t\t\t\tserializerProtocol.SerializeIn(this->", member.Name,".", member.typeNameList[index].EnumName, ");\n"
            "\t\t\t\t\tbreak;\n"
        );
    }
    outStream.Write("\t\t\t}\n");
} // WriteSerializerInBodyUnionKeyNone


void WriteSerializerInBodyUnionKeyString(Stream &outStream, const Member &member) {
    for(auto &typeName: member.typeNameList) {
        outStream.Write(
            "\t\t\tcase rohit::Hash(\"", member.displayName, ":", typeName.EnumName, "\"):\n"
            "\t\t\t\tthis->", member.Name, "_type = e_", member.Name, "::", typeName.EnumName, ";\n",
            "\t\t\t\tserializerProtocol.SerializeIn(this->", member.Name, ".", typeName.EnumName, ");\n"
            "\t\t\t\tbreak;\n");
    }
} // WriteSerializerInBodyUnionString

void WriteSerializerInBodyKeyNone(Stream &outStream, const Class *obj) {
    WriteSerializerInBodyForParentKeyNone(outStream, obj);
    for(auto &member: obj->MemberList) {
        if (member.modifer != Member::Union) {
            WriteSerializerInBodyNonUnionKeyNone(outStream, member);
        } else if (member.typeNameList.size()) {
            WriteSerializerInBodyUnionKeyNone(outStream, member);
        }
    }
} // WriteSerializerInBodyKeyNone

void WriteSerializerInBodyWithKeyInteger(Stream &outStream, const Class *obj) {
    outStream.Write(
        "\tvoid SerializeInMemberByIdentifier(auto &serializerProtocol, const uint32_t identifier) {\n"
        "\t\tswitch(identifier) {\n"
    );

    WriteSerializerInBodyForParentKeyInteger(outStream, obj);

    for(auto &member: obj->MemberList) {
        if (member.modifer != Member::Union) {
            WriteSerializerInBodyNonUnionKeyInteger(outStream, member);
        } else if (member.typeNameList.size()) {
            WriteSerializerInBodyUnionKeyInteger(outStream, member);
        }
    }
    
    outStream.Write(
        "\t\t\tdefault:\n"
        "\t\t\t\tthrow rohit::serializer::exception::KeyNotFound {serializerProtocol.GetStream(), \"Bad Member Identifier\"};\n"
        "\t\t}\n"
        "\t}\n\n");
}

void WriteSerializerInBodyWithKeyString(Stream &outStream, const Class *obj) {
    outStream.Write(
        "\tvoid SerializeInMemberByName(auto &serializerProtocol, const std::string_view &name) {\n"
        "\t\tswitch(rohit::Hash(name)) {\n"
    );

    WriteSerializerInBodyForParentKeyString(outStream, obj);

    for(auto &member: obj->MemberList) {
        if (member.modifer != Member::Union) {
            WriteSerializerInBodyNonUnionKeyString(outStream, member);
        } else if (member.typeNameList.size()) {
            WriteSerializerInBodyUnionKeyString(outStream, member);
        }
    }
    
    outStream.Write(
        "\t\t\tdefault:\n"
        "\t\t\t\tthrow rohit::serializer::exception::KeyNotFound {serializerProtocol.GetStream(), \"Bad Member Name\"};\n"
        "\t\t}\n"
        "\t}\n\n");
}

void WriteSerializerInBody(Stream &outStream, const Class *obj) {
    WriteSerializerInBodyWithKeyInteger(outStream, obj);
    WriteSerializerInBodyWithKeyString(outStream, obj);
    outStream.Write(
        "\ttemplate <typename SerializeInProtocol>\n"
        "\tvoid SerializeIn(SerializeInProtocol &serializerProtocol) {"
    );
    outStream.Write("\n\t\tif constexpr (serializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::None) {\n");
    WriteSerializerInBodyKeyNone(outStream, obj);
    outStream.Write(
        "\t\t} else if constexpr (serializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::Integer ||\n"
        "\t\t\t\t\t\tserializerProtocol.serialize_key_type == rohit::serializer::SerializeKeyType::String) {\n"
        "\t\t\tserializerProtocol.template StructSerializeIn<", obj->Name,">(this);\n"
        "\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n"
        "\t}\n\n"
        "\ttemplate <template<rohit::serializer::SerializeType> class SerializerProtocol>\n"
        "\tvoid SerializeIn(const rohit::Stream &stream) {\n"
        "\t\tusing SerializerInProtocol = SerializerProtocol<rohit::serializer::SerializeType::In>;\n"
        "\t\tSerializerInProtocol serializerProtocol { stream };\n"
        "\t\tSerializeIn(serializerProtocol);\n"
        "\t}\n\t"
    );
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