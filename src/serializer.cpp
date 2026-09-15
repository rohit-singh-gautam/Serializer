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

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
// Describe language-independent arguments and the currently implemented C++ backend options.
void display_help() {
  std::cout << "Usage: serializer input <schema> output <header> [config <file>] [language cpp]\n"
               "C++ overrides (key value pairs):\n"
               "  cpp.coding_standard serializer|core|google|llvm|gnu|cert|misra|autosar|qt\n"
               "  cpp.naming profile|preserve\n"
               "  cpp.format true|false\n"
               "  cpp.clang_format <executable>\n"
               "  cpp.format_file <.clang-format>\n"
               "Defaults: C++20, Serializer style, profile naming, clang-format 19+ on PATH.\n"
               "Precedence: defaults < config file < command line.\n";
}

// Detect aliases and hard links as well as identical normalized paths before replacing output.
bool same_file(const std::filesystem::path& first, const std::filesystem::path& second) {
  if (std::filesystem::exists(first) && std::filesystem::exists(second) &&
      std::filesystem::equivalent(first, second)) {
    return true;
  }
  return std::filesystem::weakly_canonical(first) == std::filesystem::weakly_canonical(second);
}
} // namespace

// Parse CLI overrides, emit the selected language, and write only a successfully formatted header.
int main(const int argc, const char* argv[]) {
  using namespace rohit::serializer;
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    display_help();
    return 0;
  }
  try {
    std::map<std::string, std::string> arguments{};
    for (int index = 1; index < argc; index += 2) {
      const std::string key{argv[index]};
      if (index + 1 == argc) {
        throw std::invalid_argument{"Missing value for argument: " + key};
      }
      if (key != "input" && key != "output" && key != "config" && key != "language" &&
          key != "cpp.coding_standard" && key != "cpp.naming" && key != "cpp.format" &&
          key != "cpp.clang_format" && key != "cpp.format_file") {
        throw std::invalid_argument{"Unknown argument: " + key};
      }
      if (std::string_view{argv[index + 1]}.empty() ||
          !arguments.emplace(key, argv[index + 1]).second) {
        throw std::invalid_argument{"Empty or repeated argument: " + key};
      }
    }
    if (!arguments.contains("input") || !arguments.contains("output")) {
      throw std::invalid_argument{"Both input and output are required; use --help for options"};
    }
    auto options = arguments.contains("config")
                       ? writer::read_output_options(arguments.at("config"))
                       : writer::output_options{};
    if (arguments.contains("language")) {
      options.language = arguments.at("language");
    }
    if (options.language != "cpp") {
      throw std::invalid_argument{"Unsupported output language: " + options.language};
    }
    if (arguments.contains("cpp.coding_standard")) {
      options.cpp.standard = writer::parse_coding_standard(arguments.at("cpp.coding_standard"));
    }
    if (arguments.contains("cpp.naming")) {
      const auto& value = arguments.at("cpp.naming");
      if (value != "profile" && value != "preserve") {
        throw std::invalid_argument{"cpp.naming must be profile or preserve"};
      }
      options.cpp.rename_identifiers = value == "profile";
    }
    if (arguments.contains("cpp.format")) {
      const auto& value = arguments.at("cpp.format");
      if (value != "true" && value != "false") {
        throw std::invalid_argument{"cpp.format must be true or false"};
      }
      options.cpp.format = value == "true";
    }
    if (arguments.contains("cpp.clang_format")) {
      options.cpp.clang_format = arguments.at("cpp.clang_format");
    }
    if (arguments.contains("cpp.format_file")) {
      options.cpp.format_file = arguments.at("cpp.format_file");
    }
    if (!options.cpp.format && !options.cpp.format_file.empty()) {
      throw std::invalid_argument{"cpp.format_file requires cpp.format true"};
    }
    const std::filesystem::path input_file{arguments.at("input")};
    const std::filesystem::path output_file{arguments.at("output")};
    if (same_file(input_file, output_file) ||
        (arguments.contains("config") && same_file(arguments.at("config"), output_file)) ||
        (!options.cpp.format_file.empty() && same_file(options.cpp.format_file, output_file))) {
      throw std::invalid_argument{
          "Output must not overwrite the schema or an output configuration"};
    }
    const auto extension = output_file.extension();
    if (extension != ".h" && extension != ".hpp" && extension != ".hxx") {
      throw std::invalid_argument{"C++ output must have a .h, .hpp, or .hxx extension"};
    }
    const auto input = rohit::make_stream_from_file(input_file);
    const auto statements = parser::parse(input);
    rohit::full_stream_auto_alloc output{};
    writer::cpp::write(output, statements, options.cpp);
    output.write_to_file_till_offset(output_file);
    std::cout << "Generated " << output_file << " (C++, "
              << writer::coding_standard_name(options.cpp.standard) << ")\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Serializer: " << error.what() << '\n';
    return 1;
  }
}
