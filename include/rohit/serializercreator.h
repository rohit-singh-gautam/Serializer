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

#pragma once
#include <rohit/stream.h>
#include <string.h>
#include <stack>
#include <vector>
#include <functional>
#include <memory>
#include <filesystem>
#include <iostream>
#include <exception>
#include <rohit/serializer.h>

// Gramar
// STRUCTFILE: statementlist
// namespace: "namespace" space hirerchical_identifier space '{' space statementlist space '}'
// statementlist: statement | statement space statementlist
// statement: namespace | class
// class: classheader classbody | classextendedheader space classbody
// classextendedheader: classheader space ':' space classparentlist
// classheader: "class" space identifier space classattributelist
// classparent: accesstypeidentifier space  hirerchical_identifier
// classparentlist: classparent | classparent space ',' space classparentlist
// classattributelist: identifer | identifier space classattributelist
// classbody: '{' space memberlist space '}'
// memberlist: member | member space memberlist
// member: accesstypeidentifier space typeidentifier space identifier space ';'
// hirerchical_identifier: identifier | identifier "::" hirerchical_identifier
// accesstypeidentifier: "private" | "protected" | "public"
// typeidentifier: identifier
// space: onespace | onespace space
// onespace: ' ' | '\t' | '\n' | '\r'

namespace rohit {
namespace exception {
class BadIdentifier : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadAccessType : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadObjectType : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadClassMember : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadClass : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadNamespace : public BaseParser {
public:
    using BaseParser::BaseParser;
};

} // namespace exception

void tolower_inplace(std::string &value) {
    for(auto &ch: value) {
        if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
    }
}

enum class AccessType {
    Error,
    Private,
    Protected,
    Public
};

enum class ObjectType {
    Namespace,
    Class
};

enum class ClassAtributes : uint8_t {
    None = 0x00,
    Packed = 0x01
};

ClassAtributes &operator|=(ClassAtributes &lhs, const ClassAtributes &rhs) {
    using T = std::underlying_type_t<ClassAtributes>;
    auto ulhs = static_cast<T>(lhs);
    auto urhs = static_cast<T>(rhs);
    lhs = static_cast<ClassAtributes>(ulhs | urhs);
    return lhs;
}

ClassAtributes operator&(const ClassAtributes &lhs, const ClassAtributes &rhs) {
    using T = std::underlying_type_t<ClassAtributes>;
    auto ulhs = static_cast<T>(lhs);
    auto urhs = static_cast<T>(rhs);
    return static_cast<ClassAtributes>(ulhs & urhs);
}

struct Member {
    enum ModifierType {
        none,
        array,
        map,
        Union
    };
    AccessType access;
    ModifierType modifer;
    std::vector<std::string> enumNameList;
    std::vector<std::string> typeNameList;
    std::string Name;
    uint32_t id;
    std::string Key; // Optional parameter

    bool operator==(const Member &rhs) const { return access == rhs.access && modifer == rhs.modifer && enumNameList == rhs.enumNameList && typeNameList == rhs.typeNameList && Name == rhs.Name; }
};

struct Namespace;

struct Base {
    ObjectType type;
    std::string Name;
    Namespace *parentNamespace { nullptr };
    Base(ObjectType type, std::string &&Name, Namespace *parentNamespace) : type { type }, Name { std::move(Name) },
        parentNamespace { parentNamespace } { }
    Base(Base &&base) : type { base.type }, Name { std::move(base.Name) },
        parentNamespace { base.parentNamespace } { }
    virtual ~Base() = default;
    Base(const Base &base) = delete;
    Base &operator=(const Base &) = delete; 
};

struct Class;

struct Parent {
    AccessType access { };
    std::string Name { };
    uint32_t id { };
    Namespace *currentNameSpace { };
    Class *parentClass { nullptr }; // This will be filled in later
};

// TODO: Verify parent and its namespace
struct Class : public Base {
    ClassAtributes attributes { };
    std::vector<Parent> parentlist;
    std::vector<Member> MemberList { };
    Class(ObjectType type, std::string &&Name,
        Namespace *parentNamespace, ClassAtributes attributes,
        std::vector<Parent> &&parentlist) : Base(type, std::move(Name), parentNamespace),
            attributes { attributes }, parentlist { std::move(parentlist) } { }
    Class(Class &&rhs) : Base { std::move(rhs) },
        attributes { rhs.attributes }, parentlist { std::move(rhs.parentlist) },
        MemberList { std::move(rhs.MemberList) } { }
    Class(const Class&) = delete;
    Class &operator=(const Class&) = delete;
};

struct Namespace : public Base {
    std::vector<std::unique_ptr<Base>> statementlist { };
    Namespace(ObjectType type, std::string &&Name,
        Namespace *parentNamespace) :
            Base { type, std::move(Name), parentNamespace }
                {}
};

class SerializerCreator {
#ifdef ENABLE_GTEST
public:
    FRIEND_TEST(SerializeParser, Identifier);
    FRIEND_TEST(SerializeParser, HierarchicalIdentifier);
    FRIEND_TEST(SerializeParser, SpaceSeparatedIdentifier);
    FRIEND_TEST(SerializeParser, AccessType);
    FRIEND_TEST(SerializeParser, Member);
    FRIEND_TEST(SerializeParser, ClassBody);
    FRIEND_TEST(SerializeParser, CompleteStruct);
#else
private:
#endif

    // For Optimization inStream must be null terminated
    const FullStream &inStream;
    FullStreamAutoAlloc &outStream;

    class Namespace *CurrentNamespace { nullptr };

    static constexpr bool IsWhiteSpace(const char val) noexcept { return val == ' ' || val == '\t' || val == '\n' || val == '\r'; }
    static constexpr bool IsNumber(const char val) noexcept { return val >= '0' && val <= '9'; }
    static constexpr bool IsSmallAlphabet(const char val) noexcept { return val >= 'a' && val <= 'z'; }
    static constexpr bool IsCapitalAlphabet(const char val) noexcept { return val >= 'A' && val <= 'Z'; }
    static constexpr bool IsFirstIdentifier(const char val) noexcept { return IsCapitalAlphabet(val) || IsSmallAlphabet(val) || val == '_'; }
    static constexpr bool IsIdentifier(const char val) noexcept { return IsNumber(val) || IsCapitalAlphabet(val) || IsSmallAlphabet(val) || val == '_'; }

    bool IsWhiteSpace() const { return IsWhiteSpace(*inStream); }
    bool IsNumber() const { return IsNumber(*inStream); }
    bool IsSmallAlphabet() const { return IsSmallAlphabet(*inStream); }
    bool IsCapitalAlphabet() const { return IsCapitalAlphabet(*inStream); }
    bool IsFirstIdentifier() const { return IsFirstIdentifier(*inStream); }
    bool IsIdentifier() const { return IsIdentifier(*inStream); }
    void SkipWhiteSpace() const { while(IsWhiteSpace()) ++inStream; }
    void SkipWhiteSpaceAndComment() const {
        for(;;) {
            auto ch = *inStream;
            if (IsWhiteSpace(ch)) {
                ++inStream;
                continue;
            }
            if (ch == '/') {
                ++inStream;
                auto ch1 = *inStream;
                if (ch1 == '/') {
                    // Skip till new line
                    ++inStream;
                    while(*inStream && *inStream != '\n') ++inStream;
                    continue;
                }
                if (ch1 == '*') {
                    // Skip till */
                    ++inStream;
                    for(;;) {
                        const auto ch = *inStream;
                        if (ch == '*') {
                            ++inStream;
                            const auto ch1 = *inStream;
                            if (ch1 == '/') {
                                ++inStream;
                                break;
                            }
                        }
                        ++inStream;
                    }
                    continue;
                }
            }
            break;
        }
    }

    constexpr void check_in(char value) const {
        if (*inStream != value) {
            std::string errstr {"Expected: "};
            errstr.push_back(value);
            errstr += ", Found: ";
            errstr.push_back(*inStream);
            throw exception::BadClass { inStream, errstr };
        }
        ++inStream;
    }

    std::string ParseIdentifier() {
        std::string identifier { };
        auto ch = *inStream;
        if (!IsFirstIdentifier(ch)) {
            std::string errstr { "Identifier can start with '_' or alphabet only it cannot start with: "};
            errstr.push_back(ch);
            throw exception::BadIdentifier { inStream, errstr };
        }
        identifier.push_back(ch);
        ++inStream;
        while(IsIdentifier(*inStream)) { identifier.push_back(*inStream); ++inStream; }
        return identifier;
    }

    std::string ParseHierarchicalIdentifier() {
        std::string identifier { };
        while(true) {
            if (!IsFirstIdentifier(*inStream)) {
                std::string errstr { "Identifier cannot start with " };
                errstr.push_back(*inStream);
                throw exception::BadIdentifier { inStream, errstr };
            }
            identifier.push_back(*inStream);
            ++inStream;
            while(IsIdentifier(*inStream)) { identifier.push_back(*inStream); ++inStream; }
            if (inStream.remaining_buffer() < 2) break;
            if (*inStream != ':') break;
            ++inStream;
            if (*inStream != ':') throw exception::BadIdentifier { inStream, { "Namespace and identifier must be separated by '::', only one ':' is unsupported " } };
            ++inStream;
            identifier.push_back(':');
            identifier.push_back(':');
            if (inStream.full()) throw exception::BadIdentifier { inStream, { "Atleast one characted is require for identifier" } };

        }
        return identifier;
    }

    void SpaceSeparatedIdentifier(std::function<void(std::string &&)> fn) {
        if (!IsFirstIdentifier()) return;
        while(true) {
            auto identifier = ParseIdentifier();
            fn(std::move(identifier));
            if (!IsWhiteSpace()) break;
            SkipWhiteSpaceAndComment();
            if (!IsFirstIdentifier()) break;
        }
        return;
    }

    AccessType ParseAccessType() {
        auto accessType = ParseIdentifier();
        if (accessType == "public") return AccessType::Public;
        if (accessType == "protected") return AccessType::Protected;
        if (accessType == "private") return AccessType::Private;
        std::string errorstr { "Bad access type it must be one of 'public', 'protected' or 'private' case sensitive. Unknown access type: " };
        errorstr += accessType;
        throw exception::BadAccessType { inStream, errorstr };
    }

    // Returns true if present.
    auto ParseMemberModifier(const std::string &type) {
        if (type == "array") {
            return Member::array;
        }
        else if (type == "map") {
            return Member::map;
        }
        else if (type == "union") {
            return Member::Union;
        }
        return Member::none;
    }

    Member ParseMember(const uint32_t id) {
        auto accesstype = ParseAccessType();
        SkipWhiteSpaceAndComment();
        auto nextid = ParseHierarchicalIdentifier();
        std::vector<std::string> enumNameList { };
        std::vector<std::string> typeNameList { };
        auto membermodifier = ParseMemberModifier(nextid);
        std::string key { };
        if (membermodifier == Member::none) {
            typeNameList.push_back(nextid);
        } else if (membermodifier == Member::array) {
            SkipWhiteSpaceAndComment();
            auto typeName = ParseHierarchicalIdentifier();
            typeNameList.push_back(typeName);
        } else if(membermodifier == Member::map) {
            SkipWhiteSpaceAndComment();
            check_in('(');
            SkipWhiteSpaceAndComment();
            key = ParseHierarchicalIdentifier();
            check_in(')');
            SkipWhiteSpaceAndComment();
            auto typeName = ParseHierarchicalIdentifier();
            typeNameList.push_back(typeName);
        } else if (membermodifier == Member::Union) {
            SkipWhiteSpaceAndComment();
            check_in('(');
            int count { 0 };
            while(true) {
                SkipWhiteSpaceAndComment();
                auto typeName = ParseHierarchicalIdentifier();
                typeNameList.push_back(typeName);
                SkipWhiteSpaceAndComment();
                if (*inStream == '=') {
                    ++inStream;
                    SkipWhiteSpaceAndComment();
                    auto enumName = ParseIdentifier();
                    enumNameList.push_back(enumName);
                    SkipWhiteSpaceAndComment();
                } else {
                    enumNameList.push_back("e_" + std::to_string(count));
                    ++count;
                }
                if (*inStream != ',') break;
                ++inStream;
            }
            check_in(')');
        }
        SkipWhiteSpaceAndComment();
        auto name = ParseIdentifier();
        SkipWhiteSpaceAndComment();
        check_in(';');
        return { accesstype, membermodifier, enumNameList, typeNameList, name, id, key };
    }

    ObjectType ParseObjectType() {
        auto objectType = ParseIdentifier();
        if (objectType == "class") return ObjectType::Class;
        if (objectType == "namespace") return ObjectType::Namespace;
        std::string errorstr { "Bad identifier type it must be one of 'class' or 'namespace' case sensitive. Unknown access type: " };
        errorstr += objectType;
        throw exception::BadObjectType { inStream, errorstr };
    }

    void ParseClassBody(Class &obj, uint32_t &id) {
        if (*inStream != '{' ) {
            std::string errorstr { "Expecting '{' found: "};
            errorstr += *inStream;
            throw exception::BadClass { inStream, errorstr };
        }
        ++inStream;
        SkipWhiteSpaceAndComment();
        while(*inStream != '}') {
            auto member = ParseMember(id++);
            obj.MemberList.push_back(std::move(member));
            SkipWhiteSpaceAndComment();
        }
        ++inStream;
        if (*inStream == ';') throw exception::BadClass { inStream, {"Semicolon is not expected at the end of a class"} };
    }

    Parent ParseParent(Namespace *CurrentNamespace, const uint32_t id) {
        auto access = ParseAccessType();
        SkipWhiteSpaceAndComment();
        auto fullname = ParseHierarchicalIdentifier();
        return { access, fullname, id, CurrentNamespace, nullptr };
    }

    std::vector<Parent> ParseParentList(Namespace *CurrentNamespace, uint32_t &id) {
        std::vector<Parent> ret { };
        if (!IsFirstIdentifier()) return ret;
        while(true) {
            // TODO: if ParseParent return type comes a rvalue
            ret.push_back( ParseParent(CurrentNamespace, id++) );
            SkipWhiteSpaceAndComment();
            if (*inStream != ',') break;
            ++inStream;
            SkipWhiteSpaceAndComment();
        }

        return ret;
    }

    Class ParseClassHeader(Namespace *CurrentNamespace, uint32_t &id) {
        // Object type is already parsed
        SkipWhiteSpaceAndComment();
        auto name = ParseIdentifier();
        SkipWhiteSpaceAndComment();
        auto attributes { ClassAtributes::None };
        SpaceSeparatedIdentifier([&attributes](std::string &&value) { 
            if (value == "packed") attributes |= ClassAtributes::Packed;
        });
        std::vector<Parent> parentlist;
        // At this point all whitespace is skipped
        if (*inStream == ':') {
            ++inStream;
            SkipWhiteSpaceAndComment();
            auto parentlisttemp = ParseParentList(CurrentNamespace, id);
            std::swap(parentlist, parentlisttemp);
        }

        return {ObjectType::Class, std::move(name), CurrentNamespace, attributes, std::move(parentlist)};
    }

    Class ParseClass(Namespace *CurrentNamespace) {
        uint32_t id { 1 };
        Class obj = ParseClassHeader(CurrentNamespace, id);
        // At this point all whitespace is skipped
        ParseClassBody(obj, id);
        return obj;
    }

    std::unique_ptr<Namespace> ParseNameSpace(Namespace *parentNamespace);
    
    std::vector<std::unique_ptr<Base>> ParseStatementList(Namespace *parentNamespace) {
        std::vector<std::unique_ptr<Base>> statementlist { };
        while(true) {
            SkipWhiteSpaceAndComment();
            if (inStream.full() || *inStream == '}') break;
            auto objectType = ParseObjectType();
            if (objectType == ObjectType::Class) {
                auto obj = new Class { ParseClass(parentNamespace) };
                statementlist.emplace_back(obj);
            } else if (objectType == ObjectType::Namespace) {
                statementlist.emplace_back( ParseNameSpace(parentNamespace) );
            } else {
                std::string errorstr { "Bad identifier type it must be one of 'class' or 'namespace' case sensitive." };
                throw exception::BadObjectType { inStream, errorstr };
            }
        }

        return statementlist;
    }

    void Write(const AccessType access) {
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

    constexpr const std::string &GetCPPType(const std::string &type) {
        static std::unordered_map<std::string, std::string> CPPTypeMap {
            {"char", "char"},
            {"int8", "int8_t"},
            {"int16", "int16_t"},
            {"int8", "int8_t"},
            {"int32", "int32_t"},
            {"int64", "int64_t"},
            {"uint8", "uint8_t"},
            {"uint16", "uint16_t"},
            {"uint32", "uint32_t"},
            {"uint64", "uint64_t"},
            {"float", "float"},
            {"double", "double"},
            {"bool", "bool"},
            {"string", "std::string"}
        };
        
        auto itr = CPPTypeMap.find(type);
        if (itr != std::end(CPPTypeMap)) return CPPTypeMap[type];
        else return type;
    }

    constexpr const std::string GetCPPTypeSupport(const Member &member) {
        switch(member.modifer) {
        default:
        case Member::none:
            return { };
        case Member::array:
            return { };
        case Member::map:
            return { };
        case Member::Union: {
                std::string retUnion { "\tenum class e_" };
                retUnion += member.Name;
                retUnion += " {\n";
                for(auto &enumName: member.enumNameList) {
                    retUnion += "\t\t" + enumName + ",\n";
                }
                retUnion += "\t};\n\tunion u_" + member.Name + " {\n";
                for(size_t index { 0 }; index < member.enumNameList.size(); ++index) {
                    retUnion += "\t\t" + member.typeNameList[index] + " " + member.enumNameList[index] + ";\n";
                }
                retUnion += "\t};\n\tstatic std::string to_string(const e_" + member.Name + " v) {\n\t\tswitch(v) {\n\t\t\tdefault:";
                for(auto &enumName: member.enumNameList) {
                    retUnion += "\n\t\t\tcase e_" + member.Name + "::" + enumName + ": return {\"" + enumName + "\"}; ";
                }
                retUnion += "\n\t\t}\n\t};\n\tstatic e_" + member.Name + " to_e_" + member.Name + "(const auto &v) {"
                            "\n\t\tswitch(rohit::hash(v)) {";
                for(auto &enumName: member.enumNameList) {
                    retUnion += "\n\t\t\tcase rohit::hash(\"" + enumName + "\"): return e_" + member.Name + "::" + enumName + ";";
                }
                retUnion += "\n\t\t\tdefault: throw std::runtime_error(\"Bad Enum Name\");"
                            "\n\t\t}"
                            "\n\t}";
                return retUnion;
            }
        }
    }

    constexpr const std::string GetCPPType(const Member &member) {
        switch(member.modifer) {
        default:
        case Member::none:
            // TODO: Range check
            return GetCPPType(member.typeNameList[0]);
        case Member::array:
            return std::string("std::vector<") + GetCPPType(member.typeNameList[0]) + ">";
        case Member::map:
            return std::string("std::map<") + GetCPPType(member.Key) + "," + GetCPPType(member.typeNameList[0]) + ">";
        case Member::Union:
            return "e_" + member.Name + " " + member.Name + "_type { };\n\t" + "u_" + member.Name;
        }
    }

    void Write(const std::vector<Parent> &parents) {
        bool first { true };
        for(auto &parent: parents) {
            if (first) first = false;
            else outStream.Write(", ");
            Write(parent.access);
            outStream.Write(' ', parent.Name);
        }
    }

    void Write(const std::vector<Member> &members) {
        std::vector<Member> private_members { };
        std::vector<Member> protected_members { };
        std::vector<Member> public_members { };

        for(auto &member: members) {
            switch(member.access) {
                case AccessType::Public:
                    public_members.push_back(member);
                    break;
                case AccessType::Protected:
                    protected_members.push_back(member);
                    break;
                default:
                    private_members.push_back(member);
                    break;
            }
        }

        bool prepend_newline { false };

        if (!private_members.empty()) {
            outStream.Write("private:\n");
            for(auto &member: private_members) {
                auto support = GetCPPTypeSupport(member);
                if (!support.empty()) outStream.Write(support, "\n");
                outStream.Write('\t', GetCPPType(member), ' ', member.Name, " { };\n");
            }
            prepend_newline = true;
        }
        if (!protected_members.empty()) {
            if (prepend_newline) outStream.Write('\n');
            outStream.Write("protected:\n");
            for(auto &member: protected_members) {
                auto support = GetCPPTypeSupport(member);
                if (!support.empty()) outStream.Write(support, "\n");
                outStream.Write('\t', GetCPPType(member), ' ', member.Name, " { };\n");
            }
            prepend_newline = true;
        }
        if (!public_members.empty()) {
            if (prepend_newline) outStream.Write('\n');
            outStream.Write("public:\n");
            for(auto &member: public_members) {
                auto support = GetCPPTypeSupport(member);
                if (!support.empty()) outStream.Write(support, "\n");
                outStream.Write('\t', GetCPPType(member), ' ', member.Name, " { };\n");
            }
        }
    }

    void WriteSerializerOutBody(const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type) {
        bool first = true;
        for(auto &parent: obj->parentlist) {
            if (first) {
                first = false;
                outStream.Write("\n\t\t\tSerializerProtocol::struct_serialize_out_start(stream, ");
            }
            else outStream.Write("\n\t\t\tSerializerProtocol::struct_serialize_out(stream, ");
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
        for(auto &member: obj->MemberList) {
            if (member.modifer != Member::Union) {
                    if (first) {
                        first = false;
                        outStream.Write("\n\t\t\tSerializerProtocol::struct_serialize_out_start(stream, ");
                    }
                    else outStream.Write("\n\t\t\tSerializerProtocol::struct_serialize_out(stream, ");
                    if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
                        outStream.Write(
                            "std::make_pair(std::string_view { \"", member.Name, "\" }, ", member.Name,")"
                            ");");
                    } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
                        outStream.Write(
                            "std::make_pair(static_cast<uint32_t>(", member.id,"), ", member.Name,")"
                            ");");
                    } else {
                        outStream.Write(
                            member.Name, ");");
                    }
            }
            else {
                outStream.Write("\n\t\t\tswitch(", member.Name, "_type) {");
                for(size_t index { 0 }; index < member.enumNameList.size(); ++index) {
                    outStream.Write("\n\t\t\t\tcase e_", member.Name, "::", member.enumNameList[index], ":");
                    if (first) {
                        outStream.Write("\n\t\t\t\t\tSerializerProtocol::struct_serialize_out_start(stream, ");
                    }
                    else outStream.Write("\n\t\t\t\t\tSerializerProtocol::struct_serialize_out(stream, ");
                    if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
                        outStream.Write("std::make_pair( std::string_view {\"", member.Name, ":", member.enumNameList[index], "\"}, ",
                            member.Name, ".", member.enumNameList[index], "));",
                            "\n\t\t\t\t\tbreak;");
                    } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {
                        outStream.Write("std::make_tuple(static_cast<uint32_t>(", member.id, "), static_cast<uint32_t>(", index, "), ",
                            member.Name, ".", member.enumNameList[index], "));",
                            "\n\t\t\t\t\tbreak;");
                    } else {
                        outStream.Write("std::make_pair(static_cast<uint32_t>(", index, "), ",
                            member.Name, ".", member.enumNameList[index], "));",
                            "\n\t\t\t\t\tbreak;");
                    }
                }
                first = false;
                outStream.Write("\n\t\t\t}");
            }
        }
        outStream.Write("\n\t\t\tSerializerProtocol::struct_serialize_out_end(stream);\n");
    }

    void WriteSerializerInBody(const Class *obj, const rohit::serializer::SerializeKeyType serialize_key_type) {
        outStream.Write(
            "\t\t\tSerializerProtocol::struct_serialize_in(\n"
			"\t\t\t\tstream,\n"
        );
        bool first = true;
        for(auto &parent: obj->parentlist) {
            if (!first) outStream.Write(",\n");
            else first = false;
            if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
                outStream.Write("\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> { std::string_view {\"", parent.Name, "\"}, [this] (const rohit::FullStream &stream) { this->", parent.Name, "::template serialize_in<SerializerProtocol>(stream); }}");
            } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
                outStream.Write("\t\t\t\tstd::pair<uint32_t, std::function<void(const rohit::FullStream &)>> { static_cast<uint32_t>(", parent.id, "), [this] (const rohit::FullStream &stream) { this->", parent.Name, "::template serialize_in<SerializerProtocol>(stream); }}");
            } else {
                outStream.Write("\t\t\t\t[this] (const rohit::FullStream &stream) { this->", parent.Name, "::template serialize_in<SerializerProtocol>(stream); }");
            }
        }
        if (!first) outStream.Write(",\n");
        first = true;
        for(auto &member: obj->MemberList) {
            if (member.modifer != Member::Union) {
                if (!first) outStream.Write(",\n");
                else first = false;
                if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
                    outStream.Write(
                        "\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> {"
                        "\n\t\t\t\t\tstd::string_view {\"", member.Name, "\"}, [this] (const rohit::FullStream &stream) {"
                        "\n\t\t\t\t\t\tSerializerProtocol::template serialize_in<", GetCPPType(member),">(stream, this->", member.Name, ");"
                        "\n\t\t\t\t\t}"
                        "\n\t\t\t\t}");
                } else if (serialize_key_type == rohit::serializer::SerializeKeyType::Integer){
                    outStream.Write(
                        "\t\t\t\tstd::pair<uint32_t, std::function<void(const rohit::FullStream &)>> {"
                        "\n\t\t\t\t\tstatic_cast<uint32_t>(", member.id, "), [this] (const rohit::FullStream &stream) {"
                        "\n\t\t\t\t\t\tSerializerProtocol::template serialize_in<", GetCPPType(member),">(stream, this->", member.Name, ");"
                        "\n\t\t\t\t\t}"
                        "\n\t\t\t\t}");
                } else {
                    outStream.Write(
                        "\t\t\t\tstatic_cast<std::function<void(const rohit::FullStream &)>>([this] (const rohit::FullStream &stream) {"
                        "\n\t\t\t\t\t\tSerializerProtocol::template serialize_in<", GetCPPType(member),">(stream, this->", member.Name, ");"
                        "\n\t\t\t\t\t}"
                        "\n\t\t\t\t)");
                }
            } else if (member.enumNameList.size()) {
                if (serialize_key_type == rohit::serializer::SerializeKeyType::String) {
                    for(auto &enumName: member.enumNameList) {
                        if (!first) outStream.Write(",\n");
                        else first = false;
                        outStream.Write(
                            "\t\t\t\tstd::pair<std::string_view, std::function<void(const rohit::FullStream &)>> {"
                            "\n\t\t\t\t\tstd::string_view {\"", member.Name, ":", enumName, "\"}, [this] (const rohit::FullStream &stream) {"
                            "\n\t\t\t\t\t\tthis->", member.Name, "_type = e_", member.Name, "::", enumName, ";",
                            "\n\t\t\t\t\t\tSerializerProtocol::template serialize_in(stream, this->", member.Name, ".", enumName, ");"
                            "\n\t\t\t\t\t}"
                            "\n\t\t\t\t}");
                    }
                } else {
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
                        "\t\t\t\t\t\t", member.Name, "_type = SerializerProtocol::serialize_in_variable(stream);\n"
                        "\t\t\t\t\t\tthis->", member.Name, "_type = static_cast<e_", member.Name, ">(", member.Name, "_type);\n"
                        "\t\t\t\t\t\tswitch(this->", member.Name, "_type) {\n"
                    );
                    for(size_t index { 0 }; index < member.enumNameList.size(); ++index) {
                        outStream.Write(
                            "\t\t\t\t\t\t\tcase e_", member.Name, "::", member.enumNameList[index], ":\n"
                            "\t\t\t\t\t\t\t\tSerializerProtocol::template serialize_in(stream, this->", member.Name,".", member.enumNameList[index], ");\n"
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
                }
            }
        }
        outStream.Write("\n\t\t\t);");
    }

    void WriteSerializer(const Class *obj) {
        // Serialize out
        outStream.Write(
            "\ttemplate <typename SerializerProtocol>"
            "\n\tvoid serialize_out(rohit::Stream &stream) const {"
        );
        outStream.Write("\n\t\tif constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::None) {");
        WriteSerializerOutBody(obj, rohit::serializer::SerializeKeyType::None);
        outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {");
        WriteSerializerOutBody(obj, rohit::serializer::SerializeKeyType::Integer);
        outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::String) {");
        WriteSerializerOutBody(obj, rohit::serializer::SerializeKeyType::String);
        outStream.Write("\n\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n\t}\n\n");

        // Serialize in
        outStream.Write(
            "\ttemplate <typename SerializerProtocol>\n"
            "\tvoid serialize_in(const rohit::FullStream &stream) {"
        );

        outStream.Write("\n\t\tif constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::None) {\n");
        WriteSerializerInBody(obj, rohit::serializer::SerializeKeyType::None);
        outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::Integer) {\n");
        WriteSerializerInBody(obj, rohit::serializer::SerializeKeyType::Integer);
        outStream.Write("\n\t\t} else if constexpr (SerializerProtocol::serialize_key_type == rohit::serializer::SerializeKeyType::String) {\n");
        WriteSerializerInBody(obj, rohit::serializer::SerializeKeyType::String);
        outStream.Write("\n\t\t} else { static_assert(true, \"Unsupported serializer type\"); }\n\t}\n");
    }

    void Write(const Class *obj) {
        if ((obj->attributes & ClassAtributes::Packed) == ClassAtributes::Packed)
            outStream.Write("class __attribute__ ((__packed__)) ", obj->Name);
        else outStream.Write("class ", obj->Name);
        if (!obj->parentlist.empty()) {
            outStream.Write(" : ");
            Write(obj->parentlist);
        }
        outStream.Write(" {\n");
        Write(obj->MemberList);
        outStream.Write('\n');
        WriteSerializer(obj);
        outStream.Write("}; // class ", obj->Name, "\n\n");
    }

    void Write(const std::vector<std::unique_ptr<Base>> &statementlist);
    void Write(const Namespace *namespaceptr) {
        std::string completename = namespaceptr->Name;
        while (namespaceptr->statementlist.size() == 1 && namespaceptr->statementlist.back()->type == ObjectType::Namespace) {
            namespaceptr = dynamic_cast<Namespace *>(namespaceptr->statementlist.back().get());
            completename += "::";
            completename += namespaceptr->Name;
        }
        outStream.Write("namespace ", completename, " {\n");
        Write(namespaceptr->statementlist);
        outStream.Write("} // namespace ", completename,"\n\n");
    }

    std::vector<std::unique_ptr<Base>> Parse() { 
        return ParseStatementList(nullptr);
    }

public:
    SerializerCreator(const FullStream &inStream, FullStreamAutoAlloc &outStream) : inStream { inStream }, outStream { outStream } { }

    void Write() {
        auto statementlist = Parse();
        outStream.Write(
            "/////////////////////////////////////////////////////////\n"
            "// This is auto genarated file using serializer. Must  //\n"
            "// not be manually edited. For more information refer  //\n"
            "// to https://github.com/rohit-singh-gautam/Serializer //\n"
            "/////////////////////////////////////////////////////////\n"
            "#pragma once\n"
            "#include <rohit/serializer.h>\n\n"
        );
        Write(statementlist);
    }
    
}; // class SerializerCreator

inline std::unique_ptr<Namespace> SerializerCreator::ParseNameSpace(Namespace *parentNamespace) {
    // Object type is already parsed
    SkipWhiteSpaceAndComment();
    auto name = ParseHierarchicalIdentifier();
    SkipWhiteSpaceAndComment();
    if (*inStream != '{' ) {
        std::string errorstr { "Expecting '{' found: "};
        errorstr += *inStream;
        throw exception::BadNamespace { inStream, errorstr };
    }
    ++inStream;

    auto ret = std::make_unique<Namespace>(ObjectType::Namespace, std::move(name), parentNamespace);
    auto statementlist = ParseStatementList(ret.get());
    std::swap(ret->statementlist, statementlist);
    
    if (*inStream != '}' ) {
        std::string errorstr { "Expecting '}' found: "};
        errorstr += *inStream;
        throw exception::BadNamespace { inStream, errorstr };
    }
    ++inStream;
    return ret;
}

void SerializerCreator::Write(const std::vector<std::unique_ptr<Base>> &statementlist) {
    if (statementlist.empty()) return;

    for(auto &statement: statementlist) {
        if (statement->type == ObjectType::Namespace) Write(dynamic_cast<const Namespace *>(statement.get()));
        else Write(dynamic_cast<const Class *>(statement.get()));
    }
}

} // namespace rohit