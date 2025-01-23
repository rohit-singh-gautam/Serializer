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

namespace rohit::Serializer {

void tolower_inplace(std::string &value) {
    for(auto &ch: value) {
        if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
    }
}

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

std::string GetFullNameForNamespace(const Namespace *nameSpace) {
    return nameSpace->GetFullName();
}

namespace Parser {
constexpr bool IsWhiteSpace(const char val) noexcept { return val == ' ' || val == '\t' || val == '\n' || val == '\r'; }
constexpr bool IsNumber(const char val) noexcept { return val >= '0' && val <= '9'; }
constexpr bool IsSmallAlphabet(const char val) noexcept { return val >= 'a' && val <= 'z'; }
constexpr bool IsCapitalAlphabet(const char val) noexcept { return val >= 'A' && val <= 'Z'; }
constexpr bool IsFirstIdentifier(const char val) noexcept { return IsCapitalAlphabet(val) || IsSmallAlphabet(val) || val == '_'; }
constexpr bool IsIdentifier(const char val) noexcept { return IsNumber(val) || IsCapitalAlphabet(val) || IsSmallAlphabet(val) || val == '_'; }
bool IsWhiteSpace(const Stream &inStream) { return IsWhiteSpace(*inStream); }
bool IsNumber(const Stream &inStream) { return IsNumber(*inStream); }
bool IsSmallAlphabet(const Stream &inStream) { return IsSmallAlphabet(*inStream); }
bool IsCapitalAlphabet(const Stream &inStream) { return IsCapitalAlphabet(*inStream); }
bool IsFirstIdentifier(const Stream &inStream) { return IsFirstIdentifier(*inStream); }
bool IsIdentifier(const Stream &inStream) { return IsIdentifier(*inStream); }
void SkipWhiteSpace(const Stream &inStream) { while(IsWhiteSpace(inStream)) ++inStream; }

void SkipWhiteSpaceAndComment(const Stream &inStream) {
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
} // SkipWhiteSpaceAndComment

void check_in(const Stream &inStream, char value) {
    if (*inStream != value) {
        std::string errstr {"Expected: "};
        errstr.push_back(value);
        errstr += ", Found: ";
        errstr.push_back(*inStream);
        throw exception::BadClass { inStream, errstr };
    }
    ++inStream;
} // check_in

std::string ParseIdentifier(const Stream &inStream) {
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
} // ParseIdentifier

std::string ParseHierarchicalIdentifier(const Stream &inStream) {
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
        if (inStream.RemainingBuffer() < 2) break;
        if (*inStream != ':') break;
        ++inStream;
        if (*inStream != ':') throw exception::BadIdentifier { inStream, { "Namespace and identifier must be separated by '::', only one ':' is unsupported " } };
        ++inStream;
        identifier.push_back(':');
        identifier.push_back(':');
        if (inStream.full()) throw exception::BadIdentifier { inStream, { "Atleast one characted is require for identifier" } };
    }
    return identifier;
} // ParseHierarchicalIdentifier

void SpaceSeparatedIdentifier(const Stream &inStream, std::function<void(std::string &&)> fn) {
    if (!IsFirstIdentifier(inStream)) return;
    while(true) {
        auto identifier = ParseIdentifier(inStream);
        fn(std::move(identifier));
        if (!IsWhiteSpace(inStream)) break;
        SkipWhiteSpaceAndComment(inStream);
        if (!IsFirstIdentifier(inStream)) break;
    }
    return;
} // SpaceSeparatedIdentifier

AccessType ParseAccessType(const Stream &inStream) {
    auto accessType = ParseIdentifier(inStream);
    if (accessType == "public") return AccessType::Public;
    if (accessType == "protected") return AccessType::Protected;
    if (accessType == "private") return AccessType::Private;
    std::string errorstr { "Bad access type it must be one of 'public', 'protected' or 'private' case sensitive. Unknown access type: " };
    errorstr += accessType;
    throw exception::BadAccessType { inStream, errorstr };
} // ParseAccessType

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
} // ParseMemberModifier

void ParseMemberTypeUnion(const Stream &inStream, Namespace *declaredNameSpace, std::vector<TypeName> &typeNameList) {
    SkipWhiteSpaceAndComment(inStream);
    check_in(inStream, '(');
    int count { 0 };
    while(true) {
        SkipWhiteSpaceAndComment(inStream);
        auto typeName = ParseHierarchicalIdentifier(inStream);
        SkipWhiteSpaceAndComment(inStream);
        if (*inStream == '=') {
            ++inStream;
            SkipWhiteSpaceAndComment(inStream);
            auto enumName = ParseIdentifier(inStream);
            typeNameList.emplace_back(std::move(typeName), std::move(enumName), declaredNameSpace);
            SkipWhiteSpaceAndComment(inStream);
        } else {
            std::string enumName { "e_" + std::to_string(count) };
            typeNameList.emplace_back(std::move(typeName), std::move(enumName), declaredNameSpace);
            ++count;
        }
        if (*inStream != ',') break;
        ++inStream;
    }
    check_in(inStream, ')');
} // ParseMemberTypeUnion

void ParseMemberTypeMap(const Stream &inStream, Namespace *declaredNameSpace, std::vector<TypeName> &typeNameList, std::string &key) {
    SkipWhiteSpaceAndComment(inStream);
    check_in(inStream, '(');
    SkipWhiteSpaceAndComment(inStream);
    key = ParseHierarchicalIdentifier(inStream);
    check_in(inStream, ')');
    SkipWhiteSpaceAndComment(inStream);
    auto typeName = ParseHierarchicalIdentifier(inStream);
    typeNameList.emplace_back(std::move(typeName), declaredNameSpace);
} // ParseMemberTypeMap


Member ParseMember(const Stream &inStream, const uint32_t id, Namespace *declaredNameSpace) {
    auto accesstype = ParseAccessType(inStream);
    SkipWhiteSpaceAndComment(inStream);
    auto nextid = ParseHierarchicalIdentifier(inStream);
    std::vector<std::string> enumNameList { };
    std::vector<TypeName> typeNameList { };
    auto membermodifier = ParseMemberModifier(nextid);
    std::string key { };
    if (membermodifier == Member::none) {
        typeNameList.emplace_back(std::move(nextid), declaredNameSpace);
    } else if (membermodifier == Member::array) {
        SkipWhiteSpaceAndComment(inStream);
        auto typeName = ParseHierarchicalIdentifier(inStream);
        typeNameList.emplace_back(std::move(typeName), declaredNameSpace);
    } else if(membermodifier == Member::map) {
        ParseMemberTypeMap(inStream, declaredNameSpace, typeNameList, key);
    } else if (membermodifier == Member::Union) {
        ParseMemberTypeUnion(inStream, declaredNameSpace, typeNameList);
    }
    SkipWhiteSpaceAndComment(inStream);
    auto name = ParseIdentifier(inStream);
    SkipWhiteSpaceAndComment(inStream);
    check_in(inStream, ';');
    return { accesstype, membermodifier, typeNameList, name, id, key };
} // ParseMember

ObjectType ParseObjectType(const Stream &inStream) {
    auto objectType = ParseIdentifier(inStream);
    if (objectType == "class") return ObjectType::Class;
    if (objectType == "namespace") return ObjectType::Namespace;
    if (objectType == "enum") return ObjectType::Enum;
    std::string errorstr { "Bad identifier type it must be one of 'class' or 'namespace' case sensitive. Unknown access type: " };
    errorstr += objectType;
    throw exception::BadObjectType { inStream, errorstr };
} // ParseObjectType

void ParseClassBody(const Stream &inStream, Class *obj, uint32_t &id) {
    if (*inStream != '{' ) {
        std::string errorstr { "Expecting '{' found: "};
        errorstr += *inStream;
        throw exception::BadClass { inStream, errorstr };
    }
    ++inStream;
    SkipWhiteSpaceAndComment(inStream);
    while(*inStream != '}') {
        auto member = ParseMember(inStream, id++, obj->parentNamespace);
        obj->MemberList.push_back(std::move(member));
        SkipWhiteSpaceAndComment(inStream);
    }
    ++inStream;
    if (*inStream == ';') throw exception::BadClass { inStream, {"Semicolon is not expected at the end of a class"} };
}

Parent ParseParent(const Stream &inStream, Namespace *CurrentNamespace, const uint32_t id) {
    auto access = ParseAccessType(inStream);
    SkipWhiteSpaceAndComment(inStream);
    auto fullname = ParseHierarchicalIdentifier(inStream);
    return { access, fullname, id, CurrentNamespace, nullptr };
}

std::vector<Parent> ParseParentList(const Stream &inStream, Namespace *CurrentNamespace, uint32_t &id) {
    std::vector<Parent> ret { };
    if (!IsFirstIdentifier(inStream)) return ret;
    while(true) {
        // TODO: if ParseParent return type comes a rvalue
        ret.push_back( ParseParent(inStream, CurrentNamespace, id++) );
        SkipWhiteSpaceAndComment(inStream);
        if (*inStream != ',') break;
        ++inStream;
        SkipWhiteSpaceAndComment(inStream);
    }

    return ret;
}

std::unique_ptr<Class> ParseClassHeader(const Stream &inStream, Namespace *CurrentNamespace, uint32_t &id) {
    // Object type is already parsed
    SkipWhiteSpaceAndComment(inStream);
    auto name = ParseIdentifier(inStream);
    SkipWhiteSpaceAndComment(inStream);
    auto attributes { ClassAtributes::None };
    SpaceSeparatedIdentifier(inStream, [&attributes](std::string &&value) { 
        if (value == "packed") attributes |= ClassAtributes::Packed;
    });
    std::vector<Parent> parentlist;
    // At this point all whitespace is skipped
    if (*inStream == ':') {
        ++inStream;
        SkipWhiteSpaceAndComment(inStream);
        auto parentlisttemp = ParseParentList(inStream, CurrentNamespace, id);
        std::swap(parentlist, parentlisttemp);
    }

    return std::make_unique<Class>(ObjectType::Class, std::move(name), CurrentNamespace, attributes, std::move(parentlist));
}

std::unique_ptr<Class> ParseClass(const Stream &inStream, Namespace *CurrentNamespace) {
    uint32_t id { 1 };
    auto obj = ParseClassHeader(inStream, CurrentNamespace, id);
    // At this point all whitespace is skipped
    ParseClassBody(inStream, obj.get(), id);
    return obj;
}

std::unique_ptr<Enum> ParseEnum(const Stream &inStream, Namespace *CurrentNamespace) {
    SkipWhiteSpaceAndComment(inStream);
    auto enumName = ParseIdentifier(inStream);
    SkipWhiteSpaceAndComment(inStream);
    if (*inStream != '{' ) {
        std::string errorstr { "Expecting '{' found: "};
        errorstr += *inStream;
        throw exception::BadClass { inStream, errorstr };
    }
    ++inStream;
    SkipWhiteSpaceAndComment(inStream);
    std::vector<std::string> enumNameList { };
    if (*inStream != '}') {
        while(true) {
            auto name = ParseIdentifier(inStream);
            enumNameList.push_back(name);
            SkipWhiteSpaceAndComment(inStream);
            if (*inStream != ',') break;
            ++inStream;
            SkipWhiteSpaceAndComment(inStream);
        }
        if (*inStream != '}') {
            std::string errorstr { "Expecting '}' found: "};
            errorstr += *inStream;
            throw exception::BadClass { inStream, errorstr };
        }
    }
    ++inStream;
    if (*inStream == ';') throw exception::BadClass { inStream, {"Semicolon is not expected at the end of a class"} };
    auto ret = std::make_unique<Enum>(ObjectType::Enum, std::move(enumName), CurrentNamespace, std::move(enumNameList));
    return ret;
}

std::unique_ptr<Namespace> ParseNameSpace(const Stream &inStream, Namespace *parentNamespace);

std::vector<std::unique_ptr<Base>> ParseStatementList(const Stream &inStream, Namespace *parentNamespace) {
    std::vector<std::unique_ptr<Base>> statementlist { };
    while(true) {
        SkipWhiteSpaceAndComment(inStream);
        if (inStream.full() || *inStream == '}' || *inStream == 0xcd) break;
        auto objectType = ParseObjectType(inStream);
        if (objectType == ObjectType::Class) {
            statementlist.emplace_back(ParseClass(inStream, parentNamespace));
        } else if (objectType == ObjectType::Namespace) {
            statementlist.emplace_back( ParseNameSpace(inStream, parentNamespace) );
        } else if (objectType == ObjectType::Enum) {
            statementlist.emplace_back( ParseEnum(inStream, parentNamespace) );
        } else {
            std::string errorstr { "Bad identifier type it must be one of 'class' or 'namespace' case sensitive." };
            throw exception::BadObjectType { inStream, errorstr };
        }
    }

    return statementlist;
}

std::unique_ptr<Namespace> ParseNameSpace(const Stream &inStream, Namespace *parentNamespace) {
    // Object type is already parsed
    SkipWhiteSpaceAndComment(inStream);
    auto name = ParseHierarchicalIdentifier(inStream);
    SkipWhiteSpaceAndComment(inStream);
    if (*inStream != '{' ) {
        std::string errorstr { "Expecting '{' found: "};
        errorstr += *inStream;
        throw exception::BadNamespace { inStream, errorstr };
    }
    ++inStream;

    auto ret = std::make_unique<Namespace>(ObjectType::Namespace, std::move(name), parentNamespace);
    auto statementlist = ParseStatementList(inStream, ret.get());
    std::swap(ret->statementlist, statementlist);
    
    if (*inStream != '}' ) {
        std::string errorstr { "Expecting '}' found: "};
        errorstr += *inStream;
        throw exception::BadNamespace { inStream, errorstr };
    }
    ++inStream;
    return ret;
} // ParseNameSpace

void CheckMemberTypeForPrimitive(const Stream &inStream, TypeName &typeName) {
    if (typeName.type != ObjectType::Unresolved) return;
    if (Serializer::GetCPPTypeOrEmpty(typeName.Name).empty()) {
        std::string errorstr { "Unknown type: " };
        errorstr += typeName.Name;
        throw exception::BadMemberType { inStream, errorstr };
    }
    typeName.type = ObjectType::Primitive;
}

void ResolveMember(const Stream &inStream, Member &member, const std::unordered_map<std::string, ObjectType> &VariableTypeMap) {
    for(auto &typeName: member.typeNameList) {
        std::queue<Namespace *> namespaceStack { };
        Namespace *currentNamespace = typeName.declaredNameSpace;
        while(currentNamespace) {
            namespaceStack.push(currentNamespace);
            currentNamespace = currentNamespace->parentNamespace;
        }
        while(!namespaceStack.empty()) {
            currentNamespace = namespaceStack.front();
            namespaceStack.pop();
            auto tryFullname = currentNamespace->GetFullName() + "::" + typeName.Name;
            auto typeitr = VariableTypeMap.find(tryFullname);
            if (typeitr != std::end(VariableTypeMap)) {
                typeName.type = typeitr->second;
                typeName.definedNameSpace = currentNamespace;
                break;
            }
        }
        CheckMemberTypeForPrimitive(inStream, typeName);
    }
}

void ResolveMember(
    const Stream &inStream,
    std::vector<std::unique_ptr<rohit::Serializer::Base>> &statementlist,
    std::unordered_map<std::string, ObjectType> &VariableTypeMap) 
{
    for(auto &statement: statementlist) {
        switch (statement->type)
        {
        case ObjectType::Namespace:
            {
                auto namespaceptr = dynamic_cast<Namespace *>(statement.get());
                ResolveMember(inStream, namespaceptr->statementlist, VariableTypeMap);
            }
            break;

        case ObjectType::Class:
            {
                VariableTypeMap.insert({statement->GetFullName(), ObjectType::Class});
                auto classptr = dynamic_cast<Class *>(statement.get());
                for(auto &member: classptr->MemberList) {
                    ResolveMember(inStream, member, VariableTypeMap);
                }
            }
            break;

        case ObjectType::Enum:
            VariableTypeMap.insert({statement->GetFullName(), ObjectType::Enum});
            break;
        
        default:
            break;
        }
    }
}

std::vector<std::unique_ptr<Base>> Parse(const Stream &inStream) { 
    auto statementlist = ParseStatementList(inStream, nullptr);
    std::unordered_map<std::string, ObjectType> VariableTypeMap;
    ResolveMember(inStream, statementlist, VariableTypeMap);
    return statementlist;
}

} // namespace Parser
} // namespace rohit::Serializer