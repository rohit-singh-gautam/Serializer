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
#include <queue>
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

namespace rohit::serializer {
namespace exception {
class BadIdentifier : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadAccessType : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadObjectType : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadClassMember : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadMemberType : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadClass : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

class BadNamespace : public rohit::exception::BaseParser {
public:
    using rohit::exception::BaseParser::BaseParser;
};

} // namespace exception

void tolower_inplace(std::string &value);

enum class AccessType {
    Error,
    Private,
    Protected,
    Public
};

enum class ObjectType {
    Unresolved,
    Namespace,
    Class,
    Enum,
    Primitive
};

enum class ClassAtributes : uint8_t {
    None = 0x00,
    Packed = 0x01
};

struct Namespace;

std::string GetFullNameForNamespace(const Namespace *nameSpace);

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
    std::string GetFullName() const {
        std::string fullName { };
        if (parentNamespace) {
            fullName = GetFullNameForNamespace(parentNamespace);
            fullName += "::";
        }
        fullName += Name;
        return fullName;
    }
};

struct Namespace : public Base {
    std::vector<std::unique_ptr<Base>> statementlist { };
    Namespace(ObjectType type, std::string &&Name,
        Namespace *parentNamespace) :
            Base { type, std::move(Name), parentNamespace }
                {}
};

struct TypeName {
    TypeName(std::string &&Name, Namespace *declaredNameSpace) : Name { std::move(Name) }, EnumName { }, declaredNameSpace { declaredNameSpace } { }
    TypeName(std::string &&Name, std::string &&EnumName, Namespace *declaredNameSpace) : Name { std::move(Name) }, EnumName { std::move(EnumName) }, declaredNameSpace { declaredNameSpace } { }
    TypeName(const TypeName &rhs) : Name { rhs.Name }, EnumName { rhs.EnumName }, declaredNameSpace { rhs.declaredNameSpace }, definedNameSpace { definedNameSpace } { }
    TypeName &operator=(const TypeName &rhs) {
        Name = rhs.Name;
        EnumName = rhs.EnumName;
        declaredNameSpace = rhs.declaredNameSpace;
        definedNameSpace = rhs.definedNameSpace;
        return *this;
    }

    std::string Name;
    std::string EnumName;
    Namespace *declaredNameSpace;
    Namespace *definedNameSpace { };
    ObjectType type { ObjectType::Unresolved };

    std::string GetFullName() const {
        std::string fullName { };
        if (definedNameSpace) {
            fullName = GetFullNameForNamespace(definedNameSpace);
            fullName += "::";
        }
        fullName += Name;
        return fullName;
    }

    bool operator==(const TypeName &rhs) const { return Name == rhs.Name && EnumName == rhs.EnumName && declaredNameSpace == rhs.declaredNameSpace; }
};

struct Member {
    enum ModifierType {
        none,
        array,
        map,
        Union
    };
    AccessType access;
    ModifierType modifer;
    std::vector<TypeName> typeNameList;
    std::string Name;
    uint32_t id;
    std::string Key; // Optional parameter

    bool operator==(const Member &rhs) const { return access == rhs.access && modifer == rhs.modifer && typeNameList == rhs.typeNameList && Name == rhs.Name; }
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
        std::vector<Parent> &&parentlist) : Base { type, std::move(Name), parentNamespace },
            attributes { attributes }, parentlist { std::move(parentlist) } { }
    Class(Class &&rhs) : Base { std::move(rhs) },
        attributes { rhs.attributes }, parentlist { std::move(rhs.parentlist) },
        MemberList { std::move(rhs.MemberList) } { }
    Class(const Class&) = delete;
    Class &operator=(const Class&) = delete;
};

struct Enum : public Base {
    Enum(ObjectType type, std::string &&Name,
        Namespace *parentNamespace,
        std::vector<std::string> &&enumNameList) : 
                Base { type, std::move(Name), parentNamespace }, enumNameList { std::move(enumNameList) } { }
    
    std::vector<std::string> enumNameList { };
};

ClassAtributes &operator|=(ClassAtributes &lhs, const ClassAtributes &rhs);
ClassAtributes operator&(const ClassAtributes &lhs, const ClassAtributes &rhs);

const std::string &GetCPPTypeOrEmpty(const std::string &type);
const std::string &GetCPPType(const std::string &type);

namespace Parser {
std::vector<std::unique_ptr<Base>> Parse(const Stream &inStream);
#ifdef ENABLE_GTEST
std::string ParseIdentifier(const Stream &inStream);
std::string ParseHierarchicalIdentifier(const Stream &inStream);
void SpaceSeparatedIdentifier(const Stream &inStream, std::function<void(std::string &&)> fn);
AccessType ParseAccessType(const Stream &inStream);
Member ParseMember(const Stream &inStream, const uint32_t id, Namespace *declaredNameSpace);
void ParseClassBody(const Stream &inStream, Class *obj, uint32_t &id);
#endif
} // namespace Parser

namespace Writer::CPP {
void Write(Stream &outStream, std::vector<std::unique_ptr<Base>> &statementlist);
} // namespace Writer::CPP
} // namespace rohit::serializer