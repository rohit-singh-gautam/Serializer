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
concept SerializerInEnabled = requires(T cls, Stream &stream) {
    { cls->template SerializeIn<J>(stream) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabledPtr = requires(T cls, Stream &stream) {
    { cls->template SerializeOut<J>(stream) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabled = requires(T cls, Stream &stream) {
    { cls.template SerializeOut<J>(stream) } -> std::same_as<void>;
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

class json {
public:
    constexpr static SerializeKeyType serialize_key_type = SerializeKeyType::String;

private:
    static constexpr bool IsWhiteSpace(const char val) noexcept { return val == ' ' || val == '\t' || val == '\n' || val == '\r'; }
    static void SkipWhiteSpace(const Stream &inStream) { while(IsWhiteSpace(*inStream)) ++inStream; }

    template <typename T>
    static void SerializeOutFirst(Stream &stream, auto &name, const T &value) {
        stream.Write('"', name, "\":");
        SerializeOut(stream, value);
    }

    template <typename T>
    static void SerializeOutSecond(Stream &stream, auto &name, const T &value) {
        stream.Write(",\"", name, "\":");
        SerializeOut(stream, value);
    }

    static void CheckAndIncrease(const FullStream &stream, char value) {
        if (*stream != value) {
            std::string errStr { "Expected " };
            errStr.push_back(value);
            errStr += " but found ";
            errStr.push_back(*stream);
            throw exception::BadInputData { stream , std::move(errStr) };
        }
        ++stream;
    }

    static const std::string_view SerializeInGetKey(const FullStream &stream) {
        SkipWhiteSpace(stream);
        CheckAndIncrease(stream, '"');
        auto start = stream.curr();
        // TODO:: Escape character
        while(*stream != '"') ++stream;
        auto end = stream.curr();
        ++stream;
        SkipWhiteSpace(stream);
        CheckAndIncrease(stream, ':');
        SkipWhiteSpace(stream);
        return { reinterpret_cast<const char *>(start), reinterpret_cast<const char *>(end) };
    }

    static void SerializeInBool(const FullStream &stream, bool &value) {
        if (stream.RemainingBuffer() < 4) throw exception::BadInputData { stream };
        auto ch = std::tolower(*stream);
        if (ch == 't') {
            ++stream;
            if (std::tolower(*stream) != 'r') throw exception::BadInputData { stream };
            ++stream;
            if (std::tolower(*stream) != 'u') throw exception::BadInputData { stream };
            ++stream;
            if (std::tolower(*stream) != 'e') throw exception::BadInputData { stream };
            ++stream;
            value = true;
        } else if (ch == 'f') {
            ++stream;
            if (std::tolower(*stream) != 'a') throw exception::BadInputData { stream };
            ++stream;
            if (std::tolower(*stream) != 'l') throw exception::BadInputData { stream };
            ++stream;
            if (std::tolower(*stream) != 's') throw exception::BadInputData { stream };
            ++stream;
            if (stream.full()) throw exception::BadInputData { stream };
            if (std::tolower(*stream) != 'e') throw exception::BadInputData { stream };
            ++stream;
            value = false;
        }
    }

    static void SerializeInChar(const FullStream &stream, char &value) {
        if (stream.RemainingBuffer() < 3) throw exception::BadInputData { stream };
        if (*stream != '"') throw exception::BadInputData { stream };
        ++stream;
        value = *stream;
        ++stream;
        if (*stream != '"') throw exception::BadInputData { stream };
        ++stream;
    }

    static void SerializeInUnsignedInteger(const FullStream &stream, std::unsigned_integral auto &value) {
        if (stream.full()) throw exception::BadInputData { stream };
        // TODO: Check out of range values
        if (*stream < '0' || *stream > '9') throw exception::BadInputData { stream };
        value = *stream - '0';
        ++stream;
        while(!stream.full()) {
            if (*stream < '0' || *stream > '9') break;
            value = value * 10 + *stream - '0';
            ++stream;
        }
    }

    static void SerializeInSignedInteger(const FullStream &stream, std::signed_integral auto &value) {
        if (stream.full()) throw exception::BadInputData { stream };
        // TODO: Check out of range values
        if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
        auto sign { static_cast<std::remove_reference_t<decltype(value)>>(1) };
        if (*stream == '-') {
            sign = -1;
            ++stream;
        } else if (*stream == '+') ++stream;
        value = *stream - '0';
        ++stream;
        while(!stream.full()) {
            if (*stream < '0' || *stream > '9') break;
            value = value * 10 + *stream - '0';
            ++stream;
        }
        value *= sign;
    }

    static void SerializeInString(const FullStream &stream, std::string &value) {
        if (stream.RemainingBuffer() < 2) throw exception::BadInputData { stream };
        if (*stream != '"') throw exception::BadInputData { stream };
        ++stream;
        while(*stream != '"') {
            if (stream.full()) throw exception::BadInputData { stream };
            value.push_back(*stream);
            ++stream;
        }
        ++stream;
    }

    static void SerializeInFloatingPoint(const FullStream &stream, std::floating_point auto &value) {
        if (stream.full()) throw exception::BadInputData { stream };
        // TODO: Check out of range values
        if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
        auto start = stream.curr();
        while (!stream.full() && *stream != ',' && *stream != '!' && *stream != ']' && *stream != '}' && *stream != ' ')
            ++stream;
        std::string number {start, stream.curr()};
        if constexpr (std::is_same_v<float, std::remove_reference_t<decltype(value)>>) value = std::stof(number);
        else value = std::stod(number);
    }

    static void SerializeInVector(const FullStream &stream, typecheck::vector auto &value) {
        CheckAndIncrease(stream, '[');
        SkipWhiteSpace(stream);
        if (*stream != ']') {
            while(true) {
                using value_type = std::remove_reference_t<decltype(value)>::value_type;
                value_type valuetype { };
                SerializeIn(stream, valuetype);
                value.emplace_back(valuetype);
                SkipWhiteSpace(stream);
                if (*stream == ']') break;
                CheckAndIncrease(stream, ',');
                SkipWhiteSpace(stream);
                if (*stream == ']') {
                    throw exception::BadInputData { stream, "Unexpected ',', there must be next array entry after ','" };
                }
            }
        }
        ++stream;
    }

    static void SerializeInMap(const FullStream &stream, typecheck::map auto &value) {
        CheckAndIncrease(stream, '{');
        SkipWhiteSpace(stream);
        if (*stream != '}') {
            while(true) {
                using T = std::remove_reference_t<decltype(value)>;
                typename T::key_type key { };
                SerializeIn(stream, key);
                SkipWhiteSpace(stream);
                CheckAndIncrease(stream, ':');
                SkipWhiteSpace(stream);
                typename T::mapped_type valuetype { };
                SerializeIn(stream, valuetype);
                value.emplace(std::move(key), std::move(valuetype));
                SkipWhiteSpace(stream);
                if (*stream == '}') break;
                CheckAndIncrease(stream, ',');
                SkipWhiteSpace(stream);
                if (*stream == '}') {
                    throw exception::BadInputData { stream, "Unexpected ',', there must be next map entry after ','" };
                }
            }
        }
        ++stream;
    }

public:
    template <typename T>
    static void SerializeIn(const FullStream &stream, T &value) {
        if constexpr (std::is_same_v<bool, T>) {
            SerializeInBool(stream, value);
        } else if constexpr (std::is_same_v<char, T>) {
            SerializeInChar(stream, value);
        } else if constexpr (std::unsigned_integral<T>) {
            SerializeInUnsignedInteger(stream, value);
        } else if constexpr (std::signed_integral<T>) {
            SerializeInSignedInteger(stream, value);
        } else if constexpr (std::is_same_v<std::string, T>) {
            SerializeInString(stream, value);
        } else if constexpr (std::floating_point<T>) {
            SerializeInFloatingPoint(stream, value);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, json>) {
            value->template SerializeIn<json>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json>) {
            value.template SerializeIn<json>(stream);
        } else if constexpr (typecheck::vector<T>) {
            SerializeInVector(stream, value);
        } else if constexpr (typecheck::map<T>) {
            SerializeInMap(stream, value);
        } else throw exception::BadType { stream };
    }

    template <typename T>
    static void StructSerializeIn(
        const FullStream &stream,
        const std::unordered_map<std::string_view, std::function<void(const rohit::FullStream &, T &)>>  &membermap,
        T &obj )
    {
        static_assert(serialize_key_type == SerializeKeyType::String, "Only String key type supported");
        SkipWhiteSpace(stream);
        CheckAndIncrease(stream, '{');
        SkipWhiteSpace(stream);
        while(true) {
            auto key = SerializeInGetKey(stream);
            auto itr = membermap.find(key);
            if (itr == std::end(membermap)) {
                std::string errorstr { "Key: " };
                errorstr += key;
                errorstr += " not present";
                throw exception::KeyNotFound {stream, std::move(errorstr)};
            }
            itr->second(stream, obj);
            SkipWhiteSpace(stream);
            if (*stream == '}') break;
            CheckAndIncrease(stream, ',');
            SkipWhiteSpace(stream);
            if (*stream == '}') {
                throw exception::BadInputData { stream, "Unexpected ',', there next object expected after ','" };
            }
        }
        ++stream;
    }

    template <typename T>
    static void SerializeOut(Stream &stream, const T &value) {
        if constexpr (typecheck::SerializerOutEnabledPtr<T, json>) {
            value->template SerializeOut<json>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json>) {
            value.template SerializeOut<json>(stream);
        } else {
            throw exception::UnknownSerializationType {stream, "Unknown Serialization Type"};
        }
    }

    template <typecheck::functions T>
    static void SerializeOut(Stream &stream, const T &value) {
        value(stream);
    }

    template <std::integral T>
    static void SerializeOut(Stream &stream, const T &value) {
        stream.Copy(value);
    }

    template <std::floating_point T>
    static void SerializeOut(Stream &stream, const T &value) {
        char buffer[std::numeric_limits<T>::digits10 + 3] { 0 };
        auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
        stream.Copy(buffer, result.ptr);
    }

    template <typecheck::vector T>
    static void SerializeOut(Stream &stream, const T &value) {
        stream.Write('[');
        auto itr = std::begin(value);
        if (itr != std::end(value)) {
            SerializeOut(stream, *itr);
            itr = std::next(itr);
            while(itr != std::end(value)) {
                stream.Write(',');
                SerializeOut(stream, *itr);
                itr = std::next(itr);
            }
        }
        stream.Write(']');
    }

    template <typecheck::map T>
    static void SerializeOut(Stream &stream, const T &value) {
        stream.Write('{');
        auto itr = std::begin(value);
        if (itr != std::end(value)) {
            SerializeOut(stream, itr->first);
            stream.Write(':');
            SerializeOut(stream, itr->second);
            itr = std::next(itr);
            while(itr != std::end(value)) {
                stream.Write(',');
                SerializeOut(stream, itr->first);
                stream.Write(':');
                SerializeOut(stream, itr->second);
                itr = std::next(itr);
            }
        }
        stream.Write('}');
    }

    static void StructSerializeOutStart(Stream &stream, const auto &value) {
        stream.Write('{');
        SerializeOutFirst(stream, value.first, value.second);
    }

    static void StructSerializeOut(Stream &stream, const auto &value) {
        SerializeOutSecond(stream, value.first, value.second);
    }

    static void StructSerializeOutEnd(Stream &stream) {
        stream.Write('}');
    }
}; // class json

template<>
inline void json::SerializeOut<char>(Stream &stream, const char &value) {
    stream.Write('"', value, '"');
}

template<>
inline void json::SerializeOut<bool>(Stream &stream, const bool &value) {
    if (value) stream.Copy("TRUE");
    else stream.Copy("FALSE");
}

template<>
inline void json::SerializeOut<std::string>(Stream &stream, const std::string &value) {
    stream.Write('"', value, '"');
}

template<>
inline void json::SerializeOut<std::string_view>(Stream &stream, const std::string_view &value) {
    stream.Write('"', value, '"');
}

template <SerializeKeyType SERIALIZE_KEY_TYPE = SerializeKeyType::Integer>
class binary {
public:
    constexpr static SerializeKeyType serialize_key_type = SERIALIZE_KEY_TYPE;

private:
    template <typename T>
    static void SerializeOut(Stream &stream, const std::integral auto &id, const T &value) {
        SerializeOutVariable(stream, id);
        SerializeOut(stream, value);
    }

    template <typename T>
    static void SerializeOut(Stream &stream, const std::string &name, const T &value) {
        SerializeOut(stream, name);
        SerializeOut(stream, value);
    }

    template <typename T>
    static void SerializeOut(Stream &stream, const std::string_view &name, const T &value) {
        SerializeOut(stream, name);
        SerializeOut(stream, value);
    }

    template <typename T, typename U>
    static void SerializeOut(Stream &stream, const std::pair<T, U> &value) {
        SerializeOut(stream, value.first, value.second);
    }

    template <typename T>
    static void SerializeOut(Stream &stream, const std::integral auto &id, const std::integral auto &index, const T &value) {
        SerializeOutVariable(stream, id);
        SerializeOutVariable(stream, index);
        SerializeOut(stream, value);
    }

    template <typename T, typename U, typename V>
    static void SerializeOut(Stream &stream, const std::tuple<T, U, V> &value) {
        SerializeOut(stream, std::get<0>(value), std::get<1>(value), std::get<2>(value));
    }

public:
    static void SerializeOutVariable(Stream &stream, const std::integral auto id) {
        if (id <= 0x3f) {
            *stream++ = static_cast<uint8_t>(id);
        } else if (id <= 0x3fff) {
            *stream++ = static_cast<uint8_t>(((id >> 8) | 0x40));
            *stream++ = static_cast<uint8_t>(id & 0xff);
        } else if (id <= 0x3fffff) {
            *stream++ = static_cast<uint8_t>(((id >> 16) | 0x80));
            *stream++ = static_cast<uint8_t>((id >> 8) & 0xff);
            *stream++ = static_cast<uint8_t>(id & 0xff);
        } else if (id <= 0x3fffffff) {
            *stream++ = static_cast<uint8_t>(((id >> 24) | 0xc0));
            *stream++ = static_cast<uint8_t>((id >> 16) & 0xff);
            *stream++ = static_cast<uint8_t>((id >> 8) & 0xff);
            *stream++ = static_cast<uint8_t>(id & 0xff);
        }
    }

    static uint32_t SerializeInVariable(const FullStream &stream) {
        if (stream.full()) throw exception::BadInputData { stream };
        const uint32_t val = *stream++;
        switch(val & 0xc0) {
            default:
            case 0x00: return val;
            case 0x40:
                if (stream.full()) throw exception::BadInputData { stream };
                return ((val & 0x3f) << 8) | *stream++;
            case 0x80: {
                if (stream.RemainingBuffer() < 3) throw exception::BadInputData { stream };
                const uint32_t val8 = *stream++;
                return ((val & 0x3f) << 16) | (val8 << 8) | *stream++;
            }
            case 0xc0: {
                if (stream.RemainingBuffer() < 7) throw exception::BadInputData { stream };
                const uint32_t val16 = *stream++;
                const uint32_t val8 = *stream++;
                return ((val & 0x3f) << 24) | (val16 << 16) | (val8 << 8) | *stream++;
            }
        }
    }

    template <typename T>
    static void SerializeIn(const FullStream &stream, T &value) {
        if constexpr (std::is_same_v<char, T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            value = *stream++;
        } else if constexpr (std::is_same_v<bool, T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            value = !!(*stream++);
        } else if constexpr (std::is_enum_v<T>) {
            auto ival = SerializeInVariable(stream);
            value = static_cast<T>(ival);
        }
        else if constexpr (std::integral<T>) {
            if (stream.RemainingBuffer() < sizeof(T)) throw exception::BadInputData { stream };
            T source = *reinterpret_cast<const T *>(stream.curr());
            value = ChangeEndian<std::endian::big, std::endian::native>(source);
            stream += sizeof(T);
        } else if constexpr (std::is_same_v<std::string, T>) {
            // variable size following string of size
            auto size = SerializeInVariable(stream);
            if (stream.RemainingBuffer() < size) throw exception::BadInputData { stream };
            value = std::string { stream.curr(), stream.curr() + size };
            stream += size;
        } else if constexpr (std::floating_point<T>) {
            if (stream.RemainingBuffer() < sizeof(T)) throw exception::BadInputData { stream };
            T source = *reinterpret_cast<const T *>(stream.curr());
            value = ChangeEndian<std::endian::big, std::endian::native>(source);
            stream += sizeof(T);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, binary<SERIALIZE_KEY_TYPE>>) {
            value->template SerializeIn<binary<SERIALIZE_KEY_TYPE>>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, binary<SERIALIZE_KEY_TYPE>>) {
            value.template SerializeIn<binary<SERIALIZE_KEY_TYPE>>(stream);
        } else if constexpr (typecheck::vector<T>) {
            // variable size following vector members
            auto size = SerializeInVariable(stream);
            for (size_t i = 0; i < size; ++i) {
                typename T::value_type valuetype { };
                SerializeIn(stream, valuetype);
                value.emplace_back(std::move(valuetype));
            }
        } else if constexpr (typecheck::map<T>) {
            // variable size following map members
            auto size = SerializeInVariable(stream);
            for (size_t i = 0; i < size; ++i) {
                typename T::key_type key { };
                SerializeIn(stream, key);
                typename T::mapped_type valuetype { };
                SerializeIn(stream, valuetype);
                value.emplace(std::move(key), std::move(valuetype));
            }
        } else throw exception::BadType { stream };
    }

    template <typename T>
    static void StructSerializeIn(
        const FullStream &stream,
        const std::unordered_map<uint32_t, std::function<void(const rohit::FullStream &, T &)>>  &membermap,
        T &obj )
    {
        static_assert(serialize_key_type == SerializeKeyType::Integer, "Only Integer key type supported");
        while(true) {
            auto key = SerializeInVariable(stream);
            if (key == 0) break;
            auto itr = membermap.find(key);
            if (itr == std::end(membermap)) {
                std::string errorstr { "Key: " };
                errorstr += std::to_string(key);
                errorstr += " not found";
                throw exception::KeyNotFound {stream, std::move(errorstr)};
            }
            itr->second(stream, obj);
        }
    }

    template <typename T>
    static void StructSerializeIn(
        const FullStream &stream,
        const std::unordered_map<std::string_view, std::function<void(const rohit::FullStream &, T &)>>  &membermap,
        T &obj )
    {
        static_assert(serialize_key_type == SerializeKeyType::String, "Only String key type supported");
        while(true) {
            std::string key { };
            SerializeIn(stream, key);
            if (key.empty()) break;
            auto itr = membermap.find(key);
            if (itr == std::end(membermap)) {
                std::string errorstr { "Key: " };
                errorstr += key;
                errorstr += " not found";
                throw exception::KeyNotFound {stream, std::move(errorstr)};
            }
            itr->second(stream, obj);
        }
    }

    template <typename T>
    static void SerializeOut(Stream &stream, const T &value) {
        if constexpr (std::is_same_v<char, T>) {
            stream += sizeof(T);
            *(stream.curr() - sizeof(T)) = value;
        } else if constexpr (std::is_same_v<bool, T>) {
            stream += sizeof(T);
            *(stream.curr() - sizeof(T)) = value;
        } else if constexpr (std::is_enum_v<T>) {
            SerializeOutVariable(stream, static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (std::integral<T>) {
            stream += sizeof(T);
            auto dest = reinterpret_cast<T *>(stream.curr() - sizeof(T));
            *dest = ChangeEndian<std::endian::native, std::endian::big>(value);
        } else if constexpr (std::is_same_v<std::string, T>) {
            // variable size following string of size
            SerializeOutVariable(stream, value.size());
            stream.Copy(value);
        } else if constexpr (std::is_same_v<std::string_view, T>) {
            // variable size following string of size
            SerializeOutVariable(stream, value.size());
            stream.Copy(value);
        } else if constexpr (std::floating_point<T>) {
            stream += sizeof(T);
            auto dest = reinterpret_cast<T *>(stream.curr() - sizeof(T));
            *dest = ChangeEndian<std::endian::native, std::endian::big>(value);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, binary<SERIALIZE_KEY_TYPE>>) {
            value->template SerializeOut<binary<SERIALIZE_KEY_TYPE>>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, binary<SERIALIZE_KEY_TYPE>>) {
            value.template SerializeOut<binary<SERIALIZE_KEY_TYPE>>(stream);
        } else if constexpr (typecheck::vector<T>) {
            SerializeOutVariable(stream, value.size());
            for (const auto &item : value) {
                SerializeOut(stream, item);
            }
        } else if constexpr (typecheck::map<T>) {
            SerializeOutVariable(stream, value.size());
            for (const auto &item : value) {
                SerializeOut(stream, item.first);
                SerializeOut(stream, item.second);
            }
        } else {
            throw exception::BadType {stream, "Bad Type, this is internal error."};
        }
    }

    template <typecheck::functions T>
    static void SerializeOut(Stream &stream, const T &value) {
        value(stream);
    }

    static void StructSerializeOutStart(Stream &stream, const auto &value) {
        StructSerializeOut(stream, value);
    }

    static void StructSerializeOut(Stream &stream, const auto &value) {
        SerializeOut(stream, value);
    }

    static void StructSerializeOutEnd(Stream &stream) {
        if constexpr (serialize_key_type == SerializeKeyType::Integer) {
            SerializeOutVariable(stream, 0U);
        } else if constexpr (serialize_key_type == SerializeKeyType::String) {
            std::string empty { };
            SerializeOut(stream, empty);
        }
    }
}; // class binary

using binary_integer = binary<SerializeKeyType::Integer>;
using binary_string = binary<SerializeKeyType::String>;
using binary_none = binary<SerializeKeyType::None>;

} // namespace rohit::serializer