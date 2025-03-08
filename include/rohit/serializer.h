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
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <iterator>
#include <functional>
#include <vector>
#include <map>
#include <type_traits>
#include <stdexcept>
#include <string_view>

namespace rohit::serializer {
namespace exception {
using rohit::exception::BaseParser;

class BadInputData : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class BadType : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class UnknownSerializationType : public BaseParser {
public:
    using BaseParser::BaseParser;
};

class KeyNotFound : public BaseParser {
public:
    using BaseParser::BaseParser;
};
} // namespace exception

namespace typecheck {
template <typename T, typename J>
concept SerializerInEnabled = requires(T cls, J &serializeProtocol) {
    { cls->template SerializeIn<J>(serializeProtocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabledPtr = requires(T cls, J &serializeProtocol) {
    { cls->template SerializeOut<J>(serializeProtocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabled = requires(T cls, J &serializeProtocol) {
    { cls.template SerializeOut<J>(serializeProtocol) } -> std::same_as<void>;
};

template <typename T>
concept vector = requires(T t) {
    typename T::value_type;
    requires std::is_same_v<T, std::vector<typename T::value_type>>;
};

template <typename T>
concept map = requires(T t) {
    typename T::key_type;
    typename T::mapped_type;
    requires std::is_same_v<T, std::map<typename T::key_type, typename T::mapped_type>>;
};

template <typename T>
concept functions = requires(T t) {
    requires std::is_same_v<T, void(Stream &)> || std::is_function_v<T> || std::is_same_v<T, std::function<void(Stream &)>>;
};

} // namespace typecheck

struct write_format {
    bool newline_before_braces_open { false };
    bool newline_after_braces_open { false };
    bool newline_before_braces_close { false };
    bool newline_after_braces_close { false };
    bool newline_before_bracket_open { false };
    bool newline_after_bracket_open { false };
    bool newline_before_bracket_close { false };
    bool newline_after_bracket_close { false };
    bool newline_before_object_member { false };
    bool space_after_comma { false }; // Space will not be added if newline is enabled
    bool newline_after_comma { false };
    bool space_after_colon { false };
    bool all_data_on_newline { false };
    std::string_view intendtext { };
};

namespace format {
static constexpr write_format compress { };

static constexpr write_format beautify { 
    .newline_after_braces_open = true,
    .newline_before_braces_close = true,
    .newline_after_bracket_open = true,
    .newline_before_bracket_close = true,
    .newline_before_object_member = true,
    .space_after_comma = true,
    .space_after_colon = true,
    .intendtext = { "  " } 
};

static constexpr write_format beautify_vertical { 
    .newline_before_braces_open = true,
    .newline_after_braces_open = true,
    .newline_before_braces_close = true,
    .newline_before_bracket_open = true,
    .newline_after_bracket_open = true,
    .newline_before_bracket_close = true,
    .newline_before_object_member = true,
    .space_after_comma = true,
    .newline_after_comma = true,
    .space_after_colon = true,
    .all_data_on_newline = true,
    .intendtext = { "  " } 
};
} // namespace format


template <typename TypeEnum, typename Base, typename T0>
auto typecast(TypeEnum type, Base *ptr) { return reinterpret_cast<T0 *>(ptr); }
template <typename TypeEnum, typename Base, typename T0, typename T1>
auto typecast(TypeEnum type, Base *ptr) { 
    switch(reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
        default:
        case 0: return reinterpret_cast<T0 *>(ptr);
        case 1: return reinterpret_cast<T1 *>(ptr);
    }
}
template <typename TypeEnum, typename Base, typename T0, typename T1, typename T2>
auto typecast(TypeEnum type, Base *ptr) { 
    switch(reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
        default:
        case 0: return reinterpret_cast<T0 *>(ptr);
        case 1: return reinterpret_cast<T1 *>(ptr);
        case 2: return reinterpret_cast<T2 *>(ptr);
    }
}
template <typename TypeEnum, typename Base, typename T0, typename T1, typename T2, typename T3>
auto typecast(TypeEnum type, Base *ptr) { 
    switch(reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
        default:
        case 0: return reinterpret_cast<T0 *>(ptr);
        case 1: return reinterpret_cast<T1 *>(ptr);
        case 2: return reinterpret_cast<T2 *>(ptr);
        case 3: return reinterpret_cast<T3 *>(ptr);
    }
}

template <typename TypeEnum, typename Base, typename T, typename ...TArr>
auto typecast(TypeEnum type, Base *ptr) {
    auto id = reinterpret_cast<std::underlying_type<TypeEnum>>(type);
    return typecast<TypeEnum, Base, TArr...>(reinterpret_cast<TypeEnum>(id - 1), ptr);
}

template <typename TypeEnum, typename Base, typename ...TArr>
class variable_pointer {
    TypeEnum type;
    Base *ptr;

public:
    constexpr variable_pointer(TypeEnum type, Base *ptr) : type { type }, ptr { ptr } { }
    ~variable_pointer() { if (ptr) delete ptr; }

    constexpr auto GetType() { return type; }
    constexpr auto Get() { return typecast<TypeEnum, Base, TArr...>(type, ptr); }
    constexpr auto operator*() { return *Get(); }
    constexpr auto operator->() { return Get(); }
};

enum class SerializeKeyType {
    None,
    Integer,
    String
};

enum class SerializeType {
    In,
    Out
};

template <SerializeType type>
class json { };

template<>
class json<SerializeType::In> {
public:
    constexpr static SerializeKeyType serialize_key_type = SerializeKeyType::String;

protected:
    const Stream &inStream;

public:
    json(const Stream &inStream) : inStream { inStream } { }

    const auto &GetStream() { return inStream; }
    auto &GetStream() const { return inStream; }

protected:
    constexpr bool IsWhiteSpace(const char val) noexcept { return val == ' ' || val == '\t' || val == '\n' || val == '\r'; }
    void SkipWhiteSpace() { while(IsWhiteSpace(*inStream)) ++inStream; }

    void CheckAndIncrease(char value) {
        if (*inStream != value) {
            std::string errStr { "Expected " };
            errStr.push_back(value);
            errStr += " but found ";
            errStr.push_back(*inStream);
            throw exception::BadInputData { inStream , std::move(errStr) };
        }
        ++inStream;
    }

    const std::string_view SerializeInGetKey() {
        SkipWhiteSpace();
        CheckAndIncrease('"');
        auto start = inStream.curr();
        // TODO:: Escape character
        while(*inStream != '"') ++inStream;
        auto end = inStream.curr();
        ++inStream;
        SkipWhiteSpace();
        CheckAndIncrease(':');
        SkipWhiteSpace();
        return { reinterpret_cast<const char *>(start), reinterpret_cast<const char *>(end) };
    }

    void SerializeInBool(bool &value) {
        if (inStream.RemainingBuffer() < 4) throw exception::BadInputData { inStream };
        auto ch = std::tolower(*inStream);
        if (ch == 't') {
            ++inStream;
            if (std::tolower(*inStream) != 'r') throw exception::BadInputData { inStream };
            ++inStream;
            if (std::tolower(*inStream) != 'u') throw exception::BadInputData { inStream };
            ++inStream;
            if (std::tolower(*inStream) != 'e') throw exception::BadInputData { inStream };
            ++inStream;
            value = true;
        } else if (ch == 'f') {
            ++inStream;
            if (std::tolower(*inStream) != 'a') throw exception::BadInputData { inStream };
            ++inStream;
            if (std::tolower(*inStream) != 'l') throw exception::BadInputData { inStream };
            ++inStream;
            if (std::tolower(*inStream) != 's') throw exception::BadInputData { inStream };
            ++inStream;
            if (inStream.full()) throw exception::BadInputData { inStream };
            if (std::tolower(*inStream) != 'e') throw exception::BadInputData { inStream };
            ++inStream;
            value = false;
        }
    }

    void SerializeInChar(char &value) {
        if (inStream.RemainingBuffer() < 3) throw exception::BadInputData { inStream };
        if (*inStream != '"') throw exception::BadInputData { inStream };
        ++inStream;
        value = *inStream;
        ++inStream;
        if (*inStream != '"') throw exception::BadInputData { inStream };
        ++inStream;
    }

    void SerializeInUnsignedInteger(std::unsigned_integral auto &value) {
        using ValueType = std::remove_reference_t<decltype(value)>;
        if (inStream.full()) throw exception::BadInputData { inStream };
        auto ch = *inStream;
        if (ch < '0' || ch > '9') throw exception::BadInputData { inStream };
        value = ch - '0';
        ++inStream;
        while(!inStream.full()) {
            ch = *inStream;
            if (ch < '0' || ch > '9') break;
            constexpr ValueType check_big = std::numeric_limits<ValueType>::max() / static_cast<ValueType>(10);
            constexpr ValueType check_big_last = std::numeric_limits<ValueType>::max() % static_cast<ValueType>(10);
            if (value > check_big) {
                // This will make value max
                value = std::numeric_limits<ValueType>::max();
                ++inStream;
                while(!inStream.full()) {
                    ch = *inStream;
                    if (ch < '0' || ch > '9') break;
                    ++inStream;
                }
                return;

            } else if (value == check_big) {
                ++inStream;
                if (!inStream.full()) {
                    auto ch1 = *inStream;
                    if (ch1 < '0' || ch1 > '9') {
                        if (ch < '0' + check_big_last) value = value * 10 - '0' + ch;
                        else value = std::numeric_limits<ValueType>::max();
                        return;
                    }
                    value = std::numeric_limits<ValueType>::max();
                    ++inStream;
                    while(!inStream.full()) {
                        ch = *inStream;
                        if (ch < '0' || ch > '9') break;
                        ++inStream;
                    }
                    return;
                } else {
                    if (ch < '0' + check_big_last) value = value * 10 - '0' + ch;
                    else value = std::numeric_limits<ValueType>::max();
                    return;
                }
            }
            value = value * 10 + *inStream - '0';
            ++inStream;
        }
    }

    void SerializeInSignedInteger(std::signed_integral auto &value) {
        if (inStream.full()) throw exception::BadInputData { inStream };
        // TODO: Check out of range values
        if ((*inStream < '0' || *inStream > '9') && *inStream != '-' && *inStream != '+') throw exception::BadInputData { inStream };
        auto sign { static_cast<std::remove_reference_t<decltype(value)>>(1) };
        if (*inStream == '-') {
            sign = -1;
            ++inStream;
        } else if (*inStream == '+') ++inStream;
        value = *inStream - '0';
        ++inStream;
        while(!inStream.full()) {
            if (*inStream < '0' || *inStream > '9') break;
            value = value * 10 + *inStream - '0';
            ++inStream;
        }
        value *= sign;
    }

    void SerializeInString(std::string &value) {
        if (inStream.RemainingBuffer() < 2) throw exception::BadInputData { inStream };
        if (*inStream != '"') throw exception::BadInputData { inStream };
        ++inStream;
        while(*inStream != '"') {
            if (inStream.full()) throw exception::BadInputData { inStream };
            value.push_back(*inStream);
            ++inStream;
        }
        ++inStream;
    }

    void SerializeInFloatingPoint(std::floating_point auto &value) {
        if (inStream.full()) throw exception::BadInputData { inStream };
        // TODO: Check out of range values
        if ((*inStream < '0' || *inStream > '9') && *inStream != '-' && *inStream != '+') throw exception::BadInputData { inStream };
        auto start = inStream.curr();
        while (!inStream.full() && *inStream != ',' && *inStream != '!' && *inStream != ']' && *inStream != '}' && *inStream != ' ')
            ++inStream;
        std::string number {start, inStream.curr()};
        if constexpr (std::is_same_v<float, std::remove_reference_t<decltype(value)>>) value = std::stof(number);
        else value = std::stod(number);
    }

    void SerializeInVector(typecheck::vector auto &value) {
        CheckAndIncrease('[');
        SkipWhiteSpace();
        if (*inStream != ']') {
            while(true) {
                using value_type = std::remove_reference_t<decltype(value)>::value_type;
                value_type valuetype { };
                SerializeIn(valuetype);
                value.emplace_back(valuetype);
                SkipWhiteSpace();
                if (*inStream == ']') break;
                CheckAndIncrease(',');
                SkipWhiteSpace();
                if (*inStream == ']') {
                    throw exception::BadInputData { inStream, "Unexpected ',', there must be next array entry after ','" };
                }
            }
        }
        ++inStream;
    }

    void SerializeInMap(typecheck::map auto &value) {
        CheckAndIncrease('{');
        SkipWhiteSpace();
        if (*inStream != '}') {
            while(true) {
                using T = std::remove_reference_t<decltype(value)>;
                typename T::key_type key { };
                SerializeIn(key);
                SkipWhiteSpace();
                CheckAndIncrease(':');
                SkipWhiteSpace();
                typename T::mapped_type valuetype { };
                SerializeIn(valuetype);
                value.emplace(std::move(key), std::move(valuetype));
                SkipWhiteSpace();
                if (*inStream == '}') break;
                CheckAndIncrease(',');
                SkipWhiteSpace();
                if (*inStream == '}') {
                    throw exception::BadInputData { inStream, "Unexpected ',', there must be next map entry after ','" };
                }
            }
        }
        ++inStream;
    }

public:
    template <typename T>
    void SerializeIn(T &value) {
        if constexpr (std::is_same_v<bool, T>) {
            SerializeInBool(value);
        } else if constexpr (std::is_same_v<char, T>) {
            SerializeInChar(value);
        } else if constexpr (std::unsigned_integral<T>) {
            SerializeInUnsignedInteger(value);
        } else if constexpr (std::signed_integral<T>) {
            SerializeInSignedInteger(value);
        } else if constexpr (std::is_same_v<std::string, T>) {
            SerializeInString(value);
        } else if constexpr (std::floating_point<T>) {
            SerializeInFloatingPoint(value);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, json>) {
            value->SerializeIn(*this);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json>) {
            value.SerializeIn(*this);
        } else if constexpr (typecheck::vector<T>) {
            SerializeInVector(value);
        } else if constexpr (typecheck::map<T>) {
            SerializeInMap(value);
        } else throw exception::BadType { inStream };
    }

    template <typename T>
    void StructSerializeIn(T *obj )
    {
        static_assert(serialize_key_type == SerializeKeyType::String, "Only String key type supported");
        SkipWhiteSpace();
        CheckAndIncrease('{');
        SkipWhiteSpace();
        while(true) {
            auto key = SerializeInGetKey();
            obj->SerializeInMemberByName(*this, key);
            SkipWhiteSpace();
            if (*inStream == '}') break;
            CheckAndIncrease(',');
            SkipWhiteSpace();
            if (*inStream == '}') {
                throw exception::BadInputData { inStream, "Unexpected ',', there next object expected after ','" };
            }
        }
        ++inStream;
    }
}; // class json<SerializeType::In>

template <bool beautify>
class json_formatter {
protected:
    Stream &outStream;
public:
    json_formatter(Stream &outStream, const write_format &) : outStream { outStream } { }

protected:
    inline void WriteBraceOpen() {
        outStream.Write('{');
    }

    inline void WriteBraceClose() {
        outStream.Write('}');
    }

    inline void WriteBracketOpen() {
        outStream.Write('[');
    }

    inline void WriteBracketClose() {
        outStream.Write(']');
    }

    template <bool memberObject>
    inline void WriteComma() {
        outStream.Write(',');
    }

    inline void WriteColon() {
        outStream.Write(':');
    }

    inline void NewlineAdded(bool) { }
};

template <>
class json_formatter<true> {
    bool newlineWritten { false };
protected:
    Stream &outStream;
    const write_format formatDefinition;

    std::string tabString { };

    inline void BeforeBraceOpen() {
        if (formatDefinition.newline_before_braces_open) {
            if (!newlineWritten) {
                outStream.Write('\n');
                outStream.Write(tabString);
            }
        }
    }

    inline void AfterBraceOpen() {
        if (formatDefinition.newline_after_braces_open || formatDefinition.newline_before_object_member) {
            outStream.Write('\n');
            tabString.append(formatDefinition.intendtext);
            outStream.Write(tabString);
            newlineWritten = true;
        }
    }

    inline void BeforeBraceClose() {
        if (formatDefinition.newline_before_braces_close) {
            outStream.Write('\n');
            tabString.erase(std::end(tabString) - std::size(formatDefinition.intendtext), std::end(tabString));
            outStream.Write(tabString);
        }
    }

    inline void AfterBraceClose() {
        if (formatDefinition.newline_after_braces_close) {
            outStream.Write('\n');
            outStream.Write(tabString);
            newlineWritten = true;
        }
    }

    inline void BeforeBracketOpen() {
        if (formatDefinition.newline_before_bracket_open) {
            if (!newlineWritten) {
                outStream.Write('\n');
                outStream.Write(tabString);
            }
        }
    }

    inline void AfterBracketOpen() {
        if (formatDefinition.newline_after_bracket_open) {
            outStream.Write('\n');
            tabString.append(formatDefinition.intendtext);
            outStream.Write(tabString);
            newlineWritten = true;
        }
    }

    inline void BeforeBracketClose() {
        if (formatDefinition.newline_before_bracket_close) {
            outStream.Write('\n');
            tabString.erase(std::end(tabString) - std::size(formatDefinition.intendtext), std::end(tabString));
            outStream.Write(tabString);
        }
    }

    inline void AfterBracketClose() {
        if (formatDefinition.newline_after_bracket_close) {
            outStream.Write('\n');
            outStream.Write(tabString);
            newlineWritten = true;
        }
    }

    inline void WriteBraceOpen() {
        BeforeBraceOpen();
        outStream.Write('{');
        AfterBraceOpen();
    }

    inline void WriteBraceClose() {
        BeforeBraceClose();
        outStream.Write('}');
        AfterBraceClose();
    }

    inline void WriteBracketOpen() {
        BeforeBracketOpen();
        outStream.Write('[');
        AfterBracketOpen();
    }

    inline void WriteBracketClose() {
        BeforeBracketClose();
        outStream.Write(']');
        AfterBracketClose();
    }

    template <bool memberObject>
    inline void WriteComma() {
        if (formatDefinition.newline_after_comma || (memberObject && formatDefinition.newline_before_object_member)) {
            outStream.Write(",\n", tabString);
            newlineWritten = true;
        } else if (formatDefinition.space_after_comma) outStream.Write(", ");
        else outStream.Write(',');
    }

    inline void WriteColon() {
        if (formatDefinition.space_after_colon) outStream.Write(": ");
        else outStream.Write(':');
    }

    inline void NewlineAdded(bool inNewlineWritten) { newlineWritten = inNewlineWritten; }

public:
    json_formatter(Stream &outStream, const write_format &formatDefinition) : outStream { outStream }, formatDefinition { formatDefinition } { }
};


template <bool beautify>
class JsonOut : public json_formatter<beautify> {
public:
    constexpr static SerializeKeyType serialize_key_type = SerializeKeyType::String;

public:
    JsonOut(Stream &outStream) : json_formatter<beautify> { outStream, format::compress } { }
    JsonOut(Stream &outStream, const write_format &formatDefinition) : json_formatter<beautify> { outStream, formatDefinition } { }

private:
    using json_formatter<beautify>::outStream;

    using json_formatter<beautify>::WriteBraceOpen;
    using json_formatter<beautify>::WriteBraceClose;
    using json_formatter<beautify>::WriteBracketOpen;
    using json_formatter<beautify>::WriteBracketClose;
    using json_formatter<beautify>::WriteColon;
    using json_formatter<beautify>::NewlineAdded;

    template <typename T>
    void SerializeOutFirst(auto &name, const T &value) {
        SerializeOut(name);
        WriteColon();
        SerializeOut(value);
    }

    template <typename T>
    void SerializeOutSecond(auto &name, const T &value) {
        json_formatter<beautify>::template WriteComma<true>();
        SerializeOut(name);
        WriteColon();
        SerializeOut(value);
    }

public:
    template <typename T>
    void SerializeOut(const T &value) {
        if constexpr (std::is_same_v<T, char>) {
            NewlineAdded(false);
            outStream.Write('"', value, '"');
        } else if constexpr (std::is_same_v<T, bool>) {
            NewlineAdded(false);
            if (value) outStream.Append("true");
            else outStream.Append("false");
        } else if constexpr (std::integral<T>) {
            NewlineAdded(false);
            outStream.AppendString(value);
        } else if constexpr (std::floating_point<T>) {
            NewlineAdded(false);
            auto floatStr = std::to_string(value);
            outStream.Append(floatStr);
        } else if constexpr (std::same_as<T, std::string>) {
            NewlineAdded(false);
            outStream.Write('"', value, '"');
        } else if constexpr (std::same_as<T, std::string_view>) { 
            NewlineAdded(false);
            outStream.Write('"', value, '"');
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, json<SerializeType::Out>>) {
            value->SerializeOut(*this);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json<SerializeType::Out>>) {
            value.SerializeOut(*this);
        } else {
            throw exception::UnknownSerializationType {outStream, "Unknown Serialization Type"};
        }
    }

    template <typecheck::functions T>
    void SerializeOut(const T &value) {
        value(outStream);
    }

    template <typecheck::vector T>
    void SerializeOut(const T &value) {
        WriteBracketOpen();
        auto itr = std::begin(value);
        if (itr != std::end(value)) {
            SerializeOut(*itr);
            itr = std::next(itr);
            while(itr != std::end(value)) {
                json_formatter<beautify>::template WriteComma<false>();
                SerializeOut(*itr);
                itr = std::next(itr);
            }
        }
        WriteBracketClose();
    }

    template <typecheck::map T>
    void SerializeOut(const T &value) {
        WriteBraceOpen();
        auto itr = std::begin(value);
        if (itr != std::end(value)) {
            SerializeOut(itr->first);
            WriteColon();
            SerializeOut(itr->second);
            itr = std::next(itr);
            while(itr != std::end(value)) {
                json_formatter<beautify>::template WriteComma<true>();
                SerializeOut(itr->first);
                WriteColon();
                SerializeOut(itr->second);
                itr = std::next(itr);
            }
        }
        WriteBraceClose();
    }

    void StructSerializeOutStart(const auto &value) {
        WriteBraceOpen();
        SerializeOutFirst(value.first, value.second);
    }

    void StructSerializeOut(const auto &value) {
        SerializeOutSecond(value.first, value.second);
    }

    void StructSerializeOutEnd() {
        WriteBraceClose();
    }
}; // class JsonOut<>

template <>
class json<SerializeType::Out> : public JsonOut<false> {
public:
    using JsonOut<false>::JsonOut;
};

template <SerializeType type, SerializeKeyType SERIALIZE_KEY_TYPE>
class binary { };

template <SerializeKeyType SERIALIZE_KEY_TYPE>
class binaryInBase {
public:
    constexpr static SerializeKeyType serialize_key_type = SERIALIZE_KEY_TYPE;

protected:
    const Stream &inStream;

public:
    binaryInBase(const Stream &inStream) : inStream { inStream } { }

    const auto &GetStream() { return inStream; }
    auto &GetStream() const { return inStream; }

    uint32_t SerializeInVariable() {
        if (inStream.full()) throw exception::BadInputData { inStream };
        const uint32_t val = *inStream++;
        switch(val & 0xc0) {
            default:
            case 0x00: return val;
            case 0x40:
                if (inStream.full()) throw exception::BadInputData { inStream };
                return ((val & 0x3f) << 8) | *inStream++;
            case 0x80: {
                if (inStream.RemainingBuffer() < 3) throw exception::BadInputData { inStream };
                const uint32_t val8 = *inStream++;
                return ((val & 0x3f) << 16) | (val8 << 8) | *inStream++;
            }
            case 0xc0: {
                if (inStream.RemainingBuffer() < 7) throw exception::BadInputData { inStream };
                const uint32_t val16 = *inStream++;
                const uint32_t val8 = *inStream++;
                return ((val & 0x3f) << 24) | (val16 << 16) | (val8 << 8) | *inStream++;
            }
        }
    }

    template <typename T>
    void SerializeIn(T &value) {
        if constexpr (std::is_same_v<char, T>) {
            if (inStream.full()) throw exception::BadInputData { inStream };
            value = *inStream++;
        } else if constexpr (std::is_same_v<bool, T>) {
            if (inStream.full()) throw exception::BadInputData { inStream };
            value = !!(*inStream++);
        } else if constexpr (std::is_enum_v<T>) {
            auto ival = SerializeInVariable();
            value = static_cast<T>(ival);
        }
        else if constexpr (std::integral<T>) {
            if (inStream.RemainingBuffer() < sizeof(T)) throw exception::BadInputData { inStream };
            T source = *reinterpret_cast<const T *>(inStream.curr());
            value = ChangeEndian<std::endian::big, std::endian::native>(source);
            inStream += sizeof(T);
        } else if constexpr (std::is_same_v<std::string, T>) {
            // variable size following string of size
            auto size = SerializeInVariable();
            if (inStream.RemainingBuffer() < size) throw exception::BadInputData { inStream };
            value = std::string { inStream.curr(), inStream.curr() + size };
            inStream += size;
        } else if constexpr (std::floating_point<T>) {
            if (inStream.RemainingBuffer() < sizeof(T)) throw exception::BadInputData { inStream };
            T source = *reinterpret_cast<const T *>(inStream.curr());
            value = ChangeEndian<std::endian::big, std::endian::native>(source);
            inStream += sizeof(T);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, binary<SerializeType::In, SERIALIZE_KEY_TYPE>>) {
            value->SerializeIn(*this);
        } else if constexpr (typecheck::SerializerOutEnabled<T, binary<SerializeType::In, SERIALIZE_KEY_TYPE>>) {
            value.SerializeIn(*this);
        } else if constexpr (typecheck::vector<T>) {
            // variable size following vector members
            auto size = SerializeInVariable();
            for (size_t i = 0; i < size; ++i) {
                typename T::value_type valuetype { };
                SerializeIn(valuetype);
                value.emplace_back(std::move(valuetype));
            }
        } else if constexpr (typecheck::map<T>) {
            // variable size following map members
            auto size = SerializeInVariable();
            for (size_t i = 0; i < size; ++i) {
                typename T::key_type key { };
                SerializeIn(key);
                typename T::mapped_type valuetype { };
                SerializeIn(valuetype);
                value.emplace(std::move(key), std::move(valuetype));
            }
        } else throw exception::BadType { inStream };
    }

    template <typename T>
    void StructSerializeIn(T *obj)
    {
        if constexpr (SERIALIZE_KEY_TYPE == SerializeKeyType::Integer) {
            while(true) {
                auto key = SerializeInVariable();
                if (key == 0) break;
                obj->SerializeInMemberByIdentifier(*this, key);
            }
        } else if constexpr (SERIALIZE_KEY_TYPE == SerializeKeyType::String) {
            while(true) {
                std::string key { };
                SerializeIn(key);
                if (key.empty()) break;
                obj->SerializeInMemberByName(*this, key);
            }
        }
    }

}; // class binaryInBase

template <>
class binary<SerializeType::In, SerializeKeyType::None> : public binaryInBase<SerializeKeyType::None> {
public:
    using binaryInBase<SerializeKeyType::None>::binaryInBase;
}; // class binary<SerializeType::In, SerializeKeyType::None>

template <>
class binary<SerializeType::In, SerializeKeyType::Integer> : public binaryInBase<SerializeKeyType::Integer> {
public:
    using binaryInBase<SerializeKeyType::Integer>::binaryInBase;
}; // class binary<SerializeType::In, SerializeKeyType::Integer>

template <>
class binary<SerializeType::In, SerializeKeyType::String> : public binaryInBase<SerializeKeyType::String> {
public:
    using binaryInBase<SerializeKeyType::String>::binaryInBase;
}; // class binary<SerializeType::In, SerializeKeyType::String>

template <SerializeKeyType SERIALIZE_KEY_TYPE>
class binaryOutBase {
public:
    constexpr static SerializeKeyType serialize_key_type = SERIALIZE_KEY_TYPE;

protected:
    Stream &outStream;

public:
    binaryOutBase(Stream &outStream) : outStream { outStream } { }

    const auto &GetStream() { return outStream; }
    auto &GetStream() const { return outStream; }

protected:
    template <typename T>
    void SerializeOut(const std::integral auto &id, const T &value) {
        SerializeOutVariable(id);
        SerializeOut(value);
    }

    template <typename T>
    void SerializeOut(const std::string &name, const T &value) {
        SerializeOut(name);
        SerializeOut(value);
    }

    template <typename T>
    void SerializeOut(const std::string_view &name, const T &value) {
        SerializeOut(name);
        SerializeOut(value);
    }

    template <typename T, typename U>
    void SerializeOut(const std::pair<T, U> &value) {
        SerializeOut(value.first, value.second);
    }

    template <typename T>
    void SerializeOut(const std::integral auto &id, const std::integral auto &index, const T &value) {
        SerializeOutVariable(id);
        SerializeOutVariable(index);
        SerializeOut(value);
    }

    template <typename T, typename U, typename V>
    void SerializeOut(const std::tuple<T, U, V> &value) {
        SerializeOut(std::get<0>(value), std::get<1>(value), std::get<2>(value));
    }

public:
    void SerializeOutVariable(const std::integral auto id) {
        if (id <= 0x3f) {
            outStream.WriteRaw(static_cast<uint8_t>(id));
        } else if (id <= 0x3fff) {
            outStream.WriteRaw(
                static_cast<uint8_t>(((id >> 8) | 0x40)),
                static_cast<uint8_t>(id & 0xff)
            );
        } else if (id <= 0x3fffff) {
            outStream.WriteRaw(
                static_cast<uint8_t>(((id >> 16) | 0x80)),
                static_cast<uint8_t>((id >> 8) & 0xff),
                static_cast<uint8_t>(id & 0xff)
            );
        } else if (id <= 0x3fffffff) {
            outStream.WriteRaw(
                static_cast<uint8_t>(((id >> 24) | 0xc0)),
                static_cast<uint8_t>((id >> 16) & 0xff),
                static_cast<uint8_t>((id >> 8) & 0xff),
                static_cast<uint8_t>(id & 0xff)
            );
        }
    }

    template <typename T>
    void SerializeOut(const T &value) {
        if constexpr (std::is_same_v<char, T>) {
            outStream += sizeof(T);
            *(outStream.curr() - sizeof(T)) = value;
        } else if constexpr (std::is_same_v<bool, T>) {
            outStream += sizeof(T);
            *(outStream.curr() - sizeof(T)) = value;
        } else if constexpr (std::is_enum_v<T>) {
            SerializeOutVariable(static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (std::integral<T>) {
            outStream += sizeof(T);
            auto dest = reinterpret_cast<T *>(outStream.curr() - sizeof(T));
            *dest = ChangeEndian<std::endian::native, std::endian::big>(value);
        } else if constexpr (std::is_same_v<std::string, T>) {
            // variable size following string of size
            SerializeOutVariable(value.size());
            outStream.Append(value);
        } else if constexpr (std::is_same_v<std::string_view, T>) {
            // variable size following string of size
            SerializeOutVariable(value.size());
            outStream.Append(value);
        } else if constexpr (std::floating_point<T>) {
            outStream += sizeof(T);
            auto dest = reinterpret_cast<T *>(outStream.curr() - sizeof(T));
            *dest = ChangeEndian<std::endian::native, std::endian::big>(value);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, binary<SerializeType::Out, SERIALIZE_KEY_TYPE>>) {
            value->SerializeOut(*this);
        } else if constexpr (typecheck::SerializerOutEnabled<T, binary<SerializeType::Out, SERIALIZE_KEY_TYPE>>) {
            value.SerializeOut(*this);
        } else if constexpr (typecheck::vector<T>) {
            SerializeOutVariable(value.size());
            for (const auto &item : value) {
                SerializeOut(item);
            }
        } else if constexpr (typecheck::map<T>) {
            SerializeOutVariable(value.size());
            for (const auto &item : value) {
                SerializeOut(item.first);
                SerializeOut(item.second);
            }
        } else {
            throw exception::BadType {outStream, "Bad Type, this is internal error."};
        }
    }

    template <typecheck::functions T>
    void SerializeOut(const T &value) {
        value(outStream);
    }

    void StructSerializeOutStart(const auto &value) {
        StructSerializeOut(value);
    }

    void StructSerializeOut(const auto &value) {
        SerializeOut(value);
    }

    void StructSerializeOutEnd() {
        if constexpr (SERIALIZE_KEY_TYPE == SerializeKeyType::Integer) {
            SerializeOutVariable(0U);
        } else if constexpr (SERIALIZE_KEY_TYPE == SerializeKeyType::String) {
            std::string empty { };
            SerializeOut(empty);
        }
    }
}; // class binaryOutBase 

template <>
class binary<SerializeType::Out, SerializeKeyType::None> : public binaryOutBase<SerializeKeyType::None> {
public:
    using binaryOutBase<SerializeKeyType::None>::binaryOutBase;
}; // class binary<SerializeType::Out, SerializeKeyType::None>

template <>
class binary<SerializeType::Out, SerializeKeyType::Integer> : public binaryOutBase<SerializeKeyType::Integer> {
public:
    using binaryOutBase<SerializeKeyType::Integer>::binaryOutBase;
}; // class binary<SerializeType::Out, SerializeKeyType::Integer>

template <>
class binary<SerializeType::Out, SerializeKeyType::String> : public binaryOutBase<SerializeKeyType::String> {
public:
    using binaryOutBase<SerializeKeyType::String>::binaryOutBase;
}; // class binary<SerializeType::Out, SerializeKeyType::String>

template <SerializeType type>
using binary_integer = binary<type, SerializeKeyType::Integer>;

template <SerializeType type>
using binary_string = binary<type, SerializeKeyType::String>;

template <SerializeType type>
using binary_none = binary<type, SerializeKeyType::None>;

} // namespace rohit::serializer