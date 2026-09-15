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

#include <rohit/serializer_creator.hpp>

#include <iterator>
#include <string>
#include <unordered_map>

namespace rohit {

namespace serializer {
// Look up a schema primitive and return an empty string for unknown types.
const std::string& get_cpp_type_or_empty(const std::string& type) {
  static const std::unordered_map<std::string, std::string> cpp_type_map{
      {"char", "char"},
      {"int8", "std::int8_t"},
      {"int16", "std::int16_t"},
      {"int32", "std::int32_t"},
      {"int64", "std::int64_t"},
      {"uint8", "std::uint8_t"},
      {"uint16", "std::uint16_t"},
      {"uint32", "std::uint32_t"},
      {"uint64", "std::uint64_t"},
      {"float", "float"},
      {"double", "double"},
      {"bool", "bool"},
      {"string", "std::string"}};

  static const std::string empty{};

  const auto itr = cpp_type_map.find(type);
  if (itr != std::end(cpp_type_map)) {
    return itr->second;
  }
  return empty;
}

// Return the mapped C++ type, preserving user-defined type names.
const std::string& get_cpp_type(const std::string& type) {
  const auto& ret = get_cpp_type_or_empty(type);
  if (!ret.empty()) {
    return ret;
  }
  return type;
}
} // namespace serializer

} // namespace rohit
