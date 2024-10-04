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

// Gramar
// STRUCTFILE: statementlist
// namespace: "namespace" space hirerchical_identifier space '{' space statementlist space '}'
// statementlist: statement | statement space statementlist
// statement: namespace | class
// class : classheader classbody | classextendedheader space classbody
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
class Base : public std::exception {
protected:
    const FullStream stream;
    std::string errorstr;

public:
    Base(const FullStream &stream, std::string &errorstr) : stream { stream }, errorstr { errorstr } { }
    Base(const FullStream &stream, std::string &&errorstr) : stream { stream }, errorstr { std::move(errorstr) } { }

    // TODO: Implement in detail
    const char *what() const noexcept override {
        return "Bad Identifier";
    }
};

class BadIdentifier : public Base {
public:
    using Base::Base;
};

class BadAccessType : public Base {
public:
    using Base::Base;
};

class BadObjectType : public Base {
public:
    using Base::Base;
};

class BadClassMember : public Base {
public:
    using Base::Base;
};

class BadClass : public Base {
public:
    using Base::Base;
};

class BadNamespace : public Base {
public:
    using Base::Base;
};

};

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

struct Member {
    AccessType access;
    std::string typeName;
    std::string Name;

    bool operator==(const Member &rhs) const { return access == rhs.access && typeName == rhs.typeName && Name == rhs.Name; }
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
public:
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
            SkipWhiteSpace();
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

    Member ParseMember() {
        auto accesstype = ParseAccessType();
        SkipWhiteSpace();
        auto typeName = ParseHierarchicalIdentifier();
        SkipWhiteSpace();
        auto name = ParseIdentifier();
        SkipWhiteSpace();
        if (*inStream != ';') throw exception::BadClassMember { inStream, {"Expected a ';'"} };
        ++inStream;
        return { accesstype, typeName, name };
    }

    ObjectType ParseObjectType() {
        auto objectType = ParseIdentifier();
        if (objectType == "class") return ObjectType::Class;
        if (objectType == "namespace") return ObjectType::Namespace;
        std::string errorstr { "Bad identifier type it must be one of 'class' or 'namespace' case sensitive. Unknown access type: " };
        errorstr += objectType;
        throw exception::BadObjectType { inStream, errorstr };
    }

    void ParseClassBody(Class &obj) {
        if (*inStream != '{' ) {
            std::string errorstr { "Expecting '{' found: "};
            errorstr += *inStream;
            throw exception::BadClass { inStream, errorstr };
        }
        ++inStream;
        SkipWhiteSpace();
        while(*inStream != '}') {
            auto member = ParseMember();
            obj.MemberList.push_back(std::move(member));
            SkipWhiteSpace();
        }
        ++inStream;
        if (*inStream == ';') throw exception::BadClass { inStream, {"Semicolon is not expected at the end of a class"} };
    }

    Parent ParseParent(Namespace *CurrentNamespace) {
        auto access = ParseAccessType();
        SkipWhiteSpace();
        auto fullname = ParseHierarchicalIdentifier();
        return { access, fullname, CurrentNamespace, nullptr };
    }

    std::vector<Parent> ParseParentList(Namespace *CurrentNamespace) {
        std::vector<Parent> ret { };
        if (!IsFirstIdentifier()) return ret;
        while(true) {
            // TODO: if ParseParent return type comes a rvalue
            ret.push_back( ParseParent(CurrentNamespace) );
            SkipWhiteSpace();
            if (*inStream != ',') break;
            ++inStream;
            SkipWhiteSpace();
        }

        return ret;
    }

    Class ParseClassHeader(Namespace *CurrentNamespace) {
        // Object type is already parsed
        SkipWhiteSpace();
        auto name = ParseIdentifier();
        SkipWhiteSpace();
        auto attributes { ClassAtributes::None };
        SpaceSeparatedIdentifier([&attributes](std::string &&value) { 
            if (value == "packed") attributes |= ClassAtributes::Packed;
        });
        std::vector<Parent> parentlist;
        // At this point all whitespace is skipped
        if (*inStream == ':') {
            auto parentlisttemp = ParseParentList(CurrentNamespace);
            std::swap(parentlist, parentlisttemp);
            ++inStream;
        }

        return {ObjectType::Class, std::move(name), CurrentNamespace, attributes, std::move(parentlist)};
    }

    Class ParseClass(Namespace *CurrentNamespace) {
        Class obj = ParseClassHeader(CurrentNamespace);
        // At this point all whitespace is skipped
        ParseClassBody(obj);
        return obj;
    }

    std::unique_ptr<Namespace> ParseNameSpace(Namespace *parentNamespace);
    
    std::vector<std::unique_ptr<Base>> ParseStatementList(Namespace *parentNamespace) {
        std::vector<std::unique_ptr<Base>> statementlist { };
        while(true) {
            SkipWhiteSpace();
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

    std::vector<std::unique_ptr<Base>> Parse() { 
        return ParseStatementList(nullptr);
    }
public:
    SerializerCreator(const FullStream &inStream, FullStreamAutoAlloc &outStream) : inStream { inStream }, outStream { outStream } { }

}; // class SerializerCreator

inline std::unique_ptr<Namespace> SerializerCreator::ParseNameSpace(Namespace *parentNamespace) {
    // Object type is already parsed
    SkipWhiteSpace();
    auto name = ParseHierarchicalIdentifier();
    SkipWhiteSpace();
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

} // namespace rohit