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

} // namespace exception

namespace typecheck {
template <typename T, typename J>
concept SerializerInEnabled = requires(T cls, Stream &stream) {
    { cls->template serialize_in<J>(stream) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabledPtr = requires(T cls, Stream &stream) {
    { cls->template serialize_out<J>(stream) } -> std::same_as<void>;
};

template <typename T, typename J>
concept SerializerOutEnabled = requires(T cls, Stream &stream) {
    { cls.template serialize_out<J>(stream) } -> std::same_as<void>;
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

class json {
    static constexpr bool IsWhiteSpace(const char val) noexcept { return val == ' ' || val == '\t' || val == '\n' || val == '\r'; }
    static void SkipWhiteSpace(const Stream &inStream) { while(IsWhiteSpace(*inStream)) ++inStream; }

    template <typename T>
    static constexpr void serialize_out_first(Stream &stream, auto &name, const T &value) {
        stream.Write('"', name, "\":");
        serialize_out(stream, value);
    }

    template <typename T>
    static constexpr void serialize_out_second(Stream &stream, auto &name, const T &value) {
        stream.Write(",\"", name, "\":");
        serialize_out(stream, value);
    }

    static constexpr void check_in(const FullStream &stream, char value) {
        if (*stream != value) throw exception::BadInputData { stream };
        ++stream;
    }

    static constexpr const std::string_view serialize_in_get_key(const FullStream &stream) {
        SkipWhiteSpace(stream);
        check_in(stream, '"');
        auto start = stream.curr();
        // TODO:: Escape character
        while(*stream != '"') ++stream;
        auto end = stream.curr();
        ++stream;
        SkipWhiteSpace(stream);
        check_in(stream, ':');
        SkipWhiteSpace(stream);
        return { reinterpret_cast<const char *>(start), reinterpret_cast<const char *>(end) };
    }

public:
    template <typename T>
    static constexpr void serialize_in(const FullStream &stream, T &value) {
        if constexpr (std::is_same_v<bool, T>) {
            if (stream.remaining_buffer() < 4) throw exception::BadInputData { stream };
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
        } else if constexpr (std::is_same_v<char, T>) {
            if (stream.remaining_buffer() < 3) throw exception::BadInputData { stream };
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
            value = *stream;
            ++stream;
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
        } else if constexpr (std::unsigned_integral<T>) {
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
        } else if constexpr (std::signed_integral<T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
            T sign { 1 };
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
        } else if constexpr (std::is_same_v<std::string, T>) {
            if (stream.remaining_buffer() < 2) throw exception::BadInputData { stream };
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
            while(*stream != '"') {
                if (stream.full()) throw exception::BadInputData { stream };
                value.push_back(*stream);
                ++stream;
            }
            ++stream;
        } else if constexpr (std::is_same_v<float, T> || std::is_same_v<double, T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
            auto start = stream.curr();
            while (!stream.full() && *stream != ',' && *stream != '!' && *stream != ']' && *stream != '}' && *stream != ' ')
                ++stream;
            std::string number {start, stream.curr()};
            if constexpr (std::is_same_v<float, T>) value = std::stof(number);
            else value = std::stod(number);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, json>) {
            value->template serialize_in<json>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json>) {
            value.template serialize_in<json>(stream);
        } else if constexpr (typecheck::vector<T>) {
            check_in(stream, '[');
            SkipWhiteSpace(stream);
            if (*stream != ']') {
                while(true) {
                    typename T::value_type valuetype { };
                    serialize_in(stream, valuetype);
                    value.emplace_back(valuetype);
                    SkipWhiteSpace(stream);
                    if (*stream == ']') break;
                    check_in(stream, ',');
                    SkipWhiteSpace(stream);
                }
            }
            ++stream;
        } else if constexpr (typecheck::map<T>) {
            check_in(stream, '{');
            SkipWhiteSpace(stream);
            if (*stream != '}') {
                while(true) {
                    typename T::key_type key { };
                    serialize_in(stream, key);
                    SkipWhiteSpace(stream);
                    check_in(stream, ':');
                    SkipWhiteSpace(stream);
                    typename T::mapped_type valuetype { };
                    serialize_in(stream, valuetype);
                    value.emplace(std::move(key), std::move(valuetype));
                    SkipWhiteSpace(stream);
                    if (*stream == '}') break;
                    check_in(stream, ',');
                    SkipWhiteSpace(stream);
                }
            }
            ++stream;
        } else throw exception::BadType { stream };
    }

    template <typename ... Types>
    static constexpr void struct_serialize_in(const FullStream &stream, Types ... values) {
        std::unordered_map<std::string_view, std::function<void(const rohit::FullStream &)>> membermap { values... };
        check_in(stream, '{');
        while(true) {
            SkipWhiteSpace(stream);
            auto key = serialize_in_get_key(stream);
            auto itr = membermap.find(key);
            if (itr == std::end(membermap)) throw std::runtime_error("Key not present");
            itr->second(stream);
            if (*stream == '}') {
                break;
            }
            check_in(stream, ',');
        }
        ++stream;
    }

    template <typename T>
    static constexpr void serialize_out(Stream &stream, const T &value) {
        if constexpr (std::is_same_v<T, void(Stream &)> || std::is_function_v<T> || std::is_same_v<T, std::function<void(Stream &)>>) {
            value(stream);
        } else if constexpr (std::is_same_v<char, T>) {
            *stream++ = '"';
            *stream++ = value;
            *stream++ = '"';
        } else if constexpr (std::is_same_v<bool, T>) {
            if (value) stream.Copy("TRUE");
            else stream.Copy("FALSE");
        } else if constexpr (std::integral<T>) {
            stream.Copy(value);
        } else if constexpr (std::is_same_v<std::string, T>) {
            *stream++ = '"';
            stream.Copy(value);
            *stream++ = '"';
        } else if constexpr (std::is_same_v<float, T> || std::is_same_v<double, T>) {
            char buffer[std::numeric_limits<T>::digits10 + 3] { 0 };
            auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
            stream.Copy(buffer, result.ptr);
        } else if constexpr (typecheck::SerializerOutEnabledPtr<T, json>) {
            value->template serialize_out<json>(stream);
        } else if constexpr (typecheck::SerializerOutEnabled<T, json>) {
            value.template serialize_out<json>(stream);
        } else if constexpr (typecheck::vector<T>) {
            stream.Write('[');
            auto itr = std::begin(value);
            if (itr != std::end(value)) {
                serialize_out(stream, *itr);
                itr = std::next(itr);
                while(itr != std::end(value)) {
                    stream.Write(',');
                    serialize_out(stream, *itr);
                    itr = std::next(itr);
                }
            }
            stream.Write(']');
        } else if constexpr (typecheck::map<T>) {
            stream.Write('{');
            auto itr = std::begin(value);
            if (itr != std::end(value)) {
                serialize_out(stream, itr->first);
                stream.Write(':');
                serialize_out(stream, itr->second);
                itr = std::next(itr);
                while(itr != std::end(value)) {
                    stream.Write(',');
                    serialize_out(stream, itr->first);
                    stream.Write(':');
                    serialize_out(stream, itr->second);
                    itr = std::next(itr);
                }
            }
            stream.Write('}');
        } else {
            // TODO: Improve exception
            throw std::runtime_error {"Bad Type"};
        }
    }

    static constexpr void struct_serialize_out_start(Stream &stream, const auto &value) {
        stream.Write('{');
        serialize_out_first(stream, value.first, value.second);
    }

    static constexpr void struct_serialize_out(Stream &stream, const auto &value) {
        serialize_out_second(stream, value.first, value.second);
    }

    static constexpr void struct_serialize_out_end(Stream &stream) {
        stream.Write('}');
    }
};

} // namespace rohit::serializer