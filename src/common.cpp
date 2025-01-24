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

namespace rohit {

namespace serializer {
const std::string &GetCPPTypeOrEmpty(const std::string &type) {
    static const std::unordered_map<std::string, std::string> CPPTypeMap {
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

    static const std::string empty { };

    auto itr = CPPTypeMap.find(type);
    if (itr != std::end(CPPTypeMap)) return itr->second;
    return empty;
}

const std::string &GetCPPType(const std::string &type) {
    auto &ret = GetCPPTypeOrEmpty(type);
    if (!ret.empty()) return ret;
    return type;
}
} // namespace serializer

} // namespace rohit