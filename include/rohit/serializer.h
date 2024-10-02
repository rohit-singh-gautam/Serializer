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

namespace rohit::serializer {
namespace exception {

class BadInputData : std::exception {
    const FullStream stream;

public:
    BadInputData(const FullStream &stream) : stream { stream } { }

    const char* what() const noexcept override {
        // TODO: Point where error is
        return "BadException";
    }
};


} // namespace exception

namespace typecheck {
template<typename Stream>
concept Reserve = requires(Stream stream) {
    { stream.Reserve() } -> std::same_as<void>;
};

template<typename Stream>
concept remaining_buffer = requires(Stream stream) {
    { stream.remaining_buffer() } -> std::same_as<std::size_t>;
};
}

class json {
public:
    template <typename T>
    static constexpr T serialize_in(const FullStream &stream) {
        if constexpr (std::unsigned_integral<T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if (*stream < '0' || *stream > '9') throw exception::BadInputData { stream };
            ++stream;
            T value = *stream - '0';
            while(!stream.full()) {
                if (*stream < '0' || *stream > '9') break;
                value = value * 10 + *stream - '0';
                ++stream;
            }
            return value;
        } else if constexpr (std::signed_integral<T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
            T sign { 1 };
            if (*stream == '-') {
                sign = -1;
                ++stream;
            } else if (*stream == '+') ++stream;
            T value = *stream - '0';
            while(!stream.full()) {
                if (*stream < '0' || *stream > '9') break;
                value = value * 10 + *stream - '0';
                ++stream;
            }
            return value * sign;
        } else if constexpr (std::is_same_v<char, T>) {
            if (stream.remaining_buffer() < 3) throw exception::BadInputData { stream };
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
            T value = *stream;
            ++stream;
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
            return value;
        } else if constexpr (std::is_same_v<std::string, T>) {
            if (stream.remaining_buffer() < 2) throw exception::BadInputData { stream };
            if (*stream != '"') throw exception::BadInputData { stream };
            ++stream;
            T value { };
            while(*stream != '"') {
                if (stream.full()) throw exception::BadInputData { stream };
                value.push_back(*stream);
                ++stream;
            }
            ++stream;
            return value;
        } else if constexpr (std::is_same_v<bool, T>) {
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
                return true;
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
                return true;
            }
        } else if constexpr (std::is_same_v<float, T> || std::is_same_v<double, T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*stream < '0' || *stream > '9') && *stream != '-' && *stream != '+') throw exception::BadInputData { stream };
            auto start = stream.curr();
            while (!stream.full() && *stream != ',' && *stream != '!' && *stream != ']' && *stream != '}' && *stream != ' ')
                ++stream;
            std::string number {start, stream.curr()};
            if constexpr (std::is_same_v<float, T>) return std::stof(number);
            else return std::stod(number);
        }

        return true;
    }

    /*template <typename T>
    static constexpr void serialize_out(T ) {
        if constexpr (std::unsigned_integral<T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if (*begin < '0' || *begin > '9') throw exception::BadInputData { stream };
            ++stream;
            T value = *begin - '0';
            while(!stream.full()) {
                if (*begin < '0' || *begin > '9') break;
                value = value * 10 + *begin - '0';
                ++stream;
            }
            return value;
        } else if constexpr (std::signed_integral<T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*begin < '0' || *begin > '9') && *begin != '-' && *begin != '+') throw exception::BadInputData { stream };
            T sign { 1 };
            if (*begin == '-') {
                sign = -1;
                ++stream;
            } else if (*begin == '+') ++stream;;
            T value = *begin - '0';
            while(!stream.full()) {
                if (*begin < '0' || *begin > '9') break;
                value = value * 10 + *begin - '0';
                ++stream;
            }
            return value * sign;
        } else if constexpr (std::is_same_v<char, T>) {
            if (begin + 2 >= end) throw exception::BadInputData { stream };
            if (*begin != '"') throw exception::BadInputData { stream };
            ++stream;
            T value = *begin;
            ++stream;
            if (*begin != '"') throw exception::BadInputData { stream };
            ++stream;
            return value;
        } else if constexpr (std::is_same_v<std::string, T>) {
            if (begin + 1 >= end) throw exception::BadInputData { stream };
            if (*begin != '"') throw exception::BadInputData { stream };
            ++stream;
            T value { };
            while(*begin != '"') {
                if (stream.full()) throw exception::BadInputData { stream };
                value.push_back(*begin);
                ++stream;
            }
            ++stream;
            return value;
        } else if constexpr (std::is_same_v<bool, T>) {
            if (begin + 3 >= end) throw exception::BadInputData { stream };
            auto ch = std::tolower(*begin);
            if (ch == 't') {
                ++stream;
                if (std::tolower(*begin) != 'r') throw exception::BadInputData { stream };
                ++stream;
                if (std::tolower(*begin) != 'u') throw exception::BadInputData { stream };
                ++stream;
                if (std::tolower(*begin) != 'e') throw exception::BadInputData { stream };
                ++stream;
                return true;
            } else if (ch == 'f') {
                ++stream;
                if (std::tolower(*begin) != 'a') throw exception::BadInputData { stream };
                ++stream;
                if (std::tolower(*begin) != 'l') throw exception::BadInputData { stream };
                ++stream;
                if (std::tolower(*begin) != 's') throw exception::BadInputData { stream };
                ++stream;
                if (stream.full()) throw exception::BadInputData { stream };
                if (std::tolower(*begin) != 'e') throw exception::BadInputData { stream };
                ++stream;
                return true;
            }
        } else if constexpr (std::is_same_v<float, T> || std::is_same_v<double, T>) {
            if (stream.full()) throw exception::BadInputData { stream };
            // TODO: Check out of range values
            if ((*begin < '0' || *begin > '9') && *begin != '-' && *begin != '+') throw exception::BadInputData { stream };
            auto start = begin;
            while (!stream.full() && *begin != ',' && *begin != '!' && *begin != ']' && *begin != '}' && *begin != ' ')
                ++stream;
            std::string number {start, begin};
            if constexpr (std::is_same_v<float, T>) return std::stof(number);
            else return std::stod(number);
        }

        return true;
    } */


};

} // namespace rohit::serializer